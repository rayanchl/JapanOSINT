#!/usr/bin/env python3
"""Proof-of-life probe for a batch manifest, honouring each row's own headers.

Why this is not just collectors/verify_feeds.py. That tool sends one fixed
User-Agent. Several of the highest-value endpoints refuse it and answer 403:
SEC EDGAR requires a UA naming a contact, and a few government portals reject
the default urllib signature. Probing those with the generic UA records them as
dead endpoints when they are not -- the same false-rejection class as the
non-ASCII URL bug documented in CLAUDE.md, which alone was discarding 196 live
sources.

So a row may declare `header1=...` / `header2=...` in its opts column, and this
prober sends exactly the headers the generated collector will send. The verdict
logic is NOT reimplemented -- it is imported from verify_feeds so that a row
passing here passes by exactly the criteria the rest of the tree uses:
2xx, parses in its declared shape, and yields at least one real record, with an
empty result set counted as a failure rather than a one-record document.

Usage:
  probe_hp_batch.py MANIFEST... [--out results.tsv] [--pass-ids ids.txt]
                                [--jobs N] [--only ID,ID]
"""
import os
import sys
import csv
import argparse
import urllib.request
import urllib.error
import collections
import re
import socket
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(NATIVE, "collectors"))
sys.path.insert(0, HERE)

import verify_feeds as VF          # noqa: E402  (verdict logic, reused verbatim)
from gen_hp_batch import load, split_opts   # noqa: E402  (manifest parser, reused)

# verify_feeds' UA ends in a literal "Python-urllib" token, and several hosts
# block on that token alone: opendata.gov.jo answered HTTP 451 and boi.org.il
# served an SPA shell to it, while the identical request without it returned
# JSON. Five rows in this batch were false rejections for that reason before it
# was found. This still says exactly what we are and gives a contact — it drops
# only the library name, which is not information the operator needs and is the
# one part they are filtering on.
DEFAULT_UA = "Mozilla/5.0 (compatible; JapanOSINT-research/1.0; +https://github.com/)"

# verify_feeds caps a body at 8 MB on the reasoning that "a feed that big is not
# a feed". That was true of the RSS-shaped sources it was written for, but this
# tree has since grown lib/jsonstream.c precisely so that bulk registers in the
# tens of megabytes ARE reachable -- the MITRE ATT&CK STIX bundle, the
# Exploit-DB index and the CIRCL CVE dump are all real, live, record-dense
# sources that the engine streams without difficulty. Rejecting them here would
# record a working endpoint as dead, which is the same false-negative class the
# non-ASCII-URL bug produced. The cap is raised, not removed: a body over this
# is still refused rather than held in memory.
MAXBYTES = 64 * 1024 * 1024


def row_headers(r):
    """The headers this row's generated collector will send."""
    hdrs = {}
    for kv in split_opts(r["opts"]):
        if not kv:
            continue
        k, _, v = kv.partition("=")
        if k.strip() in ("header1", "header2", "header3"):
            name, _, val = v.partition(":")
            if name.strip():
                hdrs[name.strip()] = val.strip()
    return hdrs


def fetch(url, extra):
    """Same shape as verify_feeds.fetch, plus this row's declared headers."""
    headers = {
        "User-Agent": DEFAULT_UA,
        "Accept": "*/*",
        "Accept-Encoding": "gzip",
    }
    headers.update(extra)
    req = urllib.request.Request(VF.encode_url(url), headers=headers)
    with urllib.request.urlopen(req, timeout=VF.TIMEOUT) as resp:
        raw = resp.read(MAXBYTES + 1)
        if resp.headers.get("Content-Encoding") == "gzip":
            import gzip
            try:
                raw = gzip.decompress(raw)
            except Exception:
                pass
        return resp.status, resp.headers.get("Content-Type", ""), raw


def count_xml(text):
    """Count records in a plain XML document.

    verify_feeds understands RSS and Atom and nothing else, so a document that
    is XML but not a feed falls through every branch and is recorded as
    UNPARSEABLE. That is wrong for a whole class of primary sources: the UN
    Security Council consolidated list, Switzerland's SECO list and Canada's
    SEMA list are all published as plain XML and are among the most
    authoritative sanctions files in existence.

    A record-bearing XML document has one element repeated under a common
    parent -- <sanctionsEntry>, <individual>, <record>. Find the parent with
    the most same-tag children and report that count. A configuration or error
    document has no such repetition and scores 0.
    """
    import xml.etree.ElementTree as ET
    try:
        root = ET.fromstring(text)
    except Exception:
        return None, 0
    best, bestname = 0, None
    for parent in root.iter():
        tally = collections.Counter(
            c.tag.rsplit("}", 1)[-1] for c in list(parent))
        if tally:
            tag, n = tally.most_common(1)[0]
            if n > best:
                best, bestname = n, tag
    if best < 2:
        return None, 0
    return "xml:%s" % bestname, best


def _looks_like_html(text):
    """True when a body is a rendered page rather than data.

    Content-type is the primary signal, but some challenge pages send
    text/plain, so sniff the opening bytes too.
    """
    head = text[:400].lstrip().lower()
    return head.startswith(("<!doctype html", "<html", "<head")) or \
        ("<html" in head and "<body" in text[:2000].lower())


def is_enveloped_error(text):
    """True for an error document that JSON-shape counting would call a record.

    verify_feeds.count_json only inspects *objects* for the refusal shapes, so
    an error delivered as a one-element ARRAY slips past it and is counted as a
    perfectly good single-record response. The World Bank API does exactly this:

        [{"message":[{"id":"175","key":"Invalid format",
                      "value":"The indicator was not found..."}]}]

    is 128 bytes, parses cleanly, and counts as json-array/1 -- so four rows in
    this batch were "verified" against indicators that do not exist. That is the
    same class of failure as the EMPTY_RESULTSET trap in CLAUDE.md: a source
    that looks alive and is structurally guaranteed to emit nothing. Kept
    deliberately narrow so a real one-record array is unaffected.
    """
    import json
    try:
        doc = json.loads(text)
    except Exception:
        return False
    if not isinstance(doc, list) or len(doc) != 1 or not isinstance(doc[0], dict):
        return False
    head = doc[0]
    if set(head) - {"message", "messages", "error", "errors"}:
        return False
    for v in head.values():
        blob = json.dumps(v)
        if VF.REFUSAL.search(blob) or "not found" in blob.lower() \
           or "invalid" in blob.lower():
            return True
    return False


def verify(r):
    """Verdict for one manifest row. Mirrors verify_feeds.verify exactly."""
    sid, url = r["id"], r["probe"]
    try:
        status, ctype, raw = fetch(url, row_headers(r))
    except urllib.error.HTTPError as e:
        return (sid, url, "HTTP_ERR", "", 0, e.code, 0, str(e.reason)[:60])
    except urllib.error.URLError as e:
        return (sid, url, "NET_ERR", "", 0, 0, 0, str(e.reason)[:60])
    except socket.timeout:
        return (sid, url, "TIMEOUT", "", 0, 0, 0, "timeout")
    except Exception as e:
        return (sid, url, "ERR", "", 0, 0, 0, ("%s: %s" % (type(e).__name__, e))[:60])

    nbytes = len(raw)
    if nbytes > MAXBYTES:
        return (sid, url, "TOO_BIG", "", 0, status, nbytes, "over probe cap")
    text = raw.decode("utf-8", "replace")

    if is_enveloped_error(text):
        return (sid, url, "ERROR_BODY", "", 0, status, nbytes,
                text[:80].replace("\n", " "))

    kind, items = VF.count_feed(text)
    if not kind:
        kind, items = VF.count_json(text)
    if not kind and ("csv" in ctype.lower() or url.lower().endswith(".csv")):
        kind, items = VF.count_csv(text)
    # plain XML: not a feed, but a great many primary registers publish as XML
    if not kind and ("xml" in ctype.lower() or text.lstrip()[:5] == "<?xml"):
        kind, items = count_xml(text)
    # A plain-text line list (blocklists, exit-node lists) is a real dataset the
    # engine reads in CSV mode; verify_feeds calls it UNPARSEABLE because it only
    # sniffs CSV on a csv content-type. Honour the row's own declared mode.
    if not kind and r["mode"] == "csv":
        # ...but only when the body is not HTML. A bot wall is served as
        # HTTP 200 text/html and splits into plenty of non-empty lines, so this
        # fallback happily counted one as a record-bearing text list:
        # security-tracker.debian.org answers every path with an identical
        # 2.8 KB Varnish proof-of-work challenge, and four rows in this batch
        # would have shipped "verified" against it. A CSV or blocklist is never
        # served as text/html, so the content-type alone settles it.
        if "html" in ctype.lower() or _looks_like_html(text):
            return (sid, url, "HTML_CHALLENGE", "", 0, status, nbytes,
                    text[:80].replace("\n", " "))
        lines = [x for x in text.splitlines() if x.strip() and not x.startswith("#")]
        if len(lines) > 1:
            kind, items = "text-lines", len(lines)

    # HP_HTML rows: a server-rendered listing. verify_feeds has no HTML path at
    # all, so every scraped source it saw was UNPARSEABLE and the whole modality
    # was unverifiable -- which is why earlier batches shipped HTML rows on
    # nothing but the author's word. Count the anchors the engine itself would
    # keep: real <a href> hits, filtered by href_must when the row declares one.
    # Navigation chrome is excluded the same way hpengine excludes it, so a page
    # that renders but lists nothing scores 0 and is rejected.
    if not kind and r["mode"] == "html":
        opts = dict(kv.split("=", 1) for kv in
                    (x.strip() for x in r["opts"].split(";")) if "=" in kv)
        must = opts.get("href_must", "")
        hrefs = re.findall(r'<a\b[^>]*\bhref\s*=\s*["\']([^"\'#]+)["\']',
                           text, re.I)
        hits = [h for h in hrefs if (not must or must in h)]
        hits = [h for h in hits if not h.lower().startswith(
            ("javascript:", "mailto:", "tel:"))]
        if hits:
            kind, items = "html-anchors", len(set(hits))

    if kind == "__error__":
        return (sid, url, "ERROR_BODY", "", 0, status, nbytes, text[:80].replace("\n", " "))
    if kind == "__empty__":
        return (sid, url, "EMPTY_RESULTSET", "", 0, status, nbytes, text[:80].replace("\n", " "))
    if not kind:
        return (sid, url, "UNPARSEABLE", "", 0, status, nbytes, text[:80].replace("\n", " "))
    if items < 1:
        return (sid, url, "EMPTY", kind, 0, status, nbytes, "parsed but zero items")
    return (sid, url, "PASS", kind, items, status, nbytes, "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifests", nargs="+")
    ap.add_argument("--out")
    ap.add_argument("--pass-ids")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--only", help="comma-separated ids to probe")
    a = ap.parse_args()

    rows = load(a.manifests)
    if a.only:
        want = set(x.strip() for x in a.only.split(","))
        rows = [r for r in rows if r["id"] in want]
    sys.stderr.write("probing %d rows\n" % len(rows))

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        results = list(ex.map(verify, rows))

    out = open(a.out, "w", encoding="utf-8", newline="") if a.out else sys.stdout
    w = csv.writer(out, delimiter="\t", lineterminator="\n")
    w.writerow(["id", "url", "verdict", "kind", "items", "http", "bytes", "note"])
    npass = 0
    for row in results:
        w.writerow(row)
        if row[2] == "PASS":
            npass += 1
    if a.out:
        out.close()

    if a.pass_ids:
        with open(a.pass_ids, "w", encoding="utf-8", newline="\n") as fh:
            for row in results:
                if row[2] == "PASS":
                    fh.write(row[0] + "\n")

    sys.stderr.write("# %d/%d PASS\n" % (npass, len(results)))
    for row in results:
        if row[2] != "PASS":
            sys.stderr.write("  %-32s %-16s %s\n" % (row[0], row[2], str(row[7])[:60]))


if __name__ == "__main__":
    main()
