# Batch 66 — Socrata family, deepest CKAN datastore, ODS long tail

5,565 sources (registry 93,163 → 98,728), finishing the open-data veins.

## socrata — 3,627 (`hp3b66_socrata.c`)
The Socrata family, new this session. Paged the `api.us.socrata.com` federated
discovery catalog to collect 141 distinct domains, 90 new to the tree (King
County, EnergyStar, HHS datahub, Howard County MD, Pierce County WA, GNB…).
Each domain's datasets (up to 80/domain, breadth-first) as SODA record rows:
`/resource/<id>.json` with `$offset` paging, keyed on Socrata `:id`
(`$select=*,:id&$order=:id`). 3,627 of 3,647 store — a 99.5% emit rate, the
cleanest vein of the session.

## ckands — 1,910 (`hp3b66_ckands.c`)
Deepest CKAN datastore pass: the 141 CKAN hosts re-enumerated to 400
resources/portal, excluding every resource_id registered in batches 64/65.
1,829 store.

## ods — 28 (`hp3b66_ods.c`)
The OpenDataSoft long tail: the federation facet paged by language-refinement
reached 385 more domains, but beyond the ~355 already enumerated only 70 were
new and those are sparse — 28 datasets with records. Recorded for completeness;
the ODS vein is now effectively exhausted from this environment.

## Policy (unchanged)
Kept: OK/COLLISION/SLOW + HTTP-403/egress fetch failures (disclose, fixable in
a connected deploy). Dropped 349: 117 EMITS_NOTHING + 232 HTTP-404/500/400/
503/502/non-parse — dead or wrong-shape in any environment.
`docs/rejected-sources-batch66.tsv`.
