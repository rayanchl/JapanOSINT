#!/usr/bin/env python3
"""THE parser for `docs/candidate-sources-batch<N>.<beat>.txt`.

WHY THIS FILE EXISTS. Seven tools read these manifests and, until this module,
each brought its own parser. They disagreed, and a disagreement between two
parsers of the same line is not a style problem — it is a tool reporting a
verdict about a row it did not actually read.

The incident: `OSM_API_CHANGESETS_BLACKSEA` carried

    interval=86400\\;pagination_ok=...;interval=86400

The stray backslash escaped the first separator, so the opts field held the key
`interval` TWICE — once with the value `86400;pagination_ok=OSM refuses…` and
once with a clean `86400`.

  * `gen_hp_batch.py` resolved opts LAST-wins, so it generated `.interval =
    86400`. Correct, purely by accident of ordering. `pagination_ok` had been
    swallowed into the bad value and was never registered at all.
  * `audit_batch_reachable.py` resolved them FIRST-wins, saw
    `interval="86400;pagination_ok=OSM refuses…"`, failed `.isdigit()`, and
    reported the row as "static URL and no interval — never runs" (house rule
    3) about a row that in fact runs daily.
  * `audit_batch_pagination.py` did not split on `;` at all; it looked for the
    SUBSTRING `page_param=` anywhere in the opts field, which finds it inside a
    post_body value, and its `pagination_ok` split ignored the `\\;` escape
    that exists precisely because a header value may contain a semicolon.

So: one split, one resolution rule, one place. A duplicate key is not resolved
here at all — `parse_opts` REPORTS it and lets the caller decide, because the
honest answer to "which value is meant" is that nobody knows.
`gen_hp_batch.py` rejects the row; the auditors mark it unverifiable rather
than guessing and then reporting the guess as a finding.

The other divergence this module removes is quieter and worse. Every auditor
opened the file with

    if s.startswith("#") or s.count("|") != 12: continue

so a row with the wrong field count was SKIPPED — silently, indistinguishably
from a comment. The row was then reported as having no findings, because it had
not been examined. `iter_lines()` classifies every line and hands back the
malformed ones so a tool can say "NOT CHECKED" instead of nothing.
"""
import io
import os

COLS = ["id", "mode", "want", "category", "record_type", "tags", "portal",
        "name", "name_ja", "url", "probe", "description", "opts"]


def split_opts(s):
    """Split an opts field on `;`, honouring a backslash escape.

    A semicolon is legal inside a header value -- `Accept: application/json;q=0.9`
    is an ordinary content-negotiation header, and several government WAFs only
    admit a User-Agent that contains one. Splitting naively made those headers
    inexpressible, so two sources in one batch had to be dropped for a reason
    that was purely a limitation of this file format. `\\;` now passes through
    as a literal semicolon.
    """
    out, cur, esc = [], [], False
    for ch in s:
        if esc:
            cur.append(ch if ch == ";" else "\\" + ch)
            esc = False
        elif ch == "\\":
            esc = True
        elif ch == ";":
            out.append("".join(cur)); cur = []
        else:
            cur.append(ch)
    if esc:
        cur.append("\\")
    out.append("".join(cur))
    return [x.strip() for x in out]


def parse_opts(s):
    """`k=v;k=v` -> (dict, [duplicated keys], [tokens with no `=`]).

    LAST-wins for the value, matching what the generator emits into C — but the
    duplicates come back too, and a caller that ignores them is choosing to
    resolve an ambiguity it cannot resolve. Nothing in this tree does that any
    more: see the module docstring for what it cost the one time it happened.
    """
    o, dups, junk = {}, [], []
    for kv in split_opts(s):
        if not kv:
            continue
        if "=" not in kv:
            junk.append(kv)
            continue
        k, _, v = kv.partition("=")
        k, v = k.strip(), v.strip()
        if k in o:
            dups.append(k)
        o[k] = v
    return o, dups, junk


def opt(row, key, default=None):
    """One opt from a manifest row, resolved the one way. Returns `default`
    when the key is absent OR duplicated — a duplicated key has no value this
    module is willing to state."""
    o, dups, _j = parse_opts(row.get("opts", ""))
    if key in dups:
        return default
    return o.get(key, default)


# ── line-level reading ───────────────────────────────────────────────────────
# Kinds: "row" (a parsed dict), "skip" (blank or comment), "bad" (a line that
# looks like a row and is not). Nothing is dropped on the floor.

def iter_lines(path):
    """-> (lno, raw, kind, row_or_None)"""
    for lno, line in enumerate(io.open(path, encoding="utf-8"), 1):
        s = line.rstrip("\n").rstrip("\r")
        if not s.strip() or s.lstrip().startswith("#"):
            yield lno, s, "skip", None
            continue
        if s.count("|") != len(COLS) - 1:
            yield lno, s, "bad", None
            continue
        yield lno, s, "row", dict(zip(COLS, s.split("|")))


def load(paths, strict=True):
    """Every row across `paths`, with `_src` = "file:line".

    strict=True (the generator's contract) raises SystemExit on a malformed
    line, a duplicate id, or a missing required field: the manifest is the
    source of truth and a broken one must not silently produce C. strict=False
    returns the rows it could parse AND the problems, for auditors that must
    report on the rest of the file rather than abort on line 3.
    """
    rows, seen, problems = [], {}, []
    for p in paths:
        for lno, raw, kind, r in iter_lines(p):
            where = "%s:%d" % (p, lno)
            if kind == "skip":
                continue
            if kind == "bad":
                msg = ("%s: %d fields, want %d\n  %s"
                       % (where, raw.count("|") + 1, len(COLS), raw[:150]))
                if strict:
                    raise SystemExit(msg)
                problems.append((where, "MALFORMED", msg))
                continue
            r = dict((k, v.strip()) for k, v in r.items())
            r["_src"] = where
            if r["id"] in seen:
                msg = ("%s: duplicate id %s (also at %s)"
                       % (where, r["id"], seen[r["id"]]))
                if strict:
                    raise SystemExit(msg)
                problems.append((where, "DUP-ID", msg))
            seen[r["id"]] = where
            missing = [k for k in ("id", "portal", "name", "url", "probe",
                                   "description", "category", "record_type")
                       if not r[k]]
            if missing:
                msg = "%s: empty required field %s" % (where, missing[0])
                if strict:
                    raise SystemExit(msg)
                problems.append((where, "EMPTY-FIELD", msg))
            rows.append(r)
    return rows if strict else (rows, problems)


def basename_src(r):
    """"file:line" with the directory trimmed, for report lines."""
    src = r.get("_src", "?")
    p, _, lno = src.rpartition(":")
    return "%s:%s" % (os.path.basename(p), lno)
