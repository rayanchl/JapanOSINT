# Batch 28 — Japan: police, banks/finance, government agencies, courts, industry, deep archives, public databases, security-advisory monitoring

260 rows. Every one was found by real web research (`docs/review-sources-batch25.ALL.txt`,
722 candidates), then independently re-inspected via live WebFetch to determine
its actual response shape, converted to a manifest row, probed for real over
the network, checked for id/endpoint collisions against both the committed
registry (14,728 sources, via `batch_exclusions.py --bin`) and a concurrent
session's in-flight batch25 (1,668 rows, manual cross-check), generated to C,
built, and run through the real engine end to end
(`audit_batch_emit.py`) — not just probed.

| beat | rows | what it is |
| --- | --- | --- |
| `jppolice` | 42 | NPA national directories, prefectural wanted/crime-data pages, Coast Guard, fire agency |
| `jpbank` | 13 | FSA/BOJ/industry-association link-lists (most FSA/BOJ data turned out PDF/XLSX-only — see gap below) |
| `jpgov` | 41 | ministry open data — e-Gov CKAN, jGrants, PMDA recalls, JFTC enforcement, MOE wetlands |
| `jpcourt` | 41 | court auctions (BIT), bar/professional discipline, Official Gazette, insolvency |
| `jpindustry` | 74 | all 47 prefectural labor-bureau violation-disclosure feeds, chambers of commerce, tender aggregator |
| `jpforgotten` | 30 | still-live legacy archives — old stats yearbooks, university manuscript DBs, a CGI-era registry |
| `jpdb` | 1 | CiNii Research OpenSearch (the only one of 66 database candidates with a real, accessible API) |
| `jpdeepweb` | 18 | JPCERT/CC, JVN/JVNDB, ISACs, security-vendor advisory feeds |

## Result of the real (not just probed) test

`audit_batch_emit.py --bin ./bin/japanosint`, run against a build compiled
from a peer session's stable, all-green snapshot (0 warnings, charset/XLSX
engine support included):

```
running 260 rows through the engine, timeout=90s
EMPTY_UPSTREAM=1  OK=259
records emitted across the run: 67,799
```

The one `EMPTY_UPSTREAM` (`JO28_CINII_RESEARCH_OPENSEARCH`) is confirmed
working in isolated retest (200 emitted, 202 stored) — it gets rate-limited
under the audit harness's burst load across 260 rows fired close together,
not a defect in the row.

**Update after a peer session's html-mode fix landed** (descendant/alt-text
fallback when an anchor's direct text is `<3` chars): `JO28_JFSA_WHITE_PAPER`
was re-tested and now emits cleanly (8/8 stored) — moved back from rejected
into `jpbank`. `JO28_TDB_BANKRUPTCY_AGG` (same original defect class) was
re-tested twice after the fix and still emits 0 in 66ms, too fast to be a
real fetch — left rejected rather than claimed as fixed on an unclean
signal; tagged `ANCHOR_NESTED_TEXT` for the peer session to re-check.

`make`, `make selftest`, `make unit`, `make hptest`, `make lint-sources`,
`make audit-sources` all pass, 0 compiler warnings, 0 strict-set findings.

## Three real defects found and fixed during testing (not engine-side)

1. **XML `array_path` is a bare tag name, not a dotted path.** Two rows
   declared `array_path=rss.channel.item` (copying JSON's dotted-envelope
   convention) and silently matched nothing, because `hp_run_xml` searches
   for the literal string `<rss.channel.item>`. Fixed to `array_path=item`.
2. **A hardcoded first-page query param collided with the engine's own
   pagination.** `JO28_CINII_RESEARCH_OPENSEARCH`'s URL template included
   `&start=1` literally; the engine's offset-pagination *appends*
   `&start=21`, `&start=41`, … rather than replacing an existing key, so
   every page after the first carried two `start=` parameters and the
   upstream used the first one — 179 of 200 emitted records collapsed onto
   already-written uids every run. Fixed by removing the literal `start=1`
   from the template, matching the `NDL_OPENSEARCH_*` convention already in
   the tree.
3. **Card-layout sites with all link text nested past a leading `<img>`.**
   Two rows (JFSA white paper, TDB bankruptcy flash reports) have real,
   correct `href_must` matches, but every anchor's only direct child is an
   `<img>` — the visible title text sits inside nested `<div>/<h3>/<p>`
   elements the html-mode anchor extractor (`text_len<3` direct-text check)
   can't see. Confirmed via live fetch, not fixable manifest-side; both
   rejected.

## A capability gap this batch ran into repeatedly

The engine has no PDF/XLSX parsing mode. FSA licensee registries, BOJ
statistics releases, and most industry-association "statistics" pages
publish only PDF or XLSX — confirmed live (e.g. FSA's `kasi.xlsx`, real and
correctly shaped) but unreachable by any of `json`/`csv`/`xml`/`html`
(anchor-only) modes. `jpbank` and `jpdb` took the brunt of this: 12/120 and
1/66 accepted respectively. XLSX support was in active development in a
concurrent session as of this writing.

## What was rejected, and why

Rejects are kept as data in `docs/rejected-sources-batch28.<beat>.tsv`
(514 rows across all beats), each with a specific reason. Dominant causes:

- **PDF/XLSX-only, no parseable mode** (the gap above) — the largest single
  category, concentrated in `jpbank`/`jpdb`.
- **JS-rendered SPA with no static/discoverable server API** — MLIT's 34
  `reinfolib` real-estate APIs are real and documented but require a
  mandatory area×year/tile combination this manifest format can't enumerate
  (a genuine future-batch candidate, same shape as MOJ's 47-prefecture
  land-price tables); courts.go.jp precedent search; several professional
  registries.
- **Bot-walled even with a real browser User-Agent** — all of jftc.go.jp
  (6 rows) and one Aichi labor page (redirect-loop WAF challenge).
- **Claimed structure not present on live re-fetch** — a handful of rows
  where the authoring pass's description didn't match what a second,
  independent fetch actually found (e.g. AEHA's case-list page: 0 PDF links
  on live fetch despite the original claim).
- **Duplicate of the existing tree or of a concurrent session's batch25** —
  21 rows, caught before any C was generated.
- **Genuinely empty at probe time** — two single-category-per-year crime
  CSVs (Tokushima, Kochi) that are header-only this year (0 incidents), an
  honest empty rather than a broken source.

## Coordination note

This batch shares the tree with several concurrent sessions running the
same kind of work. `docs/candidate-sources-batch25.*.txt` (a different,
larger effort, ~1,668 rows, `JP25_` ids) was cross-checked and is
independent of this batch. `native/tools/lint_baseline.json`'s
`dup-endpoint` count was bumped 484→506 via the tool's own
`--write-baseline`, verified as legitimate same-host (not same-endpoint)
growth before doing so.
