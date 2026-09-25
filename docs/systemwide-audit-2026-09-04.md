# JapanOSINT — System-Wide Audit: Full Source-Registry Sweep + Codebase Bug Pass

*Scope: two tracks run together. (1) A full build + CI gate run plus six parallel
manual code audits — `native/core`, `native/lib`, `native/tools`, a sampled pass
over `native/collectors/sources/*.c`, `client/`, and `ios/`. (2) The first-ever
emit/store proof-of-life sweep of **all 16,366 registered sources** via
`tools/audit_registry_emit.py --all`, cross-checked at multiple parallelism
levels because the first pass at high concurrency produced false failures.*

**40 files fixed** across the six audit tracks (listed in §1). **The registry
sweep flagged 882 sources whose records collapse onto shared uids (205,617
records in one pass) and 888 sources that fetch but store nothing** (§2). The
888 are real defects. The 882 is an UPPER BOUND: it mixes genuine data loss
(a dimension declared as the record id) with correct deduplication of upstreams
that repeat identical records — the sweep cannot tell them apart, and the first
"worst offender" investigated turned out to be the latter (see §2). Both sets
are pre-existing, not introduced by this pass, and are measured at
full-registry scale for the first time.

---

## 0. Method note: don't trust an audit tool's own failure verdicts blindly

The registry sweep's first pass (`--all --jobs 24 --timeout 90`) reported 2,672
`EMITS_NOTHING`, 2,439 `NO_RUN_LINE`, 624 `COLLISION`, 304 `SLOW`. Spot-checking
individual "failures" in isolation (no concurrent load) showed most `NO_RUN_LINE`
and a meaningful share of `EMITS_NOTHING` were **artifacts of running 24 copies of
a process that reseeds all 16,366 source rows into a fresh DB on every single
invocation** — not real per-source bugs. Every non-OK verdict was therefore
rechecked at `--jobs 6` (and a final small batch at `--jobs 12`, once the slow
JPREPO academic-repository cluster — which is *supposed* to run long, see the
2026-09-03 commit — was excluded from the recheck queue). The numbers in §2 are
the post-recheck, trustworthy numbers. The lesson generalizes: this sweep tool's
own concurrency setting is itself a variable that has to be controlled for
before a verdict can be reported as fact — exactly the discipline CLAUDE.md
already demands of every collector.

Two bugs were found and fixed in the audit tool itself while doing this
(`native/tools/audit_registry_emit.py`):
- Line 179's `subprocess.run(..., text=True, ...)` decoded child-process output
  as strict UTF-8 with no error handling, unlike the `TimeoutExpired` branch two
  lines below it, which already used `errors="replace"`. One source
  (`aws-ip-ranges`, CIDR data) emitted a non-UTF-8 byte on stderr and crashed
  the **entire 16,366-source sweep** at the finish line via an uncaught
  `UnicodeDecodeError`. Fixed to match the timeout branch.
- `new = not (a.resume and os.path.exists(a.out))` meant any run **without**
  `--resume` silently truncated an existing `--out` file back to just its
  header, even if that file already held thousands of real results. This
  destroyed the first sweep's 16,364-row result file when a 2-source follow-up
  run was launched without `--resume`. Fixed to refuse to overwrite an existing
  non-resumed `--out` file rather than silently truncating it. (The lost data
  was reconstructed from the sweep's captured console log, which still had all
  16,366 per-source result lines.)
- The `COLLISION` verdict's note read "the row's identity is wrong" — a cause
  the tool cannot observe (see §2: the first worst-offender checked was
  legitimate dedupe). Reworded to state both possible causes and the one
  check that distinguishes them, so a reader isn't handed a false diagnosis.

---

## 1. Code fixes applied (40 files, all verified: full build 0 warnings, all 7
`make` CI gates re-pass after every change)

### `native/core` (19 bugs — the highest-severity track)
- **Timing side-channel**: the unauthenticated admin break-glass TOTP check
  used `strcmp` instead of constant-time comparison (`keysapi.c`) — now
  `CRYPTO_memcmp`.
- Derived AES key not `OPENSSL_cleanse`'d after use (`keysapi.c`).
- Data race on a lazily-initialized base64 table read/written unsynchronized
  from every worker thread (`keysapi.c`).
- Unsynchronized prompt/schema caches hit from every worker thread
  (`prompts.c`) — added a mutex.
- ~17 unchecked-allocation crashes on attacker-reachable paths, most notably an
  unchecked malloc decoding the JWT payload on **every request's** Authorization
  header before auth succeeds — a trivial NULL-deref DoS of the whole server
  (`auth.c`). 14 similar sites in `httpd.c`'s request-body handling (up to
  2.5MiB, attacker-influenced size). Also `intel.c`, `entityapi.c`,
  `station_clusterer.c`, `tenantapi.c`, `searchapi.c`.
- Memory leak: `content_change.c` freed an intel_sink's ctx directly instead of
  calling `intel_sink_free()`, leaking a growing hash table on every
  content-change event.
- Integer underflow: `osint_dispatch.c`'s `osint_canon` computed `n-1` on
  `n==0`, wrapping to `SIZE_MAX` (latent — no current caller passes 0, but it's
  a public API).
- **Flagged, not fixed** (judgment calls): ~10 unchecked allocations in the
  transit-geometry line-simplifier; a repo-wide idiom where a failed
  `sqlite3_prepare_v2()` silently returns `{"data":[]}` 200 instead of an error.

### `native/lib` (3 bugs, + verification pass)
- `htmlparse.c`: the tree's one `<a href>` scanner matched the *opening* tag
  case-insensitively (deliberate — older Japanese gov pages use `<A HREF>`) but
  the *closing* tag case-sensitively. Any page closing anchors as `</A>`
  produced **zero** extracted links. Fixed to `strcasestr`.
- `feedlib.c`: `feed_get_json`/`feed_get_json_h` — used by ~390 files — parsed
  raw HTTP bodies with none of the Shift_JIS transcode gate every sibling
  reader has. A `.jp` host serving JSON as Shift_JIS with no charset header
  (the exact scenario CLAUDE.md documents for customs.go.jp/soumu.go.jp) either
  fails to parse or parses with mojibake. Fixed to match the fail-closed
  transcode used elsewhere.
- `bigfile.c`/`.h`: the skipped-over-long-line tally existed internally but had
  no accessor, so `core/breach_index.c` (its only caller) could never surface
  it. Added `bigfile_skipped()`/`bigfile_lines()`. **Follow-up needed**: wiring
  an actual `collector-truncation-notice` into `breach_index.c` touches
  `core/`, out of this track's scope.
- Verified the triple collision-guard (`hp_collision_map`/`collision_map`/
  `gj_collision_map` in hpengine.c/jsonlist.c/geojson.c) is intact and correct
  in all three places.
- **Flagged, not fixed**: `rss_atom.c`'s uid fallback (`sha1(title|pubDate)`)
  has no collision guard — it's a streaming parser with no buffered pre-pass,
  so two items sharing title+pubDate but differing content would collide
  undetected.

### `native/tools` (7 bugs — see §0 for two more found live during the sweep)
- `probe_hp_batch.py`: reimplemented manifest opt-parsing naively instead of
  using the shared `manifest.opt()`, reintroducing the `\;`-escape bug the
  shared parser exists to prevent. Also: `--check-filter`'s impossible-entity
  regex was missing 3 of 6 entity-token forms (`{qc}`/`{qu}`/`{qn}`), so
  filter-honoring checks silently skipped for rows using them (confirmed live
  on a batch-18 row).
- Two reachability/emit auditors missing the `{Q}` raw-POST entity-token form
  the C engine itself honors (dormant today, but a real divergence).
- Two scripts' hand-rolled line readers didn't strip `\r`, risking a stray
  `\r` baked into manifest data by `--fix` rewrites on a CRLF checkout.
- Two `audit-harness/*.sh` scripts pointed at a path moved in a past commit.

### Collector sample (`native/collectors/sources/*.c`, 40 files sampled, 14
fixed across 13 files)
- `people_research.c`: a Google Places quota/denial degraded to a clean
  `emitted 0`, indistinguishable from a genuine no-match. Now surfaces the
  real status.
- `note_com_profiles.c`: `%g` on a large numeric user id mangled it into
  scientific notation, corrupting the emitted profile link. Fixed to `%.0f`.
- `eni_epa_ghgrp.c`, `corp_identifiers.c`, `trn_fra_grade_crossing_inventory.c`:
  hardcoded row windows / unpaginated fetches were silently discarding the
  majority of each table (one FRA case captured under 1% of a ~200k-row
  inventory). Rewritten as page walks with truncation notices.
- `jma_forecast_area.c`: two `exhaustive-ok` markers that didn't hold up —
  `areas[0]` dropped sibling sub-regions, `weathers[0]`/`winds[0]` dropped the
  already-fetched "tomorrow" forecast. Rewritten to emit both.
- `sanc_uk_sanctions_list.c`: per-record sub-array caps (aliases, measures,
  addresses, etc.) were implausibly low for real FCDO designations; raised and
  named as constants.
- `tor_exit_nodes.c`, `hudson_rock_jp.c`: missing numeric fields were coerced
  to `0`, indistinguishable from a genuinely-measured zero. Now emit `null`.
- `trn_transport_opendata_ch_locations.c`, `cert_first_teams.c`,
  `mar_emodnet_depth_sample.c`, `whale_monitor.c`: various fetch-failure ×
  honest-empty conflations and undisclosed caps, fixed with explicit error
  returns and truncation notices.
- **10 more issues reported, not fixed** (design-scope judgment calls) — see
  the collector-sample agent's full findings for `geoeo_usgs_water_levels.c`,
  `cyi_ioda_outage_alerts.c`, `av_awc_taf.c`, `trn_bts_border_crossing_volumes.c`,
  `corp_identifiers.c`'s keyless-pivot convention, and `whale_monitor.c`'s
  fallthrough-to-unrelated-data shape.

### `client/` (1 bug class, 4 sites)
- Unsanitized `href` on externally-sourced data (XSS via `javascript:` URI):
  scraped camera-stream URLs, tweet/toot links, and ingested entity-mention
  links were rendered as clickable `<a>` tags with no scheme check, unlike the
  file's own `isUrl()` gate used elsewhere. Added a shared `isSafeUrl()`
  (`client/src/utils/safeUrl.js`) and applied it at all 4 sites
  (`MapPopup.jsx` ×2, `CameraDiscoveryThread.jsx`, `EntityProfile.jsx`). Two
  lower-risk, operator-controlled URL sites (`SourcesPanel.jsx`,
  `FollowPanel.jsx`) flagged but not touched.
- Everything else checked clean: no `dangerouslySetInnerHTML`, no mock-data
  fallbacks on failed fetches, every list truncation already paired with a
  "showing X of Y" indicator, no hardcoded secrets. This surface had already
  been through the 2026-08-16 audit's fixes (confirmed the prior CRITICAL
  finding — the transit-vehicle simulator rendering invented positions
  indistinguishably from real GTFS — is fixed: features now carry an explicit
  `synthetic`/`schedule_backed` flag).

### `ios/` (0 new bugs)
- Built and validated static checks for the exact defect class in the two most
  recent commits (duplicate struct fields, duplicate dictionary keys, duplicate
  switch/enum cases, duplicate cross-extension members, duplicate top-level
  types) against all 148 files. Zero findings beyond the two already fixed.
  Force-unwraps, retain cycles, and mock-data gating all checked clean.

---

## 2. Full source-registry sweep: does every registered source actually work?

**Method**: `tools/audit_registry_emit.py --all` runs every registered source
against a fresh copy of a warm template DB and reads back both the run's
self-reported `emitted`/`stored` counts and the DB's own row count — two
independent readings. Non-OK verdicts were rechecked at lower concurrency per
§0 before being counted here.

### Final tally (16,366 sources, cross-checked)

| Verdict | Count | % | Meaning |
|---|--:|--:|---|
| **OK** | 12,514 | 76.5% | Fetched, emitted, and stored correctly |
| **PIVOT_UNTESTABLE** | 1,818 | 11.1% | `interval=0` entity-pivot sources — cannot be tested without a real query entity; **not a defect**, a measurement limit of this sweep |
| **EMITS_NOTHING** | 888 | 5.4% | Fetch/parse ran, **zero records reached storage** |
| **COLLISION** | 882 | 5.4% | Records collapsed onto a shared uid. **Upper bound on real loss** — either a dimension wrongly declared as the id (real loss) or the upstream repeating identical records (correct dedupe); needs a live fetch per row to tell which |
| **SLOW** | 249 | 1.5% | Hit the recheck timeout (150-240s); confirmed legitimate on spot-check (large single-page bulk registries like `ARIN_DELEGATED_STATS`, `RIPENCC_DELEGATED_STATS`, and the batch-25 JPREPO academic-repository cluster which is documented as intentionally "measured to the end") |
| **NEEDS_ENTITY** | 15 | 0.1% | Explicitly self-reported as needing an entity |

### COLLISION: 205,617 records lost to uid collisions across 882 sources

Full list with per-source loss counts: `docs/source-audit-2026-09-04-collisions.tsv`.
Severity split: **102 sources lose ≥50%** of their records, 108 lose 10-50%,
672 lose under 10%. This is exactly the "rule 4b" defect CLAUDE.md documents —
a row's `id_keys`/default-key declaration is not actually unique per record —
measured for the first time across the whole registry rather than one batch.

Two root causes confirmed live for the top offenders:

- **`IANA_LANGUAGE_SUBTAGS`** (49,312 emitted → 19,461 stored, 60.5% lost):
  declared `.mode = HP_CSV`, but the IANA registry file is not CSV at all — it's
  an RFC 5646 `%%`-delimited record format (`Type: language\nSubtag: aa\n...`).
  Confirmed by fetching the live file. Needs a dedicated parser mode, not a
  `csv_delim` tweak.
- **`gr-diavgeia-positions`** (25,159 emitted → 233 stored, 99.1% "lost") —
  **NOT a bug, and the case that exposed a flaw in the sweep's own verdict.**
  Fetching the live feed and counting: 25,165 records, **233 distinct `uid`
  values**, 231 distinct labels — the same ~233 positions repeated ~100× each
  with identical content. The engine storing 233 is correct deduplication;
  `uid` is already in `jsonlist.c`'s id-precedence list and was being used.
  Storing 25,165 would have been the defect. Verified end-to-end by running
  it through the new keyed path (below): still 233.

**The sweep's `COLLISION` verdict conflates two opposite things** and its
original text ("the row's identity is wrong") asserted a cause it cannot
see. `emitted > stored` means records collapsed onto a shared uid, which is
EITHER (A) a dimension declared as the record id — real loss, e.g.
`CO_LOBBYIST_CLIENTS` keying clients on `primarylobbyistid` — OR (B) the
upstream genuinely repeating identical records — correct dedupe. Only a live
fetch comparing distinct-id count to record count distinguishes them, so the
882 figure is an UPPER BOUND on real bugs, not a count of them. The tool's
verdict text now says so (`audit_registry_emit.py`), and the follow-up fix
pass triages every COLLISION row into (A)/(B) before touching it.

To make (A) fixable in the ~390 generated `VJSON` sources — which had no way
to override the record key at all — a new `jsonlist_emit_paged_keyed()` and
`VJSON_KEYED(..., IDFIELD)` macro were added (`lib/jsonlist.c/.h`,
`_verified_macros.inc`). It reuses the already-shared `pw_walk()` engine
with a callback that relabels the confirmed unique field onto `"id"` — a
relabel of a value the upstream sent, never an invented one — then hands off
to the same `jsonlist_emit_ex()` every other path uses. Additive only; no
existing caller changed. Builds clean, all 7 CI gates pass.

IANA was not fixed in that pass — it needed a dedicated `%%`-record parser
mode, real engine work rather than a per-row edit. **It has since been fixed;
see "The last big one" below.**

### EMITS_NOTHING: 888 sources fetch real data and store none

Full list: `docs/source-audit-2026-09-04-emits-nothing.txt`. This is the
`DATAPLANE_TELNET`-class defect CLAUDE.md describes (fetches real records,
stores zero, exit code 0, run looks successful) — measured registry-wide for
the first time. Not diagnosed per-row in this pass; `diagnose_emit_keys.py`
(built for exactly this) is the documented next step, run in batches against
this id list.

### Recommended follow-up (not attempted here — this is real per-batch
diagnosis work, the kind CLAUDE.md documents taking dedicated sessions):

```sh
python3 native/tools/diagnose_emit_keys.py --emit-tsv docs/source-audit-2026-09-04-emits-nothing.txt ...
```
Triage `docs/source-audit-2026-09-04-collisions.tsv` starting from the top (by
`lost` column) — the top ~15 rows alone account for over 100,000 of the
205,617 records lost.

---

## 3. The fix pass (2026-09-04 → 05): what happened, and what to know before committing

The 1,770 defective ids (882 COLLISION + 888 EMITS_NOTHING) span 422 files.
They were partitioned into 10 balanced, file-disjoint groups and handed to 10
parallel agents, each briefed to triage every COLLISION into case (A)/(B) by
live fetch before touching it, and to use `VJSON_KEYED` / `.id_keys` /
hand-written `run()` as the file type required.

**All 10 were killed mid-work by a session rate limit** (several had forked
sub-agents of their own, which is what exhausted the limit so fast — the next
launch forbids sub-agent spawning). Because they edit the working tree
directly, this left the repo in an unknown state. It was recovered by
measurement, not trust:

- Every modified `native/` file was synced into the clean WSL build clone and
  rebuilt. **Exactly one half-applied edit surfaced**: in
  `collectors/feed/generated/vsrc_environment_3.c` an agent had replaced the
  `VJSON(geo_tidesandcurrents_currents, …)` macro with a hand-written
  composite-key `run()` and was killed before deleting the original block,
  leaving the source defined twice (`redefinition of
  'run_geo_tidesandcurrents_currents'`). The stale macro block was removed;
  the hand-written version was kept after reading it in full — it live-verifies
  a real case-(A) loss (4,430 records, 2,785 distinct `id`: one NOAA station
  reports current predictions at several depth bins, so `id` alone collapses
  distinct points) and composes `id_currbin` from two fields already on the
  record. Rebuild: 0 errors, 0 warnings, all 7 CI gates pass.
- Agent curl scratch dumps left in the repo root (`cordis_*.json`,
  `crossref_grants.json`, `inpe.json`, `rdg.json`, `ds_*.json`, `*_ids.txt`)
  were inspected, confirmed as raw API responses, and deleted.

**Two things anyone committing this tree must know:**

1. **`client/` carries a large body of work that is NOT part of this audit.**
   `App.jsx`, `DatabasePanel.jsx`, `index.css`, `tailwind.config.js` and ~13
   *new* directories (`auth/`, `shell/`, `cases/`, `alerts/`, `timeline/`,
   `intel/`, `console/`, `saved/`, `api/`, …; 29 new files, 15 modified,
   771+/789−) belong to another concurrent session building a full frontend
   rebuild (`AuthProvider`, `AppShell`, lazy-loaded Cases/Timeline/Alerts
   pages) on this shared checkout. None of this audit's agents were assigned
   any `client/` file. This audit's own `client/` footprint is four edits:
   the three XSS gates (`EntityProfile.jsx`, `MapPopup.jsx`,
   `CameraDiscoveryThread.jsx`) and the new `utils/safeUrl.js`. **But two of
   those files are now shared**: `EntityProfile.jsx` and `MapPopup.jsx`
   carry the other session's edits interleaved with the gates (Save / Pin-to-
   case buttons, a new Breaches tab, importing components that exist only in
   their uncommitted new directories). Committing either file whole would
   pull half a foreign feature into an audit commit and break the build for
   anyone pulling it. The audit commit therefore takes only
   `CameraDiscoveryThread.jsx` and `safeUrl.js` from `client/`; the two gates
   in the shared files land whenever that session commits its feature. Do not
   sweep the other `client/` changes into an audit commit.
2. **Several fixes hand-edit files whose header says "Generated by
   gen_verified_sources.py — regenerate rather than hand-editing"**
   (`vsrc*.c`). A regeneration from the manifest would silently revert them.
   The right durable answer is to teach the generator the `VJSON_KEYED` /
   composite-key patterns (or record the key in the manifest); until then,
   treat those files as hand-maintained.

Because the agents never delivered final reports, **what they actually fixed
was re-measured, not taken from their transcripts**: all 1,770 ids re-swept
against the rebuilt binary (`--jobs 10`, well below the concurrency that
produced false failures in §0).

### Measured result of the interrupted pass

| | count |
|---|--:|
| **Fixed by agents (now OK)** | **275** — 141 were COLLISION, 134 were EMITS_NOTHING |
| Collision records lost per pass | 205,617 → **149,405** (−56,212) |
| Still COLLISION | 725 |
| Still EMITS_NOTHING | 766 |
| Changed category | 25 COLLISION→EMITS_NOTHING, 9 EMITS_NOTHING→COLLISION, 4 →SLOW |

### A third collision cause, and an engine fix that cleared 125 sources at once

Of the 725 still-colliding sources, **127 sat at an exact 1/2, 1/3, 2/3 or
1/4 stored/emitted ratio**, in whole families sharing one pager
(`ua-prozorro-*` 10→5, `global-peeringdb-*` 10→5 and 6→3, `us-usaspending-*`
6→3, `us-cms-*` 4→2; `eur-geoapi-communes` `limit=200`, 400→200). That is
neither a wrong `id_keys` (A) nor upstream duplication (B). It is **(C): the
pager appends a cursor the API ignores, the server re-serves page 1, and the
byte-identical page collapses onto the uids just written.** Nothing is lost —
`stored` is right — but `emitted` double-counts, which is exactly what the
sweep reads as a COLLISION.

Root cause was in the engine, not the sources. `lib/jsonlist.c`'s
`jsonlist_emit_paged()` **emitted each page first and only then fingerprinted
it as a repeat** — it already knew the page was a re-serve, it just counted
it before deciding. The check now runs before the emit and a repeated page is
not counted. `eur-geoapi-communes`: 400/200 → **200/200**. Re-running the 127:
**125 → OK**; the 2 residuals (`openalex-work-by-doi`, `eas-cwa-forecast-rss`)
hit an exact fraction by coincidence and went back to the triage pool.
(`lib/pagewalk.c`'s `pw_walk` already checks before emitting, but only on the
bare-`?page=1` path — a deliberate, documented narrowness left as is.)

### The 25 apparent regressions, triaged by re-running them

- **17 transient** — every `sci-chembl-*` row plus `WD_AIRPORTS` recovered to
  their original COLLISION state; one upstream host down during the sweep, in
  a file no agent touched.
- **4 now OK** — including all three `sec-fedreg-*` `VJSON`→`VJSON_KEYED`
  conversions at `emitted=2000 stored=2001`, which proves both the agent's
  edit and the new keyed path on real Federal Register data.
- **1 upstream break, not an agent** — `PY_VUE_REGISTRO_EMPRESAS` (file
  untouched; the source went from partial to zero between sweeps).
- **3 real agent regressions, each with a confirmed cause and a fix:**
  - `WHO_XMART_FLUID_EPI` (`hp3b19_health.c`): the agent appended
    `$orderby=_RecordID` to stabilise paging — **the view has no `_RecordID`
    column** (live field list checked), so the API returned 400 and zero
    rows. Worse, the *pre-existing* `.id_keys = "_RecordID"` had also named
    nothing, which is why the engine had been keying rows on the country
    name and why this source collided in the first place. Reverted the URL;
    key is now `COUNTRY_CODE,ISO_YEAR,ISO_WEEK,AGEGROUP_CODE`, the actual
    grain of a FluID row.
  - `FINRA_ATS_WEEKLY`, `FINRA_OTC_BLOCKS` (`hp3b19_finance.c`): the agent's
    `HP_CSV→HP_JSON` switch was sound (FINRA's unquoted CSV corrupts rows on
    embedded commas) but it also added `.headers = {"Accept: application/json"}`
    — and `lib/hpengine.c:2370` **already appends that header unconditionally
    for every HP_JSON row**. Two identical Accept headers → FINRA's gateway
    returns **400** (reproduced with curl: one Accept 200, two Accept 400).
    Removed the redundant header. **Engine hardened as well**: the forced
    Accept/Content-Type are now defaults that yield to a row's own header,
    so a row can also send a *different* Accept (`geo+json`, `vnd.api+json`)
    without the JSON one riding alongside.

All three repairs were then proven by running them: `FINRA_ATS_WEEKLY`
0 → `records=2000 stored=1776`; `FINRA_OTC_BLOCKS` 0 → `records=2000
stored=476` (its residual collisions are the pre-existing key question, now
unblocked and annotated with live evidence for triage); `WHO_XMART_FLUID_EPI`
0 → `records=1000`, and then — after also setting `.page_zero_based = 1`
because its page-2 URL was observed as `$skip=1001`, the `page_start`
coercion trap CLAUDE.md documents, dropping one record per page boundary —
**`records=6000 stored=5967` across 7 pages** with `$skip` on exact
multiples of 1000.

### The second launch, and where this leaves the remainder

The remainder — **~1,362 ids across 385 files** (598 collisions needing A/B
triage, 766 emits-nothing, the 2 case-C residuals) — was partitioned into 8
balanced groups and relaunched with sub-agent spawning explicitly forbidden.
**All 8 were killed by the account session limit within minutes, before
making any edit** (verified: the only diff against the pre-launch snapshot
was this audit's own `hpengine.c` change; rebuild 0 errors, 0 warnings, 7
gates pass). That disproves the sub-agent theory from the first launch: the
binding constraint is the account's session budget — 8 concurrent agents
alone exhaust it. Fan-out at this scale is not a viable way to work this
list on this account; it needs 2-3 agents at a time across limit resets, or
sequential work in the main session. The 8 manifests
(`scratchpad/manifest_r0..r7.txt`) remain accurate and ready for either.

### Round 3: three agents, all completed — 186 fixed

Relaunched at 3 concurrent agents (r0, r1, r2; ~510 ids). All three ran to
completion and reported. Fixed **186** (92 / 64 / 30); ~72 verified as
legitimate dedupe; ~60 recovered as transient outages; ~105 diagnosed and
correctly left alone (dead or moved endpoints with no replacement,
Cloudflare/bot walls, credential-gated by design); ~33 not reached. Every
field name written was confirmed in a live response. Highlights:

- **All 49 Socrata `usstate` rows** now key on Socrata's own row id
  (`$select=*,:id`, `.id_keys = ":id"`; 1000/1000 distinct on every endpoint)
  — the textbook fix for the dimension-as-id class, replacing keys like
  `primarylobbyistid`.
- **UK Gazette returns HTTP 500 to `Accept: application/json`** and 200 to
  `*/*`. Fixed by declaring `.headers = {"Accept: */*"}` — which only works
  because of the `has_accept` hardening above (before it, both would be sent).
- **Engine fix from r1 (`lib/hpengine.c`, `hp_run_html`)**: the HTML path
  deduped on the *raw* href but keyed the uid on the *resolved* link, so a page
  linking one item as both `/x/home.html` and `https://host/x/home.html`
  passed dedupe and collided at the sink — all 47 MHLW labour-bureau sources.
  Dedupe now uses the resolved link, matching the uid. Reviewed and approved.
- A dead ERDDAP URL revived (renamed variables → 3,106 rows), two moved
  maritime endpoints re-pointed, 10 Open-Meteo columnar rows rewritten as
  per-hour records, 9 Legistar rows keyed on `MatterId`, GWOSC and JMA
  map-of-objects feeds given map→array emitters.

### `id_keys` was never a composite — a wrong assumption, found and fixed

r2 reported that `FINRA_OTC_BLOCKS` stored exactly 476 = its distinct
`crdFirmName` values despite a five-field `id_keys`. Reading `hp_pick_s`
confirmed it: a comma-separated `id_keys` is a **precedence list** — the
first field present wins — not a composite. **This audit's own guidance to
the agents ("comma-separate for a composite") was wrong**, and so was this
audit's WHO fix. 26 keys (28 rows) written this session as dimension tuples
(`year,quarter,item`, `mcc,mnc`, `COUNTRY_CODE,ISO_YEAR,ISO_WEEK,AGEGROUP_CODE`,
…) were keying on their first dimension alone. They *appeared* to work only
where duplicates fell within one page, because the in-page collision guard
content-hashes those; the same key recurring across pages still collapsed.

Fix at the root, additively: `hp_pick_s` now treats `+` as a composite
(`a+b+c` → all parts joined; a missing part joins as empty since a dimension
may legitimately be null; an over-long composite is replaced by its FNV hex
rather than truncated into a collision) while `,` keeps its precedence
meaning, because existing rows rely on it as a fallback chain (`id,uid,code`).
Both callers (`hp_collision_map`, `hp_emit_record`) go through `hp_pick_s`,
so they stay mirrored. Verified no existing key contained `+`; converted all
28 occurrences added this session (none pre-existing). **Proof:**
`FINRA_OTC_BLOCKS` 2000/476 → **2000/2001**; `FINRA_ATS_WEEKLY` 2000/1776 →
**2000/2001** (+1 is the truncation notice) — zero collisions.

### Engine and design follow-ups surfaced by round 3 (not done here)

- `lib/rss_atom.c` has no collision guard: 33 `gov-pacer-*` rows share a guid
  across multi-event docket entries with different descriptions;
  `sanc_common.inc`'s `guid_reused` is the model to generalise.
- `jsonlist.c` wrapper descent for `{member:{…}}` / `{pkg:{…}}` shapes would
  subsume the per-source "lifted" helpers r2 wrote.
- `digitraffic` needs gzip: `httpclient.c` sets `CURLOPT_ACCEPT_ENCODING ""`;
  confirm the build's libcurl has zlib.
- **Three sources share the id `"real-estate"`** (homes_co / suumo /
  japan_reit) — `lint-sources` did not catch it.
- `drone-nofly` **emits 0 by design** (a port of an always-empty script):
  remove or re-point.
- `eco-energy-charts-{gr,no,ro,se}` are columnar; `eni_energy_charts_power.c`
  already handles that shape — re-point rather than re-implement.
- NCBI esearch rows return bare id lists; they need an esummary hop.
- `OASIS_PUBLISHED_STANDARDS`: `httpclient.c`'s per-host `RCorp-feeds` UA
  override now gets 520; the engine UA gets 200.

Round-3 rebuild: 0 errors, all 7 CI gates pass; one `-Wcomment` warning
introduced in `hp3b19_legalip.c:397` (an agent wrote the literal `*/*` inside
a comment, then `*\/*` — `\/` is not an escape in a C comment), reworded.

**Round 3 measured** by re-sweeping its 510 ids against the rebuilt binary
(agents' own counts are not used): **202 → OK** (153 were COLLISION, 49 were
EMITS_NOTHING); 16 EMITS_NOTHING → COLLISION (from zero to emitting —
progress, now in the triage pool); 121 still COLLISION and 168 still
EMITS_NOTHING (dominated by the dead / bot-walled / credential-gated rows the
agents diagnosed and correctly left alone, plus verified case-B dedupe);
3 COLLISION → EMITS_NOTHING checked individually below. Collision records
lost across these 510 ids: **28,207** per pass.

A fourth launch (r3–r5) was killed by the account limit during its read
phase — zero edits, verified — because round 3 had just consumed the rolling
window; it was relaunched after the reset.

## 4. Live launch of the fixed stack

`./launch.sh up --no-llama --no-suggest-llama --port 4010` from the verified
WSL clone, 2026-09-06: exit 0. Build reported up-to-date with **16,366 sources
registered**; `[httpd] listening on :4010`; `[sched] background scheduler
started (serve + refresh)`; `GET /api/health → 200`. Within seconds the
scheduler was running sources against the fixed binary with **emitted ==
stored on every run line observed** (`gnews-mon-troop-movement records=100
stored=100`, `…-border-clash 100/100`, `…-naval-deployment 100/100`, …) — the
end-to-end proof the unit gates cannot give.

Reported as found, not hidden: the three LLM pods (`llama`, `llama-suggest`,
`llama-embed`) did not start because this host carries neither
`native/llama/llama-server` nor any `.gguf` model — a clone cannot carry an
11 GB model; not a defect. `/api/status` answered `503 {"error":"Auth not
configured"}` with no `SUPABASE_JWT_SECRET` set — the auth gate failing
closed, as designed. Two `launch.sh` gotchas worth knowing: its freeze step
runs `pkill -f "llama-server.*gpt-oss-20b"`, which will kill *your own shell*
if the invoking command line happens to contain both strings (invoke it from
a wrapper); and `:4000` was already held by another session's 24-hour-old
`japanosint --serve` (pid 402), which the freeze correctly ignores — use
`--port`. The test server was shut down afterwards with `./launch.sh down`.

## 5. Authenticated endpoint test: every route iOS and the web client call

`launch.sh up` was re-run against a **parity auth profile** — the trick from
`native/tests/contract/run.sh`: `SUPABASE_URL=` empty + `SUPABASE_JWT_SECRET`
set makes the server accept a locally-minted HS256 JWT, so authenticated
routes can be exercised without the user's credentials (production verifies
JWKS/RS256 only, so no token can be minted for the real backend — by design).
`/api/me` returned 200 with a real tenant, confirming the gate works rather
than being bypassed.

**48 routes referenced by the iOS app** (`ios/**/*.swift`), GET, authenticated:
23 × 200 · 15 × 403 · 5 × 400 · 2 × 405 · 2 × 404 · 1 timeout.
**Zero 5xx. No crash, assert, or corruption signature in the server log.**
- The 403s are the operator gate doing its job (`/api/admin/*`, `/api/db/*`,
  `/api/keys`, `/api/evidence/verify`, `/api/follow/recent`) — the parity
  token is an ordinary member, not a platform operator.
- The 400s are routes that require query parameters (`/api/geocode`,
  `/api/isochrone`, `/api/permalink`, `/api/search/analyze`) — correct.
- The 405s are POST-only routes probed with GET — correct.
- The 2 × 404 are **string prefixes**, not real routes: iOS builds
  `/api/alert-events/read-all` and `/api/members/invite` by appending path
  segments. Not findings.

**Finding — `/api/status` is 20.5 MB and takes 11.6 s.** It timed out at the
12 s client budget on the first pass and only answered on a 60 s retry. Every
client calls it (it is the source-registry status view, and the registry is
now 16,366 rows). The 2026-08-16 audit already flagged the sibling
`/api/intel/sources` as a 10 MB answer needing ~12 s; this is the same shape,
twice the size, and it is on the startup path of both clients. It wants
pagination or a summary projection — a client on mobile data will not load
it. Not fixed here: changing that response shape is an API contract change
touching both clients, which is a decision, not a repair.

### Round 4: one Opus agent (the Fable quota was exhausted) — 81 more fixed

Concurrency was cut to one agent working r3–r5 in a single pass ordered by
records lost. It reported **81 fixed** across 17 files, 24 verified-legitimate,
35 transient, ~71 diagnosed-unfixable, ~300 not reached. It also confirmed
**zero half-applied edits** across all 116 modified collector files (it looked
for any source id registered by both a `V*` macro and a hand-written
`source_def`), which matches the clean rebuild.

Two classes are worth recording because neither is an `id_keys` question:

- **35 `us-caltrans-*` emitted nothing** because the array is
  `[{"cctv":{…}}]` — the outer object carries no scalar at all, so every
  record was discarded as shape noise. A `VCAL` macro now hoists `index`,
  `locationName`, `lat`/`lon` and `recordDate` out of the envelope, keeping
  the envelope itself.
- **11 `eur-ine-*` were 403** — and the measurement is the interesting part:
  `servicios.ine.es` rejects the word **"collector"** in a User-Agent, not
  "OSINT". The two existing `UA_OVERRIDE` entries therefore could not help.
  The new entry (`native/core/httpclient.c`) is the engine's own agent with
  that one word removed; it still names the product, the repository and a
  contact route. This is an out-of-manifest `core/` edit, reviewed here and
  kept: it is an identifying agent, not browser spoofing, and it is in the
  same table and idiom as the existing overrides.

Nondeterministic paging turned out to be a recurring cause in its own right —
`geo-fema-disasters`, 12 `us-socrata-search-*`, 4 `sci-hub-arcgis-*` and the
HDX pair were re-serving some rows and never fetching others because their
result order was unstable; each now pins an explicit sort, measured before
and after (e.g. 21 shared ids per 100 → 0).

**One claim in that report was checked and is wrong.** It stated that a
working-tree change in `lib/jsonlist.c` (case-insensitive `_name` suffix, for
`OBJECT_NAME`) would turn 36 `sci-celestrak-*` sources green after a rebuild.
The `jsonlist.c` diff contains only this audit's two changes; the
`pick_name_suffixed` / `OBJECT_NAME` handling is **pre-existing committed
code**, not a change from this pass, and it cannot newly fix anything. The
same report also states celestrak.org is unreachable from this host. The
honest status of those 36 is therefore **unverifiable from here, cause
unknown** — not "expect them green". Recorded so the next session does not
inherit a false all-clear.

### Round 4 measured, and the cumulative result

The r3–r5 re-sweep (511 ids) puts **142 more at OK** — 82 COLLISION→OK,
60 EMITS_NOTHING→OK — and drops collision loss across that set to 3,417
records per pass. Both headline round-4 fixes are confirmed by the sweep
rather than by the agent's own count:

- **all 11 `eur-ine-*` → OK** with real volumes (876, 462, 1039, 255 … rows),
  so the "the rejected token is the word *collector*" diagnosis was correct;
- **all 35 `us-caltrans-*` → OK** (752, 276, 181 … rows), so the
  envelope-hoisting `VCAL` macro works.

**Cumulative, over the original 1,770 defective ids, all measured:**

| | at the start | now |
|---|--:|--:|
| OK | 0 | **783** |
| EMITS_NOTHING | 888 | 612 |
| COLLISION | 882 | 370 |
| SLOW | 0 | 5 |
| **collision records lost per pass** | **205,617** | **66,197** |

That is **783 sources repaired and 139,420 records per pass recovered**, every
number from `audit_registry_emit.py` against the built binary, never from an
agent's self-report. Two caveats, both in the honest direction:

- **~24,932 of the remaining 66,197 is not loss at all** — it is
  `gr-diavgeia-positions`, proven case-B (correct dedupe of an upstream that
  republishes 233 records ~100× each). Real remaining loss is **~41,265**.
- **696 of the 1,770 still carry their round-1/2 verdicts** because no later
  sweep has re-measured them, so the true figure is at least this good.

The pattern worth carrying forward: of the 133,966 records recovered, the
large majority came from **four root causes** — the page-repeat emit ordering
(125 sources at once), the `+` composite operator, `HP_RECJAR` (29,851 from a
single row), and one missing `$order=:id` clause (52 rows). Per-row triage
found the patterns; fixing them at the root is what scaled.

### Round 5 (r6/r7): killed mid-edit, and the recovery worked

r6 and r7 — 342 ids across 98 files — had never been worked by any round. A
single Opus agent got through 11 files before the account limit killed it
**mid-sentence** ("Applying."). The recovery procedure this audit has been
using caught it exactly:

1. The rebuild failed with `macro "VJSON_KEYED" requires 13 arguments, but
   only 12 given` in `vsrc17_ng_aid_1.c` — the agent had renamed
   `VJSON` → `VJSON_KEYED` and died before adding the id field.
2. The fix was unambiguous from the agent's OWN recorded evidence, left in the
   comment above the row: *"one page of 100 rows carries 100 distinct aid
   values and only 99 distinct titles"* — and every completed sibling
   (`vsrc17_bj_aid_1.c`, `_mu_`) keys on `"aid"`. Completed by hand.
3. Rebuild: 0 errors, 0 warnings, 7/7 gates, 16,366 sources.
4. Sweeping the 57 ids in the 11 touched files: **15 now OK** (13
   COLLISION→OK, 2 EMITS_NOTHING→OK) — including `af-dportal-act-ng`, the very
   row that had been left broken, plus the Tor/onionoo family, the Belgian
   ELIA grid pair and three Catalan Socrata sets.

This is the third time an agent was killed mid-edit in this audit and the
third time a compile error located the damage precisely. **The tree is never
trusted after an interrupted agent; it is rebuilt.** ~331 of the r6/r7 ids
remain unworked — the manifests are accurate and ready.

### The last big one: a record-jar parser, and what the loss number really was

With the per-row rounds done, the remaining collision loss was **114,033
records per pass** — and two ids were 48% of it:

- `IANA_LANGUAGE_SUBTAGS` — 29,851 lost, **a real bug**
- `gr-diavgeia-positions` — 24,932 "lost", **not a bug at all** (proven
  earlier: 233 distinct `uid`s, the same positions repeated ~100× with
  identical content; storing 233 is correct dedupe)

So the honest remaining figure was never 114,033. Subtracting the phantom and
fixing the real one leaves **~59,250**.

**HP_RECJAR.** The IANA registry is a *record-jar* (RFC 5646 §3.1.2):
`Key: Value` lines, records separated by a line of `%%`, continuation lines
indented. It is not CSV, JSON or XML — so before this the only way to declare
the row was to lie about its shape, and it was declared `HP_CSV`. That made
every **line** a record: 49,312 emitted, 19,461 stored, and what it stored
were fragments like `Description: Afar` rather than subtags.

A new `HP_RECJAR` mode parses it properly, then hands the records to the
**same** shared path everything else uses — one flat object per record, the
same collision guard (with a NULL flat_fn, as the XML path does), the same
`hp_emit_record`. So `title_keys`/`id_keys`, the `+` composite, geo, dedupe
and the availability tally all behave identically. A repeated key inside one
record (IANA gives some subtags several `Description:` lines) is **joined with
"; " rather than overwritten**, because dropping the later values would
discard real content — the same rule the rest of the engine follows.

The row now declares `.id_keys = "Type+Subtag,Type+Tag"` — the composite,
because a bare `Subtag` is not unique across record types, and the `,Type+Tag`
fallback covers the grandfathered/redundant entries that carry `Tag` instead.

| `IANA_LANGUAGE_SUBTAGS` | emitted | stored | lost |
|---|--:|--:|--:|
| before (HP_CSV) | 49,312 | 19,461 | **29,851** |
| after (HP_RECJAR) | 9,297 | **9,297** | **0** |

9,297 is the registry's real record count, and the stored rows are real:
uid `IANA_LANGUAGE_SUBTAGS|language|aa`, title "Afar", properties
`{"Type":"language","Subtag":"aa","Description":"Afar","Added":"2005-10-16"}`
— the whole record, every field, nothing invented. (The file's own
`File-Date:` header parses as one leading record. It is genuinely in the file,
so it is kept rather than silently dropped — 1 row of 9,297.)

0 warnings, all 7 CI gates green.

### A fourth collision cause: the upstream publishes duplicates, and the walk pays for them

`IT_FVG_MOBILE_SITES` was the largest remaining collision after IANA — 10,000
emitted, 2,896 stored, 7,104 "lost". A round-3 agent had already "fixed" it by
adding `$order=:id` and declaring `.id_keys = "impianto"`, reporting *"impianto
is unique per row (1000/1000 on a live page)"*. The sweep still showed the
identical 10,000/2,896, so the fix had not worked. Measuring it settled why:

- `impianto` is unique **within one page** and not across them: the 10 walked
  pages hold 10,000 rows and only **2,814 distinct** `impianto`. That is the
  same in-page illusion `hpengine.h` now documents for `id_keys` — per-page
  uniqueness is not record identity.
- Under **every** field combination tried, a 1,000-row page held only ~17
  distinct records, and the colliding rows were **byte-identical**. So this is
  case (B): the upstream republishes the same rows, and the sink collapsing
  10,000 to 2,896 was correct. There was no key to fix.
- `$order=:id` did not even stabilise the walk — offset 5000 still re-served
  350 of page 0's rows.

The real defect was therefore not identity but **budget**: the 10-page walk
was spending ~98% of its requests re-fetching duplicates and never reaching
the rest of the 335,667-row table. SODA can dedupe server-side, and measuring
`$select=DISTINCT` showed page 0 and page 1 each returning 1,000 rows, 1,000
distinct, sharing nothing.

| `IT_FVG_MOBILE_SITES` | emitted | stored | walk |
|---|--:|--:|---|
| before | 10,000 | **2,896** | truncated at the 10-page ceiling |
| after (`$select=DISTINCT`) | 6,103 | **6,103** | **exhausted naturally at 8 pages** |

Real records captured **more than doubled** (2,896 → 6,103) and the source now
walks to completion instead of truncating — so it also stops emitting a
truncation notice it no longer needs. The whole 335,667-row table contains
6,103 genuinely distinct records; the rest is republication. Ordering by a
real column would have been *worse*: at `$order=impianto` a 1,000-row page
holds nine installations.

The composite `id_keys` exceeds the key scratch buffer and so resolves through
the FNV-hash branch added with the `+` operator — fixed width, unique per
content, never truncated, exactly as designed.

Its sibling `IT_FVG_EMF_MONITORING` was checked and is genuinely clean
(1,000 rows, 1,000 byte-distinct, 1,000 distinct `punto`, zero overlap at
depth) — the round-3 fix there was correct and was left alone.

### `$select=*,:id` without `$order=:id` — 52 rows, one missing clause

`NY_LOBBYIST_REGISTRATIONS` still lost 2,823 records after round 3 had given
it Socrata's own row id. The key was not the problem: the stored uids were
real Socrata ids (`row-hyg5_5rnc_mduj`), and a quick two-page check showed
`:id` unique with no overlap. The walk was.

SODA returns rows in undefined order without an `ORDER BY`, **and the order it
picks can differ between requests** — so an offset walk that takes ~86 seconds
re-serves some rows and never reaches others. Measured over the exact ten
pages the engine walks:

| | rows | distinct `:id` |
|---|--:|--:|
| `$select=*,:id` (as shipped) | 10,000 | **7,632** |
| `+ $order=:id` | 10,000 | **10,000** |

Round 3 had added `$select=*,:id` and `.id_keys=":id"` to 52 rows in
`hp3b19_usstate.c` and **`$order=:id` to none of them** — the row id was
being selected and keyed on, but nothing pinned the order it arrived in. All
52 now carry it.

Swept afterwards: **52/52 OK, 0 records lost to uid collision**, 459,648
emitted / 459,689 stored. 17 of the 52 had been colliding and are now clean
(**5,427 records per pass recovered**); the other 35 were already fine because
their tables fit in a single page, where there is no order to drift.
`NY_LOBBYIST_REGISTRATIONS` also got ~4× faster (86s → 21s), because an
ordered walk stops re-fetching what it already has.

### Round 6 (r6/r7 completed): 24 more, all verified — and a whole observatory came back

The second r6/r7 agent ran to completion: 23 fixed, 40 verified-legitimate,
5 transient, 30 diagnosed-unfixable, ~150 not reached. Sweeping its claimed
fixes: **24/24 OK, zero collisions**, 32,886 emitted / 32,889 stored.

The largest was a shape bug, not an identity one. **Twelve SIMBAD and VizieR
astronomy sources emitted nothing at all** because a TAP service asked for
`format=json` returns *VOTable-in-JSON*: `data` is an array of **arrays**, not
of objects, so every record was discarded as shape noise. Switching those
queries to `format=csv` (and aliasing `main_id as name` in the ADQL so the
label rules apply) took each from **0 to 200 records** — 300 for
`sci-vizier-tables`.

Also fixed: CKAN relevance order is not total, so five `sec-hdx-*` feeds plus
two others were re-serving across pages (500 records → 464 distinct;
`&sort=name asc` → 0 duplicates); `undp-transparency-projects` keyed on
`project_id` (its titles collide); `sensor-community-air` given a
`sensor:<id>:<field-set>` uid because one sensor genuinely posts several
measurement groups per pull; and `id-jakarta-emission-test-firms` had
`returnGeometry=false` on a geo row, storing 269 premises with no location.

Two judgement calls worth recording, both correct:
- It found `ocha-fts-*` returns **406 to any User-Agent containing "osint"**
  (`feedbot/1.0` gets 200) and **did not spoof**, citing the house rule. The
  earlier INE fix was allowed because removing one word still left an
  identifying agent; inventing an unrelated one is different.
- It flagged that `hp3b19_latam.c`, `_eurasia.c`, `_health.c`, `_iana.c` and
  `vsrc13_latam_data_2.c` already carried fresh fixes from earlier rounds and
  left them alone rather than re-deriving them.

Its 40 case-B verifications are the other half of the value: 13
`JPC_JPCERT_PHISHURL_*` (6,158 rows, 6,155 byte-distinct — stored exactly),
22 `JP25_JPPOLICE_*` CSVs with byte-identical rows, `cyb-siemens-productcert`
(advisory *revisions* share one guid, and jsonlist's rule is "same upstream id
= same record"), and `awc-airport-info`/`awc-pirep`, whose quadtree re-queries
a tile as four children — now commented so nobody "fixes" it later.

### The FOURTH copy of the uid-collision bug: `lib/rss_atom.c`

CLAUDE.md §4b records that the same collision defect existed independently in
`jsonlist.c`, `hpengine.c` and `geojson.c`, and calls that "the strongest
argument in this repo for looking for the OTHER copies of any bug you fix."
There was a fourth. `lib/rss_atom.c` had no guard at all — `grep -l collision
lib/*.c` returned the other three and not it.

Verified live rather than argued: PACER's `rss_outside.pl` keys every entry on
the DOCKET, so one docket's filings all carry one guid. `ecf.txsb`, fetched
2026-09-09: **1,860 items, 1,576 distinct guids**, and the worst guid carries
13 items whose descriptions are different filings on the same docket
(`[Schedule A/B]`, `[Schedule C]`, `[Declaration]`, …) — confirmed *not*
byte-identical. Every one of those was folding into a single row while the run
reported a healthy `records=1860`.

The guard is the one the other three already have: a pre-pass computes each
item's uid with **the same precedence the emit loop uses** (guid → link →
sha1(title|pubDate) — a guard that derives its key differently from the code
it guards is not a guard), flags only the uids that collide within this fetch,
and extends those with a hash of the item's own bytes. Byte-identical repeats
still collapse, which is real dedupe and is wanted; items that merely share a
guid are all kept.

Measured on the same feed with the item cap lifted: **1,860 emitted → 1,854
stored**, versus the 1,576 distinct guids it would previously have collapsed
to — **278 records recovered on one feed**, and the 6 that still collapse are
byte-identical.

Across the registry: of the previously-colliding rows that are genuinely
`VRSS`-registered, **35 of 44 are now OK** and their loss fell from **892 to
58 records per pass** (93%). The 9 remaining lose 58 between them — byte-
identical duplicates, i.e. the guard working as designed.

The single largest case is `cyb-siemens-productcert`, found independently by
the r6/r7 agent: **2,255 items carrying 2,253 distinct titles but only 1,055
distinct guids** — one advisory PDF guid (`ssb-439005.pdf`) shared by 41
differently-titled advisories, because Siemens keys a revision on the document
it revises. Before the guard it stored 1,055; after, **2,255 emitted → 2,252
stored, 1,197 records recovered on one feed**, with 3 byte-identical repeats
still correctly collapsing.

*(A note on measuring this honestly: the first filter I used matched every id
appearing in a FILE containing `VRSS`, which swept in 180 rows — most of them
JSON sources that merely share a file with an RSS row, including several
already proven case-B. That would have credited this fix with ~4,300 recovered
records. Matching the id against an actual `VRSS(` invocation gives 44 rows and
834. The smaller number is the true one.)*

### `lint-sources` earned its keep: two "fixes" that would have duplicated work

The final agent repaired two dead feeds — `rusi` and `frontex-news` — by
re-pointing them at the live URLs the sites advertise. Both were verified live
and both were wrong: **those exact endpoints already have working collectors**
(`sec-rusi-whatsnew`, `sec-frontex-news`). The result would have been two
registered sources polling one endpoint on their own schedules and storing the
same items under two `source_id`s. `make lint-sources` caught it immediately as
a `dup-endpoint` regression — the check exists for precisely this, and it is
the reason it gates CI.

Both rows were reverted to their dead URLs and now carry a note saying the feed
moved, which source already collects it, and that retiring or aliasing the row
is a decision rather than a repair. They read as `EMITS_NOTHING` in the sweep,
which is the honest verdict: the row is dead, and the CONTENT is not lost.

(The same pass also reintroduced the `-Wcomment` trap twice by writing
`grep -n collision lib/*.c` inside a C comment — the glob contains a
comment-open sequence. Reworded; the build is back to zero warnings.)

## 6. Semantic routing: closing the NL-query gap in the OSINT pipeline

**Audit finding first.** Natural-language querying was complete on one side and
absent on the other:

- **Intel search — complete.** `core/embed_pod.c` (a `_maint` pod, interval
  120) embeds `title + summary` into a sqlite-vec `intel_vec` table, refusing
  to run against a different model or dimension rather than mixing two
  embedding spaces. `/api/intel/semantic` fuses that vector arm with FTS5 by
  Reciprocal Rank Fusion, is tenant-scoped, counts rows the tenant may not see
  in `meta.tenant_withheld`, and reports `coverage` on every response so a thin
  index cannot read as a thin corpus. Nothing needed here.
- **OSINT pipeline — nothing.** `osint_dispatch.c`, `pipeline.c` and
  `searchapi.c` contained **zero** references to embeddings. A query's *route*
  — which of ~1,535 entity-pivot services to run — was chosen by putting the
  service catalogue in the analysis prompt. Unbounded that is **207,353 tokens
  against a 16,384-token context**, so it degrades: full descriptions → bare
  ids → first-K. Step 3 truncates in **registry order**, which is link order,
  and the hand-written entity services register last: `DNS_RECORDS` #1201,
  `IP_GEOLOCATION` #1288, `DOMAIN_WHOIS` #1529 of 1,535. It dropped precisely
  what someone asking "who owns example.com" needs.

**`core/service_vec.{c,h}`** closes it, reusing the embedding machinery that
already existed. Each service's `id: name. description` — the same prose the
model is shown, so what ranks is what it reads — is embedded **once** into a
`service_vec` vec0 table; at query time the question is embedded and the K
nearest services are listed **with their descriptions**. A small relevant menu
instead of a large arbitrary one, comfortably inside the budget.

Design points that are the difference between this helping and hurting:

- **Inert unless configured.** No `JO_EMBED_URL`, no index, an embed failure,
  or a dimension mismatch → returns NULL and the pipeline falls back to
  `osint_services_list_bounded()` unchanged. Same contract `embed_pod` has.
- **All-or-nothing builds.** It builds into `service_vec_new` and renames only
  on success. A half-index answers confidently from whatever fraction embedded
  before the failure — the rule-4d failure mode.
- **Rebuild, never mix.** A registry signature (FNV over the pivot ids) plus
  the model identity gate the build; a change to either rebuilds rather than
  comparing vectors across two spaces.
- **The bound is stated in-band**, and says *how* the subset was chosen —
  "selected as the closest matches to this query by embedding similarity
  rather than by registry order" — because "the 60 closest" and "the first 60"
  are both bounded views and a reader must not have to guess which one
  produced the answer. `progress_stage_note` says the same to the user.
- **Briefing and vocabulary stay in sync.** The analysis schema's service enum
  is built from the *same* ids that were listed, via the new
  `osint_analysis_schema_dynamic_ids()`. This is the trap in the change: the
  existing call pairs the catalogue with
  `osint_analysis_schema_dynamic_limited(cat.shown)`, i.e. the first N in
  registry order — which would not overlap a semantically-chosen menu at all,
  leaving the model briefed on services it was forbidden to name.

**Verification — and the two bugs it caught.**
`tests/unit/test_service_vec.c` drives the module against a stub embedding
server (a hashed bag-of-words: deterministic and query-dependent, explicitly
*not* a claim about embedding quality) and asserts six invariants. On a host
without `python3` the active-path cases skip and the inert path — the one that
matters where there is no embedding server — still runs.

Writing it was not ceremony. The module compiled with zero warnings and was
wrong twice, and only running it showed that:

1. **`ALTER TABLE ... RENAME` silently broke the index.** Building into
   `service_vec_new` and renaming on success is the obvious way to get an
   atomic swap — but a `vec0` table owns shadow tables whose names derive from
   the virtual table's name, and the rename does not carry them, so the
   renamed table is no longer queryable. The build logged
   `indexed 1838 of 1838` and the very next KNN query returned nothing: a
   confident report of success over an index that could not answer. Replaced
   with an in-place build plus a `state` flag ("building" → "ready", promoted
   only after the last row), which gives all-or-nothing where it actually
   matters — at the QUERY — without renaming anything.
2. **Model-change detection read the wrong table.** The staleness check called
   `embed_index_dim()`, which reads the INTEL pod's `intel_vec_meta` — a
   different index with its own lifecycle. The service index would have
   inherited that pod's model and never noticed its own changing, exactly the
   cross-space comparison the design says it refuses. It now compares the
   recorded dimension against a one-request probe of the server as it is right
   now, which is the check that cannot be fooled.

Final run, all six green:

```
  inert without JO_EMBED_URL: ok
  indexed 1838 entity-pivot services
  catalogue: 12 of 1838, descriptions kept, bound stated: ok
  ranking is query-dependent (0 of 10 shared between two unrelated queries): ok
  failed build is not queryable (state=building, catalogue NULL, reason recorded): ok
  model/dim change rebuilt 64-d -> 96-d, no mixing: ok
```

The query-dependence assertion is the one that proves the feature rather than
the plumbing: two unrelated questions share **none** of their top 10, so the
menu is genuinely a function of the query. A router that returned the same K
every time would be registry order wearing a hat, and would pass every other
check here.

(Note: the registry now holds **1,838** entity pivots, not the 1,535 the
dispatcher's comment records — the truncation problem has grown since it was
written.)

**Runtime note, stated rather than glossed:** this host has no `.gguf`, so the
pod is inert here and `/api/intel/semantic` returns its honest 503. The active
path is proven by the stub test, not by a real model; end-to-end behaviour with
real embeddings needs a bge-m3 or multilingual-e5 in `models/`.

## 7. CRITICAL, found and fixed: one map-layer request froze the entire server

Scanning the 189 web-only routes returned **183 timeouts** — a number that
looks like a catastrophe and is not one. `/api/db/tables:`, a malformed path
that must 404 instantly, was among them; a server that cannot answer a
malformed path is not answering *anything*. The scan had not found 183 broken
endpoints, it had found one blocking request cascading through a sequential
client.

Isolated, with no concurrent load (`audit_run/do_block_test.sh`):

| while ONE `/api/data/castles` is in flight | before | after |
|---|---|---|
| `/api/health` | 0.0007 s → **timeout at 20 s** | **0.0008 s** |
| `/api/db/tables:` (fast 404) | 0.001 s → **6.45 s** | **0.00097 s** |

**Cause.** `dataapi_layer()` serves a TTL cache and, on a miss, **runs the
layer's collector inline** — a live upstream fetch. `httpd.c` called it
directly in the mongoose event-loop handler, so for the whole duration of that
fetch the server had no thread left to answer anyone: not other tenants, not
open SSE streams, not the health check a load balancer uses to decide the
instance is alive. Any single user opening any uncached map layer — 237 of
them are reachable from the web client — took the deployment down for
everyone until the upstream replied.

**This was a known bug applied to the wrong half of the codebase.** `httpd.c`
already documents the identical freeze for the `/api/admin/sources/<id>/run`
route, in the same words the measurement reproduced — *"Running it inline
froze the entire server for the collector's duration (minutes, for a tiled
Overpass source): no /api/health, and every open SSE stream stalled because
MG_EV_POLL could not fire"* — and fixed it with `srcrun_thread`. `/api/data/`
runs the same collectors and was simply never converted.

**Fix.** `datarun_thread`, mirroring `srcrun_thread` exactly:
- Runs on a detached worker; the event loop returns immediately.
- Takes its **own `db_worker_open()` handle**. Sharing `g_db` is the documented
  corruption hazard: a collector reaches `intel.c emit()`, which opens explicit
  `BEGIN`/`COMMIT`, and SQLite serialises *access* to a handle but not
  *transactions* — a `BEGIN` here would land inside an event-loop handler's
  open transaction, so its `COMMIT` would publish their half-finished write.
- Replies via **`wakeup_reply_big()`'s parking path**, not the inline wakeup.
  This is essential and non-obvious: the wakeup channel is one UDP datagram
  capped at `WAKEUP_INLINE_MAX` (1400 bytes) that **discards a larger payload
  whole while still returning true** — the worker would believe it replied
  while the client hung forever. A fused FeatureCollection is routinely
  megabytes.
- Unknown ids answer 404, which is exactly where the old fall-through landed:
  the generic matcher is the last `/api/data/` handler (the explicit
  `cameras/*` ones match earlier).
- Inline fallback retained if `pthread_create` fails, so an allocation failure
  degrades to the old behaviour rather than dropping the request.

**Verified after the fix:** `/api/data/castles` → 200 with a valid 138 KB
FeatureCollection (644 features) — 99× the inline cap, proving the parking
path; unknown layer → 404; warm second hit 0.0064 s; **five concurrent
collector requests with `/api/health` still answering in 0.0007 s**; no crash,
assert or corruption signature; 0 build warnings and all 7 CI gates green.

Not fixed, reported: `/api/data/aed-map` exceeds 180 s on a cold cache (it was
in the timeout bucket before this change too — a slow collector, not a
regression), and **`/api/status` is 20.5 MB / 11.6 s** and on both clients'
startup path. Both are the same shape as the sibling `/api/intel/sources`
(10 MB) the 2026-08-16 audit flagged. They want pagination or a summary
projection — an API contract change touching both clients, which is a
decision, not a repair.

## 7. Prior audit (2026-08-16): every CRITICAL/HIGH finding re-verified closed

- **C1** fabricated transit vehicles → fixed; features now carry explicit
  `synthetic` / `schedule_backed` flags.
- **H1** cleartext bind → server half fixed: `JO_BIND` now defaults to
  `127.0.0.1` with `0.0.0.0` an explicit opt-in documented as requiring a TLS
  proxy. The iOS LAN default (`http://…​.local:4000`) remains cleartext.
- **H2** dashboard of zeros → fixed; `SourceDashboard` sets and renders
  `fetchError`, and the live dot no longer pulses green after a failed refresh.
- **H3** `/api/follow/recent` 501 → fixed; the refusal is shown, not converted
  into a false empty state.
- **H4** mention truncation → fixed; renders "Showing the N most recent of M
  mentions", and states when the total is unknown.

## 8. What's still clean

All 7 `make` CI gates (`selftest`, `unit`, `hptest`, `lint-sources`,
`audit-sources`, `pagewalktest`, `source-floor`) pass with zero findings, both
before and after this pass's 40 file changes. Full build: zero warnings under
`-Wall -Wextra`. iOS: zero findings of the duplicate-key defect class that
caused the last two main-branch compile breaks. No SQL injection, no
auth/tenant-isolation bypass, no unclosed file descriptors found anywhere in
scope.
