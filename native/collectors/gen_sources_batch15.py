#!/usr/bin/env python3
"""Discover EU/US/JP open-data endpoints by PROBING, then emit a candidate manifest.

The difference from gen_candidates_jp_fr_us.py (batch 14) is where the hostnames
come from and how much is assumed:

  batch 14  hostnames mined from the tree + national APIs recalled from their
            documented contracts, emitted unverified because the authoring
            sandbox had no egress. 261 of its 1,001 rows turned out dead.

  batch 15  hostnames come from a machine-readable registry of open-data
            portals (dataportals.org, 631 entries) plus hosts this tree has
            already proven live. NOTHING is assumed about a portal's platform:
            each one is probed with the signature endpoint of each platform we
            can parse, and only a portal that ANSWERS gets its contract
            expanded. A hostname nobody can reach never becomes a row.

So this script needs network, by design. It is discovery, not recall.

Platform contracts. For CKAN, Socrata, OpenDataSoft and ArcGIS Hub the API
paths are fixed by the platform, not by the deployment — that is what makes
expansion safe once the platform is confirmed. Each expanded endpoint still
goes through verify_feeds.py afterwards; this script's job is to stop obviously
dead hosts from ever reaching the manifest.

Pipeline:
  gen_sources_batch15.py --portals portals.json --out candidate-sources-batch15.tsv
  tools/promote_candidates.py  ... --verified ... --rejects ...     (2xx + parse)
  gen_verified_sources.py verified-sources-batch15.tsv --prefix vsrc15
"""
import argparse
import io
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor

UA = ("Mozilla/5.0 (compatible; JapanOSINT-discover/1.0; "
      "+https://github.com/) Python-urllib")
TIMEOUT = 20

COLUMNS = ["id", "url", "kind", "items", "name", "name_ja", "collector",
           "category", "lang", "tags", "interval", "description",
           "verified", "verified_note"]

# ── platform contracts ──────────────────────────────────────────────────────
# probe  : the endpoint that proves the platform is there and answering
# accept : a predicate on the parsed probe body
# expand : (suffix, kind, slug, description) rows added once the probe passes
#
# Every path below is core platform API, present on every deployment of that
# platform — not a per-site customisation.

def _ckan_ok(doc):
    return isinstance(doc, dict) and ("result" in doc or "success" in doc)

def _socrata_ok(doc):
    return isinstance(doc, dict) and "results" in doc

def _ods_ok(doc):
    return isinstance(doc, dict) and ("results" in doc or "total_count" in doc)

def _arcgis_ok(doc):
    return isinstance(doc, dict) and ("features" in doc or "data" in doc)

PLATFORMS = {
    "ckan": {
        "probe": "/api/3/action/package_search?rows=1",
        "accept": _ckan_ok,
        "expand": [
            ("/api/3/action/package_search?rows=100", "json:result.results",
             "datasets", "Dataset catalogue: title, notes, organization, "
             "licence, tags, resource list and modification dates for every "
             "published dataset"),
            ("/api/3/action/organization_list?all_fields=true", "json:result",
             "orgs", "Every publishing organisation on the portal with its "
             "title, description, package count and image"),
            ("/api/3/action/group_list?all_fields=true", "json:result",
             "groups", "Thematic groups/collections declared by the portal"),
            ("/api/3/action/tag_list", "json:result",
             "tags", "The portal's full controlled vocabulary of dataset tags"),
            ("/api/3/action/recently_changed_packages_activity_list?limit=100",
             "json:result", "activity",
             "Recent publish/update activity — what this authority changed and "
             "when, which is the change-detection signal behind the catalogue"),
        ],
    },
    "socrata": {
        "probe": "/api/catalog/v1?limit=1",
        "accept": _socrata_ok,
        "expand": [
            ("/api/catalog/v1?limit=100", "json:results", "catalog",
             "Socrata discovery catalogue: every dataset with owner, category, "
             "provenance, update cadence and column metadata"),
            ("/api/views/metadata/v1", "json-array", "views",
             "Per-view metadata for every dataset on the domain"),
        ],
    },
    "ods": {
        "probe": "/api/explore/v2.1/catalog/datasets?limit=1",
        "accept": _ods_ok,
        "expand": [
            ("/api/explore/v2.1/catalog/datasets?limit=100", "json:results",
             "datasets", "OpenDataSoft Explore catalogue: dataset id, title, "
             "theme, publisher, licence, record count and field schema"),
            ("/api/explore/v2.1/catalog/facets", "json:facets", "facets",
             "The portal's facet tree — themes, publishers and keywords with "
             "counts, i.e. the shape of what this authority publishes"),
        ],
    },
    "arcgis": {
        "probe": "/api/search/v1/collections/dataset/items?limit=1",
        "accept": _arcgis_ok,
        "expand": [
            ("/api/search/v1/collections/dataset/items?limit=100", "geojson",
             "datasets", "ArcGIS Hub dataset collection with spatial extent, "
             "publisher and service URLs"),
        ],
    },
    # uData — the platform behind data.gouv.fr and its sibling national
    # portals. Worth its own contract: data.gouv.fr alone declares 6,532
    # organisations over 73,895 datasets, none of which the CKAN/ODS/Socrata
    # probes can see because uData answers none of their paths.
    "udata": {
        "probe": "/api/1/datasets/?page_size=1",
        "accept": lambda d: isinstance(d, dict) and "data" in d and "total" in d,
        "expand": [
            ("/api/1/datasets/?page_size=100", "json:data", "datasets",
             "National dataset catalogue: title, organisation, licence, "
             "frequency, temporal and spatial coverage, resource list and "
             "quality score for every published dataset"),
            ("/api/1/organizations/?page_size=100", "json:data", "orgs",
             "Every publishing organisation on the portal with its badges, "
             "dataset and member counts"),
            ("/api/1/reuses/?page_size=100", "json:data", "reuses",
             "Declared reuses — who is building on this data and what they "
             "built, which is the downstream-attribution signal"),
        ],
    },
}

CATEGORY = "opendata"

# Country -> (language, human name) for the rows we emit.
COUNTRY = {
    "US": ("en", "United States"), "JP": ("ja", "Japan"),
    "DE": ("de", "Germany"), "FR": ("fr", "France"), "IT": ("it", "Italy"),
    "ES": ("es", "Spain"), "NL": ("nl", "Netherlands"), "BE": ("nl", "Belgium"),
    "PL": ("pl", "Poland"), "SE": ("sv", "Sweden"), "DK": ("da", "Denmark"),
    "NO": ("no", "Norway"), "FI": ("fi", "Finland"), "AT": ("de", "Austria"),
    "CH": ("de", "Switzerland"), "IE": ("en", "Ireland"),
    "PT": ("pt", "Portugal"), "CZ": ("cs", "Czechia"), "GR": ("el", "Greece"),
    "RO": ("ro", "Romania"), "HU": ("hu", "Hungary"), "SK": ("sk", "Slovakia"),
    "SI": ("sl", "Slovenia"), "HR": ("hr", "Croatia"), "BG": ("bg", "Bulgaria"),
    "EE": ("et", "Estonia"), "LV": ("lv", "Latvia"), "LT": ("lt", "Lithuania"),
    "LU": ("fr", "Luxembourg"), "MT": ("en", "Malta"), "CY": ("el", "Cyprus"),
    "IS": ("is", "Iceland"), "UK": ("en", "United Kingdom"),
    "GB": ("en", "United Kingdom"),
}


def fetch_json(url):
    req = urllib.request.Request(url, headers={
        "User-Agent": UA, "Accept": "application/json, */*"})
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
            if r.status // 100 != 2:
                return None
            raw = r.read(2 * 1024 * 1024)
    except Exception:
        return None
    try:
        return json.loads(raw.decode("utf-8", "replace"))
    except Exception:
        return None


def portal_bases(entry):
    """Candidate API bases for a portal.

    Both the host root AND the registry URL's path prefix, because plenty of
    portals are mounted under a path (`example.gov/data/api/3/action/...`) and
    an early version of this script only tried the root — which silently missed
    every one of them. Cheap to try both; the probe decides."""
    out, seen = [], set()
    for raw in (entry.get("api_endpoint"), entry.get("url")):
        if not raw:
            continue
        p = urllib.parse.urlsplit(raw if "//" in raw else "https://" + raw)
        if not p.netloc:
            continue
        root = "%s://%s" % (p.scheme or "https", p.netloc)
        cands = [root]
        # strip the human-facing leaf (/dataset, /datasets, /api, /catalogo…)
        path = re.sub(r"/(dataset|datasets|data|api|catalog|catalogo|portal)/?$",
                      "", p.path.rstrip("/"))
        if path and path != "/":
            cands.append(root + path)
        for base in cands:
            if base not in seen:
                seen.add(base)
                out.append(base)
    return out


def detect(entry):
    """Probe one portal for each platform we can parse. Returns (base, platform)
    or None. A portal answering nothing is dropped here and never reaches the
    manifest — that is the whole point of probing rather than assuming."""
    for base in portal_bases(entry):
        for plat, spec in PLATFORMS.items():
            doc = fetch_json(base + spec["probe"])
            if doc is not None and spec["accept"](doc):
                return base, plat
    return None


def slugify(s):
    s = re.sub(r"[^a-zA-Z0-9]+", "-", (s or "").lower()).strip("-")
    return re.sub(r"-{2,}", "-", s)[:40] or "portal"


# ── the penetrancy stage ────────────────────────────────────────────────────
# A portal-level catalogue endpoint answers "what does this authority publish?"
# once. The useful axis is one level down: the PUBLISHERS inside the portal,
# each with its own dataset stream. Those are what an investigation actually
# pivots on ("what does this ministry / police department / water authority
# release, and when did it last change?").
#
# The sub-entity names are FETCHED, never guessed — if the list call fails we
# emit nothing extra rather than inventing plausible organisation slugs. That is
# the same rule as everywhere else: real fetch or honest empty.

MAX_SUBS = 150         # per portal, so one huge portal cannot swamp the batch


def deepen(base, plat):
    """Return extra (suffix, kind, slug, description) rows built from the
    portal's own real sub-entity list."""
    rows = []
    if plat == "ckan":
        # &limit=1000 matters: CKAN's organization_list defaults to 25, so the
        # obvious call silently truncates. data.gov.ie really has 174 orgs and
        # data.gov.ro 180 — asking without a limit loses 85% of them, which is
        # exactly the discard the exhaustive-use rule is about.
        doc = fetch_json(base +
                         "/api/3/action/organization_list?all_fields=true&limit=1000")
        orgs = (doc or {}).get("result") or []
        for o in orgs[:MAX_SUBS]:
            nm = o.get("name") if isinstance(o, dict) else o
            if not nm:
                continue
            title = (o.get("title") or nm) if isinstance(o, dict) else nm
            rows.append((
                "/api/3/action/package_search?fq=organization:%s&rows=100"
                % urllib.parse.quote(str(nm)),
                "json:result.results", "org-%s" % slugify(str(nm)),
                "Everything published by %s on this portal: every dataset with "
                "its resources, licence, tags and last-modified date" % title))
    elif plat == "ods":
        doc = fetch_json(base + "/api/explore/v2.1/catalog/facets")
        facets = (doc or {}).get("facets") or []
        vals = []
        for f in facets:
            if isinstance(f, dict) and f.get("name") in ("theme", "publisher", "keyword"):
                for fv in (f.get("facets") or [])[:MAX_SUBS]:
                    if isinstance(fv, dict) and fv.get("name"):
                        vals.append((f["name"], fv["name"]))
        for fname, val in vals[:MAX_SUBS]:
            rows.append((
                "/api/explore/v2.1/catalog/datasets?where=%s%%3D%%22%s%%22&limit=100"
                % (fname, urllib.parse.quote(str(val))),
                "json:results", "%s-%s" % (fname, slugify(str(val))),
                "Every dataset on this portal whose %s is %s — the publisher's "
                "full stream, with field schema and record counts" % (fname, val)))
    elif plat == "udata":
        # page_size caps at 100 and there is no sort by dataset count
        # (`sort=-metric.datasets` is a 400), so walk pages instead of asking
        # for one big sorted page.
        orgs, page = [], 1
        while len(orgs) < MAX_SUBS and page <= 4:
            doc = fetch_json(base + "/api/1/organizations/?page_size=100&page=%d" % page)
            batch = (doc or {}).get("data") or []
            if not batch:
                break
            orgs.extend(batch)
            page += 1
        for o in orgs[:MAX_SUBS]:
            if not isinstance(o, dict):
                continue
            slug = o.get("slug") or o.get("id")
            if not slug:
                continue
            rows.append((
                "/api/1/datasets/?organization=%s&page_size=100"
                % urllib.parse.quote(str(slug)),
                "json:data", "org-%s" % slugify(str(slug)),
                "Every dataset published by %s: licence, update frequency, "
                "spatial/temporal coverage and the full resource list"
                % (o.get("name") or slug)))
    elif plat == "socrata":
        doc = fetch_json(base + "/api/catalog/v1/categories")
        cats = (doc or {}).get("results") or []
        for c in cats[:MAX_SUBS]:
            nm = c.get("category") if isinstance(c, dict) else c
            if not nm:
                continue
            rows.append((
                "/api/catalog/v1?categories=%s&limit=100"
                % urllib.parse.quote(str(nm)),
                "json:results", "cat-%s" % slugify(str(nm)),
                "Every dataset on this domain categorised '%s', with owner, "
                "update cadence and column metadata" % nm))
    return rows


def build_rows(entry, base, plat, seen_ids, seen_urls, extra=()):
    cc = (entry.get("country") or "").upper()
    lang, cname = COUNTRY.get(cc, ("en", cc or "unknown"))
    host = urllib.parse.urlsplit(base).netloc
    title = entry.get("title") or host
    hslug = slugify(host.replace(".", "-"))
    rows = []
    for suffix, kind, slug, desc in list(PLATFORMS[plat]["expand"]) + list(extra):
        url = base + suffix
        if url in seen_urls:
            continue
        sid = "%s-%s-%s" % (cc.lower() or "xx", hslug, slug)
        if sid in seen_ids:
            continue
        seen_ids.add(sid)
        seen_urls.add(url)
        rows.append(dict(zip(COLUMNS, [
            sid, url, kind, "",
            "%s — %s (%s)" % (title, slug, cname),
            "",
            "%s_opendata" % (cc.lower() or "xx"),
            CATEGORY, lang,
            ",".join([cc.lower(), plat, "opendata", slug]),
            "86400",
            "%s. Served by the %s platform at %s." % (desc, plat.upper(), host),
            "no", "pending-probe",
        ])))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--portals", required=True,
                    help="JSON list of {country,url,api_endpoint,title}")
    ap.add_argument("--out", required=True)
    ap.add_argument("--exclude-urls", help="file of URLs already in the tree")
    ap.add_argument("--exclude-ids", help="file of source ids already registered")
    ap.add_argument("--workers", type=int, default=24)
    a = ap.parse_args()

    entries = json.load(io.open(a.portals, encoding="utf-8"))
    seen_urls = set()
    if a.exclude_urls and os.path.exists(a.exclude_urls):
        seen_urls = {l.strip() for l in io.open(a.exclude_urls, encoding="utf-8",
                                                errors="replace") if l.strip()}
    seen_ids = set()
    if a.exclude_ids and os.path.exists(a.exclude_ids):
        seen_ids = {l.strip() for l in io.open(a.exclude_ids, encoding="utf-8",
                                               errors="replace") if l.strip()}

    sys.stderr.write("probing %d portals with %d workers...\n"
                     % (len(entries), a.workers))
    with ThreadPoolExecutor(max_workers=a.workers) as ex:
        detected = list(ex.map(detect, entries))

    hits = [(e, d) for e, d in zip(entries, detected) if d]
    sys.stderr.write("portals answering: %d/%d — deepening...\n"
                     % (len(hits), len(entries)))

    # Second network pass: ask each live portal for its own publisher list.
    with ThreadPoolExecutor(max_workers=a.workers) as ex:
        extras = list(ex.map(lambda hd: deepen(hd[1][0], hd[1][1]), hits))

    rows, nplat = [], {}
    for (entry, det), extra in zip(hits, extras):
        base, plat = det
        nplat[plat] = nplat.get(plat, 0) + 1
        rows.extend(build_rows(entry, base, plat, seen_ids, seen_urls, extra))

    with io.open(a.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\t".join(COLUMNS) + "\n")
        for r in rows:
            fh.write("\t".join(str(r[c]).replace("\t", " ") for c in COLUMNS) + "\n")

    sys.stderr.write("platforms: %s\n" % nplat)
    sys.stderr.write("sub-entity rows: %d\n" % sum(len(x) for x in extras))
    sys.stderr.write("candidate rows: %d -> %s\n" % (len(rows), a.out))


if __name__ == "__main__":
    main()
