# Repair round 8 — perpetual sweep-and-repair agent brief

You are one of two long-running repair agents (A or B). Your job: sweep every
registered source in YOUR shard through the real binary, and repair every one
that fetches-but-does-not-store, keys records onto each other, or is dead —
then re-measure, record, and continue until the shard is clean or you run out
of budget. Measured numbers only; never trust a transcript, only a re-sweep.

Read first: `C:\Users\rayan\sources\repos\OSINTsaas\CLAUDE.md` (rules 1, 2, 3, 4,
4b, 4d — especially `id_keys`: `,` chooses, `+` composes) and
`docs/source-repair-round7-2026-09-11.md` (the previous round: its verdict
conventions, how dead sources were recorded, what fixes looked like).

## Your workspace (all under WSL distro `Ubuntu-24.04`, under $HOME, never /tmp)

Drive WSL with `wsl.exe -d Ubuntu-24.04 -- bash -s <<'EOF' ... EOF` heredocs.
Shard files (already made): `~/repair8/shard<X>_ids.txt` (ids to sweep),
`~/repair8/shard<X>_files.txt` (the collector .c files you may EDIT).
Copies are also at `docs/batch30/shard<X>_*.txt` on Windows.

Build tree — make your own, once, then keep it in sync:

```
mkdir -p ~/jorepair<X>
rsync -a --exclude=.git --exclude=obj --exclude=bin --exclude=data --exclude=models \
  --exclude=native/llama --exclude=client/node_modules --exclude='*.gguf' --exclude='*.db' \
  /mnt/c/Users/rayan/sources/repos/OSINTsaas/ ~/jorepair<X>/
cd ~/jorepair<X>/native && make -j8 2>&1 | grep -E "warning:|error:" ; ls -la bin/japanosint
```
Never write `--delete` with a variable destination. Literal paths only.
Edits go to the REAL repo under `/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/collectors/...`
(that is the shared working tree the maintainer commits from), then re-rsync
(same command, no --delete) and rebuild before measuring.

## The loop

Work in chunks of ~250 ids so a fix is measured within the hour:

```
cd ~/jorepair<X>/native
sed -n '1,250p' ~/repair8/shard<X>_ids.txt > ~/repair8/chunk<X>.txt      # advance the range each chunk
python3 tools/audit_registry_emit.py --bin ./bin/japanosint --scheduled \
  --ids-file ~/repair8/chunk<X>.txt --jobs 5 --timeout 220 \
  --workdir ~/repair8/work<X> --out ~/repair8/sweep<X>.tsv --resume --quiet
awk -F'\t' 'NR>1 && $NF!="OK"' ~/repair8/sweep<X>.tsv | tail -80   # read the header first for column order
```
(`--scheduled` skips interval-0 entity pivots — running those with no entity
produces fake EMITS_NOTHING; pivots are checked separately with
`tools/probe_hp_batch.py --check-filter` when a manifest exists.)

For each non-OK verdict:

* **EMITS_NOTHING** — `python3 tools/diagnose_emit_keys.py --bin ./bin/japanosint <ID>`
  and `./bin/japanosint --run <ID>` with `JO_DB=~/repair8/scratch<X>.db`. Three causes:
  (a) wrong `title_keys`/`id_keys`/`array_path`/`page_param` → fix the row in its C
  file (keep the inline comment style of the file; state the measured before/after
  count in a comment on the row); (b) upstream moved → find the new endpoint (WebFetch)
  and re-point, verifying records; (c) upstream dead (persistent 404/410/NXDOMAIN
  across two tries an hour apart) → record it in the round-8 TSV as DEAD with the
  HTTP evidence and quarantine it the way round 7 did. Never delete a row silently.
* **COLLISION** (stored < emitted) — identity is wrong: compose it with `+`
  (`station+date`, `code+name`) so each record keys uniquely. Re-run and confirm
  stored == emitted.
* **SLOW** — raise `timeout_ms`, lower `page_size`, or leave it and note it; SLOW is
  unmeasured, not failed. Do not spend more than one attempt on it.
* **SINK_MISMATCH** / anything that looks like an ENGINE bug (many rows failing the
  same way across unrelated files) — do NOT patch the engine; write it up in your
  report with three example ids and move on. The maintainer fixes engine code.
* **NEEDS_ENTITY** — a pivot mislabelled as scheduled or vice versa; check
  `interval` and the URL for `{q}` tokens; fix the declaration.

After each chunk's fixes: rsync → rebuild → re-measure ONLY the ids you touched
(`--only ID1,ID2,...` without `--resume`, `--out ~/repair8/recheck<X>.tsv`).
A fix counts only when the recheck says OK.

## Recording (this is the deliverable)

Append to `C:\Users\rayan\sources\repos\OSINTsaas\docs\source-repair-round8-<X>.tsv`
one line per source you acted on: `id<TAB>file<TAB>verdict_before<TAB>emitted_before<TAB>stored_before<TAB>action<TAB>verdict_after<TAB>emitted_after<TAB>stored_after<TAB>evidence`.
Keep `docs/source-repair-round8-<X>.md` updated every chunk: counts swept / OK /
fixed / dead / engine-bug / remaining, and a short list of the fix patterns seen.

## Hard rules

* No sub-agents. No editing files outside your shard's file list — if a failing
  id lives in a file on the OTHER shard's list, append it to
  `~/repair8/handoff_to_<other>.txt` and move on.
* Never fabricate: no fixture data, no placeholder endpoints, no browser
  User-Agent impersonation to get past a bot wall (honest
  `JapanOSINT/1.0` self-identification is allowed).
* Do not commit. Do not run `git checkout/reset/clean/stash`.
* Do not touch `native/core/` or `native/lib/` (engine). Collector tables only.
* Keep the tree at 0 warnings and `make audit-sources` at 0 findings for files you edit
  (`python3 tests/audit_source_exhaustiveness.py --file <path> -v`).
* Keep going chunk after chunk. When your context is nearly spent, make sure the TSV
  and .md are current and the last line of the .md says exactly which id range you
  reached, so the next agent resumes from there.
