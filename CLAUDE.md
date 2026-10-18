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

**The registered count is 21,237** (2026-10-12), measured — batch 42's 1,916
measured rows (`docs/verified-sources-batch42.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_42_JPWPROUTES.md`) on 19,321.

**Batch 42 is batch 41 asked properly.** Batch 41 requested ONE collection per
Japanese WordPress host, `wp/v2/posts`, because that is the path everyone knows.
Asking each host for its own route table at `/wp-json` instead found 6,726
advertised collections on 433 hosts — 1,067 distinct custom post types, the
registers where a company publishes its IR releases and a university its staff —
of which 2,567 held records and 1,916 survived probe and the emit sweep. **The
generalisable part is the method: ask the endpoint what it serves, rather than
probing the paths you can name.** A convention sweep found 465 live endpoints on
6,720 hosts; the route tables of those same hosts held four times as many.

**The registered count was 19,321** (2026-10-12): batch 41's 437
measured Japanese WordPress-REST rows (`docs/verified-sources-batch41.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_41_JPWP.md`) on main's 18,884.

**Batch 41 is numbered 41 because 38, 39 and 40 were all taken while it was
open.** It was generated as 38, and by the time it came to merge, another
session's batch 38 (africa/china/france/uk/usa) had landed along with 39 and 40
— including a *direct* collision on `docs/verified-sources-batch38.tsv`, the
exact file the note below predicts. Renumbering touched the ids (`JO38_`→`JO41_`),
the four manifests, the four tables and their array symbols, the rejected and
verified TSVs, the batch doc and its filename. Re-check at merge time, not just
at generation time — this branch had to do it twice, because main moved again
between the renumber and the merge.

**The count was 18,884** (2026-10-12: 18,883 plus the `log-retention`
maintenance pod from the infrastructure audit, see the last section). 18,883
(2026-10-09) was main's 18,730: 18,645 —
batches 33–36 (batch 33's 43 measured supply-chain rows,
`docs/verified-sources-batch33.tsv`; batch 34's NDJSON row; batch 35's four
string-array rows; the spec-families beats and batch 36's 188) and the audit
follow-up's two SEC listings, on top of the 18,170 that included batch 32's 340
Japanese rows, less batch 31's `EU_VIES_VAT_VALIDATION` — plus batch 38's 150
(PR #37) less 65 batch-31 rows removed for rule 4d (PR #38). Then the
JP/CN integration's 168: batch 40's 100 Japanese and Chinese rows
(`docs/verified-sources-batch40.md`; authored as "batch 38" and renumbered when
main's batch 38 merged first) and batch 39's 72 rows unlocked by date tokens /
JSONP / body paging / HTML tables less the frozen VCSV row one of them replaced
(`docs/verified-sources-batch39.md`) — less batch 36's `CN_HK_LANDS_ADDR`, a
fuzzy geocoder that answers any text with addresses (`FILTER_IGNORED`,
`docs/rejected-sources-batch36.china.tsv`), and less 14 batch-36 Socrata
portal inventories that re-read a catalogue `hp3_portals.c`/`hp3_society.c`
already reads (`docs/rejected-sources-batch36.usa.tsv`). `make lint-sources`
prints it, counting hp_source table rows as well as `REGISTER_SOURCE`, and
`./bin/japanosint --list-sources` agrees.

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
`socratacat` (139 rows, unverified); batch 35 is `strings` (4 rows, measured)
plus `jpweko` (100 rows, unverified). Nothing collides — the beat names keep
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

**Batches 34 and 35 are unverified, for the same reason as batch 31:**
authored 2026-10-04 in a session whose network policy allowed only Anthropic
APIs, package registries and GitHub, so nothing was fetched and there is no
`verified-sources-batch3{4,5}.tsv`. Both are built to be as close to provable as
an unprobed batch can be — **every host is proven live by a different row
already in the tree**, and every path, parameter set, paging rule and envelope
is fixed by a specification or copied from a row that passed the emit audit —
and **both have manifests**, so the probe and emit tools point straight at them.

* **Batch 34, 139 rows** (`hp3b34_{ckanbulk,fdsnstation,fdsnevent,socratacat}.c`):
  95 CKAN `current_package_list_with_resources`, 15 FDSN `station/1`
  inventories, 5 FDSN `event/1` catalogues, 24 Socrata Discovery pivots.
  `native/collectors/OSINT_SOURCES_BATCH_34_SPEC_FAMILIES.md`.
* **Batch 35, 100 rows** (`hp3b35_jpweko.c`): WEKO3 `/api/records/?q=` entity
  pivots on the 100 largest JAIRO Cloud repositories that were wired for OAI
  harvesting but could not be ASKED about an entity — endpoint and keys copied
  from batch 25, which ran them through the emit audit and `--check-filter`.

```sh
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint --match JO34_ --jobs 6 --timeout 220
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint --match JO35_ --jobs 6 --timeout 220
python3 native/tools/probe_hp_batch.py docs/candidate-sources-batch3{4,5}.*.txt --check-filter
```

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
* **the rest of the tree — 0 findings** (1,678 files scanned, 2026-10-07; it
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

  The first fix was the highest-leverage one: `od_udata_collect()` in
  `collectors/sources/od_shared.inc` made ONE request while the envelope
  published both `next_page` and the upstream's own `total` — so three national
  open-data catalogues stored 20 datasets each. It pages now, and discloses a
  ceiling or a `total` it did not reach:

      data.public.lu   20 -> 3,193 stored (= its declared total)
      dados.gov.pt     20 -> 20,933
      data.gov.rs      20 -> 3,527

  27,653 records from three rows that together stored 60.

  **Every candidate is now MEASURED, and the file to start from is
  `docs/measured-url-limits-2026-10-18.tsv`** — 101 rows, one per candidate.
  Its predecessor `docs/measured-url-limits-2026-10-11.tsv` said "18
  truncating, 5 complete, 19 unreachable" of 42 and that "the other 15 are
  listed here so the next pass has the measurement rather than the inference":
  **those rows were never written.** That file has held 24 lines and ZERO data
  rows since the commit that created it, and this paragraph cited it for a week
  — so the next pass was being pointed at nothing, which is the same invisible
  nothing rule 1 is about, one layer up in the documentation.

  **The tenfold-limit test over-reports on its own, and a candidate must also
  be read in its collector.** `od_udata_pt.c` — fixed by the very commit that
  wrote the empty file — answers 200 for one request and 2,000 for a tenfold
  ask, which is indistinguishable from a discard; but `od_udata_collect()`
  already walks it with `pw_walk`, so its 200 is a PAGE SIZE. One request
  compared with one request cannot see a collector that follows pages.

  **And it under-reports what can be DONE about one.** The first pass of that
  day filed 17 rows as "no next link and no offset/page parameter in the URL,
  so `pw_walk` would have to INVENT one" and stopped there. That reads
  `pw_walk`'s rule 2 backwards: it never invents a parameter, but it **does
  advance one the author wrote**, and 11 of those 17 upstreams document a
  continuation their collector had simply never asked for. **26 of 101 truncate
  on a single request; 14 rows across 11 sources are now fixed**, each proved by
  a run rather than by the spec:

      bc-mineral-tenure             500 ->  42,277   (= numberMatched)
      openaerialmap-imagery          50 ->  21,426   (= meta.found)
      openfda-enforcement-reports   100 ->  18,010   (= meta.results.total)
      who-gho-indicator             100 ->  14,112   (= $count=true)
      nasa-eonet-events              50 ->   7,222   (the whole open set)
      peertube-instance-directory   100 ->   1,772   (= the envelope's total)
      stats-ar-datos-series         100 ->     271   (= the series' length)
                                  1,000 -> 105,090

  plus the four paged on a published `links[rel=next]` (590 -> 11,800, the
  `geoeo_stac_items.c` catalogues and `eni_canada_hydrometric.c`) and three
  real-time seismic rows raised to their served maximum, 650 -> 3,000, each now
  filing the truncation notice it had been owing. **Three traps worth carrying
  forward:**

  * **Two services implementing ONE spec disagreed about the base index.** FDSN
    says `offset=1` is the first record. EMSC's `fdsnws/event` does that;
    Earthquakes Canada's does not, and neither does EMSC's own `mtws` search
    (`offset=2` starts at the third record). A stride written from the spec
    would have skipped one event per page boundary, silently, on two of the
    three. Ask for `offset=2` and see whether its first record is the plain
    page's second or its third — one request, and not inferable.
  * **A 400 on a paging parameter is worth reading before it is written off.**
    BC's GeoServer refuses `startIndex=500` with "Cannot do natural order
    without a primary key, please add it or specify a manual sort over existing
    attributes" — it would rather fail than hand back an arbitrary slice of an
    unsorted result. `sortBy=TENURE_NUMBER_ID` made it 200 with provably
    disjoint pages, and 500 records became 42,277.
  * **A parameter can be accepted and ignored.** EONET answers `offset=50` with
    the same 50 events as `offset=0`, HTTP 200 — writing it would have made the
    walk re-emit page 1 twenty times, which is rule 4d in paging form. Its
    `limit` is what is honoured: one request at `limit=20000` returns all 7,222
    open events. **Measure the continuation; do not reason about it.**

  The 12 rows still not fixed each carry their own reason, and the reasons are
  different in kind — a cross-page comparison that per-page emit would corrupt
  (`geoeo_gc_ogcapi.c`'s `superseded()`, so **a next link is permission to page
  only when the emit is per-record**), a licence that forbids bulk reuse (Find
  Case Law), a pipe-separated text body that `pw_walk`'s JSON fetch cannot
  carry, an upstream that answers "the 'Skip' query option is not supported"
  and also refuses an unbounded read (CBS), a real-time tail where re-walking
  20,000 records twice an hour would never reach the back of the catalogue, and
  an unbounded firehose with no end to truncate. The remainder could not be
  asked as literals: 32 `RUNTIME_URL`, 16 `UNREACHABLE` from this container, 11
  whose URL would not join, 10 whose tenfold ask was refused, 5 capped at the
  same count both ways, 1 complete.

  `tools/audit_url_limits.py` finds the shape, and is deliberately NOT in
  `make audit-sources`: **a URL limit is not automatically a discard and the
  script cannot tell.** A relevance-ranked search pivot asking for the top 15
  has no complete answer to truncate — that limit IS the question. A scheduled
  read of a collection that has an end stores `limit` of however many exist,
  forever, and that is the violation. They are identical in C. The narrow scope
  reports **89 sites across 67 files** (103 across 78 before the fixes above,
  100 across 75 after the first half of them), so running it is the START of
  the work; wiring it into the gate would put ninety candidates — most of them
  legitimate, several of them page sizes — into a tree whose invariant is that
  a new finding is a regression. The triage, with a verdict and a reason per
  site, is the 2026-10-18 file.

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
| `tools/probe_hp_batch.py` | proof of life: 2xx, parses in its declared mode, ≥1 real record. Honours each row's own headers, handles JSON/CSV/XML/HTML, rejects empty result sets, HTTP-200 refusals, one-element error arrays and bot-wall challenge pages. A **declared `array_path` is resolved and judged** — it used to hunt for the densest array and once counted a response's own 252-key *schema block* as records, passing a row whose result set was empty. **`--check-filter`** additionally asks each pivot row about an IMPOSSIBLE entity and fails it `FILTER_IGNORED` when the answer is the same size — see rule 4d. Both answers are judged by the same counter (XML/CSV/HTML/xlsx/`array_path` aware); a comparison that could not be made is `FILTER_UNCHECKED`, never PASS. It waits each row's own `timeout_ms`, renders `{date:}` tokens and matches keys case-insensitively, as the engine does |
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
python3 native/tools/audit_batch_emit.py docs/candidate-sources-batch<N>.*.txt \
        --bin ./bin/japanosint --jobs 6
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
    per run against one host. Those rows are a DISCLOSED sample and need a
    different strategy per row (a date window, a filter that slices the
    collection, or a bulk dump), which is a design decision and not a number.
    Listed in that file so the next pass starts from the measurement.
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
`intel_vec`; `/api/intel/semantic` queries it on a worker thread, never the
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
  targets (`JO_CAMERA_ALLOW_LAN=1` reopens LAN only).
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
