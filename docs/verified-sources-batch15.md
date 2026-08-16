# Batch 15 — 1,029 verified EU / US / JP open-data sources

**Status: VERIFIED.** Every row answered 2xx, parsed, and yielded ≥1 real
record at generation time. Manifest: `docs/verified-sources-batch15.tsv`.
Rejects are kept as data in `docs/rejected-sources-batch15.tsv`.

Collectors: `native/collectors/sources/vsrc15_*.c` (27 files)
Generator:  `native/collectors/gen_sources_batch15.py`

## How these differ from batch 14

Batch 14 was authored without egress: hostnames were recalled from documented
API contracts and emitted unverified, and 26% of them turned out dead. Batch 15
inverts that — **nothing enters the manifest that has not already answered.**

1. **Portal list from a machine-readable registry, not memory.**
   `dataportals.org/api/data.json` (631 entries) filtered to EU/US/JP, unioned
   with every host already proven live in this tree's own verified manifests →
   **1,012 distinct portal hosts**.
2. **Platform detected by probing, never assumed.** Each host is probed with the
   signature endpoint of each platform we can parse (CKAN `package_search`,
   Socrata `catalog/v1`, ODS `explore/v2.1`, ArcGIS Hub, uData). **110 of 1,012
   answered.** A host that answers nothing never reaches the manifest.
3. **Expansion only along confirmed platform contracts.** The API paths are
   fixed by the platform, not the deployment, so once the platform is confirmed
   the contract is known-good.
4. **Sub-entities fetched, never guessed.** Publisher/organisation names come
   from the portal's own `organization_list` / `categories` / facet response. If
   that call fails, no extra rows are emitted — real fetch or honest empty.
5. **Then the normal gate.** `tools/promote_candidates.py` re-probes every row;
   only PASS survives.

Result: **1,029 verified from 1,181 candidates — an 87% pass rate**, against
59% for batch 14 (594/1,001). The probing stage is what moved that number.

## Composition

| Country | Sources | | Platform | Sources |
|---|---|---|---|---|
| FR | 255 | | CKAN | 577 |
| CH | 208 | | OpenDataSoft | 397 |
| DE | 165 | | Socrata | 54 |
| US | 152 | | ArcGIS Hub | 14 |
| ES | 58 | | uData | 3 |
| RO, BE, IE, JP | 163 | | | |
| LV, UK, GB, IT, HU, HR, SI | 108 | | | |

Each source is a publisher-level stream ("everything published by *this*
authority"), not a whole-portal catalogue dump — that is the penetrancy axis:
the useful pivot is the ministry / police department / water authority, and its
change history.

## Two upstream defects this batch exposed

Both were found by running the generated sources and noticing results that were
*too clean*, and both are fixed.

**1. Every OpenDataSoft catalogue record was being dropped.**
`lib/jsonlist.c` descended one level into an envelope (`attributes`,
`properties`) looking for a label. ODS puts it two levels down:

```
{visibility, fields[], dataset_id, dataset_uid, has_records, features,
 attachments, metas:{dcat, inspire, default:{title, description, theme,
 keyword, license, modified, …}, custom}}
```

So a record with a real title, licence and modification date scored as
unlabelled and was discarded. Measured against `data.loire-atlantique.fr`:
`total_count 2`, records emitted **0**. The envelope list now accepts dotted
paths and includes `metas.default`; the same endpoints emit **2 of 2** and
**4 of 4**. Properties still serialize the *outer* record, so `dataset_id` and
the field schema are retained.

**2. The verifier passed empty result sets.**
`{"total_count": 0, "results": []}` fell through to `json-object` / 1 item and
counted as a PASS — a source verified as live that is structurally guaranteed
to emit nothing on every run. `count_json` now returns `__empty__` when the
only lists in a document are empty, and `verify_feeds.py` records the verdict
`EMPTY_RESULTSET`. Genuine single-object documents (a status doc with no list,
e.g. `{"crntObsTime": "…"}`) still pass — covered by a self-test in
`scratchpad/reverify.sh`'s five cases.

This re-verification is why the counts here are lower than the first pass:
**batch 14 740 → 594, batch 15 1,132 → 1,029** (102 and 70 empty result sets
respectively, plus timeouts and oversize bodies). Those are the honest numbers.

## Rerunning

```sh
cd native
python3 collectors/gen_sources_batch15.py \
    --portals <portals.json> --out ../docs/candidate-sources-batch15.tsv \
    --exclude-urls <tree urls> --exclude-ids <tree ids>
python3 tools/promote_candidates.py ../docs/candidate-sources-batch15.tsv \
    --verified ../docs/verified-sources-batch15.tsv \
    --rejects  ../docs/rejected-sources-batch15.tsv
python3 collectors/gen_verified_sources.py ../docs/verified-sources-batch15.tsv \
    --outdir collectors/sources --reserved <ids> --prefix vsrc15
```

Both generation and promotion need network. That is deliberate: this batch's
whole premise is that the network, not the author, decides what exists.
