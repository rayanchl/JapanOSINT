#!/usr/bin/env python3
"""Does the SECOND page actually work? Asked of every paged scheduled row.

THE GAP THIS CLOSES. Two rows were found reading a fraction of their upstream
for weeks, both by accident, and both for the same reason: the row declared
paging the upstream does not accept.

  ENA_PATHOGEN_SARSCOV2_RUNS  1,000 of 7,556,500 — ebi.ac.uk answers
                              {"message":"Unsupported param offset"} to ANY
                              offset. There is no second page to ask for.
  BC_MINERAL_TENURE           50 of 42,234 — GeoServer answers "Cannot do
                              natural order without a primary key, please add
                              it or specify a manual sort over existing
                              attributes" to `startIndex` without `sortBy`.

Every existing gate passes both. The probe fetches page ONE. `audit_batch_emit`
and `audit_registry_emit` run the row and read what it stored, and a row that
stores its whole first page stores everything it emitted — `records == stored`,
rc=0. `audit_page_ceiling` probes three pages but reads the ceiling arithmetic,
and `audit_cycle_health` only covers rows that declare `page_resume`. The
question none of them asks is the simplest one: **ask for page 2 and look at
what comes back.**

It is two requests a row, and the verdict that matters is `REFUSED` — a 4xx or
an HTTP-200 error body, with the upstream's own message quoted, because that
message is what says which remedy applies:

  * a message naming the PARAMETER as unsupported  -> the collection has no
    page 2 at all; it needs a different slicing axis (`window_days`, a filter)
  * a message naming a missing SORT                -> add the sort (BC's case)
  * a message naming a DEPTH cap                   -> narrow each request
  * 429 / 403 / 5xx                                -> one reading, re-run

`SAME_AS_PAGE1` is the other finding, and a quieter one: the server accepted
the parameter, ignored it, and served the same records again. That is rule 4d
applied to paging — a walk that emits N pages of one page's records, with the
collision guard hiding it at the sink.

Reads rows out of the C (the maintained copy), skips entity pivots (a pivot
answers about an entity, and page 2 of no entity is meaningless), and
serialises per HOST: this tool's own sibling manufactured 11 false findings by
running 12 rows of the same upstreams at once.

  python3 tools/audit_page_two.py --out page2.tsv --jobs 6
"""
import argparse
import csv
import json
import os
import re
import ssl
import sys
import threading
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor

UA = ("Mozilla/5.0 (compatible; JapanOSINT/1.0; +https://github.com/rayanchl/"
      "japanosint)")
_lock = threading.Lock()
_ctx = ssl.create_default_context()
for _ca in ("/root/.ccr/ca-bundle.crt",):
    if os.path.exists(_ca):
        _ctx.load_verify_locations(_ca)

ROW = re.compile(r'\{\s*\.id\s*=\s*"([^"]+)"')
# An entity token makes the row a pivot; page 2 of no entity means nothing.
ENTITY = re.compile(r"\{q[a-z]*\}|\{cik|\{key\}")
COLS = ["verdict", "id", "page_param", "page_size", "http1", "http2",
        "recs1", "recs2", "host", "message", "url2"]

# The upstream's own words, grouped by the remedy they imply. Matched against
# the page-2 body so the verdict carries a remedy and not just a status.
REMEDY = [
    ("no-such-parameter",
     r"unsupported param|unknown param|invalid param|not a (valid |recognized )?"
     r"param|unexpected param|param .* not support"),
    ("needs-a-sort",
     r"natural order|without a (primary key|sort)|must (be )?(specify|provide) "
     r"a sort|sortby is required|order by"),
    ("depth-cap",
     r"too deep|max(imum)? (offset|result|window|from)|result window|"
     r"deep pagination|exceeds? the (maximum|limit)|offset .* too large"),
]


def paged_rows(tree):
    """(id, host, url, page_param, page_size, next_path) for scheduled rows
    that declare offset/page-number paging."""
    out = []
    for dirpath, _d, files in os.walk(tree):
        for fn in files:
            if not fn.endswith(".c"):
                continue
            t = open(os.path.join(dirpath, fn), encoding="utf-8",
                     errors="replace").read()
            if "page_param" not in t:
                continue
            for m in ROW.finditer(t):
                nxt = t.find("{ .id =", m.end())
                body = t[m.start():nxt if nxt > 0 else len(t)]
                pp = re.search(r'\.page_param\s*=\s*"([^"]+)"', body)
                if not pp:
                    continue
                iv = re.search(r"\.interval\s*=\s*(\d+)", body)
                if not iv or int(iv.group(1)) == 0:
                    continue          # a pivot, not a scheduled walk
                u = re.search(r'\.url\s*=\s*"((?:[^"\\]|\\.)*)"', body)
                if not u:
                    continue
                url = u.group(1)
                if ENTITY.search(url) or "{page" in url or "{offset" in url:
                    continue          # pivot, or a token this tool cannot bind
                if "{date" in url or "{ago" in url or "{wstart" in url:
                    continue          # a dated window; its own rules apply
                ps = re.search(r"\.page_size\s*=\s*(\d+)", body)
                zb = re.search(r"\.page_zero_based\s*=\s*1", body)
                st = re.search(r"\.page_start\s*=\s*(\d+)", body)
                hm = re.search(r"https?://([A-Za-z0-9._-]+)", url)
                out.append({
                    "id": m.group(1), "url": url,
                    "host": hm.group(1).lower() if hm else "",
                    "page_param": pp.group(1),
                    "page_size": int(ps.group(1)) if ps else 0,
                    "zero_based": bool(zb),
                    "page_start": int(st.group(1)) if st else 0,
                })
    return out


def set_param(url, key, val):
    """Replace the parameter if present, else append — the same contract as
    hp_url_set_param, which exists because APPENDING made a server bind the
    first occurrence and serve page 1 for the whole walk."""
    enc = re.escape(key)
    if re.search(r"[?&]%s=" % enc, url):
        return re.sub(r"([?&]%s=)[^&]*" % enc, r"\g<1>%s" % val, url)
    return url + ("&" if "?" in url else "?") + "%s=%s" % (key, val)


def get(url, timeout=60):
    req = urllib.request.Request(url, headers={
        "User-Agent": UA, "Accept": "application/json, text/csv, */*"})
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=_ctx) as r:
            return r.status, r.read()[:400000]
    except urllib.error.HTTPError as e:
        try:
            return e.code, e.read()[:4000]
        except Exception:
            return e.code, b""
    except Exception as e:
        return -1, str(e)[:200].encode()


def count_records(blob):
    """A floor on how many records a body holds, mode-agnostic.

    Only ever compared with ITSELF (page 1 against page 2), so a floor is
    enough — the question is "same, fewer, or none", never an exact total."""
    s = blob.decode("utf-8", "replace").strip()
    if not s:
        return 0
    if s[0] in "[{":
        try:
            d = json.loads(s)
        except ValueError:
            return s.count("},") + 1
        best = 0
        stack = [d]
        while stack:
            cur = stack.pop()
            if isinstance(cur, list):
                best = max(best, len(cur))
                stack.extend(x for x in cur if isinstance(x, (list, dict)))
            elif isinstance(cur, dict):
                stack.extend(cur.values())
        return best
    return max(0, s.count("\n"))


def message_of(blob):
    """The upstream's own explanation, if it gave one."""
    s = blob.decode("utf-8", "replace")
    try:
        d = json.loads(s)
        for k in ("message", "error", "detail", "error_description",
                  "errorMessage", "Message"):
            v = d.get(k) if isinstance(d, dict) else None
            if isinstance(v, str) and v.strip():
                return v.strip()[:220]
            if isinstance(v, dict):
                for kk in ("message", "details", "description"):
                    vv = v.get(kk)
                    if isinstance(vv, str) and vv.strip():
                        return vv.strip()[:220]
                    if isinstance(vv, list) and vv:
                        return str(vv[0])[:220]
    except ValueError:
        pass
    # XML/OGC service exceptions say it in an element.
    for pat in (r"<(?:ows:)?ExceptionText>([^<]{3,300})",
                r"<ServiceException[^>]*>([^<]{3,300})",
                r"<(?:faultstring|message|Message)>([^<]{3,300})"):
        mm = re.search(pat, s, re.I)
        if mm:
            return " ".join(mm.group(1).split())[:220]
    s2 = " ".join(s.split())
    return s2[:220] if len(s2) < 400 else ""


def remedy_of(msg):
    for name, pat in REMEDY:
        if re.search(pat, msg, re.I):
            return name
    return ""


def probe(a, r):
    host = r["host"] or r["id"]
    lock = a.locks.setdefault(host, threading.Lock())
    with lock:
        return _probe(a, r)


def _probe(a, r):
    out = dict.fromkeys(COLS, "")
    out.update({"id": r["id"], "page_param": r["page_param"],
                "page_size": r["page_size"] or "", "host": r["host"]})
    # PAGE 1 IS THE ROW'S URL AS WRITTEN. The engine does not bind the page
    # parameter for the first request — it asks for the url and advances from
    # there — so binding it here asks a question the collector never asks.
    # That mattered: GeoServer refuses `startIndex` at ANY value without a
    # sort, so adding `startIndex=1` to page 1 reported BC_MINERAL_TENURE as
    # PAGE1_FAILED when its real page 1 is fine and only its PAGING is broken.
    # The verdict would have pointed at the wrong half of the row.
    if r["page_size"]:
        second = r["page_start"] + r["page_size"]        # an OFFSET parameter
    else:
        first = r["page_start"] if r["zero_based"] else max(r["page_start"], 1)
        second = first + 1                              # a page NUMBER
    u1 = r["url"]
    u2 = set_param(r["url"], r["page_param"], str(second))
    out["url2"] = u2[:300]

    s1, b1 = get(u1, a.timeout)
    out["http1"] = s1
    if s1 != 200:
        out["verdict"] = "PAGE1_FAILED"
        out["message"] = message_of(b1)[:200]
        return out
    n1 = count_records(b1)
    out["recs1"] = n1
    if n1 == 0:
        out["verdict"] = "PAGE1_EMPTY"
        return out

    s2, b2 = get(u2, a.timeout)
    out["http2"] = s2
    n2 = count_records(b2)
    out["recs2"] = n2
    msg = message_of(b2)

    if s2 == -1:
        out["verdict"] = "NET_ERR"
        out["message"] = msg[:200]
        return out
    if s2 != 200:
        out["verdict"] = "REFUSED"
        out["message"] = ("%s | %s" % (remedy_of(msg), msg)).strip(" |")[:220]
        return out
    # An HTTP-200 refusal: the status says yes and the body says no. The same
    # shape probe_hp_batch.py rejects, and the shape ArcGIS uses to say
    # returnCentroid is polygon-only.
    if n2 == 0 and msg and remedy_of(msg):
        out["verdict"] = "REFUSED"
        out["message"] = ("200-body | %s | %s"
                          % (remedy_of(msg), msg))[:220]
        return out
    if b2 == b1:
        # Byte-identical: the parameter was accepted and ignored. Two pages of
        # genuinely different records cannot be byte-identical.
        out["verdict"] = "SAME_AS_PAGE1"
        out["message"] = "page 2 is byte-identical to page 1"
        return out
    if n2 == 0:
        out["verdict"] = "PAGE2_EMPTY"
        out["message"] = "collection may simply end inside page 1"
        return out
    out["verdict"] = "OK"
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tree", default="collectors")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--out")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--ids-file")
    a = ap.parse_args()

    rows = paged_rows(a.tree)
    if a.ids_file:
        want = {x.strip() for x in open(a.ids_file) if x.strip()}
        rows = [r for r in rows if r["id"] in want]
    if a.limit:
        rows = rows[:a.limit]
    a.locks = {}
    sys.stderr.write("asking page 2 of %d paged scheduled rows on %d hosts, "
                     "%d jobs (one row at a time per host)\n"
                     % (len(rows), len({r["host"] for r in rows}), a.jobs))
    done = [0]

    def one(r):
        res = probe(a, r)
        with _lock:
            done[0] += 1
            if res["verdict"] not in ("OK", "PAGE2_EMPTY"):
                sys.stderr.write("%-14s %-40s %s\n" % (
                    res["verdict"], res["id"], str(res["message"])[:80]))
            if done[0] % 100 == 0:
                sys.stderr.write("  ... %d/%d\n" % (done[0], len(rows)))
            sys.stderr.flush()
        return res

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        res = list(ex.map(one, rows))

    if a.out:
        with open(a.out, "w") as f:
            w = csv.DictWriter(f, COLS, delimiter="\t", extrasaction="ignore")
            w.writeheader()
            for r in sorted(res, key=lambda r: (r["verdict"], r["id"])):
                w.writerow(r)
    tally = {}
    for r in res:
        tally[r["verdict"]] = tally.get(r["verdict"], 0) + 1
    sys.stderr.write("\n")
    for k in sorted(tally, key=lambda k: -tally[k]):
        sys.stderr.write("  %-14s %d\n" % (k, tally[k]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
