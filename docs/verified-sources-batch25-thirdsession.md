# Batch 25 — the seven beats authored by a third session, verified before commit

`docs/candidate-sources-batch25.{jpacademic,jpbiz2,jpgeospatial,jpkaigo,jpkeizai,jpmedia2,jpmunireserve}.txt`
were authored by a Claude session that ended before running the emit
gates. They compiled against the merged engine and sat in the worktree
untracked. House rule 4 says a batch is not believed until it has been run
through the real binary, so session 6dd0b0d8 ran the two remaining gates on
2026-08-30 before committing anything:

* `audit_batch_emit.py --bin ./bin/japanosint --jobs 3 --timeout 400` over
  all 525 rows → `docs/verified-sources-batch25-thirdsession-emit.tsv`
* `audit_registry_emit.py --ids-file <the OK rows> --jobs 3 --timeout 400`
  (the rule-4b store read-back from a fresh DB) →
  `docs/verified-sources-batch25-thirdsession-4b.tsv`

## Result

| beat | rows | emit | 4b read-back | shipped |
| --- | --- | --- | --- | --- |
| `jpkeizai` | 142 | OK 142 | OK 142 | **142** |
| `jpgeospatial` | 88 | OK 88 | OK 88 | **88** |
| `jpmunireserve` | 75 | OK 75 | OK 75 | **75** |
| `jpbiz2` | 73 | OK 71 · EMPTY_UPSTREAM 2 | OK 71 | **71** — the two empty rows (`JPBIZ_NAGANO_CCI`, `JPBIZ_SHIZUOKA_CCI`) are in `docs/rejected-sources-batch25-jpbiz2.tsv` |
| `jpkaigo` | 35 | OK 35 | OK 35 | **35** |
| `jpmedia2` | 17 | OK 17 | OK 17 | **17** |
| `jpacademic` | 95 | OK 17 · **SLOW 78** | OK 17 | **held** — see below |

Over the 445 rows that passed both gates: **246,682 records emitted,
249,337 rows stored, 0 lost to uid collision** (the surplus is
`collector-shape-notice` records, which the run line reports after
`stored=`). Registry with the six beats installed: 16,366. `make` 0
warnings; `audit-sources` 0 findings, strict 0; `lint-sources` OK;
`hptest` and `unit` pass.

## `jpacademic` is held, not rejected

78 of its 95 rows are university OAI-PMH repositories (`JPREPO_*_OAI`)
that had not finished their first page inside the 400 s cap — the tool's
`SLOW` verdict, which is *unmeasured*, not failed (the same verdict that
made `RIPENCC_DELEGATED_STATS` look dead in the 08-25 sweep when it was
merely long). They are being re-measured with `--timeout 1500`; the beat
will be committed with whatever that run proves and nothing more. The 17
rows that already pass are not split out, so the manifest stays whole.

Not committed either: `docs/review-sources-batch25.*.txt` (the raw
722-candidate research lists batch 28 was cut from — kept on disk as
scratch), `docs/breach-corpus.json` (an unexplained modification by the
ended session), `docs/candidate-sources-batch23.jpdeep.txt` (a manifest
with no generated table) and `docs/rejected-sources-batch23-jpgov.tsv` (a
reject ledger for a beat that never shipped).
