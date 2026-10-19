# Batch 45 — ArcGIS feature layers, found by asking each service root for its own directory

**164 rows, every one measured live** (2026-10-19).
Manifest: `docs/candidate-sources-batch45.arcgislayers.txt`.
Table: `native/collectors/pivot/table/hp3b45_arcgislayers.c`.
Probe: `docs/verified-sources-batch45.tsv`. Rejects: `docs/rejected-sources-batch45.tsv`.

## The method, which is the point

This is batch 42's method on a different family. Batch 42 asked each Japanese
WordPress host for its route table at `/wp-json` instead of requesting the one
path everybody knows (`wp/v2/posts`), and found 6,726 advertised collections on
433 hosts where a convention sweep had found 465 endpoints on 6,720. CLAUDE.md
draws the general lesson: **ask the endpoint what it serves, rather than probing
the paths you can name.**

An ArcGIS service root is the same shape. The tree already reached 195 of them
— `services7.arcgis.com/<org>/arcgis/rest/services`,
`gis.rrc.texas.gov/server/rest/services`, `www.snirh.gov.br/arcgis/rest/services`
— and each was reached by **one row naming one layer of one service**: the path
somebody happened to know. A root publishes its whole directory at `?f=json`:
its folders, its services, and for each FeatureServer or MapServer the layers
and tables it holds.

Asking produced the multiplier the method predicts:

| | |
| --- | --- |
| service roots already in the tree | 195 |
| services they advertise | thousands — one root alone published **899** |
| candidate layers taken (capped at 5 per root) | 899 |
| probed PASS | 819 |
| after dedupe, a 50-record floor and dropping cartography | 456 on 157 roots |
| selected | 164 |

The cap is the honest part of that table: the directories hold far more than
899 layers. Five per root was a deliberate bound on how much of one host's
directory to walk, not a limit the hosts imposed.

## What each row is

```
<root>/<Service>/FeatureServer/<layer>/query
    ?where=1=1&outFields=*&<geometry>&orderByFields=<oid> ASC
    &resultRecordCount=<maxRecordCount>&f=json
```

`array_path=features`, `id_keys=attributes.<objectIdField>`,
`title_keys=attributes.<name-ish field>,attributes.<objectIdField>`,
`page_param=resultOffset` with `page_size` and `page_zero_based=1`. That is the
shape of `FAA_ARCGIS_OBSTACLES`, which has been through the emit audit, copied
rather than re-derived.

Four things in it were measured rather than assumed.

### The id field is the layer's own, not `OBJECTID`

Each layer publishes `objectIdField` in its metadata and that is what `id_keys`
names. Assuming `OBJECTID` would have been wrong on the layers that use `FID`,
and rule 4b's failure is silent: a key that is not unique per record reports a
healthy `records=N` and stores one row.

### The page size is the layer's own `maxRecordCount`

124 of the selected layers publish 2,000, 36 publish 1,000, and a few publish
3,000, 10,000 and 16,000. `page_size` is `min(maxRecordCount, 2000)` — asking
for more than a server will give is a page that silently comes back short, and
asking 16,000 of one host in a single response is not politeness.

`resultOffset` is a record OFFSET, so `page_size` belongs on it. This is the
case CLAUDE.md's `page-size-on-page-number` lint exists to separate from the
one that broke 83 rows: `make lint-sources` passes.

### An ascending sort, because these rows cycle

`orderByFields=<oid> ASC`. ArcGIS promises no stable ordering without one, and
43 of these layers hold more than their 20-page ceiling can take in a run, so
they declare `page_resume=1` and read their collection over a cycle. CLAUDE.md's
ascending-sort note is exactly about this: under a resumed walk an unstable or
descending order re-reads or skips records at every page boundary. The object id
is the most stable key such a layer has.

### Geometry, decided per geometry type and measured each way

| geometry | asked for | measured |
| --- | --- | --- |
| point (90) | `returnGeometry=true&outSR=4326` | the coordinate is the fact and costs almost nothing |
| polyline (18) | `returnGeometry=true&outSR=4326` | paths cost 1.3x–9.3x (99→175 KB, 160→1,495 KB per 200 records) |
| polygon (43) | `returnGeometry=false&returnCentroid=true&outSR=4326` | rings cost **64x** (125 KB → 8.0 MB per 200 records); a centroid costs 1.7% of that |
| non-spatial table (10) | `returnGeometry=false` | the layer has no geometry; that is not a shortfall |
| multipatch (1) | — | dropped: a 3D mesh has no location this engine can carry |

A polygon's rings are coordinates hpengine cannot index as geometry anyway —
HP_JSON has no FeatureCollection mode — so a centroid is strictly more useful
per byte. **`returnCentroid` works only on polygon layers**, and that cost a
whole probe round: the rule was first written for polygons and polylines
together, on four test layers that happened to all be polygons. ArcGIS answers

```
'returnCentroid' parameter is invalid
Return geometry centroid is only supported on layer with polygon geometry type.
```

with **HTTP 200**, so all 18 polyline rows came back as a 216-byte error body.
`probe_hp_batch.py` caught every one as `PATH_UNRESOLVED` — an HTTP-200 refusal
is precisely what it is built to reject — which is the gate working, and the
reason to re-measure rather than widen the rule.

## Two defects this batch found in the tooling

**`gen_hp_batch.py` could not express `page_resume`.** The field landed in the
engine and in 533 rows of C without landing in the generator's opt table, so
the first batch to need it had every such row rejected as an unknown opt. The
same gap, found the same way, as `page_walk` before it — and `window_days` /
`window_from`, added to the engine the same day as this batch, were missing for
the same reason. All three are in the table now.

**`batch_exclusions.py` matched a `record_type` literal as a dataset name.** It
reported `JO45_AGS_SERVICES6_AIRSPACE` as a DUP-ENDPOINT-COMPOSED collision with
`collectors/sources/av_faa_arcgis.c`, "`'airspace'` is one of its layer/dataset
names". It is not: `"airspace"` appears in that file as the `record_type` stamped
on three of its decoders. The 14 services that collector actually reads are
named in its header, and neither `Airspace` nor `ATSRoute` is among them. Checked
against the upstream rather than reasoned about:

```
Airspace        16,789 records
ATSRoute        52,096 records
Class_Airspace   6,045 records   (the one the existing collector reads)
```

Three different collections. Both rows are kept. This was the tool's only
collision and its only near-miss across 164 rows, and it was a false positive —
which is the right direction for that gate to err, and why it prints near-misses
for a human to read instead of dropping them.

## Two defects this batch found in its own generator

Worth recording because both are the shape CLAUDE.md keeps naming: a confident
wrong answer that every record-counting gate passes.

**Publisher attribution keyed on the HOST.** `services1.arcgis.com` is ArcGIS
Online's multi-tenant front — hundreds of unrelated organisations behind it,
told apart only by the 32-character org id in the path. Keying the "who
publishes this" lookup on the host attributed a Wyoming EMS response-time layer
and a Montgomery County bridge layer to **"Wakayama City traffic accident record
points 2020-2022"**. Every gate would have passed it: the endpoint is live, the
records are real, the counts agree. Only the name was false, and a reader cannot
tell a false attribution from a real one. Keyed on the full root now; 128 of 164
rows carry a publisher established that way and the other 36 name their HOST,
which is a fact about where the data came from rather than a claim about whose
it is.

**Category inferred from COLUMN names.** A bridge layer carrying a `FLOODPRONE`
column was filed under `safety`; a power-outage layer under `health`; an EMS
travel-area layer was tagged a permit register because it has a `License__`
column. A category is user-visible on every page that groups by it. It is now
read from the layer's and service's own names only, and where those say nothing
the row takes the publisher's category or `geospatial` — which is true of every
row here and claims nothing. A guess read off a column is not a measurement.

All 12 categories used are from the vocabulary `native/source.h` writes down.
Twelve values carried over from existing rows (`disaster`, `aviation`,
`maritime`, `corporate`, `geology`, `public-safety`, …) are mapped onto it
rather than inherited, because source.h's list exists "so the next author picks
from it instead of inventing a 32nd" and inheriting one is still picking it.

## `name_ja` is empty on every row, deliberately

The manifest format allows it ("Japanese name (may be empty)") and the
alternative was 164 machine translations of agency and layer names. A wrong
Japanese name is worse than none: it reads as authoritative and nothing in the
tree would ever catch it. The 90 Japanese-published layers keep their own
Japanese layer names in `title_keys` and in every record's `properties`.

## Gates

```
batch_exclusions.py --check --bin   1 collision, 1 near-miss — both read by hand, both false positives
audit_batch_pagination.py           0 of 164 declare no pagination but look paged
audit_batch_reachable.py            0 of 164 can never run
probe_hp_batch.py                   164/164 PASS
audit_page_param.py                 0 batch-45 rows bind their own page_param in the URL
audit_batch_emit.py                 OK=161, SLOW=3, 1,790,908 records emitted; no DROPS_EVERYTHING
audit_registry_emit.py --match ^JO45_  per row, emitted AND stored — docs/verified-sources-batch45.tsv
make audit-sources                  strict set 319 files, 0 findings
make lint-sources                   OK
make source-floor / registry-floor  21,953; 0 duplicates, 0 dropped, 0 never-runs
make / selftest / unit / hptest / authtest / pagewalktest / ci_concurrency_gate.sh   all pass
```

## Rule 4 and 4b, measured

`audit_batch_emit.py` ran all 164 rows through the real binary: **161 OK, 3
SLOW** (unmeasured at a 240 s kill line, not failed — they are the largest
layers), **1,790,908 records emitted** across the batch and not one
`DROPS_EVERYTHING`. That is rule 4: the endpoint answering is not the engine
emitting, and here it does.

Rule 4b is the one that fails silently, so it was measured per row rather than
sampled: `audit_registry_emit.py --match ^JO45_` reads `emitted` and `stored`
off each run line AND reads the stored count back out of the database, which is
two independent readings. The per-row table is `docs/verified-sources-batch45.tsv`.
`attributes.<objectIdField>` is unique per record by ArcGIS's own definition,
which is why `id_keys` names it and not the first field that looked like an id —
the mistake batch 36's generator made, where every uid became a content hash and
a record that CHANGED became a new row instead of an update.
