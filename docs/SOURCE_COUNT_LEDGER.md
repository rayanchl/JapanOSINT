# Source count ledger

The registered-source count over time, batch by batch, with what each batch
was seeded from, what it measured and what it taught. Moved out of `CLAUDE.md`
on 2026-10-19 so that file keeps the rules and the current number; the lessons
that a new batch must not relearn are summarised there. Newest first.

**The registered count is 48,794** (2026-10-20): the 48,507 below, plus batch
49's **290 ArcGIS feature layers** — the remainder of batch 46's own probed
pool, through the same filters and the same live probe
(`native/collectors/OSINT_SOURCES_BATCH_50_ARCGISLAYERS.md`,
`docs/verified-sources-batch50.tsv`) — and then a net −3 from a day of
measuring rows rather than adding them:

| change | rows | why |
| --- | --- | --- |
| batch 50 | +290 | batch 46's pool finished; its 164 were a batch-size cap, not a quality one |
| KKJ 三重県 | +1 | the set covered 46 of Japan's 47 prefectures and nobody had counted them |
| Setagaya street fire extinguishers | +5 | one dead row, looking for the name we expected, became five ward layers (4,951 records) plus the ward's earthquake-well register (1,148) |
| IDESCA San Carlos | −2 | `patentes` and `licores` withdrawn — 38 feature types and 0 catalogue hits for `patente`, `licores` AND `comercial`, so absent rather than renamed |
| jic-bestork.co.jp | −10 | the whole WordPress REST API behind `rest_login_required`, and its `/feed/` holds one default "Hello world!" item, so there is nothing to re-point at |

The day's larger result is not in the count at all: **654,412 KKJ procurement
notices became reachable** where 46,000 were being re-read forever, 47 rows
moved onto a measured date-window cycle after twelve probes established that
the portal honours no offset parameter under any name. Plus CV INE 50 → 2,425,
MET Malaysia 100 → 3,080 and 100 → 902, and EU Parliament 3,717 → 3,972.

Three tools were also found measuring their own limits and reporting them as
findings — the page-2 sweep's saturated egress tunnel (569 false dead
endpoints), Python's default TLS context (20 live rows called dead), and a
rebuild during an emit audit (229 of 290 rows as `NO_RUN_LINE`). All three are
now impossible rather than remembered; see CLAUDE.md.

**The registered count is 48,507** (2026-10-19): the 48,343 below plus **batch
46's 164 measured ArcGIS feature layers**
(`docs/verified-sources-batch46.tsv`, `docs/emit-sources-batch46.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_46_ARCGISLAYERS.md`).

**Batch 46 is batch 42's method on a THIRD family.** The tree already reached
195 ArcGIS service roots, each by ONE row naming ONE layer of ONE service — the
path somebody happened to know. A root publishes its whole directory at
`?f=json`: its folders, its services, and for each FeatureServer or MapServer
the layers and tables it holds. 195 roots → 899 candidate layers (a
self-imposed 5-per-root cap, not a limit the hosts set; one root alone published
899 services) → 819 probed PASS → 456 after dedupe of the `arcgis`/`ArcGIS` and
MapServer/FeatureServer spellings, a 50-record floor and dropping cartography →
164 selected at 2 per root. Measured: probe 164/164 PASS,
`audit_registry_emit.py` 163 OK / 1 SLOW, **1,832,156 records emitted and not
one row storing fewer than it emits**.

**It was authored as batch 45 and renumbered to 46 at merge time**, the third
session in this ledger to be renumbered by the same rule: 44 was the highest
number when it generated, and by merge time main had taken 45 (PR #50) and
renumbered its own 45/46 to 47/48, leaving only 46 free.
`docs/verified-sources-batch45.tsv` was a direct collision. Renumbering BEFORE
merging main kept it to one pass — the merge then had no both-added file.

Also in that branch, outside the batch: **`window_days`**, a date-window cycle
for a collection whose only slicing axis is a date
(`ENA_PATHOGEN_SARSCOV2_RUNS` read 1,000 of 7,556,500 for ever because
ebi.ac.uk honours no `offset` at all); **a sweep of all 530 cycling rows**
(`docs/cycle-health-2026-10-19.tsv`, `tools/audit_cycle_health.py`) which found
exactly one row stuck for a reason that was a property of its endpoint
(`BC_MINERAL_TENURE`, 50 of 42,234, GeoServer refusing `startIndex` without a
sort); and **`CR_SANCARLOS_PREDIOS` recovered** from a month documented dead —
its cadastre is published under `geonode:Catastro_sc`, which no `predio*` search
could match, and it now stores 20,000 records a run of 99,222.

**The registered count is 48,343** (2026-10-19): the 47,178 below merged with
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

