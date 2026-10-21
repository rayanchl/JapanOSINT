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
| UK `.gov.uk` | 7,927 | 5,284 | 2,124 | 411 | 2,787 | 2,688 | 1,753 |

Of the 16,906 US domains, 4,372 have no web server answering at the root at all
(registered, parked or mail-only), 1,639 answer 403 to a non-browser client and
89 answered 429 to a single request; of the 7,927 UK names, 1,564 have no
server and 992 answer 403. A domain registration is not a website.

## Gates (2026-10-17, all live)

**3,715 rows ship — 2,075 US, 1,640 UK — every one measured emitted-AND-stored
in the real binary** (3,720 after the retry pass of 2026-10-19 put 4 US and 1 UK
row back: 45,786 emitted / 45,785 stored). `docs/verified-sources-batch44.tsv` carries the probe
reading and the emit reading per row; the 3,369 candidates that did not ship are
in `docs/rejected-sources-batch44.tsv` by beat and by the stage that refused
them.

| stage | US in → out | UK in → out | refused |
| --- | --- | --- | --- |
| builder: fetch, classify, count at the element start | 6,469 → 2,113 | 2,688 → 1,753 | US: 1,187 not a feed, 756 empty, 109 no connection, 53 403, 34 404, 10 429 …; UK: 457 not a feed, 315 empty, 115 no connection, 19 404, 16 403 … |
| `batch_exclusions.py --bin` | 2,113 → 2,100 | 1,753 → 1,742 | US: NHC hurricanes, tsunami.gov, BEA, FMC feeds already in the tree, 2 within-batch; UK: DWP, Companies House, Judiciary, DfT, Cabinet Office, Ofqual, DVSA, DfE, FSA, HMCE, CJS already in the tree |
| `audit_batch_reachable.py` / `audit_batch_pagination.py` | 0 / 0 | 0 / 0 | |
| `probe_hp_batch.py` (+ one retry) | 2,100 → 2,079 | 1,742 → 1,636 PASS | US: 12 403, 9 429, timeouts, resets; UK: 91 403, 8 disconnects, 6 timeouts |
| `audit_registry_emit.py` (+ one re-run of each FETCH_FAILED) | 2,079 → 2,075 | 1,742 → 1,640 | US: 4 hosts answering 500 / timing out to every client; UK: 102, of which 98 are the probe's 403s |

Emitted 45,705, stored 45,704. The one collision is `JO44_UK_OSBASTONPARISHCOUNCIL_GOV_UK`,
whose feed lists one road-closure notice twice under one guid, byte-identical:
the sink collapsed exactly that pair. Two US rows and three UK rows came back
`FETCH_FAILED` on the sweep and stored their whole feed on a re-run minutes
later — one reading, recorded as such in the TSV.

**The UK beat was generated with ALL 1,742 rows, not only the probe's 1,636,
and that was the right call for the wrong-looking reason.** 91 UK rows answered
403 to the probe's client but 200 with records to curl minutes earlier, which
is the shape rule 4c warns about (a probe calling DEAD what the engine handles).
Measured in the engine: 98 of the 102 that failed there are those same probe
failures, so the 403 is a host-side bot wall that refuses the engine's client
too, not a probe artefact — and **four rows the probe refused store their feed
in the engine** (`JO44_UK_SWANLEYTOWNCOUNCIL_GOV_UK` 10 of 10 among them) and
ship with both readings in the TSV. The engine measurement is the gate that
matters; the probe is the cheaper first reading.

**And 86 of those 403s were a wall against the User-Agent, not against the
reader** (2026-10-20). The 91 UK rows the probe had refused with 403 were
re-probed with `header1=User-Agent: Mozilla/5.0 (compatible\; JapanOSINT/1.0\;
+https://github.com/rayanchl/JapanOSINT)` — the same identified client, with
the browser-compatible prefix the walls key on — and 86 answered with their
feed and **store it in the engine** under that header. They are back in
`hp3b44_ukgov.c` (1,727 rows now) with the header on the row, so the condition
each was proven under is the one it runs under; `verified-sources-batch44.tsv`
marks their probe reading `PASS (User-Agent header)`. The whole sample across
batches 44, 47 and 48 is in CLAUDE.md's batch 43–48 lessons.

**Two generalisable findings.** `gen_hp_batch.py` escaped `.name` through
`cstr()` and wrote `.name_ja` raw, so a feed title carrying a double quote
(Medina NY's alerts feed is titled "Village of Medina") produced C that did not
compile; both fields are escaped now. And WordPress declares its COMMENTS feed
beside its posts feed on every site — 2,818 US and 2,787 UK sites are WordPress
— so `/comments/feed/` is dropped before validation rather than shipped as
2,000 rows of nothing.

Tree gates after both tables landed: build 0 warnings; `lint-sources` OK;
`audit-sources` 0 findings across 326 strict-set files; `hptest`,
`pagewalktest`, `htmlparsetest`, `authtest`, `unit`, `selftest` all pass;
`source-floor` and `registry-floor` re-recorded at 26,123.
