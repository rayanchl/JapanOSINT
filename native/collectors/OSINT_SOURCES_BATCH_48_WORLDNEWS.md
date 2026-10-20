# batch 48 — news outlets and international organisations, worldwide

`collectors/pivot/table/hp3b48_{igo,news}.c` · manifests
`docs/candidate-sources-batch48.{igo,news}.txt`

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
| news | 8,482 (174 countries) | 5,998 | 3,147 | 1,216 | 5,890 | 4,303 | 4,189 (115 already in the tree) |

Of the news hosts 1,292 answer 403 to a non-browser client and 787 have no
server; 28 feeds answered 429 to a single request.

## Gates (2026-10-18, all live)

**4,472 rows ship — 492 international organisations, 3,980 news outlets from
137 countries — every one measured emitted-AND-stored in the real binary**
(4,494 after the retry pass of 2026-10-19 put 2 and 26 rows back and the merge
with main removed six news feeds its own Japan beats had added first: 494 /
4,000, 156,288 emitted / 156,084 stored).
155,994 records emitted, 155,790 stored. `docs/verified-sources-batch48.tsv`
carries the probe reading and the emit reading per row; the 2,369 candidates
that did not ship are in `docs/rejected-sources-batch48.tsv` by beat and stage.
The news beat's weight: US 2,152, Norway 520, Spain 398, Italy 354, Russia 316,
India 286, Korea 252, Germany 220, Mexico 206, France 206.

| stage | igo in → out | news in → out |
| --- | --- | --- |
| builder: fetch, classify, count at the element start | 780 → 515 | 5,890 → 4,303 |
| `batch_exclusions.py --bin` | → 500 | → 4,189 (El Khabar found a second time after a row repair, dropped at the end) |
| reachability / pagination | 0 / 0 | 0 / 5 SPIP `?page=backend` rows, `pagination_ok` |
| `probe_hp_batch.py` | 492 PASS | 3,851 PASS |
| generated with EVERY row; `audit_registry_emit.py` + one re-run of each FETCH_FAILED | → 492 (8 failed twice) | → 3,980 (208 failed twice) |

**175 rows the probe refused ship on the engine's measurement** (2 igo, 173
news) and carry both readings. The 10 collisions were each measured against
the feed itself: nine are the feed's own byte-identical duplicates (one
Ukrainian outlet lists 200 items under 62 keys, 138 copies identical), and the
tenth is a single-page feed with 61 repeated keys where the sink collapsed 18
items identical after flattening — a single page cannot skip, so nothing is
lost that was not a duplicate.

**Three things from this batch:**

* **The builder hard-coded `category=government` and
  `record_type=government-notice`** since batch 44, so the first generation of
  the news beat labelled newspapers as government notices. It takes category,
  record type and tags as arguments now (`tools/build_feed_manifest.py`); the
  news rows are `news` / `news-article`, the igo rows
  `international` / `organisation-notice`.
* **Wikidata's class is a claim, not a fact**: it calls Capcom a think tank.
  The class moved out of the row name into the description, where it is
  stated as Wikidata's. The country tag stays in the name.
* **A title with a pipe splits a manifest row.** Three Algerian dailies (El
  Khabar, El Bilad, Le Soir d'Algérie) were dropped by the row repair and
  rebuilt from their discovery records. The pipe was in the feed's DATE cell,
  the one field the builder did not sanitise; it does now. El Khabar then
  turned out to be in the tree already.

Tree gates after both tables landed: build 0 warnings; `lint-sources` OK;
`audit-sources` 0 findings across 331 strict-set files; `hptest`,
`pagewalktest`, `htmlparsetest`, `authtest`, `unit`, `selftest` all pass;
`source-floor` and `registry-floor` re-recorded at 47,115.
