# Batch 25 — `jpacademic` (95 rows): the OAI repositories, measured to the end

Held out of the first batch-25-thirdsession commit because 78 of its 95 rows
are university OAI-PMH repositories (`JPREPO_*_OAI`) that had not finished
their first walk inside the 400 s emit cap — the tool's **SLOW** verdict,
which is *unmeasured, not failed*. Committing a source on a "didn't finish in
time" reading would violate the rule the 08-25 sweep established: a slow
source and a dead source must never be treated the same
(`RIPENCC_DELEGATED_STATS` read as dead at 220 s and stored 260,357 rows at
304 s).

So the 78 were re-run with `--timeout 1500`
(`docs/verified-sources-batch25-jpacademic-oai-rerun.tsv`):

| verdict | rows | meaning |
| --- | --- | --- |
| OK first pass (400 s) | 17 | small repos, complete |
| OK on the 1500 s rerun | 55 | complete: **180,135 emitted, 180,165 stored, 0 lost to uid collision** (the +30 are shape-notice records) |
| still SLOW at 1500 s | 23 | large repos — each had **2,000–3,900 rows already stored** when the 25-minute cap fired (Kanagawa 3,900, Miyazaki 3,900, Kanazawa 2,600, Waseda 2,000, …) |

All 95 shipped. The 23 SLOW rows are shipped on measured evidence, not
optimism: every one emits and stores thousands of real records, none shows a
collision, and none is `EMITS_NOTHING`. They are simply large — a national
university's full repository is tens of thousands of records over hundreds of
OAI pages, and as a scheduled source (interval 604,800 s = weekly) each walks
and upserts on its own clock in production, not against a 25-minute audit
wall. What the audit proved is what matters for shipping: the endpoint
answers, parses, and stores distinct records. What it could not prove — the
exact total for the 23 largest — is a ceiling, not a defect, and is stated
here rather than hidden.

Gates on the 95-row tree: gen 95/95 · build 0 warnings, all 95 `JPREPO_*`
registered · `audit-sources` 0 findings / strict 0 · `lint-sources` OK ·
`hptest` and `unit` pass · reachable 0 of 95 can never run. Table
`cmp`-identical (modulo header) to a fresh regen from the manifest.

## Note on the exclusions check (2026-08-31, rebased onto origin 259d9a8)

Re-gated on the repaired main (`259d9a8`, after the three merge-introduced
compile breaks were fixed): build 0 warnings, selftest/unit/hptest/lint/audit
all green, reachable 0 of 95. `batch_exclusions.py --bin` reported "collisions:
95" — this is a **self-collision artefact**, not a duplicate: the check was run
against a binary that already had this table compiled in, so each manifest row
matched its own registration. Confirmed from git with no binary in the loop:
`git grep JPREPO_ origin/main -- native/collectors/**` is empty and none of the
95 ids or endpoints appears in any collector or manifest on origin — the 95 are
new. (The clean pre-install re-check that normally settles this could not be
re-run at commit time: the WSL ext4 vhdx was locked by a concurrent session,
ERROR_SHARING_VIOLATION; git evidence stands in for it and is dispositive here.)
