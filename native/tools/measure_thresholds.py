#!/usr/bin/env python3
"""Measure the embedding-similarity thresholds against a REAL embedding server.

Open item E1: every cut-off in `core/thresholds.c` — entity merge, story
joining, semantic alerts, relevance, suggestions, the id resolver, camera
duplicates, the repair gate, the corpus KNN arm, source routing — was chosen
from typical multilingual-e5 scores and tested against a stub. CLAUDE.md says
so plainly: "none has been measured on the production model". Each one decides
whether two entities merge, whether two items are one story, and whether an
alert fires.

WHAT THIS TOOL DOES, AND WHAT IT REFUSES TO DO.

It measures. It does not write a threshold, and it never prints a number it
could not compute — that is rule 1 applied to a gate, the same discipline
`audit_page_two.py` learned when its record counter answered 1,232 for a page
holding 48. A key with no label set gets its score DISTRIBUTION and the word
UNLABELLED, never a recommendation.

A threshold is only meaningful FOR ONE MODEL, so the run records the model the
server reports at /v1/models and puts it in every output row. A reading taken
against a different model is a different reading.

THE LABEL PROBLEM, which is the whole difficulty.

To measure a cut-off you need pairs that SHOULD be judged the same and pairs
that should not. Three of the keys have a label already in the database that
does NOT come from an embedding, so they can be measured today:

  story_tau             intel_items.cluster_id — simhash near-duplicate
                        clustering. Independent of embeddings, so using it as
                        the positive label is not circular. story_id is NOT
                        usable: it was assigned using story_tau itself, and
                        measuring a threshold against its own output would
                        confirm whatever it already does.
  entity_vec_cos_floor  entities.norm_key, and each row's aliases. Two rows
                        that normalise to one key are the same entity by a
                        string rule, not a vector one.
  camera_dup_cosine_min camera_stills.sha256 (exact) and .phash (perceptual).
                        A pHash Hamming distance is an independent signal.

The other eight judge a QUERY against a record or a source, and nothing in the
database records a human's verdict on such a pair. They need a labelled file,
whose format is below; until one exists this tool prints their distribution and
says they are unlabelled. Inventing the labels would make the measurement a
restatement of the guess it is meant to replace.

LABEL FILE (--labels), one JSON object per line:

    {"key": "relevance_sim_min", "a": "<query text>", "b": "<record text>",
     "same": true, "note": "why a human says so"}

`same` is the human's verdict. `note` is free text and is carried through to
the output so a reviewer can see what was judged, not just the score.

USAGE

    python3 tools/measure_thresholds.py \
        --embed-url http://127.0.0.1:8081 \
        --db /path/to/jo.db \
        --labels docs/threshold-labels.jsonl \
        --out docs/measured-thresholds-<date>.tsv

Without --embed-url (or $JO_EMBED_URL) it exits 2 and says so: there is no
offline substitute, and a stub is what produced the numbers being questioned.
"""
import argparse
import json
import math
import os
import re
import sqlite3
import sys
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    from jo_tls import context as jo_tls_context      # the ONE TLS context (rule 4c)
except Exception:                                     # a plain http:// server needs none
    jo_tls_context = None

# ── the registry, read from the C so this tool cannot drift from it ──────────
# core/thresholds.c is the one definition; parsing it beats a second copy here,
# for the reason rule 4c gives about seven manifest parsers.
THR_RE = re.compile(
    r'\{\s*"(?P<key>[a-z_]+)"\s*,\s*"(?P<env>JO_[A-Z0-9_]+)"\s*,\s*'
    r'(?P<dflt>[A-Z0-9_]+)\s*,\s*(?P<min>[0-9.]+)\s*,\s*(?P<max>[0-9.a-zA-Z_()]+)\s*,\s*'
    r'"(?P<metric>[a-z_0-9]+)"', re.S)


def read_registry(src):
    """[(key, env, metric, default_macro)] from core/thresholds.c, in file order."""
    text = open(src, encoding="utf-8").read()
    out = [(m.group("key"), m.group("env"), m.group("metric"), m.group("dflt"))
           for m in THR_RE.finditer(text)]
    if not out:
        sys.exit("could not read any threshold from %s — has the table moved?" % src)
    return out


# A numeric #define, with an optional trailing `f` and an optional comment.
_DEFINE_RE = re.compile(r"^\s*#\s*define\s+(\w+)\s+\(?\s*([0-9]*\.?[0-9]+)f?\s*\)?\s*(?:/\*|//|$)",
                        re.M)


def compiled_defaults(root):
    """{macro: float} for every numeric #define under core/ and lib/.

    WHY THIS EXISTS. core/thresholds.c names each default as a MACRO — the value
    itself lives in the owning module's header, which CLAUDE.md calls "the
    documented default". Without resolving it, `in_force` was blank on any run
    without a live server, and `prec_at_in_force` / `rec_at_in_force` — the two
    columns that answer "is the value we ship right?" — could not be computed at
    all. That made the tool able to report a BETTER cut-off while saying nothing
    about the one in force, which is most of the question.

    A macro that does not resolve to a number is left out, and the row then
    reads an empty in_force with source `unresolved`: a default this tool could
    not read is not a default it may invent (rule 1 applied to the gate).

    HEADERS ARE READ FIRST AND WIN, then .c files fill the gaps. Measured
    2026-10-23: only six of the eleven defaults are in a header (relevance.h,
    suggest.h, story.h, camera_dupes.h, entity_vec.h, alert_sem.h) — the other
    five (RESOLVE_EMB_MIN_COS_DEFAULT, RESOLVE_EMB_MARGIN_DEFAULT,
    REPAIR_SEM_COS_FLOOR_DEFAULT, CORPUS_VEC_MAX_DISTANCE_DEFAULT,
    ROUTE_MIN_COS_DEFAULT) are in core/thresholds.c itself, so a header-only
    read resolved none of them. A conflict is only reported between two HEADERS, because a macro
    defined differently in two .c files is file-local to each and no conflict at
    all: PROBE_TIMEOUT_MS is 20000 in core/pagination_probe.c and 10000 in
    core/statusapi.c, and warning about that pair said nothing true.
    """
    vals, from_header = {}, set()
    for headers_pass in (True, False):
        for sub in ("core", "lib"):
            d = os.path.join(root, sub)
            if not os.path.isdir(d):
                continue
            for fn in sorted(os.listdir(d)):
                if fn.endswith(".h") != headers_pass:
                    continue
                try:
                    t = open(os.path.join(d, fn), encoding="utf-8",
                             errors="replace").read()
                except OSError:
                    continue
                for m in _DEFINE_RE.finditer(t):
                    name, num = m.group(1), m.group(2)
                    try:
                        v = float(num)
                    except ValueError:
                        continue
                    if name not in vals:
                        vals[name] = v
                        if headers_pass:
                            from_header.add(name)
                    elif headers_pass and name in from_header and vals[name] != v:
                        # Two headers disagreeing about one default is the drift
                        # rule 4c names, and guessing which is live hides it.
                        print("  (%s is #defined in two headers with different "
                              "values: %s and %s — not resolving it)"
                              % (name, vals[name], v), file=sys.stderr)
                        vals[name] = None
    return {k: v for k, v in vals.items() if v is not None}


def live_values(api):
    """The value in force per key, from GET /api/admin/thresholds, or {}."""
    if not api:
        return {}
    try:
        with urllib.request.urlopen(api.rstrip("/") + "/api/admin/thresholds",
                                    timeout=20) as r:
            j = json.load(r)
    except Exception as e:
        print("  (could not read live thresholds from %s: %s)" % (api, e), file=sys.stderr)
        return {}
    rows = j.get("thresholds") or j.get("data") or []
    return {t.get("key"): t for t in rows if isinstance(t, dict)}


# ── the embedding server, speaking exactly what core/llm.c speaks ───────────
class Embedder:
    """POST /v1/embeddings {"input": [...], "model": ...} -> unit vectors.

    Honours each entry's `index`, as llm_embed_ex does: a count check alone
    passes a response that names index 0 twice and never answers index 1.
    """

    def __init__(self, base, model=None, batch=32, timeout=120):
        self.base = base.rstrip("/")
        self.model = model or os.environ.get("JO_EMBED_MODEL") or "embedding-model"
        self.batch = batch
        self.timeout = timeout
        self.calls = 0
        self.dim = None
        self.cache = {}

    def _open(self, req):
        if req.full_url.startswith("https://") and jo_tls_context is not None:
            return urllib.request.urlopen(req, timeout=self.timeout,
                                          context=jo_tls_context())
        return urllib.request.urlopen(req, timeout=self.timeout)

    def identify(self):
        """What the server says it is. A threshold is per MODEL."""
        req = urllib.request.Request(self.base + "/v1/models")
        with self._open(req) as r:
            j = json.load(r)
        data = j.get("data") or []
        if isinstance(data, list) and data and isinstance(data[0], dict):
            return data[0].get("id") or "(unnamed)"
        return "(no /v1/models answer)"

    def __call__(self, texts):
        """Embed `texts`, returning a list of unit-normalised vectors."""
        need = [t for t in dict.fromkeys(texts) if t not in self.cache]
        for i in range(0, len(need), self.batch):
            chunk = need[i:i + self.batch]
            body = json.dumps({"input": chunk, "model": self.model}).encode()
            req = urllib.request.Request(
                self.base + "/v1/embeddings", data=body,
                headers={"Content-Type": "application/json"})
            with self._open(req) as r:
                j = json.load(r)
            self.calls += 1
            data = j.get("data")
            if not isinstance(data, list) or len(data) != len(chunk):
                raise RuntimeError("server answered %s entries for %d inputs"
                                   % (len(data) if isinstance(data, list) else "?",
                                      len(chunk)))
            seen = set()
            for e in data:
                idx, emb = e.get("index"), e.get("embedding")
                if not isinstance(idx, int) or idx in seen or not (0 <= idx < len(chunk)):
                    raise RuntimeError("response index %r is not a fresh input index" % idx)
                if not isinstance(emb, list) or not emb:
                    raise RuntimeError("entry %d carries no embedding" % idx)
                seen.add(idx)
                v = [float(x) for x in emb]
                if not all(math.isfinite(x) for x in v):
                    raise RuntimeError("entry %d has a non-finite component" % idx)
                n = math.sqrt(sum(x * x for x in v))
                if n <= 0:
                    raise RuntimeError("entry %d is a zero vector" % idx)
                self.dim = self.dim or len(v)
                self.cache[chunk[idx]] = [x / n for x in v]
        return [self.cache[t] for t in texts]


def cos(a, b):
    return sum(x * y for x, y in zip(a, b))


def l2(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


# ── label sources that already exist in the database ────────────────────────
def _text(title, body, limit=400):
    t = " ".join(x for x in (title, body) if x)
    return re.sub(r"\s+", " ", t).strip()[:limit]


def pairs_story(db, cap):
    """Positives: two items sharing a simhash cluster_id. Negatives: items from
    different clusters AND different sources. cluster_id is simhash, so it is
    independent of the embedding this measures.

    THE SAMPLE IS DRAWN PER SOURCE, not off the front of the table, and that is
    not a refinement — it is the difference between a measurement and nothing.
    This used to be `order by cluster_id limit cap*40`, which on a real database
    returns a prefix of ONE corner of the corpus: measured 2026-10-23 against a
    412,521-row database holding 31 sources, every one of the 8,000 rows it read
    came from the same source, so the "different source" test for a negative
    pair never once held and the key reported `n_neg = 0` — one class, no
    cut-off, after a full embedding run. It is the same error this tree wrote up
    the same day for a record key measured on a 2,000-row sample: a sample taken
    off the head of a collection is not a sample of the collection.
    """
    c = db.cursor()
    pos, neg = [], []
    # Up to `cap * 4` rows per source, so a corpus dominated by one source
    # cannot crowd the others out. row_number() needs SQLite 3.25+ (2018).
    try:
        c.execute("""with ranked as (
                       select cluster_id, uid, source_id, title, body,
                              row_number() over (partition by source_id
                                                 order by uid) rn
                       from intel_items
                       where cluster_id is not null and cluster_id != ''
                         and title is not null and title != '')
                     select cluster_id, uid, source_id, title, body
                     from ranked where rn <= ?""", (cap * 4,))
        rows = c.fetchall()
    except sqlite3.Error:
        # No window functions: fall back to the old prefix read rather than
        # measuring nothing, and the caller's label_source says which it was.
        c.execute("""select cluster_id, uid, source_id, title, body from intel_items
                     where cluster_id is not null and cluster_id != ''
                       and title is not null and title != ''
                     order by cluster_id limit ?""", (cap * 40,))
        rows = c.fetchall()
    by = {}
    for cid, uid, src, title, body in rows:
        by.setdefault(cid, []).append((uid, src, _text(title, body)))
    multi = [v for v in by.values() if len(v) >= 2]
    for grp in multi[:cap]:
        pos.append((grp[0][2], grp[1][2], "same simhash cluster_id"))
    # Negatives: one item per (source, cluster), then pair items whose source
    # AND cluster both differ. Walking sources round-robin rather than in file
    # order is what keeps a 400,000-row source from supplying both halves of
    # every pair.
    per_src = {}
    for cid, v in by.items():
        for _uid, src, txt in v:
            if txt:
                per_src.setdefault(src, []).append((cid, txt))
    order = sorted(per_src)
    i = 0
    while len(neg) < cap and len(order) >= 2:
        a_src = order[i % len(order)]
        b_src = order[(i + 1 + (i // len(order))) % len(order)]
        i += 1
        if i > cap * 20:
            break
        if a_src == b_src or not per_src[a_src] or not per_src[b_src]:
            continue
        acid, atxt = per_src[a_src].pop()
        bcid, btxt = per_src[b_src].pop()
        if acid != bcid:
            neg.append((atxt, btxt, "different cluster and different source"))
    return pos, neg


def pairs_entity(db, cap):
    """Positives: entity rows that normalise to one norm_key, or whose name is
    in another's aliases. Negatives: different norm_key AND different type."""
    c = db.cursor()
    c.execute("""select entity_id, type, canonical, norm_key, aliases_json
                 from entities where canonical is not null and canonical != ''
                 limit ?""", (cap * 60,))
    rows = c.fetchall()
    by_key, pos, neg = {}, [], []
    for eid, typ, canon, nkey, aliases in rows:
        by_key.setdefault((typ, nkey), []).append(canon)
        try:
            al = json.loads(aliases) if aliases else []
        except Exception:
            al = []
        for a in al[:2]:
            if isinstance(a, str) and a and a != canon and len(pos) < cap:
                pos.append((canon, a, "alias of the same entity row"))
    for (_t, _k), names in by_key.items():
        if len(names) >= 2 and len(pos) < cap:
            pos.append((names[0], names[1], "same norm_key"))
    keys = [k for k in by_key if by_key[k]]
    for i in range(0, min(len(keys) - 1, cap * 2), 2):
        (ta, ka), (tb, kb) = keys[i], keys[i + 1]
        if ka != kb and ta != tb:
            neg.append((by_key[keys[i]][0], by_key[keys[i + 1]][0],
                        "different norm_key and different type"))
        if len(neg) >= cap:
            break
    return pos, neg


def pairs_camera(db, cap):
    """Positives: stills sharing a sha256 (byte-identical) or a close pHash.
    Negatives: stills from different cameras with distant pHashes. OCR text is
    the only text a still carries, so a still without it cannot be paired."""
    c = db.cursor()
    try:
        c.execute("""select camera_id, sha256, phash, ocr_text from camera_stills
                     where ocr_text is not null and ocr_text != '' limit ?""",
                  (cap * 40,))
    except sqlite3.Error:
        return [], []
    rows = [(cid, sha, ph, re.sub(r"\s+", " ", (txt or "")).strip()[:400])
            for cid, sha, ph, txt in c.fetchall()]
    pos, neg, by_sha = [], [], {}
    for cid, sha, ph, txt in rows:
        if sha:
            by_sha.setdefault(sha, []).append(txt)
    for txts in by_sha.values():
        if len(txts) >= 2 and len(pos) < cap:
            pos.append((txts[0], txts[1], "identical sha256"))
    for i in range(0, min(len(rows) - 1, cap * 2), 2):
        a, b = rows[i], rows[i + 1]
        if a[0] != b[0] and a[3] and b[3]:
            neg.append((a[3], b[3], "different camera"))
        if len(neg) >= cap:
            break
    return pos, neg


DB_LABELS = {
    "story_tau": pairs_story,
    "entity_vec_cos_floor": pairs_entity,
    "camera_dup_cosine_min": pairs_camera,
}


# ── scoring ─────────────────────────────────────────────────────────────────
def deciles(xs):
    if not xs:
        return []
    s = sorted(xs)
    return [round(s[min(len(s) - 1, int(len(s) * q / 10))], 4) for q in range(11)]


def sweep(pos, neg, lower_is_same):
    """Precision/recall/F1 at every cut-off worth trying, plus the best F1.

    `lower_is_same` is true for an L2 DISTANCE, where a SMALLER score means
    "the same" — getting that backwards would report the worst cut-off as the
    best, and corpus_vec_max_distance is the one key it applies to.
    """
    scores = sorted(set(round(s, 4) for s in pos + neg))
    best, table = None, []
    for t in scores:
        if lower_is_same:
            tp = sum(1 for s in pos if s <= t); fp = sum(1 for s in neg if s <= t)
        else:
            tp = sum(1 for s in pos if s >= t); fp = sum(1 for s in neg if s >= t)
        fn = len(pos) - tp
        prec = tp / (tp + fp) if tp + fp else 0.0
        rec = tp / (tp + fn) if tp + fn else 0.0
        f1 = 2 * prec * rec / (prec + rec) if prec + rec else 0.0
        table.append((t, prec, rec, f1))
        if best is None or f1 > best[3]:
            best = (t, prec, rec, f1)
    return best, table


def at(value, pos, neg, lower_is_same):
    """Precision/recall the CURRENT value achieves — the number that says
    whether today's guess is wrong, and in which direction."""
    if value is None or not (pos or neg):
        return None
    if lower_is_same:
        tp = sum(1 for s in pos if s <= value); fp = sum(1 for s in neg if s <= value)
    else:
        tp = sum(1 for s in pos if s >= value); fp = sum(1 for s in neg if s >= value)
    fn = len(pos) - tp
    prec = tp / (tp + fp) if tp + fp else 0.0
    rec = tp / (tp + fn) if tp + fn else 0.0
    return prec, rec, (2 * prec * rec / (prec + rec) if prec + rec else 0.0)


def self_test():
    """Exercise the whole pipeline against a stub server in this process.

    This tests THE TOOL, which is a different act from measuring a threshold:
    a stub may not produce a reading about a model, and the run above refuses
    one for that reason, but it is exactly what is wanted to prove the sweep
    orders cut-offs correctly and that the L2 direction is not inverted.
    `cleanup_stale_uids.py --self-test` sets the precedent.
    """
    import threading, hashlib
    from http.server import BaseHTTPRequestHandler, HTTPServer

    def vec(t):
        """Deterministic 16-d unit vector; equal texts are identical, and a
        shared prefix lands them near each other."""
        h = hashlib.sha256(t.encode()).digest()
        base = [((h[i] / 255.0) - 0.5) for i in range(16)]
        if "SAME" in t:                    # a controllable near-duplicate family
            base = [b * 0.02 + 1.0 for b in base]
        return base

    class H(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            body = json.dumps({"data": [{"id": "stub-embed-model"}]}).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body))); self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            n = int(self.headers.get("Content-Length", 0))
            req = json.loads(self.rfile.read(n))
            data = [{"index": i, "embedding": vec(t)}
                    for i, t in enumerate(req["input"])]
            body = json.dumps({"data": data}).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body))); self.end_headers()
            self.wfile.write(body)

    srv = HTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = "http://127.0.0.1:%d" % srv.server_address[1]
    fails = []

    def ok(cond, what):
        print("  %-4s %s" % ("ok" if cond else "FAIL", what))
        if not cond:
            fails.append(what)

    e = Embedder(url)
    ok(e.identify() == "stub-embed-model", "the model is read from /v1/models")
    v = e(["alpha", "alpha", "beta"])
    ok(len(v) == 3 and abs(cos(v[0], v[1]) - 1.0) < 1e-9,
       "identical texts embed identically (cosine 1)")
    ok(abs(sum(x * x for x in v[0]) - 1.0) < 1e-9, "vectors come back unit-normalised")
    ok(cos(v[0], v[2]) < 0.999, "different texts are not identical")
    before = e.calls
    e(["alpha"])
    ok(e.calls == before, "an already-embedded text costs no request")

    # the sweep, on scores whose answer is known by construction
    pos, neg = [0.9, 0.85, 0.8], [0.3, 0.2, 0.1]
    best, tbl = sweep(pos, neg, lower_is_same=False)
    ok(abs(best[3] - 1.0) < 1e-9 and 0.3 < best[0] <= 0.8,
       "a separable cosine set gives F1 1.0 at a cut-off between the classes")
    ok(len(tbl) == 6, "the sweep tries every distinct score")
    # the same thing with the direction reversed: a DISTANCE, small = same.
    best_l2, _ = sweep([0.1, 0.2], [0.9, 1.0], lower_is_same=True)
    ok(abs(best_l2[3] - 1.0) < 1e-9 and 0.2 <= best_l2[0] < 0.9,
       "an L2 distance is swept the other way round (lower means same)")
    # and the direction MATTERS: read the same set the wrong way and it breaks.
    wrong, _ = sweep([0.1, 0.2], [0.9, 1.0], lower_is_same=False)
    ok(wrong[3] < 1.0, "reading a distance as a similarity cannot reach F1 1.0")

    cur = at(0.5, pos, neg, lower_is_same=False)
    ok(cur == (1.0, 1.0, 1.0), "precision/recall at a given value is reported")
    cur_bad = at(0.15, pos, neg, lower_is_same=False)
    ok(cur_bad[0] < 1.0 and cur_bad[1] == 1.0,
       "a too-low cut-off shows as full recall and lost precision")

    ok(deciles([1.0]) == [1.0] * 11, "deciles of one value do not crash")
    ok(deciles([]) == [], "deciles of nothing is nothing")

    reg = read_registry(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "..", "core", "thresholds.c"))
    ok(len(reg) >= 11, "the registry is read from core/thresholds.c (%d keys)" % len(reg))
    ok(any(m == "l2_distance" for _k, _e, m, _d in reg),
       "the one l2_distance key is seen as a distance")
    ok(any(m == "cosine_margin" for _k, _e, m, _d in reg),
       "the one margin key is seen as a margin")

    # Every key's compiled default must resolve to a number from its own
    # header, or `in_force` is blank and the two columns that judge the value
    # we ship cannot be computed — which is most of what this tool is for.
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    dfl = compiled_defaults(root)
    unresolved = [k for k, _e, _m, d in reg if d not in dfl]
    ok(not unresolved,
       "every key's compiled default resolves from its own header%s"
       % ("" if not unresolved else " (missing: %s)" % ", ".join(unresolved)))
    ok(all(0.0 <= dfl[d] <= 2.0 for _k, _e, _m, d in reg if d in dfl),
       "every resolved default is in the range a cosine or an L2 can take")

    srv.shutdown()
    print("\n%s" % ("all passed" if not fails else "%d FAILURES" % len(fails)))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--self-test", action="store_true",
                    help="exercise the tool against an in-process stub server")
    ap.add_argument("--embed-url", default=os.environ.get("JO_EMBED_URL"))
    ap.add_argument("--db")
    ap.add_argument("--labels")
    ap.add_argument("--registry", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "core", "thresholds.c"))
    ap.add_argument("--api", help="a running server, to read the values in force")
    ap.add_argument("--cap", type=int, default=200, help="pairs per class per key")
    ap.add_argument("--out", default="-")
    a = ap.parse_args()

    if a.self_test:
        sys.exit(self_test())

    if not a.embed_url:
        sys.exit("no embedding server: pass --embed-url or set JO_EMBED_URL.\n"
                 "There is no offline substitute — a stub is what produced the\n"
                 "numbers this tool exists to replace, so it measures nothing\n"
                 "rather than reporting a stub's agreement with itself.")

    reg = read_registry(a.registry)
    defaults = compiled_defaults(os.path.dirname(os.path.abspath(a.registry)) + "/..")
    emb = Embedder(a.embed_url)
    try:
        model = emb.identify()
    except Exception as e:
        sys.exit("embedding server at %s did not answer /v1/models: %s" % (a.embed_url, e))
    print("# model: %s" % model, file=sys.stderr)

    live = live_values(a.api)
    labels = {}
    if a.labels:
        for ln in open(a.labels, encoding="utf-8"):
            ln = ln.strip()
            if not ln or ln.startswith("#"):
                continue
            o = json.loads(ln)
            labels.setdefault(o["key"], []).append(o)

    db = sqlite3.connect(a.db) if a.db else None
    out = sys.stdout if a.out == "-" else open(a.out, "w", encoding="utf-8")
    print("# measured %s against model %s" % (
        __import__("datetime").date.today().isoformat(), model), file=out)
    print("# Nothing here is written back to any threshold. A key with no label\n"
          "# set reads UNLABELLED and carries a distribution only.", file=out)
    print("\t".join(["key", "metric", "in_force", "source", "label_source",
                     "n_pos", "n_neg", "pos_deciles", "neg_deciles",
                     "best_cutoff", "best_prec", "best_rec", "best_f1",
                     "prec_at_in_force", "rec_at_in_force", "verdict"]), file=out)

    for key, env, metric, dflt_macro in reg:
        lower_is_same = (metric == "l2_distance")
        in_force = None
        src = "unknown"
        if key in live:
            in_force, src = live[key].get("value"), live[key].get("source", "api")
        elif os.environ.get(env):
            try:
                in_force, src = float(os.environ[env]), "env"
            except ValueError:
                pass
        if in_force is None:
            # The same precedence core/thresholds.c implements: db > env >
            # the module's compiled constant. With no --api and no env var,
            # the compiled constant IS the value in force.
            if dflt_macro in defaults:
                in_force, src = defaults[dflt_macro], "default (%s)" % dflt_macro
            else:
                src = "unresolved (%s)" % dflt_macro

        pos_t, neg_t, label_source = [], [], "NONE"
        if key in labels:
            label_source = "labels file"
            for o in labels[key]:
                (pos_t if o.get("same") else neg_t).append((o["a"], o["b"], o.get("note", "")))
        elif db is not None and key in DB_LABELS:
            try:
                p, n = DB_LABELS[key](db, a.cap)
            except sqlite3.Error as e:
                p, n = [], []
                print("  (%s: database label query failed: %s)" % (key, e), file=sys.stderr)
            pos_t, neg_t = p, n
            label_source = {"story_tau": "intel_items.cluster_id (simhash)",
                            "entity_vec_cos_floor": "entities.norm_key + aliases",
                            "camera_dup_cosine_min": "camera_stills.sha256"}[key]

        if metric == "cosine_margin":
            # A margin is the GAP between the best and the runner-up, so a pair
            # cannot express it: it needs a triple. Said, not approximated.
            print("\t".join([key, metric, _f(in_force), src,
                             "NONE (needs a triple, not a pair)", "0", "0", "", "",
                             "", "", "", "", "", "UNMEASURABLE FROM PAIRS"]), file=out)
            continue

        if not pos_t and not neg_t:
            print("\t".join([key, metric, _f(in_force), src, label_source,
                             "0", "0", "", "", "", "", "", "", "", "UNLABELLED"]), file=out)
            continue

        texts = [t for p in (pos_t + neg_t) for t in p[:2]]
        try:
            vecs = emb(texts)
        except Exception as e:
            print("\t".join([key, metric, _f(in_force), src, label_source,
                             str(len(pos_t)), str(len(neg_t)), "", "", "", "", "", "",
                             "", "EMBED FAILED: %s" % e]), file=out)
            continue
        vi = {t: v for t, v in zip(texts, vecs)}
        f = l2 if lower_is_same else cos
        ps = [f(vi[x], vi[y]) for x, y, _ in pos_t]
        ns = [f(vi[x], vi[y]) for x, y, _ in neg_t]
        best, _tbl = sweep(ps, ns, lower_is_same)
        cur = at(in_force, ps, ns, lower_is_same)
        verdict = "MEASURED"
        if not ps or not ns:
            verdict = "ONE CLASS ONLY — distribution only, no cut-off"
            best = None
        print("\t".join([
            key, metric, _f(in_force), src, label_source,
            str(len(ps)), str(len(ns)),
            ",".join(str(x) for x in deciles(ps)),
            ",".join(str(x) for x in deciles(ns)),
            _f(best[0]) if best else "", _f(best[1]) if best else "",
            _f(best[2]) if best else "", _f(best[3]) if best else "",
            _f(cur[0]) if cur else "", _f(cur[1]) if cur else "", verdict]), file=out)

    print("# embedding requests: %d, dimension: %s" % (emb.calls, emb.dim), file=out)
    if out is not sys.stdout:
        out.close()
        print("wrote %s" % a.out, file=sys.stderr)


def _f(x):
    return "" if x is None else ("%.4f" % x if isinstance(x, float) else str(x))


if __name__ == "__main__":
    main()
