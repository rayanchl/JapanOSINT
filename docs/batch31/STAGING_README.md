# Batch 31 — global high-penetrancy sources (STAGING, pending live probe)

**Status: UNVERIFIED candidates. Nothing here is registered or generated into C yet.**
Authored 2026-10-02. Rule 1 (never fabricate) is respected: every `description`
field says "PENDING LIVE PROBE" — no observation counts were invented, because
the authoring session had **no network egress** to the source hosts (the cloud
environment's network policy returned `403 / EGRESS_BLOCKED` for every OSINT host,
and the project probe, WebFetch, and the binary's own fetchers all go through the
same blocked proxy).

## What is staged

144 candidate rows, deduped against the live 17,469-id registry, across:

| beat | file | rows | families |
| --- | --- | --- | --- |
| jp  | `candidate-sources-batch31.jp.txt`  | 13 | prefectural/municipal CKAN, ministry/agency RSS not already registered |
| fr  | `candidate-sources-batch31.fr.txt`  | 33 | data.gouv.fr API, ~30 OpenDataSoft tenants, geo reference API |
| us  | `candidate-sources-batch31.us.txt`  | 41 | ~38 Socrata catalogs (city/state/federal), data.gov CKAN, NWS, FEC |
| cam | `candidate-sources-batch31.cam.txt` | 10 | official DOT/agency ArcGIS camera inventories |
| eu  | `candidate-sources-batch31.eu.txt`  | 20 | EU/UK/DE/NL/Nordics/CEE national CKAN, ECB FX, UK police |
| row | `candidate-sources-batch31.row.txt` | 27 | CA/AU/NZ/LATAM/Africa/APAC national open data, HDX, World Bank |

Strategy: favour **catalogue endpoints** (CKAN `package_search`, Socrata
`/api/catalog/v1`, OpenDataSoft `/api/explore/v2.1/catalog/datasets`) — these are
uniform, high-penetrancy, and almost entirely white space in the current registry
(only 3 Socrata + 1 ODS host were already present). Each needs only the host to be
correct, so the expected probe PASS rate is high.

## Scope deliberately excluded

Per the standing scope note: no unsecured/private cameras and nothing
SIGINT-interception. `cam` rows are **deliberately-published** government camera
*inventories* (ArcGIS layers the agencies publish), consistent with batch 29.

## Gates already passed (no network needed)

- `audit_batch_reachable.py` → 0 of 144 can never run (rule 3 OK)
- `audit_batch_pagination.py` → 0 undeclared-paged (5 declared `pagination_ok`)
- `batch_exclusions.py --bin ./bin/japanosint` → **0 collisions, 0 near-misses**

## Run sequence once network egress is enabled (NEW session required)

The environment network-policy change only takes effect in a fresh container, so
**restart the session** after broadening Network access, then:

```sh
cd native
# 1. Proof of life + does it parse + >=1 real record (drops empty sets, bot walls)
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --jobs 6 \
        --out /tmp/probe31.tsv --pass-ids /tmp/pass31.txt
grep -vc PASS /tmp/probe31.tsv; grep -v PASS /tmp/probe31.tsv   # inspect/fix/drop fails
# 1b. Pivot rows only: catch filter-ignored APIs
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter

# 2. For PASS rows: replace the "PENDING LIVE PROBE" description with the OBSERVED
#    count + field names (rule 1). Record every dropped candidate in
#    docs/rejected-sources-batch31.<beat>.tsv  (id<TAB>url<TAB>reason).

# 3. Generate C (one table per beat), then build
for b in jp fr us cam eu row; do
  python3 tools/gen_hp_batch.py ../docs/candidate-sources-batch31.$b.txt \
          --prefix JO31 --batch 31_$b
done
make

# 4. Prove it EMITS and STORES (not just fetches) — the gate that matters
python3 tools/audit_registry_emit.py --bin ./bin/japanosint --scheduled \
        --match JO31 --jobs 6 --timeout 220 --out /tmp/sweep31.tsv
#    Keep OK; fix COLLISION (id_keys identity wrong) and EMITS_NOTHING (title_keys)
#    with diagnose_emit_keys.py; drop rows that cannot be made to store.

# 5. Final gate (what CI runs)
make lint-sources && make audit-sources && make source-floor
```

Target: net **300+** sources that verify to EMIT. Expect attrition at step 1/4;
the pool above is the high-confidence seed. Once egress is live I will also expand
the pool *against live responses* (far higher quality than blind staging) — e.g.
enumerate real OpenDataSoft tenants (300+ exist), additional Socrata tenants, and
per-DOT ArcGIS camera layers — to clear the 300 net target.
