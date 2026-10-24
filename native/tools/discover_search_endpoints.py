#!/usr/bin/env python3
"""Ask a host what it SERVES, instead of guessing a replacement path.

For a row whose path has gone 404 while its host still answers 200, the host
itself usually says where the search went. This is CLAUDE.md's batch-42 and
batch-46 lesson -- "ask the endpoint what it serves rather than probing the
paths you can name" -- applied to a stale SEARCH path rather than to a WordPress
route table or an ArcGIS service directory.

Input is any TSV whose first column is a source id and whose 7th is the stale
URL, which is the shape docs/batch31-refused-triage-2026-10-20.tsv already has.
For each host root, read:

  * every <form> whose action or inputs look like a search, with its method and
    its field names, because the field name is what a pivot has to send;
  * every link whose href or text names an API, developer portal, OpenAPI
    document or open-data endpoint;
  * the sitemap(s) robots.txt declares, which often name the search path.

Nothing is changed. This writes one block per host for a human to read -- the
point is to replace a guess per row with a reading per row.

IT IS READ, NEVER GATED, and a candidate it finds is NOT a fix. Measured
2026-10-21 over B2's 44 rows: of the eight candidates this found that answered
HTTP 200 with a plausible page of anchors, SIX ignore the query parameter
entirely (docs/b2-path-stale-triage-2026-10-21.md). Every candidate has to go
through probe_hp_batch.py --check-filter before it goes anywhere near a row.
"""
import html
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, '/home/user/JapanOSINT/native/tools')
import jo_tls                                                    # noqa: E402

UA = ("JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; "
      "feed collector; contact via repo issues)")
CTX = jo_tls.context()
LOCAL = ("connection reset by peer", "ws_closed_mid_exchange",
         "connection refused", "unexpected eof while reading")

API_WORD = re.compile(r'(api|opendata|open-data|developer|swagger|openapi|'
                      r'odata|rest|graphql|dataset|json)', re.I)


def get(url, tries=3):
    for a in range(tries):
        try:
            r = urllib.request.urlopen(
                urllib.request.Request(url, headers={"User-Agent": UA}),
                timeout=45, context=CTX)
            return r.status, r.read(600000).decode("utf-8", "replace"), r.geturl()
        except urllib.error.HTTPError as e:
            return e.code, "", url
        except Exception as e:
            m = str(e).lower()
            if any(k in m for k in LOCAL) and a + 1 < tries:
                time.sleep(2 * (a + 1)); continue
            return -1, str(e)[:90], url
    return -1, "LOCAL_EGRESS", url


def forms(body, base):
    """[(method, action, [field names])] for forms that look like a search."""
    out = []
    for m in re.finditer(r'<form\b([^>]*)>(.*?)</form>', body, re.S | re.I):
        attrs, inner = m.group(1), m.group(2)
        act = re.search(r'action\s*=\s*["\']([^"\']*)["\']', attrs, re.I)
        meth = re.search(r'method\s*=\s*["\']([^"\']*)["\']', attrs, re.I)
        action = html.unescape(act.group(1)) if act else ""
        names = re.findall(r'<(?:input|select)\b[^>]*\bname\s*=\s*["\']([^"\']+)["\']',
                           inner, re.I)
        blob = (attrs + " " + action + " " + " ".join(names)).lower()
        if "search" in blob or "query" in blob or "recherche" in blob or \
           "buscar" in blob or "suche" in blob or any(
               n.lower() in ("q", "qs", "keys", "keyword", "k", "text") for n in names):
            out.append(((meth.group(1) if meth else "get").upper(),
                        urllib.parse.urljoin(base, action) if action else base,
                        sorted(set(names))[:12]))
    return out[:6]


def api_links(body, base):
    seen, out = set(), []
    for m in re.finditer(r'<a\b[^>]*href\s*=\s*["\']([^"\']+)["\'][^>]*>(.*?)</a>',
                         body, re.S | re.I):
        href, text = html.unescape(m.group(1)), re.sub(r'<[^>]+>', ' ', m.group(2))
        text = html.unescape(re.sub(r'\s+', ' ', text)).strip()[:60]
        if not API_WORD.search(href) and not API_WORD.search(text):
            continue
        u = urllib.parse.urljoin(base, href)
        if u in seen or u.startswith("mailto:"):
            continue
        seen.add(u)
        out.append((u, text))
        if len(out) >= 10:
            break
    return out


def main():
    rows = []
    for line in open(sys.argv[1]):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split("\t")
        if len(f) < 7 or f[1] == "prior":
            continue
        rows.append((f[0], f[6]))

    last = None
    for sid, url in rows:
        p = urllib.parse.urlsplit(url)
        root = "%s://%s/" % (p.scheme, p.netloc)
        if p.netloc == last:
            time.sleep(1.5)
        last = p.netloc
        print("\n=== %s  %s" % (sid, root))
        print("    stale path: %s" % url)
        st, body, final = get(root)
        print("    root: HTTP %s, %d bytes%s" % (st, len(body),
              "" if final == root else " -> " + final))
        if st != 200:
            sys.stdout.flush(); continue
        for meth, act, names in forms(body, final):
            print("    FORM %-4s %s" % (meth, act))
            if names:
                print("         fields: %s" % ", ".join(names))
        for u, t in api_links(body, final):
            print("    LINK %s   (%s)" % (u, t))
        st2, rb, _ = get(root + "robots.txt")
        if st2 == 200:
            for sm in re.findall(r'(?im)^\s*sitemap:\s*(\S+)', rb)[:3]:
                print("    SITEMAP %s" % sm)
        sys.stdout.flush()


main()
