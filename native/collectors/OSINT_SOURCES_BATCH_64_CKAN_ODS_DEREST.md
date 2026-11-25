# Batch 64 — CKAN datastore, more OpenDataSoft portals, DE/IT ArcGIS-REST

5,623 sources (registry 82,897 → 88,520), three EU non-AGOL veins, all
engine-measured.

## ckands — 2,197 CKAN datastore resources (`hp3b64_ckands.c`)
The datastore vein (as done for ODS): across 141 CKAN hosts already in the
tree, `package_search` → resources with `datastore_active:true` → one row per
resource reading `datastore_search?resource_id=…` (offset paging, keyed on the
datastore `_id`). Breadth-first, ≤60 resources/portal. 2,126 store on the
first pass.

## ods — 2,575 OpenDataSoft datasets (`hp3b64_ods.c`)
59 more ODS portals, discovered properly this time: the
`data.opendatasoft.com` federation exposes each dataset's `source_domain_
address`, so faceting on it yields the ODS portal directory (100 shown, 59 new
to the tree — Rotterdam, Dortmund, Birmingham, Bologna, Corsica, Bordeaux,
Leicester, Castilla y León…). Each portal's own (non-federated) datasets as
record rows, breadth-first ≤~45/portal. 2,199 store.

## derest — 851 German/Italian ArcGIS-REST layers (`hp3b64_derest.c`)
Standalone government ArcGIS-REST geoportals that are NOT WFS — German Länder
(Bayern, MV, Niedersachsen, NRW, Saarland, BfG, …) 784 and Italian regioni
(Emilia-Romagna, Veneto, Lazio, Piemonte, …) 67. Same enumerator and paging as
the US/EU AGOL beats. 775 store.

## Policy
Kept per the owner's keep-all rule: OK/COLLISION/SLOW and HTTP-403 fetch
failures (plausibly UA/rate-limit, fixable in a connected deployment; they
disclose their own failure). Dropped 229: 89 EMITS_NOTHING (empty in any
environment) and 140 HTTP-404/400/non-parse (dead or wrong-shape everywhere) —
`docs/rejected-sources-batch64.tsv`.

Also in this batch: the 2,870 shape-inferred `cn` rows of batch 63 were
REMOVED (a /t20-guessed scaffold is not verified data).
