#!/usr/bin/env python3
"""Verify a candidate manifest and promote the survivors to verified.

Batch 14 (docs/candidate-sources-batch14.tsv) was authored in an environment
with no outbound egress, so its rows carry verified=no. This tool closes that
gap in one pass, from a machine that does have egress:

  1. runs collectors/verify_feeds.py over every candidate URL;
  2. keeps only the rows that came back PASS — 2xx, parsed, >=1 record;
  3. rewrites those rows with the OBSERVED kind and item count, not the kind
     the candidate generator guessed. This matters: the generator predicts a
     shape from the API contract, and where the prediction is wrong the
     collector would parse nothing. The verifier's answer is ground truth.
  4. writes the survivors to a verified manifest and the failures to a
     rejects file, so the dead endpoints are visible as data rather than
     silently dropped.

The two output files are then the input to the normal path:

  python3 collectors/gen_verified_sources.py docs/verified-sources-batch14.tsv \\
      --outdir collectors/sources --reserved <ids> --prefix vsrc14

...and the csrc14_*.c files this batch shipped should be deleted in the same
commit, so a source is registered from exactly one manifest.

Usage:
  tools/promote_candidates.py docs/candidate-sources-batch14.tsv \\
      --verified docs/verified-sources-batch14.tsv \\
      --rejects  docs/rejected-sources-batch14.tsv
"""
import argparse
import csv
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
VERIFIER = os.path.join(NATIVE, "collectors", "verify_feeds.py")

# The verified manifest's column order, matching docs/verified-sources-manifest.tsv.
VERIFIED_COLUMNS = ["id", "url", "kind", "items", "name", "name_ja",
                    "collector", "category", "lang", "tags", "interval",
                    "description"]


def run_verifier(rows, workdir):
    """Feed id/url pairs to verify_feeds.py, return {url: result-dict}."""
    probe = os.path.join(workdir, "probe.tsv")
    with open(probe, "w", encoding="utf-8", newline="\n") as fh:
        for r in rows:
            fh.write(f"{r['id']}\t{r['url']}\n")

    proc = subprocess.run([sys.executable, VERIFIER, probe],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr, file=sys.stderr)
        raise SystemExit(f"verify_feeds.py exited {proc.returncode}")
    print(proc.stderr.strip(), file=sys.stderr)

    out = {}
    for rec in csv.DictReader(proc.stdout.splitlines(), delimiter="\t"):
        out[rec["url"]] = rec
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("candidates")
    ap.add_argument("--verified", required=True)
    ap.add_argument("--rejects", required=True)
    ap.add_argument("--workdir", default="/tmp")
    args = ap.parse_args()

    with open(args.candidates, encoding="utf-8", newline="") as fh:
        rows = list(csv.DictReader(fh, delimiter="\t"))
    print(f"{len(rows)} candidates", file=sys.stderr)

    results = run_verifier(rows, args.workdir)

    kept, rejected = [], []
    for r in rows:
        res = results.get(r["url"])
        if not res:
            r["_verdict"], r["_note"] = "NO_RESULT", "verifier returned no row"
            rejected.append(r)
            continue
        if res["verdict"] != "PASS":
            r["_verdict"], r["_note"] = res["verdict"], res["note"]
            rejected.append(r)
            continue
        # Ground truth wins over the generator's predicted shape.
        r["kind"] = res["kind"]
        r["items"] = res["items"]
        kept.append(r)

    with open(args.verified, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\t".join(VERIFIED_COLUMNS) + "\n")
        for r in kept:
            fh.write("\t".join(r[c].replace("\t", " ")
                               for c in VERIFIED_COLUMNS) + "\n")

    reject_cols = VERIFIED_COLUMNS + ["_verdict", "_note"]
    with open(args.rejects, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\t".join(reject_cols) + "\n")
        for r in rejected:
            fh.write("\t".join(str(r.get(c, "")).replace("\t", " ")
                               for c in reject_cols) + "\n")

    print(f"--- {len(kept)} PASS -> {args.verified}", file=sys.stderr)
    print(f"--- {len(rejected)} rejected -> {args.rejects}", file=sys.stderr)
    if kept:
        by_kind = {}
        for r in kept:
            by_kind[r["kind"]] = by_kind.get(r["kind"], 0) + 1
        for k, n in sorted(by_kind.items(), key=lambda kv: -kv[1]):
            print(f"    {k}: {n}", file=sys.stderr)


if __name__ == "__main__":
    main()
