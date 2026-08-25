# Known issues register — 2026-08-24

Everything found during the full-codebase audit of 2026-08-24, with status. This
is a register, not a narrative: each entry says what is wrong, how it was
established, and where it stands. Entries marked **OPEN** are real work someone
still has to do.

Two rules for maintaining this file. Every claim must be something that was
*measured on a machine*, not inferred — a register that mixes the two stops
being usable. And an entry only moves to FIXED when there is a test or a
reproducible measurement pinning it, because the failure mode this whole audit
kept finding is a fix that silently regressed somewhere else.

---

## The theme

Nearly every serious defect below is one shape: **work that reports success
while producing nothing, or less than it should.** A collector that fetches
36,166 records and stores none, exit code 0. A CLI that prints `PASS` for a run
it never did. An endpoint that returns 500 of 700 rows and says nothing about
the other 200. A bloom filter that resets its own dedup history every run.

That is why the house rules are written the way they are, and it is the lens to
apply to anything new.

---

## FIXED this session

### Build and gates

| # | Issue | Evidence |
|---|---|---|
| 1 | 124 compiler warnings | now **0 errors, 0 warnings** on a clean rebuild |
| 2 | `make unit` dead on every Windows-origin checkout | `.gitattributes` forces LF but could not rewrite files checked out before it existed; git normalises CRLF on check-in so `git status` stayed clean while the tree stayed broken. Exit 127 → passes |
| 3 | `--selftest` advertised but never parsed | `docs/backend-structure-and-pipeline-report.md:108` had recorded it as a known dead flag; it only "worked" because unknown flags fell through |

### Fabricated or lost data (house rule 1)

| # | Issue | Evidence |
|---|---|---|
| 4 | Timestamps formatted from an **uninitialised `struct tm`** — `gmtime_r` NULL and `strftime` 0 returns ignored — written into `published_at` | 5 sites |
| 5 | `atlas_jp` staged a pagination cursor in 1024 bytes then copied it into 768 | every next-page URL past the limit was silently cut |
| 6 | `chan_5ch` declared Monazilla protocol headers, then fetched with a function that takes none | every board fetched as a generic crawler |
| 7 | `httpd.c` built a JSON reply with `snprintf` into 128 bytes | any source id past ~77 chars returned JSON cut mid-token |
| 8 | Breach ingest bloom filter rebuilt **every run**, wiping dedup history | created with capacity exactly `expect`, so its own `count + expect > capacity` check could never pass. Re-ingest reported every row new. Verified: re-ingest now 0 new |
| 9 | CLI reported success for work it never did — `--run` with an empty id, `--dispatch` without an entity, any flag typo → `[selftest] PASS`, exit 0 | now exit 2 with usage |
| 10 | `--type passwrod` silently ingested a password dump in `auto` mode | explicit type that fails to parse is now an error |

### Discarded data (house rule 2)

`make audit-sources`: **91 findings across 54 files → 0 across the whole tree.**
Every finding closed as a real fix, a justified `exhaustive-ok` marker, or a
scanner false positive.

Largest recoveries, all measured against live endpoints:

| source | was | now |
|---|---|---|
| IRS exempt organisations | 5,000 | **278,014** |
| EEA industrial pollutant releases | 1,000 of 140,743 | all 141 pages |
| ECDC respiratory (see #14) | 31 stored | **12,648** |
| luchtmeetnet NL | page 1 of 94 | full walk |
| Rocky Linux errata | 100 | 9,608 |
| Homebrew formulae | 300 | 23,133 |
| sensor.community | 3,000 of 18,056 | unbounded |
| APNIC AS population | 25 per country | every AS |
| ESMA register | 1 | 1,377 |
| Wikimedia top articles | 200 | 1,000 |

Plus twelve scheduled feeds that were asking upstream for `limit=1`, RDAP
discarding an abuse contact's eleven nested sub-entities, and USGS fetching each
gauge's backup sensor series and throwing it away.

### Fetch/emit/store (house rules 4 and 4b)

| # | Issue | Evidence |
|---|---|---|
| 11 | Traps documented and fixed for the hp engine were **never applied to the older `vsrc`/`VJSON`/`VCSV` tree** — CSV comment banners parsed as headers, two-level envelopes, nested scalar maps, list envelopes | sweep of 1,197 scheduled sources: zero-emitters **260 (21.7%) → 102 (8.5%)**; rows stored per pass **1,457,499 → 2,002,348 (+544,849)** |
| 12 | `gen_verified_sources.py` carried a dead module-level macro copy **three fixes behind** the real `.inc` | re-enabling it would have put ~6,500 sources back on page-1-only |
| 13 | `lib/jsonlist.c` uid collision — fallback key (title, link, published) | `us-openfda-device-pma-detail`: 109 emitted, **1 stored** → 109/109 |
| 14 | `lib/hpengine.c` uid collision — same defect, independently | 46 rows losing **114,795 records per pass**. `ECDC_RESPIRATORY` 12,648 emitted / 31 stored → **12,648/12,648** |
| 15 | `remote_key` built with `%.120s` — two identifiers sharing a 120-char prefix collapsed | now hashed rather than cut |
| 15a | **`lib/geojson.c` — the THIRD independent copy of the same collision defect.** It was first reported as "same class, ~9 sources, ~3,052 records/pass" with no evidence; that report was wrong about the mechanism, which is why it was investigated before being changed. The content-hash fallback is genuinely collision-free. The defect is in the **upstream-id** path: `NATIVE_ID_KEYS` contains `station_id` and `id`, names that are routinely a DIMENSION — a station reporting hourly is one `station_id` and many observations, so a FeatureCollection of readings collapsed to one row per station. Same guard added. Measured across 45 geo sources: **474 rows rescued**, 454 of them from `geo-cwfis-firewx-naefs` (which emits 500) |
| 16 | 48 `afr-hdx-*` sources dead: `data.humdata.org` blocklists the substring **"OSINT"** in the User-Agent | `Japan` → 200, `OSINT/1.0` → 406, no UA → 200; our repo URL also trips it. Per-host UA override (not browser spoofing, which this codebase rejects). Result: 14,599 records, 0 failures |
| 16a | **Breach `keyid` used only the first 10 hex digits of a SHA-1** — 40 bits — and `keyid` is what `breach_store_put()` upserts on. At Pwned-Passwords scale (~850M) the birthday bound gives **~330,000 password records silently overwriting each other**, with the ingest reporting every one as written. The identity path had the same defect scoped per breach (~4,500 lost in a 100M-row dump), plus an 80-char cap on `source_id` that merged breaches sharing a prefix. Forced by `keyid[32]`, which could not hold `"password:"` + 40 chars — while the column is SQLite TEXT with no length limit, so the cap bought nothing. Now full-hash; verified keyids are 40-char and the derived `entity_mentions.item_uid` still matches its item |
| 17 | Rule 3: one registered row could never execute | `audit_batch_reachable.py` resolved duplicate manifest opts **first**-wins while `gen_hp_batch.py` resolved them **last**-wins. Generator now rejects duplicate and non-integer opts |

**Note on #14's rule.** Colliding keys are disambiguated by **content hash**, so
byte-identical records still collapse (real deduplication) while records that
merely share a key are all kept. The engine never invents a distinction the data
lacks, and never merges records that differ. The underlying trap: `id_keys` is a
*manifest declaration*, not the upstream's identity — declaring a dimension
(`id_keys=country_code` on a weekly time series) as the record id silently
discards the series.

### Second wave — the multi-agent sweep

| # | Issue | Evidence |
|---|---|---|
| 16b | **`ised-spectrum-sites` punched holes through its own walk.** It requested `resultRecordCount=2000` and strode `resultOffset = i * 2000`, but the layer's `maxRecordCount` is **1000** — so each page returned 1000 while the offset advanced 2000, and records 1000–1999 and 3000–3999 were **never requested**. Not a cap: gaps, with the run reporting success. Now strides by what the server actually returned, and the bound is disclosed with the measured total — `used 3000 of 843979 available records` |
| 16c | **`nsidc-sea-ice-extent` fetched 1.8 MB and kept 45 rows.** `NSIDC_TAIL 45` against a complete daily series since 1978 (15,816 rows/hemisphere). `audit-sources` does not match this cap shape, so nothing flagged it. The `remote_key` is `<hemisphere>\|<date>` — stable and unique — so a full emit upserts cleanly and only the first run pays. **45 → 31,630 records**; second run stores 31,630 with the table unchanged, confirming no growth |
| 16d | **~18 threat-feed collectors degraded to a silent `records=0`** when `ABUSE_CH_AUTH_KEY` was unset — one shared gate in `lib/threatintel.c`, so it was eighteen sources failing invisibly at one call site. Now emits the tree's single `collector-status-notice` shape (constant `remote_key`, so one row however long it stays unconfigured; no observation in the row) and still returns 0, because "gated" is a state, not a run failure. Verified: `urlhaus-jp` and `threatfox-jp` each emit exactly 1 notice |
| 16e | **Breach `keyid` truncated a SHA-1 to 10 hex digits** — see 16a |
| 16f | Two lint counts improved but the baseline still carried the old numbers, so the gate could have silently drifted back up. Ratcheted: dup-endpoint 528 → **484**, geo-precision 118 → **116** |
| 16g | **`core/camera_store.c` merged two halves of one record by opposite rules.** `spread_into(m, p); spread_into(m, pp);` lets the STORED value win for every key not named in the overrides — right for what a row accumulates (`first_seen_at`, `seen_count`), wrong for a value the collector RECOMPUTES each run. `geo_precision` was unnamed, so when the cam-centroid unification corrected eleven camscape anchors from `"prefecture"` to `"city"`, that fix would have been **inert for every camera already stored, forever** — while the geometry beside it was being rebuilt from fresh coordinates on the same run. `geo_precision` is now fresh-wins with the stored value as fallback |

Plus, from the agents: **time formatting unified** (15 named helpers + ~60 inline copies → one `_timefmt.inc` across 78 files; **77 sites** were ignoring `gmtime_r`'s NULL and `strftime`'s 0, and four used non-reentrant `gmtime()` whose NULL was dereferenced — a crash and a data race). **`fcc-3650-locations` 3,993 → 7,829 stored** and **`ised-spectrum-sites` 973 → 3,000** from wrong composite keys. **XML attributes** are no longer dropped, which unblocks the whole SDMX family. **EU sanctions** now screen only the name columns (measured on UK OFSI: 708 rows contain "Putin" anywhere, **11** in the name columns).

### API (house rule 2 and security)

| # | Issue |
|---|---|
| 18 | `/api/alert-events` and `/api/alerts/:id/events` capped at 500 with no total and no cursor — a 500-row reply over a 700-row inbox looked complete and rows 501+ were unreachable |
| 19 | Four more capped endpoints with no disclosure, found by grepping every hard cap: `entityapi.c` ×3, `miscapi.c` |
| 20 | **Arbitrary local file read** — `/api/admin/breach/catalog/preview?path=/etc/passwd` returned the file's parsed contents in `sample[]`; `/ingest` and `/catalog/load` shared the unconfined read |
| 21 | **SSRF** — `/api/admin/breach/fetch` used the non-strict host gate and reached loopback |
| 22 | Bodies over ~3 MB reset the connection instead of returning 413 |
| 23 | `/api/intel/search` silently returned the whole corpus when a query sanitised to only FTS metacharacters, while echoing the query back as if applied |
| 24 | `max_rounds` unclamped on `/api/search/analyze`; the `(int)` narrowing of a double outside int range was UB |

### Verified clean

- **ASan**: 863 live collector runs, 0 errors. **TSan**: 600 s scheduler soak, 0 races.
- Contract fixtures: all 19 byte-identical to HEAD.
- Batch 18/19 accounting reconciles exactly (1810 − 118 rejected = 1692 shipped, 0 unexplained).
- Web client: 12 tests pass, production build clean.

---

## OPEN

### Data loss still on the table

| # | Issue | Size |
|---|---|---|
| 25 | **102 sources still emit zero** — 31 HTTP-status refusals, 20 fetch failures, 11 credential-gated, 5 "no Overpass endpoint answered", 35 empty-or-unparsed | of 1,197 sampled |
| 27 | jsonlist/hpengine collision guards are **per page**. A record on page 2 keying onto one from page 1 is not caught; catching it means buffering the whole walk | documented limitation, not a regression |
| 28 | 108 of 2,188 manifest rows "declare no pagination but look paged" | heuristic; each needs a read |
| 28a | `content_change.c:1384` builds the evidence uid as `"cc:%.80s\|%.180s"` (source_id, ref key). A ref key longer than 180 chars — an ordinary URL with query parameters — is cut, so two watched refs sharing a 180-char prefix share one evidence uid and one overwrites the other. Same class as #15/#16a. **Not fixed:** lower severity (needs two long, prefix-sharing refs on one source) and I could not exercise the content-change path end-to-end to verify a change, so I did not make one I could not prove. The established remedy is to hash rather than cut, keeping short keys byte-identical so nothing correctly stored is re-keyed | analysed, unverified |

### House rule 1 violations

| # | Issue |
|---|---|
| 29 | **12 sources degrade to a silent `records=0` when a credential is missing** instead of an explicit note: `abuseipdb-jp, bear-encounters, estat-employment, facebook-geo, fofa-jp, jstat-map, msil-umishiru, odpt-train, resas-population, sentinel-japan, softbank-crowd, wifi-networks-wigle` |
| 30 | An **HTTP-200 error document is stored as a finding** — ArcGIS answers over-quota with 200 + `{"error":{"code":429}}`, the "root IS the record" fallback fires, and the engine files a record titled `airway-record 429`. `probe_hp_batch.py` guards this shape; the engine does not |

### Engine limitations

| # | Issue |
|---|---|
| 31 | `hp_xml_flatten` walks child elements only and **drops XML attributes**. Makes the whole SDMX structural-metadata family (ILO/OECD/ABS/Istat/ECB/Eurostat/IMF, all verified live) unreachable, since identity lives in `id=`/`agencyID=` |
| 32 | `sanctions_world.c`: **EU_SANCTIONS is screened over its entire row including narrative columns.** The live export is semicolon-delimited with names at columns 17–20; `csv_name_cols` means "leading comma-separated columns" and cannot express that. Same false-positive class the code already documents for OFSI ("Putin" → 708 records). A false positive in a sanctions screen is a person wrongly flagged |
| 33 | `AU_DFAT` unverifiable — `dfat.gov.au` refused every connection from this host |
| 34 | `%.80s` source-id caps at `httpd.c:1198` and ~1245 silently name a different source than the one acted on |

### Tooling that under-reports

| # | Issue |
|---|---|
| 35 | **No `audit_batch_emit.py` equivalent for the `vsrc*` fleet.** Rule 4 is enforced for hpengine batches only; the generated fleet was verified by fetch-probe alone — which is exactly why 260 of 1,197 sat at zero |
| 36 | **`records=N` cannot see stored-row loss.** It counts `emit()` calls. Surfacing `stored` in `fetch_log` would make rule 4b enforceable |
| 37 | `batch_exclusions.py` blind spots: it harvests only literal string URLs, so it cannot see the 14 endpoints `av_faa_arcgis.c` composes with `snprintf` (batch 20 nearly shipped **42 duplicates**); and `--skip-prefix hp3_` hides batch 18 from `--dump-urls`, which is what a discovery pass reads |
| 38 | `audit_batch_emit.py`'s fixed 180 s cap is reported identically to a real defect |

### The full-registry retest — what it measured

`docs/source-health-2026-08-24.tsv` / `.md`. **13,178 of 13,193 registered
sources measured** (15 were lost to a raw newline in a record title breaking the
TSV's line framing — see #53). Buckets:

| bucket | rows | share |
|---|---:|---:|
| healthy | 9,843 | 74.7 % |
| fetch-failure | 1,270 | 9.6 % |
| **KEY_COLLISION** (rule 4b) | **1,053** | **8.0 %** |
| empty-unproven | 365 | 2.8 % |
| not-exercised | 243 | 1.8 % |
| emits-nothing-upstream-has-records | 183 | 1.4 % |
| credential-gated | 98 | 0.7 % |
| timeout | 62 | 0.5 % |
| honest-empty-proven | 49 | 0.4 % |
| pods (not sources) | 12 | 0.1 % |

**`DROPS_EVERYTHING`: zero** — not one source emitted > 0 and stored 0. Of
11,450,709 records emitted, 10,752,480 stored: **698,229 lost per pass**,
581,909 of them to uid collision.

Note the honesty of the empty buckets: only **49** rows are *proven* honest
empties (stored 0 and an independent fetch of the same endpoint also returned
0). The 365 `empty-unproven` are explicitly **not** counted as honest.

#### 16h — the collision guard had a blind spot, and it was mine

`hp_collision_map()` computed `kp = rkey ? rkey : title` and **skipped a record
where both were NULL**. `hp_emit_record()` does not — it falls back to
`hp_first_scalar()`. So a row declaring neither `id_keys` nor `title_keys` was
the one shape the guard could not see, and every record whose first scalar was a
dimension constant collapsed onto one uid, unguarded. **1,818 registered hp rows
are in that shape.** `WHO_XMART_NCD_MORTALITY` emitted 10,001 and stored **2**.

Fixed by mirroring the emitter's fallback exactly, and pinned by hptest 12d.
Re-measured: **10,001 emitted → 10,001 stored.**

The lesson is general enough to state: **a collision guard that derives its key
differently from the code it guards is not a guard.** That is now a comment at
the site.

#### Also fixed from the retest

* **176 rows given a measured identity** — for each collision row declaring no
  keys, a live page was fetched and each field's distinct-count and coverage
  measured before choosing `id_keys`/`title_keys`. **+360,213 stored rows per
  pass**, −897 (all nine regressions upstream drift/429). `OONI_DOMAIN_INDEX`
  31→29,605 · `CO_XM_SIMEM_DESPACHO` 48→24,192 · `D3FEND_MAPPINGS` 1→16,164.
  Two proposals were rejected on purpose (a measure and a dimension) and five
  geometry-derived titles reverted after measuring worse than the fallback.
* **The three `\;`-corrupted rows**, plus a defect each one hid — repairing
  `UA_DREAM_PROJECTS` *activated* a detail hop whose `detail_url` used `{q}`
  instead of `{v}` (25 identical 404s). 22 other `\;` uses were checked: 19 are
  legitimate escaped semicolons inside header values.
* **`www.oasis-open.org`** added to the per-host UA table — same "OSINT" token
  block as HDX; measured 520 with the engine agent, 200 with the honest
  replacement.

### Routed but not taken — the dead-canonical-copy family

Found by the unification sweep, deliberately not changed by it (out of its
scope), and not yet taken. Each is the same anti-pattern: **a file that declares
itself canonical while the copies it was meant to replace are still in use.**

| # | Issue |
|---|---|
| 47 | **`core/` has 14 copies of `static void iso_now()`** (`alertsapi, camera_store, casesapi, dataapi, entityapi, exportapi, httpd, intel, intelapi, miscapi, reportapi, statusapi, sweepapi, timelineapi`) plus `lib/probe.c` — all ignoring `gmtime_r`'s NULL and `strftime`'s 0. `lib/jocore.h:jo_iso_now()` is already the canonical one and **nothing in `core/` calls it**. 35 unchecked sites across `lib/`+`core/`. **Lower severity than the collector case that was fixed:** these all convert `time(NULL)`/`clock_gettime`, where `gmtime_r` cannot fail — the collectors' epochs came from upstream and were unbounded. Worth unifying for the duplication, not urgent for correctness |
| 48 | `lib/jocore.h` itself: `jo_iso_now()` ignores both returns; `jo_days_ago_iso()` checks `gmtime_r` but not `strftime`. These are the tree's most-called time helpers |
| 49 | `lib/camfeature.h` declares itself "the ONE camera-Feature constructor" and then records that the fourteen `cam_*.c` keep their local copies. Either convert them or stop calling it canonical |
| 51 | `lint_sources.py`'s `quarantine-empty: 11` are **all false positives** — the check matches `return fetched > 0 ? 0 : -1` (correct) as well as `return n > 0 ? 0 : -1`. It should test only against an emitted-record count. Nothing to fix in the collectors |
| 52 | **dup-endpoint 484** is the largest open registry-duplication backlog — 484 pairs of registered sources pointing at the same endpoint |
| 53 | **A raw newline in a record title breaks TSV line framing** — 15 sources could not even be reported by the health sweep because of it. A title is upstream text and must be sanitised before it reaches any line-oriented output |
| 54 | **86 rows walk N pages that all return page 1** — `stored == emitted / N` exactly, which is the signature. 69,041 records/pass. `stats-pt-ine-indicator` fetches the same page 57 times; `PY_CGR_*` 10×; the five `FAA_ARCGIS_*` ~10×. Either the page parameter is ignored by the server or it is the wrong parameter name — each needs a probe |
| 55 | **37 rows declare a `detail_url` with no `{v}` token** (32 use `{q}`, 4 a literal `{}`, 1 `{qU}`). The hop fires and fetches the same URL for every record — one was measured producing 25 identical 404s. A `detail_url` without `{v}` cannot be per-record and should be rejected at generation time |
| 56 | **513 registry rows have `interval=0` and no entity token** — registered, never scheduled, never dispatchable (house rule 3). Note the earlier "0 of 2,188 rows can never run" was **manifest rows only**; `audit_batch_reachable.py` cannot see rows that are not in a manifest, so the registry-wide number was never checked until now |
| 57 | **6 hosts confirmed to block the engine's User-Agent.** One (`www.oasis-open.org`) was fixed. **`registry.faa.gov` was reported as fixable and is not** — it returns 403 to the engine agent AND to the honest replacement, and 200 only with no User-Agent at all. It refuses every self-identifying client. The per-host table exists to route around a filter objecting to one WORD in an otherwise honest agent, not to stop identifying ourselves; browser spoofing is ruled out by `httpclient.h`. Left honestly failing. The other four are the same shape. 55 further hosts return 403 to all three probes — genuinely refusing us |
| 58 | **3 ArcGIS rows stored a record titled `… 429`** (`PORTWATCH_DAILY_REGIONAL`, `PORTWATCH_MARITIME_LINKS`, `FAA_AMD_PENDING_BUILDING`) — the HTTP-200-error-document class. The engine fix for it landed this session; these three predate it and the condition is **load-intermittent** (a 40-way burst returned 40×200 with real data), so they could not be re-triggered to confirm the fix covers them |

### Coverage gaps

| # | Issue |
|---|---|
| 39 | Web client: **15,263 lines across 54 files, 3 test files.** `MapView.jsx` alone is 4,926 lines with no tests |
| 40 | iOS targets cannot be built or tested here (no macOS); CI's `ios-build` is their only proof |
| 41 | 118 `geo-precision` lint advisories (baselined): collectors emitting geometry with no `geo_precision`, so a point's meaning — building vs country centroid — is unstated |

### External, not ours

| # | Source | State |
|---|---|---|
| 42 | `afr-eg-madamasr` | Cloudflare 520 for our UA, 200 for a browser UA. Not fixable without spoofing, which this codebase rejects |
| 43 | `afr-mz-jornalnoticias` | WAF 406 for any non-browser UA; 200 only with no UA at all |
| 44 | `afr-ekur-*` | ArcGIS endpoint times out at 40 s+; host root answers in 1 s |
| 45 | `afr-moh-gh` | Host unreachable from here with every UA |
| 46 | `sslbl-jp` | Upstream **retired the feed** — 200 with `# ATTENTION: This list has been deprecated on 2025-01-03` and zero rows. Should be retired or repointed |

All of #42–45 are reported honestly by the engine as anomalies, which is the
correct behaviour.

---

## Environment notes

- **WSL is the only build host** — no compiler on Windows. Build under `$HOME`,
  not `/mnt/c` (10× slower) and not `/tmp`.
- `wsl.exe -d Ubuntu-24.04 -- bash -lc '...'` **silently eats shell variables**;
  pipe scripts on stdin with `bash -s <<'EOF'`. A heredoc also loses one level
  of backslash escaping — writing C strings containing `\n` through one
  corrupted a source file during this audit. Use an editor for those.
- The Windows host filled to **0 bytes free** during this session (a sweep
  harness grew a 14 GB SQLite WAL in two minutes), which wedged WSL entirely.
  Deleting files inside WSL does **not** return space to Windows — the virtual
  disk only shrinks with an offline compaction. Watch `df -h /` during long
  runs, and cap or checkpoint scratch databases.
