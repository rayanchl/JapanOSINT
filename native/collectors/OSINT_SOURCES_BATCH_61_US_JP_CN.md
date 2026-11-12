# Batch 61 — US staging drain, deeper Japan, China re-probe

4,697 emit-verified sources (registry 71,215 → 75,912), measured 2026-11-12.

## us — 3,908 (`hp3b61_us.c`)
The 3,922 US government ArcGIS Online layers batch 60 measured but left
unregistered (its selection favoured publisher breadth). Re-id'd to batch 61,
re-checked for collisions, run through the engine: 3,908 store records, 14
rejected (`docs/rejected-sources-batch61.tsv`). Same org-attribution and
paging as batch 60.

## jp — 761 (`hp3b61_jp.c`)
A deeper walk of the Japanese prefecture/city publishers already found for
batch 59: batch 59 capped 12 layers per service, so services with more than 12
layers had a tail left unread. Walking all services of the 152 gov orgs with a
40-layer cap and de-duplicating against every layer already registered yielded
762 genuinely new layers from 10 large-GIS orgs (埼玉県GIS, 網走市, …); 761
store records.

## cn — 28 (`hp3b61_cn.c`)
Re-probe of the 2,956 `.gov.cn` hosts that did not answer during batch 59.
This time 83 answered; 32 carry ≥8 dated CMS article links; 28 store records
(4 egress-failed). The other ~2,870 still did not answer — this container's
egress to China is the limit, not the sites.

## A China ArcGIS dead end, recorded so it is not retried
The AGOL public search was re-run with Chinese terms passed via a file (the
batch-60 run lost them to a shell encoding bug). 10,274 services matched, but
only **2** sit under a China-region org (Esri China + one junk org): Chinese
government bodies do not publish to ArcGIS Online. No CN AGOL rows were made;
the `.gov.cn` homepage beat remains the only China vein.
