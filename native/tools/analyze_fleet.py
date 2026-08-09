#!/usr/bin/env python3
"""Turn the raw fleet-probe TSV into an actionable triage list.

The probe records what each source did against its real upstream. This groups
the failures so the answer to "which sources are actually broken" is separable
from "which sources share one throttled host" — the distinction that matters,
because the audit found ~24% of the fleet quarantining on a single upstream's
rate limiter rather than on 600 independent defects.
"""
import csv, collections, re, sys, os, json

RESULTS = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
    "~/jotest/fleet/results.tsv")
LOGDIR = os.path.join(os.path.dirname(RESULTS), "logs")
ALLTSV = os.path.join(os.path.dirname(RESULTS), "all.tsv")

rows = list(csv.DictReader(open(RESULTS, encoding="utf-8"), delimiter="\t"))
meta = {}
if os.path.exists(ALLTSV):
    for line in open(ALLTSV, encoding="utf-8"):
        p = line.rstrip("\n").split("\t")
        if len(p) >= 3:
            meta[p[0]] = {"collector": p[1], "interval": p[2]}

by_verdict = collections.Counter(r["verdict"] for r in rows)
total_rows = sum(int(r["records"]) for r in rows
                 if r["records"].isdigit())

# Pull the upstream host out of each failing source's log. A collector prints
# its own diagnostics; the host is the strongest grouping signal we have.
HOSTRE = re.compile(r"https?://([^/\s\"']+)")
fail_hosts = collections.Counter()
fail_detail = collections.defaultdict(list)
for r in rows:
    if r["verdict"] in ("OK", "EMPTY_OK"):
        continue
    log = os.path.join(LOGDIR, r["id"] + ".log")
    host = ""
    if os.path.exists(log):
        try:
            txt = open(log, encoding="utf-8", errors="replace").read()
            m = HOSTRE.search(txt)
            if m:
                host = m.group(1)
        except Exception:
            pass
    fail_hosts[host or "(unknown)"] += 1
    fail_detail[host or "(unknown)"].append((r["id"], r["verdict"]))

by_collector = collections.defaultdict(lambda: collections.Counter())
for r in rows:
    c = meta.get(r["id"], {}).get("collector", "?")
    by_collector[c][r["verdict"]] += 1

print("=== VERDICTS ===")
for v, n in by_verdict.most_common():
    print(f"{n:6d}  {v}  ({100.0*n/len(rows):.1f}%)")
print(f"\ntotal sources probed : {len(rows)}")
print(f"total rows collected : {total_rows:,}")
healthy = by_verdict['OK'] + by_verdict['EMPTY_OK']
print(f"healthy (OK+EMPTY_OK): {healthy} ({100.0*healthy/len(rows):.1f}%)")

print("\n=== FAILURES BY UPSTREAM HOST (top 30) ===")
print("A large count on one host is a throttle/outage, not N broken sources.")
for h, n in fail_hosts.most_common(30):
    print(f"{n:5d}  {h}")

print("\n=== FAILURES BY COLLECTOR ===")
for c, ctr in sorted(by_collector.items(),
                     key=lambda kv: -(sum(kv[1].values()) - kv[1]['OK'] - kv[1]['EMPTY_OK'])):
    bad = sum(ctr.values()) - ctr['OK'] - ctr['EMPTY_OK']
    if bad:
        print(f"{bad:5d} bad / {sum(ctr.values()):5d}  {c}")

# The actionable artefact: single-source failures on hosts that are otherwise
# fine. These are real per-source defects rather than collateral damage.
lonely = [ids for h, ids in fail_detail.items() if len(ids) <= 2
          and h != "(unknown)"]
flat = sorted(x for grp in lonely for x in grp)
print(f"\n=== ISOLATED FAILURES ({len(flat)}) — likely real per-source defects ===")
for sid, v in flat[:120]:
    print(f"  {v:12s} {sid}")

out = os.path.join(os.path.dirname(RESULTS), "triage.json")
json.dump({"verdicts": dict(by_verdict), "total_rows": total_rows,
           "fail_hosts": dict(fail_hosts.most_common()),
           "isolated": [s for s, _ in flat]},
          open(out, "w"), indent=1)
print(f"\nwrote {out}")
