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

## Gates (2026-10-17, all live)

**1,156 rows ship, every one measured emitted-AND-stored.** 546 municipalities,
679 RSS 1.0 (RDF), 387 RSS 2.0, 90 Atom. `docs/verified-sources-batch43.tsv`
carries the probe reading and the emit reading per row; the 749 candidates that
did not ship are in `docs/rejected-sources-batch43.jprss.tsv` by the stage that
refused them.

| stage | in | out | refused |
| --- | --- | --- | --- |
| pass 1: feed declared in `<link rel=alternate>` on 1,601 site roots | 278 candidate URLs | 147 | 123 already registered, 18 empty, 12 404, 6 reset, 2 timeout |
| pass 2: anchors to rss/rdf/feed/.xml on the 1,207 roots that declare none | 1,596 candidate URLs | 1,098 | 218 not a feed, 171 already registered, 48 empty, 23 404, 20 reset, 6 timeout, 4 500, 3 403, 3 202, 2 406 |
| re-measure: every merged row's `<item`/`<entry` count at the ELEMENT START | 1,245 | 1,169 | 60 empty (the builder had counted RSS 1.0's `<items>` container as a record), 12 reset, 4 timeout |
| `batch_exclusions.py --bin` | 1,169 | 1,163 | 6 endpoints already read by `hp3b23`/`hp3b25`/`hp3b30`/`vsrc2` rows |
| `audit_batch_reachable.py` / `audit_batch_pagination.py` | 1,163 | 1,163 | 0 / 0 |
| `probe_hp_batch.py` (official; 18 transport failures re-probed once) | 1,163 | 1,156 | 3 hosts whose TLS the probe cannot negotiate, 2 resets, 1 403, 1 blank error |
| `audit_registry_emit.py --ids-file` | 1,156 | 1,156 | 1,155 OK, 1 COLLISION, 0 EMITS_NOTHING |

Emitted 45,217, stored 45,216. The one collision is `JO43_MUNI_112127_3`
(Higashimatsuyama's list5), whose feed carries one notice three times under one
link, two of the copies byte-identical: the sink collapsed exactly the identical
pair and kept the third, which is the guard working. `JO43_MUNI_363219` came back
`FETCH_FAILED` on the sweep and 10 of 10 on a re-run minutes later — one reading,
recorded as such in the TSV.

One record of each shape was read back out of the database before the batch was
believed: an RSS 1.0 row keeps `dc:date`, `dc:creator`, `dc:subject`,
`dc:publisher` and the `@rdf:about` link; an Atom row keeps `link.@href`,
`category.@term`, `summary` and `content`. Titles, dates and links land in their
columns for all three shapes.

**The 60 empty feeds are the generalisable finding.** Almost all are a
municipality's 緊急 (emergency) feed — `kinkyu.xml`, `emergency.xml` — which is a
well-formed RDF document with an empty `<rdf:Seq>` on every day there is no
emergency. The probe used to call that body UNPARSEABLE (count_feed scores 0,
count_xml needs two same-tag children); it now says `EMPTY_RESULTSET`, which is
what it is. They are not registered: a feed that has never been seen to emit is
not verified, however plausible its purpose. The list is in the rejects file for a
future pass that can prove one of them during an actual emergency.

Tree gates after the table landed: build 0 warnings; `lint-sources` OK;
`audit-sources` 0 findings across 324 strict-set files; `hptest`,
`pagewalktest`, `htmlparsetest`, `authtest`, `unit`, `selftest` all pass;
`source-floor` and `registry-floor` re-recorded at 22,408.
