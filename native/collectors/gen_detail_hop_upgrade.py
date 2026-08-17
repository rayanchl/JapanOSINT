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
      [--dry-run]
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
}
# Argument order of each macro, so a parsed call becomes a named dict. Mirrors
# collectors/sources/_verified_macros.inc — if that file's signatures change,
# this table has to change with it.
SIGNATURES = {
    "VJSON":    ["sym", "id", "name", "name_ja", "collector", "category",
                 "url", "path", "lang", "tags", "interval", "description"],
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

MACRO_CALL = re.compile(r"^(V[A-Z]+)\(", re.M)

# Cursor parameters the URL itself names but lib/pager.c's table cannot infer,
# because the row's page-SIZE parameter is house-specific (`rp`, `num`) rather
# than one of the seven conventional spellings. Each of these was found by
# `make audit-sources`, which flags them "paged endpoint fetched once — every
# later page is discarded"; the URL shipped with the cursor pinned at its first
# value, which is the discard. Declaring page_param makes the row walk.
#
# This is evidence from the URL, not a guess about the platform: an endpoint
# verified answering at `page=1` has a `page` cursor by construction.
PAGE_PARAM = {
    "global-taginfo-key-values":   "page",  # &page=1&rp=10
    "us-nhtsa-vpic-manufacturers": "page",  # &page=1
    "cordis-search-projects-query": "p",    # &p=1&num=10
    "cordis-search-results-query":  "p",    # &p=1&num=5
    "eu-cordis-projects-search":    "p",    # &p=1&num=5
}

# One row asks the upstream for a single record (`rows=1`) — the probe's own
# page size, shipped as the collector's. Fetching 1 of N sanctions and calling
# it a collection is the plainest possible rule-2 violation, so the page size is
# raised to a real one. `rows`/`start` is Solr's own paging contract and is
# already in lib/pager.c's cursor table, so the walk follows automatically.
#
# NOT RE-PROBED: this session has no egress (see docs/verified-sources-batch18…
# for the whole story), so `rows=100` is asserted from Solr's contract and not
# from a request anybody watched. If ESMA refuses it, the row reports a fetch
# failure — an honest error, which is the correct degradation.
URL_FIXUP = {
    "esma-solr-sanctions": ("&rows=1", "&rows=100"),
}


def split_c_args(text):
    """Split a macro argument list on top-level commas.

    C string literals in these files contain commas, escaped quotes and JSON
    brackets ("[\"ke\",\"statistics\"]"), so a naive split on "," corrupts the
    tags argument — which is how a lint pass once truncated 2,000 URLs at their
    first comma. Track quoting and nesting instead."""
    args, depth, quote, esc, cur = [], 0, False, False, []
    for ch in text:
        if esc:
            cur.append(ch)
            esc = False
            continue
        if ch == "\\":
            cur.append(ch)
            esc = True
            continue
        if ch == '"':
            quote = not quote
            cur.append(ch)
            continue
        if not quote:
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == "," and depth == 0:
                args.append("".join(cur).strip())
                cur = []
                continue
        cur.append(ch)
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
        while i < len(src):
            ch = src[i]
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                quote = not quote
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
        out.append("    .detail_url = \"%s\", .detail_key = \"%s\",\n"
                   % (r["detail_url"], r["detail_key"]))
        if r["page_param"]:
            out.append("    .page_param = \"%s\",   /* the URL pins this cursor "
                       "at its first value */\n" % r["page_param"])
        out.append("    .interval = %s, .free_tier = 1 },\n\n" % r["interval"])
    out.append("};\nHP_REGISTER_TABLE(T)\n")
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--generated", required=True)
    ap.add_argument("--detail-hops", action="append", required=True)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--outdir", default=None,
                    help="where the hp tables go (default: --generated)")
    args = ap.parse_args()
    outdir = args.outdir or args.generated

    hops = load_detail_hops(args.detail_hops)
    print("detail hops loaded: %d" % len(hops))

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
            if sid not in hops:
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
                "page_param": PAGE_PARAM.get(sid, ""),
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
    return 0


if __name__ == "__main__":
    sys.exit(main())
