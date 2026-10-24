#!/usr/bin/env python3
"""Assemble docs/verified-sources-batch<N>.tsv and docs/rejected-sources-batch<N>.tsv
for a discovered-feed batch (43–48 and onward) from the per-beat artefacts.

    python3 tools/feed_batch_tsv.py --batch 44 --docs ../docs --date 2026-10-17 \
        ukgov:MANIFEST:PROBE_FULL:PROBE_RETRY:EMIT:BUILDER_REJECTS:DUPIDS:DEAD_IDS ...

One spec per beat, eight colon-separated paths:
  MANIFEST         the beat's pipe-delimited manifest (the rows that SHIPPED)
  PROBE_FULL       probe_hp_batch.py TSV of the first probe over every candidate
  PROBE_RETRY      probe_hp_batch.py TSV of the retry over the first probe's failures
  EMIT             audit_registry_emit.py TSV (emitted / stored / db_rows per id)
  BUILDER_REJECTS  build_feed_manifest.py's <out>.rejects.tsv
  DUPIDS           id<TAB>where lines from batch_exclusions.py for endpoint duplicates
  DEAD_IDS         ids that FETCH_FAILED on the sweep AND on a re-run (one per line)

The verified TSV carries BOTH readings for every shipped row — the probe's and
the engine's — because the two disagree in both directions (CLAUDE.md rule 4c):
a row the probe refused and the engine stores ships, with the refusal on
record; a row the probe passed and the engine cannot store does not.

The rejects TSV is the record of every candidate that did not ship, by beat and
by the stage that refused it. A probe refusal on an id that shipped (the engine
overruled it) is not a reject and is not written here."""
import sys, csv, argparse


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--batch", type=int, required=True)
    ap.add_argument("--docs", required=True, help="the docs/ directory")
    ap.add_argument("--date", required=True, help="the measurement date for the header")
    ap.add_argument("specs", nargs="+", help="beat:manifest:probe_full:probe_retry:emit:builder_rejects:dupids:dead_ids")
    a = ap.parse_args()
    V = open(f"{a.docs}/verified-sources-batch{a.batch}.tsv", "w", encoding="utf-8")
    V.write("id\tbeat\trecord_type\turl\tprobe_verdict\tprobe_items\temit_verdict\temitted\tstored\tdb_stored\n")
    R = open(f"{a.docs}/rejected-sources-batch{a.batch}.tsv", "w", encoding="utf-8")
    R.write(f"# Batch {a.batch} candidates REJECTED, by beat and by the stage that refused them, {a.date}.\n")
    R.write("# builder: the discovery fetch (DUP_URL = already registered, NOT_FEED = no RSS/RDF/Atom root, EMPTY_RESULTSET = a feed\n")
    R.write("#   with zero records, HTTP_000 = no connection). batch_exclusions: endpoint already read by a registered row.\n")
    R.write("# probe_hp_batch: the official gate, after one retry. engine: FETCH_FAILED on the emit sweep AND on a re-run.\n")
    R.write("beat\tstage\tcode_or_id\tname\turl\tverdict\tnote\n")
    nv = nr = 0
    for spec in a.specs:
        beat, manifest, pfull, pretry, emit, brej, dups, dead = spec.split(":")
        rows = {}
        for line in open(manifest, encoding="utf-8"):
            if line.startswith("#") or not line.strip():
                continue
            f = line.rstrip("\n").split("|")
            rows[f[0]] = f
        probe = {}
        for pf in (pfull, pretry):
            for r in csv.DictReader(open(pf, encoding="utf-8"), delimiter="\t"):
                if r["verdict"] == "PASS" or r["id"] not in probe:
                    probe[r["id"]] = r
        em = {r["id"]: r for r in csv.DictReader(open(emit, encoding="utf-8"), delimiter="\t")}
        for i, f in rows.items():
            p = probe.get(i, {}); e = em.get(i, {})
            V.write("\t".join([i, beat, f[4], f[9], p.get("verdict", ""), p.get("items", ""),
                               e.get("verdict", ""), e.get("emitted", ""), e.get("stored", ""),
                               e.get("db_rows", "")]) + "\n")
            nv += 1
        for r in csv.DictReader(open(brej, encoding="utf-8"), delimiter="\t"):
            v = "EMPTY_RESULTSET" if r["verdict"] == "OK" else r["verdict"]
            R.write(f"{beat}\tbuilder\t{r['key']}\t{r['name']}\t{r['url']}\t{v}\tkind={r['kind']} items={r['items']}\n")
            nr += 1
        for line in open(dups, encoding="utf-8"):
            if not line.strip():
                continue
            i, where = line.rstrip("\n").split("\t", 1)
            R.write(f"{beat}\tbatch_exclusions\t{i}\t\t\tDUP_ENDPOINT\t{where}\n")
            nr += 1
        for r in csv.DictReader(open(pretry, encoding="utf-8"), delimiter="\t"):
            if r["verdict"] != "PASS" and r["id"] not in rows:   # the engine overruled the probe: not a reject
                R.write(f"{beat}\tprobe_hp_batch\t{r['id']}\t\t{r['url']}\t{r['verdict']}\t{(r.get('note') or '')[:120]}\n")
                nr += 1
        for line in open(dead, encoding="utf-8"):
            i = line.strip()
            if i:
                R.write(f"{beat}\tengine\t{i}\t\t\tFETCH_FAILED\tstored nothing in the engine on the sweep and again on a re-run (refused, timed out, or served HTML to the engine's client)\n")
                nr += 1
    print("verified", nv, "rejected", nr, file=sys.stderr)


if __name__ == "__main__":
    main()
