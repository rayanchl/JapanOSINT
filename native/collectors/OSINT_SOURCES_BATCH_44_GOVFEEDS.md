# Batch 44 — the feeds US and UK public bodies declare on their own sites

`collectors/pivot/table/hp3b44_{usgov,ukgov}.c` · manifests
`docs/candidate-sources-batch44.{usgov,ukgov}.txt`

## Method

Batch 43's method, applied to two public registers of official domains rather
than one agency's list of municipalities. The register is the seed; nothing in
this batch was found by guessing a path.

* **US**: CISA's register of every `.gov` domain
  (`cisagov/dotgov-data`, `current-full.csv`, 16,906 domains on 2026-10-17:
  9,201 city, 2,713 county, 1,438 state/territory, 1,174 federal executive,
  1,109 special district, 671 election offices, 289 tribal, 121 federal
  legislative, 75 school districts …). Organisation, type, city and state come
  from the register and are the row's name.
* **UK**: the Cabinet Office's list of `.gov.uk` domain names as of
  31 March 2026 (7,927 after dropping DNS-challenge and malformed entries). The
  register carries only the domain, so the row's label is the feed's own title.

Each domain's root (`https://<domain>/`) was fetched ONCE, following redirects,
and three things read from the body (`discover_feeds.py`, one pass, no second
crawl): the feeds it DECLARES in `<link rel="alternate" type="application/rss+xml
| atom+xml">`, anchors whose target or label says `rss` / `atom` / `feed` or ends
in `.xml` / `.rdf`, and whether the site is WordPress (which declares `/feed/`
and `/comments/feed/`; the comments feed is dropped before validation — comment
threads on a government site are not a register of anything).

Every candidate URL was then fetched, classified by its root element (Atom →
`entry`, RSS 1.0 → `item`, RSS 2.0 → `channel.item`), its records counted at the
ELEMENT START (`<item[\s>]`, the batch-43 lesson), and deduplicated against the
25,361 endpoint URLs already in the tree and then against the tree by
`batch_exclusions.py --bin`. A feed that answered 200 with zero records was
rejected (`EMPTY_RESULTSET`): a feed verified live that emits nothing is not
verified.

## What a row is

`mode=xml`, the array path its format dictates, `id_keys=guid,link` (Atom:
`id,link.@href`), `date_keys=pubDate,dc:date` (Atom: `updated,published`),
hourly. Each description carries the item count and newest date **from the
fetch that qualified it**.

## Discovery, measured

| register | domains | root answered 200 | declared a feed | anchor hints | WordPress | candidate URLs | feed with ≥1 record |
| --- | --- | --- | --- | --- | --- | --- | --- |
| US `.gov` | 16,906 | 10,489 | 2,602 | 1,159 | 2,818 | 6,469 | 2,113 |
| UK `.gov.uk` | 7,927 | (see Gates) | 2,124 | 411 | 2,787 | | |

Of the 16,906 US domains, 4,372 have no web server answering at the root at all
(registered, parked or mail-only), 1,639 answer 403 to a non-browser client and
89 answered 429 to a single request. A `.gov` registration is not a website.

## Gates

(filled in from the run below)
