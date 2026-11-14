# Batch 65 — more EU open data: tier-2 ODS portals + deeper CKAN datastore

4,643 sources (registry 88,520 → 93,163), continuing batch 64's veins.

## ods — 2,580 (`hp3b65_ods.c`)
The ODS federation facet caps at 100 domains, so it was paged with cumulative
`exclude=source_domain_address:"…"` to reach 400 distinct ODS portals; 296
were new to the tree (beyond batch 64's 59). Each new portal's own
(non-federated) datasets as record rows, breadth-first across 226 reachable
portals. 2,263 OK + 265 byte-identical-dedup COLLISION + 52 kept env/403.

## ckands — 2,063 (`hp3b65_ckands.c`)
Deeper CKAN datastore pass: re-enumerated the 141 CKAN hosts at 150
resources/portal (batch 64 took 60), excluded every resource_id batch 64
already registered, kept the additional resources. 1,984 OK.

## Policy (unchanged)
Kept: OK/COLLISION/SLOW + HTTP-403/egress fetch failures (disclose, fixable in
a connected deploy). Dropped 314: 88 EMITS_NOTHING + 226 HTTP-404/500/400/502/
non-parse (dead or wrong-shape in any environment) —
`docs/rejected-sources-batch65.tsv`.
