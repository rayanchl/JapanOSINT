# What still needs fixing — 2026-10-20

Every item below was checked against the tree today, not remembered. Where a
number differs from CLAUDE.md, the number here is the one `make` prints.

Ordered by how much it costs to leave alone.

---

## A. The gates can no longer be read, which blocks everything else

> **A1 and the audit-sources half of A2 are FIXED** (same day). The scanner now
> knows the four walkers and the `source_def.url` rule, and `make audit-sources`
> reports 0 findings across the whole tree again, strict set 0/342. Fixtures 5
> and 6 pin both, and were checked against the old behaviour. The rest of A2
> and sections B–E stand.
>
> **A2 is FIXED too**: every drifted number corrected, including the batch-37
> line, which had claimed for a fortnight that a probed-and-generated set was
> "still not generated into C, still unprobed".

### A1. `make audit-sources` reports 50 findings where CLAUDE.md claims 0

```
strict set (the gated glob): 342 files, 0 findings      <- still clean
the rest of the tree:         27 files, 50 findings     <- CLAUDE.md says 0
    single-page  49
    record-cap    1
```

CLAUDE.md's whole argument for holding the non-strict scope at zero is that it
"makes any NEW finding a regression rather than a number in a backlog". That
property is currently gone: nobody can tell which of the 50 is new.

**They are false positives, verified on one.** `od_ckan_ch_zurich.c` pages
through the shared `od_ckan_collect()` → `pw_walk`, and carries an
`exhaustive-ok` marker on its `CKAN_PAGE_MAX` line — but its two `.url` lines
have no marker, and the scanner matches per line and cannot see into the shared
helper. The 26 files are the CKAN / ODS / ArcGIS portal callers fixed on
2026-10-12, plus `eni_bc_mineral_tenure.c`.

**Fix:** either put a one-line `exhaustive-ok` on each flagged `.url` line
(cheap, and exactly what CLAUDE.md prescribes for a scanner false positive), or
teach the scanner that a row whose `run()` calls `od_ckan_collect` /
`od_ods_catalog_collect` / `pw_walk` is paged. The second is better — 26 files
of markers is 26 chances to mark something that is genuinely broken.

**Do not just re-record the number.** Confirm the `record-cap` finding is also
benign first; it was not identified today.

### A2. CLAUDE.md's measured numbers have drifted

| claim in CLAUDE.md | what the tree says now |
| --- | --- |
| strict set "0 findings across 308 files" | 342 files |
| "the rest of the tree — 0 findings (1,678 files)" | 27 files, 50 findings (A1) |
| gated set "290 files as of 2026-10-05" | 342 |
| "554 keys" (collectors) | 593 |
| batch 37 "still not generated into C, still unprobed" | **generated and probed** — commit `5d5e1f99`, "15 of 80 pass, 16 rows measured", 70 rejected into `docs/rejected-sources-batch37.*.tsv` |
| `audit_url_limits.py` "89" | 70 |

The batch-37 line is the one that matters: a future session reading CLAUDE.md
would re-probe a set that has already been probed and rejected.

---

## B. Rule-4 debt — registered rows with no emit reading

### B1. ~~Batches 34 and 35: 239 rows never measured~~ — DONE

**Measured, same day: `docs/BATCH_34_35_EMIT_AUDIT_2026-10-20.md`.** 115
scheduled rows through the registry sweep (92 OK, 20 refused, 3 collisions) and
124 pivots through the manifest audit (73 OK, 40 EMPTY_UPSTREAM, 11 upstream
errors) — the two tools the two halves need, since the sweep's own
`NEEDS_ENTITY` covers all 124 pivots by design. 33,108 emitted, 33,108 stored,
**0 lost to uid collision**.

Eleven rows repaired or removed, each on a measurement: six hosts whose filter
rejects one WORD of the engine's User-Agent (0 → 96/186/397/166/963/1,823
records, and the tree already knew host by host in per-ROW headers), four rows
re-pointed from a refused bulk action to `package_search` on the same CKAN, and
two removed — a `data.gov.uk` row that is a proxy in front of the row beside it,
and an FDSN event row on a node that does not run the event service. Registry
48,794 → 48,792.

The original item read:

### B1 (as written). Batches 34 and 35: 239 rows never measured

139 `JO34_*` and 100 `JO35_*` rows. `docs/verified-sources-batch3{4,5}.tsv`
exist but describe only the small measured beats (`ndjson`, `strings`) — the
hazard CLAUDE.md flags as "read the BEAT, not the batch".

They were unverifiable when authored because that session's network policy
reached only package registries. **That is no longer the constraint**, and two
things fixed today remove the rest of the excuse: the shared TLS context (a
strict default context was calling live hosts dead) and the pinned binary (a
rebuild mid-audit silently voided 229 of 290 verdicts).

```sh
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO34_' --jobs 4 --timeout 220
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO35_' --jobs 4 --timeout 220
```

### B2. Batch 31's 90 refused rows have never been re-run

`docs/emit-sources-batch31.tsv` records 48 answering 404 and 42 answering 403.
They were deliberately kept, correctly, because "one probe is one reading" —
but CLAUDE.md's own rule is "re-run each FETCH_FAILED once; a row that fails
twice leaves", and the second reading was never taken. A retry pass on batches
44–46 recovered 63 of 2,724, so the expected yield is real but small.

---

## C. Rows with a measured shortfall and a known next step

### C1. KKJ's two entity pivots read 1,000 of 62,255

`JP25_JPBIZ_KKJ_QUERY` and `_ORG` are bounded at the portal's hard `Count` cap.
That is legitimate for a pivot and is now disclosed, and the order is
newest-first so the bound is a window rather than an arbitrary slice. But the
47 prefecture rows now read the same corpus exhaustively by date, so the pivots
could slice per entity the same way instead of bounding.

### C2. Toyama's dump days are unreachable by any means currently available

Single days of **1,449** and **1,161** notices against a per-read cap of 1,000,
measured day by day. No `window_days` fixes it — the cap is per READ. Reaching
them needs a different route (a bulk export, or a filter that subdivides within
a day). ~460 records.

### C3. Tokyo's KKJ backfill takes ~132 days

4-day windows, 530 of them, at a 6-hour cadence. Deliberate and disclosed, and
the notice says where it is. On the list so nobody reads it later as a stall.

### C4. Eight cycling rows still carry a declared descending sort

CLAUDE.md: a descending sort and a cycle do not compose, and the fix is an
ascending sort **verified per host**. 22 rows were done that way on 2026-10-18;
these 8 were deliberately left rather than changed on reasoning. Each needs its
own host checked.

### C5. ~~`PLOS_RECENT` wants a sibling~~ — DONE

417,719 records against a 20-page ceiling. It deliberately does not cycle —
recency is its purpose — so the whole corpus needs a second row with an
ascending sort.

**`PLOS_CORPUS` ships.** `publication_date asc,id asc`, `page_resume = 1`, and
`page_max = 50` because a 50-page burst was MEASURED on api.plos.org (50× 200 in
16.0 s, no rate-limit headers) rather than extrapolated from the default 20.
Deep paging reaches `start=400000` in 1.1 s, so there is no cap to work around
and no hostgate gap was invented. Verified over two runs against one database:
5,000 records a run, 10,001 distinct after the second, no overlap, dates
2003-08-18 → 2008-11-10 — the oldest end of the corpus, walking forward. Full
cycle ≈ 21 days. 418,142 by PLOS's own `numFound` today.

### C6. ~~`JO32_MUNI_YAMAGUCHI_CKAN` became reachable today~~ — REFUTED, and it found a bigger gap

It was rejected as a TLS failure. With the shared context it now reaches the
host and answers **HTTP 409**, not a TLS alert — a CKAN `datastore_search`
missing its `resource_id`. That read as "a URL defect, which is fixable: one
recoverable source". **Measured, it is not a source at all**, and the thing it
pointed at is already read exhaustively:

* the candidate was a HOST-level placeholder from batch 32's generation, with no
  `resource_id` because no particular resource was ever chosen. There is nothing
  to re-point;
* `yamaguchi-opendata.jp` publishes **20 organisations and 788 datasets**
  (`organization_list` and `package_search?rows=0` agree), and the tree holds
  **all 20** organisation rows plus the org list and the search pivot — an exact
  set match, measured id by id;
* run serially through the emit audit, every one of those 20 rows stores
  **exactly** the `package_count` the portal declares for it (102/102 for the
  prefecture, 76/76 Ube, 47/47 Shimonoseki …): **808 emitted, 808 stored, 0 lost
  to collision**. Dataset-level coverage of that host is complete.

**What the 409 did surface is a layer the tree does not read anywhere**: the
portal's datastore holds **799 tabular resources**
(`datastore_search?resource_id=_table_metadata` → `total: 799`), i.e. the ROWS
inside the datasets, where every CKAN row in this tree reads only the dataset
METADATA. That is batch-shaped work rather than a repair — one row per resource,
each named from `package_search`, with counts measured per row — and it
generalises to every CKAN with the datastore extension enabled, which is most of
the ones already registered. `_table_metadata` itself is NOT a source: its
records carry a UUID and an oid and no title.

Filed as an opportunity, not a defect. The item as written was a false positive
of my own: it inferred a repairable row from an error code without asking what
the row was meant to be.

---

## D. Unmeasurable from this container — record, do not guess

### D1. 31 rows fail certificate verification, for the engine too

`docs/tls-rejects-revisited-2026-10-20.tsv`. Untrusted or incomplete chains,
which a cipher security level does not address and which must not be bypassed.
Measured on six (IODA ×3, Dataverse UCLA, NADA Nepal, NI MARENA): each answers
`rc=-1` and files a `collector-fetch-failure`.

**This needs a decision, not a measurement**: keep them disclosing, or remove
them under "fails twice leaves". They are currently honest and useless.

### D2. A set of hosts answer HTTP 000 through this container's proxy

Measured one at a time, with no concurrency: `odre.opendatasoft.com`,
`opendata.bordeaux-metropole.fr`, `bodacc-datadila.opendatasoft.com`,
Dataverse KU Leuven and others. The proxy names the cause itself —
`ws_closed_mid_exchange` — and the hosts are not denied by policy. So these
rows are unmeasured **from here** rather than broken, and a sweep run from
elsewhere would settle them. The final page-2 sweep is quantifying how many.

### D3. Archive-It's rate-limit budget cannot be measured from behind the limit

`partner.archive-it.org` answers `<title>Rate limit reached</title>` with HTTP
**200**, an hour apart, to single requests. The engine now names that shape and
points at `core/hostgate.c` — but deliberately no gap was added, because a gap
chosen without measuring the budget is a guess.

### D4-bis. The "it's the client" question is ANSWERED for one family

D4 below says the remaining difference between a host that serves a hand request
and refuses the engine is **the client, since the engine sends its own headers**,
and that nobody had measured it. Measured now, on a different family: six US
municipal CKAN portals answer **502 to the engine's User-Agent and 200 to the
same agent with the word "collector" removed**, six for six, reproducibly, and
serially — so concurrency was ruled out first. Entries added to
`core/httpclient.c`'s UA_OVERRIDE table; 0 → 3,631 records.

That does not settle `data.gov.au` below, which is a different host and may be a
different filter. It does say the bisection is cheap, it works, and it belongs
before any 4xx/5xx row is written off.

### D4. The `au-asic` 403s are not concurrency, and the next step is named

Five rows share `data.gov.au`; host-paced, two cycle, two read FETCH_FAILED and
one STUCK. By hand the host served 8 rapid serial then 12 concurrent requests,
all 200. CLAUDE.md already names the remaining difference: **the client**,
since the engine sends its own headers. Nobody has measured that.

### D5. `trickest_cve` — already recorded as unmeasurable; do not re-litigate.

---

## E. Correctness nobody has measured

### E1. Every embedding threshold is a guess from the literature

CLAUDE.md says it plainly: entity 0.80, story 0.85, alert 0.82, relevance, the
resolver margin — "**none has been measured on the production model**". They
were chosen from typical multilingual-e5 scores and tested against a stub.
Each one decides whether two entities merge, whether two items are one story,
and whether an alert fires.

### E2. One sortBy-less WFS row is unaccounted for

Of 16 other WFS `startIndex` rows without `sortBy`, CLAUDE.md records that "12
of the 13 reachable ones serve page 2 fine". The 13th is not named, and neither
are the 3 unreachable. Four rows with no recorded verdict.

---

## The re-run finished — `docs/page-two-final-2026-10-20.tsv`

**654 of the 818 (80%) are healthy** — 350 OK, 304 PAGE1_IS_ALL — where the
first run had written every one of them off. That is the measure of what the
saturated egress and the strict TLS context were doing.

Findings acted on: `LUCHTMEETNET_LKI` 100 -> **604** (its bare path 302s and the
redirect drops the page parameter; its `id_keys` was wrong too, and invisible
while it read one page) and `JO34_CKANRES_DATENREGISTER_BERLIN_DE` 2,000 ->
**2,627**. `UKPARL_LDA_SESSIONS` was benign but running it surfaced a DEAD
`id_keys` matching 0 of 12 records, now keyed on `_about`. Three more were
complete as they stood.

**73 rows remain LOCAL_EGRESS** — unmeasured from here, which is item D2, not a
defect in those rows.
