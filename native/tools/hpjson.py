#!/usr/bin/env python3
"""lib/hpengine.c's JSON record path, in Python — for the tools that ask "what
would the ENGINE make of this body" without running the engine.

Why one module. Three tools answered that question with three private models,
and each disagreed with the engine in a direction that produced findings:

  * tools/audit_idkeys.py required EVERY `+` part of a composite key to be
    present, where hp_pick_s keys a record when AT LEAST ONE part is (a
    dimension may legitimately be null — an "all ages" row); it kept null and
    "" fields in its flatten, where hp_flatten_c adds only non-empty strings,
    numbers and bools — so a key that is ALWAYS null read OK while the engine
    fell back to another key; and its array finder walked dict keys only, then
    fell back to the densest array where the engine, for a declared path that
    does not resolve, emits NOTHING.
  * tools/diagnose_emit_keys.py still modelled an engine that rejected numeric
    identifiers (its NUMERIC_ID verdict; hp_scalar_str has accepted numbers
    since batch 18), never applied hp_first_scalar's last resort, flattened to
    depth 3 and 4 list elements (the engine: depth 8, 256 elements), and let a
    LATER field win a shared last segment where hp_flat_get takes the FIRST in
    document order.
  * tools/probe_hp_batch.py's resolve_array_path had neither `$last` nor the
    literal-dot fallback hp_path gained for OData's `@odata.nextLink`.

Every function below names the C function it mirrors. The fallback key lists
are PARSED out of lib/hpengine.c, so they cannot drift; --self-test fails if
the parse finds nothing, and pins each behaviour above against a fixture that
the old models get wrong.

    python3 tools/hpjson.py --self-test
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
ENGINE = os.path.join(NATIVE, "lib", "hpengine.c")

MISSING = object()          # "no node", distinct from a JSON null (None)


def _engine_src():
    try:
        with open(ENGINE, encoding="utf-8") as fh:
            return fh.read()
    except OSError:
        return ""


def _c_list(name, src=None):
    src = src if src is not None else _engine_src()
    m = re.search(r"static const char \*%s\[\]\s*=\s*\{(.*?)\};" % name, src, re.S)
    return re.findall(r'"([^"]*)"', m.group(1)) if m else []


def _c_define(name, default, src=None):
    src = src if src is not None else _engine_src()
    m = re.search(r"#define\s+%s\s+(\d+)" % name, src)
    return int(m.group(1)) if m else default


_SRC = _engine_src()
TITLE_FALLBACK = _c_list("TITLE_FALLBACK", _SRC)
ID_FALLBACK = _c_list("ID_FALLBACK", _SRC)
LINK_FALLBACK = _c_list("LINK_FALLBACK", _SRC)
DATE_FALLBACK = _c_list("DATE_FALLBACK", _SRC)
MAX_PROPS = _c_define("HP_MAX_PROPS", 2048, _SRC)
MAX_ARR = _c_define("HP_MAX_ARR", 256, _SRC)
MAX_DEPTH = _c_define("HP_MAX_DEPTH", 8, _SRC)
del _SRC


# ── cJSON lookups ────────────────────────────────────────────────────────
def get_ci(node, key):
    """cJSON_GetObjectItem: case-INsensitive, first match. MISSING if absent."""
    if not isinstance(node, dict):
        return MISSING
    if key in node:
        return node[key]
    kl = key.lower()
    for k, v in node.items():
        if isinstance(k, str) and k.lower() == kl:
            return v
    return MISSING


def _segments(path):
    return [s for s in (path or "").split(".") if s]


def hp_path(root, path):
    """hp_path: dotted keys, a numeric index into a list, `key=value` (first
    list element whose string `key` equals `value`), `$last`; then, only when
    the split walk failed and there was a dot, the WHOLE path as one key
    (OData's `@odata.nextLink`). -> node, or MISSING."""
    if not path:
        return root
    cur = root
    for seg in _segments(path):
        if cur is MISSING:
            break
        if "=" in seg and isinstance(cur, list):
            k, want = seg.split("=", 1)
            hit = MISSING
            for it in cur:
                f = get_ci(it, k)
                if isinstance(f, str) and f == want:
                    hit = it
                    break
            cur = hit
            continue
        if seg == "$last" and isinstance(cur, list):
            cur = cur[-1] if cur else MISSING
            continue
        if seg.isdigit() and isinstance(cur, list):
            i = int(seg)
            cur = cur[i] if i < len(cur) else MISSING
        else:
            cur = get_ci(cur, seg)
    if cur is MISSING and "." in path:
        cur = get_ci(root, path)
    return cur


def _seg_step(node, seg, out):
    """hp_seg_step: as hp_path's step, but MAP a name across every element
    of a list instead of failing on it."""
    if "=" in seg and isinstance(node, list):
        k, want = seg.split("=", 1)
        for it in node:
            f = get_ci(it, k)
            if isinstance(f, str) and f == want:
                out.append(it)
                break
        return
    if isinstance(node, list):
        if seg.isdigit():
            i = int(seg)
            if i < len(node):
                out.append(node[i])
        else:
            for it in node:
                _seg_step(it, seg, out)
        return
    hit = get_ci(node, seg)
    if hit is not MISSING:
        out.append(hit)


def hp_path_multi(root, path):
    """hp_path_multi: every node the path reaches, a list node SPLICED."""
    cur = [root]
    for seg in _segments(path):
        nxt = []
        for n in cur:
            _seg_step(n, seg, nxt)
        cur = nxt
    flat = []
    for it in cur:
        if isinstance(it, list):
            flat.extend(it)
        else:
            flat.append(it)
    return flat


def find_densest(doc):
    """hp_find_array: the array with the most ELEMENTS among arrays holding
    at least one object, depth <= 5, first in document order on a tie.
    -> (array or None, path, number of candidate arrays)."""
    best = [None, -1, "", 0]

    def walk(node, depth, path):
        if depth > 5:
            return
        if isinstance(node, list):
            if any(isinstance(x, dict) for x in node):
                best[3] += 1
                if len(node) > best[1]:
                    best[0], best[1], best[2] = node, len(node), path
            for i, ch in enumerate(node):
                if isinstance(ch, (dict, list)):
                    walk(ch, depth + 1, "%s[%d]" % (path, i))
        elif isinstance(node, dict):
            for k, ch in node.items():
                if isinstance(ch, (dict, list)):
                    walk(ch, depth + 1, "%s.%s" % (path, k) if path else str(k))
    walk(doc, 0, "")
    return best[0], best[2], best[3]


def select_records(doc, array_path=None):
    """The records hp_run_json iterates for a parsed body, and HOW:
    -> (records list, how). `how` is one of

        declared        the declared path resolved to an array (or a `+` union)
        declared-multi  … only by mapping across a list (hp_path_multi)
        declared-object the declared path holds ONE object: one record
        declared-missing the declared path did not resolve: the engine EMITS
                        NOTHING (records = []) — no densest-array guess
        densest         no path declared: hp_find_array's pick
        root-record     no path, no array of objects: the document is the record
    """
    if array_path and "+" in array_path:
        recs = []
        for alt in [a.strip() for a in array_path.split("+")]:
            if not alt:
                continue
            n = hp_path(doc, alt)
            if isinstance(n, list):
                recs.extend(n)
            elif isinstance(n, dict):
                recs.append(n)
            elif n is MISSING:
                recs.extend(hp_path_multi(doc, alt))
        return (recs, "declared") if recs else ([], "declared-missing")
    if array_path:
        n = hp_path(doc, array_path)
        if n is MISSING:
            multi = hp_path_multi(doc, array_path)
            if multi:
                return multi, "declared-multi"
        if isinstance(n, list):
            return n, "declared"
        if isinstance(n, dict):
            return [n], "declared-object"
        return [], "declared-missing"
    arr, _path, _cands = find_densest(doc)
    if arr is not None:
        return arr, "densest"
    return [doc], "root-record"


def json_error_doc(doc):
    """hp_json_error_doc: an object whose `error`/`errors`/`fault` is a
    non-empty object, a non-empty array or a non-empty string. Asked only
    once no records array was found (an envelope carrying records AND an
    error member keeps its records)."""
    if not isinstance(doc, dict):
        return False
    for k in ("error", "errors", "fault"):
        e = get_ci(doc, k)
        if (isinstance(e, (dict, list)) and e) or (isinstance(e, str) and e):
            return True
    return False


# ── flatten and pick ─────────────────────────────────────────────────────
def hp_flatten(node, prefix="", out=None, depth=0):
    """hp_flatten_c: every NON-EMPTY string, number and bool under `node`, in
    document order, as [(dotted key, value)]; nested objects dotted, list
    elements indexed (at most MAX_ARR of them), depth and field bounds as in
    the C. A null, an empty string, an empty object/list add NOTHING."""
    if out is None:
        out = []
    if depth > MAX_DEPTH or len(out) >= MAX_PROPS:
        return out
    if isinstance(node, list):
        items = []
        for i, v in enumerate(node):
            if i >= MAX_ARR:
                break
            items.append(("%s.%d" % (prefix, i), v))
    elif isinstance(node, dict):
        items = [("%s.%s" % (prefix, k) if prefix else str(k), v) for k, v in node.items()]
    else:
        return out
    for key, v in items:
        if len(out) >= MAX_PROPS:
            return out
        if isinstance(v, bool):
            out.append((key, v))
        elif isinstance(v, str):
            if v:
                out.append((key, v))
        elif isinstance(v, (int, float)):
            out.append((key, v))
        elif isinstance(v, (dict, list)):
            hp_flatten(v, key, out, depth + 1)
    return out


def _split_keys(spec):
    return [t.strip() for t in re.split(r"[,+]", spec or "") if t.strip()]


def seed_declared_keys(rec, flat, key_lists):
    """hp_seed_declared_keys: for each token of the row's declared key lists
    not already a flat key (case-sensitive), resolve it straight from the
    record with hp_path and add it when it is a non-empty scalar."""
    if not isinstance(rec, dict):
        return flat
    have = {k for k, _v in flat}
    for spec in key_lists:
        for tok in _split_keys(spec):
            if tok in have:
                continue
            v = hp_path(rec, tok)
            if isinstance(v, bool) or (isinstance(v, str) and v) or \
                    (isinstance(v, (int, float)) and not isinstance(v, bool)):
                flat.append((tok, v))
                have.add(tok)
    return flat


def json_flat(rec, key_lists=()):
    """hp_json_flat: an object or list is flattened (then the declared keys
    seeded); a non-empty string is {"value": string}; anything else is an
    empty record."""
    if isinstance(rec, (dict, list)):
        return seed_declared_keys(rec, hp_flatten(rec), key_lists)
    if isinstance(rec, str) and rec:
        return [("value", rec)]
    return []


def flat_get(flat, name):
    """hp_flat_get: the exact key (case-insensitively, as cJSON_GetObjectItem
    matches), else the FIRST key in document order whose last dotted segment
    matches case-insensitively, a leading `@` on it skipped unless the name
    starts with `@`. -> value or MISSING."""
    if not name:
        return MISSING
    nl = name.lower()
    for k, v in flat:
        if k.lower() == nl:
            return v
    for k, v in flat:
        last = k.rsplit(".", 1)[-1]
        if last.startswith("@") and not name.startswith("@"):
            last = last[1:]
        if last.lower() == nl:
            return v
    return MISSING


def scalar_str(v):
    """hp_scalar_str: a non-empty string, a number as text (an integral value
    without a fraction), a bool as true/false; else None."""
    if v is MISSING or v is None:
        return None
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, str):
        return v or None
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        if v == int(v):
            return "%d" % int(v)
        return "%.10g" % v
    return None


def pick(flat, keys, fallback=None):
    """hp_pick_s: the first `,` alternative that resolves; a `+` composite
    resolves when AT LEAST ONE part is present (a missing part joins as "").
    -> (value, the alternative that matched) or (None, None)."""
    for tok in (keys or "").split(","):
        tok = tok.lstrip(" ")
        if not tok:
            continue
        if "+" in tok:
            vals = [scalar_str(flat_get(flat, p.lstrip(" "))) for p in tok.split("+") if p.lstrip(" ")]
            if any(v is not None for v in vals):
                return "|".join(v or "" for v in vals), tok
            continue
        v = scalar_str(flat_get(flat, tok))
        if v is not None:
            return v, tok
    for f in fallback or ():
        v = scalar_str(flat_get(flat, f))
        if v is not None:
            return v, f
    return None, None


def first_scalar(flat):
    """hp_first_scalar: the first non-empty scalar in document order, the
    engine's own `_`-prefixed annotations skipped. -> (value, key)."""
    for k, v in flat:
        if k.startswith("_"):
            continue
        s = scalar_str(v)
        if s is not None:
            return s, k
    return None, None


def record_key(flat, id_keys=None, title_keys=None):
    """What hp_emit_record keys a record on -> (basis, value, key).

    basis: `id_keys` (the declared key matched), `id_fallback` (ID_FALLBACK),
    `title` (no id at all: the title is the key), `first_scalar` (neither a
    title nor an id), or `empty` (the record is dropped as shape noise)."""
    title, _tk = pick(flat, title_keys)
    rkey, rk = pick(flat, id_keys)
    basis = "id_keys" if rkey is not None else ""
    if title is None:
        title, _tk = pick(flat, None, TITLE_FALLBACK)
    if rkey is None:
        rkey, rk = pick(flat, None, ID_FALLBACK)
        if rkey is not None:
            basis = "id_fallback"
    if title is None and rkey is None:
        rkey, rk = first_scalar(flat)
        if rkey is None:
            return "empty", None, None
        return "first_scalar", rkey, rk
    if rkey is None:
        return "title", title, None
    return basis, rkey, rk


def parse_body(text):
    """A body as hp_run_json parses it: JSONP / XSSI unwrapped first
    (hp_jsonp_inner), BOM tolerated. -> document or MISSING."""
    t = text.lstrip("﻿")
    try:
        return json.loads(t)
    except ValueError:
        pass
    inner = jsonp_inner(t)
    if inner is not None:
        try:
            return json.loads(inner)
        except ValueError:
            return MISSING
    return MISSING


def jsonp_inner(text):
    """hp_jsonp_inner (same rules: valid JSON is never unwrapped)."""
    p = text.lstrip("﻿").lstrip()
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
    while inner[:1] == "(" and inner[-1:] == ")":
        inner = inner[1:-1].strip()
    return inner if inner[:1] in ("{", "[") else None


# ── fixtures ─────────────────────────────────────────────────────────────
def self_test():
    bad = []

    def check(label, got, want):
        if got != want:
            bad.append("%-58s got=%r want=%r" % (label, got, want))

    check("ID_FALLBACK parsed from the C", ID_FALLBACK[:3], ["id", "uid", "uuid"])
    check("TITLE_FALLBACK parsed from the C", TITLE_FALLBACK[:2], ["name", "title"])
    check("LINK_FALLBACK parsed", "url" in LINK_FALLBACK, True)
    check("DATE_FALLBACK parsed", bool(DATE_FALLBACK), True)
    check("flatten bounds read from the C", (MAX_DEPTH, MAX_ARR) >= (8, 256), True)

    # flatten: only non-empty scalars (audit_idkeys kept null/"").
    f = hp_flatten({"a": None, "b": "", "c": 0, "d": False, "e": {"f": "x"}, "g": []})
    check("null and \"\" are absent from the flatten", f, [("c", 0), ("d", False), ("e.f", "x")])
    check("an always-null key does not resolve", flat_get(hp_flatten({"k": None}), "k"), MISSING)
    # composite: at least one part (audit_idkeys required all).
    fl = hp_flatten({"year": 2020, "age": None})
    check("a composite with one part present keys", pick(fl, "year+age")[0], "2020|")
    check("a composite with no part present does not", pick(fl, "age+sex")[0], None)
    # numbers are identifiers (diagnose's NUMERIC_ID).
    check("a numeric id is an id", record_key(hp_flatten({"id": 3215}))[:2], ("id_fallback", "3215"))
    check("a float id prints as hp_scalar_str does", scalar_str(1.5), "1.5")
    check("an integral float has no fraction", scalar_str(42.0), "42")
    # first in document order wins a shared last segment (diagnose: later won).
    fl = hp_flatten({"a": {"code": "first"}, "b": {"code": "second"}})
    check("first last-segment match wins", flat_get(fl, "code"), "first")
    check("exact key beats a last-segment match",
          flat_get(hp_flatten({"x": {"id": "deep"}, "id": "top"}), "id"), "top")
    check("@attribute matches a bare name", flat_get([("r.@id", "7")], "id"), "7")
    # first scalar is the last resort (diagnose ignored it).
    check("first scalar keys an unlabelled record",
          record_key(hp_flatten({"_x": "meta", "OBJ": "A1", "n": 3}))[:2], ("first_scalar", "A1"))
    check("an empty record is dropped", record_key([])[0], "empty")
    # flatten depth 8 / list 256 (diagnose: depth 3, 4 elements).
    deep = {"a": {"b": {"c": {"d": {"e": "v"}}}}}
    check("depth 4 is reached", flat_get(hp_flatten(deep), "e"), "v")
    check("list element 10 is reached", flat_get(hp_flatten({"l": list(range(12))}), "l.10"), 10)
    # array selection: a declared path that does not resolve emits NOTHING
    # (audit_idkeys fell back to the densest array).
    doc = {"meta": {"x": 1}, "other": [{"a": 1}, {"a": 2}]}
    check("declared-missing emits nothing", select_records(doc, "results"), ([], "declared-missing"))
    check("no path: densest", select_records(doc)[1], "densest")
    check("no path, no array of objects: root record",
          select_records({"a": 1, "nums": [1, 2]}), ([{"a": 1, "nums": [1, 2]}], "root-record"))
    check("`.` is the root", select_records(["x", "y"], ".")[0], ["x", "y"])
    check("a path that crosses a list maps across it",
          len(select_records({"items": [{"items": [1, 2]}, {"items": [3]}]}, "items.items")[0]), 3)
    check("key=value selector", hp_path({"links": [{"rel": "self", "h": 1}, {"rel": "next", "h": 2}]},
                                        "links.rel=next.h"), 2)
    check("$last", hp_path({"r": [{"t": 1}, {"t": 9}]}, "r.$last.t"), 9)
    check("literal-dot fallback (OData)", hp_path({"@odata.nextLink": "u"}, "@odata.nextLink"), "u")
    check("an index into a list", hp_path({"r": [{"t": 1}, {"t": 9}]}, "r.1.t"), 9)
    check("case-insensitive key", hp_path({"Results": [1]}, "results"), [1])
    check("a `+` union", len(select_records({"a": [{"x": 1}], "b": {"y": 2}}, "a+b+c")[0]), 2)
    check("a declared object is one record", select_records({"r": {"x": 1}}, "r")[1], "declared-object")
    check("a bare string is {value}", json_flat("abc"), [("value", "abc")])
    check("a declared dotted key is seeded past the flatten",
          json_flat({"a": {"b": "v"}}, ["a.b"])[-1], ("a.b", "v"))
    check("JSONP unwrapped", parse_body('cb({"a":1});'), {"a": 1})
    check("error doc", json_error_doc({"error": {"code": 400}}), True)
    check("an empty error member is not an error", json_error_doc({"error": ""}), False)
    for b in bad:
        print("hpjson SELF-TEST FAIL  " + b)
    return not bad


if __name__ == "__main__":
    ok = self_test()
    print("hpjson fixtures: %s" % ("all pass" if ok else "FAILED"))
    sys.exit(0 if ok else 2)
