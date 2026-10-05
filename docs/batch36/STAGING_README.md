# Batch 36 — global high-penetrancy sources (STAGING, pending live probe)

> ## Revised 2026-10-05 — renumbered 31 → 36, deduped, two defect classes fixed
>
> **Renumbered.** This set was authored as "batch 31" on 2026-10-02, but
> `collectors/pivot/table/hp3b31_*.c` (PR #23, 361 rows) had already merged under
> that number. Whoever merges first keeps a number, and those tables also needed
> `docs/candidate-sources-batch31.*.txt` for their own manifests, so this set moved
> to the next number free on main at the time: **36**. Ids are unchanged (they
> carry no batch prefix); only file names, paths and references moved.
>
> **Deduped against the tree as it is now — 61 of 144 rejected, 83 kept.**
> The original dedupe ran against a 17,469-id registry that did not yet contain
> hp3b31, batches 32-35 or anything since, and `batch_exclusions.py` compares
> whole endpoints — so a row whose host and path an existing row already reads,
> differing only by paging or sort parameters, passes it. A host+path comparison
> that ignores paging/format/sort/`order_by`, treats a content filter (`q=`, `fq=`,
> `where=`) as a different request, and ignores trailing-slash composition
> prefixes found 61 such duplicates. Each is in
> `docs/rejected-sources-batch36.<beat>.tsv` with the existing collector it
> duplicates. Eleven rows that share a host+path but are NOT duplicates were kept:
> a scheduled full listing where the tree only has an entity pivot or a filtered
> subset (BODIK `fq=`, data.go.jp `q=`, data.gov, data.europa.eu, four Socrata
> domains, NZ, Colombia, Peru).
>
> **Socrata catalogue rows were not scoped to their own domain.** A domain's own
> `/api/catalog/v1` searches every Socrata portal unless `domains=` and
> `search_context=` are passed, so these rows would have stored other portals'
> datasets attributed to this one. All 17 kept catalogue rows now pass both, in
> the url and the probe field. (All 41 originally lacked it; the rest were
> rejected as duplicates.)
>
> **`page_size` on a page-NUMBER parameter makes the engine use offset
> arithmetic** — `offset_style = (page_size > 0) || offset_named` in
> `lib/hpengine.c`, so page 2 is requested as `page=101`. Four staged rows had
> it; three were duplicates, and `US_FEC_CANDIDATES` had `page_size` removed.
>
> Gates re-run on the revised set: `manifest.py` strict load OK, reachability
> 0 of 83 unreachable, pagination clean, `batch_exclusions.py --bin --strict`
> 0 collisions against the 18,457-id tree. **Still nothing is generated into C,
> and still nothing has been probed** — the network policy of the revising
> session was the same as the authoring one's.
>
> **Two commands in the run sequence were wrong and are fixed below.** Step 3
> generated with `--prefix JO31 --batch 31_$b`, and `--prefix` is the output
> FILENAME prefix, so it would have written `hp3b31_jp.c` beside the merged
> batch-31 tables. Step 4 swept with `--match JO31`, a regex on source id, and
> these ids carry no batch prefix, so it selected zero rows and would have
> printed a clean sweep of nothing. It now takes the probe's PASS id list.
>
> (The number was first chosen as 38 while PR #27 was renumbering itself to
> 36/37; main merged that PR keeping 34/35 instead, so 36 is the next free.)

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
| jp  | `candidate-sources-batch36.jp.txt`  | 12 (was 13; 1 rejected) | prefectural/municipal CKAN, ministry/agency RSS not already registered |
| fr  | `candidate-sources-batch36.fr.txt`  | 14 (was 33; 19 rejected) | data.gouv.fr API, ~30 OpenDataSoft tenants, geo reference API |
| us  | `candidate-sources-batch36.us.txt`  | 17 (was 41; 24 rejected) | ~38 Socrata catalogs (city/state/federal), data.gov CKAN, NWS, FEC |
| cam | `candidate-sources-batch36.cam.txt` | 10 (was 10; 0 rejected) | official DOT/agency ArcGIS camera inventories |
| eu  | `candidate-sources-batch36.eu.txt`  | 14 (was 20; 6 rejected) | EU/UK/DE/NL/Nordics/CEE national CKAN, ECB FX, UK police |
| row | `candidate-sources-batch36.row.txt` | 16 (was 27; 11 rejected) | CA/AU/NZ/LATAM/Africa/APAC national open data, HDX, World Bank |

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
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch36.*.txt --jobs 6 \
        --out /tmp/probe36.tsv --pass-ids /tmp/pass36.txt
grep -vc PASS /tmp/probe36.tsv; grep -v PASS /tmp/probe36.tsv   # inspect/fix/drop fails
# 1b. Pivot rows only: catch filter-ignored APIs
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch36.*.txt --check-filter

# 2. For PASS rows: replace the "PENDING LIVE PROBE" description with the OBSERVED
#    count + field names (rule 1). Record every dropped candidate in
#    docs/rejected-sources-batch36.<beat>.tsv  (id<TAB>url<TAB>reason).

# 3. Generate C (one table per beat), then build
for b in jp fr us cam eu row; do
  python3 tools/gen_hp_batch.py ../docs/candidate-sources-batch36.$b.txt \
          --prefix hp3b36 --batch 36   # was `--prefix JO31 --batch 31_$b`, which
                                       # named the output hp3b31_<beat>.c — the
                                       # merged batch-31 tables' own namespace
done
make

# 4. Prove it EMITS and STORES (not just fetches) — the gate that matters
#    --match is a regex on SOURCE ID and these ids carry no batch prefix, so the
#    old `--match JO31` selected zero rows and reported a clean sweep of nothing.
#    Feed it the PASS ids from step 1 instead:
python3 tools/audit_registry_emit.py --bin ./bin/japanosint \
        --ids-file /tmp/pass36.txt --jobs 6 --timeout 220 --out /tmp/sweep36.tsv
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
