#!/usr/bin/env python3
"""Ask WordPress hosts what they serve, then write manifest rows for what answered.

The method behind batches 42 and 45 (CLAUDE.md, "Batch 42 is batch 41 asked
properly"): a WordPress site publishes its REST root, its post types and the
collections behind them. This tool asks, never guesses, and keeps every answer.

  sweep     hosts.txt DATA/            one TSV row per host (routes.tsv) and per
                                       collection (probe.tsv); DATA/ is where
                                       downloaded bodies land — treat it as data
  manifest  DATA/probe.tsv OUT/ DATE   candidate-sources-batchNN.<beat>.txt per
            --batch NN [--existing-ids ids.txt] [--host-categories h.tsv]
                                       host-type beat, plus rejected-*.sweep.tsv

Hosts are NORMALISED before the sweep: lower-cased, a leading "www." dropped,
duplicates removed — funq.jp and www.funq.jp were both in the registry and
reached the same REST root, which only tools/batch_exclusions.py caught. The
REST root itself is taken from the homepage's `Link: rel="https://api.w.org/"`
(or the same relation as a <link> tag), falling back to a bare /wp-json; one
manifest row per (rest_root, rest_base), whatever hostname led there.

Editor and theme machinery exposed as post types (banners, sliders, forms,
block patterns, popups, layouts) is dropped as "machinery" and kept in the
rejected TSV with that shape; wp/v2/media is never asked (batch 42: its
X-WP-Total cannot be trusted).
"""
import sys, os, re, json, csv, time, ssl, threading, queue, argparse, unicodedata
import urllib.request, urllib.error

UA = "Mozilla/5.0 (compatible; JapanOSINT/1.0; +https://github.com/rayanchl/JapanOSINT)"
CTX = ssl.create_default_context()
SKIP_TYPES = {"attachment", "wp_block", "wp_template", "wp_template_part", "wp_navigation",
              "wp_font_family", "wp_font_face", "wp_global_styles", "nav_menu_item", "revision",
              "customize_changeset", "oembed_cache", "user_request", "custom_css", "acf-field",
              "acf-field-group", "acf-post-type", "acf-taxonomy", "acf-ui-options-page",
              "wpcf7_contact_form", "e-landing-page", "elementor_library", "elementor_font",
              "elementor_icons", "elementor_snippet"}
MACHINERY = re.compile(r"(banner|bnr|slide|carousel|^mv$|^mvtop$|visual|form|snippet|lazyblock|^blocks?$"
                       r"|parts|pattern|template|layout|grid|widget|pochipps|marquee|^lp$|^search$|^manage$"
                       r"|measurement|queue|mail|section|^link|^top[-_]|^pr$|smartnews|^yahoo$|robo_gallery"
                       r"|^xo_|selecter|taxonomy-page|^other$|loos-cbp|tieup|tie-up|^sidebnr$|^recommend$"
                       r"|let_me_know)", re.I)
POST_OPTS = ("array_path=.;interval=21600;title_keys=title.rendered;id_keys=id;date_keys=date;link_keys=link;"
             "body_keys=content.rendered;page_param=page;page_start=1;page_max=60;timeout_ms=30000;"
             "pagination_ok=WP REST pages with ?page=N and advertises X-WP-TotalPages -- walked until a page comes back short")
USER_OPTS = ("array_path=.;interval=86400;title_keys=name;id_keys=id;link_keys=link;body_keys=description;"
             "page_param=page;page_start=1;page_max=60;timeout_ms=30000;"
             "pagination_ok=WP REST pages with ?page=N and advertises X-WP-TotalPages -- walked until a page comes back short. "
             "No date field exists on a user record so date_keys is deliberately unset")


def get(url, timeout=20, want_json=True):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/json, */*"})
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=CTX) as r:
            body = r.read(4_000_000)
            hdrs = {k.lower(): v for k, v in r.getheaders()}
            st = r.status
    except urllib.error.HTTPError as e:
        return e.code, {}, None
    except Exception as e:
        return -1, {}, type(e).__name__
    if not want_json:
        return st, hdrs, body
    try:
        return st, hdrs, json.loads(body.decode("utf-8", "replace"))
    except Exception:
        return st, hdrs, None


def normalise_host(h):
    h = h.strip().lower().rstrip("/")
    h = re.sub(r"^https?://", "", h).split("/")[0]
    return re.sub(r"^www\d*\.", "", h)


def rest_root(host):
    """The REST root the homepage advertises, else a bare /wp-json; tries the
    host as given and with www. in front, since a normalised host may only
    answer on one of them."""
    for h in (host, "www." + host):
        for scheme in ("https", "http"):
            st, hd, body = get(f"{scheme}://{h}/", want_json=False)
            if st == -1:
                continue
            m = re.search(r'<([^>]+)>;\s*rel="https://api\.w\.org/"', hd.get("link", ""))
            if m:
                return m.group(1).rstrip("/"), "link-header"
            if isinstance(body, (bytes, bytearray)):
                m = (re.search(rb'href=["\']([^"\']+/wp-json/?)["\'][^>]*rel=["\']https://api\.w\.org/', body) or
                     re.search(rb'rel=["\']https://api\.w\.org/["\'][^>]*href=["\']([^"\']+/wp-json/?)["\']', body))
                if m:
                    return m.group(1).decode("ascii", "replace").rstrip("/"), "link-tag"
            break
    for h in (host, "www." + host):
        for scheme in ("https", "http"):
            st, hd, j = get(f"{scheme}://{h}/wp-json")
            if st == 200 and isinstance(j, dict) and "routes" in j:
                return f"{scheme}://{h}/wp-json", "guessed-root"
    return None, "no-rest-root"


def shape_of(rec):
    if not isinstance(rec, dict):
        return "not-object"
    if isinstance(rec.get("title"), dict) and "rendered" in rec["title"]:
        return "post"
    if "name" in rec and "slug" in rec and "link" in rec and "date" not in rec:
        return "user"
    return "other"


def sample_of(rec, shape):
    try:
        if shape == "post":
            return re.sub(r"\s+", " ", rec["title"]["rendered"])[:80]
        if shape == "user":
            return str(rec.get("name"))[:80]
        for k in ("title", "name", "slug", "id"):
            if k in rec:
                return str(rec[k])[:80]
    except Exception:
        pass
    return ""


def cmd_sweep(a):
    os.makedirs(a.data, exist_ok=True)
    hosts, seen = [], set()
    for line in open(a.hosts, encoding="utf-8"):
        h = normalise_host(line)
        if h and h not in seen:
            seen.add(h); hosts.append(h)
    routes_out = open(os.path.join(a.data, "routes.tsv"), "w", buffering=1)
    probe_out = open(os.path.join(a.data, "probe.tsv"), "w", buffering=1)
    log = open(os.path.join(a.data, "sweep.log"), "w", buffering=1)
    lock = threading.Lock()
    routes_out.write("host\trest_root\thow\tstatus\tn_types\ttypes\n")
    probe_out.write("host\trest_root\trest_base\ttype_name\ttype_label\tstatus\tcount\tx_wp_total\tshape\tsample\n")
    roots_seen = set()

    def work(host):
        root, how = rest_root(host)
        if not root:
            with lock:
                routes_out.write(f"{host}\t\t{how}\t\t0\t\n")
            return
        with lock:
            key = root.replace("://www.", "://")
            if key in roots_seen:
                routes_out.write(f"{host}\t{root}\t{how}\tduplicate-root\t0\t\n")
                return
            roots_seen.add(key)
        st, hd, types = get(f"{root}/wp/v2/types?context=view")
        if st != 200 or not isinstance(types, dict):
            with lock:
                routes_out.write(f"{host}\t{root}\t{how}\ttypes:{st}\t0\t\n")
            return
        bases = [(t.get("rest_base") or n, n, t.get("name") or n)
                 for n, t in types.items() if n not in SKIP_TYPES and isinstance(t, dict)]
        bases.append(("users", "user", "Users"))
        with lock:
            routes_out.write(f"{host}\t{root}\t{how}\t200\t{len(bases)}\t{','.join(b[0] for b in bases)}\n")
        for rb, tname, label in bases:
            q = "per_page=100" if tname == "user" else "per_page=100&orderby=date&order=desc"
            st, hd, j = get(f"{root}/wp/v2/{rb}?{q}")
            total = hd.get("x-wp-total", "")
            if st == 200 and isinstance(j, list):
                shp = shape_of(j[0]) if j else "empty"
                smp = sample_of(j[0], shp) if j else ""
                row = f"{host}\t{root}\t{rb}\t{tname}\t{label}\t200\t{len(j)}\t{total}\t{shp}\t{smp}\n"
            else:
                row = f"{host}\t{root}\t{rb}\t{tname}\t{label}\t{st}\t0\t{total}\t{'not-a-list' if st == 200 else ''}\t\n"
            with lock:
                probe_out.write(row)
            time.sleep(a.pace)

    q = queue.Queue()
    for h in hosts:
        q.put(h)
    n, done = len(hosts), [0]

    def runner():
        while True:
            try:
                host = q.get_nowait()
            except queue.Empty:
                return
            try:
                work(host)
            except Exception as e:  # one host's surprise must not end the sweep
                with lock:
                    log.write(f"{host}\tEXC\t{e!r}\n")
            with lock:
                done[0] += 1
                if done[0] % 50 == 0:
                    log.write(f"{time.strftime('%H:%M:%S')} {done[0]}/{n}\n")
    ts = [threading.Thread(target=runner, daemon=True) for _ in range(a.jobs)]
    for t in ts: t.start()
    for t in ts: t.join()
    log.write(f"DONE {done[0]}/{n}\n")
    print(f"swept {n} hosts -> {a.data}/routes.tsv, {a.data}/probe.tsv")


BEAT_OF_CAT = {"media": "jpwpmedia", "wire": "jpwpmedia", "web": "jpwpmedia",
               "business": "jpwpcorp", "corporate": "jpwpcorp", "commerce": "jpwpcorp", "finance": "jpwpcorp",
               "technology": "jpwpcorp", "infrastructure": "jpwpcorp", "maritime": "jpwpcorp",
               "organization": "jpwporg", "disciplinary": "jpwporg", "dispute-resolution": "jpwporg",
               "directory": "jpwporg", "policy": "jpwporg",
               "library": "jpwpculture", "security": "jpwpcyber", "physics": "jpwpscience", "research": "jpwpscience",
               "hazard": "jpwpenv", "geospatial": "jpwpenv", "government": "jpwpgov", "local-gov": "jpwpgov"}
# A federation sweep (the index pages of nationwide bodies) names each host by
# the federation that listed it; --host-categories may carry those names too.
FEDERATION = {
    "medical":     ("jpwpmedical",  "health",        "Japanese medical, pharmacist, nursing and veterinary associations"),
    "agri":        ("jpwpagri",     "agriculture",   "JA cooperatives, NOSAI and agricultural committees"),
    "chambers":    ("jpwpcci",      "business",      "Chambers of commerce and industry, and their youth wings"),
    "commerce":    ("jpwpcommerce", "business",      "Corporate-tax associations (法人会) and local business bodies"),
    "coops":       ("jpwpcoop",     "commerce",      "Consumer, fishery, labour-bank and SME co-operatives"),
    "jc":          ("jpwpjc",       "social",        "Junior Chamber (JC) local organisations"),
    "professions": ("jpwpprof",     "legal",         "Tax-accountant, judicial-scrivener, labour-consultant, administrative-scrivener, surveyor, architect and realtor associations"),
    "fire":        ("jpwpfire",     "public-safety", "Fire departments and fire-chief associations"),
    "welfare":     ("jpwpwelfare",  "health",        "Social welfare councils and welfare bodies"),
    "culture":     ("jpwpculture",  "culture",       "Public cultural facilities, zoos and aquariums"),
    "education":   ("jpwpedu",      "education",     "Junior-college and private-university associations"),
}
BLURB = {"jpwpmedia": "Japanese online media and magazines — every WordPress collection their REST root advertises",
         "jpwpcorp": "Japanese company sites — every WordPress collection their REST root advertises",
         "jpwporg": "Japanese associations and professional bodies — WordPress collections",
         "jpwpculture": "Japanese libraries and cultural institutions — WordPress collections",
         "jpwpcyber": "Japanese security organisations — WordPress collections",
         "jpwpscience": "Japanese research institutions — WordPress collections",
         "jpwpenv": "Japanese hazard and geospatial sites — WordPress collections",
         "jpwpgov": "Japanese government sites — WordPress collections"}


def cmd_manifest(a):
    rows = list(csv.DictReader(open(a.probe, encoding="utf-8"), delimiter="\t"))
    existing = set(open(a.existing_ids).read().split()) if a.existing_ids else set()
    hostcat = {}
    if a.host_categories:
        for line in open(a.host_categories, encoding="utf-8"):
            h, _, c = line.rstrip("\n").partition("\t")
            c = c.split(":")[0].strip()
            if c:
                hostcat[normalise_host(h)] = c
    ok, rej = [], []
    seen_ep = set()
    for r in rows:
        live = r["status"] == "200" and int(r["count"] or 0) >= 1 and r["shape"] in ("post", "user")
        if not live:
            rej.append(r); continue
        if r["type_name"] not in ("post", "page", "user") and MACHINERY.search(r["rest_base"]):
            r["shape"] = "machinery"; rej.append(r); continue
        k = (r["rest_root"].replace("://www.", "://"), r["rest_base"])
        if k in seen_ep:
            r["shape"] = "duplicate-endpoint"; rej.append(r); continue
        seen_ep.add(k); ok.append(r)
    # one REST-root read per site for its name; concurrent, because a thousand
    # federation hosts read one at a time with a 20 s timeout is an afternoon
    names = {}
    roots = sorted({r["rest_root"] for r in ok})
    def _name(root):
        j = get(root, timeout=12)[2]
        nm = j.get("name") if isinstance(j, dict) else None
        return root, (re.sub(r"\s+", " ", unicodedata.normalize("NFKC", nm)).strip().replace("|", " / ") if nm else None)
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(max_workers=16) as ex:
        for root, nm in ex.map(_name, roots):
            names[root] = nm

    def slug(host):
        h = normalise_host(host)
        h = re.sub(r"\.(co|or|ne|ac|go|lg|ed|gr)\.jp$|\.(jp|com|net|org|info|biz)$", "", h)
        return re.sub(r"[^a-z0-9]+", "", h).upper()[:28]

    def tok(s):
        return re.sub(r"[^a-z0-9]+", "", s.lower()).upper()[:20]

    beats, seen_ids = {}, set(existing)
    pfx = f"JO{a.batch}_WP_"
    for r in ok:
        host, root, rb, tname, label = r["host"], r["rest_root"], r["rest_base"], r["type_name"], r["type_label"]
        total = r["x_wp_total"] or "?"
        site = names.get(root) or host
        hc = hostcat.get(normalise_host(host))
        if hc in FEDERATION:
            beat, cat, _ = FEDERATION[hc]
        else:
            cat = hc or (
                "government" if re.search(r"\.go\.jp$", host) else "corporate" if re.search(r"\.co\.jp$", host)
                else "organization" if re.search(r"\.or\.jp$|\.org$", host) else "research" if ".ac.jp" in host
                else "local-gov" if ".lg.jp" in host else "media")
            beat = BEAT_OF_CAT.get(cat, "jpwpmedia")
        if tname == "post": suffix, rtype, tags = "POSTS", "site-post", "japan,wordpress,posts"
        elif tname == "page": suffix, rtype, tags = "PAGES", "site-page", "japan,wordpress,pages"
        elif tname == "user": suffix, rtype, tags = "USERS", "site-user", "japan,wordpress,users"
        else: suffix, rtype, tags = tok(rb) or tok(tname), f"cpt-{re.sub(r'[^a-z0-9]+', '-', rb.lower()).strip('-')}", "japan,wordpress,cpt"
        sid, n = f"{pfx}{slug(host)}_{suffix}", 2
        while sid in seen_ids:
            sid = f"{pfx}{slug(host)}_{suffix}{n}"; n += 1
        seen_ids.add(sid)
        if tname == "user":
            url = f"{root}/wp/v2/users?per_page=100"
            name, name_ja = f"{site} — author/staff register (WordPress REST)", f"{site} 利用者一覧（WordPress REST）"
            desc = (f"WordPress REST author/staff register of {site} ({host}). Fetched {a.date}: the collection answered with "
                    f"{r['count']} record(s) and declares {total} in X-WP-Total. Discovered from this host's own REST root "
                    f"(/wp/v2/types), not guessed. Keyed on the user id with name as the title: a user record carries no date field.")
            opts = USER_OPTS
        else:
            url = f"{root}/wp/v2/{rb}?per_page=100&orderby=date&order=desc"
            what = {"post": "posts", "page": "pages"}.get(tname, f"custom post type `{rb}` ({label})")
            ja = {"post": "投稿", "page": "固定ページ"}.get(tname, label)
            name, name_ja = f"{site} — {what} (WordPress REST)", f"{site} {ja}（WordPress REST）"
            desc = (f"WordPress REST {what} of {site} ({host}). Fetched {a.date}: the collection answered with {r['count']} "
                    f"record(s) and declares {total} in X-WP-Total. Discovered from this host's own REST root (/wp/v2/types), "
                    f"not guessed. Post-shaped: title.rendered, date, link and content.rendered are kept.")
            opts = POST_OPTS
        portal = re.sub(r"/wp-json.*$|/index\.php.*$", "/", root) if ("/wp-json" in root or "index.php" in root) else root + "/"
        for f in (name, name_ja, desc):
            assert "|" not in f, (sid, f)
        beats.setdefault(beat, []).append("|".join([sid, "json", "any", cat, rtype, tags, portal, name, name_ja, url, url, desc, opts]))
    os.makedirs(a.out, exist_ok=True)
    for beat, lines in beats.items():
        with open(os.path.join(a.out, f"candidate-sources-batch{a.batch}.{beat}.txt"), "w", encoding="utf-8") as f:
            blurb = BLURB.get(beat) or next((v[2] for v in FEDERATION.values() if v[0] == beat), beat)
            f.write(f"# Batch {a.batch} — beat {beat}: {blurb}\n# id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts\n#\n")
            f.write(f"# Generated by tools/wp_routes_sweep.py manifest from a sweep measured {a.date}: each host was asked for its\n"
                    f"# WordPress REST root (Link rel=\"https://api.w.org/\", else /wp-json), its post types and one page of each\n"
                    f"# collection. Only collections that answered 200 with >=1 record are here; the rest are in the rejected TSV.\n#\n")
            f.write("\n".join(lines) + "\n")
        print(beat, len(lines))
    with open(os.path.join(a.out, f"rejected-sources-batch{a.batch}.sweep.tsv"), "w", encoding="utf-8") as f:
        f.write("host\trest_root\trest_base\ttype_name\tstatus\tcount\tx_wp_total\tshape\n")
        for r in rej:
            f.write("\t".join(r[k] for k in ("host", "rest_root", "rest_base", "type_name", "status", "count", "x_wp_total", "shape")) + "\n")
    print("rejected", len(rej), "| hosts named", sum(1 for v in names.values() if v), "of", len(names))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sweep"); s.add_argument("hosts"); s.add_argument("data")
    s.add_argument("--jobs", type=int, default=24); s.add_argument("--pace", type=float, default=0.3)
    m = sub.add_parser("manifest"); m.add_argument("probe"); m.add_argument("out"); m.add_argument("date")
    m.add_argument("--batch", required=True); m.add_argument("--existing-ids"); m.add_argument("--host-categories")
    a = ap.parse_args()
    (cmd_sweep if a.cmd == "sweep" else cmd_manifest)(a)


if __name__ == "__main__":
    main()
