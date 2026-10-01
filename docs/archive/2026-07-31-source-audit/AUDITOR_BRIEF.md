# Auditor brief — OSINTsaas source & collector audit

You are one of ten auditors. You own one slice of the 2,189 registered data
sources. Your job: **prove, per source, whether it returns real data** — and
fix it when it does not.

---

## 1. The system in one page

`native/source.h` is the whole data-acquisition ABI. Every data source is a
`source_def` with a `run(const source_ctx*, intel_sink*)`. There is no
distinction between a scheduled map feed and an on-demand OSINT service — only
`update_interval_sec` (>0 = scheduled, 0 = on-demand) and whether `run()` reads
`ctx->entity` (the OSINT pivot value; NULL on a scheduled run).

`run()` emits `intel_item`s into `sink->emit()`. That single chokepoint
(`core/intel.c`) upserts into `intel_items`, mirrors into FTS, and fires entity
and alert hooks. `emit()` returns 1 for a new row, 0 for an update, <0 on error.

The `intel_item` fields that matter for this audit:

| field | why it matters |
|---|---|
| `title` | the row is unreadable in every UI without it |
| `body` / `summary` | the actual payload |
| `link` | provenance — lets a user verify the claim upstream |
| `published_at` | timeline placement |
| `has_geo` + `lat`/`lon` | **map pin**. No lat/lon → the source can never pin |
| `geometry_geojson` | polygons/lines (areas, routes) |
| `record_type` | what kind of thing this row is |
| `properties_json` | the structured payload; `"{}"` if unset |
| `tags_json` | `"[]"` if unset |
| `remote_key` / `uid` | dedupe identity across re-runs |

## 2. Your environment

Everything runs in **WSL Ubuntu**, from `/mnt/c/Users/rayan/sources/repos/OSINTsaas/native`.
Invoke it from Windows as:

    wsl.exe -- bash -c "cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native && <cmd>"

Use the **PowerShell tool**, not the Bash tool, for `wsl.exe` — Git Bash mangles
`/mnt/...` paths into `C:/Program Files/Git/mnt/...`. Avoid multi-line python
`-c` through PowerShell (it reparses it); put anything non-trivial in a script
file and run the file.

**Your build slot is `<SLOT>`** (given in your task). Never run bare `make` and
never touch `obj/` or `bin/japanosint` — a bulk sweep is running against them,
and relinking underneath it kills it with `Text file busy`.

    # build YOUR binary (first call seeds obj-<SLOT>/ from obj/, ~20s after that)
    wsl.exe -- bash /mnt/c/.../native/tests/audit/agent_build.sh <SLOT>

    # run ONE source and dump everything it emitted
    ... && python3 tests/audit/verify_one.py --slot <SLOT> <SOURCE_ID> [entity]
    ... && python3 tests/audit/verify_one.py --slot <SLOT> --stderr --rows 5 <ID> [entity]

`verify_one.py` uses a throwaway DB per invocation, so runs never interfere.

## 3. Ground truth about this machine

- **No API credentials are configured.** `.env` has ~86 credential variables and
  all of them are empty. A source that logs `gated (no FOO_API_KEY)` and emits
  nothing is behaving **correctly** — classify it `KEY_GATED`, do not "fix" it,
  and never invent a fallback that fabricates data.
- Network is live. Upstream 403/429/Cloudflare responses are real and are a
  finding (`WAF_BLOCKED`), but re-test once before concluding it.
- `DB_ERROR: … Could not decode to UTF-8` is **not** a harness bug. It means the
  collector persisted bytes that are not valid UTF-8 into a TEXT column —
  typically a Shift_JIS / Latin-1 upstream body passed through without
  transcoding. That is a real finding: it corrupts search and the API response.
- The bulk-sweep `verdict` in your slice file is **advisory**. A blank means the
  sweep had not reached it; an `EMPTY` on an on-demand source usually means the
  sweep guessed the wrong pivot entity. **Re-run anything you report on.**

## 4. What to determine for each source

1. **Does it return real data?** Run it. Read the emitted rows. The data must
   come from the fetch — not from a hardcoded table dressed up as a result.
2. **Is the data exhaustive?** Compare what the upstream response contains
   against what `run()` actually carries into the `intel_item`. Dropped fields
   that a user would want (coordinates, dates, identifiers, status, links) are
   findings. Fetching 500 records and emitting 20 is a finding.
3. **Should it pin, and does it?** If the thing is a physical place (station,
   camera, shelter, tower, plant, incident) it must set `has_geo`+`lat`/`lon`,
   or `geometry_geojson` for an area/route. Geo-less physical sources are a
   headline finding.
4. **Is the structure unified?** Within your slice, `record_type`,
   `properties_json` keys, and `tags_json` should be consistent for the same
   kind of thing. Note divergence; converge it where the fix is local and safe.
5. **Bugs.** Leaks, unchecked NULLs, buffer sizes, wrong error handling,
   `rc=-1` returned for an honest empty (that trips the anomaly detector and
   quarantines the source — an empty upstream is `return 0`, not `-1`).

## 5. Rules

- **Only edit files listed in your slice.** Files under `core/` and `lib/` are
  shared and READ-ONLY for you: if the bug is there, report it, don't patch it.
  If two of your sources need the same `lib/` change, report it.
- **Never fabricate data.** No hardcoded values standing in for a fetch, no
  invented coordinates, no placeholder records. An honest empty beats a
  fake row. If a source can only return names/labels rather than measurements,
  say so — that is exactly the "shows names, not data" class we are hunting.
- **Verify every fix by re-running the source** and pasting the real output.
- Do not `git commit`, `git checkout`, or `git stash`. Leave changes in the tree.
- If a source is genuinely dead upstream (domain gone, API retired), say so with
  the evidence; do not paper over it.

## 6. Deliverable

Write `native/tests/audit/report_<SLOT>.md` with:

```
# Slice <SLOT> — <N> sources

## Summary
<counts by verdict: DATA / KEY_GATED / WAF_BLOCKED / EMPTY / RC_ERROR / TIMEOUT / CRASH>
<the 5 most important findings, most severe first>

## Fixes applied
| file | source(s) | bug | fix | re-test result |

## Findings not fixed (with reason)
| source | issue | why not fixed |

## Per-source table
| id | verdict | rows | geo | data quality | notes |
```

`data quality` is your judgement in a few words: `real+complete`,
`real+thin (missing X,Y)`, `labels-only`, `gated`, `dead upstream`, …

Then reply with a compact summary: counts, the fixes you made, and the findings
that need my attention. Your final message is the only thing that reaches the
orchestrator — put the conclusions in it, not just a pointer to the file.
