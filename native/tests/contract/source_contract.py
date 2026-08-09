#!/usr/bin/env python3
"""tests/contract/source_contract.py — run sources and judge what they persisted.

This is the audit in tests/audit/ turned into something repeatable. That audit
found 16 cross-cutting defects by hand across ten agent slices; every one of
them is a property of the ROWS a collector writes, so every one of them is
checkable automatically. The point is not to re-find those bugs — most are
fixed — it is that nothing currently stops the 17th.

Usage:
  python3 tests/contract/source_contract.py --ids-file new_ids.txt [--jobs 6]
  python3 tests/contract/source_contract.py --all            # whole fleet
  python3 tests/contract/source_contract.py --ids a,b,c --baseline prev.tsv

Writes a TSV verdict table to --out (default tests/contract/verdicts.tsv) and
exits non-zero if any source is FAIL. With --baseline it also reports
regressions against a previous run, which is the form a nightly job wants:
"what got worse since yesterday" is the actionable signal, not the absolute
list.

Each source runs against its OWN scratch database so one collector's rows can
never be mistaken for another's, and so a run is never judged against
leftovers from a previous one.
"""

import argparse, concurrent.futures, json, os, re, sqlite3, subprocess, sys, tempfile, shutil

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BIN  = os.path.join(REPO, "bin", "japanosint")

# Defect #1 was RFC-822 (`Fri, 01 Aug 2026 …`) stored in a column every
# consumer ORDER BYs as TEXT: 'F' > '2', so those rows outrank every ISO row
# regardless of date and sort among themselves by weekday name. What actually
# matters is therefore "does it start with a lexically sortable YYYY-MM-DD",
# not "does it carry a time".
#
# An earlier version required a [T ] separator after the date and flagged 43
# sources that were emitting plain `2026-07-31` — a perfectly valid ISO-8601
# date that sorts correctly. Date-only is the honest representation when the
# upstream publishes a date; demanding a fake midnight would be worse.
ISO = re.compile(r"^\d{4}-\d{2}-\d{2}([T ].*)?$|^\d{4}-\d{2}-\d{2}[A-Z+\-]")


# An entity-pivot service reads ctx->entity and emits nothing without one, so
# running it bare produces a false EMPTY. The audit hit this hard: ~50 verdicts
# were overturned simply by supplying an entity of the right KIND (ICAO24 hex,
# VIN, DOI, postal code...). These are the defaults; override per id with
# --entities-file (TSV: id<TAB>entity).
PIVOT_DEFAULTS = {
    "domain":   "example.com",
    "ip":       "8.8.8.8",
    "email":    "test@example.com",
    "username": "torvalds",
    "company":  "Toyota Motor Corporation",
    "person":   "Ada Lovelace",
    "asn":      "AS15169",
    "hash":     "44d88612fea8a8f36de82e1278abb02f",
    "url":      "https://example.com/",
    "coord":    "35.6812,139.7671",
    "icao24":   "4b1815",
    "imo":      "9074729",
    "mmsi":     "636019825",
    "lei":      "5493001KJTIIGC8Y1R12",
    "vin":      "1HGCM82633A004352",
    "phone":    "+81312345678",
    "keyword":  "security",
    "norad":    "25544",
    "callsign": "N0CALL",
    # Kinds the 2026-08 source sweep introduced. A pivot kind with no default
    # silently falls back to a keyword and the service reports a false EMPTY,
    # so every kind that appears in entities.tsv needs an entry here.
    "tail":     "N628TS",          # aircraft registration
    "summit-ref": "G/CE-001",      # SOTA summit reference
    "person or company name": "Toyota Motor Corporation",
}

# Heuristic used when no explicit entity is given: an UPPER_SNAKE id is the
# repo's convention for an entity-pivot OSINT service.
def is_pivot_id(sid):
    return sid.isupper() or (sid.replace("_", "").isupper() and "_" in sid)


def run_source(sid, timeout, entity=None):
    """Run one source into a private scratch DB; return (rc, stderr, dbpath, tmpdir)."""
    tmp = tempfile.mkdtemp(prefix="jo-contract-")
    db = os.path.join(tmp, "s.db")
    env = dict(os.environ, JO_DB=db)
    cmd = [BIN, "--run", sid] + ([entity] if entity else [])
    try:
        p = subprocess.run(cmd, env=env, timeout=timeout,
                           capture_output=True, text=True, errors="replace")
        return p.returncode, p.stderr, db, tmp
    except subprocess.TimeoutExpired:
        return 124, f"TIMEOUT after {timeout}s", db, tmp


def check_rows(db, sid):
    """Inspect what actually landed. Returns (nrows, [violations])."""
    v = []
    if not os.path.exists(db):
        return 0, ["no database was created"]
    con = sqlite3.connect(db)
    con.text_factory = bytes          # judge encoding ourselves, don't let sqlite hide it
    cur = con.cursor()
    try:
        # Column is `properties` in the schema (the C struct field is
        # properties_json; they are not the same name).
        cur.execute("SELECT uid,title,summary,body,published_at,properties,"
                    "lat,lon,geometry FROM intel_items WHERE source_id=?", (sid,))
        rows = cur.fetchall()
    except sqlite3.Error as e:
        con.close()
        return 0, [f"query failed: {e}"]
    con.close()

    n = len(rows)
    if n == 0:
        return 0, []                  # emptiness is judged by the caller, not here

    def dec(b):
        if b is None:
            return None
        try:
            return b.decode("utf-8")
        except UnicodeDecodeError:
            return False              # sentinel: invalid UTF-8

    untitled = bad_utf8 = bad_props = bad_date = 0
    coords = []
    declared_coarse = 0        # rows that admit their coordinate is approximate
    for uid, title, summary, body, pub, props, lat, lon, geom in rows:
        t = dec(title)
        if t is False or dec(summary) is False or dec(body) is False:
            bad_utf8 += 1             # defect #2: byte-boundary truncation of UTF-8
        elif not t or not t.strip():
            untitled += 1             # defect #9: title-less rows are blank in every UI

        p = dec(props)
        if p not in (None, False):
            try:
                parsed = json.loads(p)
                if not isinstance(parsed, dict):
                    bad_props += 1
                elif parsed.get("geo_precision"):
                    declared_coarse += 1
            except Exception:
                bad_props += 1        # defect #3: fixed-buffer truncated JSON

        d = dec(pub)
        if d and not ISO.match(d):
            bad_date += 1             # defect #1: RFC-822 stored, sorts as text

        if lat is not None and lon is not None:
            coords.append((round(float(lat), 6), round(float(lon), 6)))

    if untitled:
        v.append(f"{untitled}/{n} rows have no title")
    if bad_utf8:
        v.append(f"{bad_utf8}/{n} rows contain invalid UTF-8")
    if bad_props:
        v.append(f"{bad_props}/{n} rows have unparseable properties_json")
    if bad_date:
        v.append(f"{bad_date}/{n} rows have a non-ISO published_at")

    # Fabricated geometry: the audit's single most damaging class. >1 geo row
    # all sharing one exact point is the Tokyo-Station signature (1,112 rows on
    # one pin). 2 rows could coincide legitimately; 3+ identical cannot.
    #
    # EXCEPT when the source SAYS the point is approximate. `classifieds`
    # legitimately pins every listing in a scrape region to that region's
    # centroid and stamps geo_precision on every row — coarse-but-declared is
    # the behaviour the audit asked for, not the behaviour it banned. Treating
    # it as a failure would train everyone to ignore this check, so it is
    # reported as WARN and does not fail the source. Silent stacking, with no
    # geo_precision, stays a hard failure.
    if len(coords) >= 3 and len(set(coords)) == 1:
        if declared_coarse == n:
            v.append(f"WARN: all {len(coords)} geo rows share one point "
                     f"{coords[0]}, declared geo_precision — coarse but honest")
        else:
            v.append(f"all {len(coords)} geo rows share one exact point {coords[0]} "
                     f"— fabricated/fallback coordinate")
    for la, lo in coords:
        if not (-90 <= la <= 90) or not (-180 <= lo <= 180):
            v.append(f"coordinate out of range ({la},{lo})")
            break

    return n, v


def judge(sid, timeout, entity=None):
    rc, err, db, tmp = run_source(sid, timeout, entity)
    try:
        n, viol = check_rows(db, sid)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    tail = " | ".join(l.strip() for l in err.strip().splitlines()[-2:])[:240]

    hard = [x for x in viol if not x.startswith("WARN")]
    if rc != 0:
        verdict = "FAIL"
        hard.insert(0, f"run returned rc={rc}")
        viol.insert(0, f"run returned rc={rc}")
    elif hard:
        verdict = "FAIL"
    elif viol:
        verdict = "WARN"
    elif n == 0:
        # rc=0 with no rows is the honest-empty contract (R3), not a pass.
        verdict = "EMPTY"
    else:
        verdict = "PASS"
    return dict(id=sid, verdict=verdict, rc=rc, rows=n, entity=entity or "",
                violations="; ".join(viol), stderr=tail)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ids"); ap.add_argument("--ids-file"); ap.add_argument("--all", action="store_true")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--out", default=os.path.join(REPO, "tests", "contract", "verdicts.tsv"))
    ap.add_argument("--baseline")
    ap.add_argument("--entities-file",
                    help="TSV 'id<TAB>entity' — the pivot value to run each "
                         "entity-pivot service with. Without it, UPPER_SNAKE "
                         "ids fall back to PIVOT_DEFAULTS['keyword'].")
    a = ap.parse_args()

    entities = {}
    if a.entities_file and os.path.exists(a.entities_file):
        for line in open(a.entities_file, encoding="utf-8"):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) >= 2:
                # value may be a pivot KIND name or a literal entity
                entities[parts[0]] = PIVOT_DEFAULTS.get(parts[1], parts[1])

    if not os.path.exists(BIN):
        sys.exit(f"no binary at {BIN} — run make first")

    if a.all:
        out = subprocess.run([BIN, "--list-sources"], capture_output=True, text=True,
                             env=dict(os.environ, JO_DB=tempfile.mktemp(suffix=".db")))
        ids = [l.split()[0] for l in out.stdout.splitlines() if l.strip()]
    elif a.ids_file:
        ids = [l.strip() for l in open(a.ids_file) if l.strip() and not l.startswith("#")]
    elif a.ids:
        ids = [s.strip() for s in a.ids.split(",") if s.strip()]
    else:
        sys.exit("need --ids, --ids-file or --all")

    def ent_for(sid):
        if sid in entities:
            return entities[sid]
        # No mapping: give pivot services *something* of the right shape rather
        # than nothing, so an EMPTY verdict means "found no data for this
        # entity" and not "was never given an entity to look up".
        return PIVOT_DEFAULTS["keyword"] if is_pivot_id(sid) else None

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        for r in ex.map(lambda s: judge(s, a.timeout, ent_for(s)), ids):
            results.append(r)
            ent = f" [{r['entity']}]" if r["entity"] else ""
            print(f"{r['verdict']:6} {r['id']:<34} rows={r['rows']:<6}{ent} "
                  f"{r['violations'][:80]}", flush=True)

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    cols = ["id", "verdict", "rc", "rows", "entity", "violations", "stderr"]
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("\t".join(cols) + "\n")
        for r in sorted(results, key=lambda x: x["id"]):
            f.write("\t".join(str(r[c]).replace("\t", " ") for c in cols) + "\n")

    tally = {}
    for r in results:
        tally[r["verdict"]] = tally.get(r["verdict"], 0) + 1
    print("\n" + "  ".join(f"{k}={v}" for k, v in sorted(tally.items())))
    print(f"verdicts -> {a.out}")

    if a.baseline and os.path.exists(a.baseline):
        prev = {}
        with open(a.baseline, encoding="utf-8") as f:
            next(f, None)
            for line in f:
                p = line.rstrip("\n").split("\t")
                if len(p) >= 2:
                    prev[p[0]] = p[1]
        regressed = [r for r in results
                     if prev.get(r["id"]) == "PASS" and r["verdict"] != "PASS"]
        if regressed:
            print(f"\nREGRESSIONS vs baseline ({len(regressed)}):")
            for r in regressed:
                print(f"  {r['id']}: PASS -> {r['verdict']}  {r['violations'][:100]}")

    sys.exit(1 if tally.get("FAIL") else 0)


if __name__ == "__main__":
    main()
