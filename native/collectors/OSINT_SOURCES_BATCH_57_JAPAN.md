# Batch 57 — Japan: what 6,983 public-body hosts declare

2026-10-30. Eight beats, `collectors/pivot/table/hp3b57_*.c`, generated from
`docs/candidate-sources-batch57.<beat>.txt` by `tools/gen_hp_batch.py`.

The brief was "add 3,000 Japanese sources and try them". The number is the easy
part; the five things below are what the batch is actually worth, and four of
them are mistakes caught before they shipped.

## Where the pool came from

Not from probing paths. Japan has ~1,747 municipalities and several thousand
public institutions, and every one of them publishes its own list of what it
serves — so the method is batch 42's and batch 46's, one level out: **ask the
publisher's own homepage what it declares.**

The seed is Wikidata's official-website property (P856), and **every class id
was resolved through Wikidata's own search API rather than recalled**, because
a wrong QID returns an empty result set that reads exactly like a class with no
members — a filter matching nothing, which this tree has recorded four times.

| pass | classes | hosts | answered 200 | feed candidates |
| --- | --- | --- | --- | --- |
| administrations | city, town, village, ward, prefecture, core city, designated city, prefectural police | 1,797 | 1,722 | 2,607 |
| institutions | university, junior college, research institute, observatory, science museum, hospital, museum, art museum, public library, botanical garden, zoo, aquarium, railway company, airport, fire department, tourism office, broadcaster, district court | 5,186 | 4,077 | 1,397 |

The administrative counts are the check that the harvest is near-complete:
755 cities / 743 towns / 184 villages against Japan's real 792 / 743 / 183.

A grouped SPARQL query asking which classes carry Japanese official websites
**timed out (504)**, which is why the classes were resolved one label at a time
through the search API instead. `tools/wd_harvest.py` was not used: it loops
every sovereign state against every class, ~1,800 paced queries for a job whose
classes are Japan-only by definition.

## The asymmetry that decided the batch

**Administrations declare feeds; institutions run WordPress.** The institution
pass found only 1,397 feed candidates from 5,186 hosts — Japanese hospitals,
museums and libraries mostly publish no RSS — but **1,628 of them carry a
WordPress marker**, and `tools/wp_routes_sweep.py` turned those into
**1,675 REST roots and 6,069 collections**. That is batch 45/49's method, and
it is where the bulk of this batch comes from. One discovery pass answered both
questions because `discover_feeds.py` reports the WordPress marker beside the
declared feeds.

## Four corrections, each caught before it shipped

**1. An exclusion list built by regex missed 1,499 duplicates.** A scan of the
tree's C for URL literals found 89,119 of them and looked thorough. It was not:
`tools/batch_exclusions.py --bin` reported 1,499 municipal rows already
registered, and says why in its own output —

```
id set: 57905 from the binary's registry (regex scan of the tree finds 46142)
```

The municipal pool is largely covered already by batch 50 and the `vsrc`
government feeds, so that beat went **2,069 → 570 genuinely new**. Across all
beats 437 more rows were dropped the same way, mostly against batch 42's
university/pages tables and batch 49's culture and medical beats. **Pass
`--bin`; a regex approximation of the registry is not the registry.**

**2. The first cut repeated the batch-47 mistake, and the publisher's class was
already in hand.** The feed pass had put museums, hospitals, libraries and
universities into ONE beat labelled `science`; the WordPress sweep grouped by
host suffix, so its `jpworg` beat held `.or.jp` museums, galleries and
hospitals. Copying batch 45/54's `jpwporg → grp_legal` would have filed 697
hospitals and museums under Legal. Every row was re-beated by **the class the
harvest measured for its host**, recovered per host and, for the hosts whose
feed lives off-site, from the Wikidata QID in the row's own id. 0 rows left
unclassified. Category is user-visible wherever sources are grouped, so a beat
named after the tool that found it is not a classification.

**3. `science` and `organization` are not categories this tree resolves.**
`make collectors-def-check` reported `UNASSIGNED: hp3b57_jpinst
hp3b57_jpwporg`, exactly as batch 54's note predicts. The fix is not an
`OVERRIDES` entry here: `CATEGORY_GROUP` maps `research`, `education`,
`health`, `culture`, `infrastructure` and `police`, and once each beat carried
the category it really is, all eight keys were assigned with no override at
all — 640 collectors, 23 groups, **0 unassigned**.

**4. A double slash made a host called `cgi-bin`.** These CMS feed URLs are
commonly `https://host//cgi-bin/feed.php?...`, and splitting on `//` took
`cgi-bin` as the hostname, which filed a Gunma town under an unclassified
residue. One row, and it is recorded because it is the same shape as every
scanner defect in this tree: it could not see something and acted anyway.

That row keeps its literal `&amp;` (`...&amp;displayCount=10`), which sends
`amp;displayCount` and is therefore ignored by the server. It is NOT decoded,
on this file's own rule: the `&amp;` form is what was measured passing, and of
1,076 such rows re-measured on 2026-10-22, 206 stored LESS once decoded.
Measure both URLs before changing one.

## Proof of life

`tools/probe_hp_batch.py`, paced per host, 20 minutes:

| verdict | rows |
| --- | --- |
| PASS | **3,622** |
| HTTP_ERR | 25 |
| NET_ERR | 15 |
| TIMEOUT | 3 |
| UNPARSEABLE | 1 |
| ERR | 1 |

**150,735 records visible on page 1** across the passing rows. The 45 failures
are kept in the manifest rather than deleted — one probe is one reading, and
HTTP and transport verdicts can be a user-agent or a bad afternoon — but they
are not in `pass-ids.txt`, so they did not generate.

## What shipped

| beat | rows | category | group |
| --- | --- | --- | --- |
| `hp3b57_jpedu` | 1,160 | education | grp_education |
| `hp3b57_jpheal` | 935 | health | grp_health |
| `hp3b57_jpcult` | 624 | culture | grp_culture |
| `hp3b57_jpmuni` | 606 | government | grp_government |
| `hp3b57_jpsci` | 256 | research | grp_science |
| `hp3b57_jpinfra` | 27 | infrastructure | grp_infrastructure |
| `hp3b57_jppolice` | 11 | police | grp_safety |
| `hp3b57_jppref` | 3 | government | grp_government |
| **total** | **3,622** | | |

Registry 57,905 → **61,527**, with `0 duplicates, 0 dropped, 0 never-runs`;
`make` rc=0 and 0 warnings; `lint-sources` OK; `audit-sources` 372 files and
**0 findings** (these tables are in the STRICT gated set).

## What is staged rather than registered

`docs/staging-sources-batch57.jpwpmedia.txt` (1,999 rows) and
`docs/staging-sources-batch57.jpwpcorp.txt` (95) are measured, de-duplicated
candidates that are NOT registered, following
`docs/staging-sources-batch45.federations.tsv`'s precedent. They are a record,
not a to-do list — this file's own history records a staging set whose header
said "NOT registered" for a fortnight after it was, which would have created
5,392 duplicate endpoints if believed.

They are held back for one reason: they are the two beats whose rows are
grouped by host suffix rather than by a measured publisher class, so shipping
them would re-introduce correction 2 above. Re-beat them the same way before
registering.

## Measured through the engine — the reading the batch is believed on

`tools/audit_batch_emit.py`, every row through the real binary against a
scratch database, 20 jobs, 20 minutes. The probe says an endpoint answers; only
this says it emits, and this tree has 59 rows of batch 18 to prove the
difference.

| beat | rows | OK | SLOW | TRANSPORT_FAIL | records emitted |
| --- | --- | --- | --- | --- | --- |
| jpedu | 1,181 | 1,136 | 24 | 0 | 180,155 |
| jpheal | 938 | 932 | 3 | 0 | 70,720 |
| jpcult | 626 | 616 | 3 | 5 | 31,157 |
| jpmuni | 614 | 601 | 2 | 3 | 29,036 |
| jpsci | 266 | 254 | 2 | 0 | 35,237 |
| jpinfra | 28 | 27 | 0 | 0 | 284 |
| jppolice | 11 | 11 | 0 | 0 | 122 |
| jppref | 3 | 3 | 0 | 0 | 21 |
| **total** | **3,667** | **3,580** | **34** | **8** | **346,732** |

**3,580 of the 3,622 generated rows emit records, and NOT ONE row fetched and
stored nothing** — no `DROPS_EVERYTHING`, which is the verdict rule 4 exists to
catch and the one that would have sent this batch back. 346,732 records in a
single pass.

The 45 `UNREGISTERED` are the arithmetic closing rather than a defect: they are
exactly the 45 probe failures, which never generated, so the binary does not
hold them and the audit says so. 3,580 + 34 + 8 + 45 = 3,667.

34 SLOW are UNMEASURED, not failed — killed at a 90 s line that is a property
of the tool, as this file records from batch 53 (a row that stored 22,000 of
35,204 at 200 s stored all 35,204 at 600 s). Re-run them with a larger
`--timeout` to measure them.

**A cost note worth keeping: the sweep is 20 minutes at 20 jobs and projected
to FIVE HOURS at 8.** The first attempt watched three runs take 35-66 s each
and extrapolated; a 341-row stratified sample then measured the real
throughput at 0.42 s/row effective. These rows are network-bound against
~2,500 distinct hosts, so concurrency buys almost linearly and the per-run
figure says nothing about the sweep. **Measure the throughput, do not
extrapolate from one run's duration** — the same error as reading a file's size
as progress.

## Open

* The emit reading per row is in `docs/verified-sources-batch57.tsv`; rows that
  fetch and store nothing are in `docs/rejected-sources-batch57.*.tsv` with
  their reading. A row that failed twice leaves; a row that failed once is
  re-run first.
* The 45 probe failures want one re-read on another day before any of them is
  removed, which is this tree's rule and has recovered 63 of 2,724 before.
* 833 institution hosts answered nothing at all (`http 000`) and 128 answered
  403. Those are a reading of this environment's egress as much as of the host —
  `LOCAL_EGRESS` is a verdict about here, never about a source — so they are
  unmeasured rather than dead.
