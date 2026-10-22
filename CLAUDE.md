# JapanOSINT — house rules

Read these before writing collector, pipeline or API code. Both rules are about
the same thing: what we display is exactly what we actually obtained.

## 1. Never fabricate data

Real fetch or honest empty. No fixture rows, no placeholder records, no
hardcoded registry/source names emitted as if they were findings, no seeded
values standing in for a failed fetch. A failure degrades to an explicit
`error` / `not_found` / "needs credential" note, never to invented content.

**That note did not exist until 2026-10-06.** The engine could not invent a
record, so the first half held — but a refused fetch stored NOTHING, which made
it identical in the database to a source that ran and found nothing:
`records=0 stored=0 rc=0`, the invisible nothing this rule names. Eight
`collector-shape-notice` kinds covered a body whose SHAPE surprised us; a body
we were REFUSED had none. `lib/hpengine.c` now stores one
**`collector-fetch-failure`** record per (source, entity) — upsert-keyed, so a
permanently dead source carries one row that updates rather than one per run —
naming the HTTP status, whether the body parsed, and the remedy (a `key_env`, a
per-host gap in `core/hostgate.c` for a 429, or re-reading an endpoint that has
moved). Found by measuring the 45 detail-hop rows: eight `il-knesset-*` rows
whose OData service 303s to an HTML maintenance page, and `us-courtlistener-*`
answering 429 — nine registered sources storing zero and saying nothing.

It changes two things that read the run line. A dead source now reports
`records=1 stored=1`, so `audit_registry_emit.py` counts engine notices
separately from records and has a `FETCH_FAILED` verdict; and `hptest`'s
404/5xx assertions now pin zero RECORDS plus one disclosure, which is what they
always meant.

Full audit of how each collector behaves today:
`native/collectors/SOURCE_REALITY_REPORT.md`.

**Every registered source is now proof-of-life verified.** Batch 14's 1,001
unverified `csrc14_*` candidates were probed and promoted (594 PASS →
`vsrc14_*`); batch 15 added 1,029 more (`vsrc15_*`, see
`docs/verified-sources-batch15.md`). Rejects are kept as data in
`docs/rejected-sources-batch{14,15}.tsv`. No `csrc14_*` file remains.

**The registered count is 57,527** (2026-10-22): 57,536 less nine rows that
duplicated another row once their URL's `&amp;` was decoded. **A literal
`&amp;` in a row URL is not always a bug to fix**: it sends `amp;type=…`,
which the server ignores, and 1,076 rows had one. Decoded and re-measured
(`docs/amp-decode-2026-10-22.tsv`), 714 store as much or more (16 of them
only once decoded), but 206 stored
LESS — a newspaper search whose real filter finds nothing, a Joomla row then
served the Atom it asked for against an RSS declaration, a CMS honouring a
small `displayCount` — and keep the `&amp;` with a marker on the row. Measure
both URLs before changing one. Ledger: `docs/SOURCE_COUNT_LEDGER.md`.

**The registered count was 57,536** (2026-10-21): main's 57,415 (below) plus
this branch's 121 on the same 56,955 base — batch 51's 136, less 29
takeovers, plus 15 browser-profile feeds, less one duplicate FAA row — merged
with no shared endpoint. On this branch alone the count was **57,076**.

**Browser profile for hosts that refuse every honest agent** (2026-10-21, the
repository owner's decision, reversing the earlier "never impersonate a
browser" rule FOR LISTED HOSTS ONLY): `HA_BROWSER` entries in
`core/host_agents.def` (below). `http_request()` gives a request to such a host a browser User-Agent plus
`Accept-Language` and `Sec-Fetch-*` (replacing any `User-Agent` the caller set)
and keeps the caller's `Accept` — NOAA answers an HTML `Accept` with a 202
challenge, and `www.bls.gov` needs the `Sec-Fetch-*` headers, not just the
agent. Every other host still identifies itself. Admit a host only after
bisecting it: engine agent, the agent without "collector", a bare
`JapanOSINT/1.0`, the `Mozilla/5.0 (compatible; JapanOSINT/1.0; …)` prefix —
and give the host the cheapest agent that passes. Listing `registry.faa.gov` revived `FAA_REGISTRY` (31 fields:
owner, address, Mode S) and `US_FAA_AIRCRAFT_REGISTRY`, which was asking for
`Nametxt=` (the form) where `nametxt=…&sort_option=1` returns results: 1,729
aircraft for one operator over 36 pages, keyed `N-Number+Serial Number` because
an N-number can hold a reservation row and an aircraft row. Since 2026-10-21
each hit also fetches its own N-number page (`detail_html_kv`, below), so a
record carries the full registration (37 fields for an aircraft: owner and
address, Mode S octal and hex, engine, airworthiness, certificate dates; a
reservation's own fields for a reserved number). Measured: one operator's
aircraft, 37 of 37 fields; SOUTHWEST AIRLINES, 743 pages fetched in the
120 s detail budget and 986 marked `_detail_pending` with the notice saying so. A third FAA row,
`FAA_NNUMBER_REGISTRY`, read the same page as anchors — once the 403 lifted it
stored the site's navigation links as aircraft, a confident wrong answer — and
is removed as a duplicate of `FAA_REGISTRY`. **When a fix lets a refused row
through, read what it stores**: a 403 notice is honest, a page of nav links is
not. And `sec-reliefweb-headlines` was never an agent problem: its URL 301s to
plain `http://`, where a firewall answers a zero-byte 202; the curl CLI hid it.

**One per-host User-Agent table: `core/host_agents.def`** (2026-10-21). It was
three mechanisms with three precedences — `UA_OVERRIDE` and `BROWSER_HOSTS` in
`core/httpclient.c`, and 471 per-row `.headers` User-Agents on 320 hosts
(batches 44–48), where the row's header beat `UA_OVERRIDE` and `BROWSER_HOSTS`
beat both. Now one line per host names one agent kind (`HA_NOCOLLECTOR`,
`HA_CONTACT`, `HA_RCORP`, `HA_COMPAT`, `HA_BROWSER`), and for a listed host the
table's agent goes on the wire whatever a row set; an unlisted host keeps a
row's own header, else `JO_USER_AGENT`. 403 hosts. `./bin/japanosint
--host-agents` prints it (host, kind, agent, extra headers), and
`tools/probe_hp_batch.py` reads the same file, so the probe sends what the
engine sends. Measured before the move: no other source fetches from the 320
compat hosts, so the wire did not change, and the emit sweep of all 586 rows on
listed hosts read 562 OK; the rest failed under every agent tried, including
the old one (down hosts, 403 walls, two browser-profile hosts refusing this
environment that day). The move dropped `midjourneyvideo.net`: a gambling site
on a city's lapsed domain, whose row had already been removed as a takeover,
was still in `UA_OVERRIDE`. `tests/unit/test_host_agents.c` pins the
resolution.

**The registered count was 57,062** (2026-10-21): the 56,955 below, plus
**batch 51's 136** Hong Kong and Macao government feeds and China News Service
(`collectors/sources/cn_feeds_batch51.c`, read off the publishers' own RSS
directories), less **29 rows found taken over or injected by a sweep of every
feed in the tree** (42,593 URLs, 52 flagged, each read by hand —
`docs/removed-sources-2026-10-21-takeovers.tsv`). New tool:
`./bin/japanosint --discover-feeds <sites> [--jobs N]` (`core/feeddisc.c`)
asks each homepage which feeds it declares and verifies them through the
engine's own client, flagging `TAKEOVER_SUSPECT` on item titles.

**The registered count was 56,955** (2026-10-21): main's 53,739, less six

**Main's count was 57,415** (2026-10-21): 57,409 plus the six batch-49 rows
the next-day retry pass restored (below). 57,409 was main's 56,955 (below) plus
this branch's 453 on the same base — the 491 rows the User-Agent header
recovered from bot walls, less 19 of them whose feed main's batch 50 reads
(merged first, so its rows stay) and one whose domain the takeover scan below
caught (`bnm.mg`, casino spam under Madagascar's standards bureau — the other
490 were clean), plus `PLOS_CORPUS`, less the 19 rows batches 34 and 35 lost to
their first measurement — then plus one, the ods147 pair that replaced a
misnamed row (the ELIA paragraph under "Cadence policy"). The ledger has both
sessions' days.

**The count was 56,955** (2026-10-21): main's 53,739, less six
`jp_feeds_batch43/44` feeds measured dead, less 25 rows of batches 44/47/48
whose official domains now publish gambling spam (below), plus **batch 50's
3,247 government and institutional RSS/Atom feeds** for Japan, the US, the UK,
France and China (`collectors/sources/{jp,us,uk,fr,cn,intl}_feeds_batch50.c`,
`docs/verified-sources-batch50.tsv`, 3,237 OK on the emit sweep, 54,800 emitted
/ 54,802 stored). Authored as batch 45 and renumbered twice at merge time — to 49, then 50 —
because main's 45, 47, 48 and 49 all landed first. Details: `docs/SOURCE_COUNT_LEDGER.md`.

* **An official domain is not forever, and no record count can see it lapse.**
  Wikidata's `P856` official website — and any register like it — lists the
  domain a body USED to hold. Scanning the ITEM TITLES of 2,729 autodiscovered
  feeds found 27 that serve Vietnamese/Indonesian betting, crypto-casino or
  pharma spam under a city's name (Hudson, Madison, South Houston, Yokohama's
  `kaikou.yokohama`…), and the same scan over main's batches 44/47/48 found 25
  more, including a hacked `pinelandtx.gov` and `ffc.gov.pk`. Every one passed
  probe AND emit with healthy numbers. Page titles are not enough — South
  Houston's homepage title was its own name while its feed was slot spam. Scan
  item titles for gambling vocabulary and for a script the source has no reason
  to publish in, then read the hits: "cialis" is inside "Commercialisti" and
  "casino" inside "Liceo Pascasino". Removals and their evidence:
  `docs/removed-sources-2026-10-20-takeovers.tsv`.
* **Ask the homepage for its feeds.** Probing `/rss/`, `/feed/` on Japanese
  government hosts found 27 feeds; reading each official homepage's own
  `<link rel="alternate" type="application/rss+xml">` found 961 — the batch-42
  lesson again, one level up.

**The registered count was 53,739** (2026-10-20): batch 49's 5,393 measured rows
(`docs/verified-sources-batch49.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_49_JPWPFEDERATIONS.md`) on 48,346 — 5,393
on 2026-10-20 and six restored by the next-day retry pass.
**Batch 49 is batch 45's staging set registered** — the WordPress collections of
the member organisations of Japan's nationwide federations (medical and
pharmacist associations, JA, chambers of commerce, 法人会, co-ops, JC, the 士業
bodies, fire chiefs, welfare councils, public cultural facilities), found by
reading the federations' own link pages for hosts and asking each host for its
REST root; eleven beats named after the federation that listed the host.
5,405 rows were generated and the engine decided: 5,371 OK on the first pass,
28 given a second reading of which 16 recovered, 12 removed
(`docs/rejected-sources-batch49.emit.tsv`); 518,813 emitted / 518,827 stored
over one pass. **The next-day look at the 12 restored six** (749 / 749): three
were transient, two hosts are user-agent bot walls — and `newspromotion.jp`
resets the HTTP/2 stream for **any User-Agent containing the word
`collector`**, which `JO_USER_AGENT` does ("feed collector"), so what the
engine logged as a transport error was a refusal. A transport failure that
reproduces only with one header is a bot wall; `header1=User-Agent: Mozilla/5.0
(compatible; JapanOSINT/1.0)` is the remedy, URL-free because `lint-sources`
reads a URL inside a header as an endpoint. The six that stay out are a
disabled REST API (401 `rest_disabled`), a disabled route (404) and two
collections that hold nothing. Their declared interval is 21600 as data; the cadence that runs
is the system-wide policy (see "Cadence policy" below).

**The registered count was 48,346** (2026-10-19; 48,343 plus the three
embedding maintenance pods of the infrastructure audit): main's 22,408 — its own
`jp_feeds_batch41..44.c` beats, PR #48's paging cycle and PR #50's batch 45 of
619 WordPress collections — merged into this branch's 47,178, less six news
feeds both sessions had added (main merged first, so its rows stay). **Batch numbers 43 and 44 now name two things**: main's
`collectors/sources/jp_feeds_batch4N.c` beats and this branch's `hp3b4N_*`
tables with `JO4N_` ids — no file or id collides, but read the BEAT, as with
34 and 35. **And this branch's batches 45 and 46 are 47 and 48**: main merged
its own batch 45 (PR #50, `hp3b45_jpwp*`, `verified-sources-batch45.tsv`) while
this branch waited on CI, a direct file collision, so the public-body beats
(`hp3b47_{itgov,frgov,wdgov}`, `JO47_`) and the press beats
(`hp3b48_{igo,news}`, `JO48_`) were renumbered at merge time — ids, tables,
manifests, TSVs, docs and collector keys — exactly as the batch-41 note below
predicts. The count ledger — every
batch from 37 to 48, main's 45 included, what seeded it, what it measured and what it taught — is
`docs/SOURCE_COUNT_LEDGER.md`; this file keeps the rules and the current number.
Both floors are re-recorded at 57,536 and `make source-count` agrees. The
lessons from batches 43–48 that a new batch must not relearn:

* **A registration count is not a website, and a build is not a registry.**
  A quarter of any official-domain register has no server at the root; and
  `MAX_SOURCES` (`registry.c`, now 131,072) silently dropped 2,392 rows at
  43,453 with a clean build — compare a beat's registration count with its
  table, and run `make registry-floor`, before believing an audit.
* **The engine's measurement outranks the probe, in both directions.** Generate
  a beat with every probe candidate and let `audit_registry_emit.py` decide:
  shared hosting that answers 502/403 to twelve concurrent probes stores its
  feed at six, and a 403 that refuses the engine too is a bot wall. Both
  readings go in the verified TSV. Re-run each FETCH_FAILED once; a row that
  fails twice leaves — and a retry pass a day later recovers a few: 63 of the
  2,724 rows batches 44–46 dropped were alive on 2026-10-19 and are back in
  their beats. Never rebuild the binary while an audit runs.
  **And a 403 is usually a wall against the default User-Agent, not against
  us** (2026-10-20): of the 523 dropped rows that had answered 403 to the
  engine's client, 495 answered the probe and **491 store their feed in the
  engine** — 11,159 emitted, 11,159 stored — under a declared
  `header1=User-Agent: Mozilla/5.0 (compatible\; JapanOSINT/1.0\; +https://github.com/rayanchl/JapanOSINT)`:
  the same identified client with the browser-compatible prefix the walls
  key on, nothing hidden. They are back in their beats (86 UK, 328 FR, 58 IT,
  8 Wikidata, 4 IGO, 7 press) with the agent their hosts were proven under —
  first as a header on each row, since 2026-10-21 as an `HA_COMPAT` line in
  `core/host_agents.def`; the four that refused twice stay out. Try that
  agent before calling a 403 dead.
* **A URL changed after validation is unvalidated** (an unescaped `&amp;`
  made Joomla serve Atom where rows declared RSS), **a well-formed feed with
  zero items is `EMPTY_RESULTSET`**, **a substring count of `<item` counts the
  RSS 1.0 `<items>` container**, and **WordPress declares a comments feed
  beside every posts feed**.
* **A collision is a question**: measure the feed against itself (distinct
  keys, byte-identical surplus) before touching a key. Every collision in
  25,863 feed rows was the upstream's own duplicate; a single page cannot skip.
* **Generator defaults and Wikidata classes are claims**: the builder takes
  category, record type and tags as arguments, and a class Wikidata asserts
  goes in the description, never the name. Titles are unescaped,
  whitespace-normalised and pipe-free; a URL carrying a credential is dropped.
* **The interval follows the measured freshness** of each feed (newest item
  within 3 days → hourly, 30 days → 6 h, a year → daily, else weekly); the
  press stays hourly. **And the scheduler re-measures it** (2026-10-20): the
  four feed batches carry `adaptive_interval=1` (`hpengine.h`, through
  `source_def`), and after every run that WORKED `core/scheduler.c` re-reads
  the newest ISO `published_at` the row has stored and re-buckets it by the
  same rule (`sched_freshness_measure`, persisted as
  `source_sched_state.freshness_interval`, and what the row's
  `effective_interval` then shows). Both directions, bounded by the buckets:
  a parish council that goes quiet drifts to weekly, one that wakes before an
  election is hourly on its next run. An undated feed keeps its declared
  weekly (no evidence, no change) and a refused fetch changes nothing (a 403
  says nothing about how often the upstream publishes). It is opt-in because
  a row's author may have set a cadence the item dates cannot see — a
  forecast feed, a window the row exists to be fresh for.
  `tools/tune_intervals.py` is the authoring-time half of the same rule and
  marks the rows it tunes; `test_sched_backoff.c` pins the bucket, the base
  and the measurement. The operator's system-wide cadence policy (the
  "Cadence policy" section) applies ON TOP of this measurement, never under
  it: `sched_row_interval()` is the one composition.

**Batch 45's method is worth more than its 619 rows.** Every `.jp` host a
registered row already fetched was asked whether it also runs WordPress REST —
the homepage's `Link: rel="https://api.w.org/"` names the root (232 of 245),
a bare `/wp-json` only 13 — then for its post types and one page of each
collection; the tool is `tools/wp_routes_sweep.py`. The same sweep over 5,123
federation-member hosts (chambers of commerce, JA, medical associations, fire
chiefs, welfare councils, co-ops, professional bodies) found 5,678 live
collections on 1,372 hosts, kept in `docs/staging-sources-batch45.federations.tsv`
until they are registered as their own batch.

**Batch 33 is the one batch measured end to end against live upstreams** —
43 supply-chain and package-registry rows
(`collectors/pivot/table/hp3b33_supplychain.c`,
`native/collectors/OSINT_SOURCES_BATCH_33_SUPPLYCHAIN.md`). The environment it
was authored in reaches package-registry hosts and only those, so probe
`--check-filter`, `audit_batch_emit`, per-pivot `--run` and the registry sweep
all ran for real: every row has an emitted-AND-stored reading, and the counts
in its descriptions are what the bodies held. The same is true of the two small
beats that followed it — batch 34's `ndjson` and batch 35's `strings` — which
is why those three carry `docs/verified-sources-batch3{3,4,5}.tsv`. Note what
that environment constrains: `docs/rejected-sources-batch33.tsv` lists the hosts
the policy refused, OSV, deps.dev, libraries.io and ecosyste.ms among them, and
the staging set now at `docs/candidate-sources-batch37.*.txt` (144 rows on 140
hosts when this was measured, 80 after its dedupe) has **zero** hosts reachable
from it, so it stays unprobed.

**Batch numbers 34 and 35 each hold beats from TWO sessions, and the
`verified-sources-batch3{4,5}.tsv` files describe only one of them.** Batch 34
is `ndjson` (1 row, measured) plus `ckanbulk`/`fdsnstation`/`fdsnevent`/
`socratacat` (122 rows, measured 2026-10-20 — see below); batch 35 is `strings`
(4 rows, measured) plus `jpweko` (98 rows, measured 2026-10-20). Since that
day the per-batch TSVs cover every beat of the number, each row saying which
measurement it carries. Nothing collides — the beat names keep
every file, id and table symbol distinct — but a per-batch TSV or note is no
longer a statement about the whole number, so read the BEAT, not the batch.
This is the same hazard the paragraph below is about, hit a second time by the
session that had already renumbered itself out of it once.

**Batch numbers are claimed by whoever merges first, so check before you
generate.** Two sessions authored a "batch 33" on 2026-10-04; the one that
merged first kept the number and the other was renumbered to 34 and 35 at merge
time (ids, table symbols, file names, manifests and every textual reference).
The collision would have made `verified-sources-batch33.tsv` describe one batch
while another batch's headers said that file did not exist. The renumbering
then landed on 34 and 35, which a third session had taken in the meantime — so
the rule is not "renumber once and you are safe", it is **re-check at merge
time**, because the number you picked when you generated may have been claimed
while you worked. The same collision existed in docs: a docs-only STAGING set
of 144 global candidates had been written as `docs/candidate-sources-batch31.*.txt`
after the unrelated `hp3b31_*.c` tables had merged under 31. Once those tables
got manifests of their own, a `batch31.*.txt` glob would have fed both sets to
every gate, so the staging set moved — to 36, which a live-measured batch then
merged under first, and so to **batch 37** (`docs/batch37/STAGING_README.md`,
80 rows kept, 64 rejected as duplicates of the tree into
`docs/rejected-sources-batch37.<beat>.tsv`; still not generated into C, still
unprobed). Before picking `<N>`:
`git ls-tree -r --name-only origin/main | grep -oE '(batch|hp3b)[0-9]+' | sort -u`.

**Batches 34 and 35 were authored unfetched (2026-10-04, a session whose
network policy allowed only Anthropic APIs, package registries and GitHub) and
MEASURED on 2026-10-20** — probe with `--check-filter`, the registry sweep for
the scheduled rows, `audit_batch_emit.py` with the manifest for the pivots, one
re-run of each `FETCH_FAILED`. `docs/verified-sources-batch3{4,5}.tsv` now
carry every beat of both numbers, one reading per row;
`docs/rejected-sources-batch3{4,5}.tsv` the rows that left.

* **Batch 34: 139 → 122 rows** (`hp3b34_{ckanbulk,fdsnstation,fdsnevent,socratacat}.c`,
  `native/collectors/OSINT_SOURCES_BATCH_34_SPEC_FAMILIES.md`). Rule 4d
  held: 0 `FILTER_IGNORED` across the 24 Socrata pivots. Removed: the three
  FDSN `event/1` rows whose node does not run that service (the batch's own
  review warned of the shape), and 14 CKAN portals that refused twice. **The
  three COLLISION verdicts were skips**: `current_package_list_with_resources`
  orders by `metadata_modified desc` with no sort parameter, so an edit during
  the walk shifts every later page. All 81 surviving CKAN rows read
  `package_search?sort=id+asc&rows=100` with `page_resume=1` now, re-measured
  the same day: Zürich 942 / 942 (was 942 / 926), govdata's 169,254 datasets
  over a cycle instead of its first 2,000 forever.
* **Batch 35: 100 → 98 rows** (`hp3b35_jpweko.c`,
  `native/collectors/OSINT_SOURCES_BATCH_35_JPWEKO.md`): 0 `FILTER_IGNORED`;
  59 store records for the probe entity, 39 answer an honest empty for it,
  two repositories answered 5xx twice and left. JAIRO Cloud's shared hosting
  answers a burst with 5xx — the probe at twelve saw twelve, the engine at six
  saw five — the reason the probe now paces per host.

Three things this branch measured on 2026-10-21 that the readings above predate:

* **A `page_max` of 20 on every surviving CKAN row reads 2,000 datasets a run
  and cycles, and 14 of them do not need to.** `package_search?rows=0` answers
  `result.count` on the same endpoint those rows now read, so the pages each
  needs is arithmetic: 19 portals measured, **792,984 datasets**, 14 needing
  ≤200 pages and now completing in ONE run (opendata.nhsbsa 2,166, Berlin
  2,627, Aragón 2,425, data.ca.gov 4,675 — each its declared total exactly,
  `stored == emitted`), 5 keeping the cycle at a 200-page cost ceiling
  (Hamburg 246,666, geoscience QLD 192,703, govdata.de 169,142). `page_resume`
  stays on all of them: a walk that completes stores no position, so it is a
  genuine no-op. `docs/measured-page-ceilings-batch34-2026-10-21.tsv`.
* **A `page_max` set to EXACTLY the pages the total needs manufactures a FALSE
  truncation notice.** data.ca.gov at `page_max = 47` stored all 4,675 records
  *and* filed "the page ceiling or a cancel stopped the walk" with
  `more_pages_pending: true`: its last page was short (75 of 100, the
  upstream's own end of data) and our ceiling bit in the same breath, which the
  engine cannot distinguish. At 52 the same 4,675 arrive with no notice. Leave
  headroom — a notice that fires when nothing was lost teaches a reader to
  ignore notices, and that is the one thing rule 2's disclosures cannot afford.
* **NOA was removed as "does not run event/1" and it does.** An FDSN
  `nodata=404` means NO DATA MATCHED, not NO SUCH SERVICE, and that parameter
  is in the row's own URL; a 400 can mean the query was too broad. The test is
  `/fdsnws/<service>/1/version`: NOA answers **1.2.4** and refuses an unbounded
  query with 400, so a rolling 30-day window — `starttime={date:%Y-%m-%d:-30}`,
  never a frozen date — stores **1,985 events** where the row stored nothing.
  The row is back. KOERI really has no event service (404 on `version` over
  https and http and on `application.wadl`, while `station/1/version` is 200)
  and ICGC's is deployed but answers "no matching events found" for every
  window including `starttime=1900-01-01`; both stay removed, and both hosts'
  station services were already wired. **Ask the `version` endpoint before
  concluding a service is absent**, and read a status against the parameters
  the row itself sent. The two surviving catalogues were also still
  `orderby=time` (FDSN's newest-first) at `page_max = 20`: ETHZ is `time-asc`
  at 52 now and stores **46,620 events**, exactly the count an offset bisection
  predicted, and SCEDC cycles ascending.

**21 of the 179 stopped dead on the ceiling, and all 21 are now measured and
fixed** (2026-10-21, `docs/measured-page-ceilings-batch34-2026-10-21.tsv`): 19
CKAN portals at exactly 2,000 (20 pages x 100) and the two surviving FDSN event
catalogues at exactly 20,000 (20 x 1,000). **A round number equal to ceiling x
page size is never the upstream's own total.** Their declared totals come to
**792,984 datasets** for the CKAN 19 alone.

Finding them needed a count the row's own endpoint does not publish:
`current_package_list_with_resources` returns a BARE LIST, which is precisely
why 19 rows could sit at 2,000 looking healthy — there was no total in the
envelope for any gate to compare against. `package_search?rows=0` on the same
host answers `result.count`, so the total comes from the upstream itself.
Offset safety was measured too, not assumed, on all 19: stable (the same
offset twice gives the same ids), disjoint, and the offset honoured.

Fixes follow this file's own 200-page split: 14 rows ≤200 pages get a measured
`page_max` (opendata.nhsbsa 2,000 → 2,166, Berlin → 2,627, Aragón → 2,425,
data.ca.gov → 4,675 — each its declared total exactly, `stored == emitted`);
5 rows over 200 get `page_max = 200` plus **`page_resume = 1`**, so the per-run
cost stays at the count this file calls sane and the collection is read over a
cycle. Verified over two runs against one database: `data.gov.ie` 20,000 then
2,881 = **22,881, its declared total exactly**, and run 2 filed no notice
because the cycle completed and cleared.

* **A `page_max` set to EXACTLY the pages the total needs manufactures a false
  truncation notice.** Measured on `data.ca.gov`: at `page_max = 47` the walk
  stored all 4,675 records and filed `"the page ceiling or a cancel stopped the
  walk"` with `more_pages_pending: true`. The last page was SHORT (75 of 100) —
  the upstream's own end of data — but our ceiling bit in the same breath, and
  the engine cannot tell the two apart. At 52 the same 4,675 records arrive with
  **no notice at all**. So leave headroom above the measured need: a notice that
  fires when nothing was lost teaches a reader to ignore notices, which is the
  one thing rule 2's disclosures cannot afford. (The cycling rows' notices are
  TRUE — they really did stop mid-collection — and say where.)
* **FDSN publishes no total, so the catalogue was sized by bisecting the
  offset** — ~20 one-row requests. ETHZ holds ~46,620 events, oldest ECOS-09's
  year-0250 entry; the walk then emitted exactly 46,620, so the probe and the
  engine agree to the record. SCEDC answers at offset 200,000 with a 1987 event
  and times out at 1,000,000, so its depth is recorded as **unmeasured rather
  than absent** and it cycles. Both were `orderby=time` — FDSN's newest-first —
  which is the drift hazard this file names: new events land at the HEAD of the
  collection the walk is reading. Both are `time-asc` now, verified accepted
  per host.

Two review findings from batch 34 that generalise:

* **A Socrata domain's own `/api/catalog/v1` is NOT scoped to that domain.**
  Without `domains=` and `search_context=`, a pivot returns matching datasets
  from every Socrata portal and attributes them to this one — rule 4d's
  confident wrong answer. 64 existing rows already passed `domains=`; batch
  34's 24 did not until review.
* **The FDSN spec defines services; a node runs only some of them.** EIDA nodes
  serve dataselect/station/availability, not event; USGS ComCat and EMSC serve
  event, not station; IRIS retired fdsnws-event. 11 batch-34 rows asking a host
  for a service it does not run were dropped before any probe.

**A CSV delimiter that defaults to comma silently unparsed a working row.**
`lib/hpengine.c` sets `char delim[64] = ","` and does NOT sniff. FDSN's
`format=text` is PIPE-delimited, so batch 31's `FDSN_STATION_INVENTORY` — cited
below as storing 151,303 records — was putting each whole line into ONE cell:
`col1`, `col2`, `col3` and `col5` all resolved to nothing, the title fell back
to the raw line, and no station got coordinates. The record COUNT was real,
which is precisely why it read as healthy for weeks. Fixed 2026-10-04 with
`csv_delim=pipe;csv_comment=#;csv_no_header=1`, and its `id_keys` moved from
`col1` to `col0+col1+col6` because a station code is not unique across networks.
**A row's record count says nothing about whether its FIELDS were parsed** —
check one record's `properties` the first time a non-comma CSV row ships.

**Batch 31 has now had its `--check-filter` run (2026-10-06), and 65 rows
failed it.** All 360 rows of `docs/candidate-sources-batch31.*.txt` were probed
live; the full result is `docs/probe-sources-batch31.tsv`. 51 PASS. Sixty-five
came back `FILTER_IGNORED` — the upstream takes the entity parameter, ignores
it, and returns the same collection for a real entity and for one that cannot
exist — which rule 4d calls a confident wrong answer and this file calls fatal.
They were REMOVED (registry 18,643 → 18,578, both floors re-recorded), after two
were confirmed by hand first, because 65 live sources is not something to delete
on one tool's say-so: `UK_INSOLVENCY_INDIVIDUAL` answered 9,213 bytes for a real
surname and 9,203 for an impossible one; `KE_BRS_BUSINESS_SEARCH` 348,130 and
348,402. The lesson generalises past batch 31: **65 of 360 means roughly one
entity pivot in six was answering about the wrong thing**, and nothing but
`--check-filter` can see it.

The rows that failed for other reasons — 146 HTTP_ERR, 39 UNPARSEABLE, 31
NET_ERR, and the rest — were NOT removed. One probe is one reading and those
verdicts can be transient, a user-agent or a bad afternoon, whereas
FILTER_IGNORED is a property of the endpoint's query handling. They disclose
themselves now instead; see the fetch-failure notice under rule 1.

**Rules 4 and 4b are now measured too (2026-10-11), and the tool you pick
decides whether the answer means anything.** `docs/emit-sources-batch31.tsv`
holds it: of the 295 live rows, **53 OK** (they fetch and store real records
for their probe entity), 99 `EMPTY_UPSTREAM` — which is not "broken", the
endpoint answered and held nothing for that particular entity — and 48 404s,
42 403s and 37 transport failures. The 65 `UNREGISTERED` are exactly the 65
removed above, which is also the proof the removal was complete.

**Use `audit_batch_emit.py` with the MANIFEST for pivot rows, never the
registry sweep.** `audit_registry_emit.py` runs every row with no entity, so
it reported **268 of these 295 as EMITS_NOTHING** — a verdict about sources
that had never been asked anything. The sweep has a `NEEDS_ENTITY` verdict for
exactly this and it could not fire, because an on-demand pivot asked with no
entity returned SILENTLY from `hp_run`: the run line read
`records=0 0ms stored=0`, byte-identical to a row that went and found nothing.
The engine says it now and the sweep matches on it.

Nothing is removed on that reading. One probe is one reading, 403 and 404 can
be a user-agent or a bad afternoon, and each of these rows now files a
collector-fetch-failure record naming its status. The 27 scheduled rows were run on
2026-10-02: 13 store real records (FDSN 151,303; Safecast 30,000; Sejm 15,000;
USGS NWIS 53,789 before the audit timeout …) and 14 need an API key and store a
"gated" notice saying so. Ten rows were removed when the branches merged: seven
dead endpoints (400/401/404 on every run) and three that re-fetch what an
existing collector already reads. See
`native/collectors/OSINT_SOURCES_BATCH_31_GOV_PUBLIC_SURVEILLANCE.md`. Treat
those pivots as registered, not proven, until they are verified.

Batch 31 was hand-written and had no manifest, so no manifest-driven gate could
reach it — and the command its own headers gave instead,
`audit_registry_emit.py --match hp3b31`, measured nothing: **`--match` is a
regex on SOURCE ID, not file name**, and no batch-31 id contains `hp3b31`.
Zero rows selected reads exactly like a clean sweep. Its 16 manifests
(`docs/candidate-sources-batch31.<beat>.txt`) were reconstructed from the C on
2026-10-05 and proven by round trip (360 rows regenerate with 0 field
mismatches); the C stays the maintained copy. Reconstructing them found
`EU_VIES_VAT_VALIDATION` unable to form a valid request for any entity (removed;
`corp_identifiers.c` already does VIES correctly), `JP_NTA_INVOICE_ISSUER`
stripping the `T` the API requires, three Socrata pivots without `domains=`, and
four rows that stopped at page 1. With network:

```sh
cd native
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter
python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch31.*.txt --bin ./bin/japanosint --jobs 6
```

`key_env` rows need the key set (the probe sends `{key}` literally) and
`post_body` rows need `audit_batch_emit.py` (the probe only GETs).

Three verifier/engine traps that pass exposed — check for them before trusting
any "verified" number:

* **Non-ASCII URLs.** `urllib` puts the URL in the request line, which must be
  ASCII, so a CKAN `?q=防災` died inside `fetch()` and was logged as a dead
  endpoint. 196 live sources were being discarded by that alone.
* **Empty result sets.** `{"total_count":0,"results":[]}` used to count as
  `json-object`/1 item and PASS — a source verified live that emits nothing on
  every run, forever. Now `EMPTY_RESULTSET` (172 rows across the two batches).
* **Two-level envelopes.** `lib/jsonlist.c` descended one level looking for a
  label; OpenDataSoft puts the title at `metas.default.title`, so every ODS
  catalogue record was dropped as unlabelled despite carrying a real title and
  licence. The envelope list now takes dotted paths.

## 2. Never discard data — a source that is called must be used exhaustively

**If we spend a request on a source, we use everything it gave back.** Every
record, every field, every page, and the detail endpoint behind each list hit.
Nothing is dropped at a seam between collector, sink, API and client.

A consumer that physically cannot take everything (an LLM prompt, a mobile list)
may bound its own *view* — but the full data must still be fetched, stored and
served, and the bounded view must state in-band how much it is showing out of
how much exists. Silent slicing is the violation.

If anything was left unused, it is reported as data (a
`collector-truncation-notice` record), not as a log line nobody reads.

Rule, examples of violations, and what the shared machinery guarantees:
`docs/SOURCE_EXHAUSTIVENESS.md`.

VJSON collectors page through `jsonlist_emit_paged()`, whose per-page decision
— a next link the server published, else a cursor paired with a page size the
URL declares, else a page number the URL carries when the upstream's own total
says more remains — is `jsonlist_next_page()` in `lib/jsonlist.c`. hpengine rows
declaring `page_walk=1` call that same function, so a row moved from VJSON onto
hpengine pages exactly as it did. Without that (or an explicit `next_path` /
`page_param` / `{page}`) an hp row makes ONE request.

Where the tree actually stands, as `make audit-sources` reports it:

* **strict set — 0 findings across 308 files** (2026-10-07; 256 on 2026-09-27):
  `collectors/pivot/table/hp*_*.c` plus the generated deep-record tables
  `collectors/feed/generated/hp1[0-9]_*.c`. This is the part the Makefile
  gates on, and it is held clean. Run `make audit-sources`
  after adding a table: batch 18 introduced two `single-page` findings here (a
  paged endpoint declared without `page_param`) and they had to be fixed before
  the gate would pass again.
* **the rest of the tree — 0 findings** (1,736 files scanned, 2026-10-21; it
  had drifted to 50, all false: 49 URLs handed to `pw_walk_max`,
  `od_walk_rows_max`, `od_ckan_walk` or `od_ods_catalog_walk` — walkers the
  scanner did not name — plus the `.url` copy of each in its `source_def`;
  1,678 files on 2026-10-07; it
  first reached zero on 2026-08-24, from ~127 across ~74 files). Every first-only, single-page, record-cap, loop-break,
  limit-one and dedupe-ring finding has been read and closed one of three ways:
  the discard was real and was fixed, the line carries an `exhaustive-ok`
  marker with a reason, or it was a scanner false positive and is marked as
  such. The recoveries were not small — IRS exempt-orgs went from 5,000 rows to
  278,014, ESMA from 1 to 1,377, Homebrew from 300 to 23,133, and twelve
  scheduled feeds were asking their upstream for `limit=1`.

  So "zero audit findings" is now true of the whole tree, not just the strict
  set — which makes any NEW finding a regression rather than a number in a
  backlog. Keep it that way: `make audit-sources` is cheap and takes seconds.

* **A bound in a loop's own CONDITION was invisible until `loop-cap`.**
  `while (cJSON_GetArraySize(akas) < 24 && …)` is neither a `#define …MAX`
  (`record-cap`) nor a `break` (`loop-break`), so the tree read "zero" while
  the OFAC consolidated parser dropped a designated person's aliases past the
  24th, TDnet stopped at 100 disclosures a day and EDINET-x at 25 filings. The
  check exists now; all of those are fixed.

* **A SEVENTH shape the six checks cannot see: a record limit the row asks
  the UPSTREAM for.** Every one of the six looks for a bound written in C — a
  `#define`, a `break`, a loop condition, a `seen[]` ring, a first-element
  read, a single request. A limit spelled into a URL is none of them, so the
  tree read "0 findings" while `go-module-index` asked index.golang.org for a
  six-hour window with `&limit=200` and no paging, and stored **200 of the
  22,248 versions that window holds** — 22,048 Go module publications dropped
  per hourly run, at `rc=0`, `records=200`, `stored=200`. Both of rules 4 and
  4b's "the numbers look fine" traps at once. Measured 2026-10-05; that row is
  retired in favour of `GO_MODULE_INDEX`, which pages on
  `next_path=$last.Timestamp`.

  **42 of those candidates are now MEASURED** (2026-10-11,
  `docs/measured-url-limits-2026-10-11.tsv`): each upstream was asked the same
  question with a tenfold limit. **18 are truncating**, 5 are complete, 19 are
  unreachable from here. The first fix was the highest-leverage one:
  `od_udata_collect()` in `collectors/sources/od_shared.inc` made ONE request
  while the envelope published both `next_page` and the upstream's own `total`
  — so three national open-data catalogues stored 20 datasets each. It pages
  now, and discloses a ceiling or a `total` it did not reach:

      data.public.lu   20 -> 3,193 stored (= its declared total)
      dados.gov.pt     20 -> 20,933
      data.gov.rs      20 -> 3,527

  27,653 records from three rows that together stored 60. **All 18 are fixed as
  of 2026-10-12**, each re-measured after, and the readings are in that file.
  Eleven rows went through `lib/pagewalk.c` rather than growing a loop of their
  own, so the continuation rules, the repeat-page guards and the disclosure
  stay in one place. Ten are now COMPLETE (`stored == emitted`, no notice):
  EONET 50 → 7,208, EMSC moment tensors 50 → 48,729, IFRC GO 50 → 6,088,
  OpenAerialMap 50 → 21,437, the PeerTube index 100 → 1,785, Argentina's series
  100 → 271, ECCC hydrometric 500 → 3,366, and Statistics Netherlands' CPI
  table 20 → 159,120, which is exactly its own `$count`.
  Five are bounded and DISCLOSED,
  because their corpus is larger than anything wants: Spaceflight News 2,000 of
  36,457, ECCC SWOB 10,000 of 18,950, and the STAC catalogues.

  **`pw_walk_max()`** is the new piece: an explicit page ceiling for a source
  whose total the UPSTREAM itself publishes. The 20-page default is a brake on
  an unknown upstream and raising it globally would lift that brake for all
  2,727 of them; `JO_PAGE_MAX` still overrides in both directions, and the
  notice now names which ceiling bit.

  **Two of the 15 were not truncation bugs at all, and no record count could
  have shown it.** `dea-stac`'s default order is OLDEST FIRST, so `?limit=20`
  stored the twenty oldest Sentinel-2 scenes over Africa — acquired 2015-10-22
  — and re-stored those same twenty every six hours while 6,594,309 items
  accumulated behind them: 20 of 20 emitted, 20 of 20 stored, every gate green.
  All four STAC rows and both ECCC OGC rows now ask for newest-first
  (`sortby=-datetime`, `sortby=-DATETIME`, `sortby=-date_tm-value` — the SWOB
  key keeps its `-value` suffix or it is a 400). **When a row carries a limit,
  ask what the upstream's DEFAULT ORDER is before deciding the limit is the
  problem** — a bound over the newest records is a window, a bound over an
  arbitrary slice is a lie about what the source holds. And
  `od_stats_nl_cbs` read `/ODataApi/`, which answers `$skip` with HTTP 500 and
  the remedy in its own body: skip exists only on **`/ODataFeed/`**, which
  additionally needs `$format=json` or the 200 is Atom XML. Every CBS row that
  needs to page needs that path.

  Two more generalisable findings from the pass:

  * **A dedupe keyed on a different field from the row's `remote_key` is a
    silent 4b failure.** `gc-swob-observations` deduped on `tc_id-value` while
    keying rows on `tc_id-value` OR `stn_nam-value`, so every station without a
    tc_id passed the dedupe and collapsed at the sink — 25 of 384, measured.
    Dedupe on the identity, not on a field near it.
  * **`lib/seenset.c` already exists**; do not hand-roll a second one. It grows,
    which is the property a fixed ring lacks, and it replaced an O(n²)
    scan-the-whole-array dedupe that was tolerable at 500 records and is not at
    10,000.

  And a bound can be honest and still be a bug: `html_table_parse` allocated
  `width + 1000` cells for EVERY row, so the 1,000,000-row `htmlparsetest`
  fixture asked for **16 GB** and `make` printed `*** Killed` — which reads
  like a flaky runner, not an allocation bug (see the "that job is just flaky"
  note below). Rows grow towards the bound as their cells arrive; peak RSS is
  now 350 MB.

  **Then the SHARED HELPERS, which are where the leverage is** (2026-10-12).
  Three of them took one page while the caller's URL hard-coded a tiny size,
  and because they are shared the same discard was multiplied by every portal
  behind them. Each is now one `pw_walk` call, and the callers' URLs carry the
  offset the engine advances (`od_shared.inc` — the engine never invents a
  parameter, so a caller that forgets `start=0`/`offset=0` gets one page AND a
  notice rather than a silent single read):

  * `od_ckan_collect()` — **11 CKAN portals**, every one asking
    `rows=20&sort=id asc`, so each stored the first twenty datasets by id
    forever while `result.count` sat in the same envelope. Measured and
    re-measured live: Zurich 20 → 942, energydata.info → 1,191, London → 1,306,
    South Australia → 1,929, Berlin → 2,627, Western Australia → 2,917,
    Ontario → 2,964, NSW → 17,192, Queensland → 188,880 emitted / 188,879
    stored, its own `result.count` exactly, across 945 pages. (München 503s and
    open.africa 403s behind bot walls, at `rows=20` as much as at 200 — checked,
    not assumed.)
  * `od_ods_catalog_collect()` — **7 OpenDataSoft catalogues** at
    `limit=20&order_by=dataset_id`. ODS caps `limit` at 100, so the default
    ceiling covers all seven: Vancouver 20 → 200, Brussels → 207, Namur → 295,
    Basel-Stadt → 364, Thurgau → 458, Rennes → 672, Toulouse → 845. Every one
    equals its own `total_count` exactly.
  * `canada-hydrometric-realtime` — the OBSERVATION-level twin of
    `gc-hydrometric` on the same collection, `limit=500` of 19,108,759 with no
    window at all. A one-hour window, paged: 500 → 7,821. The two are a
    deliberate pair (newest-per-station for the map, every observation for the
    series) and must be fixed together.

  **A declared page size is not evidence the server honours it.** `pw_walk`'s
  same-body guard — stop when the next page comes back byte for byte — used to
  run on the INFERRED-size walk only, on the reasoning that a URL stating its
  own size documents an author who knew the paging contract. The London
  Datastore disproves it: `?rows=200&start=0` declares both, and that server
  honours neither, answering every request with its whole 1,306-dataset
  catalogue. 1,306 ≥ 200 reads as a full page, so the walk would have advanced
  `start` to the ceiling, re-emitted the same records every time, and published
  the multiple in the notice **as fact** — the disclosure becoming the
  fabrication. The guard runs on every walk now; it can only ever stop a walk
  early, because two pages of genuinely different records cannot be
  byte-identical. `pagewalk_test.c` pins the shape, and its fixtures now stamp
  a page serial so a multi-page test models a server that actually pages (they
  were arrays of empty objects, hence byte-identical, hence silently testing
  the broken case).

  **An offset or page walk with no STABLE SORT silently skips records, and the
  only visible symptom is a collision.** Measured twice on 2026-10-12.
  OpenAerialMap reported 21,437 emitted / 21,291 stored, and 10 contiguous
  pages of 100 really do hold 6 repeated `_id`s — so the first reading of this
  was "the upstream's own duplicates, nothing to fix". That was a true
  observation and the wrong diagnosis: an index that reorders between requests
  serves one record twice and another NOT AT ALL, and the skip leaves no trace.
  `order_by=_id&sort=asc` takes the duplicates to 0 per 1,000 and the run to
  21,437 / 21,437. Velib was the same: 1,519 / 1,508, and a
  `where=stationcode="13022"` query proved the dataset holds exactly ONE record
  for the code that had supposedly appeared twice. **A collision that is really
  a skip is the worst shape in this file** — the visible half is harmless and
  the invisible half is lost data. So: whenever a row pages by `offset` or
  `page` against a live dataset, give it `order_by` on the row's own id, and
  treat a small collision count on such a walk as a skip until measured
  otherwise. The four ODS records rows and OpenAerialMap carry it now; the CKAN
  rows already had `sort=id asc` and the ODS catalogues `order_by=dataset_id`.

  **Two large portals got a hostgate gap rather than a lower ceiling — and the
  first gap was wrong.** `data.qld.gov.au` is a complete read of ~945 pages and
  `data.nsw.gov.au` ~86. At 250 ms the QLD walk got 76 pages in 47 s and page
  77 answered **HTTP 429**; `lib/pagewalk.c` stopped and filed a notice naming
  the status and pointing at `core/hostgate.c`, which is the only reason the
  pacing was identified as the problem rather than the ceiling. The budget is
  not a per-second rate — twelve requests at 250 ms all answered 200 — so at
  **1,500 ms** the complete read finishes in 72 minutes with no 429 and no
  notice: 188,880 emitted / 188,879 stored, its own `result.count` exactly.
  A public portal is the reason to pace, not the reason to truncate; and a gap
  is a measurement, not a guess you leave in.

  **Six more individual rows, then the rest TRIAGED** (2026-10-12). WHO GHO
  100 → 14,112 (`@odata.count`), openFDA enforcement 100 → 18,002
  (`meta.results.total`), the ArcGIS layer 100 → 900 (`returnCountOnly`), BC
  mineral tenure 500 → 42,234 (`numberMatched`), FediDB 40 → 4,320 (a
  `links.next` cursor nobody followed), and the two EMSC FDSN rows and IODA.
  Two of them had the capability named **in their own header and unused** —
  openFDA's note said `total` "supports skip/limit paging" two lines after "the
  URL is used EXACTLY as verified", and the ArcGIS row emitted
  `exceededTransferLimit` into its properties "so truncation is visible". **A
  capability a comment claims is not a capability anything uses.**

  BC's WFS is the same unstable-order hazard from the other side: it **refuses**
  `startIndex` without `sortBy` ("Cannot do natural order without a primary
  key"). A hard 400 is the better failure — it cannot be mistaken for success.

  And **an honest empty must not read as a dead source.** Windowing the EMSC
  rows surfaced it: FDSN answers an empty result set with HTTP **204 and no
  body**, so `feed_get_json` returns NULL where rule 1's honest empty belongs,
  and `fdsn_quakes` reported `rc=-1`. The Iceland AOI held zero events for the
  last day, so the narrow window would have marked a working source dead every
  quiet day. Check what an upstream does with an EMPTY result before narrowing
  a window.

  The remaining 89 candidates are triaged **with their reasons** in
  `docs/measured-url-limits-2026-10-11.tsv` — ranked/keyed searches where the
  limit is the question, recency windows whose count is generous against a
  measured interval, rows already paged on a stride, and one
  (`trickest_cve`) recorded as **unmeasurable from here** rather than fixed on
  inference. Do not re-litigate them; read the file.

  `tools/audit_url_limits.py` finds the shape, and is deliberately NOT in
  `make audit-sources`: **a URL limit is not automatically a discard and the
  script cannot tell.** A relevance-ranked search pivot asking for the top 15
  has no complete answer to truncate — that limit IS the question. A scheduled
  read of a collection that has an end stores `limit` of however many exist,
  forever, and that is the violation. They are identical in C. The narrow scope
  reported **103 sites across 78 files** when it was written; after this pass it
  reports 89, and every one of those 89 is either a shape the lint cannot see
  through (a row that now pages via `pw_walk`, whose `.url` and page-ceiling
  lines carry `exhaustive-ok` markers) or a triaged non-discard recorded in
  `docs/measured-url-limits-2026-10-11.tsv`. It still is not gated, for the
  reason above: the script cannot tell a question from a bound, so a number
  from it is the START of the work and never the result.

Deliberate exceptions carry an inline `/* exhaustive-ok: <reason> */` marker
(`grep -rn exhaustive-ok`). The marker must sit **on the flagged line itself** —
the scanner matches per line, so a marker in the comment block above the line it
explains is silently ignored and the finding stays. That is easy to get wrong,
because the explanation naturally wants to be a paragraph: put the paragraph
above and a one-line `/* exhaustive-ok: … */` on the line.

Two amendments from the deep-record batch:

* **The gated set is now two globs**, `collectors/pivot/table/hp*_*.c` plus the
  generated deep-record tables `collectors/feed/generated/hp1[0-9]_*.c` — 290
  files as of 2026-10-05 (262 when this was written), 0 findings. `--strict` had to be made repeatable to say that honestly:
  it took a single glob, so passing two kept only the LAST one and the gate
  printed "0 findings" for a set it had never opened.
* **`loop-cap` is a sixth check, and it was not redundant.** A bound written in
  a loop's own CONDITION is neither a `#define …MAX` (`record-cap`) nor a
  `break` (`loop-break`), so nothing could see it. Added after the tree reached
  zero, it immediately found four live caps — all in the OFAC sanctions
  collectors: aliases and sanctions programs capped at 24, addresses at 12, and
  SDN "features" (date and place of birth, nationality, passport and
  national-ID numbers, crypto addresses) at 40. On a sanctions list those are
  the fields screening matches on, so each dropped entry is a false negative on
  a designated person, produced in silence. All four are gone.

  Its own first regex missed the very line it was written for: `\b` before the
  counter list cannot match `cJSON_GetArraySize`, because the underscore is a
  word character. Zero findings from a new check deserves one suspicious look.

* **And it still missed ten (audit of 2026-10-02).** `loop-cap` required a
  trailing `&&` and could not reach a `for` condition; `loop-break` wanted
  `n >=` and missed `if (n++ >= N) break`; `single-page` excused a row by its
  NEIGHBOUR's paging within ±16 lines. Ten live caps passed as "0 findings" —
  OpenCitations stored 50 of 72,181, bike-share GBFS 1,000 of 17,273, J-STAGE
  text search 10 of 9,309. The checks now cover those shapes, scope context to
  the row's own `{ .id = … }` block, and the script runs its own fixtures first
  and exits 2 if any check stops seeing its case.

**Detail hops: 359 rows, and what a generator that rewrites this tree must not
do.** `collectors/gen_detail_hop_upgrade.py` moves a row that has a PROVEN
per-record detail endpoint off VJSON onto hpengine, so the record behind each
list hit is fetched. It reads the `.c` files, never the manifest, for the reason
the batch-tooling section gives: the C is the maintained copy.

It cost 73 sources to learn three things, all the same mistake wearing different
hats — the script could not see something, and deleted it anyway:

* `^(V[A-Z]+)\(` cannot match an underscore, so `VJSON_KEYED`, `VJSON_IDKEYS`,
  `VJSON_PREP` and friends were invisible — and the "no registrations left,
  delete the file" step could not see them either, so files were removed with
  those rows still inside. 20 sources.
* It writes one table per (batch, collector) group of the CURRENT run, so
  running it twice with different row sets rewrote a group's file with only the
  second run's rows. 53 sources. **Regenerate from a clean checkout of the
  directory in ONE run**, never incrementally.
* Its argument splitter did not skip comments, and this tree comments INSIDE
  argument lists; 86 rows parsed as 15 arguments instead of 12 and were passed
  over. That one failed safe, but it failed.

So it now asserts that no id registered in the directory before the run — from
the vsrc macros AND from any hp table already there — is missing after it, and
exits non-zero naming them. The only reason any of this was caught is an id-set
diff against the base branch; `--list-ids | sort | comm` costs seconds and is
the check that actually works.

`VJSON_KEYED`'s IDFIELD and `VJSON_IDKEYS`'s IDKEYS carry over verbatim into
`.id_keys` — `_vjson_idkeys.inc` promises the same `+` composes semantics, and
`hptest` now pins it, because rule 4b's failure is silent. The 45 rows that
moved on that promise are in `docs/detail-hops-need-emit-check.tsv`, and they
were **MEASURED on 2026-10-06**: the promise held. The five 8-part composites
(`af-dportal-trans-*`) store 90,785–96,154 rows each — had `+` been read as
`,`, keying on `aid`, each would have collapsed to its few thousand distinct
activities. Across all 19 COLLISION verdicts the loss is 218,769 emitted
against 218,692 stored, 0.035%, which is the upstreams' own duplicates.

What the pass found instead is what reasoning about `id_keys` could never have
found: **nine rows that fetch nothing at all** — eight `il-knesset-*` whose
OData service 303s to an HTML maintenance page, and `us-courtlistener-*`
answering 429 — and one real defect that was not identity either.
`us-sf-311-open311-requests` lost 5 of 20 because SF's Open311 server
rate-limited both the page walk and the per-record detail hop; pacing
`mobile311.sfgov.org` at 1200 ms in `core/hostgate.c` took it from 20 records
across 5 pages to 90 across 19. **A collision verdict is a question, not a
diagnosis** — measure the upstream before touching a key.

`VJSON_PREP` is deliberately NOT converted: its extra argument is a C function
that reshapes each page before emit, and a declarative row cannot hold code.
VRSS and VGEO stay out too — hpengine has no RSS or FeatureCollection mode.

```sh
cd native
make                 # full build (-Wall -Wextra); the tree is at 0 warnings, keep it there
make selftest        # boot self-test: DB integrity, schema objects, llm probe
make unit            # tests/unit/run.sh against a scratch DB
make hptest          # offline check of the engine's guarantees
make authtest        # JWT / tenant auth
make pagewalktest    # offline check of the paging + disclosure engine
make htmlparsetest   # the one anchor scanner
make lint-sources    # dup ids/endpoints, quarantine-empty, snprintf guards
make source-floor    # fails if a collector stopped registering (tools/source-floor.txt)
make registry-floor  # the same against the binary's registry (tools/registry-floor.txt)
make audit-sources   # scan every collector for discard patterns
tools/ci_concurrency_gate.sh ./bin/japanosint 1.0   # the event loop stays free under load
```

That is the native job in `.github/workflows/ci.yml`; CI also runs TSan and
ASan builds and the client's `vitest` and `vite build`. Run gates with `JO_DB`
pointing at a scratch file — several of them open the default DB otherwise.

**And treat "CI is red" as a question, not an answer.** `ci.yml` carried
`JO_DB: ${{ runner.temp }}/ci.db` as a JOB-LEVEL `env:` entry, where the
`runner` context does not exist yet. That is an unrecognised named-value, which
invalidates the whole FILE rather than the one job: GitHub created each run,
failed it immediately and scheduled ZERO jobs. **Every one of the first 74 runs
failed that way, from this file's first commit** — so for 74 runs "CI is red"
was never a test result, because nothing had ever been compiled or executed.
PR #28 moved it to a `$GITHUB_ENV` step and the runs after it are the first
real ones. A failing run with `total_count: 0` in its jobs list is this shape,
not a gate.

**And the inverse: "that job is just flaky" is also a question.** The `asan` job
failed on `test_export_stream`'s backpressure assertion about 40% of the time
for weeks, and runs #116 and #117 are the SAME SHA with opposite outcomes, which
is as flaky as a signal gets. It was not noise. `xs_pump` set `x->queued = 0`
before the bytes it had taken were counted into `x->unsent`, so between those
two statements the worker's gate (`queued + unsent >= XS_HIGH_WATER`) saw
neither, and a worker racing the first pump queued a SECOND full high-water mark
while the first was in flight: 1.5 MB held against a 1 MB mark, on the one code
path whose entire purpose is that the bound holds. An intermittent assertion
means the invariant is intermittently false. Reproduce it (`2 of 30` runs
locally without ASan) and instrument the quantity the assertion names — the peak
distribution was `38 x 1024 KB, 1 x 1280, 1 x 1536` and said what the bug was —
rather than widening the slack until the red goes away.

If `make unit` dies with `tests/unit/run.sh: No such file or directory` (exit
127) on a tree that came from a Windows checkout, the script has CRLF line
endings and the kernel is reading `#!/bin/bash\r` as the interpreter. The error
names a script that is sitting right there and is executable, so it reads as
"missing file". `.gitattributes` forces LF for `*.sh`/`*.py` on checkout, but it
cannot rewrite files that were checked out BEFORE it existed — and because git
normalises CRLF away on check-in, `git status` stays clean forever while the
working tree stays broken. Repair the tree, don't re-clone:

```sh
git ls-files -z '*.sh' '*.py' | xargs -0 sed -i 's/\r$//'   # content-identical to HEAD
```

`make source-count` (and the `source-floor` gate built on it) counts hp table
rows as well as `REGISTER_SOURCE`, and agrees with the binary. Both floors were
recorded at 18,170 on 2026-10-02, at 18,644 on 2026-10-05 and at 18,645 on
2026-10-06; they had been
left at 16,271 and 13,081, low
enough that batches 31 and 32 could both stop registering with CI green.
Re-record them when a batch lands (`make source-floor-record
registry-floor-record`).

## Source pages API (2026-10-12)

`core/sourceapi.c` serves the Sources, source detail and Collectors pages:
`GET /api/sources/search` (server-searched, paged, `meta.facets`),
`GET /api/sources/:id/detail` (the row as declared via `hp_describe_json`,
schedule, status, data shape, notices, runs, credentials, pacing, provenance),
`GET /api/sources/:id/summary` (small-model summary from measured facts only —
on any LLM failure it returns the status code AND the facts, never prose),
`GET /api/collectors`, `GET /api/collectors/:key`. Three things changed
underneath them that older notes in this file predate:

* **`sources.status` is now written** by `scheduler_run_source()` after every
  run (`sources_row_record`): online / degraded (ok, stored nothing) / offline
  / pending (never ran). Until then nothing wrote it after the seed, so every
  row read `pending` forever and `/api/status`'s online count was the seed.
* **An hpengine row's default collector is its table file's basename**
  (`hp3b25_jpmedia`), not `osint`. Every source has a collector; 554 keys
  (2026-10-19, round 4); `core/collectors.def` labels and groups them (below).
  Round 4 also moved batches 43/44's 400 Japanese web-media RSS feeds off
  `social` (their macro's label) onto `jp_webmedia` under News & media.
  **There is no `osint` collector any more** (round 5, 2026-10-20). Round 3
  moved 968 scheduled feeds that had been authored under that label (Google
  News editions and keyword monitors, world outlets, arXiv, think tanks,
  threat feeds, Reddit multireddits) onto `gnews`, `gnews_monitors`,
  `world_outlets`, `conflict_press`, `trade_press`, `thinktanks`,
  `ngo_rights`, `arxiv`, `science_press`, `finance_press`, `reddit`, `cyber`;
  round 5 split the 492 hand-written entity pivots that remained by their
  CATEGORY: `DEFR`/`DEF` in `_source_macros.inc` label a pivot
  `"osint_" CAT` (string concatenation, so a file-level `#define CAT` works
  too) and every explicit `.collector = "osint"` row became
  `"osint_" <its category>` — `osint_government` 143, `osint_cyber` 120,
  `osint_investigation` 62, … The generator files `osint_<cat>` under the
  category's topical group (`CATEGORY_GROUP`), else under Investigation
  pivots, and labels it "OSINT pivots — <cat>". Safe because the pivot rule
  below never read the label; a scheduled row's `collector` must name its
  beat, not a macro's default.
* **"Entity pivot" is `registry_is_entity_pivot()`** in `registry.c`, the one
  rule both the dispatcher and `service_vec` use: interval 0, not a `_` pod,
  and (hpengine) an entity token in the URL/body OR `filter_query=1` (a bulk
  file filtered by the entity locally — 18 such rows, OFAC's sdn.csv among
  them, read as "never runs" until that counted). `collector=="osint"` is no
  longer part of the definition, and `registry.c`'s NEVER-RUNS check (the
  `make registry-floor` gate) is the complement of this same rule.

* **`fetch_log.bytes_fetched`** (2026-10-12) is the run's transfer size —
  every response body `http_client` received, retries included — and the run
  line ends ` bytes=N requests=M`. NULL = a row from before the column.
* `/api/status` rows carry `collector`, `collectorLabel`, `collectorGroup`,
  `collectorGroupLabel`, `engine`, `kind`; the iOS dashboard groups on
  `collector` under group headings (it grouped on `layer`, which ~1% of
  sources declare).
* **Collectors are grouped** (2026-10-17): `core/collectors.def` is GENERATED
  by `tools/gen_collectors_def.py` from the collector census and gives every
  collector key a `parent` among 22 `grp_*` groups (government, legal,
  corporate, procurement, open data, science, health, environment, space,
  cyber, telecom, infrastructure, transport, media, social, geo, safety,
  sanctions, culture, surveillance, OSINT pivots, internal). **A new beat
  needs a line there**: a collector the def does not list shows under
  `grp_unassigned` on every page and `test_sourceapi.c` fails. **`make
  collectors-def`** (2026-10-20) takes the census from the binary itself
  (`./bin/japanosint --collector-census`, which agrees key-for-key and
  count-for-count with `GET /api/collectors`), writes it under
  `docs/source-pages-revamp/` and regenerates the def — rebuild afterwards,
  the def is compiled in; **`make collectors-def-check`** fails while any
  registered key has no line. The rules live in the script (OVERRIDES,
  KEYWORDS, CATEGORY_GROUP).

* **Per-source operator settings** (2026-10-20): `core/source_settings.c`,
  `GET`/`PUT /api/sources/:id/settings`, and a Settings section on the source
  page. The key set is **CLOSED** — a table in the .c file declares each key's
  type, bounds, default and help text, a `PUT` of anything else is refused by
  name, and the bound is re-checked on every READ because a row can arrive
  from an older build or a hand-edited database. A free key/value store hung
  off every source would be a configuration surface nobody can audit: a typo
  would read as a setting that is silently ignored.

  **`enabled` is separate from the value on purpose**: switching a knob off
  keeps the number an operator tuned, so a re-enable does not start from a
  guess, and a disabled row resolves exactly as an absent one. The payload
  carries `effective` — what the code will actually use — because the toggle
  and the number do not say that between them, and `can_write` from the route
  that enforces the gate, so the page states who may change a thing instead of
  offering a form and then refusing it with 403. Reads are plain-auth; writes
  are operator-gated and audited (`source.setting.put`).

  The one key it ships with is **`http_body_ceiling_bytes`**, which raises
  `core/httpclient.c`'s 64 MB response ceiling for ONE source. That ceiling is
  a security bound — `CURLOPT_ACCEPT_ENCODING ""` means libcurl inflates
  before we see a byte, so a small gzip bomb expands without limit — and it
  fails CLOSED (the transfer aborts, body NULL), which is why
  `ods-public-geonames` needed it: 171,176 records in a ~111 MB
  `exports/jsonl` the ODS records API refuses to page past 10,000. Measured
  2026-10-20: 10,000 → **171,176 stored**, `stored == emitted`, 111,298,693
  bytes. How much memory one run may hold is a decision about the machine it
  runs on, so it belongs to the operator and not to a `#define` in a
  collector. `http_request_max()` carries it per call, thread-local and
  restored on return so it never widens another worker's fetch, it is still
  enforced on the decompressed stream, and **an explicit `$JO_HTTP_MAX_BODY`
  clamps it** — an operator who lowers the ceiling to bound a small deployment
  is not overruled by a collector. It reaches the collector on
  `source_ctx.body_ceiling` (0 = the default), for the reason `resume_in`/
  `resume_out` do: `lib/` and the collectors hold no database code.

  `tests/unit/test_source_settings.c` pins the unknown key, the out-of-range
  write AND read, disabled-is-not-unset, and a setting for a source nobody
  registers; the client's `sourcePages.test.jsx` pins that the page states the
  server's bounds rather than its own, reads a disabled knob as the default,
  and disables the controls for a non-operator.

Plan, audit and as-built notes: `docs/source-pages-revamp/`.

## Cadence policy (2026-10-20)

Every source still declares its own `update_interval_sec` — that is the
measured value it was authored with — but ONE policy sits above all of them:
`core/cadence.{c,h}`. It is the operator's, system-wide, and lives in the
database (`app_settings`, keys `sched.global_interval_sec`,
`sched.min_interval_sec`, `sched.max_interval_sec`), so it survives restarts
and is edited from the Sources page (`GET`/`PUT /api/sources/cadence`, PUT
gated like `/api/sources/:id/schedule`: platform operator). Three shapes:

* **global** — every scheduled row runs at one interval;
* **clamped** — a floor and/or a ceiling the declared values are held inside;
* **declared** — nothing set, each row as authored (the default).

`JO_SCHED_INTERVAL_SEC` / `JO_SCHED_MIN_INTERVAL_SEC` /
`JO_SCHED_MAX_INTERVAL_SEC` bootstrap an EMPTY database only; a stored policy
always wins. The scheduler reads `cadence_apply(declared)` at every site that
used the declared value (due time, boot ramp, backoff base, cancellation
window, search-only and quarantine holds), and re-plans on a change without a
restart: a row whose next due time is further away than its new interval is
pulled in to now + interval. Entity pivots (interval 0) are never touched.

**It composes with the adaptive interval in ONE order** (the two met in a
merge on 2026-10-20, each written without knowing the other existed):
`sched_row_interval()` in `core/scheduler.c` is
`cadence_apply(sched_state_base(...))` — the row's own measurement inside,
the operator's policy outside. A feed whose items say "hourly" runs hourly
under no policy, six-hourly under an operator floor of six hours, and at the
global cadence when one is set; a person's floor outranks a measurement, a
global outranks both, and the measurement is still taken and stored so the
page can say "measured hourly, held at 6 h by policy". Every site the
scheduler reads a row's interval goes through that one function; the source
pages show `declaredInterval`, `measuredInterval`, `policyInterval` and
`effectiveInterval` side by side; `test_sched_backoff.c` pins the order.

**And ONE source can be set by hand, from its own page** (2026-10-21,
`core/source_interval.{c,h}`; `GET`/`PUT`/`DELETE /api/sources/:id/interval`,
PUT and DELETE platform-operator like the policy). An enabled per-source
interval outranks both the measurement and the system-wide policy — specific
beats general: a floor of six hours is a statement about the fleet, "this one
source every hour" is a statement about that source made by the same operator,
later and more precisely, and it would be useless if the floor silently held
it. So `sched_row_interval()` is now: the override when one is enabled, else
`cadence_apply(sched_state_base(...))`. It cannot create a schedule (a pivot is
refused, as the measurement cannot create one either), it is stored in
`source_interval_override` and cached because the scheduler asks for every row
every tick, disabling keeps the number, clearing deletes the row, and a change
re-plans the running scheduler exactly as a policy change does. The source
page's Frequency card shows declared, measured, policy and "set for this
source" side by side and names which one decided (`schedule.cadenceBasis`);
`/api/status` rows carry `overrideInterval`. **It is edited from the page's
Settings section as the key `interval_sec`** (2026-10-22): `core/source_settings.c`
lists it beside the body ceiling, but as a FACADE — reads, writes and the JSON
for that key delegate to `source_interval.{c,h}`, so there is one row, one
cache and one generation, not a copy in `source_settings`. Its "default" in
the JSON is per source (what runs without an override), the key is absent for
an entity pivot, and the same `PUT /api/sources/:id/settings` the body ceiling
uses writes it (audited `source.setting.put`). `tests/unit/test_source_interval.c`
pins the refusals, the precedence under a global policy, disable, persistence
and clear.

What a choice costs is in the response, measured over the registry, not
guessed: `estimate.runs_per_day_declared` against
`estimate.runs_per_day_effective`. Measured 2026-10-20 on 19,954 scheduled
rows: 240,318 runs/day as declared; a global 1 h would be 479,000, 6 h 80,000,
daily 20,000. The 25 rows under 5 min (webcams, grid telemetry, Tor exits)
alone make 42,000 of today's runs, which is what a global cadence trades away.
Every source's pages show `declaredInterval`, `policyInterval` and `policy`
(`/api/sources/:id/detail` → `sched`, `/api/status` rows → `policyInterval`).
`tests/unit/test_cadence.c` pins the rules, the persistence and the refusals.


## Where things live

```
native/source.h                    the ONE data-acquisition ABI (source_def + intel_sink)
native/lib/hpengine.{c,h}          declarative deep-record collector engine
native/collectors/sources/*.c      hand-written collectors, one file per family
native/collectors/feed/generated/  generated scheduled feed collectors (vsrc*)
native/collectors/pivot/table/*.c  hpengine tables (hp*, hp2*, hp3*) — the strict set
native/collectors/pod/*.c          enrichment/maintenance pods
native/core/                       db, http, intel sink, dispatcher, pipeline, HTTP API
native/tools/                      lint_sources.py, gen_hp_batch.py, probe_hp_batch.py
docs/                              plans, pipeline notes, and the two house rules above
```

The Makefile globs `collectors/` RECURSIVELY, so a collector registers from any
depth. Do not flatten it back.

## 3. A registered source must actually be reachable

`lib/hpengine.c` (hp_run) reaches a row in exactly two ways:

* it references an entity token (`{q}`, `{qd}`, `{qh}`, …) in its URL or POST
  body, so it is dispatchable as an entity pivot; or
* it declares `interval > 0`, so the scheduler picks it up.

A row with **neither** — a static URL and no interval — is registered, appears
in `/api/status`, and never executes. It emits nothing, forever, which is the
same silent-nothing as an `EMPTY_RESULTSET` source and just as invisible. This
is easy to introduce by accident because `hp_source.interval` defaults to 0 and
0 means "on-demand pivot", which is right for a `{q}` row and wrong for a bulk
file. 763 rows across batches 18 and 19 were in that state before it was
checked for.

```sh
python3 native/tools/audit_batch_reachable.py docs/candidate-sources-batch*.txt
```

**The DISPATCHER does not test for an entity token, and a wrong `collector`
label silently retired 16 live pivots.** `is_entity_pivot()`
(`core/osint_dispatch.c`) asks `collector == "osint" && update_interval_sec == 0`
— a proxy for "takes an entity", and it cannot do better, because
`source_def.url` is the human-facing portal, not the URL template, so nothing
at that layer can see `{q}`. The proxy holds only because every hand-written
osint service is labelled that way. A row authored as a pivot under a FAMILY
label satisfies neither reach path: not the scheduler (interval 0) and not the
dispatcher (collector ≠ "osint"). Found 2026-10-12: the nine openFDA identifier
pivots (`us_health`), CVE Program and MITRE CWE lookup (`cyber`), two elevation
lookups and NASA POWER (`geospatial`/`environment`), Diavgeia by issuing body
and MLIT Kawabou detail. `core/service_vec.c` asks the same question, so they
were out of the service-vector index too.

The openFDA nine are the lesson: `hp18_us_openfda_ids.c`'s header documents
converting them from scheduled feeds that re-fetched one 2009 device clearance
twice a day into lookups answering for any identifier. **That repair landed and
did nothing**, because dropping the interval to 0 while keeping the feed-era
label removed them from the only path left that could run them.

Two things now hold it:

* `registry.c` prints `[registry] NEVER-RUNS id '…'` for any source with
  interval 0 under a collector that is neither `"osint"` nor an internal `_`
  lane, beside the DUPLICATE and OVERFLOW reports it already makes for the same
  reason, and **`make registry-floor` fails on a non-zero count**. The `_`
  exemption is the convention `source.h` documents; `PORT_SCANNER` is
  deliberately `_probe` because it opens raw sockets, and keeping it out of the
  LLM's schema enum is a security decision, not an oversight.
* `osint_lookup()` resolves a UNIQUE case-fold match after an exact miss.
  `osint_canon()` uppercases and `registry_get()` is a strcmp hash, so a
  lower-case id — 9,941 of 18,884 — could never be found by name at all, and
  `osint_resolve_near()` could not rescue it either: a pure case difference
  costs one edit per letter, 22 for `cve-program-record-api`, against a ceiling
  of 2. An AMBIGUOUS fold is refused rather than guessed, because `RELIEFWEB`
  (an osint pivot) and `reliefweb` (a scheduled feed) are two different sources.

## Batch tooling

New sources are AUTHORED as a pipe-delimited manifest and scaffolded, not
hand-written: `docs/candidate-sources-batch<N>.<beat>.txt` goes through the
probe/exclusion/reachability gates below and then through `gen_hp_batch.py`,
which is how a beat of 100+ rows gets written at all.

**After a beat ships, the C is the maintained copy and the manifest is the
record of how it was probed.** That is a correction of what this file used to
say ("the manifest is the source of truth; edit the manifest"), and it is a
correction to match reality rather than a change of policy: measured
2026-09-11 across every batch manifest, **48 of 79 beats already differ from
what their manifest generates — 62,461 lines** — and the direction is always
the same. The C holds `id_keys = "公司代號+姓名+職稱"` with the live counts
that proved it (27,528 rows, 26,789 distinct triples); the manifest still says
`公司代號`, the key that lost 97% of them. The manifest format has nowhere to
put that evidence, and a repair without its evidence is unreviewable.

So: **regenerating a shipped beat reverts verified repairs.** Both generators
now refuse to overwrite an existing file and report how many lines would have
changed; `--force` is for the case where the manifest really is the newer copy
(a whole beat re-authored), and a `diff` is the answer every other time. Mirror
an opts change back into the manifest when it is cheap — it keeps the probe
gates honest for a future re-probe — but never at the cost of the comment that
says why.

| tool | what it enforces |
| --- | --- |
| `tools/manifest.py` | THE manifest parser — line split, field count, and `opts` resolution — imported by every tool below. It is one file because it used to be seven, and they disagreed: see rule 4c |
| `tools/probe_hp_batch.py` | proof of life: 2xx, parses in its declared mode, ≥1 real record. Honours each row's own headers, handles JSON/CSV/XML/HTML, rejects empty result sets, HTTP-200 refusals, one-element error arrays and bot-wall challenge pages. A **declared `array_path` is resolved and judged** — it used to hunt for the densest array and once counted a response's own 252-key *schema block* as records, passing a row whose result set was empty. **`--check-filter`** additionally asks each pivot row about an IMPOSSIBLE entity and fails it `FILTER_IGNORED` when the answer is the same size — see rule 4d. Both answers are judged by the same counter (XML/CSV/HTML/xlsx/`array_path` aware); a comparison that could not be made is `FILTER_UNCHECKED`, never PASS. It waits each row's own `timeout_ms`, renders `{date:}` tokens and matches keys case-insensitively, as the engine does. **Paced per host** (2026-10-20, `--host-gap-ms`, default 1000): one request per host per gap across all workers, a 429 or `Retry-After` widening that host's gap for the rest of the run — the probe used to put every worker on one shared host at once and then report ITS OWN burst (502s, 403s) as the row's verdict. Rows on distinct hosts are not slowed |
| `tools/tune_intervals.py` | the authoring-time interval rule (newest item ≤3 d → hourly, ≤30 d → 6 h, ≤1 y → daily, else weekly), applied from the `newest <date>` each feed row's description records, and `adaptive_interval=1` so the scheduler keeps re-measuring it |
| `tools/feed_batch_tsv.py` | assembles `docs/verified-sources-batch<N>.tsv` (the probe's AND the engine's reading per shipped row) and `docs/rejected-sources-batch<N>.tsv` (every candidate that did not ship, by stage) from a feed batch's per-beat artefacts |
| `tools/feed_collision_check.py` | measures a feed row's COLLISION verdict against the feed itself — distinct keys, repeated keys, byte-identical surplus — so a collision is read as the upstream's own duplicate or as a wrong key, never guessed (rule 4b) |
| `tools/feed_reclassify.py` | re-fetches rows whose URL changed after validation and rewrites kind/opts/description from what the server serves NOW (the `&amp;` lesson) |
| `tools/manifest_repair_rows.py` | re-joins manifest rows a multi-line field split, and names the ones still malformed |
| `tools/discover_feeds.py`, `tools/build_feed_manifest.py`, `tools/wd_harvest.py` | the discovery half of batches 43–48: a register of official domains (or a Wikidata class per country) → each root fetched once → declared/linked feeds validated live and written as manifest rows |
| `tools/batch_exclusions.py` | no duplicate id or endpoint against the existing tree or within the batch (normalising `{q}` and `%s` to one form; `.portal` is documentation and is excluded). Sees **runtime-composed** endpoints too — it resolves string macros, joins adjacent literals, follows `#include "*.inc"`, and matches a `%s` URL family on its layer/dataset NAME. Pass **`--bin ./bin/japanosint`**: without it the id set is a regex approximation (4,754 of 13,193) and it says so |
| `tools/audit_batch_pagination.py` | a paged endpoint declares `page_param` or `next_path` — read from the parsed opts, not as a substring of the whole field |
| `tools/audit_batch_reachable.py` | rule 3 above. A row whose opts are ambiguous is reported UNVERIFIABLE, never "never runs" |
| `tools/audit_url_limits.py` | the seventh discard shape — a record limit asked of the UPSTREAM in a URL, with no paging to follow it. Read, not gated: 103 narrow candidates, unmeasured, and a limit on a ranked pivot is legitimate |
| `tools/audit_page_ceiling.py` | which scheduled rows stop at the PAGE CEILING, and how many pages their own upstream says they need. Probes SHALLOW (`--probe-pages`, default 3) and then does arithmetic, because the engine's truncation notice already carries the only number that settles it: a `records_available` whose basis is "upstream declared this total". `CEILING_DECLARED` is measured (pages_needed = total / page_size, the page size derived from the probe when the row declares none); `CEILING_UNKNOWN` means the upstream publishes no total and only a full walk can size it, which is a decision to take deliberately, not a finding. Reads `collector-fetch-failure` too, so a 429 is `FETCH_FAILED` and not "this source returns nothing" |
| `tools/audit_page_param.py` | rows whose URL already binds their own `page_param`. The engine used to APPEND (`…&pagina=1&pagina=2`) and a server binding the first occurrence then served page 1 for the whole walk — N pages emitted, one stored, `rc=0`. Fixed in `hp_url_set_param` and pinned by `hptest` "9f-bis"; the lint stays because 103 of 1,431 paged rows are that shape and their paging depends on the replacement being right |
| `tools/audit_batch_emit.py` | rule 4 below: runs each MANIFEST row through the real binary and reads back `emitted N of M`. `--timeout S` moves the kill line; a run that hits it is **`SLOW`**, carrying its partial counts — unmeasured, not failed. Each run gets a scratch DB from a warm template (it used to write the developer's live DB), and it exits non-zero on UNREGISTERED, UNPARSEABLE, NO_RUN_LINE or when nothing was measured |
| `tools/audit_registry_emit.py` | rule 4 **and** 4b for the whole REGISTRY, manifest or not — `--list-sources` is the source list, so nothing registered can hide. Measures emitted *and* stored, per run, against a fresh copy of a warm template DB. `--scheduled`/`--match`/`--only`/`--ids-file`, `--jobs`, `--timeout`, TSV out, `--resume` |
| `tools/diagnose_emit_keys.py` | why a row emitted nothing, and which `title_keys`/`id_keys` fix it |
| `tools/cleanup_stale_uids.py` | the rows identity fixes left in an OLD database: a plain-key row whose qualified siblings now hold the same record (the keyqual collectors and FRA), and wayback_jp's captures from before digest+timestamp+URL. Reports by default. `--delete` removes, in one transaction, only rows whose replacement is proven to exist and that nothing references. `--self-test` runs it on a scratch DB (also from `make unit`) |
| `tools/wp_routes_sweep.py` | the WordPress method as a tool: `sweep` asks each host for its REST root (the homepage's `Link rel="https://api.w.org/"`, else `/wp-json`), its post types and one page of each collection, writing routes.tsv/probe.tsv; `manifest` turns probe.tsv into host-type beats and a rejected TSV. Hosts are normalised (`www.` dropped, one row per REST root) because `funq.jp` and `www.funq.jp` reached the same site and only `batch_exclusions.py` noticed |
| `./bin/japanosint --host-agents` | the per-host User-Agent table (`core/host_agents.def`) as the engine resolved it: host, agent kind, the agent string, and the extra headers an `HA_BROWSER` host gets. The probe reads the same `.def`, so what it prints is what both send |
| `./bin/japanosint --discover-feeds <sites> [--jobs N]` | RSS/Atom autodiscovery in C (`core/feeddisc.c`, `test_feeddisc`): reads each homepage's own `<link rel="alternate">` declarations (relative hrefs resolve against the URL asked for, so a homepage redirecting to a squatter's domain yields nothing), skips WordPress comment feeds, decodes `&amp;`, then fetches every feed through the engine's HTTP client — same agent, per-host overrides and pacing — and prints a TSV verdict per feed: `PASS`, `TAKEOVER_SUSPECT` (gambling/pharma phrases or an unexpected script in the item titles — a question for a human, not a verdict), `REGISTERED` (any source's portal or hp row URL), or the failure. No database |
| `tools/gen_hp_batch.py` | manifest → C, one table per beat, `--prefix`/`--batch` so batches never collide — but the batch NUMBER is yours to keep unique: main shipped a batch 32 while a branch was open with its own, both created `docs/verified-sources-batch32.tsv`, and renumbering the branch to 33 after the fact meant touching the manifest, both TSVs, the table, its array name and two batch citations that had landed in `lib/hpengine.h` and `core/hostgate.c`. Rejects duplicate opts, non-integer int opts, and an opt whose value **swallowed the next one** through a stray `\;` |

## 4. Fetching is not emitting — prove the second one

`probe_hp_batch.py` proves an endpoint answers and that its body parses. It says
nothing about whether the **engine** turns those records into `intel_items`, and
the gap between the two is not small. Batch 18 was fully probe-verified, and
when its 223 rows were first run through the actual binary, **59 of them fetched
real records and stored none** — `DATAPLANE_TELNET` fetched 36,166 and emitted
0, exit code 0, run reported successful. That is the same invisible nothing as
an `EMPTY_RESULTSET` source.

```sh
cd native && make batch-gates MANIFEST='../docs/candidate-sources-batch<N>.*.txt'   # exclusions, pagination, reachability, probe → .run/batch-gates/pass-ids.txt
python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch<N>.*.txt --bin ./bin/japanosint --jobs 6
```

Run it before believing a batch. `DROPS_EVERYTHING` is the verdict that matters;
`diagnose_emit_keys.py` then separates the three causes, which want different
fixes (an engine bug, a per-row `title_keys`, or a row that is not a record
source at all).

### 4b. Emitting is not storing either

`records=N` counts `emit()` CALLS. The sink upserts on `remote_key`, so a source
whose records key onto each other reports a healthy N and stores one row — and
**every check in this file is blind to it**, because emit really was called.

Measured over a 1,197-source sweep: 46 hpengine rows losing 114,795 records per
pass. `ECDC_RESPIRATORY` emitted 12,648 and stored **31**.

**The same defect existed independently in all three record paths** —
`lib/jsonlist.c` (`us-openfda-device-pma-detail`: 109 emitted, 1 stored),
`lib/hpengine.c`, and `lib/geojson.c` (474 rows rescued across 45 geo sources,
454 of them from one). Fixing one left the others losing data, which is the
strongest argument in this repo for looking for the *other copies* of any bug
you fix. All three now carry a collision guard that flags records colliding
within one page/array and disambiguates them by CONTENT hash — so byte-identical
records still collapse (real dedupe) while records that merely share a key are
all kept. Nothing is invented, and nothing that differs is merged.

The trap that makes this easy to introduce: `id_keys` is a MANIFEST DECLARATION,
not the upstream's identity. Declaring a dimension (`id_keys=country_code` on a
weekly time series) as the record id silently discards the series. When you add
a row, check that its `id_keys` is unique per record, not per group:

```sh
./bin/japanosint --run <ID>          # the run line now states BOTH numbers
sqlite3 $JO_DB "select count(*) from intel_items where source_id='<ID>'"
```

**In `id_keys`, `+` COMPOSES and `,` CHOOSES.** `year+quarter+item` joins all
three into one key; `year,quarter,item` keys on `year` and never reads the
rest. They nest: `a+b,c` = "a+b, or else c". Use `+` whenever identity is a
tuple of dimensions, which is most statistical and regulatory tables.

Getting this backwards fails SILENTLY, and it is the most repeated mistake in
this file's history: 26 rows written in the 2026-09 fix pass used commas
meaning composites. They looked right because the in-page collision guard
content-hashes records that collide inside ONE page — the same key recurring
on a LATER page still collapsed at the sink. `FINRA_OTC_BLOCKS` emitted 2,000
and stored 476 (its distinct first-token values) until the commas became
pluses, then stored 2,000. A single-page `--run` cannot see this; only the
registry sweep can.

The generated `VJSON` fleet has the same question and a smaller answer:
`VJSON` keys on a fixed precedence list (`id`, `uid`, `guid`, `uuid`, `_id`,
`identifier`, `code`, `key`), and **`VJSON_KEYED(..., IDFIELD)`** overrides it
with ONE named top-level field. There is no composite form: when a generated
row needs a two-field key, hand-write a `run()` — the worked example is
`geo_tidesandcurrents_currents` at the top of
`collectors/feed/generated/vsrc_environment_3.c` (`pw_walk` +
`jsonlist_emit_ex`, composing the id from two fields the record already
carries). Delete the `VJSON(...)` block you replace; leaving both defines the
source twice and breaks the build.

If the second number is smaller than the first, the row's identity is wrong.

**The run line says it without being asked.** `core/intel.c` counts the DISTINCT
uids a run upserts and `core/scheduler.c` prints it, so the two numbers arrive
together and the defect is legible without a second command:

```
[sched] ANTARES_LOCI run rc=0 records=1001 7388ms stored=1001
[sched] WHO_XMART_WHSA_FACT run rc=0 records=10001 7935ms stored=2 \
        UID-COLLISION: 9999 of 10001 emitted records collapsed onto a uid already written this run
```

`stored` is DISTINCT-UID, not rows-inserted, and the difference matters: a
scheduled source re-fetching an unchanged feed inserts nothing and updates
everything, so rows-inserted would report total loss on every ordinary re-run.
A metric that cries wolf on every re-run is one nobody reads when a real
discard happens. Distinct-uid is stable across re-runs and moves only when a
run's own records collapse onto each other. It also lands in
`fetch_log.stored` (NULL = not measured, negative = a floor), so the history is
comparable and not just whatever run a human happened to watch.

The `stored=` field is appended AFTER the duration deliberately — eight parsers
in `tests/audit/` and `tools/` match `records=(-?\d+) (\d+)ms` as one unit.
Two later fields follow it the same way. `notices=N` counts the
collector-*-notice records, and UID-COLLISION is judged against stored MINUS
notice uids (a notice used to hide one collision). `failed=N EMIT-FAILED: …`
counts emits the sink refused (SQLITE_BUSY, a failed COMMIT): before it, a
lock held by the WAL checkpoint pod lost records with nothing on the run line.
`stored=` itself still equals the DB's distinct-uid count.

Sweep the whole registry for both failures, not just a batch:

```sh
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint \
        --scheduled --jobs 6 --timeout 220 --out sweep.tsv
```

It runs each source against a fresh copy of a warm template DB and reads the
stored count back out of that database as well as off the run line — two
independent readings, because a checker that believes one self-report is how
260 silent sources got through in the first place. Verdicts: `OK`,
`EMITS_NOTHING`, `COLLISION`, `SLOW` (hit `--timeout`; unmeasured, not failed),
`NEEDS_ENTITY`, `SINK_MISMATCH`.

### 4d. Answering is not answering THE QUESTION

Every gate above counts records. None of them asks whether the records are about
the thing you asked for. Three APIs in batch 21 accept a filter, **silently
ignore it, and return the whole unfiltered collection with HTTP 200**:

* EPA Envirofacts, given a column that does not exist
  (`tri_reporting_form/facility_name`) → 10,000 unrelated records
* the German BMJ portal, on `court` / `documentNumber` / `dateFrom`
* Health Canada MDALL, on an unvalidated query → the entire 21 MB table

probe PASS. emit OK. `stored == emitted`. Every gate green — and the row is an
ENTITY PIVOT, so an analyst asking "what do we have on X" gets thousands of
records about everything else, attributed to X. **That is worse than a source
that returns nothing: it is a confident wrong answer**, and no amount of record
counting can see it.

The check is one extra request: ask the same endpoint about an entity that
cannot exist. A working filter returns nothing, or something much smaller. A
filter being ignored returns the same collection it just returned for the real
entity.

```sh
python3 native/tools/probe_hp_batch.py docs/candidate-sources-batch<N>.*.txt --check-filter
```

Opt-in because it doubles the request count for pivot rows. Run it at least once
per batch, and treat `FILTER_IGNORED` as fatal — the row must be dropped or
re-pointed at a parameter the upstream actually honours.

### 4c. Two tools reading one manifest must read it the same way

`OSM_API_CHANGESETS_BLACKSEA` carried `interval=86400\;pagination_ok=…;interval=86400`.
`gen_hp_batch.py` resolved the duplicated key last-wins and generated correct C;
`audit_batch_reachable.py` resolved it first-wins, and reported a row that runs
daily as one that can never run (house rule 3). Seven tools read these
manifests and each brought its own parser.

There is now exactly one: **`native/tools/manifest.py`**. It does not resolve a
duplicate at all — it reports it, and every reader refuses to guess
(`gen_hp_batch.py` rejects the row, the auditors call it UNVERIFIABLE). It also
hands back the lines that LOOK like rows and are not, because "skipped" and
"checked and clean" used to be the same output.

Two more of the same family turned up in batch 33, both in
`tools/probe_hp_batch.py`, and both made it report a source DEAD that the
engine measurably handles — which matters more than the reverse, because the
probe is the tool that decides whether a row ships:

* its anchor regex was `href\s*=\s*["\']([^"\'#]+)["\']`, and that class stops
  at the `#` and then demands the closing quote right there, so an href
  carrying a fragment matched NOTHING. Every anchor in a PEP 503 simple index
  ends `...tar.gz#sha256=<64 hex>`, so `pypi.org/simple/requests/` scored 0
  anchors on a page holding 244 and came back UNPARSEABLE. hpengine reads the
  whole attribute and keeps them.
* its declared-`array_path` resolver walked dict keys only, so it understood
  neither `.` (the root) nor a path that crosses an array, and four live rows
  came back `PATH_UNRESOLVED`. `_resolve_array_path()` now mirrors `hp_path`
  and `hp_path_multi`.

Unifying the parser immediately found three live defects of the same family, where a
stray `\;` escaped the separator and the following opt was swallowed into the
previous VALUE — shipped into committed C as
`.date_keys = "exchangedate;pagination_ok=start/end are a DATE range…"`, a
`.detail_key` that names no field, and a User-Agent header with 130 characters
of prose glued to it. `gen_hp_batch.py` now rejects that shape by name.

Engine subtleties worth knowing before writing a row:

* `page_start`'s unset value is 0, which is also a legitimate first page. Set
  **`page_zero_based=1`** for a 0-based API — otherwise the engine coerces the
  start to 1 and silently never fetches page 1.
* **`page_size` makes `page_param` an OFFSET.** Declare it only for offset
  parameters. On a page-NUMBER API (`page`, `pageNo`, `PAGENO`) it requests
  pages 1, 1+N, 1+2N … and drops N−1 of every N pages with the run green.
  83 shipped rows were in that state until 2026-10-05; `AP19_CN_SZSE_LISTED`
  stored ~160 of 2,904 listings. Put the page size in the URL instead.
  `make lint-sources` now fails the shape (`page-size-on-page-number`), and
  on the merge with main it caught 97 more that batch 36 had shipped the
  same day: every World Bank (`per_page`) and Socrata `views.json` (`limit`)
  row. `US_SOCRATA_CAMBRIDGEMA` stored 100 datasets and reported "emitted 100
  of 100 available"; without `page_size` it stores 446. An
  API whose `page` really is a record offset (MapRoulette) says so with a
  `page-offset-ok: <measurement>` marker in the row. And check where a
  page-number API STARTS: a 0-based one (Spring, DSpace) needs
  `page_zero_based=1`, or the walk goes page 0 → page 2.
* **A file re-published under a new name each edition** (monthly attachment
  ids) is not a static `url`: declare **`index_url`** (the landing page) and
  **`index_href_must`** (a substring of the right link). The engine follows it
  every run, and the generator and probe accept both. Batch 32's Niigata
  register 404'd three days after shipping without it.
* **Never freeze a date into a URL.** `{date}`, `{date-7d}`, `{date-1m:%Y%m}`,
  `{date:@ms}`, main's `{date:%Y%m%d:-2}` and `{ago:N}` (lib/datetok.c — one implementation for hp rows AND the VJSON
  fleet's `vsrc_url_dates`) render in UTC+**`tz_minutes`** (540 Japan, 480
  China). **`date_back=N`** steps the date back when the first page is a 4xx
  or empty (weekends, holidays, not yet published), in units of
  **`date_step`** (d/w/m/y — `y` for EPA's TRI reporting year). 43 rows had a
  window frozen at authoring and re-fetched the same past week forever. A
  dated file whose ROWS carry no date (SHFE's daily kx, SZSE margin) re-keys
  every day onto yesterday's uids: **`date_field=<name>`** stamps each record
  with the rendered date (after any step back) so `id_keys` can include it.
* **JSONP is unwrapped automatically** (`cb(…)`, `null(…)`, `(…)`, `)]}'`).
* **A page that lives in a POST body or a URL path** is `{page}` / `{offset}` /
  `{offset1}` / `{offset_end}` / `{size}`, re-expanded every page in the url
  AND the body (EPA Envirofacts: `rows/{offset}:{offset_end}`). A body with a
  literal `"page":1` reads page 1 forever: four pivots did, 40 records each.
* A failure on the page AFTER a short page (fewer than `page_size` records) is
  the upstream's end of data, not a truncation; later pages get 5 retries.
* **`page_delay_ms=N`** paces a walk (a pause before every page after the
  first) for a firewall that answers a burst with 403: the Palace Museum's
  blocked us after 31 of 9,300 pages.
* **A register published as an HTML table** is `mode=html` +
  **`table_must=<caption or header text>`** (`table_index`,
  `table_skip_rows`): every row becomes a record keyed by its header cells
  (lib/htmlparse.c `html_table_parse` + hp_run_table; colspan/rowspan,
  entities, nested tables handled; the row's first link is `link`). Before it,
  html mode read anchors only, and ~20 tabular registers were rejected.
* **A bare root array must be DECLARED: `array_path=.`** is the document
  itself. With no `array_path` the engine picks the densest array of OBJECTS,
  which is not the root as soon as a root record carries a longer nested array
  of its own. hex.pm returns 100 packages per page, each with its own
  `releases` list, so on a 60-page walk the engine mined `[33].releases` (133)
  over the root (100) on 34 of the 60 pages: 9,643 records emitted where the
  pages hold 6,000, keyed on a field the release entries do not carry, 2,881
  collapsing at the sink. It is disclosed — a `collector-shape-notice` naming
  the array mined and the runner-up — but a notice is a row in the database,
  not a build error, so a row nobody reads after its first run keeps doing it.
  Six rows in batch 33 were that shape. `hptest` pins both halves, the hijack
  and the fix.
* **An array of bare JSON STRINGS is a record set** — `title_keys=value`,
  `id_keys=value`. `hp_json_flat()` maps a string WITH CONTENT to
  `{"value": "<the string>"}`; the `empty` counter is for a NULL, a number or
  an EMPTY string (the trailing newline of a CSV), which is what the comment
  beside it is about. Four live sources — the Microsoft Container Registry
  catalogue, NuGet's flat version list, and the complete Packagist and pub.dev
  name lists — were written off in `docs/rejected-sources-batch34.tsv` on that
  comment read as a capability claim, and all four store exactly what they
  emit (3,851 / 86 / 464,521 / 91,388). **A capability claim read off a comment
  is not a measurement**, and the measurement is one `--run` away.
* **`mode=ndjson`** (`HP_NDJSON`) for one complete JSON value per line, no
  enclosing array and no commas — the shape `index.golang.org/index` and
  `index.crates.io` publish. Declared `json` the body dies at line 2 and the
  row emits nothing forever; declared `csv` every line is one unqueryable
  cell, which is the mistake `IANA_LANGUAGE_SUBTAGS` made. It works by
  rewriting the lines into ONE array and handing that to the JSON path, so
  every key list, the collision guard, detail hops, caps, paging and the
  disclosure notices apply unchanged. A line that is not valid JSON is a LOST
  RECORD, counted into `malformed`, reported in the run line and disclosed as
  a truncation notice. Note the parse is `require_null_terminated`: plain
  `cJSON_Parse` accepts `{"a":1} oops` and copying that line's text into the
  array then poisons the whole array, taking every good line down with the
  one bad one.
* **`next_path` takes a `$last` segment** — the final element of an array —
  for an upstream whose cursor is the last record's own field rather than
  anything in an envelope. The Go module index publishes no next link at all
  and pages by `since=<the Timestamp of the last record it gave you>`, and no
  numeric index can name that element because the array's length is not known
  when the row is written.
* **`{ago:<seconds>}`** expands to RFC 3339 UTC of (now - seconds), the one
  token not derived from the entity. It is for an upstream whose only ordering
  is "everything at or after this instant", with no reverse order and no
  `latest`: the Go index is append-only from 2019-04-10, so a row without the
  anchor walks forward from 2019 on every run, emits 40,000 records an hour
  and never reaches today. `since={ago:7200}` makes the same row the live
  publish tail. **Check this when a feed's URL has no entity token and its
  interval is short** — a source that emits plenty and tells you nothing new
  is as invisible as one that emits nothing.
* An **INCLUSIVE cursor** repeats one record per page boundary (the Go index
  hands back page N's last record as page N+1's first), and the sink collapses
  it as the byte-identical duplicate it is. Do not widen `id_keys` to "fix" a
  `stored` that is a handful short of `emitted` on a cursor-paged feed —
  measure the upstream first.
* **A later page that fails is disclosed, not a quiet end** (2026-10-06,
  `hp_later_page_cut`). After a SHORT page any failure is the end of data.
  Otherwise transport errors, 3xx/304, 401/403/407, 408, 429, 5xx and an
  unreadable 200 always mean the walk was cut short, and other 4xx do when the
  upstream showed more (its total, a next link, a full previous page). A cut
  walk files a truncation notice naming `failed_page`, `failed_page_status` and
  `failed_page_url`; VJSON, geojson and `pw_walk` name the same three. A
  row-declared conditional header goes on page 1 only.
* **A detail hop carries the row's headers** (token, UA, Accept), minus the
  POST body's content type. It used to carry none, so a header-authenticated
  row stored `_detail_error` on every record (PY_AIP: 500 without, 200 with).
* **A detail hop that answers HTML** is `detail_html_kv=1`: the record page's
  label/value cells merge under `detail.` — every cell with a non-empty
  `data-label` (the accessible responsive-table pattern; an EMPTY data-label
  is the visible label column and is skipped), and a `<th>` paired with the
  `<td>` after it. A repeated label is indexed (`Expiration Date.1`), never
  overwritten; a page with no such cells is `_detail_error` on the record.
  Opt-in, because navigation tables are cells too. Without it an HTML detail
  body is "not JSON; nothing merged", which is why the FAA owner search
  stored a summary row and no registration. `hptest` 41 pins it.
* **XML records keep every field**: repeated children are indexed (`name`,
  `name.1`, `author.1.name`), text beside children is `<key>.#text`, and
  whatever the depth (32) / field (`HP_MAX_PROPS`) bounds keep out is counted
  into `_fields_dropped`. **An HTML link takes its strongest label** (own text
  > image alt > borrowed text, a label that contains another is kept), the
  others the page gave it land in `other_labels.N`.
* A row that declares no paging does ONE request. **`page_walk=1`** (HP_JSON
  only) walks it the way a VJSON collector is walked instead — the server's own
  next link, else the cursor its declared page size pairs with, advanced while
  pages come back full — through `jsonlist_next_page()` in `lib/jsonlist.c`,
  the one copy of that decision. The 322 batch-16/17 rows moved onto hpengine for
  their detail hops (`collectors/gen_detail_hop_upgrade.py`) carry it so they
  did not trade their later pages for the second hop.
* **`page_walk` moves only a cursor whose page-size sibling it KNOWS**
  (`PAGERS` in `lib/jsonlist.c`: `rows`/`start`, `limit`/`offset`,
  `per_page`/`page` …). `count`/`start` — the GOV.UK Search API's pair, also
  J-STAGE's and CiNii's, with WFS 2.0's `startIndex` as the alternate — was
  not in it until 2026-10-20, so six GOV.UK rows declaring `page_walk=1` AND
  `page_resume=1` read ONE page of three records a run, and cycled nothing,
  because a one-page walk "completes". The ceiling audit had said so
  (`pages_read 1` against `pages_needed 44,885` for the employment-tribunal
  row) and it read as a row waiting for its cycle. Measured after:
  `uk-govuk-search-cma-cases` resumes at `start=60`, then `start=120`; the
  six are at `count=100` with an ascending sort now. **A walk that reads
  exactly one page of a declared size deserves one look at that table.**
* **The page ceiling is one number with one knob: `jo_page_max()`, 20, under
  `$JO_PAGE_MAX`** (`lib/pagewalk.c`). It was three, with two numbers and two
  knobs, one of which did not exist — `pagewalk.c` 20/`$JO_PAGE_MAX`,
  `jsonlist.c` 20/`$JO_JSONLIST_PAGE_MAX`, and `hpengine.c` **10 with no
  variable at all**. So `JO_PAGE_MAX=50` raised the generated fleet, silently
  did nothing for `page_walk` rows, and could not move a row that declares its
  own paging at any value. That third group is the largest: 1,919 scheduled rows
  declare `page_param`/`next_path`/`{page}` with no per-row `page_max`, and
  stopped at ten pages on a constant none of them mentions.
  `JSTAGE_CORPUS_BULK` went from 1,000 records to 2,000 on the unification, and
  its own notice had been saying `declared_max_items: 20000` the whole time.
  The legacy variable still works, so an operator who set it is honoured rather
  than ignored. Twenty-two comments in 14 collector files cite measurements made
  under "the default 10-page ceiling": those are dated records, not claims about
  today.

  **`$JO_PAGE_MAX` moves the DEFAULT; a row's own `page_max` is final.**
  `hpengine.c` reads `s->page_max > 0 ? s->page_max : jo_page_max(NULL)`, so
  the variable cannot lower OR raise a row that declares a ceiling — it only
  decides what a row WITHOUT one gets. "Overrides in both directions" above is
  about that default. Read the other way it costs a whole run: an attempt to
  shrink `JO34_FDSNEVT_SERVICE_SCEDC_CALTECH_EDU`'s declared 200 pages to 5 for
  a quick cycle test read **80 pages** before the clock stopped it
  (2026-10-21). To bound a declared row for a test, change the row.
* **The ceiling is now MEASURED across every paged scheduled row**
  (2026-10-12, `docs/measured-page-ceilings-2026-10-12.tsv`): 2,137 rows,
  3-page probe. 1,111 COMPLETE, 534 `CEILING_UNKNOWN` (the upstream publishes
  no total, so only a full walk can size them), 282 `CEILING_DECLARED`, 114
  SLOW, 89 `FETCH_FAILED`, 7 EMPTY. **149 declare a total needing more than 20
  pages**, and they split on cost, which is the whole decision:
  * **60 rows need ≤200 pages** — a per-row `page_max` completes them at a
    sane request count. Done, with the measurement in each comment: 634,915
    declared records against 148,840 the 20-page default reached, so **486,075
    recovered**. `BGS_LANDSLIDE_INDEX` 2,000 → 18,698 (its whole total);
    `CMS_NURSING_HOME_PROVIDER_INFO` → 14,690.
  * **89 rows need more than 200**, up to 55,971 pages for a 5.6-million-item
    STAC catalogue, and 1.93 BILLION declared records between them. Raising
    `page_max` there is not exhaustiveness, it is tens of thousands of requests
    per run against one host. **81 of them now read their collection over a
    CYCLE** — see `page_resume` below; the other eight are named there.
* **`page_resume = 1` reads a collection over a CYCLE**, one ceiling's worth a
  run, instead of re-reading its first pages forever. The walk starts where the
  last run stopped and stores where this one stopped; when it reaches the end
  the position is CLEARED and the next run starts over. This is what
  `collectors/sources/wayback_jp.c` has always done by hand (mhlw's 2,064-page
  index over ~207 daily runs); it is now a field, so 81 rows got it without 81
  hand-written collectors.

  The position is a URL — the one the engine would have requested next — so
  `page_param`, `next_path` and `page_walk` all resume from one stored string.
  A URL is NOT enough on its own for `page_param`: the next page is computed as
  `page_start + (page+1)*step` from the ROW's url, so a run resumed at
  `offset=4` with `page_start` still 0 asks for `offset=2` next and walks
  BACKWARDS over ground it covered, every page full and the run green. The
  engine reads the number back out of the stored URL for that reason.

  Persistence is NOT in the engine: `lib/` holds no database code (hptest links
  `lib/hpengine.c` with no sqlite3 at all), so the position crosses the ABI as
  `source_ctx`'s `resume_in` / `resume_out`, and `core/scheduler.c` keeps it in
  `source_sched_state.resume_token`. That also makes the mechanism available to
  every collector rather than to one engine.

  Measured live, three mechanisms, two runs each against one database:
  `BGS_ONSHORE_BOREHOLE_INDEX` (next_path) 300 → 600 distinct rows,
  `IA_COLLECTION_ARCHIVETEAM` (page_param) 1,000 → 2,000,
  `gleif-lei-filter-lapsed` (page_walk) 20 → 40. `hptest` 40–40e pins the
  cycle, the clearing, the backwards-walk trap, and all three refusals.

  **It is refused, with a line on stderr, for an ENTITY run** ("what do we have
  on X" must answer from the start of X's results, not page 37 of the last
  entity asked about — the scheduler also passes no position on a pivot run),
  for a `{date…}`/`{ago…}` row (a stored URL would pin one day forever; those
  rows already walk forward by date), for a `{page}`/`{offset}` token in the
  path or body (the position is not readable back out of the URL), and for a
  stored position whose HOST is not the row's own — a reading position moves us
  through a collection, never to another server.

  **Two things a cycle does not fix, and what each got instead (2026-10-20).**
  Five ELIA grid rows ran every 900–3,600 s over millions of quarter-hourly
  points, where a cycle would blind them to the current grid for days: four
  now carry a **date window** sized on their MEASURED publication lag
  (`where=datetime>=now(days=-5)` for wind, whose newest record was 41 hours
  old when read; 48 hours for asset loading, published ~14 hours behind in
  ~6,000-record days; three days for recent generation and for the system
  imbalance) with `order_by=datetime desc`, 100 a page and a `page_max` that
  holds the window. Engine readings the same day: asset loading 7,003 / 7,003
  in its 48 hours, recent generation 2,115 / 2,115 in its three days; the wind
  and imbalance windows were validated by direct reads (2,068,300 total and
  212 in-window records respectively) because opendata.elia.be refused the
  engine's two attempts from the measuring environment, as it refused nine
  requests in ten all day — a reading of that path, not of the rows. One of
  the five was not truncated at all: **`ods045` ENDED
  in May 2024** (newest record 2024-05-21, total unchanged between audits) and
  the row had re-read its first 150 points of 2019 every 15 minutes for 17
  months — it reads the successor `ods134` now, 212 records in its three-day
  window, measured. The fifth, `ods147`, was not the dataset its row said it
  was (2026-10-21): the row called it a renewable intraday forecast; its own
  metadata — the first answer the host gave in two days — titles it "System
  imbalance forecast next quarter hour (near real-time)", one record a
  MINUTE since 2023-10-06 (1,437 in the last day, 1,576,655 in all), time
  field `predictiontimeutc`, which is why `datetime` was "Unknown field".
  It is now the deliberate pair `ELIA_SI_FORECAST_NEXT_QH` (the last six
  hours newest first, every 15 minutes: 359 of 359 stored across four pages)
  and `ELIA_SI_FORECAST_NEXT_QH_HISTORY`, the whole series — and because the
  records endpoint caps `offset` at 10,000, an offset walk could never reach
  past the first 10,000 of 1.58 million, so the history row is a **keyset
  cursor**: `order_by=predictiontimeutc asc` with
  `next_path=results.$last.predictiontimeutc` spent through a `next_tmpl`
  whose `where=predictiontimeutc>date'{v}'` asks for everything strictly
  after the last record (the Go-module-index shape), 100 pages an hour under
  `page_resume=1`. Measured: 10,000 of 10,000 in 77 s, 2023-10-06T12:57 to
  2023-10-13T11:36, the resume URL stored; the second run resumed from that
  URL and stored the next 10,000 (to 2023-10-20T10:32) — 20,000 distinct
  rows over two runs against one database, no overlap, 72 s each. About a
  week per cycle against 1,440 new records a day. The
  `/exports/csv` route (the whole dataset as one ~210 MB stream, read to three
  rows) is the other complete read; the cursor was chosen because a cut walk
  resumes where it stopped and a cut export starts over. **A row's name is a
  claim about its dataset too — read the metadata endpoint once**, and
  **before windowing a row, read its newest record: a frozen series looks
  exactly like a truncated one from the count alone.** The two GOV.UK report
  searches (MAIB, RAIB) were not a window problem but the `count`/`start`
  pager above: at `count=100` each reads its whole register in 12 / 7 pages.
  And `PLOS_RECENT` — 417,719 records against a 20-page ceiling —
  deliberately does NOT cycle: its name, tags and `publication_date desc` sort
  all say recency is the point, and a cycle would cost it exactly that.
  `PLOS_CORPUS` is the second row that note asked for: the same query with
  `publication_date asc,id asc` and `page_resume`, 418,125 articles over a
  daily cycle (2,000 a run, resuming at `start=2000`, then `start=4000` —
  measured).

  **A descending sort and a cycle do not compose, and the fix is the sort.**
  New items land at the HEAD of a newest-first collection, so every page
  boundary shifts under a walk that is halfway down and a seam can re-read or
  skip records — the drift `wayback_jp.c` documents. Ascending puts new items at
  the TAIL, where a forward walk has not been yet, so a resumed run asks for the
  pages it meant to. **22 rows were re-sorted on 2026-10-18**, each verified
  against its own upstream first: 17 Internet Archive collections
  (`sort[]=publicdate+asc` — same 5,430,347 total, oldest first), four FR BODACC
  gazettes (`order_by=dateparution asc` — same 6,641,689 total, from
  2008-01-02), and `CZ_CRO_PUBLIC_OFFICIALS`, which already sorted on `id`, the
  most stable key there is, and only needed `order=ASC`. Measured after: 2,000 /
  400 / 40 distinct rows over two runs each, no overlap.

  `UK_EC_DONATIONS` kept `order=desc`: ascending leads with records whose
  `AcceptedDate` is null, so that sort is not a stable ordering, and a drifting
  head is the documented trade rather than a silent one. A row that does NOT
  cycle keeps `desc` deliberately — reading the newest pages is then the
  question being asked.

  **The 534 `CEILING_UNKNOWN` rows: 452 of them now cycle too** (2026-10-18).
  No arithmetic can size them, which is exactly why a cycle fits — the walk does
  not need to know how big the collection is, only where it stopped. Three
  measured properties make it safe to apply without probing each one:
  * a row that COMPLETES inside its ceiling stores no position, so cycling is a
    genuine no-op for it. `JPN_EGOV_DATA_ORG_ORG_0100`: 13 records on both runs,
    token `(none)`. `hptest` 40b-ii pins the same thing offline;
  * a row that does not complete reads a further slice instead of the same first
    pages — strictly more of the collection;
  * every stop is still disclosed, now naming the slice.

  Four exclusions, each a case where a cycle costs something: **cadence under
  six hours** (44 rows — the row exists to be fresh), **recency in its own
  identity** (29 — its id or record_type says `recent`/`latest`/`newest`/
  `daily`), **a declared descending sort** (8 — the fix is an ascending sort
  verified per host, which was done for the 22 above and is not extended to
  hundreds of unverified hosts), and **a `{page}` token** (1 — the engine
  refuses it). **A 24-row sample was run twice each against one database and
  every row behaved correctly**: 15 cycling mid-collection
  (`TX_TCEQ_VIOLATION_CITATIONS` 20,000 → 40,000, `FAA_ARCGIS_OBSTACLES`
  10,000 → 20,000, `FMCSA_MOTUS_AUTHORITY_HISTORY` 19,997 → 39,997), 2 finishing
  the collection and clearing (`MD_LOBBYIST_CAMPAIGN_CONTRIBUTIONS`
  20,000 → 20,387; `au-asic-smsf-auditors` 2,000 → 2,400), and 7 no-ops that
  complete inside their ceiling and store nothing (`WA_CRIME_NIBRS` 3,471 both
  runs, `UK_HYDROLOGY_STATIONS` 9,537, `CA_CALGEM_FACILITY_TANKS` 15,732).

  **One row in that sample was not a cycle at all, and it found a false
  disclosure.** `ENA_PATHOGEN_SARSCOV2_RUNS` read 1,000 of millions on both
  runs with no position stored, which reads like "completes in its ceiling" —
  and a round 1,000 against a `limit=1000` is worth one suspicious look.
  ebi.ac.uk answers **HTTP 400 to `offset=1000`**: page 2 is refused, so the
  position is cleared (bookmarking a page that is refused would retry that one
  page forever) and the cycle can never advance. The notice then said
  `next_run_resumes_at: "the start — this run completed the cycle"`, which is
  exactly the kind of tidy falsehood these notices exist to prevent. It now
  distinguishes the three outcomes, and a failed page says the position was NOT
  advanced and the row re-reads the same slice until its paging is re-pointed
  (`hptest` 40b-iii). Any other row in this shape says so on its next run; a
  sweep for them is the obvious next measurement.

  533 rows cycle in total. One shape to keep in mind: `interval = 2592000` is
  monthly, so a 500-page collection on that cadence takes about two years to
  come round — better than never, and the notice says where it is.
* **A deep-paging cap is the upstream's, and it is disclosed by name.** GLEIF
  refuses `page[number]=51` with HTTP 400 — 50 pages × 200 = 10,000 records —
  so `GLEIF_LEI_HK` reaches 10,000 of 13,419 and the rest needs a narrower
  filter per request. The notice says exactly that, including
  `failed_page_url`. Leaving `page_max` ABOVE the cap is deliberate there: the
  notice then names GLEIF's refusal rather than our own ceiling, for the price
  of one 400 a run.
* **`page_size` on a bracketed page-number parameter hid from the lint that
  exists for it.** `_PAGENUM_NAME` in `tools/lint_sources.py` anchored the name
  list with `$`, so JSON:API's `page[number]` — written percent-encoded in a
  URL as `page%5Bnumber%5D` — matched nothing, and **63 rows shipped three days
  after that check was added** with page_size on a page NUMBER. Every one asked
  for page 201 (GLEIF) or 501 (US Treasury Fiscal Data) as its SECOND request,
  got HTTP 400, and stored one page: `GLEIF_LEI_HK` 200 of 13,419,
  `US_TREAS_MTS_TABLE_2` 500 of 2,070. Removing `page_size` (the size is
  already in the URL as `page[size]`) took those two to 10,000 and 2,070. The
  pattern now carries the bracketed and encoded spellings, and reintroducing
  one `page_size` fails the gate by name.
* **`more_pages_pending` is not the test for "data remains".** It reports
  whether the walk still held a next-page URL when it stopped, and it is FALSE
  at the ceiling. The authoritative signal is the upstream's own total —
  `records_available` with `records_available_basis: "upstream declared this
  total"` — exceeding `records_used`. `JPN_EGOV_DATA_ORG_ORG_0300` files 300 of
  a declared 325 with `more_pages_pending: false`, so a checker keyed on the
  flag calls it complete while 25 declared records are dropped every run.
  `tools/audit_page_ceiling.py` keys on the totals for that reason.
* `next_path` accepts a `key=value` segment: write **`links.rel=next.href`**,
  not `links.1.href`. Indexing a hypermedia link array positionally breaks when
  a server reorders it, and the failure mode is the engine refetching page 1
  until the page ceiling — every later page lost, with the run still looking
  successful.
* A "CSV" feed is often not comma-separated. DataPlane.org publishes
  `ASN | AS name | ip | lastseen | category`; declare **`csv_delim=pipe`** (or
  `tab`, or `semi` — named, because the manifest is itself pipe-delimited) or
  the whole line lands as one unqueryable cell. **`csv_comment=#`** strips a
  banner before the parse, which matters because URLhaus keeps its column names
  in a `#` line and header parsing would otherwise name every column after a
  comment.
* **A generated row's `id_keys` is a guess until it is measured.** Batch 36's
  generator keyed every openFDA row on the record's FIRST field — an identity
  on four endpoints and a dimension on the rest (`status`, `third_party_flag`,
  `update_type`, whole arrays). The in-page collision guard content-hashes
  such rows, so `stored` matched `emitted` and nothing looked wrong; the cost
  is that every uid is a content hash, so a recall whose status changes is a
  NEW row instead of an update. A key naming a bare ARRAY
  (`establishment_type`) matches no flattened field at all — name the element
  (`establishment_type.0`). The same batch's World Bank `country/all` rows
  read 200 of 17,490 country-years, one request per run: the seventh discard
  shape, a limit asked of the upstream (`tools/audit_url_limits.py`).
* Titles: the engine will key a record on its first non-empty scalar rather than
  drop it, so a row without `title_keys` still emits — but it emits titled
  `<record_type> <whatever came first>`. Declare `title_keys`/`id_keys` for
  anything whose fields are not named `name`/`title`/`id`.
* **`{date:FORMAT}` / `{date:FORMAT:±N}`** in any templated URL is today's UTC
  date shifted N days, through strftime (`{date:%d-%m-%Y:-2}`). For an
  upstream that answers one day at a time; without it a row pinned its authoring
  day and fetched that one day forever while reporting success.
* **`array_path = "a+b+c"`** (HP_JSON) emits several sibling arrays of one
  response — SIDOF's morning, evening and extraordinary editions. An absent one
  is not an error; none at all is the declared-path-missing case.
* A walk whose URL carries no page number (or `page=0`) asks for page 1 next and
  compares it with the first page: identical means 1-based, continue at 2;
  different means 0-based and page 1 was new data. It used to jump to 2, so
  every 0-based API lost its second page (Diavgeia, 99 of 100 records).
  Paged JSON rows stop on repeated RECORDS, not a repeated body, so an ignored
  cursor plus a changing envelope (a timestamp) no longer re-emits 20 pages.
* CSV: the unterminated-quote repair fires only on a real malformation — a long
  cell whose closing quote is followed by the delimiter or a line end is a cell
  (`csv_parse_wellformed()` skips the repair entirely, for xlsx-written text).
  Blanks before an opening quote are padding in every mode: abuse.ch writes
  `"a", "b"`, and comma mode used to keep the quotes and split tags on commas.
* Use `jo_strcasestr`/`jo_memmem` (`lib/jocore.h`), never `strcasestr`/`memmem`:
  glibc declares them only under `_GNU_SOURCE`, which this tree never defines,
  so on Linux CI they were implicitly declared with a truncated return.

Every source self-registers with `REGISTER_SOURCE` (or `HP_REGISTER_TABLE`) and
is both schedulable (`update_interval_sec > 0`) and dispatchable as an
entity pivot. There is no separate "service" type. Dispatch resolves a
misspelled id to the nearest ENTITY PIVOT only, and runs it under the resolved
id; it used to run under the misspelling and log the failure against the
healthy source.

## Embedding pipeline and tenancy (audit of 2026-10-02)

`embedding-backfill` (core/embed_pod.c) embeds new and changed rows into
`intel_vec_v2`; `/api/intel/semantic` queries it on a worker thread, never the
event loop. What it guarantees now, each pinned by `tests/unit/test_embed_pod.c`:

* Re-fetched rows whose text did not change are marked seen, and the delta walk
  resumes from a saved keyset — it used to rescan the same first 2,000 uids
  every tick and stop embedding new rows for good. `JO_EMBED_MAX_PER_RUN`
  counts rows SENT; `JO_EMBED_MAX_WALK` bounds rows looked at.
* A batch the server refuses is retried row by row; a row that fails alone goes
  to `intel_vec_failed` and is skipped until its text changes. Coverage reports
  `failed_count` and a sample. Text is cleaned to valid UTF-8 before sending.
* The model is identified from `/v1/models` and stored only with the first
  successful write; a failed detection skips the tick. A same-dimension model
  swap is refused by the pod AND by queries (503), and `service_vec` uses the
  same identity rule.
* A row's date is `published_at` only when it is ISO, else `fetched_at` — epoch
  and dd/mm/yyyy dates used to make rows ineligible and uncounted.
* Distance is L2. llama-server returns unit vectors, so the ranking equals
  cosine (d² = 2 − 2cos).

**One vector library, several indexes (2026-10-12).** `lib/vecindex.{c,h}` is
the only implementation of model identity, dimension probe, incremental upsert
by (key, text hash), KNN with metadata filters, coverage and RRF fusion; every
index below is built on it and listed under `vectorIndexes` in `/api/status`.
**Never `ALTER … RENAME` a vec0 table** (its shadow tables do not follow), and
never put two models' vectors in one index — image vectors have their own.

| index | holds | read by |
| --- | --- | --- |
| `intel_vec_v2` | items; `tenant_id` partition, `source_id`/`ts`/`record_type` filter INSIDE the search (the old `intel_vec` is migrated by copy, not re-embedded) | `/api/intel/semantic` (+filters), `sort=hybrid`, `/items/:uid/similar`, `/cases/:id/related`, Nearby `q`, `/timeline?q=`, the corpus lookup, prompt record ranking, stories/novelty, semantic alerts |
| `service_vec` | every registered source's card (kind pivot/scheduled/pod, aliases from `core/service_aliases.c`), incremental | phase-1 and per-round routing (pivots only), `/api/sources/semantic`, `service_vec_route_sources`, canonical category votes, layer review, `batch_exclusions.py --semantic` |
| `entity_vec` | entity name + romaji, partitioned by type | merge candidates for the LLM judge (nothing auto-merges), `/api/entities/search?mode=hybrid` |
| `fewshot_vec`, `anomaly_vec` | the analysis examples (`grammars/fewshot_analysis.jsonl`), past anomalies | per-query few-shot selection, triage memory |

`embed_hook_register()` (core/embed_pod.h) runs after each committed batch
with the vectors in hand — stories (`story_id`, separate from simhash's
`cluster_id`), `novelty`, and natural-language alert rules are evaluated
there, never by an embed call on the ingest path. **Every similarity threshold
lives in ONE registry, `core/thresholds.{c,h}`** (2026-10-21), and is read
through `threshold_get("<key>")` — never a module-local `getenv()` or a bare
constant at the comparison. Resolution per key: a runtime override in
`settings_thresholds` > its env var (the same names as before; an unparsable or
out-of-range value is ignored, as it always was) > the module's compiled
constant, which stays in its header as the documented default. The DB layer is
cached in memory (loaded by `threshold_migrate()` from `db_open`, refreshed on
every write), so a hot path never touches SQLite. Eleven keys:
`entity_vec_cos_floor` 0.80, `story_tau` 0.85, `alert_sem_threshold` 0.82 (a
rule's own `threshold` still wins), `relevance_sim_min` 0.40,
`suggest_min_cos` 0.50, `resolve_emb_min_cos` 0.60, `resolve_emb_margin` 0.10,
`camera_dup_cosine_min` 0.97, `repair_sem_cos_floor` 0.70 and
`corpus_vec_max_distance` √2 (an L2 distance — lower is stricter) and
`route_min_cos` 0.55 (which scheduled sources a question with no entity is
routed to — every one at or above it, no count cap). Operators
move them live, without a restart, through `GET /api/admin/thresholds` (value,
`source` db|env|default, default, min, max, env) and
`PUT /api/admin/thresholds/:key` `{value}` | `{reset:true}` — platform-operator
gate, a value outside [min,max] is a 400 not a clamp, every change an
audit-chain row (`threshold.set` / `threshold.reset`, old and new value) — from
Admin › Thresholds in the web client and Admin › Similarity thresholds on iOS.
**A new similarity cut-off gets a row in that table**, not its own getenv.
And **none has been measured on the production model**:
they were chosen from typical multilingual-e5 scores and tested against a stub.
Without `JO_EMBED_URL` every feature falls back to its pre-embedding
behaviour byte for byte; that is pinned per feature in `tests/unit/`.

Tenancy: OSINT records are one shared corpus (tenant `legacy`, uid
`source|key`). A search run is not: its summary row is written under the tenant
that started it, `/api/intel/items`, `/search` and `/items/:uid` read "this
tenant + the shared corpus", and `/api/search/results` plus the SSE stream
answer the owner tenant (or the run's `stream_key`) only.

Workspace = tenant, and **everything a workspace authors is readable by all of
its members** (decided 2026-10-05): OSINT runs and their syntheses
(`GET /api/search/runs`), saved searches, search history, case notes. Reads
carry `user_id` + `mine` and `meta.scope`; `?mine=1` narrows a list (and its
`total`) to the caller. Edits, deletes and clearing history stay with the
author. Nothing crosses workspaces — `test_search_runs_workspace.c` and
`test_saved_search_workspace.c` pin both halves. A case VIEWER reads only;
content writes are analyst+ or the case's lead or a contributor
(`test_case_roles.c`). The case list and the case detail both carry the
caller's `my_case_role`, so clients gate on that same rule.

Search history holds every COMMITTED search (2026-10-09): the clients
`POST /api/search-history` on submit / opening a result / a committed URL's
results — never per keystroke, since `/api/entities/search` serves a
search-as-you-type box — and `searchapi.c` records an OSINT run at start.
The same search by the same person within `SS_HISTORY_MERGE_SEC` refreshes one
row instead of copying it. Do not record from the search routes themselves. List routes answer
`page:{limit,offset,count,total,has_more}`, `total` measured under the same
filters as the rows.

CORS is an explicit allow-list or nothing: `JO_CORS_ORIGINS` (comma-separated
exact `scheme://host[:port]`) is echoed with `Vary: Origin` and preflights are
answered before auth; unset, no CORS header is sent at all. It used to be a
hard-coded `*` on every reply — a cross-origin deployment of the web client
(`VITE_API_HOST`) must now set the variable. Local dev goes through the Vite
proxy and needs nothing.

## Infrastructure audit (W1a–W2a, merged 2026-10-12)

The `claude/audit-fixes-and-embeddings` work, integrated into main with its two
failing tests repaired. What it guarantees, and the knobs it adds:

* **Auth off the loop.** The JWKS is refreshed on its own thread, never fetched
  inside a request; break-glass tokens verify and every use is audited.
* **The audit log is a hash chain** (`audit_events`); verification reports
  unchained rows. Collector runs started through the API are gated by role and
  a per-user budget.
* **Outbound politeness / SSRF** (`core/hostgate.c`): per-host capacity and
  `Retry-After`; the camera proxy refuses LAN, metadata and link-local
  targets, always — there is no LAN opt-in (`JO_CAMERA_ALLOW_LAN` was removed
  2026-10-20; tests use `hostgate_camera_test_allow_lan()`).
* **LLM queue**: bounded lanes (`JO_LLM_QUEUE_MAX`, default 64 waiting; a job
  waits at most `JO_LLM_QUEUE_WAIT_FACTOR` × its timeout, default 4), no retry
  after a timeout, queue time reported apart from model time;
  `JO_EMBED_QUERY_URL` (default `JO_EMBED_URL`) serves query-time embeds on
  their own slot, and both servers' `/v1/models` must agree. Triage and
  translation inputs are FENCED as untrusted text; a run's synthesis row is
  neither translated nor mined for entities. An LLM outage is NOT charged to
  the items it was asked about (only an answer the model gave is).
* **Scheduler**: a boot ramp (`JO_SCHED_RAMP_SEC`, 900 s) instead of a stampede,
  a maintenance lane (`JO_SCHED_MAINT_WORKERS`, 2) that collectors and pods
  never share, real cancellation; `log-retention` prunes fetch_log, anomalies,
  repairs and alert deliveries/events in `JO_RETAIN_BATCH` (5,000) batches.
* **Alerts**: shared-corpus rows meet every tenant's rules, and alert_events is
  UNIQUE(rule_id, item_uid) — one event per (rule, item), even under racing
  double-fires.
* **Breach**: an audited reveal that carries a reason (iOS sends it), paged
  search off the loop, a keyed lookup hash, and a corpus root of
  `$JO_BREACH_DIR`, else `$JO_REPO_ROOT/data/breach`, else the repo the binary
  was built from — never a path written into the source.
* **Detail hops** (hpengine): one admission point, `hp_detail_admit()`, spends a
  per-run budget of a count (default 1,000, was 25) and a clock
  (`JO_HP_DETAIL_SEC`); every record it did not reach carries
  `_detail_pending`, and the run files a `detail-truncation` notice with the
  counts. Every mode deepens, not only JSON.

The concurrency gate (`tools/ci_concurrency_gate.sh`) keeps three `/api/status`
LOOPS running for `LOAD_SEC` (12 s) rather than three single requests: a build
now takes ~0.25 s, which left the old gate passing or failing on timing. CI
runs it with a 0.5 s health limit.
