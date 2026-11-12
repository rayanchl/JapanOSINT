# Batch 59 — Japanese public GIS layers and Chinese government notice pages

2,982 emit-verified sources (registry 64,474 → 67,456), measured through the
engine on 2026-11-11: 2,987 candidates, 2,963 OK on the first pass, 19 of the
23 slow/failed recovered on one re-read, 5 rejected
(`docs/rejected-sources-batch59.tsv`).

## jpgis — 2,543 layers (`hp3b59_jpgis.c`)
ArcGIS Online's own public search was asked for Japanese-language feature
services (40 terms: 防災, 避難, 都市計画, 公園, …): 29,931 services under 2,188
org ids. Each org id was resolved through `sharing/rest/portals/<id>` to the
name the organisation gives itself, which is what batch 58 lacked when it
dropped org-id layers as unattributable. 350 orgs are Japanese; 153 name
themselves as a prefecture, municipality, ministry, police or fire body
(埼玉県GIS, 網走市, 会津若松市, 浜松市, 佐渡市役所, 八王子市, つくば市, 世田谷区,
秋田県警察本部, 国土交通省 …). Companies, universities, Esri Japan's own orgs
and one placeholder-named org were excluded by name.

Up to 60–80 services per publisher, 12 layers per service; a layer is a row
only if its `returnCountOnly` is non-zero. Rows page on the layer's own
`objectIdField ASC` with `page_resume=1`; non-ASCII service names are
percent-encoded in the URL. Category is read from the publisher's own service
and layer title (避難/防災 → safety, 水道/都市計画 → infrastructure …), never
from column names.

Known limits, stated rather than hidden:
* A layer with no name-like field is titled by its object id.
* Some publishers ship shapefile-era field names that are mojibake upstream;
  they are stored as served.
* ISUT (Cabinet Office disaster team, 3,397 services) and NIED (740) were not
  taken: mostly per-event temporary layers, worth a deliberate beat.
* 93 services answered "Token Required" and were skipped — they are not public.

## cngov — 439 sites (`hp3b59_cngov.c`)
Seed: every Wikidata item with country = PRC and an official website on
`.gov.cn` — 4,090 hosts, 12 already known. From this environment 1,134
answered. `--discover-feeds` over all of them found **zero** declared feeds,
so RSS is not how these sites publish. What they do share is two CMS families
whose article URLs have a fixed shape: `/tYYYYMMDD_<id>.html` (326 sites),
`/content/post_<id>.html` (94), `/art/YYYY/M/D/art_…` (47). A row reads the
homepage in html mode with `href_must` set to that shape, so only dated
article links become records, not navigation. Kept only where ≥8 such links
carried a real label; stored per site: min 6, median 52, max 1,024.

These rows read the HOMEPAGE: they are a recency window on each body's
notices, not its archive. The 2,956 hosts that did not answer from here are
unmeasured, not dead (this container's egress resets many CN hosts).

## Not done
City webcams: no new published camera-list endpoint was found that the tree
does not already read; nothing was registered on a guessed path.
