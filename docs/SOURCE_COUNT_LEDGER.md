# Source count ledger

The registered-source count over time, batch by batch, with what each batch
was seeded from, what it measured and what it taught. Moved out of `CLAUDE.md`
on 2026-10-19 so that file keeps the rules and the current number; the lessons
that a new batch must not relearn are summarised there. Newest first.

**The registered count is 56,955** (2026-10-21): main's 53,739 (its batch 49, PR #55) − 6 − 25 + 3,247 (batch 50).

* **−6**: `jp_feeds_batch43/44` feeds measured dead on the 2026-10-20 emit
  sweep (game-i.daa.jp serves XHTML, peco-japan.com an S3 AccessDenied,
  buddhanet.net HTML, tecture.jp an empty body, kotaku.com 403, kinmaweb.jp an
  HTTP/2 framing error on every attempt); suigenkyo moved from its comments
  feed to its blog feed. The merge with main had reintroduced all six.
* **−25**: batch 44/47/48 rows whose feeds now carry gambling spam under an
  official or press name — lapsed domains re-registered by betting sites, and
  official WordPress sites with injected casino posts.
  `docs/removed-sources-2026-10-20-takeovers.tsv` keeps the item titles that
  convicted each one; manifests are trimmed to match.
* **+3,247 — batch 50**, government and institutional RSS/Atom: Japan 883,
  US 915, UK 1,111, France 261, China/HK 39, international 38. Two seeds.
  Research agents proposed 1,744 feeds, 1,371 of them live (gov.uk's
  per-organisation `.atom` is most of the UK). Then **autodiscovery**: ~20,000
  official homepages — Wikidata `P856` for Japanese municipalities and
  agencies, US states/cities/agencies/universities, UK public bodies, French
  regions/départements/communes over 15,000 people and Chinese government
  bodies, plus every gov/academic host already in the tree — were fetched and
  their own `<link rel="alternate">` declarations read: 3,641 declared feeds,
  2,729 live. 802 were already registered after merging main (its batch 44 and
  47 seed from the same kind of list) and are not duplicated. Rejected rows,
  each with a reason, are `docs/rejected-sources-batch50.tsv`: 25 domain
  takeovers (27 found; the other two were already main's rows and are in the
  removal file), 16 hosts that refuse every honest agent, 5 comment feeds, 4
  misattributions, 1 ccTLD contradiction. Emit sweep: 3,237 OK, 2 collisions
  that are byte-identical upstream repeats, 8 transient or currently empty.
  Closed gov.uk bodies (`govuk_status=closed` in gov.uk's organisations API)
  poll weekly, not hourly.
* Two engine fixes came out of it: `lib/rss_atom.c` waits 25 s instead of 8
  (ipu-japan.ac.jp's 4.6 MB feed went from 0 to 500 records), and 48 more hosts
  get the no-"collector" agent in `core/httpclient.c` (86 rows recovered).
* Batch number 50, not 45: main merged its own 45, 47, 48 and then 49 (PR #55)
  while this one was open — it was renumbered at two merges. 46 is absent from main and was left alone in case an open branch holds
  it. Batches 43 and 44 now also name main's `JO43_` municipal and `JO44_` US
  government tables; the `jp_feeds` measurements are
  `verified-sources-batch4{3,4}.jp_feeds.tsv`.

**The registered count was 48,343** (2026-10-19): the 47,178 below merged with
main's 22,408 (21,789 plus PR #50's batch 45) (its own `jp_feeds_batch41..44.c` beats of Japanese RSS sources,
PR #48's paging cycle of 533 rows, PR #49's collector labels), less six news
feeds both sessions had added — `make lint-sources` named them at merge time
and main's rows stay. Batch numbers 43 and 44 are shared between main's
`jp_feeds_batch4N.c` beats and this branch's `hp3b4N_*` tables; nothing
collides, read the beat. Batches 47 and 48 below were authored as 45 and 46 and
renumbered at merge time when main merged its own batch 45 (PR #50) first.

**On main, merged into this branch at 48,343** — the numbering collision this
branch's batches 47/48 (authored as 45/46) came from:

**main's count was 22,408** (2026-10-19), measured — batch 45's 619
measured rows (`docs/verified-sources-batch45.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_45_JPWPREGISTRY.md`) on 21,789, which
was 21,237 plus batches 43/44's 400 Japanese RSS feeds and the rows that came
with them.

**Batch 45 is batch 42's question asked of the registry itself.** Every `.jp`
host a registered row already fetches (an RSS feed, an HTML register, a CSV) was
asked whether it also runs WordPress REST — the homepage's
`Link: rel="https://api.w.org/"` names the root, which found 232 of the 245
hosts that have one; a bare `/wp-json` found 13. 691 collections on 148 hosts
answered with records; 619 survived machinery filtering, probe and the emit
sweep (580 OK, 38 SLOW with thousands of rows stored at the 200 s line). The
tables are split by HOST type (media, company, association, culture, cyber,
science, hazard, government), not by collection type, so each beat files
under one collector group. The same sweep over **5,123 federation-member hosts**
(chambers of commerce, JA, medical associations, fire chiefs, welfare councils,
co-ops, professional bodies) found **5,678 live collections on 1,372 hosts** —
NOT registered, because ~5,700 scheduled rows on a 6-hour cadence is a
scheduler-load decision; kept with counts in
`docs/staging-sources-batch45.federations.tsv`.

**main's count was 21,237** (2026-10-12), measured — batch 42's 1,916
measured rows (`docs/verified-sources-batch42.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_42_JPWPROUTES.md`) on 19,321.

**The count was 47,178** (2026-10-19): 47,115 plus the 63 rows a
retry pass recovered. Every row batches 44–46 had dropped as dead on two
readings minutes apart — 2,724 of them — was rebuilt from git history and
re-probed a day later: 74 passed the probe, 63 of those stored their feed in
the engine and are back in their beats (4 US, 1 UK, 25 Italian, 4 French, 1
Wikidata public body, 2 international organisations, 26 news outlets), with
their readings appended to the batch's verified TSV and their `engine` lines
removed from its rejects. The other 2,650 are still dead: the UK and Italian
403 bot walls, and hosts that were down and stayed down. The same day the
interval of every public-body feed row was set from the newest-item date its
description records (hourly within 3 days, 6-hourly within 30, daily within a
year, else weekly), the World Bank sanctions row moved to hpengine so its
apikey could leave the source file, and the 20 French collisions that had been
unverifiable were re-measured: every one the feed's own byte-identical copies.

**Batch 46 is the Wikidata seed on the two classes an analyst asks for first:
the press and the international bodies.** Newspapers, daily and online
newspapers, news websites and news agencies per country (8,482 hosts → 3,980
rows), and international organisations, intergovernmental organisations and
think tanks in one country-less query each (2,141 → 492): 155,994 emitted /
155,790 stored. Two things generalise. **A generator's defaults are claims
too**: `build_feed_manifest.py` had hard-coded `category=government` and
`record_type=government-notice` since batch 44, so the first generation of the
news beat labelled newspapers as government notices — it takes those as
arguments now. And **Wikidata's class is a claim, not a fact** (it calls Capcom
a think tank), so the class lives in the description as Wikidata's, never in
the row name.

**The count was 42,643** (2026-10-18): batch 45's 16,520 measured
public-body feed rows (`docs/verified-sources-batch47.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_47_WORLDGOVFEEDS.md`) on 26,123. Both
floors were re-recorded at 42,643 and `make source-count` agreed.

**Batch 45 is the same method on three more registers, and it hit the registry
cap.** Italy's IPA (22,455 official sites → 5,640 rows), France's DILA annuaire
(28,885 → 9,319) and Wikidata's public bodies in 196 countries, harvested one
cheap (country, class) SPARQL query at a time because a world-wide query times
out (11,245 → 1,561 rows from 157 countries): 277,289 emitted / 276,945 stored.
Four things generalise. **`MAX_SOURCES` (`registry.c`) was 32,768 and the tree
reached 43,453**: the build was clean and 2,392 rows did not exist at runtime,
found only by comparing a beat's registration count with its table — the cap
is 131,072 now, and `make registry-floor` fails on any OVERFLOW line, so run it
before believing an audit. **A URL changed after validation is unvalidated**:
fixing an HTML-escaped `&amp;` in 534 French URLs made Joomla finally receive
`type=atom` and serve Atom where the rows declared `channel.item`; every changed
row was re-fetched, re-classified and re-measured. **The engine's measurement
outranks the probe**: 392 rows the probe refused (502s and 403s from shared
municipal hosting at twelve concurrent requests) store their feed at six, and
ship with both readings. And **rebuilding the binary while an emit audit runs
corrupts the audit** (NO_RUN_LINE, UNREGISTERED): finish or stop the audit
first. Feed titles are unescaped and whitespace-normalised by the builder now;
batches 43–45 were regenerated with the names fixed.

**The count was 26,123** (2026-10-17): batch 44's 3,715 measured
US and UK public-body feed rows (`docs/verified-sources-batch44.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_44_GOVFEEDS.md`) on 22,408. Both floors
were re-recorded at 26,123 and `make source-count` agreed.

**Batch 44 is batch 43's method on two public registers of official domains**
— CISA's 16,906 `.gov` domains and the Cabinet Office's 7,927 `.gov.uk` names —
each root fetched once for the feeds it declares or links: 2,075 US and 1,640 UK
rows, 45,705 emitted / 45,704 stored. What generalises: **a domain
registration is not a website** (4,372 `.gov` and 1,564 `.gov.uk` names have no
server at the root; 1,639 and 992 answer 403 to a non-browser client);
**WordPress declares a COMMENTS feed beside every posts feed**, so drop
`/comments/feed/` before it ships as thousands of rows of nothing; and
**the engine measurement outranks the probe in both directions** — 91 UK rows
answered 403 to the probe's client and 200 to curl, so the table was generated
with all of them and the emit audit decided: 98 of those 403s refuse the engine
too (a bot wall, removed), and 4 rows the probe refused store their whole feed
(kept, both readings in the TSV). `gen_hp_batch.py` now escapes `.name_ja` as it
did `.name`: a feed title with a double quote broke the build.

**The count was 22,408** (2026-10-17): batch 43's 1,156 measured
Japanese municipal feed rows (`docs/verified-sources-batch43.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_43_JPRSS.md`) on 21,252. Both floors were
re-recorded at 22,408 and `make source-count` agreed.

**Batch 43 is what the municipalities themselves publish, and how it was found
matters more than the number.** The Digital Agency's list of 1,648 open-data
municipalities was followed to each site root; the catalogue axis yielded zero
(776 hosts swept for CKAN/ODS/Hub/dataeye paths, body-judged:
`docs/discovery-jp-catalogue-hosts-2026-10-17.tsv`), the feed axis 1,156 rows on
546 municipalities — 195 declared in `<link rel=alternate>`, the rest found by
reading the root page's anchors. Three things from the gates generalise. **A
substring count of `<item` is not a record count**: RSS 1.0 wraps its sequence in
an `<items>` container, so 60 EMPTY feeds carried "1 items" until every row was
re-measured at the element start. **A well-formed feed with zero items is
`EMPTY_RESULTSET`, not UNPARSEABLE** — `probe_hp_batch.py` says so now; the 60 are
almost all 緊急 (emergency) feeds that are empty on every day there is no
emergency, and they are NOT registered, because a source never seen to emit is
not verified however plausible its purpose. And **one reading is one reading**:
18 probe transport failures became 11 PASS on a second probe, and the one
`FETCH_FAILED` of the emit sweep stored 10 of 10 minutes later. 45,217 emitted,
45,216 stored; the one collision is an upstream feed carrying the same notice
three times, two copies byte-identical.

**The count was 21,252** (2026-10-17): main's 21,237 (below) plus batch 37's 14
measured rows and the two data.e-gov.go.jp organisation rows it surfaced, less the
one duplicate the url-limit pass removed (`geo-stac-swisstopo-swissimage`).

**Batch 42 is batch 41 asked properly.** Batch 41 requested ONE collection per
Japanese WordPress host, `wp/v2/posts`, because that is the path everyone knows.
Asking each host for its own route table at `/wp-json` instead found 6,726
advertised collections on 433 hosts — 1,067 distinct custom post types, the
registers where a company publishes its IR releases and a university its staff —
of which 2,567 held records and 1,916 survived probe and the emit sweep. **The
generalisable part is the method: ask the endpoint what it serves, rather than
probing the paths you can name.** A convention sweep found 465 live endpoints on
6,720 hosts; the route tables of those same hosts held four times as many.

**The count was 21,237** (2026-10-12), measured — batch 42's 1,916 measured rows
(`docs/verified-sources-batch42.tsv`, `native/collectors/OSINT_SOURCES_BATCH_42_JPWPROUTES.md`)
on 19,321. **19,321** was batch 41's 437
measured Japanese WordPress-REST rows (`docs/verified-sources-batch41.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_41_JPWP.md`) on main's 18,884, which
made 19,321, LESS the one duplicate the url-limit pass removed:
`geo-stac-swisstopo-swissimage` read the same swisstopo endpoint, interval and
collection as the hand-written `swisstopo-stac`, and `make lint-sources` caught
it (dup-endpoint) the moment that row's page size changed to match. Both floors
are re-recorded at 19,320 and `make source-count` agrees.

**Batch 37 is the staging set finally probed (2026-10-12..17), and the headline is
15 of 80.** It was authored 2026-10-02 with no egress to any of its hosts, so every
host and path was inferred from its family's convention, and its README predicted a
high pass rate because a catalogue endpoint "needs only the host to be correct".
Sound reasoning, wrong conclusion, 19%: **a uniform API family tells you the shape of
the request, not whether the host serves it.** The whole `cam` beat is gone (every
ArcGIS path guessed; the server answers HTTP 200 carrying
`{"error":{"code":400,"Invalid URL"}}`, which only a body judgement can see),
eight cities have left Socrata, four national portals are not CKAN at all. The 65
are kept with their measured verdicts in `docs/rejected-sources-batch37.<beat>.tsv`;
the 15 are `hp3b37_{jp,fr,us,eu,row}.c`, every one measured emitted-AND-stored
(`docs/verified-sources-batch37.tsv`): Alberta 37,515, Greece 22,780, BODIK
18,535, opendata.swiss 16,521 — each equal to its own declared count.
Three defects were in the rows that PASSED, none visible to a probe: all 75 paged
rows lacked a stable sort (and Socrata's `order=name` is NOT unique — Colombia
stored 9,934 of 10,000 on it and 10,000 on `order=dataset_id`); `EU_ECB_FXREF`
keyed 29 daily rates on a field not on the record (repeated XML siblings are
INDEXED fields, not an array — read one record's `properties` before choosing a
key); and `JP_CKAN_DATAGOJP` pointed at `www.data.go.jp`, which **301s to
`data.e-gov.go.jp` and drops the query string**, so it stored ten of 18,826 at
`rc=0`. The first repair there lowered the stride to match "a server that serves
10" — the right observation and the wrong diagnosis. **Follow a 301 by hand before
concluding anything about what a server will serve.** And once it was pointed at
the real host it measured 18,826/18,826 — and `make lint-sources` removed it as a
duplicate: `hp3b23_jpnational.c` already reads that endpoint per organisation,
23 rows of `fq=organization:`. The portal has 25; the two it lacked are added as
rows of that family rather than as a second full read of the catalogue.

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

