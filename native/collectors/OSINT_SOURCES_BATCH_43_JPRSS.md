# Batch 43 — the feeds Japanese municipalities declare on their own sites

`collectors/pivot/table/hp3b43_jprss.c` · manifest `docs/candidate-sources-batch43.jprss.txt`

## Where the candidates came from, and what did NOT work

The Digital Agency publishes a CSV of every municipality that has an open-data
programme — **1,648 rows on 2026-06-30** (`20260630_resources_opendata_lg_list_02.csv`
on `digital.go.jp/resources/data_local_governments`), with the municipality's own
open-data page (1,601 URLs) and up to three catalogue sites (1,541 / 396 / 16).
Both axes were measured live on 2026-10-17.

**The catalogue axis yielded nothing.** 1,049 distinct catalogue hosts; 776 not yet
registered. Each was asked for the CKAN, OpenDataSoft, ArcGIS-Hub DCAT and dataeye
catalogue paths and the BODY was judged, not the status
(`docs/discovery-jp-catalogue-hosts-2026-10-17.tsv`): 637 answer 404 on every path,
113 are unreachable from here, 25 answer 200 on every path (a catch-all municipal
site), one is CKAN and holds a single dataset, and three are ArcGIS Hub catalogues
the tree already walks through `/api/search/v1`. The "open-data site" column of
that list is overwhelmingly an HTML page. The big shared platforms (BODIK 281
municipalities, HARP 110, the prefectural CKANs) were all registered already.

**The feed axis is the batch.** Each of the 1,601 municipal site roots was fetched
once and the RSS/Atom feed it declares in `<link rel="alternate">` recorded: 195
sites declare one. A second pass over the 1,335 that declare none read the page
body for anchors to `rss` / `rdf` / `feed` / `.xml` targets. Every candidate URL was
then fetched, classified by its root element (RSS 2.0 → `channel.item`, RDF 1.0 →
`item`, Atom → `entry`), counted, and deduplicated against the 1,400 Japanese feed
URLs already registered (123 of the first pass's 278 candidates were already in the
tree) and then against the tree by `batch_exclusions.py --bin`, which caught six
more under a different scheme or normalisation.

A feed shared by many municipalities — BODIK's platform feed, declared by its 281
members — is ONE row, not one per member.

## What a row is

`mode=xml`, the array path its format dictates, `id_keys=guid,link` (Atom:
`id,link.@href`), `date_keys=pubDate,dc:date` (Atom: `updated,published`), hourly.
Each description carries the item count and newest date **from the fetch that
qualified it**, never a guess. Feeds that answered 200 with zero items were
rejected (`OK`/0 in the rejects file): a feed verified live that emits nothing is
the `EMPTY_RESULTSET` failure by another name.

## Gates

(filled in from the run below)
