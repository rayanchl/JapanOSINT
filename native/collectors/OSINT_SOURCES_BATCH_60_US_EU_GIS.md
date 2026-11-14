# Batch 60 — US and European public GIS layers (ArcGIS Online, org-attributed)

3,759 emit-verified sources (registry 67,456 → 71,215), measured through the
engine on 2026-11-12: 2,495 US + 1,264 EU. 3,765 candidates, 3,755 OK on the
first pass, 4 of 6 slow/failed recovered on one re-read, 6 rejected
(`docs/rejected-sources-batch60.tsv`).

## Method — the batch-58 attribution gap, closed
Batch 58 dropped ArcGIS Online org-id layers because a multi-tenant
`services*.arcgis.com/<orgid>` host cannot be attributed by host. Batch 59
fixed this for Japan by resolving each org id through
`sharing/rest/portals/<id>` to the name the organisation gives itself; batch 60
applies the same to the US and Europe.

ArcGIS Online's public search was asked for ~45 ASCII/Latin terms (flood,
zoning, parcels, cadastre, planning, land use, boundaries, wards, precincts,
hydrants, wildfire, …): 88,505 public feature services under 12,671 org ids.
Each org resolved to its portal name and kept only where that name is a
government body in the US or an EU/EEA/UK/CH country —
2,027 US orgs (City of Austin, King County, Miami-Dade, Broward, Sonoma, …)
and 128 EU orgs (Ayuntamiento de Sevilla, Fingal/Galway/Northumberland
County Councils, Communauté d'Agglomération …, Município de Mafra, …).
Companies, universities, schools, Esri's own orgs, demos and consultancies
were excluded by name.

Selection favours **breadth** (penetrancy): every EU gov layer (deep, EU is
scarce), and a round-robin across US publishers (2,500 layers over 1,941
distinct US orgs, ≈1–2 per publisher). The remaining 3,922 US candidates are
kept, unregistered, in `docs/candidate-sources-batch60.usgov.staging.txt` as a
measured record for a follow-up beat, not a to-do.

Rows page on the layer's own `objectIdField ASC` with `page_resume=1`;
polygons request a WGS84 centroid, points/lines keep geometry; category is read
from the publisher's own service/layer title, not from column names.

## Not done
Chinese AGOL terms were lost to a shell encoding issue in the authoring run and
are not part of this batch; China is covered by batch 59's `.gov.cn` homepage
beat. No webcams added.
