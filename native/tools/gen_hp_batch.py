#!/usr/bin/env python3
"""Generate hpengine collector tables from a pipe-delimited source manifest.

Why this exists. The `vsrc*` path (collectors/gen_verified_sources.py) emits
scheduled FEED collectors from a TSV. It has no way to express the things an
hpengine row is for: an entity-pivot URL template, a shape gate, a detail
second hop, or a pagination rule. Authoring those by hand is ~25 lines of C per
source, which does not scale to a batch of several hundred and cannot be
audited by eye.

So: one manifest line per source, one generated table per manifest file. The
manifest carries the endpoint, the pivot template and the prose; this script
carries the C. Nothing here invents a source -- it only reformats rows a human
wrote and that collectors/verify_feeds.py has already proven fetch and emit.

Manifest columns, pipe-delimited (a description may contain commas, never a pipe):

   1 id            registry id, unique across the whole tree
   2 mode          json | csv | html
   3 want          shape gate: any|domain|ip|email|numeric|hash|icao24|eth|btc|asn
   4 category      source_def.category
   5 record_type   stamped on every emitted item
   6 tags          comma-separated, bare words
   7 portal        human-facing base URL
   8 name          English name
   9 name_ja       Japanese name (may be empty)
  10 url           endpoint template, with {q}/{qd}/{qh}/... tokens
  11 probe         concrete URL used for proof-of-life (template with a real entity)
  12 description   prose: what the record contains and why it is worth a request
  13 opts          k=v;k=v -- any other hp_source field

Usage:
  gen_hp_batch.py MANIFEST... --probe-out probes.tsv
  gen_hp_batch.py MANIFEST... --outdir DIR --pass-ids ids.txt --group G:blurb
"""
import os
import sys
import argparse
import re

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# ONE manifest parser for the seven tools that read these files. See
# tools/manifest.py for the incident that made a shared one non-negotiable:
# this generator resolved a duplicated opt last-wins while
# audit_batch_reachable.py resolved it first-wins, and the auditor therefore
# reported a row that runs daily as one that can never run.
from manifest import (COLS, split_opts, parse_opts,          # noqa: E402
                      load as _manifest_load)

MODE = {"json": "HP_JSON", "csv": "HP_CSV", "html": "HP_HTML", "xml": "HP_XML"}
WANT = {"any": "HP_ANY", "domain": "HP_DOMAIN", "ip": "HP_IP",
        "email": "HP_EMAIL", "numeric": "HP_NUMERIC", "hash": "HP_HASH",
        "icao24": "HP_ICAO24", "eth": "HP_ETH", "btc": "HP_BTC",
        "asn": "HP_ASN"}

STR_OPTS = {"array_path", "title_keys", "id_keys", "link_keys", "link_tmpl",
            "csv_delim", "csv_comment",
            "date_keys", "body_keys", "lat_key", "lon_key", "href_must",
            "base", "detail_url", "detail_key", "detail_path", "next_path",
            "page_param", "post_body", "content_type", "key_env", "type",
            "collector"}
INT_OPTS = {"detail_max", "page_start", "page_size", "page_max", "max_items",
            "page_zero_based",
            "filter_query", "csv_no_header", "free_tier", "interval"}
# hp_source.headers is `const char *headers[5]`, so it cannot be set by the
# generic scalar path above. A row declares them as header1/header2/header3 and
# they are emitted as an initialiser list. tools/probe_hp_batch.py sends the
# same headers when proving the row, so a row is never verified under different
# request conditions than the ones its collector will actually use.
HDR_OPTS = ("header1", "header2", "header3")

# Documentation-only opts. They are parsed, validated and then NOT emitted --
# they carry a statement about the row for another tool to read, exactly as the
# inline  marker does in C.
#
#   pagination_ok=<reason>   this endpoint genuinely cannot be paged, and here
#                            is what was measured. Without it a row like the
#                            Wikimedia search API -- whose limit is capped at
#                            100 with no offset parameter at all -- stays
#                            flagged by audit_batch_pagination forever, and a
#                            permanent warning is one nobody reads.
DOC_OPTS = {"pagination_ok"}

# An opt value that CONTAINS `;<known opt>=` was not written that way on
# purpose: an escaped `\;` ate the separator and the opt that followed it was
# absorbed into the previous value. Three rows in the tree are in that state
# right now and the shape is always the same --
#
#   ...;detail_key=id\;pagination_ok=from= is not an offset — measured...
#
# which generates `.detail_key = "id;pagination_ok=from= is not an offset…"`.
# The row is not merely missing its pagination_ok: it carries a WRONG detail
# key, silently, in committed C. The duplicate-opt check does not see this
# (there is only one key) and the integer check does not either (these are all
# string opts), so it needs its own.
#
# `post_body` is exempt: a form body legitimately contains `k=v` pairs and a
# semicolon is a legal separator in one.
def _swallowed_re():
    known = sorted(STR_OPTS | INT_OPTS | DOC_OPTS | set(HDR_OPTS))
    return re.compile(r";\s*(%s)=" % "|".join(re.escape(k) for k in known))


SWALLOWED = _swallowed_re()


def load(paths):
    """Parse manifests, failing loudly on any malformed or duplicated row.

    Line splitting, field count, duplicate ids and required fields are
    manifest.load()'s job — shared, so no two tools can read the same line
    differently. What stays here is the half only the generator can check:
    that `mode` and `want` name real hp_source enums, because only this file
    knows what those enums are."""
    rows = _manifest_load(paths, strict=True)
    for r in rows:
        if r["mode"] not in MODE:
            raise SystemExit("%s: bad mode %r" % (r["_src"], r["mode"]))
        if r["want"] not in WANT:
            raise SystemExit("%s: bad want %r" % (r["_src"], r["want"]))
        validate_opts(r)
    return rows


def validate_opts(r):
    """-> the resolved opts dict, or SystemExit naming the row.

    Called from load(), not only from emit_row(), because probe_hp_batch.py and
    diagnose_emit_keys.py import THIS load() and then send requests built from
    these opts. A row whose `header1` has had a pagination_ok reason glued onto
    it by a stray `\\;` gets probed with a corrupted User-Agent — verified under
    request conditions the engine will never reproduce, which is the exact
    failure a previous fix to probe_hp_batch was about."""
    parsed, dups, junk = parse_opts(r["opts"])
    for k in dups:
        raise SystemExit("%s: %s: duplicate opt %r -- there is no correct way "
                         "to resolve it, so nothing tries: manifest.parse_opts "
                         "reports duplicates and every reader refuses to guess"
                         % (r["_src"], r["id"], k))
    for tok in junk:
        raise SystemExit("%s: %s: opts token %r has no '=' -- a mistyped "
                         "separator silently swallows the opt that follows it"
                         % (r["_src"], r["id"], tok))
    for k, v in parsed.items():
        m = SWALLOWED.search(v) if k != "post_body" else None
        if m:
            raise SystemExit(
                "%s: %s: opt %s=... swallowed %r -- a `\\;` escaped the "
                "separator, so %r is part of this VALUE instead of being its "
                "own opt. The generated C would carry the wrong %s.\n  %s=%s"
                % (r["_src"], r["id"], k, m.group(1), m.group(1), k, k, v[:160]))
    return parsed


def cstr(s, indent="      "):
    """A C string literal, wrapped so no generated line runs long."""
    s = s.replace("\\", "\\\\").replace('"', '\\"')
    out, line = [], ""
    for word in s.split(" "):
        if line and len(line) + len(word) + 1 > 64:
            out.append(line)
            line = word
        else:
            line = (line + " " + word) if line else word
    if line:
        out.append(line)
    if len(out) == 1:
        return '"%s"' % out[0]
    body = ("\n" + indent).join('"%s "' % x for x in out[:-1])
    return body + "\n" + indent + '"%s"' % out[-1]



def emit_row(r):
    """One hp_source initialiser."""
    o = {}
    # WHY THE TWO CHECKS BELOW EXIST.
    #
    # `o[k] = v` is last-wins, and it used to be the only thing here. That made
    # two different failures invisible at generation time:
    #
    #   1. A DUPLICATED KEY. OSM_API_CHANGESETS_BLACKSEA carried
    #      `interval=86400\;pagination_ok=...;interval=86400`. The stray
    #      backslash escaped the separator, so the FIRST token parsed as the
    #      key `interval` with the value `86400;pagination_ok=OSM refuses...`,
    #      and the trailing duplicate then overwrote it with a clean 86400.
    #      The generated C was correct -- purely by accident of ordering -- so
    #      nothing downstream complained, while `pagination_ok` was swallowed
    #      into the bad value and never registered at all.
    #
    #      Worse, tools/audit_batch_reachable.py reads the same manifest with a
    #      FIRST-wins `opt()` lookup, so it saw interval="86400;pagination_ok=…",
    #      failed `.isdigit()`, and reported the row as "static URL and no
    #      interval -- never runs" (house rule 3) about a row that in fact runs
    #      daily. Two tools disagreeing about the same manifest line is the real
    #      defect; rejecting the duplicate outright removes the ambiguity
    #      instead of trying to keep two parsers in agreement forever.
    #
    #   2. A NON-INTEGER INT OPT. Those are emitted unquoted as
    #      `.interval = <v>,`, so a malformed value does not become a wrong
    #      number -- it becomes a C syntax error several hundred lines into a
    #      generated file, blamed on the table rather than on the manifest row
    #      that caused it. Checking here names the row.
    parsed = validate_opts(r)
    for k, v in parsed.items():
        if k in DOC_OPTS:
            if not v:
                raise SystemExit("%s: %s needs a reason" % (r["_src"], k))
            continue
        if k not in STR_OPTS and k not in INT_OPTS and k not in HDR_OPTS:
            raise SystemExit("%s: unknown opt %r" % (r["_src"], k))
        if k in INT_OPTS:
            body = v[1:] if v[:1] == "-" else v
            if not body.isdigit():
                raise SystemExit("%s: %s: opt %s=%r is not an integer"
                                 % (r["_src"], r["id"], k, v))
        o[k] = v
    hdrs = [o.pop(h) for h in HDR_OPTS if h in o]
    L = ['  { .id = "%s", .name = %s,' % (r["id"], cstr(r["name"]))]
    if r["name_ja"]:
        L.append('    .name_ja = "%s",' % r["name_ja"])
    L.append('    .category = "%s", .portal = "%s",'
             % (r["category"], r["portal"]))
    L.append('    .record_type = "%s",' % r["record_type"])
    tags = ",".join('\\"%s\\"' % t.strip()
                    for t in r["tags"].split(",") if t.strip())
    if tags:
        L.append('    .tags = "%s",' % tags)
    L.append('    .mode = %s, .want = %s, .free_tier = %s,'
             % (MODE[r["mode"]], WANT[r["want"]], o.pop("free_tier", "1")))
    L.append('    .url = "%s",' % r["url"].replace('"', '\\"'))
    if hdrs:
        L.append('    .headers = { %s },'
                 % ", ".join('"%s"' % h.replace('"', '\\"') for h in hdrs))
    for k in sorted(o):
        if k in STR_OPTS:
            L.append('    .%s = "%s",' % (k, o[k].replace('"', '\\"')))
        else:
            L.append('    .%s = %s,' % (k, o[k]))
    L.append('    .description = %s },' % cstr(r["description"]))
    return "\n".join(L)


HEADER = '''/* collectors/pivot/table/%(pfx)s_%(g)s.c -- batch %(batch)s: %(blurb)s
 *
 * Every row here names a machine-readable endpoint that was proof-of-life
 * verified before it was written: fetched over the wire by
 * collectors/verify_feeds.py, parsed in its declared mode, and confirmed to
 * yield at least one real record. Rows whose endpoint answered but produced
 * an empty result set were rejected rather than shipped -- a source that is
 * structurally guaranteed to emit nothing on every run is the exact failure
 * the no-fabrication rule exists to prevent.
 *
 * The observed shape and record count for each row are recorded in
 * docs/verified-sources-batch%(batch)s.tsv; the candidates that failed are kept as
 * data in docs/rejected-sources-batch%(batch)s.tsv rather than silently dropped.
 *
 * Generated by tools/gen_hp_batch.py from
 * docs/candidate-sources-batch%(batch)s.%(g)s.txt. Edit the manifest, not this file.
 */
#include "lib/hpengine.h"

static const hp_source %(tbl)s[] = {
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifests", nargs="+")
    ap.add_argument("--probe-out",
                    help="write id<TAB>probe_url for collectors/verify_feeds.py")
    ap.add_argument("--outdir")
    ap.add_argument("--pass-ids",
                    help="file of ids proven to emit; every other row is skipped")
    ap.add_argument("--group", action="append", default=[],
                    help="GROUP:blurb -- group is the manifest file stem")
    # Batches reuse beat names: batch 18 and batch 19 both have a `sigint`
    # group, and without a per-batch prefix both would generate hp3_sigint.c --
    # the later batch silently overwriting the earlier one's table. Namespace
    # the output file, never the group name.
    ap.add_argument("--batch", default="18", help="batch number used in the file header")
    ap.add_argument("--prefix", default="hp3",
                    help="output filename prefix, e.g. hp3b19 (default hp3)")
    a = ap.parse_args()

    rows = load(a.manifests)
    sys.stderr.write("%d manifest rows\n" % len(rows))

    if a.probe_out:
        with open(a.probe_out, "w", encoding="utf-8", newline="\n") as fh:
            for r in rows:
                fh.write("%s\t%s\n" % (r["id"], r["probe"]))
        sys.stderr.write("wrote %s\n" % a.probe_out)
        return

    if a.pass_ids:
        keep = set(x.strip() for x in open(a.pass_ids, encoding="utf-8")
                   if x.strip())
        before = len(rows)
        rows = [r for r in rows if r["id"] in keep]
        sys.stderr.write("proof-of-life filter: %d of %d rows kept\n"
                         % (len(rows), before))

    if not a.outdir:
        return

    blurbs = dict(g.split(":", 1) for g in a.group)
    groups = {}
    for r in rows:
        # Derive the group from the manifest filename, for any batch number.
        # This used to strip a hardcoded "candidate-sources-batch18." prefix, so
        # a batch-19 manifest produced a group of
        # "candidate-sources-batch19.energyenv" -- which became an array name
        # containing '-' and '.' and would not compile.
        stem = os.path.basename(r["_src"].rsplit(":", 1)[0])
        stem = re.sub(r"^candidate-sources-batch\d+\.", "", stem)
        stem = re.sub(r"\.txt$", "", stem)
        groups.setdefault(stem, []).append(r)

    total = 0
    for g, rs in sorted(groups.items()):
        tbl = a.prefix.upper() + "_" + g.upper()
        path = os.path.join(a.outdir, "%s_%s.c" % (a.prefix, g))
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(HEADER % {"g": g, "blurb": blurbs.get(g, g), "tbl": tbl, "pfx": a.prefix, "batch": a.batch})
            fh.write("\n\n".join(emit_row(r) for r in rs))
            fh.write("\n};\n\nHP_REGISTER_TABLE(%s)\n" % tbl)
        sys.stderr.write("%-46s %4d rows\n" % (path, len(rs)))
        total += len(rs)
    sys.stderr.write("total %d rows\n" % total)


if __name__ == "__main__":
    main()
