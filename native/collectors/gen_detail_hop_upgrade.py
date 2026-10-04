#!/usr/bin/env python3
"""Turn list-only collectors that hold a PROVEN detail endpoint into deep ones.

THE GAP THIS CLOSES. Batches 16 and 17 did two things: they verified ~840
endpoints live, and they separately probed a per-record detail endpoint for many
of them with a real id and watched a record come back. The verified list rows
were then generated as VJSON collectors — one GET of the list — and the proven
detail endpoints were parked in a side-car TSV. 382 collectors ended up shipping
with this sentence in their own description:

    "A per-record detail endpoint was verified for this source; see the batch's
     detail-hops side-car. This collector fetches the list endpoint only."

That is a rule-2 violation stated out loud (docs/SOURCE_EXHAUSTIVENESS.md): we
spend a request on the list, we hold a proven route to the record behind each
hit, and we drop it. The engine to walk it has existed the whole time —
hp_source.detail_url/detail_key in lib/hpengine.h.

So this script moves those rows from VJSON onto hpengine, which fetches the list
AND the record behind each list hit.

WHY IT PARSES THE .c FILES AND NOT THE MANIFEST TSV. Batch 16 lost sources to
exactly that shortcut: regenerating from candidate rows put the DISCOVERING
agent's predicted array path back over the VERIFIER's observed one, and the
collectors emitted nothing. The .c file is the artefact that was measured
emitting records, so it is the authority here for url, array path, name,
language, tags, cadence and description. The side-car TSV is consulted for one
thing only — the detail_url/detail_key pair it proved.

WHAT IS DELIBERATELY LEFT ALONE. VRSS and VGEO rows: hpengine has no RSS or
FeatureCollection mode, so a feed row with a detail hop stays list-only and is
reported at the end rather than half-converted.

Usage:
  python3 gen_detail_hop_upgrade.py \
      --generated collectors/feed/generated \
      --detail-hops ../docs/candidate-sources-batch16.detail-hops.tsv \
      --detail-hops ../docs/candidate-sources-batch17.detail-hops.tsv \
      --keep-on-vjson ../docs/detail-hops-kept-on-vjson.tsv \
      [--dry-run]

Before trusting a run, sweep the converted ids in BOTH forms with
tools/audit_registry_emit.py and compare stored counts. Rows whose hpengine form
stores less go in docs/detail-hops-kept-on-vjson.tsv and stay on VJSON.
"""
import argparse
import csv
import os
import re
import sys
from collections import defaultdict

# The list-only macros this script can move onto hpengine, and the mode each
# becomes. VRSS/VGEO have no hpengine equivalent and are skipped by omission.
CONVERTIBLE = {
    "VJSON":    "HP_JSON",
    "VJSONBIG": "HP_JSON",
    "VCSV":     "HP_CSV",
    # Both of these are a VJSON row plus a declared record identity, and that
    # identity translates exactly: _vjson_idkeys.inc says of its IDKEYS field
    # "`+` composes, as in hpengine id_keys", and VJSON_KEYED's IDFIELD is one
    # top-level field, which is the same string with no separator. So the extra
    # argument becomes .id_keys and nothing is invented.
    #
    # NOT here, and deliberately: VJSON_PREP, whose 14th argument is a C
    # function that reshapes each page before emit. A row cannot express
    # arbitrary code, so converting one would quietly drop the hook that makes
    # its records labellable. VRSS and VGEO stay out for the older reason —
    # hpengine has no RSS or FeatureCollection mode.
    "VJSON_KEYED":  "HP_JSON",
    "VJSON_IDKEYS": "HP_JSON",
}
# Argument order of each macro, so a parsed call becomes a named dict. Mirrors
# collectors/sources/_verified_macros.inc — if that file's signatures change,
# this table has to change with it.
SIGNATURES = {
    "VJSON":    ["sym", "id", "name", "name_ja", "collector", "category",
                 "url", "path", "lang", "tags", "interval", "description"],
    "VJSON_KEYED":  ["sym", "id", "name", "name_ja", "collector", "category",
                     "url", "path", "lang", "tags", "interval", "description",
                     "id_keys"],
    "VJSON_IDKEYS": ["sym", "id", "name", "name_ja", "collector", "category",
                     "url", "path", "lang", "tags", "interval", "description",
                     "id_keys"],
    "VJSONBIG": ["sym", "id", "name", "name_ja", "collector", "category",
                 "url", "path", "lang", "tags", "interval", "description"],
    "VCSV":     ["sym", "id", "name", "name_ja", "collector", "category",
                 "url", "lang", "tags", "interval", "description"],
}
# The sentence the generator appended to every list-only row that had a proven
# hop. It is now false for the converted rows, so it is replaced rather than
# left to mislead.
LIST_ONLY_NOTE = ("  A per-record detail endpoint was verified for this source; "
                  "see the batch's detail-hops side-car. This collector fetches "
                  "the list endpoint only.")

# `[A-Z0-9_]` matters. This was `^(V[A-Z]+)\(`, which cannot match an
# underscore — so VJSON_IDKEYS, VJSON_KEYED, VJSON_PREP, VCSV_STRIP_BOGUS_ID,
# VOM_HOURLY and friends (400+ live rows) were INVISIBLE to this script. That
# was not merely a missed conversion: the "this file has no registrations left,
# delete it" step below could not see them either, so a file whose VJSON rows
# were all moved was deleted with its VJSON_IDKEYS rows still inside. It cost
# 20 real sources, caught only by diffing the registered-id set against the
# base branch. Hence also the assertion at the end of main().
MACRO_CALL = re.compile(r"^(V[A-Z][A-Z0-9_]*)\(", re.M)

# Paging for a converted row is DERIVED FROM ITS OWN URL, with the same
# vocabulary and the same rule lib/pagewalk.c applies to the generated
# collectors these rows are moving off:
#
#   * a cursor parameter that is ALREADY PRESENT in the author's URL may be
#     advanced — advancing a number somebody wrote is not guessing;
#   * an OFFSET cursor additionally needs a declared page size, because that
#     size is the stride. A PAGE NUMBER does not: it advances by one.
#   * nothing is ever ADDED to a URL that did not have it. A row whose URL
#     declares only a size and no cursor gets no paging here — which is exactly
#     what pagewalk does with it today, so the move costs it nothing.
#
# This matters because the rows are moving from VJSON (walked by pagewalk) onto
# hpengine, which pages only when the row declares how. Without this, each row
# would have bought its detail hop with every page after the first.
PW_OFF_PARAMS = ("offset", "$offset", "$skip", "skip", "resultOffset",
                 "startIndex", "start")
PW_PAGE_PARAMS = ("page", "pageNumber", "p")
PW_SIZE_PARAMS = ("limit", "rows", "$limit", "resultRecordCount", "page_size",
                  "per_page", "pageSize", "$top", "size", "maxRecords",
                  "count", "retmax", "itemsPerPage", "length")
# A record offset is a small number; anything larger is a timestamp that happens
# to be numeric (`start=1754697600`). Same bound pagewalk uses.
PW_OFFSET_MAX = 10000000


def _num_param(url, name):
    """Value of `name=<int>` in the query, or None. Compares from a parameter
    boundary so `$offset` never matches a bare `offset`."""
    q = url.split("?", 1)
    if len(q) < 2:
        return None
    for part in q[1].split("&"):
        k, _, v = part.partition("=")
        if k == name:
            try:
                return int(v)
            except ValueError:
                return None
    return None


def paging_for(url):
    """-> (page_param, page_size, page_start) for a URL, or (None, 0, None)."""
    size = None
    for nm in PW_SIZE_PARAMS:
        size = _num_param(url, nm)
        if size is not None and size > 0:
            break
        size = None
    for nm in PW_PAGE_PARAMS:                 # page numbers need no stride
        v = _num_param(url, nm)
        if v is not None:
            return nm, 0, v
    for nm in PW_OFF_PARAMS:
        v = _num_param(url, nm)
        if v is None or v > PW_OFFSET_MAX:
            continue
        if size is None:
            continue                          # an offset with no stride: skip
        return nm, size, v
    return None, 0, None

# No URL rewrites are needed any more. This used to raise ESMA's `rows=1` to a
# real page size; main fixed that row upstream (it now asks for rows=500), and
# the generator reported the pattern as missing rather than silently doing
# nothing — which is how the obsolete entry was noticed. Keep this empty rather
# than deleting it: the next probe-page-size-shipped-as-a-collector goes here.
URL_FIXUP = {}


def split_c_args(text):
    """Split a macro argument list on top-level commas.

    C string literals in these files contain commas, escaped quotes and JSON
    brackets ("[\"ke\",\"statistics\"]"), so a naive split on "," corrupts the
    tags argument — which is how a lint pass once truncated 2,000 URLs at their
    first comma. Track quoting and nesting instead."""
    args, depth, quote, esc, cur = [], 0, False, False, []
    # Comment state. This tree comments INSIDE argument lists — a path argument
    # followed by /* …36 emitted, 22 stored, 2026-09-14… */ — and those commas
    # and parens are prose, not syntax. Without skipping them a 12-argument
    # VJSON call parses as 15 and the row is passed over (it failed safe, but it
    # failed: three rows with proven detail hops went unconverted).
    block, line_c = False, False
    i, n = 0, len(text)
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if line_c:
            cur.append(ch)
            if ch == "\n":
                line_c = False
            i += 1
            continue
        if block:
            cur.append(ch)
            if ch == "*" and nxt == "/":
                cur.append(nxt)
                block = False
                i += 2
                continue
            i += 1
            continue
        if esc:
            cur.append(ch)
            esc = False
            i += 1
            continue
        if ch == "\\":
            cur.append(ch)
            esc = True
            i += 1
            continue
        if ch == '"':
            quote = not quote
            cur.append(ch)
            i += 1
            continue
        if not quote and ch == "/" and nxt == "*":
            cur.append(ch); cur.append(nxt)
            block = True
            i += 2
            continue
        if not quote and ch == "/" and nxt == "/":
            cur.append(ch); cur.append(nxt)
            line_c = True
            i += 2
            continue
        if not quote:
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == "," and depth == 0:
                args.append("".join(cur).strip())
                cur = []
                i += 1
                continue
        cur.append(ch)
        i += 1
    if cur:
        args.append("".join(cur).strip())
    return args


def c_str_value(arg):
    """Value of a C string-literal argument, keeping its escapes as written.

    Adjacent literals ("a" "b") are concatenated the way the compiler does.
    Returns None for a non-string argument (an integer interval, NULL)."""
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', arg)
    if not parts:
        return None
    return "".join(parts)


def parse_macro_calls(src):
    """Yield (macro, start, end, args) for every top-level V*(...) call."""
    for m in MACRO_CALL.finditer(src):
        macro = m.group(1)
        open_paren = m.end() - 1
        depth, quote, esc, i = 0, False, False, open_paren
        block = line_c = False
        while i < len(src):
            ch = src[i]
            nxt = src[i + 1] if i + 1 < len(src) else ""
            if line_c:
                if ch == "\n":
                    line_c = False
            elif block:
                if ch == "*" and nxt == "/":
                    block = False
                    i += 2
                    continue
            elif esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                quote = not quote
            elif not quote and ch == "/" and nxt == "*":
                block = True
                i += 2
                continue
            elif not quote and ch == "/" and nxt == "/":
                line_c = True
                i += 2
                continue
            elif not quote:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                    if depth == 0:
                        break
            i += 1
        if i >= len(src):
            continue
        end = i + 1
        while end < len(src) and src[end] in ";\n":
            end += 1
            if src[end - 1] == ";":
                break
        yield macro, m.start(), end, split_c_args(src[open_paren + 1:i])


def load_detail_hops(paths):
    hops = {}
    for p in paths:
        with open(p, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f, delimiter="\t"):
                url = (row.get("detail_url") or "").strip()
                key = (row.get("detail_key") or "").strip()
                if not url or not key or "{v}" not in url:
                    continue
                hops[row["id"].strip()] = (url, key, os.path.basename(p))
    return hops


def tags_members(tags_literal):
    """VJSON's tags argument is a whole JSON array; hp_source.tags is the
    members that go INSIDE the array hpengine builds. Drop the two hpengine
    always emits so they are not duplicated, and add the marker that says this
    row now walks its second hop."""
    body = tags_literal.strip()
    if body.startswith("[") and body.endswith("]"):
        body = body[1:-1]
    members = [m for m in re.findall(r'\\"((?:[^"\\]|\\.)*?)\\"', body)]
    keep = [m for m in members if m not in ("osint-search", "high-penetrancy")]
    if "deep-record" not in keep:
        keep.append("deep-record")
    return ",".join('\\"%s\\"' % m for m in keep)


def describe(desc, detail_url):
    """Replace the list-only sentence with what the row actually does now.

    Deliberately does NOT paste detail_url into the prose. The URL is already
    the row's .detail_url field, and tools/lint_sources.py reads URLs out of
    every string in the file — a second copy in a description registers as a
    second collector fetching that endpoint, which is how this generator's first
    run reported 26 duplicate endpoints that did not exist."""
    d = desc
    if LIST_ONLY_NOTE in d:
        d = d.replace(LIST_ONLY_NOTE, "")
    return (d.rstrip() +
            "  Second hop: the record behind each list hit is fetched from the "
            "row's detail endpoint and merged in under detail.*, so the row "
            "returns the record and not just the search result. Records past the "
            "per-run detail budget ($JO_HP_DETAIL_MAX) are stamped "
            "_detail_pending rather than shipped as though nothing was behind "
            "them.")


def c_ident(s):
    return re.sub(r"[^A-Za-z0-9]", "_", s)


def render_table(rows, collector, batch, part, total_parts):
    out = []
    out.append("/* Deep-record %s sources (%d), part %d of %d.\n"
               " *\n"
               " * Batch %s verified each of these endpoints live AND separately probed the\n"
               " * per-record detail endpoint below it with a real id. The list half shipped as\n"
               " * a VJSON collector and the proven detail half sat unused in\n"
               " * docs/candidate-sources-batch%s.detail-hops.tsv, which is a rule-2 violation\n"
               " * (docs/SOURCE_EXHAUSTIVENESS.md): the route to the record behind each hit was\n"
               " * known and not walked. These rows walk it.\n"
               " *\n"
               " * Generated by collectors/gen_detail_hop_upgrade.py — regenerate rather than\n"
               " * hand-editing. */\n"
               % (collector, len(rows), part, total_parts, batch, batch))
    out.append('#include "lib/hpengine.h"\n\n')
    out.append("static const hp_source T[] = {\n")
    for r in rows:
        out.append("  { .id = \"%s\",\n" % r["id"])
        out.append("    .name = \"%s\",\n" % r["name"])
        if r["name_ja"] and r["name_ja"] != r["name"]:
            out.append("    .name_ja = \"%s\",\n" % r["name_ja"])
        out.append("    .collector = \"%s\", .category = \"%s\",\n"
                   % (r["collector"], r["category"]))
        out.append("    .description = \"%s\",\n" % r["description"])
        out.append("    .record_type = \"%s\", .tags = \"%s\", .lang = \"%s\",\n"
                   % (r["record_type"], r["tags"], r["lang"]))
        if r["mode"] != "HP_JSON":
            out.append("    .mode = %s,\n" % r["mode"])
        out.append("    .url = \"%s\",\n" % r["url"])
        if r["array_path"]:
            out.append("    .array_path = \"%s\",\n" % r["array_path"])
        if r.get("id_keys"):
            # Carried verbatim from the row's own macro argument. Getting this
            # wrong fails SILENTLY — the records collapse onto one uid at the
            # sink and the run still reports success — so it is copied, never
            # re-derived.
            out.append("    .id_keys = \"%s\",\n" % r["id_keys"])
        out.append("    .detail_url = \"%s\", .detail_key = \"%s\",\n"
                   % (r["detail_url"], r["detail_key"]))
        if r["page_param"]:
            out.append("    .page_param = \"%s\",%s%s   /* declared by the row's "
                       "own URL */\n"
                       % (r["page_param"],
                          (" .page_size = %d," % r["page_size"]) if r["page_size"] else "",
                          (" .page_start = %d," % r["page_start"])
                          if r["page_start"] is not None else ""))
        elif r["mode"] == "HP_JSON":
            # No cursor in the URL is not the same as no paging: the VJSON row
            # this replaces was walked by jsonlist_next_page(), which also
            # follows a next link the SERVER publishes (GLEIF's links.next, a
            # page-size/cursor pair the response proves). page_walk keeps that
            # decision, so the move does not trade later pages for the hop.
            out.append("    .page_walk = 1,\n")
        out.append("    .interval = %s, .free_tier = 1 },\n\n" % r["interval"])
    out.append("};\nHP_REGISTER_TABLE(T)\n")
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--generated", required=True)
    ap.add_argument("--detail-hops", action="append", required=True)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--keep-on-vjson", action="append", default=[],
                    help="TSV of ids measured to store FEWER records on "
                         "hpengine than on VJSON; they are left where they are")
    ap.add_argument("--outdir", default=None,
                    help="where the hp tables go (default: --generated)")
    args = ap.parse_args()
    outdir = args.outdir or args.generated

    hops = load_detail_hops(args.detail_hops)
    print("detail hops loaded: %d" % len(hops))
    # Measured, not guessed: each id here was run in BOTH forms through the
    # real binary and the hpengine form stored less. Moving it anyway would
    # trade records for depth, which rule 2 does not allow.
    keep = set()
    for p in args.keep_on_vjson:
        with open(p, encoding="utf-8") as f:
            for line in f:
                if line.startswith("#") or line.startswith("id\t") or not line.strip():
                    continue
                keep.add(line.split("\t", 1)[0].strip())
    print("kept on VJSON by measurement: %d" % len(keep))

    # Every id registered in the generated dir before this run — from the vsrc
    # MACROS *and* from any hp table already there.
    #
    # The hp half is not decoration. This script writes one table per
    # (batch, collector) group of the CURRENT run, so running it twice with
    # different row sets rewrites a group's file with only the second run's
    # rows and drops the first run's. That cost 53 sources, and the earlier
    # version of this check could not see it: it read ids_before from vsrc
    # macros only, and rows converted by the first run are no longer in a vsrc
    # file, so they were outside the set being protected.
    ids_before = set()
    for fname in os.listdir(args.generated):
        if not fname.endswith(".c"):
            continue
        txt = open(os.path.join(args.generated, fname), encoding="utf-8").read()
        ids_before |= set(re.findall(
            r'^V[A-Z][A-Z0-9_]*\(\s*\w+\s*,\s*"([^"]+)"', txt, re.M))
        ids_before |= set(re.findall(r'^\s*\{ \.id = "([^"]+)"', txt, re.M))

    converted = []           # rows that become hp_source entries
    skipped_shape = []       # has a hop, but the macro has no hpengine mode
    edits = {}               # path -> new source text
    for fname in sorted(os.listdir(args.generated)):
        if not (fname.startswith("vsrc16_") or fname.startswith("vsrc17_")):
            continue
        path = os.path.join(args.generated, fname)
        src = open(path, encoding="utf-8").read()
        cuts = []
        for macro, start, end, raw in parse_macro_calls(src):
            vals = [c_str_value(a) for a in raw]
            if not vals or vals[1] is None:
                continue
            sid = vals[1]
            if sid not in hops or sid in keep:
                continue
            if macro not in CONVERTIBLE:
                skipped_shape.append((sid, macro, fname))
                continue
            sig = SIGNATURES[macro]
            if len(raw) != len(sig):
                print("  ?? %s in %s: %d args, expected %d — left alone"
                      % (sid, fname, len(raw), len(sig)), file=sys.stderr)
                continue
            a = dict(zip(sig, raw))
            detail_url, detail_key, _ = hops[sid]
            path_arg = c_str_value(a["path"]) if "path" in a else ""
            batch = "17" if fname.startswith("vsrc17_") else "16"
            list_url = c_str_value(a["url"])
            _pg = paging_for(list_url or "")
            if sid in URL_FIXUP:
                old, new = URL_FIXUP[sid]
                if old not in list_url:
                    print("  ?? %s: URL_FIXUP pattern %r not present — left alone"
                          % (sid, old), file=sys.stderr)
                else:
                    list_url = list_url.replace(old, new)
            converted.append({
                "id": sid,
                "name": c_str_value(a["name"]),
                "name_ja": c_str_value(a["name_ja"]),
                "collector": c_str_value(a["collector"]),
                "category": c_str_value(a["category"]),
                "url": list_url,
                # "" and "*" both mean "find the array yourself" to hpengine,
                # which is what jsonlist did with them too.
                "array_path": "" if path_arg in (None, "", "*") else path_arg,
                "lang": c_str_value(a["lang"]) or "en",
                "tags": tags_members(c_str_value(a["tags"]) or "[]"),
                "interval": a["interval"].strip(),
                "description": describe(c_str_value(a["description"]) or "",
                                        detail_url),
                "record_type": "%s-record" % c_str_value(a["category"]),
                "detail_url": detail_url,
                "detail_key": detail_key,
                "mode": CONVERTIBLE[macro],
                "id_keys": c_str_value(a["id_keys"]) if "id_keys" in a else "",
                "page_param": _pg[0] or "",
                "page_size": _pg[1],
                "page_start": _pg[2],
                "batch": batch,
                "src_file": fname,
            })
            cuts.append((start, end))
        if cuts:
            new = src
            for start, end in sorted(cuts, reverse=True):
                new = new[:start] + new[end:]
            # Collapse the blank runs the cuts leave behind.
            new = re.sub(r"\n{3,}", "\n\n", new)
            edits[path] = new

    print("convertible rows with a proven hop: %d" % len(converted))
    print("rows skipped for having no hpengine mode (VRSS/VGEO): %d"
          % len(skipped_shape))
    for sid, macro, fname in skipped_shape:
        print("  skip %-46s %-6s %s" % (sid, macro, fname))

    by = defaultdict(list)
    for r in converted:
        by[(r["batch"], r["collector"])].append(r)

    if args.dry_run:
        print("\n--dry-run: writing nothing. %d tables would be written."
              % len(by))
        return 0

    written = 0
    for (batch, collector), rows in sorted(by.items()):
        # Same chunking the vsrc generator uses: a table per collector, split so
        # no single file becomes unreviewable.
        chunks = [rows[i:i + 40] for i in range(0, len(rows), 40)] or [[]]
        for i, chunk in enumerate(chunks, 1):
            name = "hp%s_%s_%d.c" % (batch, c_ident(collector), i)
            open(os.path.join(outdir, name), "w", encoding="utf-8").write(
                render_table(chunk, collector, batch, i, len(chunks)))
            written += 1

    for path, text in edits.items():
        # A file whose every registration moved out is now just an include.
        # Leave no such stub behind — the Makefile globs this directory.
        if not MACRO_CALL.search(text):
            os.remove(path)
            print("  removed emptied %s" % os.path.basename(path))
        else:
            open(path, "w", encoding="utf-8").write(text)

    print("\nwrote %d hp tables; rewrote %d vsrc files" % (written, len(edits)))

    # NOTHING MAY VANISH. Every id this script saw in a vsrc file before it ran
    # must still be registered somewhere afterwards — moved into an hp table, or
    # left where it was. The alternative is what actually happened once: a
    # macro variant this parser could not see was deleted along with its file,
    # and the only thing that noticed was a manual id-set diff against the base
    # branch. A move that loses a source is not a move.
    moved = {r["id"] for r in converted}
    after = set()
    for fname in os.listdir(outdir):
        if not fname.endswith(".c"):
            continue
        txt = open(os.path.join(outdir, fname), encoding="utf-8").read()
        after |= set(re.findall(r'"([a-z0-9][a-z0-9._-]*)"', txt))
    lost = sorted(i for i in ids_before if i not in after and i not in moved)
    if lost:
        print("\nLOST %d id(s) — refusing to call this a success:" % len(lost),
              file=sys.stderr)
        for i in lost:
            print("   " + i, file=sys.stderr)
        return 2
    print("id check: %d ids seen before, 0 lost (%d moved to hp tables)"
          % (len(ids_before), len(moved)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
