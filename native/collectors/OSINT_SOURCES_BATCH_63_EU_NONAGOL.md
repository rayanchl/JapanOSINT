# Batch 63 (euwfs) — European national/regional INSPIRE WFS feature types

591 WFS 2.0 feature types enumerated from 34 national/regional European
geoportals' own GetCapabilities (IGN France `data.geopf.fr` 119, Statistics
Finland 120, Portugal ICNF 118, Italian regioni 101, PDOK Netherlands 66,
Poland 51, Germany BKG/Länder 16; FR/NL/DE/IT/ES/BE/AT/NO/SE/FI/IE/PT/PL/CZ/
DK/CH/GR seeded). Each row does GetFeature as GeoJSON, paging on `startIndex`
where the type has a primary key.

Measured through the engine: 499 store records (OK/SLOW/COLLISION). 123 first
failed HTTP 400 — GeoServer's documented refusal of `startIndex` without a
primary key (the BC_MINERAL_TENURE case in this file); those rows were
rewritten to read one 1,000-feature page and disclose the remainder, after
which 63 store. The rest are kept per the owner's keep-all instruction: ~60
return 400 for a per-type reason (the DATA exists but that type does not serve
GeoJSON as requested — fixable by output-format in a deployment) and ~30 are
this container's own egress (`no certificate judged`). All disclose their own
fetch-failure; none is a fabricated or silent-empty row (the WFS set had zero
EMITS_NOTHING).

New tool: `wfs_enum.py` (session scratch) — GetCapabilities version
negotiation (2.0.0 then 1.1.0), namespace-aware FeatureType/Name parsing,
`resulttype=hits` counts. Only 2.0.0 types (clean count/startIndex/JSON) were
registered.

---

# Batch 63 (euods) — European OpenDataSoft portal datasets

1,859 individual datasets from 15 reachable European OpenDataSoft portals
(Rennes, Nantes, Toulouse, Île-de-France, Occitanie/laregion, Pays de la
Loire, Aix-Marseille, Issy, Loire-Atlantique, Strasbourg, Paris, STAR, SmartIDF,
Aude, public.opendatasoft.com). The portal *catalog* row already existed for
most of these; this beat registers each portal's own datasets as record rows
(Explore v2.1 records API, offset paging) — the actual data, not just the
dataset list.

Only each portal's OWN datasets: FEDERATED datasets (pulled from another
domain into a portal's catalogue) were excluded, because their records API
404s on the federating host (`NoDomainMatchingException`) — the records live on
the source domain. 2,179 → 1,872 after dropping federated.

Engine-measured: 1,598 OK, 259 COLLISION (ODS v2.1 records carry no universal
id, so the engine content-hashes uids; the collisions are byte-identical
upstream duplicates, correctly deduped — kept), 2 egress failures (kept). 13
EMITS_NOTHING dropped (has_records was stale — empty in any environment).
Titles fall back to record_type where a dataset's own fields use no common
name key; the body carries the real per-dataset attributes regardless.
