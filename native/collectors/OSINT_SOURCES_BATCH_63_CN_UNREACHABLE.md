# Batch 63 (cn) — Chinese government homepages, shape-inferred scaffolds

2,870 `.gov.cn` government homepages from Wikidata P856 that are UNREACHABLE
from this authoring container (egress resets them) and that batches 59/61 could
not measure. Registered at the repository owner's explicit instruction to keep
all sources even when they cannot be verified here.

## Why they could not be measured, and the "find a way" attempts
Every route that could have revealed each site's CMS link shape without a live
fetch is also egress-blocked from this container, each tested and recorded:
- Wayback *availability* API (archive.org) works and confirms every host has a
  live snapshot — but Wayback page content (`/web/<ts>/` and `id_/`) returns
  HTTP 403, the CDX index returns "Blocked by egress policy", Common Crawl's
  index returns empty, and mementoweb 502s.
- Search-engine `site:` scraping (Bing/DDG) returns only bare hostnames, not
  the deep article paths needed to detect a pattern.

So the exact per-site pattern is unknowable here. Each row is given
`href_must=/t20`, the dominant Chinese-government CMS article pattern measured
across the ~467 reachable sites of batches 59/61 (~70% of those that had any
recognised pattern: `/t20\d{6}_\d+.html`). Sample of 40 run through the engine:
35 FETCH_FAILED (egress), 2 SLOW, 3 reachable-but-empty-under-this-pattern.

## What these rows are, honestly
A scaffold, not measured data. In a normally-connected deployment each row
stores dated notices where the site uses the `/t20` CMS and files an honest
empty/notice otherwise, to be re-pointed. The shape-learner is
`tools/`-style `wayback_shape.py` in the session scratch; from a connected
environment, fetch each homepage, detect its real pattern, and rewrite
`href_must`. Tagged `shape-inferred` so they are easy to find and refine.
They are deliberately NOT a claim of verified emission.

---

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
