#!/usr/bin/env python3
"""Emit the ids and endpoints already present in the tree, and check a manifest.

A new batch must not re-register something the tree already has. There are
13k+ sources across 4k+ hosts, so this is not something an author can hold in
their head -- batch 18 wrote 70 rows that duplicated existing endpoints or ids
before this check existed.

Two subtleties this encodes, both learned the hard way:

  * `.portal` is documentation, never fetched. Harvesting it as an endpoint
    makes two tables that merely cite the same portal look like a duplicate
    fetch. For hp_source tables only `.url` and `.detail_url` count.
  * URL templates differ only in their placeholder: `?q={q}` and `?q=%s` are
    the same endpoint. Both normalise to `{}` before comparison.

WHAT THIS FILE GOT WRONG, AND WHY IT MATTERED
=============================================

This tool reported ZERO collisions for batch 20, which then shipped 42 rows
duplicating endpoints the tree already fetched. Three separate blind spots,
all of the same shape -- a check that could not see something, and said
"collisions: 0" anyway:

 1. IDS WERE HARVESTED WITH `[A-Za-z0-9_]+`, which does not include a hyphen.
    Most of the generated fleet is named with hyphens (`511-ontario-cameras`,
    `faa-class-airspace`), so they were invisible. Measured on this tree: the
    old pattern found 4,030 ids where the binary's registry holds 13,193. A
    duplicate-id check blind to 69% of the registry is not a check.

 2. IDS COMPOSED BY A MACRO WERE INVISIBLE AT ANY CHARSET.
    collectors/sources/av_faa_arcgis.c registers its fourteen sources through
    `AV_ARC_DEF(sym, ID, ...)`, so the literal `.id = "faa-class-airspace"`
    never appears anywhere. No regex over the source can find it. The fix is
    not a better regex: pass `--bin ./bin/japanosint` and the id set comes
    from the REGISTRY ITSELF via --list-sources, which is ground truth and
    immune to how the C was written. Without --bin the tool now says out loud
    that its id set is an approximation, instead of implying completeness.

 3. ENDPOINTS COMPOSED AT RUNTIME WERE INVISIBLE.
    The same file's fourteen endpoints are built by
        snprintf(url, ..., AV_ARC_BASE "%s/FeatureServer/0/query?...", service)
    with the layer name passed in from a table of macro invocations. A scan
    for literal `https://...` strings finds the BASE and nothing else, so a
    manifest row naming
        .../rest/services/Class_Airspace/FeatureServer/0/query?...
    matched nothing and was reported clean.

    This file now resolves `#define NAME "…"` string macros, joins C's
    adjacent-string-literal concatenation, follows `#include "*.inc"` so a
    builder in a shared .inc is attributed to the files that use it, and
    records any URL containing a `%s`/`%d` as a PREFIX FAMILY plus the set of
    identifier-like literals in the same translation unit. A candidate URL
    that starts with such a prefix AND whose path contains one of those
    literals as a component is a DUP-ENDPOINT-COMPOSED -- which is exactly how
    `Class_Airspace` is caught.

    A candidate that matches a family prefix but no literal is reported as
    NEAR-ENDPOINT: the tool genuinely cannot decide, and saying so is the only
    honest option. Near matches do not fail the run by default (`--strict`
    makes them fail); they are printed and counted, because a warning that is
    printed is reporting and a suppressed one is not.

 4. `--skip-prefix hp3_` USED TO DEFAULT ON AND APPLY TO --dump-urls, so a
    discovery pass reading that dump never saw batch 18's tables and happily
    proposed sources the tree already had. The default is now empty, and a
    skip NEVER applies to the dumps -- only to --check, where it exists for
    one narrow purpose (excluding the tables generated FROM the manifests
    being checked, which would otherwise self-collide). When a skip is in
    effect the tool prints what it suppressed.

Usage:
  batch_exclusions.py --dump-ids ids.txt --dump-urls urls.txt [--bin BIN]
  batch_exclusions.py --check MANIFEST... [--bin BIN] [--strict]
"""
import argparse
import collections
import io
import os
import re
import shutil
import subprocess
import tempfile
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
COLLECTORS = os.path.join(NATIVE, "collectors")

sys.path.insert(0, HERE)
from manifest import iter_lines                             # noqa: E402

URL_FIELD = re.compile(r'\.(?:url|detail_url)\s*=\s*"([^"]+)"')
ANY_URL = re.compile(r'https?://[^"\s\\]+')
# `-` and `.` belong here: `faa-class-airspace`, `511-ontario-cameras` and
# `us-openfda-device-pma-detail` are all real registry ids. Their absence is
# blind spot 1 above.
ID_FIELD = re.compile(r'\.id\s*=\s*"([A-Za-z0-9_.:-]+)"')
REG_SOURCE = re.compile(r'REGISTER_SOURCE\(\s*([A-Za-z0-9_]+)')
IS_TABLE = re.compile(r"hp_source\s+\w+\[\]")
TRAIL = '",)\\.'

STRLIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
# `#define NAME "..."` possibly continued over several lines with backslashes.
DEFINE = re.compile(r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+'
                    r'((?:[^\n\\]|\\\n|\\[^\n])*)', re.M)
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]+"([^"]+\.inc)"', re.M)
# A run of adjacent string literals is ONE C string. This is how
# `AV_ARC_BASE "%s/FeatureServer/..."` becomes a URL once the macro is
# substituted.
ADJACENT = re.compile(r'(?:"(?:[^"\\]|\\.)*"\s*)+')
# Identifier-shaped literals: a dataset, layer or table NAME that gets pasted
# into a %s. Four characters minimum, no whitespace, no slashes.
IDENTLIT = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_.+-]{3,}$')
# Words that are identifier-shaped but say nothing about WHICH dataset a URL
# points at. Matching on these would flag every ArcGIS row against every other.
STOPWORDS = frozenset("""
json geojson query true false null application text plain html xml csv utf-8
utf8 name title id uid link href type kind value data items results records
featureserver mapserver rest services arcgis http https where outfields
resultoffset resultrecordcount returngeometry format limit offset page count
api v1 v2 v3 v4 v5 latest current all none default gzip deflate accept
user-agent content-type authorization bearer token key apikey
""".split())


def norm(u):
    u = u.strip().rstrip(TRAIL)
    u = re.sub(r"^https?://", "", u)
    u = re.sub(r"\{q[a-zA-Z]*\}|%s|%d|\{v\}|\{key\}", "{}", u)
    return u.rstrip("/").lower()


def keep_ep(k):
    """A normalised endpoint worth indexing. `https://` on its own, or a
    fragment with no host in it, is not an endpoint — it is scanner noise, and
    an index full of it makes the collision report harder to read without
    catching anything."""
    return len(k) >= 8 and "." in k.split("/", 1)[0]


def unesc(s):
    """The literal's actual characters. Only the escapes that occur in URLs."""
    return (s.replace('\\"', '"').replace("\\\\", "\\")
             .replace("\\n", "\n").replace("\\t", "\t"))


def expand_macros(text):
    """Substitute `#define NAME "literal"` string macros into the text.

    Only pure-string macros, and only whole-word occurrences. This is not a C
    preprocessor and does not pretend to be one -- it exists to make
    `AV_ARC_BASE "%s/FeatureServer/…"` readable as the URL it becomes."""
    macros = {}
    for m in DEFINE.finditer(text):
        name, body = m.group(1), m.group(2)
        if "(" in text[m.start(1):m.start(2)]:
            continue                        # function-like macro
        body = body.replace("\\\n", " ")
        parts = STRLIT.findall(body)
        if parts and not re.sub(r'"(?:[^"\\]|\\.)*"', "", body).strip():
            macros[name] = "".join(parts)
    if not macros:
        return text
    pat = re.compile(r"\b(%s)\b" % "|".join(re.escape(k) for k in
                                            sorted(macros, key=len, reverse=True)))
    # Two passes: a base macro is routinely defined in terms of another.
    for _ in range(2):
        text = pat.sub(lambda m: '"%s"' % macros[m.group(1)], text)
    return text


def joined_literals(text):
    """Every C string constant in `text`, with adjacent literals joined."""
    out = []
    for run in ADJACENT.finditer(text):
        out.append("".join(unesc(x) for x in STRLIT.findall(run.group(0))))
    return out


def read_incs():
    inc = {}
    for root, _d, names in os.walk(COLLECTORS):
        if os.sep + "obj" in root:
            continue
        for f in names:
            if f.endswith(".inc"):
                try:
                    inc[f] = io.open(os.path.join(root, f), encoding="utf-8",
                                     errors="replace").read()
                except Exception:
                    pass
    return inc


def scan(skip_prefix=None):
    """-> (ids, {endpoint: {file}}, {family prefix: {file}}, {file: {segment}})

    `families` and `segments` are the composed-URL half: a family is the fixed
    part of a runtime-built URL, and the segments are the identifier-shaped
    literals in the same translation unit -- the things that can land in its
    %s. See blind spot 3 in the module docstring."""
    ids = set()
    eps = collections.defaultdict(set)
    fams = collections.defaultdict(set)
    segs = collections.defaultdict(set)
    incs = read_incs()
    used_incs = set()
    units = []

    for root, _d, names in os.walk(COLLECTORS):
        if os.sep + "obj" in root:
            continue
        for f in sorted(names):
            if not f.endswith((".c", ".inc")):
                continue
            if skip_prefix and f.startswith(skip_prefix):
                continue
            p = os.path.join(root, f)
            try:
                t = io.open(p, encoding="utf-8", errors="replace").read()
            except Exception:
                continue
            if f.endswith(".c"):
                for name in INCLUDE.findall(t):
                    b = os.path.basename(name)
                    if b in incs:
                        used_incs.add(b)
                        t += "\n" + incs[b]
            units.append((os.path.relpath(p, NATIVE), f, t))

    for rel, fname, t in units:
        # An .inc that some .c includes has already been folded into that .c;
        # scanning it standalone would create a family with no segments and a
        # NEAR-ENDPOINT warning nobody can act on.
        if fname.endswith(".inc") and fname in used_incs:
            continue
        ids.update(ID_FIELD.findall(t))
        ids.update(REG_SOURCE.findall(t))
        if IS_TABLE.search(t):
            urls = URL_FIELD.findall(t)
            for u in urls:
                if u.startswith("http") and keep_ep(norm(u)):
                    eps[norm(u)].add(rel)
            continue
        ex = expand_macros(t)
        lits = joined_literals(ex)
        for lit in lits:
            if not lit.startswith("http"):
                if IDENTLIT.match(lit) and lit.lower() not in STOPWORDS:
                    segs[rel].add(lit.lower())
                continue
            m = ANY_URL.match(lit)
            u = m.group(0) if m else lit
            # A format-string URL is BOTH: an endpoint in its own right (norm()
            # folds `%s` to `{}`, which is how a `{q}` manifest template
            # matches it) AND a prefix family. Registering only the family is
            # how an earlier draft of this fix LOST 691 endpoints that the
            # exact-match half had been catching all along.
            if keep_ep(norm(u)):
                eps[norm(u)].add(rel)
            fmt = re.search(r"%[-0-9.]*[sdlu]", u)
            if fmt:
                pre = norm(u[:fmt.start()])
                # A family must be more than a bare host, or every URL on that
                # host would "match" it.
                if pre.count("/") >= 1 and len(pre) >= 18:
                    fams[pre].add(rel)
        # A bare `https://…` in a comment, or inside a literal this scanner did
        # not join, still names an endpoint the tree knows about.
        for u in ANY_URL.findall(t):
            if keep_ep(norm(u)):
                eps[norm(u)].add(rel)
    return ids, eps, fams, segs


def ids_from_binary(binpath):
    """Ground truth: what the built registry actually holds.

    Immune to macros, generated code and naming conventions, which is the whole
    point -- a regex over C can only ever approximate this."""
    # Its own scratch database, removed afterwards: listing the registry boots
    # the binary, which applies the schema and seeds ~13k source rows. Doing
    # that into whatever JO_DB happens to be set would write 4 MB into a
    # developer's live database as a side effect of a lint.
    tmp = tempfile.mkdtemp(prefix="jo_batch_excl.")
    try:
        env = dict(os.environ, JO_FTS_REBUILD="0",
                   JO_DB=os.path.join(tmp, "ids.db"))
        p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                           text=True, env=env, timeout=900)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    out = set()
    for line in (p.stdout or "").splitlines():
        m = re.match(r"^(\S+)\s+collector=", line)
        if m:
            out.add(m.group(1))
    if not out:
        raise SystemExit("--bin %s produced no source list; refusing to report "
                         "on an id set this tool did not get.\n%s"
                         % (binpath, (p.stderr or "")[-600:]))
    return out


def composed_hit(url, fams, segs):
    """-> (verdict, file, why) for a URL built at runtime, or None."""
    k = norm(url)
    best = None
    for pre, files in fams.items():
        if k.startswith(pre) and (best is None or len(pre) > len(best[0])):
            best = (pre, files)
    if not best:
        return None
    pre, files = best
    # Only the part BEYOND the shared prefix can identify the dataset; the
    # prefix itself is common to every sibling and matching inside it would
    # flag every ArcGIS row against every other.
    comps = set(x for x in re.split(r"[/?&=;,]+", k[len(pre):]) if x)
    for f in sorted(files):
        hit = comps & segs.get(f, set())
        if hit:
            return ("DUP-ENDPOINT-COMPOSED", f,
                    "%s builds this endpoint at runtime; %r is one of its "
                    "layer/dataset names" % (f, sorted(hit)[0]))
    return ("NEAR-ENDPOINT", sorted(files)[0],
            "%s builds URLs under %s... at runtime — verify by hand that this "
            "layer is not already one of them" % (sorted(files)[0], pre[:60]))


def load_manifest(paths):
    """Manifest rows, and the lines that could not be parsed as rows.

    The skipped lines are RETURNED rather than dropped: a row with the wrong
    field count used to vanish silently here, so a malformed row was reported
    as having no collisions rather than as not having been checked."""
    rows, bad = [], []
    for p in paths:
        for lno, raw, kind, r in iter_lines(p):
            if kind == "row":
                rows.append((os.path.basename(p), lno, r["id"], r["url"]))
            elif kind == "bad":
                bad.append((os.path.basename(p), lno, raw.count("|") + 1))
    return rows, bad


def main():
    ap = argparse.ArgumentParser(
        description="Duplicate ids and endpoints, against the whole tree.")
    ap.add_argument("--dump-ids")
    ap.add_argument("--dump-urls")
    ap.add_argument("--check", nargs="*")
    ap.add_argument("--bin",
                    help="japanosint binary; its --list-sources is the "
                         "AUTHORITATIVE id set. Without it the id half of this "
                         "check is a regex approximation and says so.")
    ap.add_argument("--strict", action="store_true",
                    help="NEAR-ENDPOINT warnings fail the run too")
    ap.add_argument("--skip-prefix", default="",
                    help="collector filename prefix to treat as 'not yet in "
                         "tree'. --check only; NEVER applied to the dumps. "
                         "Use it only for the tables generated from the "
                         "manifests being checked.")
    a = ap.parse_args()

    # The dumps always see the whole tree. A discovery pass reads them, and a
    # dump with a hole in it proposes sources that already exist.
    dump_ids, dump_eps, _f, _s = (scan(None) if (a.dump_ids or a.dump_urls)
                                  else (set(), {}, {}, {}))
    if a.dump_ids:
        src = dump_ids
        if a.bin:
            src = ids_from_binary(a.bin)
        io.open(a.dump_ids, "w", newline="\n").write("\n".join(sorted(src)) + "\n")
        sys.stderr.write("%d ids -> %s%s\n"
                         % (len(src), a.dump_ids,
                            "" if a.bin else "  (regex scan; pass --bin for the"
                                             " real registry)"))
    if a.dump_urls:
        io.open(a.dump_urls, "w", newline="\n").write("\n".join(sorted(dump_eps)) + "\n")
        sys.stderr.write("%d endpoints -> %s\n" % (len(dump_eps), a.dump_urls))

    if not a.check:
        return 0

    ids, eps, fams, segs = scan(a.skip_prefix)
    if a.skip_prefix:
        sys.stderr.write("SUPPRESSED: files named %s* were not scanned; any id "
                         "or endpoint only they register cannot be reported "
                         "here\n" % a.skip_prefix)
    if a.bin:
        reg = ids_from_binary(a.bin)
        sys.stderr.write("id set: %d from the binary's registry "
                         "(regex scan of the tree finds %d)\n" % (len(reg), len(ids)))
        ids = ids | reg
    else:
        sys.stderr.write(
            "WARNING: no --bin, so the id set is a REGEX APPROXIMATION (%d "
            "ids). Macro-composed registrations (av_faa_arcgis.c's fourteen, "
            "for one) are invisible to it and a DUP-ID here can still be "
            "missed. Pass --bin ./bin/japanosint for the real registry.\n"
            % len(ids))

    rows, malformed = load_manifest(a.check)
    for fname, lno, nf in malformed:
        print("%s:%d MALFORMED    %d fields, want 13 — NOT CHECKED"
              % (fname, lno, nf))
    bad = len(malformed)
    near = 0
    seen_new = {}
    for fname, lno, sid, url in rows:
        if sid in ids:
            print("%s:%d DUP-ID       %s" % (fname, lno, sid)); bad += 1
        elif sid in seen_new:
            print("%s:%d DUP-ID-BATCH %s (also %s)" % (fname, lno, sid, seen_new[sid])); bad += 1
        else:
            seen_new[sid] = "%s:%d" % (fname, lno)
        k = norm(url)
        if k in eps:
            print("%s:%d DUP-ENDPOINT %s -> already in %s"
                  % (fname, lno, sid, sorted(eps[k])[0])); bad += 1
            continue
        hit = composed_hit(url, fams, segs)
        if hit and hit[0] == "DUP-ENDPOINT-COMPOSED":
            print("%s:%d DUP-ENDPOINT-COMPOSED %s -> %s" % (fname, lno, sid, hit[2]))
            bad += 1
            continue
        if hit:
            print("%s:%d NEAR-ENDPOINT %s -> %s" % (fname, lno, sid, hit[2]))
            near += 1
        eps[k].add("(this batch)")

    print("collisions: %d   near-misses needing a human read: %d" % (bad, near))
    if malformed:
        print("NOTE: %d manifest line(s) were not checked at all (see "
              "MALFORMED above)." % len(malformed))
    return 1 if (bad or (near and a.strict)) else 0


if __name__ == "__main__":
    sys.exit(main())
