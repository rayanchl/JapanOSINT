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
import urllib.parse
import urllib.error
import collections
import json
import re
import socket
from concurrent.futures import ThreadPoolExecutor

HERE = "/Users/rayan/OSINTsaas/native/tools"
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


def row_timeout(r):
    """Seconds this row's collector will wait: its declared timeout_ms, never
    below the probe's own floor. A row that declares timeout_ms=150000 for a
    host that takes 50 s a page was being probed with 25 s and reported
    TIMEOUT -- a live source read as dead because the probe waited under
    different conditions than the collector does, the same false-negative
    class as the header mismatch row_headers() exists to prevent."""
    o, _dups, _junk = parse_opts(r["opts"])
    try:
        ms = int(o.get("timeout_ms") or 0)
    except ValueError:
        ms = 0
    return max(VF.TIMEOUT, ms / 1000.0)


# ── runtime tokens, mirroring lib/hpengine.c ─────────────────────────────
# {date[±N(d|w|m|y)][:strftime | :@s | :@ms]} rendered in UTC+tz_minutes and
# stepped back `shift` days by date_back; {page} {offset} {offset1}
# {offset_end} {size} take their FIRST-page values (the probe reads page 1).
DATE_RE = re.compile(r"\{(date(?:[+-]\d+[dwmy]?)?(?::[^}]*)?)\}")


def render_date(spec, tz_min=0, shift=0, now=None, step="d"):
    import calendar
    import datetime as _dt
    m = re.fullmatch(r"date(?:([+-])(\d+)([dwmy]?))?(?::(.+))?", spec)
    if not m:
        return None
    sign, num, unit, fmt = m.groups()
    amount = (int(num) * (-1 if sign == "-" else 1)) if num else 0
    unit = unit or "d"
    fmt = fmt or "%Y-%m-%d"
    base = (now if now is not None else _dt.datetime.now(_dt.timezone.utc))
    local = base.astimezone(_dt.timezone.utc).replace(tzinfo=None) + _dt.timedelta(minutes=tz_min)

    def move(d, n, u):                     # hp_shift_days: months clamp
        if u in "my":
            mm = d.month - 1 + (n * 12 if u == "y" else n)
            y, mo = d.year + mm // 12, mm % 12 + 1
            return d.replace(year=y, month=mo, day=min(d.day, calendar.monthrange(y, mo)[1]))
        return d + _dt.timedelta(days=n * (7 if u == "w" else 1))
    local = move(local, amount, unit)
    if shift:
        local = move(local, -shift, step if step in ("d", "w", "m", "y") else "d")
    if fmt in ("@s", "@ms"):
        ep = calendar.timegm(local.timetuple()) - tz_min * 60
        return str(ep * 1000 if fmt == "@ms" else ep)
    return local.strftime(fmt)


def expand_runtime(s, r, shift=0):
    """`s` with date tokens and first-page tokens expanded for row `r`."""
    if not s:
        return s
    o, _d, _j = parse_opts(r["opts"])
    tz = int(o.get("tz_minutes") or 0)
    step = (o.get("date_step") or "d")[:1]
    s = DATE_RE.sub(lambda m: render_date(m.group(1), tz, shift, step=step) or m.group(0), s)
    size = int(o.get("page_size") or 0)
    page = int(o.get("page_start") or 0) or (0 if o.get("page_zero_based") == "1" else 1)
    for tok, val in (("{page}", page), ("{offset}", 0), ("{offset1}", 1),
                     ("{offset_end}", max(size, 1) - 1), ("{size}", size)):
        s = s.replace(tok, str(val))
    return s


def _path_step(node, seg, out):
    """hp_seg_step: key=value selector, positional index, map across."""
    if isinstance(node, list):
        if "=" in seg:
            k, v = seg.split("=", 1)
            for it in node:
                if isinstance(it, dict) and it.get(k) == v:
                    out.append(it)
                    break
        elif seg.isdigit():
            i = int(seg)
            if i < len(node):
                out.append(node[i])
        else:
            for it in node:
                _path_step(it, seg, out)
    elif isinstance(node, dict) and seg in node:
        out.append(node[seg])


def resolve_array_path(doc, path):
    """lib/hpengine.c hp_run_json: hp_path first (keys, a numeric index into a
    list, `key=value`), then hp_path_multi (map across every list on the way,
    flattened one level) when that fails. The probe used to take dict keys
    only, so `0.content` under a `null([...])` JSONP body never resolved."""
    node = doc
    for seg in [s for s in path.split(".") if s]:
        crosses = isinstance(node, list) and not seg.isdigit() and "=" not in seg
        nxt = []
        if not crosses:                       # hp_path cannot map across a list
            _path_step(node, seg, nxt)
        if not nxt:
            node = None
            break
        node = nxt[0]                      # scratch fix: a JSON null at the path is no array
    if node is not None:
        return node
    cur = [doc]
    for seg in [s for s in path.split(".") if s]:
        nxt = []
        for n in cur:
            _path_step(n, seg, nxt)
        cur = nxt
    flat = []
    for it in cur:
        if it is None:                     # scratch fix: hp_path_multi never adds a JSON null
            continue
        flat.extend(it) if isinstance(it, list) else flat.append(it)
    return flat or None


def jsonp_inner(text):
    """The JSON inside a JSONP / XSSI-guarded body, else None (lib/hpengine.c
    hp_jsonp_inner, same rules: valid JSON is never unwrapped)."""
    p = text.lstrip("\ufeff").lstrip()
    if p.startswith(")]}'"):
        nl = p.find("\n")
        return p[nl + 1:] if nl >= 0 else None
    if p.startswith("/**/"):
        p = p[4:].lstrip()
    m = re.match(r"[A-Za-z_$][\w$.]*", p)
    if m:
        p = p[m.end():]
    p = p.lstrip()
    if not p.startswith("("):
        return None
    inner = p[1:].rstrip()
    if inner.endswith(";"):
        inner = inner[:-1].rstrip()
    if not inner.endswith(")"):
        return None
    inner = inner[:-1].strip()
    while inner[:1] == "(" and inner[-1:] == ")":   # cb(({...})): NMC typhoons
        inner = inner[1:-1].strip()
    return inner if inner[:1] in ("{", "[") else None


def row_body(r, entity=None, shift=0):
    """(body, headers) for a row that declares `post_body`, else (None, {}).

    The engine POSTs `post_body` with its entity tokens expanded and the row's
    `content_type` (default `Content-Type: application/json`, lib/hpengine.c).
    The probe used to GET every row, so a POST-only API could never be proven
    -- batch 40 rejected the Palace Museum collection (1.86 M objects) and the
    CNEMC station feed for exactly that. A pivot row's tokens take `entity`
    (the --check-filter impossible value) or PROBE_ENTITY."""
    o, _dups, _junk = parse_opts(r["opts"])
    body = o.get("post_body")
    if not body:
        return None, {}
    body = TOKEN_RE.sub(lambda m: entity if entity is not None else PROBE_ENTITY, body)
    body = expand_runtime(body, r, shift)
    ct = o.get("content_type") or "Content-Type: application/json"
    name, _, val = ct.partition(":")
    return body, {name.strip(): val.strip()}


def fetch(url, extra, timeout=None, body=None):
    """Same shape as verify_feeds.fetch, plus this row's declared headers and
    timeout, and its POST body when it declares one."""
    headers = {
        "User-Agent": DEFAULT_UA,
        "Accept": "*/*",
        "Accept-Encoding": "gzip",
    }
    headers.update(extra)
    data = body.encode("utf-8") if body is not None else None
    req = urllib.request.Request(VF.encode_url(url), headers=headers, data=data,
                                 method="POST" if data is not None else "GET")
    with urllib.request.urlopen(req, timeout=timeout or VF.TIMEOUT) as resp:
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
    "qc": "9999999999",
    "qh": "zzqx9nonexistent7q.invalid",
    "qu": "zzqx9nonexistentuser7q",
    "ql": "zzqx9nonexistent7q",
    "qU": "ZZQX9NONEXISTENT7Q",
    "qn": "Zzqx9NonexistentEntity7Q",
    "Q":  "zzqx9nonexistent7q",
}
# Every entity token hp_expand() (lib/hpengine.c) recognises: {q} {qd} {qc}
# {qh} {qu} {ql} {qU} {qn} plus the raw POST form {Q} -- the same list
# hp_uses_entity()'s comment gives. This used to be `q[dhlU]?|Q`, missing
# {qc}, {qu} and {qn}: a row whose ONLY entity token was one of those (e.g.
# WIKIPEDIA_PAGEVIEWS's {qn}) made impossible_probe_url() return None, so
# --check-filter silently skipped the honoured-filter check for it entirely
# -- a PASS that never actually asked the question, which is exactly the
# failure class rule 4d exists to catch.
TOKEN_RE = re.compile(r"\{(q[cdhlnuU]?|Q)\}")
# What a pivot row's POST body is probed with. The manifest's `probe` field
# carries a URL with the author's entity already inlined, but a body template
# has no such field, so it gets one fixed, common term.
PROBE_ENTITY = "tokyo"

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


def _host_is_jp(url):
    """lib/feedlib.c's feed_url_host_is_jp, including its userinfo guard:
    "user.jp@evil.com" is evil.com, not a .jp host."""
    h = url.split("://", 1)[-1]
    h = h.split("/", 1)[0].split("?", 1)[0].split("#", 1)[0]
    if "@" in h:
        h = h.rsplit("@", 1)[1]
    return h.split(":", 1)[0].rstrip(".").lower().endswith(".jp")


# The spellings lib/hpengine.c accepts for `charset`, mapped to Python codecs.
_CHARSETS = {"sjis": "cp932", "shift_jis": "cp932", "shift-jis": "cp932",
             "cp932": "cp932", "ms932": "cp932", "windows-31j": "cp932",
             "euc-jp": "euc_jp", "euc_jp": "euc_jp", "eucjp": "euc_jp"}


def decode_body(raw, r=None, url=""):
    """Decode a body the way the ENGINE will, so the gate judges what hp_run
    actually parses. Three rules, each copied from the C rather than invented:

      * a row that DECLARES `charset` is transcoded with it — the encoding is a
        property of the endpoint, not of its TLD (lib/hpengine.h);
      * otherwise lib/feedlib.c's gate: a .jp host whose body is not valid
        UTF-8 is read as Shift_JIS, and that decode FAILS CLOSED, so a .jp host
        serving something else is returned verbatim rather than turned to kanji;
      * a leading UTF-8 BOM is stripped, as lib/csv.c does, because a BOM
        welded to the first header name makes that column match nothing.

    Until this existed the probe read every legacy-encoded Japanese body as
    U+FFFD soup: `title_keys=市区町丁` could not match a cp932 header, so the
    verdict said "key missing from header" when the truth was "the prober
    cannot read this file". That is the checker-blind-to-its-input failure —
    absence reported as a result — and it silently applied to the whole
    Shift_JIS half of the JP registry.
    """
    enc = None
    if r is not None:
        declared = (opt(r, "charset") or "").strip().lower()
        enc = _CHARSETS.get(declared, declared or None)
    text = None
    if enc:
        try:
            text = raw.decode(enc, "replace")
        except LookupError:
            text = None
    if text is None and url and _host_is_jp(url):
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            try:
                text = raw.decode("cp932")
            except UnicodeDecodeError:
                text = None
    if text is None:
        text = raw.decode("utf-8", "replace")
    return text[1:] if text.startswith("﻿") else text


def filter_is_honoured(r, real_items):
    """(ok, note). ok=False means the endpoint returned substantially the same
    result set for an impossible entity as for the real one."""
    url = impossible_probe_url(r)
    if not url or real_items < 2:
        return True, ""          # nothing to compare against
    url = expand_runtime(url, r)
    body, bh = row_body(r, IMPOSSIBLE["q"])
    try:
        status, ctype, raw = fetch(url, dict(row_headers(r), **{k: v for k, v in bh.items() if k not in row_headers(r)}),
                                   row_timeout(r), body)
    except Exception:
        return True, ""          # a refusal here is not evidence either way
    if status < 200 or status >= 300:
        return True, ""
    text = decode_body(raw, r, url)
    if r["mode"] == "json":
        text = jsonp_inner(text) or text     # hp_jsonp_inner, before any JSON read
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


def _xlsx_rows(raw, sheet_name, sheet_index):
    """The chosen worksheet as a list of rows of cell strings, following the
    same reading lib/xlsx.c does: sharedStrings for t="s" cells, inline
    strings, phonetic <rPh> runs skipped, sheet chosen by exact tab name else
    0-based tab order (workbook.xml order resolved through the rels part).
    Returns (tab_name, rows) or raises ValueError."""
    import zipfile
    import io
    z = zipfile.ZipFile(io.BytesIO(raw))
    names = set(z.namelist())
    wb = z.read("xl/workbook.xml").decode("utf-8", "replace")
    sheets = re.findall(r'<(?:\w+:)?sheet\b[^>]*?\bname="([^"]*)"[^>]*?\br:id="([^"]*)"', wb)
    if not sheets:
        sheets = [(n, "") for n in re.findall(r'<(?:\w+:)?sheet\b[^>]*?\bname="([^"]*)"', wb)]
    rels = {}
    if "xl/_rels/workbook.xml.rels" in names:
        rx = z.read("xl/_rels/workbook.xml.rels").decode("utf-8", "replace")
        for rid, tgt in re.findall(r'<Relationship\b[^>]*?\bId="([^"]*)"[^>]*?\bTarget="([^"]*)"', rx):
            rels[rid] = tgt
        for tgt, rid in re.findall(r'<Relationship\b[^>]*?\bTarget="([^"]*)"[^>]*?\bId="([^"]*)"', rx):
            rels.setdefault(rid, tgt)
    if not sheets:
        raise ValueError("no worksheet in workbook.xml")
    pick = None
    if sheet_name:
        for i, (nm, _) in enumerate(sheets):
            if nm == sheet_name:
                pick = i
        if pick is None:
            raise ValueError("no sheet named %r (have %s)" % (sheet_name, [s[0] for s in sheets]))
    else:
        pick = sheet_index or 0
        if pick < 0 or pick >= len(sheets):
            raise ValueError("sheet index %d out of range (%d sheets)" % (pick, len(sheets)))
    tab, rid = sheets[pick]
    part = rels.get(rid, "")
    if part:
        part = part.lstrip("/")
        if not part.startswith("xl/"):
            part = "xl/" + part
    if not part or part not in names:
        part = "xl/worksheets/sheet%d.xml" % (pick + 1)
    if part not in names:
        raise ValueError("worksheet part %s missing" % part)
    ss = []
    if "xl/sharedStrings.xml" in names:
        sx = z.read("xl/sharedStrings.xml").decode("utf-8", "replace")
        # Element names may carry a namespace prefix (`<x:si>`, `<x:row>`): HKEX
        # writes its ListOfSecurities.xlsx that way and lib/xlsx.c accepts it.
        for si in re.findall(r"<(?:\w+:)?si\b[^>]*>(.*?)</(?:\w+:)?si>", sx, re.S):
            si = re.sub(r"<(?:\w+:)?rPh\b.*?</(?:\w+:)?rPh>", "", si, flags=re.S)
            ss.append(_xml_unescape("".join(re.findall(r"<(?:\w+:)?t\b[^>]*>(.*?)</(?:\w+:)?t>", si, re.S))))
    sx = z.read(part).decode("utf-8", "replace")
    # Placement follows lib/xlsx.c, not document order. The old reading took
    # `<c\b([^>]*)>(.*?)</c>`, so a self-closing empty cell `<c r="E1" s="3"/>`
    # swallowed the NEXT cell's value and every later column shifted one slot
    # left; it also ignored the `r` refs, so a sheet starting at row 2 (the
    # FIT-portal registers) lost the blank line the engine writes for row 1,
    # and the two disagreed about which physical line csv_skip_lines lands on.
    # Batch 40's MHLW drug-price lists hid their 品名 header that way.
    rows = []
    cur_row = 0
    for rattrs, rbody in re.findall(r"<(?:\w+:)?row\b([^>]*?)(?:/>|>(.*?)</(?:\w+:)?row>)", sx, re.S):
        m = re.search(r'\br="(\d+)"', rattrs)
        rnum = int(m.group(1)) if m else 0
        if rnum <= cur_row:
            rnum = cur_row + 1
        while cur_row + 1 < rnum:              # a missing row is a blank line
            rows.append([])
            cur_row += 1
        cur_row = rnum
        cells = []
        for attrs, inner in re.findall(r"<(?:\w+:)?c\b([^>]*?)(?:/>|>(.*?)</(?:\w+:)?c>)", rbody, re.S):
            ref = re.search(r'\br="([A-Za-z]+)\d*"', attrs)
            col = 0
            if ref:
                for ch in ref.group(1).upper():
                    col = col * 26 + (ord(ch) - 64)
            if col <= len(cells):              # absent or out of order
                col = len(cells) + 1
            cells.extend([""] * (col - 1 - len(cells)))
            v = re.search(r"<(?:\w+:)?v>(.*?)</(?:\w+:)?v>", inner, re.S)
            t = re.search(r'\bt="(\w+)"', attrs)
            if v is None:
                inner = re.sub(r"<(?:\w+:)?rPh\b.*?</(?:\w+:)?rPh>", "", inner, flags=re.S)
                cells.append(_xml_unescape("".join(re.findall(r"<(?:\w+:)?t\b[^>]*>(.*?)</(?:\w+:)?t>", inner, re.S))))
            elif t and t.group(1) == "s":
                try:
                    cells.append(ss[int(v.group(1))])
                except (ValueError, IndexError):
                    cells.append("")
            else:
                cells.append(_xml_unescape(v.group(1)))
        rows.append(cells)
    return tab, rows


def _xml_unescape(s):
    return (s.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"')
             .replace("&apos;", "'").replace("&amp;", "&"))


def verify_xlsx(r, sid, url, raw, status, nbytes):
    """An .xlsx row is judged the way the engine reads it: the chosen sheet is
    written out as CSV (cells with , \" or a line break quoted, so a multi-line
    banner cell occupies several PHYSICAL lines), `csv_skip_lines` physical
    lines are dropped, the next row is the header, and the rest are records.
    Before this branch every xlsx row was UNPARSEABLE — a zip is not text — so
    the whole modality was shipping on the author's word. The verdict also
    checks that every name in title_keys / id_keys is a header cell, because a
    wrong skip count reads a banner line as the header and the engine then
    emits records titled after whatever came first."""
    if raw[:8] == b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1":
        return (sid, url, "UNPARSEABLE", "", 0, status, nbytes,
                "OLE2 compound file (.xls), not an .xlsx")
    if raw[:2] != b"PK":
        return (sid, url, "UNPARSEABLE", "", 0, status, nbytes,
                "not a zip: " + raw[:60].decode("utf-8", "replace").replace("\n", " "))
    sheet = opt(r, "xlsx_sheet", "") or ""
    try:
        sidx = int(opt(r, "xlsx_sheet_index", "0") or 0)
    except ValueError:
        sidx = 0
    try:
        tab, rows = _xlsx_rows(raw, sheet, sidx)
    except Exception as e:
        return (sid, url, "UNPARSEABLE", "", 0, status, nbytes,
                ("xlsx: %s" % e)[:80])
    try:
        skip = int(opt(r, "csv_skip_lines", "0") or 0)
    except ValueError:
        skip = 0
    # The engine's CSV rendering, so the skip count means the same thing here.
    out = []
    for cells in rows:
        line = []
        for c in cells:
            if any(ch in c for ch in ',"\r\n'):
                c = '"' + c.replace('"', '""') + '"'
            line.append(c)
        out.append(",".join(line))
    text = "\n".join(out) + "\n"
    for _ in range(skip):
        nl = text.find("\n")
        text = text[nl + 1:] if nl >= 0 else ""
    import io
    rd = list(csv.reader(io.StringIO(text)))
    rd = [row for row in rd]
    if not rd:
        return (sid, url, "EMPTY_RESULTSET", "xlsx:" + tab, 0, status, nbytes,
                "no header row after csv_skip_lines=%d" % skip)
    header = [h.strip() for h in rd[0]]
    data = [row for row in rd[1:] if any(c.strip() for c in row)]
    wanted = []
    for k in ("title_keys", "id_keys", "date_keys"):
        v = opt(r, k, "") or ""
        for name in re.split(r"[,+]", v):
            if name.strip():
                wanted.append(name.strip())
    stamped = opt(r, "date_field", "") or ""       # hp_stamp_date adds it
    missing = [w for w in wanted if w not in header and w != stamped]
    if missing:
        return (sid, url, "KEY_MISSING", "xlsx:" + tab, len(data), status, nbytes,
                ("keys %s not in header %s" % (missing, header[:8]))[:120])
    if not data:
        return (sid, url, "EMPTY_RESULTSET", "xlsx:" + tab, 0, status, nbytes,
                "header %s but no data rows" % header[:6])
    return (sid, url, "PASS", "xlsx:" + tab, len(data), status, nbytes,
            "header=" + "|".join(header[:8])[:100])


def verify(r):
    """Verdict for one manifest row, stepping a dated row's date back the way
    hp_run's date_back does: a 4xx, or a first page with no records."""
    o, _dups, _junk = parse_opts(r["opts"])
    back = int(o.get("date_back") or 0)
    dated = bool(DATE_RE.search(r["url"] + " " + (o.get("post_body") or "")))
    shift = 0
    while True:
        res = _verify_once(r, shift)
        retry = res[2] in ("EMPTY_RESULTSET", "PATH_UNRESOLVED", "PATH_NOT_ARRAY",
                           "UNPARSEABLE", "EMPTY") or \
            (res[2] == "HTTP_ERR" and 400 <= (res[5] or 0) < 500)
        if dated and retry and shift < back and not o.get("index_url"):
            shift += 1
            continue
        if shift and res[2] == "PASS":
            res = res[:7] + (("date_back %d: " % shift) + (res[7] or ""),)
        return res


def _verify_once(r, shift=0):
    """One attempt. Mirrors verify_feeds.verify exactly."""
    sid, url = r["id"], expand_runtime(r["probe"], r, shift)
    o, _dups, _junk = parse_opts(r["opts"])
    if o.get("index_url") and o.get("index_href_must"):
        # hp_run resolves the data URL from a landing page before anything
        # else (lib/hpengine.c, index_url): GET it, take the FIRST anchor whose
        # href contains index_href_must, resolve it against the page. The
        # probe follows the same link, so an index row is judged on the file
        # the engine will actually read, and a pattern that matches nothing
        # fails here the way it fails there.
        try:
            _st, _ct, page = fetch(o["index_url"], row_headers(r), row_timeout(r))
        except Exception as e:
            return (sid, o["index_url"], "INDEX_ERR", "", 0, 0, 0, ("%s: %s" % (type(e).__name__, e))[:60])
        hrefs = re.findall(r"""<a\b[^>]*?\bhref\s*=\s*["']([^"']+)["']""",
                           page.decode("utf-8", "replace"), re.I)
        hit = next((h for h in hrefs if o["index_href_must"] in h), None)
        if not hit:
            return (sid, o["index_url"], "INDEX_NO_MATCH", "", 0, _st, len(page),
                    "no href containing %r" % o["index_href_must"])
        url = urllib.parse.urljoin(o["index_url"], hit)
    body, bh = row_body(r, shift=shift)
    hdrs = row_headers(r)
    for k, v in bh.items():
        hdrs.setdefault(k, v)            # a declared header wins, as in hp_run
    try:
        status, ctype, raw = fetch(url, hdrs, row_timeout(r), body)
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
    if r["mode"] == "xlsx":
        return verify_xlsx(r, sid, url, raw, status, nbytes)
    if raw[:4] == b"PK\x03\x04":
        # hp_run inflates a ZIP body's FIRST entry for every non-xlsx mode
        # (lib/zipread.c); the probe read the archive bytes as text and called
        # every zip-distributed CSV unparseable.
        import zipfile, io
        try:
            z = zipfile.ZipFile(io.BytesIO(raw))
            first = min(z.infolist(), key=lambda i: i.header_offset)
            raw = z.read(first)
            nbytes = len(raw)
        except Exception as e:
            return (sid, url, "UNPARSEABLE", "zip", 0, status, nbytes,
                    ("zip: %s" % e)[:80])
    text = decode_body(raw, r, url)
    if r["mode"] == "json":
        text = jsonp_inner(text) or text     # hp_jsonp_inner, before any JSON read

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
        node = resolve_array_path(doc, ap_decl)
        if isinstance(node, dict):            # hp_run_json: one record
            node = [node]
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
        # manifest.opt(), not a naive r["opts"].split(";") -- that ignores the
        # `\;` escape (the exact defect manifest.py exists to remove; see its
        # module docstring) and a header value containing an escaped semicolon
        # would shift or truncate the parsed href_must, probing a filter the
        # generated collector does not actually apply.
        must = opt(r, "href_must", "")
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
