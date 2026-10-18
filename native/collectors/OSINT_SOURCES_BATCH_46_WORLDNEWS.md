# Batch 46 — news outlets and international organisations, worldwide

`collectors/pivot/table/hp3b46_{igo,news}.c` · manifests
`docs/candidate-sources-batch46.{igo,news}.txt`

## Method

Batch 45's Wikidata seed on two higher-value classes of body. Each item that
carries an official website (P856) and is a direct instance of one of the
classes below was harvested with `tools/wd_harvest.py` — one cheap SPARQL query
per (country, class), paced at one request every two seconds, because a
world-wide query times out — and each site root was fetched ONCE for the feeds
it declares or links (`tools/discover_feeds.py`), every candidate URL fetched,
classified by its root element, counted at the element start and deduplicated
against the tree (`tools/build_feed_manifest.py`, `batch_exclusions.py --bin`).

* **`igo`** — international organization (Q484652), intergovernmental
  organization (Q245065), think tank (Q1137109). These carry no country, so
  they were harvested in one country-less query each: 2,141 hosts.
* **`news`** — newspaper (Q11032), daily newspaper (Q1110794), online
  newspaper (Q1153191), news website (Q17232649), news agency (Q192283), per
  country across 196 sovereign states.

A news outlet's feed is what it publishes, in its own order, with its own
dates: the exact thing an analyst wants from a region's press, and the shape
every gate in this tree measures best.

## What a row is

`mode=xml`, the array path its format dictates, `id_keys=guid,link` (Atom:
`id,link.@href`), `date_keys=pubDate,dc:date` (Atom: `updated,published`),
hourly. Each description carries the item count and newest date **from the
fetch that qualified it**.

## Discovery, measured

| beat | hosts | root answered 200 | declared a feed | anchor hints | candidate URLs | feed with ≥1 record | after exclusion |
| --- | --- | --- | --- | --- | --- | --- | --- |
| igo | 2,141 | 1,687 | 555 | 134 | 780 | 515 | 500 (15 already in the tree) |
| news | | | | | | | |

## Gates

(filled in from the run below)
