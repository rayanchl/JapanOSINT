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

 5. NEW ID, NEW ENDPOINT, SAME DATA. Every check above compares STRINGS, so
    a candidate that re-reads an existing dataset through a mirror, a
    different API version or another portal's copy is reported clean. Batch
    36 shipped 14 Socrata portal inventories that re-read a catalogue two
    hand-written collectors already read; batch 31 shipped three rows that
    re-fetched what an existing collector reads. `--semantic` compares
    MEANING: each candidate row (name + description + host/path) is embedded
    through JO_EMBED_URL and compared with every registered source's card —
    the exact text the service index embeds, from `japanosint
    --dump-source-cards` (core/source_card.h) — and a cosine at or above
    --semantic-threshold (0.92) is a SEMANTIC-DUP. It is a question for a
    human, not a verdict: counted with the near-misses, fatal only under
    --strict. Without JO_EMBED_URL it says the semantic check was SKIPPED;
    it never reports "0 semantic duplicates" for a check it did not make.

    Card vectors are cached per model (--semantic-cache) keyed by a hash of
    the exact text embedded, so only cards whose text changed are sent: the
    whole registry is ~18,900 cards, ~45 minutes of CPU embedding cold.

    `--coverage` prints the registry's cards counted by category x country
    (country read off the id's ISO 3166 token; "??" when the id carries
    none), and where the candidate rows would land — a thin cell is where a
    batch adds coverage, a thick one is where it more likely duplicates.

Usage:
  batch_exclusions.py --dump-ids ids.txt --dump-urls urls.txt [--bin BIN]
  batch_exclusions.py --check MANIFEST... [--bin BIN] [--strict]
                      [--semantic [--semantic-threshold 0.92]] [--coverage]
"""
import argparse
import array
import collections
import hashlib
import io
import json
import math
import os
import re
import shutil
import subprocess
import tempfile
import sys
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
COLLECTORS = os.path.join(NATIVE, "collectors")

sys.path.insert(0, HERE)
from manifest import iter_lines                             # noqa: E402

URL_FIELD = re.compile(r'\.(?:url|detail_url)\s*=\s*"([^"]+)"')
# The same field, with every adjacent literal of its value (after macro
# expansion) — see the IS_TABLE branch of scan().
URL_FIELD_JOINED = re.compile(r'\.(?:url|detail_url)\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+)')
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
    # runtime tokens FIRST (a date format holds %d): {date…} and the per-page {page}/{offset…}/{size}
    u = re.sub(r"\{date[^}]*\}|\{page\}|\{offset(?:1|_end)?\}|\{size\}", "{}", u)
    u = re.sub(r"\{q[a-zA-Z]*\}|\{Q\}|%s|%d|\{v\}|\{key\}", "{}", u)
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
    %s. See blind spot 3 in the module docstring.

    `skip_prefix` is a LIST of filename prefixes, because --skip-prefix is
    repeatable. A bare string is accepted and treated as a one-element list so
    that older callers keep working."""
    skips = ([skip_prefix] if isinstance(skip_prefix, str)
             else list(skip_prefix or []))
    skips = [sp for sp in skips if sp]
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
            if skips and any(f.startswith(sp) for sp in skips):
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
        ex = expand_macros(t)
        if IS_TABLE.search(t):
            # An hp table's URL is still C: `.url = CH_BASE "/company/{qn}/
            # officers?…"` and TW GCIS's two adjacent literals are one string
            # each. This branch used to take the FIRST literal of the field
            # straight from the raw text, before expand_macros() and literal
            # joining ran, so 188 of 8,712 table url/detail_url fields were
            # indexed truncated or not at all, and a manifest row duplicating
            # one of them passed as new. Only the url/detail_url fields are
            # read here — `.portal` is documentation, not an endpoint.
            for run in URL_FIELD_JOINED.finditer(ex):
                u = "".join(unesc(x) for x in STRLIT.findall(run.group(1)))
                if u.startswith("http") and keep_ep(norm(u)):
                    eps[norm(u)].add(rel)
            continue
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


# ---- semantic duplicates (blind spot 5) -----------------------------------

def load_cards(binpath):
    """[(id, kind, category, card_text)] from `--dump-source-cards`, with the
    dump's \\n \\t \\\\ escapes undone so the text is byte-identical to what
    the C index embeds."""
    out = subprocess.run([binpath, "--dump-source-cards"], capture_output=True,
                         check=True).stdout.decode("utf-8", "replace")
    cards = []
    for line in out.split("\n"):
        if not line:
            continue
        parts = line.split("\t", 3)
        if len(parts) != 4:
            continue
        t = re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t"}.get(m.group(1),
                                                              m.group(1)), parts[3])
        cards.append((parts[0], parts[1], parts[2], t))
    return cards


def bound_text(s):
    """The C side's bound (embed_bound_text): JO_EMBED_MAX_CHARS BYTES,
    default 1000, cut on a UTF-8 boundary — so a long card is embedded
    exactly as the index embedded it."""
    try:
        m = int(os.environ.get("JO_EMBED_MAX_CHARS", "1000"))
    except ValueError:
        m = 1000
    if m < 64:
        m = 1000
    b = s.encode("utf-8", "replace")
    return b[:m].decode("utf-8", "ignore") if len(b) > m else s


def http_json(url, body=None, timeout=120):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, headers={
        "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8"))


def embed_model(base):
    m = os.environ.get("JO_EMBED_MODEL")
    if m:
        return m
    d = http_json(base.rstrip("/") + "/v1/models", timeout=15)
    return os.path.basename(str(d["data"][0]["id"]))


def embed_batch(base, texts):
    d = http_json(base.rstrip("/") + "/v1/embeddings", {"input": texts})
    rows = sorted(d["data"], key=lambda e: e.get("index", 0))
    if len(rows) != len(texts):
        raise RuntimeError("embedding server answered %d vectors for %d inputs"
                           % (len(rows), len(texts)))
    return [unit(r["embedding"]) for r in rows]


def unit(v):
    n = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / n for x in v]


def text_key(t):
    return hashlib.sha1(t.encode("utf-8")).digest()[:8]


def cache_path(arg, model):
    if arg:
        return arg
    root = os.environ.get("XDG_CACHE_HOME") or os.path.join(
        os.path.expanduser("~"), ".cache")
    safe = re.sub(r"[^A-Za-z0-9_.-]", "_", model)
    return os.path.join(root, "japanosint", "source-cards-%s.bin" % safe)


def cache_load(path):
    """{8-byte text key: unit vector}. A cache of another width is ignored."""
    out, dim = {}, 0
    try:
        raw = open(path, "rb").read()
    except OSError:
        return out, 0
    if len(raw) < 4:
        return out, 0
    dim = int.from_bytes(raw[:4], "little")
    rec = 8 + 4 * dim
    if dim <= 0 or (len(raw) - 4) % rec:
        return {}, 0
    for off in range(4, len(raw), rec):
        a = array.array("f")
        a.frombytes(raw[off + 8:off + rec])
        out[raw[off:off + 8]] = list(a)
    return out, dim


def cache_save(path, vecs, dim):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(int(dim).to_bytes(4, "little"))
        for k, v in vecs.items():
            if len(v) != dim:
                continue
            f.write(k)
            f.write(array.array("f", v).tobytes())
    os.replace(tmp, path)


def embed_all(base, texts, cache, batch=32, label="cards"):
    """Vectors for `texts`, from the cache where the exact text is there."""
    need = [t for t in dict.fromkeys(texts) if text_key(t) not in cache]
    if need:
        sys.stderr.write("semantic: embedding %d %s (%d cached)\n"
                         % (len(need), label, len(texts) - len(need)))
    for i in range(0, len(need), batch):
        chunk = need[i:i + batch]
        for t, v in zip(chunk, embed_batch(base, chunk)):
            cache[text_key(t)] = v
    return [cache[text_key(t)] for t in texts]


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def candidate_text(r):
    """name + description + host/path — the candidate's own words, and the
    endpoint, which is where two rows reading one dataset most often agree."""
    u = urllib.parse.urlsplit(r.get("url", ""))
    return "%s. %s %s%s" % (r.get("name", ""), r.get("description", ""),
                            u.netloc, u.path)


def semantic_check(paths, binpath, threshold, cache_arg):
    """Print SEMANTIC-DUP lines; return the count, or None when skipped."""
    base = os.environ.get("JO_EMBED_URL", "").strip()
    if not base:
        print("SEMANTIC: SKIPPED — JO_EMBED_URL is not set, so no semantic "
              "duplicate check was made (this is not '0 semantic duplicates').")
        return None
    if not binpath:
        print("SEMANTIC: SKIPPED — --semantic needs --bin: the registry's cards "
              "come from japanosint --dump-source-cards.")
        return None
    px = os.environ.get("JO_EMBED_DOC_PREFIX", "")
    model = embed_model(base)
    cpath = cache_path(cache_arg, model)
    cache, cdim = cache_load(cpath)
    # The live width decides: a cache written at another width is another
    # space, even under the same model name.
    dim = len(embed_batch(base, ["dimension probe"])[0])
    if cdim != dim:
        cache = {}
    n_cached = len(cache)
    cards = load_cards(binpath)
    ctexts = [bound_text(px + c[3]) for c in cards]
    cvecs = embed_all(base, ctexts, cache)
    rows = []
    for p in paths:
        for lno, _raw, kind, r in iter_lines(p):
            if kind == "row":
                rows.append((os.path.basename(p), lno, r))
    qtexts = [bound_text(px + candidate_text(r)) for _f, _l, r in rows]
    qvecs = embed_all(base, qtexts, cache, label="candidate rows")
    if len(cache) != n_cached:
        cache_save(cpath, cache, dim)
    found = 0
    for i, (fname, lno, r) in enumerate(rows):
        q = qvecs[i]
        best, bi = -2.0, -1
        for j, v in enumerate(cvecs):
            s = dot(q, v)
            if s > best:
                best, bi = s, j
        if bi >= 0 and best >= threshold:
            print("%s:%d SEMANTIC-DUP %s ~ %s (cosine %.3f, %s/%s) — review: "
                  "the same data under another id or endpoint?"
                  % (fname, lno, r["id"], cards[bi][0], best, cards[bi][1],
                     cards[bi][2]))
            found += 1
        for k in range(i):
            s = dot(q, qvecs[k])
            if s >= threshold:
                print("%s:%d SEMANTIC-DUP-BATCH %s ~ %s (%s:%d, cosine %.3f)"
                      % (fname, lno, r["id"], rows[k][2]["id"], rows[k][0],
                         rows[k][1], s))
                found += 1
                break
    print("semantic: %d candidate row(s) against %d registry cards, model %s, "
          "threshold %.2f: %d flagged for review"
          % (len(rows), len(cards), model, threshold, found))
    return found


# ISO 3166-1 alpha-2, plus the two non-ISO tokens this registry's ids use
# for a jurisdiction ("uk", "eu").
ISO2 = frozenset("""
ad ae af ag ai al am ao aq ar as at au aw ax az ba bb bd be bf bg bh bi bj bl
bm bn bo bq br bs bt bv bw by bz ca cc cd cf cg ch ci ck cl cm cn co cr cu cv
cw cx cy cz de dj dk dm do dz ec ee eg eh er es et fi fj fk fm fo fr ga gb gd
ge gf gg gh gi gl gm gn gp gq gr gs gt gu gw gy hk hm hn hr ht hu id ie il im
in io iq ir is it je jm jo jp ke kg kh ki km kn kp kr kw ky kz la lb lc li lk
lr ls lt lu lv ly ma mc md me mf mg mh mk ml mm mn mo mp mq mr ms mt mu mv mw
mx my mz na nc ne nf ng ni nl no np nr nu nz om pa pe pf pg ph pk pl pm pn pr
ps pt pw py qa re ro rs ru rw sa sb sc sd se sg sh si sj sk sl sm sn so sr ss
st sv sx sy sz tc td tf tg th tj tk tl tm tn to tr tt tv tw tz ua ug um us uy
uz va vc ve vg vi vn vu wf ws ye yt za zm zw uk eu
""".split())
# Batch / family prefixes that come before the jurisdiction token.
BATCH_TOK = re.compile(r"^(jo|ap|vs|cs|hp|vsrc|csrc)\d*$")


def country_of(sid):
    toks = [t for t in re.split(r"[_\-.:]+", sid.lower()) if t]
    while toks and BATCH_TOK.match(toks[0]):
        toks = toks[1:]
    for t in toks[:2]:
        if t in ISO2:
            return "gb" if t == "uk" else t
        if t.startswith("jp") or t.startswith("jpl"):
            return "jp"
    return "??"


def coverage_report(paths, binpath):
    if not binpath:
        print("COVERAGE: SKIPPED — --coverage needs --bin for the registry's cards.")
        return
    cards = load_cards(binpath)
    cell = collections.Counter((c[2], country_of(c[0])) for c in cards
                               if c[1] != "pod")
    unknown = sum(n for (cat, cc), n in cell.items() if cc == "??")
    print("coverage: %d non-pod cards in %d category x country cells "
          "(%d with no country token in the id)"
          % (sum(cell.values()), len(cell), unknown))
    for (cat, cc), n in cell.most_common(25):
        print("  %-20s %-3s %6d" % (cat, cc, n))
    rows = []
    for p in paths or []:
        for lno, _raw, kind, r in iter_lines(p):
            if kind == "row":
                rows.append(r)
    if rows:
        land = collections.Counter((r.get("category", ""), country_of(r["id"]))
                                   for r in rows)
        print("candidate rows by cell (registry count beside each):")
        for (cat, cc), n in sorted(land.items(), key=lambda kv: -kv[1]):
            print("  %-20s %-3s +%-4d (registry %d)%s"
                  % (cat, cc, n, cell.get((cat, cc), 0),
                     "  NEW CELL" if not cell.get((cat, cc)) else ""))


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
    # REPEATABLE, and it has to be. It was a plain string option, so passing
    # it twice kept only the LAST value -- the identical repeated-flag trap
    # CLAUDE.md records for the exhaustiveness scanner's --strict, where the
    # gate printed "0 findings" for a set it had never opened. Here it meant
    # two batches could not be checked in one call: the first prefix was
    # silently dropped, that batch's own table WAS scanned, and every one of
    # its rows self-collided as a DUP-ID. PR #27 hit exactly that and had to
    # run one batch at a time against main's binary to get an answer.
    ap.add_argument("--skip-prefix", action="append", default=[],
                    help="collector filename prefix to treat as 'not yet in "
                         "tree'. Repeatable. --check only; NEVER applied to "
                         "the dumps. Use it only for the tables generated "
                         "from the manifests being checked.")
    ap.add_argument("--semantic", action="store_true",
                    help="also embed each --check row and compare it with every "
                         "registered source's card (blind spot 5). Needs --bin "
                         "and JO_EMBED_URL; says SKIPPED without them.")
    ap.add_argument("--semantic-threshold", type=float, default=0.92,
                    help="cosine at or above which a row is a SEMANTIC-DUP")
    ap.add_argument("--semantic-cache",
                    help="card-vector cache file (default "
                         "~/.cache/japanosint/source-cards-<model>.bin)")
    ap.add_argument("--coverage", action="store_true",
                    help="print registry cards by category x country, and "
                         "where the --check rows land")
    a = ap.parse_args()

    if a.coverage:
        coverage_report(a.check, a.bin)

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
        sys.stderr.write("SUPPRESSED: files named %s were not scanned; any id "
                         "or endpoint only they register cannot be reported "
                         "here\n"
                         % ", ".join(sp + "*" for sp in a.skip_prefix))
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

    if a.semantic:
        sem = semantic_check(a.check, a.bin, a.semantic_threshold,
                             a.semantic_cache)
        if sem:
            near += sem

    print("collisions: %d   near-misses needing a human read: %d" % (bad, near))
    if malformed:
        print("NOTE: %d manifest line(s) were not checked at all (see "
              "MALFORMED above)." % len(malformed))
    return 1 if (bad or (near and a.strict)) else 0


if __name__ == "__main__":
    sys.exit(main())
