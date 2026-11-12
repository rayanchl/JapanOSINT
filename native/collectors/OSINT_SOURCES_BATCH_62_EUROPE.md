# Batch 62 — European public GIS layers (multilingual ArcGIS Online)

4,535 sources (registry 75,912 → 80,447). The EU vein was shallow in batch 60
(128 gov orgs) because the search terms were mostly English/Latin. This batch
re-ran ArcGIS Online's public search with **121 terms across 20+ European
languages** (cadastre/catasto/kataster/kadaster, inondation/overstroming/
powódź, arbres/alberi/bäume/drzewa, commune/comune/gemeente/gmina/δήμος …),
passed through a file so the shell could not mangle non-ASCII.

146,416 public services under 16,983 org ids (merged with batch 60's search).
Each org id resolved through `sharing/rest/portals/<id>` to its own name;
kept the 407 that name themselves as a European government body across 19
countries — NL 64, FR 61, GB 61, SE 41, DE 32, NO 26, ES 25, IE 22, BE 19,
PT 13, DK 12, CZ 11, IT, SK, PL, AT, CH, FI, GR. Companies, universities,
schools, foundations, Esri and demos excluded by name. 4,554 new layers
enumerated (deduped against every registered layer; 16 batch-60 duplicates
removed).

Measured through the engine: 4,527 OK on the first pass, 7 of 8 slow rows
store at a higher timeout. **3 genuine-empty layers dropped** (fetch fine,
zero records — these would be dead in any environment). **1 layer kept despite
a transport failure** here: per the owner's instruction this round, a row that
fails for an *environment* reason (egress/transport/timeout, not an empty
body) is kept — it discloses its own `collector-fetch-failure` and will store
in a normally-connected deployment. That is the only policy change from
batches 58–61; genuine empties are still excluded, because a row that returns
nothing everywhere is the silent-nothing rule 1 forbids.

Same paging as the other ArcGIS beats (`resultOffset`, layer's own
`objectIdField ASC`, `page_resume`; polygon centroid, point/line geometry).
