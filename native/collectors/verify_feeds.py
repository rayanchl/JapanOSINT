#!/usr/bin/env python3
"""Verify candidate OSINT endpoints are genuinely live and parseable.

Input : TSV on stdin or a file — one candidate per line, fields:
        proposed_id <TAB> url [<TAB> anything else, ignored]
        Lines starting with '#' and blank lines are skipped.
Output: TSV to stdout — id, url, verdict, kind, items, http, bytes, note

A candidate PASSES only if the endpoint returns 2xx AND the body parses as a
feed/dataset AND that parse yields at least one item. This is deliberately
strict: the whole point of the exercise is that the tree already contains
sources that pass every row-count sweep while fetching nothing.

No third-party deps — this has to run in CI and in a bare WSL image.
"""
import sys, json, csv, io, re, gzip, socket
import urllib.request, urllib.error
from urllib.parse import urlsplit, urlunsplit, quote
from concurrent.futures import ThreadPoolExecutor

TIMEOUT = 25
UA = ("Mozilla/5.0 (compatible; JapanOSINT-verify/1.0; "
      "+https://github.com/) Python-urllib")
MAXBYTES = 8 * 1024 * 1024      # a feed that big is not a feed

# Refusals that arrive as HTTP 200. Kept narrow on purpose: a real feed may
# legitimately contain the word "unauthorized" in an article title, so this is
# only consulted for documents small enough to be a bare status envelope.
REFUSAL = re.compile(
    r"access denied|invalid or missing|api[ _-]?key|unauthori[sz]ed|"
    r"forbidden|not authori[sz]ed|quota exceeded|rate limit|"
    r"subscription required|token (?:is )?(?:invalid|required|expired)",
    re.I)

# urllib puts the URL straight into the request line, which must be ASCII. Our
# manifests legitimately carry non-ASCII URLs — a CKAN portal is queried as
# `?q=防災`, and French portals carry accented facet values — and those used to
# die inside fetch() as `UnicodeEncodeError`, caught by verify()'s generic
# handler and recorded as a rejection. That read as "endpoint is dead" when the
# endpoint was never contacted: 208 of batch 14's 457 rejections were this bug.
# Percent-encode instead. Idempotent, because '%' is in every `safe` set, so an
# already-encoded URL passes through byte-identical.
def encode_url(url):
    p = urlsplit(url)
    host = p.netloc
    if any(ord(c) > 127 for c in host):
        try:
            host = host.encode("idna").decode("ascii")
        except UnicodeError:
            pass                       # let the request fail honestly as NET_ERR
    return urlunsplit((
        p.scheme, host,
        quote(p.path,     safe="/%:@!$&'()*+,;=~-._"),
        quote(p.query,    safe="/%:@!$&'()*+,;=~-._?"),
        quote(p.fragment, safe="/%:@!$&'()*+,;=~-._?"),
    ))

def fetch(url):
    req = urllib.request.Request(encode_url(url), headers={
        "User-Agent": UA,
        "Accept": "application/rss+xml, application/atom+xml, application/xml, "
                  "application/json, application/geo+json, text/csv, */*",
        "Accept-Encoding": "gzip",
    })
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        raw = r.read(MAXBYTES + 1)
        if r.headers.get("Content-Encoding", "") == "gzip":
            try:
                raw = gzip.decompress(raw)
            except Exception:
                pass
        return r.status, r.headers.get("Content-Type", ""), raw

def count_feed(text):
    """RSS <item> / Atom <entry>. Regex, not a parser: many real government
    feeds are technically malformed XML and a strict parser rejects feeds that
    every reader in the world displays fine."""
    items = len(re.findall(r"<item[\s>]", text, re.I))
    if items:
        return "rss", items
    entries = len(re.findall(r"<entry[\s>]", text, re.I))
    if entries:
        return "atom", entries
    return None, 0

def count_json(text):
    try:
        doc = json.loads(text)
    except Exception:
        return None, 0
    if isinstance(doc, list):
        return "json-array", len(doc)
    if isinstance(doc, dict):
        # ArcGIS and several OGC servers answer HTTP 200 with an error
        # DOCUMENT. Without this, {"error":{"code":400,"details":[...]}} scores
        # as a healthy feed named "json:error.details" — a live-looking source
        # that fetches nothing, which is the exact failure mode this harness
        # exists to catch. Found by the aviation/maritime discovery pass.
        err = doc.get("error")
        if isinstance(err, (dict, str)) and len(doc) <= 3:
            return "__error__", 0
        if doc.get("status") in ("error", "ERROR", "failed"):
            return "__error__", 0
        # Auth/quota refusals served as HTTP 200 with a tiny JSON body. GIE's
        # gas-storage APIs do exactly this: {"error":"access denied",
        # "message":"Invalid or missing API key"} parses as a two-key document
        # and would otherwise be catalogued as a working keyless source.
        if len(doc) <= 4:
            blob = " ".join(str(v) for v in doc.values() if isinstance(v, str))
            if REFUSAL.search(blob):
                return "__error__", 0
        # GeoJSON first — it is the shape the map layers want.
        if doc.get("type") == "FeatureCollection" and isinstance(doc.get("features"), list):
            return "geojson", len(doc["features"])
        # Otherwise the longest list of dicts anywhere shallow in the doc.
        best, bestk = 0, None
        for k, v in doc.items():
            if isinstance(v, list) and v:
                if len(v) > best:
                    best, bestk = len(v), k
            elif isinstance(v, dict):
                for k2, v2 in v.items():
                    if isinstance(v2, list) and len(v2) > best:
                        best, bestk = len(v2), f"{k}.{k2}"
        if best:
            return f"json:{bestk}", best
        # An envelope whose ONLY list is empty is a result set with no results,
        # not a one-record document. `{"total_count": 0, "results": []}` used to
        # fall through to json-object/1 and PASS — a source verified as live
        # that is structurally guaranteed to emit nothing on every run, which is
        # the same silent-empty this harness exists to catch. Real single-object
        # documents (a status/summary doc with no list at all) still pass.
        if any(isinstance(v, list) for v in doc.values()) or \
           any(isinstance(v, dict) and any(isinstance(x, list) for x in v.values())
               for v in doc.values()):
            return "__empty__", 0
        if doc:
            return "json-object", 1
    return None, 0

def count_csv(text):
    try:
        head = text[:200000]
        sniff = csv.Sniffer().sniff(head[:4096])
        rows = list(csv.reader(io.StringIO(head), sniff))
        return ("csv", len(rows) - 1) if len(rows) > 1 else (None, 0)
    except Exception:
        return None, 0

def verify(rec):
    sid, url = rec
    try:
        status, ctype, raw = fetch(url)
    except urllib.error.HTTPError as e:
        return (sid, url, "HTTP_ERR", "", 0, e.code, 0, str(e.reason)[:60])
    except urllib.error.URLError as e:
        return (sid, url, "NET_ERR", "", 0, 0, 0, str(e.reason)[:60])
    except socket.timeout:
        return (sid, url, "TIMEOUT", "", 0, 0, 0, "timeout")
    except Exception as e:
        return (sid, url, "ERR", "", 0, 0, 0, f"{type(e).__name__}: {e}"[:60])

    nbytes = len(raw)
    if nbytes > MAXBYTES:
        return (sid, url, "TOO_BIG", "", 0, status, nbytes, "over 8MB")
    try:
        text = raw.decode("utf-8", "replace")
    except Exception:
        return (sid, url, "DECODE_ERR", "", 0, status, nbytes, "")

    kind, items = count_feed(text)
    if not kind:
        kind, items = count_json(text)
    if not kind and ("csv" in ctype.lower() or url.lower().endswith(".csv")):
        kind, items = count_csv(text)

    if kind == "__error__":
        head = re.sub(r"\s+", " ", text[:100])
        return (sid, url, "ERROR_BODY", "", 0, status, nbytes, head)
    if kind == "__empty__":
        head = re.sub(r"\s+", " ", text[:100])
        return (sid, url, "EMPTY_RESULTSET", "", 0, status, nbytes, head)
    if not kind:
        head = re.sub(r"\s+", " ", text[:80])
        return (sid, url, "UNPARSEABLE", "", 0, status, nbytes, head)
    if items < 1:
        return (sid, url, "EMPTY", kind, 0, status, nbytes, "parsed but zero items")
    return (sid, url, "PASS", kind, items, status, nbytes, "")

def main():
    src = open(sys.argv[1], encoding="utf-8") if len(sys.argv) > 1 else sys.stdin
    cands, seen = [], set()
    for line in src:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) < 2:
            continue
        sid, url = parts[0].strip(), parts[1].strip()
        if url in seen:            # dedup: duplicate URLs are duplicate sources
            continue
        seen.add(url)
        cands.append((sid, url))

    w = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    w.writerow(["id", "url", "verdict", "kind", "items", "http", "bytes", "note"])
    npass = 0
    with ThreadPoolExecutor(max_workers=24) as ex:
        for row in ex.map(verify, cands):
            w.writerow(row)
            if row[2] == "PASS":
                npass += 1
            sys.stdout.flush()
    print(f"# {npass}/{len(cands)} PASS", file=sys.stderr)

if __name__ == "__main__":
    main()
