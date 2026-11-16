#!/usr/bin/env python3
"""threshold_label.py — collect the human verdicts measure_thresholds.py needs.

Eight of the eleven similarity cut-offs in core/thresholds.c judge a QUERY
against a record, a source card or a rule, and nothing in the database records
a human's verdict on such a pair. Measured on the production model
(docs/measured-thresholds-2026-10-23.md), six of them sit below the band where
UNRELATED pairs score, so they filter nothing — and choosing a real value needs
pairs a person has marked "these two should match" or "these should not".
This tool gathers those marks. It never invents one: every label it writes is
a click.

Two halves:

  candidates   draw (a, b) pairs for each pair-judgeable key from a REAL,
               populated database. With --embed-url the pairs are spread across
               the whole score range (so the labels land around the boundary,
               where they decide something); without it, word-overlap pairs are
               mixed with random ones so both verdicts occur.
  serve        a page on 127.0.0.1 that shows one pair at a time and appends
               each verdict to the labels file, in exactly the format
               measure_thresholds.py --labels reads:
                 {"key": ..., "a": ..., "b": ..., "same": true|false, "note": ...}
               Scores are NOT shown while labelling, so they cannot anchor the
               verdict. Pairs already in the labels file are skipped, so a
               session can be stopped and resumed.

What each key's pair is (core/thresholds.c's own descriptions):

  relevance_sim_min        a question asked  vs a stored record
  corpus_vec_max_distance  a question asked  vs a stored record (L2 metric)
  route_min_cos            a question asked  vs a scheduled source's card
  suggest_min_cos          a question asked  vs an entity name
  resolve_emb_min_cos      a source's name   vs an entity-pivot source's card
  repair_sem_cos_floor     a source's record vs another record (same source?)
  alert_sem_threshold      a semantic alert rule's query (else a question)
                                             vs a stored record
  resolve_emb_margin       not a pair (a gap between two matches): not offered

USAGE

  ./bin/japanosint --dump-source-cards > /tmp/cards.tsv
  python3 tools/threshold_label.py candidates --db $JO_DB --cards /tmp/cards.tsv \\
      --embed-url http://127.0.0.1:8081 --per-key 60 --out /tmp/candidates.jsonl
  python3 tools/threshold_label.py serve --candidates /tmp/candidates.jsonl \\
      --out ../docs/threshold-labels.jsonl          # then open http://127.0.0.1:8765
  python3 tools/measure_thresholds.py --embed-url ... --db $JO_DB \\
      --labels ../docs/threshold-labels.jsonl --out ../docs/measured-thresholds-<date>.tsv
"""
import argparse
import html
import http.server
import json
import os
import random
import re
import sqlite3
import sys
import tempfile
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from measure_thresholds import Embedder, cos, l2, _text  # noqa: E402

KEYS = {
    "relevance_sim_min":       ("questions", "records", "cosine"),
    "corpus_vec_max_distance": ("questions", "records", "l2_distance"),
    "route_min_cos":           ("questions", "cards_scheduled", "cosine"),
    "suggest_min_cos":         ("questions", "entities", "cosine"),
    "resolve_emb_min_cos":     ("source_names", "cards_pivot", "cosine"),
    "repair_sem_cos_floor":    ("records", "records", "cosine"),
    "alert_sem_threshold":     ("alert_queries", "records", "cosine"),
}
QUESTION_FOR = {
    "relevance_sim_min": "Is this record about what was asked?",
    "corpus_vec_max_distance": "Should this record come back for this question?",
    "route_min_cos": "Should this question read this source?",
    "suggest_min_cos": "Is this entity a fair suggestion for what was typed?",
    "resolve_emb_min_cos": "Is this the source that name means?",
    "repair_sem_cos_floor": "Is the second record the same kind of content as the first?",
    "alert_sem_threshold": "Should this alert rule fire on this record?",
}


# ── pools: real rows only ────────────────────────────────────────────────────
def _rows(db, sql, args=()):
    try:
        return db.execute(sql, args).fetchall()
    except sqlite3.Error:
        return []        # a table this database does not have: an empty pool


def pool_questions(db, cap):
    out = [r[0] for r in _rows(db, "SELECT DISTINCT query FROM search_run_owners "
                                    "WHERE query IS NOT NULL AND trim(query)<>'' "
                                    "ORDER BY created_at DESC LIMIT ?", (cap,))]
    for (p,) in _rows(db, "SELECT params_json FROM search_history ORDER BY ts DESC LIMIT ?", (cap,)):
        try:
            q = json.loads(p or "{}").get("q")
        except ValueError:
            q = None
        if isinstance(q, str) and q.strip() and q not in out:
            out.append(q.strip())
    return out[:cap]


def pool_records(db, cap):
    # every source gets a chance (a prefix would be one source: the
    # measure_thresholds lesson), notices and disclosures excluded
    rows = _rows(db, "SELECT source_id, title, body FROM intel_items "
                     "WHERE (record_type IS NULL OR record_type NOT LIKE 'collector-%') "
                     "AND coalesce(title,'')<>'' ORDER BY random() LIMIT ?", (cap,))
    return [{"text": _text(t, b), "source": s} for s, t, b in rows if _text(t, b)]


def pool_entities(db, cap):
    return [r[0] for r in _rows(db, "SELECT canonical FROM entities "
                                    "ORDER BY mention_count DESC LIMIT ?", (cap,)) if r[0]]


def pool_alert_queries(db, cap):
    out = []
    for (p,) in _rows(db, "SELECT predicate_json FROM alert_rules LIMIT ?", (cap,)):
        try:
            j = json.loads(p or "{}")
        except ValueError:
            continue
        if j.get("mode") == "llm" and isinstance(j.get("nl_query"), str) and j["nl_query"].strip():
            out.append(j["nl_query"].strip())
    return out


def read_cards(path):
    """`japanosint --dump-source-cards`: id <TAB> kind <TAB> category <TAB> card."""
    out = []
    if not path:
        return out
    for ln in open(path, encoding="utf-8"):
        f = ln.rstrip("\n").split("\t")
        if len(f) < 4:
            continue
        card = f[3].replace("\\n", " ").replace("\\t", " ").replace("\\\\", "\\")
        out.append({"id": f[0], "kind": f[1], "text": card[:500]})
    return out


def build_pools(db, cards, cap):
    recs = pool_records(db, cap)
    q = pool_questions(db, cap)
    aq = pool_alert_queries(db, cap) or q
    return {
        "questions": [{"text": x} for x in q],
        "records": recs,
        "entities": [{"text": x} for x in pool_entities(db, cap)],
        "alert_queries": [{"text": x} for x in aq],
        "cards_scheduled": [c for c in cards if c["kind"] == "scheduled"][:cap],
        "cards_pivot": [c for c in cards if c["kind"] == "pivot"][:cap],
        "source_names": [{"text": c["id"]} for c in cards if c["kind"] == "pivot"][:cap],
    }


# ── choosing pairs ───────────────────────────────────────────────────────────
_WORD = re.compile(r"\w{3,}", re.UNICODE)


def overlap(a, b):
    wa, wb = set(_WORD.findall(a.lower())), set(_WORD.findall(b.lower()))
    return len(wa & wb) / (len(wa | wb) or 1)


def choose_pairs(key, A, B, n, rng, embed=None):
    """Up to n (a, b, score) pairs. With an embedder: spread evenly over the
    score range of a candidate grid, so both verdicts and the boundary are
    represented. Without: half best word overlap, half random."""
    if not A or not B:
        return []
    grid = []
    for a in rng.sample(A, min(len(A), max(4, n // 3))):
        for b in rng.sample(B, min(len(B), 40)):
            if a["text"] != b["text"]:
                grid.append((a, b))
    if not grid:
        return []
    metric = KEYS[key][2]
    if embed is not None:
        texts = list({t for a, b in grid for t in (a["text"], b["text"])})
        vec = dict(zip(texts, embed(texts)))
        scored = []
        for a, b in grid:
            va, vb = vec[a["text"]], vec[b["text"]]
            scored.append((a, b, l2(va, vb) if metric == "l2_distance" else cos(va, vb)))
        scored.sort(key=lambda t: t[2])
        if len(scored) <= n:
            return scored
        step = (len(scored) - 1) / (n - 1) if n > 1 else 0
        return [scored[round(i * step)] for i in range(n)]
    ranked = sorted(grid, key=lambda ab: overlap(ab[0]["text"], ab[1]["text"]), reverse=True)
    top = ranked[: n // 2]
    rest = [p for p in grid if p not in top]
    picks = top + rng.sample(rest, min(len(rest), n - len(top)))
    return [(a, b, None) for a, b in picks]


def cmd_candidates(args):
    db = sqlite3.connect(args.db)
    cards = read_cards(args.cards)
    pools = build_pools(db, cards, args.pool)
    rng = random.Random(args.seed)
    embed = Embedder(args.embed_url) if args.embed_url else None
    keys = args.key or list(KEYS)
    n_out = 0
    with open(args.out, "w", encoding="utf-8") as out:
        for key in keys:
            if key not in KEYS:
                sys.exit("unknown key %s (pair-judgeable keys: %s)" % (key, ", ".join(KEYS)))
            ap, bp, metric = KEYS[key]
            pairs = choose_pairs(key, pools[ap], pools[bp], args.per_key, rng, embed)
            if not pairs:
                print("  %-24s no pairs: pool %s=%d %s=%d" % (key, ap, len(pools[ap]), bp, len(pools[bp])),
                      file=sys.stderr)
            for a, b, sc in pairs:
                o = {"key": key, "a": a["text"], "b": b["text"], "metric": metric}
                if sc is not None:
                    o["score"] = round(sc, 4)
                out.write(json.dumps(o, ensure_ascii=False) + "\n")
                n_out += 1
            print("  %-24s %d pair(s)" % (key, len(pairs)), file=sys.stderr)
    print("wrote %d candidate pair(s) to %s%s" % (n_out, args.out,
          "" if embed else " (no --embed-url: word-overlap + random, not score-spread)"),
          file=sys.stderr)
    return 0


# ── labelling ────────────────────────────────────────────────────────────────
def label_key(o):
    return (o.get("key"), o.get("a"), o.get("b"))


def load_jsonl(path):
    out = []
    if path and os.path.exists(path):
        for ln in open(path, encoding="utf-8"):
            ln = ln.strip()
            if ln and not ln.startswith("#"):
                out.append(json.loads(ln))
    return out


class Session:
    """The candidates, the labels already written, and the append."""

    def __init__(self, candidates, out):
        self.cands = load_jsonl(candidates)
        self.out = out
        self.lock = threading.Lock()
        self.done = {label_key(o) for o in load_jsonl(out)}

    def next(self):
        for i, c in enumerate(self.cands):
            if label_key(c) not in self.done:
                return i, c
        return None, None

    def record(self, idx, same, note):
        c = self.cands[idx]
        o = {"key": c["key"], "a": c["a"], "b": c["b"], "same": bool(same),
             "note": (note or "").strip()}
        with self.lock:
            if label_key(o) in self.done:
                return False
            d = os.path.dirname(os.path.abspath(self.out))
            os.makedirs(d, exist_ok=True)
            with open(self.out, "a", encoding="utf-8") as f:
                f.write(json.dumps(o, ensure_ascii=False) + "\n")
            self.done.add(label_key(o))
        return True


PAGE = """<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Threshold labels</title><style>
:root{--bg:#fff;--fg:#1b1b1b;--muted:#666;--card:#f4f4f2;--line:#ddd;--acc:#2a5bd7}
@media (prefers-color-scheme:dark){:root{--bg:#141414;--fg:#eee;--muted:#aaa;--card:#202020;--line:#333;--acc:#7da2ff}}
body{background:var(--bg);color:var(--fg);font:16px/1.5 system-ui,sans-serif;max-width:860px;margin:0 auto;padding:16px}
.q{font-weight:600;margin:8px 0 12px}.k{color:var(--muted);font-size:13px}
.box{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:12px;margin:8px 0;white-space:pre-wrap;word-break:break-word}
.lbl{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.04em}
button{font:inherit;padding:10px 16px;margin:6px 6px 0 0;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg);cursor:pointer}
button.p{border-color:var(--acc)}input{font:inherit;width:100%%;box-sizing:border-box;padding:8px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg)}
</style></head><body>%s</body></html>"""


def render(sess):
    idx, c = sess.next()
    total, done = len(sess.cands), len(sess.done)
    if c is None:
        return PAGE % ("<h2>All %d pairs labelled</h2><p class=k>Labels are in %s.</p>"
                       % (total, html.escape(sess.out)))
    e = html.escape
    body = (
        "<div class=k>%d labelled · pair %d of %d · key <b>%s</b></div>"
        "<div class=q>%s</div>"
        "<div class=lbl>A</div><div class=box>%s</div>"
        "<div class=lbl>B</div><div class=box>%s</div>"
        "<form method=post action=/label>"
        "<input type=hidden name=i value=%d>"
        "<input name=note placeholder='note (optional): why'>"
        "<button class=p name=v value=1 accesskey=s>Same / should match (s)</button>"
        "<button name=v value=0 accesskey=d>Different (d)</button>"
        "<button name=v value=skip accesskey=k>Skip (k)</button></form>"
    ) % (done, idx + 1, total, e(c["key"]), e(QUESTION_FOR.get(c["key"], "")),
         e(c["a"]), e(c["b"]), idx)
    return PAGE % body


def make_handler(sess):
    skipped = set()

    class H(http.server.BaseHTTPRequestHandler):
        def _send(self, code, body, ctype="text/html; charset=utf-8"):
            b = body.encode()
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(b)))
            self.end_headers()
            self.wfile.write(b)

        def do_GET(self):
            self._send(200, render(sess))

        def do_POST(self):
            from urllib.parse import parse_qs
            n = int(self.headers.get("Content-Length") or 0)
            f = parse_qs(self.rfile.read(n).decode())
            try:
                i = int(f.get("i", ["-1"])[0])
            except ValueError:
                i = -1
            v = f.get("v", [""])[0]
            if 0 <= i < len(sess.cands):
                if v in ("0", "1"):
                    sess.record(i, v == "1", f.get("note", [""])[0])
                elif v == "skip":
                    skipped.add(i)
                    sess.done.add(label_key(sess.cands[i]))   # this session only
            self.send_response(303)
            self.send_header("Location", "/")
            self.end_headers()

        def log_message(self, *a):
            pass
    return H


def cmd_serve(args):
    sess = Session(args.candidates, args.out)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(sess))
    print("labelling %d pair(s), %d already labelled -> %s\nopen http://127.0.0.1:%d"
          % (len(sess.cands), len(sess.done), args.out, args.port), file=sys.stderr)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


# ── self-test ────────────────────────────────────────────────────────────────
def self_test():
    ok = True

    def check(cond, what):
        nonlocal ok
        print("  %s %s" % ("ok  " if cond else "FAIL", what))
        ok = ok and bool(cond)

    d = tempfile.mkdtemp()
    db = sqlite3.connect(os.path.join(d, "t.db"))
    db.executescript(
        "CREATE TABLE intel_items(uid TEXT, source_id TEXT, title TEXT, body TEXT, record_type TEXT);"
        "CREATE TABLE search_run_owners(request_id TEXT, query TEXT, created_at TEXT);"
        "CREATE TABLE entities(canonical TEXT, mention_count INT);")
    for i in range(30):
        db.execute("INSERT INTO intel_items VALUES(?,?,?,?,?)",
                   ("u%d" % i, "s%d" % (i % 3), "earthquake report %d" % i if i % 2 else "port traffic %d" % i,
                    "body", "news"))
    db.execute("INSERT INTO intel_items VALUES('n','s0','notice','x','collector-truncation-notice')")
    for q in ("earthquake damage", "port congestion", "election results"):
        db.execute("INSERT INTO search_run_owners VALUES(?,?,datetime('now'))", (q, q))
    db.execute("INSERT INTO entities VALUES('Toyota',5)")
    db.commit()
    pools = build_pools(db, [], 100)
    check(len(pools["questions"]) == 3, "questions come from the runs actually asked")
    check(all("notice" not in r["text"] for r in pools["records"]), "notices are never offered as records")
    check(pools["alert_queries"] == pools["questions"], "no semantic alert rules: questions stand in")
    rng = random.Random(1)
    pairs = choose_pairs("relevance_sim_min", pools["questions"], pools["records"], 10, rng)
    check(len(pairs) == 10 and all(s is None for _, _, s in pairs), "no embedder: 10 unscored pairs")
    check(any(overlap(a["text"], b["text"]) > 0 for a, b, _ in pairs)
          and any(overlap(a["text"], b["text"]) == 0 for a, b, _ in pairs),
          "both word-overlap and unrelated pairs are offered")

    class Fake:                       # a deterministic 2-d "embedding"
        def __call__(self, texts):
            return [[1.0, 0.0] if "earthquake" in t else [0.6, 0.8] for t in texts]
    sp = choose_pairs("relevance_sim_min", pools["questions"], pools["records"], 6, rng, Fake())
    sc = [s for _, _, s in sp]
    check(len(sp) == 6 and sc == sorted(sc) and sc[0] < sc[-1], "with an embedder: spread over the score range")
    cand = os.path.join(d, "c.jsonl")
    with open(cand, "w") as f:
        for a, b, _ in pairs[:3]:
            f.write(json.dumps({"key": "relevance_sim_min", "a": a["text"], "b": b["text"]}) + "\n")
    out = os.path.join(d, "labels.jsonl")
    s = Session(cand, out)
    i, c = s.next()
    check(s.record(i, True, " because ") and not s.record(i, False, ""), "a verdict is written once")
    lab = load_jsonl(out)
    check(len(lab) == 1 and lab[0]["same"] is True and lab[0]["note"] == "because"
          and set(lab[0]) == {"key", "a", "b", "same", "note"},
          "the labels file has exactly measure_thresholds' fields")
    s2 = Session(cand, out)
    check(s2.next()[0] == 1, "a resumed session skips what is already labelled")
    page = render(s2)
    check("score" not in page.lower() and "Same / should match" in page, "the page never shows a score")
    print("threshold_label self-test: %s" % ("ok" if ok else "FAILED"))
    return 0 if ok else 1


def main():
    if "--self-test" in sys.argv:
        return self_test()
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("candidates", help="draw pairs to label from a populated database")
    c.add_argument("--db", required=True)
    c.add_argument("--cards", help="output of ./bin/japanosint --dump-source-cards")
    c.add_argument("--embed-url", default=os.environ.get("JO_EMBED_URL"))
    c.add_argument("--key", action="append", help="limit to these keys (repeatable)")
    c.add_argument("--per-key", type=int, default=40)
    c.add_argument("--pool", type=int, default=400, help="rows drawn per pool")
    c.add_argument("--seed", type=int, default=0)
    c.add_argument("--out", required=True)
    s = sub.add_parser("serve", help="label the candidates on a local page")
    s.add_argument("--candidates", required=True)
    s.add_argument("--out", required=True, help="e.g. ../docs/threshold-labels.jsonl")
    s.add_argument("--port", type=int, default=8765)
    a = ap.parse_args()
    return cmd_candidates(a) if a.cmd == "candidates" else cmd_serve(a)


if __name__ == "__main__":
    sys.exit(main())
