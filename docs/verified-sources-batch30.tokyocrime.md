# Batch 30 — `tokyocrime`: the Tokyo Metropolitan Police statistics section

82 rows, **361,030 records emitted and 360,559 stored** in one pass, measured
2026-09-21. Manifest: `docs/candidate-sources-batch30.tokyocrime.txt`. Table:
`native/collectors/pivot/table/hp30tokyo_tokyocrime.c`. Per-row sweep results:
`docs/verified-sources-batch30.tokyocrime.tsv`.

This is the 警視庁 (Keishichō) statistics section, which no row in the tree
reached before — and which the tree had explicitly written off. See "How this
was nearly missed" below.

## What it is

| Series | Rows | Records | What one record is |
|---|---|---|---|
| `ninchikensu` — 区市町村の町丁別、罪種別及び手口別認知件数 | 17 | 83,195 | One **chōme** (city block), with 36 offence counts: 凶悪犯 robbery, 粗暴犯 assault/injury/intimidation/extortion, 侵入窃盗 by method (safe-breaking, school, office, storefront, empty-house, sneak-in, occupied-room), 非侵入窃盗 by method (vehicle, motorcycle, bicycle, theft-from-vehicle, vending machine, worksite, pickpocketing, snatch, left-property, shoplifting), その他 fraud/embezzlement/gambling |
| `hanzaihasseijyouhou` — 犯罪発生情報 | 56 | 255,718 | One **reported incident**: offence and modus, the police station and kōban covering the scene, municipality code, ward/city, chōme, date and hour of occurrence, and case-specific fields (victim sex/age/occupation, locking state, anti-theft device, principal property taken) |
| `zencho` — 前兆事案認知情報 | 9 | 22,117 | One **precursor incident** — the approach calls and doorstep visits that precede special fraud (特殊詐欺): type, month, weekday, hour, ward, place and place detail, victim sex/age/occupation, suspect's mode of transport |

Coverage: the aggregates run 2017→2026 (平成29年 through the August-2026
cumulative), the incident files 2018→2025 across seven offence types, the
precursor files 2024→2026.

The incident series is the valuable one and is easy to undersell: it is
**incident-level, chōme-granular, geocodable crime data**, not an aggregate. The
aggregates tell you how many bicycle thefts happened in a block; the incident
files give you each one with its date, hour, kōban and locking state.

## Evidence

Every gate, with its number, in the order it was run:

| Gate | Result |
|---|---|
| `probe_hp_batch.py` (proof of life) | **82/82 PASS** — 2xx, parses as CSV, ≥1 real record |
| `batch_exclusions.py --bin` | **0 collisions, 0 near-misses** against all 16,792 registered ids |
| `audit_batch_reachable.py` | **0 of 82 rows can never run** (house rule 3 — all are static bulk files, so all declare `interval`) |
| `audit_batch_pagination.py` | **0 of 82** look paged without declaring it |
| `gen_hp_batch.py` | 82 of 82 rows kept by the proof-of-life filter |
| `make` | OK, **0 warnings** |
| `selftest` / `unit` / `hptest` / `audit-sources` / `pagewalktest` / `source-floor` | all **OK** |
| `lint-sources` | **OK** — `dup-endpoint 425 at per-file baseline`. It failed transiently while this beat was being verified, on another beat's file (`hp3b30_jplibrary.c`): 48 NDL OAI rows share one `resumptionToken={v}` continuation template, and a new file's per-file baseline starts at 0, so the first collision tripped the gate. Its owner confirmed the 12 OAI sets are distinct with no overlap against the 20 in `hp3b23_jpnational.c` and baselined it. **This beat contributed 0 findings** to that check at every point — all 82 rows point at one host with 82 distinct `.url` values |
| `audit_batch_emit.py` (rule 4) | **OK=81**, 1 `TRANSPORT_FAIL` — a TLS reset under a concurrent `make -j`, re-run individually and clean. No row fetched records and emitted none |
| `audit_registry_emit.py` (rule 4b) | **emitted 361,030, stored 360,559, lost 471, unexplained 0** |

## The 471 "lost" records are dedupe, not discard

Every one of the 471 was cross-checked against a duplicate measured in the file
itself, and **none is unexplained**. The beat happens to demonstrate both halves
of the collision guard doing their separate jobs:

* **Byte-identical duplicates collapse — real dedupe.** `R7.csv` publishes
  `神津島村` twice and `R5.csv` publishes `西多摩郡檜原村` twice, with every one
  of the 37 columns equal. Each stores once. The incident files carry the same
  shape (two identical bicycle thefts in one chōme, same date, same hour, same
  victim demographics): `BICYCLE_2024` 28,831 → 28,779.
* **Records that merely share a key are all kept.** `R8.csv` also repeats
  `西多摩郡檜原村`, but with **totals 8 and 10** — different records under one
  name. It stored **5,156 of 5,156, with no `UID-COLLISION` line at all**: the
  guard content-hashed them and kept both.

That second case corrected a prediction of mine. I forecast stored from
*distinct key* for the aggregate files and from *distinct whole line* for the
incident files, and only the second is what the guard actually keys on — so the
aggregate forecasts were lower bounds and came in four high in total. The
manifest header records the correction.

## Three shapes that needed engine features

* **`R8.3.csv` is published in a spreadsheet layout** — a title banner, a ward
  line and a three-row merged header occupy lines 1–7, with records from line 8.
  `csv_skip_lines=7` drops physical lines *before the header is read*, so
  skipping 7 alone would have consumed the first real record as the column
  names. Paired with `csv_no_header=1` it reads positionally: 4,472 records,
  4,471 stored, nothing eaten.
* **`R8.5.csv` is UTF-8 with a BOM** while the other 16 are Shift_JIS. It is the
  only row in the beat that declares no `charset`, and it is the row that
  exercises the BOM strip in `lib/csv.c` end to end: 4,877 → 4,876.
* **No file has an id column.** Identity is the tuple of every dimension the
  file publishes, composed with `+` (which JOINS) and never `,` (which CHOOSES):
  the 10 columns common to all 56 incident files, and all 11 for zencho. At 252
  and 172 bytes both clear `hp_pick_s`'s `buf[512]`, and an over-long composite
  becomes FNV hex rather than being truncated into manufactured collisions.

## How this was nearly missed

All three series were already in the tree — as *rejections*.
`docs/rejected-sources-batch28.jppolice.tsv` recorded five keishicho URLs as
"Unreachable — DNS resolution failure (getaddrinfo ENOTFOUND)".

The observation was correct and the conclusion was not. The host written in that
file is `www.keishicho.metro.tokyo.jp`, which genuinely is NXDOMAIN. The site is
served from `www.keishicho.metro.tokyo.**lg**.jp`. One missing token retired the
publisher of ~344,000 rows, and the rejection read as authoritative because the
DNS failure behind it was real.

All five re-probed at HTTP 200 on 2026-09-21; three are now registered here, and
the two remaining (`toukei/johomap/johomap.htm`, `jiken_jiko/hassei/map_annai.html`)
are live but not yet assessed as record sources — reachability established,
shape not. The five reason cells now say all of this.

The general form is worth keeping: **a DNS failure is evidence about a hostname,
not about a source.** "The host does not resolve" only becomes a statement about
the publisher once the hostname itself has been checked.

## Caveats, stated rather than buried

* **Probe record counts taken before 2026-09-21 are floors.** `count_csv` in
  `verify_feeds.py` parsed only the first 200,000 characters; this beat is what
  exposed it (2,320 counted of 5,156 real). Fixed, and the counts above are
  post-fix.
* **One row's batch-audit verdict was a transport failure, not a defect.**
  `CHOME_R8_5` returned `TRANSPORT_FAIL` while a `make -j$(nproc)` had the box at
  load 21 and the host was resetting TLS handshakes. Re-run alone: `records=4877
  stored=4876`. No row in this beat is unmeasured.
* **Intervals are deliberate, not uniform.** The rolling current-year files
  (`R8.x`) refresh weekly (604800); the closed years refresh monthly (2592000).
  A frozen 2017 file does not need a daily fetch, and rule 3 only requires that
  the interval be non-zero.
* **The aggregates include publisher summary rows.** The last three lines of
  each `ninchikensu` file are `海外認知` (overseas), `不明` (unknown) and `合計`
  (grand total — 72,017 offences in the August-2026 file). These are real
  published rows and are stored as such; they are not chōme, and each row's
  description says so.
