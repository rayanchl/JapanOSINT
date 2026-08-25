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
import json
import re
import socket
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(NATIVE, "collectors"))
sys.path.insert(0, HERE)

import verify_feeds as VF          # noqa: E402  (verdict logic, reused verbatim)
from gen_hp_batch import load               # noqa: E402  (adds the mode/want check)
from manifest import parse_opts, opt        # noqa: E402  (THE opts parser)

# verify_feeds' UA ends in a literal "Python-urllib" token, and several hosts
# block on that token alone: opendata.gov.jo answered HTTP 451 and boi.org.il
# served an SPA shell to it, while the identical request without it returned
# JSON. Five rows in this batch were false rejections for that reason before it
# was found. This still says exactly what we are and gives a contact — it drops
# only the library name, which is not information the operator needs and is the
# one part they are filtering on.
def _engine_user_agent():
    """The User-Agent the ENGINE will send, read from core/httpclient.h.

    This prober's whole promise is that a row is never verified under different
    request conditions than the ones its collector will actually use — and it
    was breaking that promise on the single most consequential header. It sent
    `Mozilla/5.0 (compatible; JapanOSINT-research/1.0; …)` while every hp row at
    runtime sends JO_USER_AGENT, so 1,623 of batch 19's 1,810 rows (the ones not
    declaring a header of their own) were proven against a string they will
    never send. Measured, the difference decides the response rarely — 13 of 14
    HTTP-error rows answered identically to both — but "rarely" is not "never",
    and IXF_TORIX answers 200 to one and 403 to the other.

    Parsed from the header rather than copied, so the two cannot drift apart
    again. A row that genuinely needs a different UA declares header1, which the
    engine then honours — that is the supported way to ask for one.
    """
    path = os.path.join(NATIVE, "core", "httpclient.h")
    try:
        src = open(path, encoding="utf-8").read()
    except OSError:
        return None
    m = re.search(r"#define\s+JO_USER_AGENT\s*((?:\\\s*\n|.)*)", src)
    if not m:
        return None
    parts = re.findall(r'"([^"]*)"', m.group(1))
    return "".join(parts) or None


DEFAULT_UA = _engine_user_agent() or \
    "JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"

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
    """The headers this row's generated collector will send.

    Read through manifest.parse_opts — the ONE opts parser — so a probe can
    never be sent under different request conditions than the generator emits
    into C because the two split the field differently. That is not
    hypothetical: the `\\;` escape exists precisely because header values
    contain semicolons."""
    hdrs = {}
    o, _dups, _junk = parse_opts(r["opts"])
    for k in ("header1", "header2", "header3"):
        v = o.get(k)
        if not v:
            continue
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


# ── is the row's filter actually honoured? ───────────────────────────────────
#
# A 2xx that parses and yields records proves the endpoint answered. It does NOT
# prove it answered THE QUESTION. Three APIs in batch 21 accept a filter,
# silently ignore it, and return the whole unfiltered collection with HTTP 200:
#
#   * EPA Envirofacts, given a column that does not exist
#     (tri_reporting_form/facility_name) -> 10,000 unrelated records
#   * the German BMJ portal, on court / documentNumber / dateFrom
#   * Health Canada MDALL, on licence_id -> the entire 21 MB table
#
# Every existing gate passes those rows: probe PASS, emit OK, stored == emitted.
# But the row is an ENTITY PIVOT, so an analyst asking "what do we have on X"
# receives thousands of records about everything else, attributed to X. That is
# worse than a source that returns nothing — it is a confident wrong answer, and
# no amount of record counting can see it.
#
# The check is cheap: ask the same endpoint about an entity that cannot exist.
# A working filter returns nothing (or something much smaller). A filter being
# ignored returns the same collection it just returned for the real entity.
IMPOSSIBLE = {
    "q":  "zzqx9nonexistent7q",
    "qd": "99999999999999999",
    "qh": "zzqx9nonexistent7q.invalid",
    "ql": "zzqx9nonexistent7q",
    "qU": "ZZQX9NONEXISTENT7Q",
    "Q":  "zzqx9nonexistent7q",
}
TOKEN_RE = re.compile(r"\{(q[dhlU]?|Q)\}")

# Set from --check-filter. Off by default because it doubles the request count
# for every pivot row; a batch should be run through it at least once.
CHECK_FILTER = False


def impossible_probe_url(r):
    """The row's URL template with every entity token replaced by a value that
    cannot match anything. None when the row takes no entity (a bulk file has
    no filter to honour, so there is nothing to check)."""
    tmpl = r.get("url") or ""
    if not TOKEN_RE.search(tmpl):
        return None
    return TOKEN_RE.sub(lambda m: IMPOSSIBLE.get(m.group(1), IMPOSSIBLE["q"]), tmpl)


def filter_is_honoured(r, real_items):
    """(ok, note). ok=False means the endpoint returned substantially the same
    result set for an impossible entity as for the real one."""
    url = impossible_probe_url(r)
    if not url or real_items < 2:
        return True, ""          # nothing to compare against
    try:
        status, ctype, raw = fetch(url, row_headers(r))
    except Exception:
        return True, ""          # a refusal here is not evidence either way
    if status < 200 or status >= 300:
        return True, ""
    text = raw.decode("utf-8", "replace")
    kind, items = VF.count_feed(text)
    if not kind:
        kind, items = VF.count_json(text)
    if not kind:
        return True, ""
    if not isinstance(items, int) or items < 1:
        return True, ""          # empty for a nonsense entity == filter works
    # Same-sized answer for a nonsense entity: the filter is not being applied.
    # 90% rather than equality because a few APIs pad a collection differently
    # between calls, and a genuine filter never lands within 10% of the whole.
    if items >= real_items * 0.9:
        return False, ("filter ignored: an impossible entity returned %d records "
                       "vs %d for the real one" % (items, real_items))
    return True, ""


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

    # A DECLARED array_path beats every heuristic below.
    #
    # VF.count_json() hunts for the densest array in the document, which is a
    # reasonable guess when the row says nothing — and a wrong answer whenever
    # the row HAS said something. OPENPAY_RESEARCH_2024 answered
    #     {"results":[], "count":0, "query":{"properties":{ …252 keys… }}}
    # and scored `PASS json:query.properties 252`: the prober counted the
    # response's own SCHEMA BLOCK as records and passed a row whose result set
    # was empty. Only audit_batch_emit's EMPTY_UPSTREAM caught it later.
    #
    # The engine reads exactly one place — the declared array_path — so the
    # probe must judge exactly that place, and an empty one is an
    # EMPTY_RESULTSET, not a pass on some other array that happens to be
    # nearby. This is the same rule the CLAUDE.md notes already state for the
    # engine: a declared path that does not resolve means the response is not
    # the shape the row expects, and guessing is what produces the false pass.
    kind = items = None
    # manifest.opt(), not parse_opts(): it returns None for a DUPLICATED key,
    # which is the right answer here — a row that declares array_path twice has
    # no value this can act on, so fall through to the heuristics rather than
    # pick one and judge the row against it.
    ap_decl = opt(r, "array_path")
    if r["mode"] == "json" and ap_decl:
        try:
            doc = json.loads(text)
        except Exception:
            return (sid, url, "UNPARSEABLE", "", 0, status, nbytes,
                    "declared array_path but body is not JSON")
        node = doc
        for seg in ap_decl.split("."):
            if isinstance(node, dict) and seg in node:
                node = node[seg]
            else:
                node = None
                break
        if node is None:
            return (sid, url, "PATH_UNRESOLVED", "", 0, status, nbytes,
                    "array_path %r not present in the response" % ap_decl)
        if not isinstance(node, list):
            return (sid, url, "PATH_NOT_ARRAY", "", 0, status, nbytes,
                    "array_path %r is %s, not an array" % (ap_decl, type(node).__name__))
        if not node:
            return (sid, url, "EMPTY_RESULTSET", "json:" + ap_decl, 0, status,
                    nbytes, "declared array_path resolved to an empty array")
        kind, items = "json:" + ap_decl, len(node)

    if not kind:
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

    # Answering is not answering THE QUESTION — see filter_is_honoured().
    if CHECK_FILTER:
        ok, why = filter_is_honoured(r, items)
        if not ok:
            return (sid, url, "FILTER_IGNORED", kind, items, status, nbytes, why)

    return (sid, url, "PASS", kind, items, status, nbytes, "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifests", nargs="+")
    ap.add_argument("--out")
    ap.add_argument("--pass-ids")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--only", help="comma-separated ids to probe")
    ap.add_argument("--check-filter", action="store_true",
                    help="for every entity-pivot row, ask the endpoint about an "
                         "IMPOSSIBLE entity as well and fail it (FILTER_IGNORED) "
                         "when the answer is the same size. Catches an API that "
                         "accepts a filter, ignores it, and returns the whole "
                         "collection with HTTP 200 — which every other gate "
                         "passes. Doubles the request count for pivot rows, so "
                         "it is opt-in; run it at least once per batch.")
    a = ap.parse_args()
    global CHECK_FILTER
    CHECK_FILTER = a.check_filter

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
