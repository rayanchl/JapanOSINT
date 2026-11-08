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
import sys
import time
import threading
from concurrent.futures import ThreadPoolExecutor

# Run under `python3 -I`, which strips this script's own directory from
# sys.path, so a sibling module has to be found explicitly.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jo_tls                                                     # noqa: E402
import jo_http                                                    # noqa: E402  (THE request conditions)
import ctable                                                     # noqa: E402  (THE C row reader)
import hpjson                                                     # noqa: E402  (THE engine JSON model)

_lock = threading.Lock()
# TLS is tools/jo_tls.py, through jo_http: Python's default refuses handshakes
# the ENGINE completes, which made this sweep report 27 live rows as dead.

# An entity token makes the row a pivot; page 2 of no entity means nothing.
ENTITY = re.compile(r"\{q[a-z]*\}|\{cik|\{key\}")
COLS = ["verdict", "id", "page_param", "page_size", "http1", "http2",
        "recs1", "recs2", "host", "message", "url2"]

# The upstream's own words, grouped by the remedy they imply. Matched against
# the page-2 body so the verdict carries a remedy and not just a status.
#; A page-2 failure that means "there is no page 2 because the collection
# ended" — NOT a refusal to page. Matched on the error CODE first, because a
# code is language-independent and the message is not: WordPress answers
# `rest_post_invalid_page_number` with the text in the site's own language, and
# the first run of this sweep reported 18 REFUSED of which 17 were Japanese
# municipal sites saying "リクエストされたページ数は存在するページ数を上回って
# います。" — "the requested page exceeds the number of existing pages". A gate
# whose findings are mostly benign is a gate nobody reads.
BENIGN_CODE = re.compile(
    r"invalid_page_number|rest_post_invalid_page|page_out_of_range|"
    r"invalid_offset|no_more_(results|pages)|out_of_bounds", re.I)
BENIGN_MSG = re.compile(
    r"exceeds the (number of )?(existing |available )?pages?|"
    r"page number .* (exceeds|greater than|out of)|beyond the (last|end)|"
    r"no (more|further) (results|records|pages)|"
    r"存在するページ数を上回|ページが存在しません", re.I)

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
    """(id, host, url, page_param, page_size, …) for scheduled rows that
    declare offset/page-number paging, read with tools/ctable.py — so a .url
    split over adjacent literals or spelled through a string macro is ONE
    string (this used to regex the first literal and probed 26 rows at a
    fragment), and a row's span is its own braces rather than "up to the next
    `{ .id =`". The row's own .headers, mode and array_path come with it: the
    request is the engine's (jo_http) and page 1's records are counted where
    the engine reads them."""
    out = []
    for r in ctable.iter_rows(tree, need="page_param"):
        pp = r.s("page_param")
        if not pp:
            continue
        if r.i("interval") <= 0:
            continue          # a pivot, not a scheduled walk
        url = r.s("url")
        if not url:
            continue
        if ENTITY.search(url) or "{page" in url or "{offset" in url:
            continue          # pivot, or a token this tool cannot bind
        if "{date" in url or "{ago" in url or "{wstart" in url:
            continue          # a dated window; its own rules apply
        hdrs = r.get("headers") if isinstance(r.get("headers"), list) else []
        out.append({
            "id": r.s("id"), "url": url,
            "host": ctable.host_of(url),
            "page_param": pp,
            "page_size": r.i("page_size"),
            "zero_based": r.i("page_zero_based") == 1,
            "page_start": r.i("page_start"),
            "headers": hdrs,
            "mode": (r.s("mode") or "HP_JSON").replace("HP_", "").lower(),
            "array_path": r.s("array_path"),
        })
    return out


def set_param(url, key, val):
    """hp_url_set_param exactly: replace the FIRST `?key=` / `&key=` (its
    value ends at `&` or `#`), else append. It used to replace EVERY
    occurrence, so a URL binding the key twice was asked a question the
    engine never asks (the engine binds the first and leaves the rest)."""
    q = url.find("?")
    p = q
    while p >= 0:
        cand = p + 1
        if url.startswith(key + "=", cand):
            end = cand + len(key) + 1
            while end < len(url) and url[end] not in "&#":
                end += 1
            return url[:cand] + "%s=%s" % (key, val) + url[end:]
        p = url.find("&", p + 1)
    return url + ("&" if q >= 0 else "?") + "%s=%s" % (key, val)


def url_num_param(url, key):
    """hp_url_num_param: the integer the URL binds to `key` (first
    occurrence, the WHOLE value digits), else None."""
    p = url.find("?")
    while p >= 0:
        cand = p + 1
        if url.startswith(key + "=", cand):
            m = re.match(r"(\d+)(?=$|[&#])", url[cand + len(key) + 1:])
            return int(m.group(1)) if m else None
        p = url.find("&", p + 1)
    return None


def offset_named(param):
    """hpengine.c's offset test: the name contains offset/start/skip,
    case-insensitively (resultOffset, Start, $skip, startIndex)."""
    pl = (param or "").lower()
    return "offset" in pl or "start" in pl or "skip" in pl


def page_two_value(r, page1_records):
    """The value hp_run puts in page_param for the SECOND request.

      page_start: unset (0) and not page_zero_based -> an OFFSET-named param
                  starts at the value the URL binds (or 0); any other param
                  is coerced to 1
      step:       page_size, else the number of records page 1 returned
      offset style (page_size set, or the name is offset-shaped):
                  page_start + step;  page-number style: page_start + 1
    It used to treat an offset-named param with no page_size as a page
    NUMBER (`start=1`), so a `start`/`$skip` row's page 2 was record 1 —
    an overlap with page 1 the engine never asks for."""
    ps = r["page_start"]
    named = offset_named(r["page_param"])
    if not ps and not r["zero_based"] and named:
        v0 = url_num_param(r["url"], r["page_param"])
        if v0 and 0 < v0 < 100000000:
            ps = v0
    elif not ps and not r["zero_based"]:
        ps = 1
    step = r["page_size"] if r["page_size"] > 0 else page1_records
    if r["page_size"] > 0 or named:
        return ps + step
    return ps + 1


# A transport failure that is OURS, not the upstream's — measured, in both of
# the two shapes it takes here.
#
# "Connection refused" names a socket that was never opened, so it says
# nothing about the server at the other end. The first full run of this sweep
# recorded 569 of them as PAGE1_FAILED, all in its last 15% and spread across
# wholly unrelated hosts (a Japanese university's WordPress, the EIB, NHTSA);
# both of the two re-tested by hand answered 200 immediately afterwards.
#
# "Connection reset by peer" LOOKS like the upstream's own refusal, and the
# first reading of it here was that a reset is the peer's action and so belongs
# to the row. That was wrong in this environment, and worth the one command it
# took to find out: curl against the same hosts reports HTTP 000, and the agent
# proxy names the cause itself — ws_closed_mid_exchange, "the tunnel to the
# egress proxy closed before the exchange completed". Confirm with
#     curl -sS "$HTTPS_PROXY/__agentproxy/status"
# The hosts are not denied by policy: each answers 200 on its own. It is the
# tunnel's capacity under many concurrent connections.
#
# Both are the same shape as audit_cycle_health.py's load-made STUCK verdicts,
# one layer up: a verdict that changes with the tool's own concurrency is not a
# measurement. They are retried with a back-off, and if they persist they are
# reported as LOCAL_EGRESS so that nobody can read one as a finding about a
# source. A row that only ever comes back LOCAL_EGRESS is UNMEASURED from
# here, which is a thing to write down rather than a verdict to act on.
# How much of a body to read. It was 400 KB, which truncated an ArcGIS page of
# 300 features and a CKAN page of 48 packages — and a truncated body cannot be
# counted, so both became "records unknown" and neither the short-page rule nor
# the empty-page test could run on them. 8 MB holds every page this sweep has
# met; a body past it is genuinely too large to count and says so (-1) instead
# of guessing, which is what the old `s.count("},")` fallback did.
MAXREAD = 8 * 1024 * 1024

_LOCAL_EGRESS = re.compile(
    r"Connection refused|Connection reset by peer|Temporary failure in name "
    r"resolution|Cannot assign requested address|Too many open files", re.I)


def get(url, timeout=60, tries=3, headers=None, mode="json"):
    """(status, body) for one request with the ENGINE's request conditions
    (jo_http: the row's .headers, the per-host agent table, the engine's
    Accept for the row's mode). This used to send its own agent and Accept
    to every row and ignored .headers, so a header-authenticated or
    agent-walled row was asked under conditions its collector never uses."""
    last = b""
    for attempt in range(tries):
        st, _ct, raw, err = jo_http.fetch_any(url, headers, timeout, None, MAXREAD,
                                              pacer=None, mode=mode)
        if st >= 0:
            return st, raw[:MAXREAD] if st == 200 else raw[:4000]
        last = err[:200].encode()
        if not _LOCAL_EGRESS.search(err):
            return -1, last
        # Back off: the point is to stop ASKING for a while, since the
        # refusal is on this side of the wire.
        if attempt + 1 < tries:
            time.sleep(2.0 * (attempt + 1))
    return -2, last


# The envelope names a record set is published under, in the order the engine's
# own array_path resolution would try them. Checked BEFORE any search, because
# a search cannot be trusted — see count_records().
_ENVELOPES = (
    ("features",), ("results",), ("result", "results"), ("result",),
    ("data",), ("items",), ("records",), ("value",), ("rows",),
    ("response", "docs"), ("hits", "hits"), ("packages",),
)


def _at(d, path):
    cur = d
    for k in path:
        if not isinstance(cur, dict) or k not in cur:
            return None
        cur = cur[k]
    return cur if isinstance(cur, list) else None


def count_records(blob, array_path=None):
    """How many records a body holds.

    A row that DECLARES an array_path is counted where the engine reads it
    (tools/hpjson.py select_records): a path that does not resolve is 0 —
    the engine emits nothing — never "the longest list elsewhere". The row's
    declared path used to be ignored entirely.

    This used to take the longest list ANYWHERE in the document, described as
    "a floor, only ever compared with ITSELF". Both halves were wrong.

    It is not only compared with itself: the short-page rule compares it with
    the page SIZE the URL asked for, and that comparison is what separates a
    benign PAGE1_IS_ALL from a REFUSED or a SAME_AS_PAGE1 finding. And the
    longest list in an ArcGIS response is a polygon's coordinate RING, not its
    records — `EAD_PROTECTED_AREAS` holds 21 features and was counted as 3,748,
    `AR_DATOS_TRANSPORTE` holds 48 packages (its own `result.count` says so)
    and was counted as 1,232. Over-counting makes a short page look full, so a
    complete collection reads as a finding.

    This is the same array hijack CLAUDE.md records for the ENGINE — "with no
    array_path the engine picks the densest array of OBJECTS, which is not the
    root as soon as a root record carries a longer nested array of its own" —
    in a tool that never got the engine's fix. So: the declared envelope first,
    then the longest array of OBJECTS (a ring is an array of arrays of
    numbers), and never a bare list of scalars found by searching."""
    # str.strip() does not remove a UTF-8 BOM, and a BOM-prefixed JSON body then
    # fell through to the newline count below and read as 0 records.
    s = blob.decode("utf-8", "replace").lstrip("\ufeff").strip()
    if not s:
        return 0
    if s[0] == "<":
        # XML (WFS GML, OAI-PMH, SRU, Atom): records are one element repeated
        # under a common parent — the probe's count_xml rule. Counting NEWLINES
        # read a one-line GeoServer FeatureCollection as 0 records, so a live
        # row came back PAGE1_EMPTY.
        import collections
        import xml.etree.ElementTree as ET
        try:
            root = ET.fromstring(s)
        except ET.ParseError:
            return -1                 # truncated or not XML: UNKNOWN, not 0
        best = 0
        for parent in root.iter():
            tally = collections.Counter(c.tag.rsplit("}", 1)[-1] for c in list(parent))
            if tally:
                best = max(best, tally.most_common(1)[0][1])
        return best
    if s[0] in "[{" and array_path:
        try:
            d = json.loads(s)
        except ValueError:
            return -1
        recs, _how = hpjson.select_records(d, array_path)
        return len(recs)
    if s[0] in "[{":
        try:
            d = json.loads(s)
        except ValueError:
            # A body this tool truncated at MAXREAD cannot be counted, and the
            # old fallback — `s.count("},") + 1` — did not decline, it guessed:
            # 1,232 for a CKAN page holding 48 packages, whose own
            # `result.count` says 48. A fabricated count then feeds the
            # short-page rule and turns a complete collection into a finding.
            # -1 means UNKNOWN and every comparison below guards on it.
            return -1
        if isinstance(d, list):
            return len(d)
        for path in _ENVELOPES:
            got = _at(d, path)
            if got is not None:
                return len(got)
        # No envelope we know: the longest array whose elements are OBJECTS.
        best, stack = 0, [d]
        while stack:
            cur = stack.pop()
            if isinstance(cur, list):
                if cur and all(isinstance(x, dict) for x in cur):
                    best = max(best, len(cur))
                stack.extend(x for x in cur if isinstance(x, (list, dict)))
            elif isinstance(cur, dict):
                stack.extend(cur.values())
        return best
    return max(0, s.count("\n"))


def self_test():
    """The counter's own fixtures, run before any sweep trusts it.

    CLAUDE.md's reason, from the exhaustiveness scanner: a check whose own case
    it has stopped seeing reports zero findings and reads like good news. Both
    of this counter's defects — a polygon ring counted as records, and a
    guessed count for a body that would not parse — produced numbers that
    looked like measurements, so the fixtures pin the shapes rather than the
    code."""
    cases = [
        ("bare root array", b'[{"a":1},{"a":2},{"a":3}]', 3),
        ("CKAN result.results beats a search",
         json.dumps({"result": {"count": 48,
                                "results": [{"i": i} for i in range(48)]}}
                    ).encode(), 48),
        ("ArcGIS: 7 features, not a 900-point ring",
         json.dumps({"features": [{"attributes": {"a": 1},
                                   "geometry": {"rings": [[[0, 0]] * 900]}}
                                  for _ in range(7)]}).encode(), 7),
        ("no envelope: longest array of OBJECTS, not of numbers",
         json.dumps({"weird": {"nums": list(range(5000)),
                               "recs": [{"x": 1}, {"x": 2}]}}).encode(), 2),
        ("a truncated body DECLINES rather than guessing",
         b'{"result":{"results":[{"a":1},{"a":2},{"a":3}', -1),
        ("empty body", b"", 0),
        ("an empty result set is zero, not one", json.dumps({"results": []}).encode(), 0),
        ("csv falls back to lines", b"a,b\n1,2\n3,4\n", 2),
        ("one-line WFS GML: 2 members, not 0 newlines",
         b'<wfs:FeatureCollection xmlns:wfs="w"><wfs:member><a/></wfs:member>'
         b'<wfs:member><a/></wfs:member></wfs:FeatureCollection>', 2),
        ("a BOM before JSON is not a text body",
         b'\xef\xbb\xbf[{"a":1},{"a":2}]', 2),
        ("truncated XML DECLINES", b'<root><r/><r/', -1),
    ]
    # The row's DECLARED array_path is where the engine counts (2026-11-07).
    cases += [
        ("declared array_path: the declared array, not a longer one",
         (json.dumps({"data": {"rows": [{"a": 1}]}, "results": [{"x": 1}] * 5}).encode(),
          "data.rows"), 1),
        ("declared array_path that does not resolve: the engine emits 0",
         (json.dumps({"results": [{"x": 1}] * 5}).encode(), "items"), 0),
    ]
    bad = 0
    for label, blob, want in cases:
        if isinstance(blob, tuple):
            got = count_records(*blob)
            if got != want:
                bad += 1
                print("SELF-TEST FAIL  %-50s got=%s want=%s" % (label, got, want))
            continue
        got = count_records(blob)
        if got != want:
            bad += 1
            print("SELF-TEST FAIL  %-50s got=%s want=%s" % (label, got, want))
    # The request this tool builds is the ENGINE's (hp_url_set_param,
    # hp_url_declared_size and hp_run's page-2 arithmetic).
    row = {"url": "https://e.x/q?start=0&k=1&start=9", "page_param": "start",
           "page_size": 0, "page_start": 0, "zero_based": False}
    for label, got, want in (
        ("set_param replaces the FIRST occurrence only",
         set_param("https://e.x/q?p=1&a=2&p=3", "p", "7"), "https://e.x/q?p=7&a=2&p=3"),
        ("set_param appends when absent", set_param("https://e.x/q", "p", "2"), "https://e.x/q?p=2"),
        ("set_param does not match a longer key", set_param("https://e.x/q?pp=1", "p", "2"),
         "https://e.x/q?pp=1&p=2"),
        ("declared_size reads the engine's list (perPage)", declared_size("https://e.x/?perPage=50"), 50),
        ("declared_size reads `size`", declared_size("https://e.x/?size=25"), 25),
        ("declared_size: not a list the engine lacks (maxFeatures)",
         declared_size("https://e.x/?maxFeatures=40"), 0),
        ("declared_size: a non-integer value is not a size", declared_size("https://e.x/?limit=10a"), 0),
        ("offset-named param without page_size steps by page 1's count",
         page_two_value(row, 100), 100),
        ("page-number param steps by one", page_two_value(dict(row, page_param="page", url="https://e.x/"), 100), 2),
        ("page_size makes it an offset from 1 (engine coerces page_start)",
         page_two_value(dict(row, page_param="page", page_size=50, url="https://e.x/"), 7), 51),
        ("an offset the URL binds is the start",
         page_two_value(dict(row, url="https://e.x/?$skip=200", page_param="$skip"), 50), 250),
        ("zero-based page number", page_two_value(dict(row, page_param="page", zero_based=True,
                                                       url="https://e.x/"), 9), 1),
    ):
        if got != want:
            bad += 1
            print("SELF-TEST FAIL  %-50s got=%r want=%r" % (label, got, want))
    if not _SIZE_KEYS:
        bad += 1
        print("SELF-TEST FAIL  hp_url_declared_size's list not parsed from lib/hpengine.c")
    if bad:
        print("\n%d counter fixture(s) failed — the sweep's verdicts would not "
              "mean anything, so it is not run." % bad)
        return False
    return True


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


def code_of(blob):
    """The upstream's machine-readable error code, if it gave one."""
    try:
        d = json.loads(blob.decode("utf-8", "replace"))
    except Exception:
        return ""
    if not isinstance(d, dict):
        return ""
    for k in ("code", "errorCode", "error_code"):
        v = d.get(k)
        if isinstance(v, str):
            return v
    e = d.get("error")
    if isinstance(e, dict):
        for k in ("code", "errorCode"):
            v = e.get(k)
            if isinstance(v, (str, int)):
                return str(v)
    return ""


# How many records the URL ITSELF asked for — hp_url_declared_size's list,
# READ OUT OF lib/hpengine.c so there is one list (this tool kept its own:
# `maxFeatures`, `page[size]`, `n` that the engine never reads, and no `size`,
# `pagesize`, `perPage`, `take` or `Count`, which 36 rows use and only the
# engine saw). Same semantics: first occurrence, whole value an integer,
# 0 < v < 1,000,000, case-sensitive name.
def _c_size_keys():
    try:
        src = open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                "lib", "hpengine.c"), encoding="utf-8").read()
    except OSError:
        return ()
    m = re.search(r"hp_url_declared_size\(const char \*url\)\s*\{\s*static const char \*SZ\[\]\s*=\s*\{(.*?)\};",
                  src, re.S)
    return tuple(re.findall(r'"([^"]+)"', m.group(1))) if m else ()


_SIZE_KEYS = _c_size_keys()


def declared_size(url):
    for k in _SIZE_KEYS:
        v = url_num_param(url, k)
        if v is not None and 0 < v < 1000000:
            return v
    return 0


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
    u1 = r["url"]
    hdrs, mode, ap = r.get("headers"), r.get("mode", "json"), r.get("array_path") or None

    s1, b1 = get(u1, a.timeout, headers=hdrs, mode=mode)
    out["http1"] = s1
    # -2 is OUR egress refusing, not the upstream (see get()). It is not a
    # finding about this row and must never be counted as one.
    if s1 == -2:
        out["verdict"] = "LOCAL_EGRESS"
        out["message"] = ("this environment's egress tunnel failed, retried "
                          "with a back-off and failed again — NOT a reading "
                          "about this source, which is UNMEASURED from here: "
                          "%s" % message_of(b1))[:240]
        return out
    if s1 != 200:
        out["verdict"] = "PAGE1_FAILED"
        out["message"] = message_of(b1)[:200]
        return out
    n1 = count_records(b1, ap)
    out["recs1"] = n1
    # page 2 as hp_run computes it — which needs page 1's record count when
    # the row declares no page_size (see page_two_value)
    u2 = set_param(r["url"], r["page_param"], str(page_two_value(r, max(n1, 0))))
    out["url2"] = u2[:300]
    if n1 == 0:
        out["verdict"] = "PAGE1_EMPTY"
        return out
    if n1 < 0:
        # Page 1 arrived larger than this tool reads, so it cannot be counted
        # and neither the short-page rule nor an empty-page test means
        # anything. The byte comparison below still does, so say what is known.
        out["verdict"] = ("SAME_AS_PAGE1" if get(u2, a.timeout, headers=hdrs, mode=mode)[1] == b1
                          else "RECS_UNKNOWN")
        out["message"] = ("page 1 is larger than this tool reads (%d bytes), "
                          "so its records cannot be counted; only the "
                          "byte-identity test applies" % len(b1))
        return out

    s2, b2 = get(u2, a.timeout, headers=hdrs, mode=mode)
    out["http2"] = s2
    n2 = count_records(b2, ap)
    out["recs2"] = n2
    msg = message_of(b2)

    if s2 == -2:
        out["verdict"] = "LOCAL_EGRESS"
        out["message"] = ("this environment's egress tunnel failed on page 2, "
                          "retried with a back-off and failed again — NOT a "
                          "reading about this source, which is UNMEASURED "
                          "from here: %s" % msg)[:240]
        return out
    if s2 == -1:
        out["verdict"] = "NET_ERR"
        out["message"] = msg[:200]
        return out
    if s2 != 200:
        # THE ENGINE'S OWN RULE FIRST, and it is the general one: "A failure on
        # the page AFTER a short page (fewer than page_size records) is the
        # upstream's end of data, not a truncation" (CLAUDE.md). A page 1 that
        # came back short of what the URL asked for IS the whole collection, so
        # whatever page 2 answers is not a finding. Measured: a Japanese
        # municipal site returned 7 posts against `per_page=100` and then 400
        # on page 2 — the collection is 7 records.
        want = declared_size(r["url"]) or r["page_size"]
        if want and n1 >= 0 and n1 < want:
            out["verdict"] = "PAGE1_IS_ALL"
            out["message"] = ("page 1 held %d of the %d it asked for, so the "
                              "collection ends inside it" % (n1, want))
            return out
        if BENIGN_CODE.search(code_of(b2)) or BENIGN_MSG.search(msg):
            out["verdict"] = "PAGE1_IS_ALL"
            out["message"] = ("upstream says there is no page 2: %s"
                              % msg)[:220]
            return out
        out["verdict"] = "REFUSED"
        out["message"] = ("%s | %s" % (remedy_of(msg), msg)).strip(" |")[:220]
        return out
    want = declared_size(r["url"]) or r["page_size"]
    if n2 < 0:
        want = 0          # an uncountable page 2 cannot be judged by size
    # An upstream that says in a 200 body that there is no further page.
    if BENIGN_CODE.search(code_of(b2)) or BENIGN_MSG.search(msg):
        out["verdict"] = "PAGE1_IS_ALL"
        out["message"] = ("upstream says there is no page 2: %s" % msg)[:220]
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
        # genuinely different records cannot be byte-identical — so this is a
        # real finding EVEN when page 1 was short, because a short page that
        # repeats is still a walk that re-emits the same records.
        out["verdict"] = "SAME_AS_PAGE1"
        out["message"] = "page 2 is byte-identical to page 1"
        return out
    if n2 == 0:
        # The engine's short-page rule again: an empty page 2 after a page 1
        # that did not fill is the end of the collection, not a shortfall.
        if want and n1 >= 0 and n1 < want:
            out["verdict"] = "PAGE1_IS_ALL"
            out["message"] = ("page 1 held %d of the %d it asked for and page "
                              "2 is empty" % (n1, want))
        else:
            out["verdict"] = "PAGE2_EMPTY"
            out["message"] = ("page 1 filled (%d) but page 2 is empty — worth "
                              "one look" % n1)
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
    ap.add_argument("--self-test", action="store_true",
                    help="run the record counter's fixtures and exit")
    a = ap.parse_args()

    # Fixtures FIRST, always — not only under the flag. A sweep whose counter
    # has stopped seeing its own case produces verdicts that read like
    # measurements, which is how both of this counter's defects survived.
    if not self_test():
        return 2
    if a.self_test:
        print("counter fixtures: all pass")
        return 0

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
