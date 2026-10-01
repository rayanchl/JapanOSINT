#!/usr/bin/env python3
"""Partition every testable source into N disjoint auditor slices.

Two rules make this work:

1. Partition by DEFINING FILE, never by source id. One .c file often registers
   dozens of sources through X-macros, so slicing by id would hand the same
   file to several auditors and their edits would collide.

2. Slice off the INVENTORY, not off the sweep results. The bulk sweep is a
   triage aid that may still be running (or may have mis-guessed an entity);
   the auditor is responsible for every source in the slice whether or not a
   sweep row exists for it.

Writes slice_<i>.md — a self-contained brief per auditor.
"""
import json, os, collections, sys, re

HERE = os.path.dirname(os.path.abspath(__file__))

# This machine has 0 of ~86 credential vars populated in .env, so every keyed
# source honestly self-gates. That is a different thing from "broken" and must
# not be reported as one — refine EMPTY from what the collector actually logged.
GATED_RE = re.compile(r"gated|no [A-Z0-9_]*(KEY|TOKEN|SECRET|USER_AGENT)|"
                      r"not set|requires? (an )?api key|credentials?[ _]not|missing key", re.I)
WAF_RE = re.compile(r"cloudflare|WAF|\b403\b|\b429\b|captcha|blocked|rate.?limit", re.I)

MAPPISH = {"geospatial", "transport", "infrastructure", "environment",
           "satellite", "safety", "crime", "health", "tourism", "industry"}


def load(name):
    p = os.path.join(HERE, name)
    if not os.path.exists(p):
        return []
    out = []
    for line in open(p, encoding="utf-8"):
        line = line.strip()
        if line:
            try:
                out.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    return out


def refine(r):
    if r["verdict"] != "EMPTY":
        return r["verdict"]
    tail = r.get("stderr_tail") or ""
    if GATED_RE.search(tail):
        return "KEY_GATED"
    if WAF_RE.search(tail):
        return "WAF_BLOCKED"
    return "EMPTY"


def main():
    n_slices = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    inv = [r for r in load("inventory.jsonl")
           if not (r.get("collector") or "").startswith("_")]
    results = {r["id"]: r for r in
               load("results_scheduled.jsonl") + load("results_ondemand.jsonl")}
    probes = {}
    pk = os.path.join(HERE, "plan_ondemand_kinds.tsv")
    if os.path.exists(pk):
        for line in open(pk, encoding="utf-8"):
            p = line.rstrip("\n").split("\t")
            if len(p) == 3:
                probes[p[0]] = (p[1], p[2])

    byfile = collections.defaultdict(list)
    for r in inv:
        byfile[r.get("file") or "(unattributed)"].append(r)

    bins = [[] for _ in range(n_slices)]
    load_ = [0] * n_slices
    for f, recs in sorted(byfile.items(), key=lambda kv: -len(kv[1])):
        i = load_.index(min(load_))
        bins[i].append((f, recs))
        load_[i] += len(recs)

    for i, b in enumerate(bins, 1):
        path = os.path.join(HERE, "slice_%02d.md" % i)
        total = sum(len(r) for _, r in b)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("# Audit slice %d — %d sources across %d files\n\n" % (i, total, len(b)))
            fh.write("`verdict` is from the bulk sweep and is ADVISORY: a blank means the "
                     "sweep had not reached it, and an EMPTY may just mean the sweep guessed "
                     "the wrong pivot entity. Re-run anything you report on.\n")
            for f, recs in sorted(b):
                fh.write("\n## `native/collectors/sources/%s`  (%d sources)\n\n" % (f, len(recs)))
                fh.write("| id | cat | iv | pivot | sweep | rows | geo | ms | note |\n")
                fh.write("|---|---|---|---|---|---|---|---|---|\n")
                for rec in sorted(recs, key=lambda r: r["id"]):
                    sid = rec["id"]
                    r = results.get(sid)
                    kind, ent = probes.get(sid, ("", ""))
                    note = []
                    if rec["reads_entity"]:
                        note.append("reads ctx->entity")
                    if r:
                        if r.get("timed_out"):
                            note.append("TIMED OUT")
                        if r.get("db_error"):
                            note.append("db:%s" % str(r["db_error"])[:40])
                        if r["rows"] and not r["geo_rows"] and rec.get("category") in MAPPISH:
                            note.append("NO GEO despite map-ish category")
                        if r["record_types"]:
                            note.append("rt=" + ",".join(r["record_types"][:3]))
                    fh.write("| %s | %s | %s | %s | %s | %s | %s | %s | %s |\n" % (
                        sid, rec.get("category") or "-", rec.get("interval"),
                        ("%s=%s" % (kind, ent)) if kind else "-",
                        refine(r) if r else "(not swept)",
                        r["rows"] if r else "-", r["geo_rows"] if r else "-",
                        r["duration_ms"] if r else "-",
                        "; ".join(note)))
        print("slice_%02d.md: %4d sources, %3d files" % (i, total, len(b)))


if __name__ == "__main__":
    main()
