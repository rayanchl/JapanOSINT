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

**A REFUSAL CAN WEAR A SUCCESS STATUS, and then the note points the wrong
way.** Archive-It answers `<title>Rate limit reached</title>` with HTTP **200**,
so the fetch-failure note read "the endpoint may have moved, started
redirecting to an HTML page, or changed shape" — sending a reader to hunt a
move that never happened, when the remedy is a per-host gap. A non-parsing 200
whose body says "rate limit" or "too many requests" is named as one now, with
the remedy pointing at `core/hostgate.c` AND telling the reader to measure the
budget first, because a gap set on one reading is a guess. No gap was added for
Archive-It for exactly that reason: it refuses single requests an hour apart,
so its budget cannot be measured from behind the limit. `hptest` 44 pins the
shape and 44b pins that an ordinary unparseable 200 is NOT reported as one —
"limit" is a legitimate parameter name and appears in half the URLs in this
tree, so only the refusal's own words count.

**A BOT WALL does the same thing and wants the opposite remedy**, which is why
it is named separately (2026-10-20). `catalogue.data.govt.nz` answers its CKAN
bulk endpoint with HTTP **200** and Imperva's `<title>Pardon Our
Interruption</title>`, so the note again said "the endpoint may have moved" —
the third reader sent hunting a move that never happened. Pacing cannot reach a
wall that refuses an automated client at any rate, so the remedy must not be a
hostgate gap either: it points at `core/httpclient.c`'s UA_OVERRIDE table, where
one WORD in the agent is the measured cause on every host so far. `hptest` 45,
and 45b pins that a body merely saying "access denied" is not reported as a wall
— permit decisions and FOI logs are full of the phrase.

**And the inverse mistake: AN UPSTREAM'S OWN "NOTHING MATCHED" IS NOT A
REFUSAL.** FDSN's `nodata` parameter names the status a service must use for an
empty result and every FDSN row here passes `nodata=404`, so those rows ASK to
be told "nothing" with a 404 — and the fetch-failure note read it back as "the
upstream refused the request", remedy "the endpoint has moved or retired". That
is this notice making the very confusion it exists to prevent, since its whole
job is to separate a refusal from a source that ran and found nothing.
`hp_status_is_declared_empty()` now reads a 204, or a 404 whose BODY carries the
service's own no-data wording, as the honest empty rule 1 asks for: rc=0, no
records, no notice, and the bytes and request count on the run line showing it
went and looked. A window row counts such a read as SERVED and advances, or it
would re-read one empty window for ever and never reach the records beyond it.

The discriminator is the body's words, never the status and never the row's own
URL, and both halves were measured on one beat. `ws.icgc.cat` is live
(`/fdsnws/event/1/version` → `1.2.4`), holds no events for any window including
a whole year, and answers the identical query with **204** when `nodata` is not
forced — empty, not broken. `eida.koeri.boun.edu.tr` answers the SAME 404 to the
same row with "The requested resource does not exist on this server", and its
`/fdsnws/event/1/version` is 404 while `dataselect/1/version` is 200: a node
that does not run the service at all. Keying on the status, or on the row having
asked for `nodata=404`, would have filed that dead service as a healthy empty —
the dangerous direction. `hptest` 46–46d, each checked against the old
behaviour.

It changes two things that read the run line. A dead source now reports
`records=1 stored=1`, so `audit_registry_emit.py` counts engine notices
separately from records and has a `FETCH_FAILED` verdict; and `hptest`'s
404/5xx assertions now pin zero RECORDS plus one disclosure, which is what they
always meant.

**AND FOR SIXTEEN DAYS IT WAS hpengine's DISCLOSURE AND NOBODY ELSE'S.** The
notice above is written by `lib/hpengine.c`, so the 44,017 hp table rows
disclosed a refused fetch and the **~7,350 VJSON/VGEO rows and ~1,250 bespoke
`REGISTER_SOURCE` collectors kept the old silence** — `records=0 stored=0`,
indistinguishable from a source that ran and found nothing, which is this rule's
whole subject. This is the lesson this file already records twice, hit a third
time: the uid-collision guard existed in three record paths and was repaired in
one, the page ceiling lived in three places with two knobs, and **when you fix
something, look for the other copies.**

Measured 2026-10-22 by the registry-wide sweep: of its first 3,612 rows, **288
came back `rc=-1` having emitted nothing and stored no notice** — 18 of 18
sampled were `vsrc` rows — against 34 that DID disclose, every one an `hp16_*`
row. About one row in twelve, failing in silence, and the sweep could only
report them `EMITS_NOTHING`: a verdict about a source that FETCHED and stored
none, a different defect wanting a different fix.

`core/scheduler.c` files it for every collector now (`sched_disclose_fetch_failure`),
because the scheduler is the ONE place that sees every run. Four things about it
are deliberate, each pinned by a fixture that fails without it
(`test_sched_runline` 3–3f, and the over-broad variants were checked too):

* **It states only what the run exposed** — the return code, hosts contacted
  and hosts that answered, bytes, requests, the thread's last transport-error
  class, and which hosts never answered. **There is no `http_status`**, because
  a bespoke collector never reported one, and naming a status nobody measured
  would be this notice inventing the cause — the mistake it has already made
  three times above (Archive-It, the Imperva wall, FDSN's `nodata=404`). The
  remedy says where to look, including that in this container a peer reset can
  be OUR egress.
* **An honest empty stays silent**, which is what makes it safe to ship: the
  first test is `run_status()`'s verdict — the same classifier `fetch_log` and
  the dashboards use, not a second one beside it (rule 4c). A failure notice on
  every quiet feed would be noise, and noise is the one thing these disclosures
  cannot afford.

  **AND THAT ALONE LEFT A THIRD OF THE SILENCE IN PLACE** (2026-10-23).
  `run_status()` returns "ok" whenever `hosts > 0 && hosts_ok == hosts`, under
  a comment reading "fetched fine, feed was empty". The sweep's
  `EMITS_NOTHING` column says that comment is wrong for **32 of its 80 rows**:
  `il-knesset-dates` returns rc=-1 after fetching **131,618 bytes** — the
  OData service's HTML maintenance page — and `geo-defra-ukair-stations` and
  `eco-511ny-events` do the same with 2,038 and 9,364 bytes. Every host
  answered, so the run read "ok", so nothing was disclosed, and the database
  again could not tell a refusal from an empty source. A refusal wearing a
  success status, one layer up from the Archive-It case hpengine already names.

  So there is a **second trigger**: the collector returned non-zero and left no
  record and no notice of its own. That is not a second status classifier —
  `run_status()` still decides red or green and `fetch_log` and the pages are
  untouched — it is a different question: not "is this source healthy" but
  "can the database tell what happened". The record distinguishes the two
  triggers, and for the new one it **names both readings instead of picking**:
  when every host answered, the run is either a refusal served as a success or
  a collector that returns non-zero on a quiet hour, and the remedy says to
  read the BODY and not the status (the byte count is on the run line and in
  the record). In the second case the finding is the return code itself — a
  source that published nothing should return 0. The ambiguity is affordable
  because the record is upsert-keyed on (source, entity), so a quiet feed with
  a sloppy return code carries ONE row that updates, and the class is 30 rows
  in 10,252 measured. `test_sched_runline` 3g pins the disclosure and **3h
  pins its control** — the same answering host with rc=0 stays silent — and
  the over-broad variant (disclose whenever a run stored nothing) was checked
  to fail 3c.
* **A row that disclosed anything itself gets nothing more**, which is also
  what keeps every hpengine row out of this path without naming one: the
  engine's own record goes through the same sink and counts into `records=`.
* **A maintenance pod is exempt** (`_` collector), or this would make the very
  confusion `audit_registry_emit.py`'s `POD` verdict exists to expose.

`audit_registry_emit.py` is the third copy and was fixed in the same change:
its `FETCH_FAILURE` pattern matched `[hp:…]` lines only, so a scheduler-filed
disclosure would still have read `EMITS_NOTHING`. Extending it exposed a
quieter defect in the same tool — a FETCH_FAILED row reads `emitted=1` (the
disclosure counts as a record on the run line) against `db=0` (the database
reading excludes `collector-fetch-failure` by design), so the difference was
tallied as a record **"lost to uid collision"**: a disclosure counted as a
discard. Invisible at 34 rows; it would have become the headline number once
every refused row in the fleet files one. Those rows are counted and named
separately now.

One consistency worth knowing rather than discovering: a disclosed dead source
reads `records_count = 1` on its sources row and the dashboards, because the
disclosure is not a `collector-*-notice` and `stored_data` does not subtract
it. That is exactly what the 44,017 hp rows have done since 2026-10-06, so the
new path matches them rather than inventing a second convention.

**A FEED URL THAT HAS STOPPED BEING A FEED IS A FOURTH SHAPE OF THE SAME
NOTHING** (2026-10-23, `lib/rss_atom.c`, `make rsstest`). That file already
argues the case for an empty BODY — "no RSS or Atom document is zero bytes
long, so this is an error" — and the next step in the same argument had not been
taken: a 2xx body that is not a feed AT ALL is not an empty feed either.
`https://www.fijitimes.com.fj/feed/` answers HTTP **200**, `text/html`, and
**128,800 bytes of the newspaper's homepage**; `nation-thailand` (107,639 B),
`daily-mirror-lk` (70,103 B) and `cyb-cybereason` (106,845 B) do the same. The
parser found no `<item>`, emitted 0 and returned 0, so each run read
`rc=0 records=0 stored=0` — byte for byte what a newspaper that published
nothing today looks like. Four national press feeds, dead, and nothing said so.

The rule is deliberately narrow, and `rsstest`'s own controls are what keep it
there: **it runs only when the parse found no items**, so it cannot change a run
that emitted anything, and a body carrying `<item` or `<entry` is a feed
whatever its root element is called, so the parser's own vocabulary decides
rather than a list of root names. The over-broad variant (fail whenever a feed
holds nothing) was checked and fails exactly the three honest-empty fixtures.

The discriminating case is worth knowing: `news24-za` returns **424 bytes of
valid RSS with today's `lastBuildDate` and zero items** — a feed that is being
built and is serving nothing. It stays silent, correctly: the body IS a feed,
and the finding there is the upstream's.

**AND THE DISCRIMINATOR FOR EVERY REMAINING SHAPE WAS ALREADY ON THE RUN LINE:
THE BYTE COUNT** (2026-10-25). The complete registry sweep read 631 rows
`EMITS_NOTHING`, all carrying one note — "no records reached intel_items" —
which is identical for a feed that published nothing today and for a row that
fetched 131,618 bytes of a maintenance page. `fetch_log.bytes_fetched` has been
on the run line since 2026-10-12 and the gate was discarding it. With it, the
467 rows that survived re-reading split **370 honest empties to 97 that
transferred a real body and found no records in it** — and only 8 of those 97
had disclosed anything. The 97 are a ranked list; the top of it was three
distinct defects, each a shape this rule had not yet recorded:

* **EVERY FIELD OF A RECORD CAN BE NESTED, and then a VJSON row stores
  nothing.** Four rows read `laws.e-gov.go.jp/api/2/laws` and transferred 4.1,
  4.4, 4.5 and 2.0 MB a run — **14.9 MB** — for `rc=0 records=0 stored=0`. An
  e-Gov law record is `{law_info:{…}, revision_info:{…},
  current_revision_info:{…}}` with not one top-level scalar, and VJSON's uid
  and title precedence lists are single-level BY DESIGN: `pick()` looks up a
  bare name and deliberately skips a nested value so a later key in the list
  can still match. With every candidate nested there is nothing to key on and
  nothing to title. **No VJSON row can read such an API whatever its keys
  say**, so the fix is never per row: the four are replaced by one hpengine row
  (`EGOV_LAW_LIST_ALL`), where `id_keys`/`title_keys` take dotted paths —
  9,567 of 9,567, its own declared total exactly.

  **IT IS A FAMILY, AND THE SCAN FOR IT COST FIVE REQUESTS.** Two unrelated
  APIs in one hour made it worth looking for the other copies rather than
  waiting to meet them: of the 97 rows, five are VJSON rows with a literal
  endpoint, and **four of those turned out to be this shape** — ECHR's HUDOC
  (`{"columns": {…}}`, 232,481 documents), TDnet's mirror (`{"Tdnet": {…}}`),
  and both InterPro rows (`{"metadata": {…}}`). Eight rows across five APIs in
  one day. The CHECK is mechanical and worth running on any row that transfers
  a real body and stores nothing: resolve the record array, and ask whether the
  first record has a single top-level scalar. The two that were NOT this shape
  matter as much — `sci-usgs-astro-pds` carries a top-level `id`, and
  `geo-gfz-kp-nowcast` is parallel arrays and not records at all — because the
  symptom is identical and the remedies are not.

  **And one of them keys on BOTH shapes, because the shape depends on a
  parameter's VALUE.** TDnet's mirror returns items wrapped in `Tdnet` for no
  limit, `limit=3`, `300` and `500`, and **FLAT for `limit=100`** — same
  endpoint, same minute, measured twice at each value and stable every time. A
  row keyed on the wrapped path alone stores nothing the day somebody tunes the
  limit to 100. `Tdnet.id,id` reads the wrapped path and falls back to the flat
  one, which is what `,` CHOOSES is for.
* **"HONEST EMPTY, NEVER rc=-1" IS A CLAIM ABOUT THE UPSTREAM, AND IT CAN BE
  FALSE.** `denki_yoho.h` returned 0 when it parsed no observation, under a
  comment saying exactly that. There is no quiet hour for electricity demand:
  the CSV always carries the current hour, the rows run every 300 s, and the
  collector already retries yesterday. `kyushu-power` fetched 107,690 bytes of
  an HTML landing page every run — its dated CSV now 302s to
  `/td/power-usages01.html` after a site redesign — and said nothing. It
  returns -1 now, so `sched_disclose_fetch_failure` names the hosts, bytes and
  remedy. Two of the ten TSO rows were affected; the other eight are untouched.
* **AN ANCHOR CALENDAR DEFEATS THE LABEL MINIMUM, and that was the fourth
  record path without a disclosure.** The US NRC's event-notification year
  pages carry 205 and 245 real `<a href>` anchors, every one matching the row's
  `href_must` — and the listing is a calendar, so every visible label is the
  day of the month: "23", "22", "1". hpengine rejects a label under three
  characters, which must stay (it is what keeps pagination digits and "»" out
  of every HTML row), so a page whose links are ALL labelled that way read as a
  page with no records. The JSON path has had `label-keys-unmatched` for this
  shape all along; the HTML path has `anchors-unlabelled` now, naming how many
  anchors matched and telling the reader NOT to lower the minimum. `hptest`
  57/57b, with 57c/57d as the controls — and the first version of 57 proved
  nothing, because putting the anchors under an `<h3>October</h3>` let the
  context fallback borrow that heading as a label: **a fixture that does not
  reproduce the condition is not a test of it.**

Two gate lessons from the same pass, both the shape of the `POD` and `GATED`
corrections: a verdict must not put a source behaving correctly in a defect
bucket. **A row that DISCLOSED is not a row that stayed silent** — 8 of those
97 had filed a shape notice and the verdict read the same for both — and **a
gate that gives every row a fresh database reads a date-window row's window 1
of N**, which is how 47 healthy KKJ prefecture rows came back `EMITS_NOTHING`
(`WINDOW_EMPTY`, above).

Full audit of how each collector behaves today:
`native/collectors/SOURCE_REALITY_REPORT.md`.

**THE REGISTRY HAS NOW BEEN READ END TO END, AND THE READING IS THE POINT**
(2026-10-25, `docs/registry-health-sweep-2026-10-25.tsv`). Every scheduled row
run through the real binary against a fresh copy of a warm template database:
**55,834 rows — 49,375 OK, 4,761 FETCH_FAILED, 631 EMITS_NOTHING, 472 SLOW
(unmeasured, not failed), 461 COLLISION, 114 GATED, 19 POD, 1 SINK_MISMATCH.**
Four lessons from working it, each costing more than the number it corrected:

* **A GATE THAT RUNS EVERY ROW IN A FRESH DATABASE CANNOT SEE A CYCLE'S
  POSITION, so it reads window 1 of N and calls it dead.** All 47 KKJ
  prefecture rows came back `EMITS_NOTHING` — forty-seven healthy sources in a
  defect bucket — because a `window_days` row with no stored position starts at
  its declared `window_from`, and kkj.go.jp answers that first window with
  `<SearchHits>0</SearchHits>` in 144 bytes while holding 218 records in a
  window near today. `WINDOW_EMPTY` joins `POD` and `GATED` as the third
  correction of the same shape: a source behaving exactly as designed does not
  belong in a defect bucket, and the note says to re-read with a window near
  today before treating it as a finding. **The tidy-up that followed was
  measured and REFUSED**: Japan's FY2021 boundary really does hold (Aomori 0 in
  each of 2021-01..03, then 108, 68, 117 from April; the quarter empty for
  Hokkaido, Aichi, Osaka, Fukuoka and Okinawa), but **Tokyo has 1 record in
  it**, so moving `window_from` would discard a notice to save two runs on a
  fresh install. Six prefectures agreeing is not the seventh.
* **THE LARGEST FAILURE CLASS SAID NOTHING, BECAUSE THE GATE WAS MATCHING
  STDERR WHILE THE ANSWER SAT IN A ROW BESIDE IT.** All 4,761 FETCH_FAILED rows
  carried one fixed sentence — "the upstream refused the request or served a
  body that did not parse" — which is the verdict restating itself. The engine
  has written the cause as DATA since 2026-10-06 and, for every other
  collector, since 2026-10-22, the nine TLS causes among it. That is rule 4c
  against the engine the tool reports on. `fetch_failure_reason()` reads
  whichever fields are present rather than assuming a shape, because the two
  writers differ on purpose; a status of -1 is "nobody measured one" and is NOT
  printed as a status, for the reason this file records three times over.
* **MOST OF A COLLISION LIST IS STALE OR ALREADY ANSWERED, so re-measure
  before reading.** Of the 30 rows losing ≥250 records a run, 8 were fixed
  earlier the same week and the sweep simply predated the fix (UK Parliament
  10,000/10,001, JCG 19,370/19,370, VNDirect, SECOP, dataeuropa), and 12 more
  carried their own arithmetic in a comment saying "do not re-key this row".
  The single biggest number in the whole sweep, `gr-diavgeia-positions` at
  25,287 emitted / 233 stored, is a 1.6 MB file in which the upstream repeats
  233 position titles 25,188 times byte for byte. **A collision count ranks
  rows to look at; it is never a budget of what they cost.**
* **AND THE GUARD GIVES A FREE DISCRIMINATOR NOBODY WAS USING.** The in-page
  content-hash guard covers one page or array, so **a SINGLE-REQUEST row's
  surviving collisions are byte-identical by construction** and need no
  investigation at all. Validated rather than assumed: `geo-jrc-inform-scores`
  serves 46,461 rows carrying exactly **46,079 distinct whole records**, which
  is precisely what the engine stored. That took the list worth reading from
  419 rows to the handful that make more than one request.

**Every registered source is now proof-of-life verified.** Batch 14's 1,001
unverified `csrc14_*` candidates were probed and promoted (594 PASS →
`vsrc14_*`); batch 15 added 1,029 more (`vsrc15_*`, see
`docs/verified-sources-batch15.md`). Rejects are kept as data in
`docs/rejected-sources-batch{14,15}.tsv`. No `csrc14_*` file remains.

**The registered count is MEASURED ON THIS MERGE** (2026-10-25, the fifth merge
of main into this branch): `make source-count` is the number, never the
arithmetic, and the removals on both sides are why. This merge took main's
2026-10-25 HTML reachability sweep, and resolving it per row is the lesson
worth more than the count — **main's sweep measured ten of this branch's rows
at a URL they no longer carry.** Of the 23 rows in conflict, 13 had been
re-pointed here and measured on the new endpoint (the ZA gazette 3 anchors for
a real entity and 0 for an impossible one, Municipal Money 39 facts of a
declared 39, the ECB workbook, four Stortinget exports, Eduskunta's
`avoindata` API, and the three bulk-file stand-ins), so main's 404 is a true
reading of a dead path and a false one about the row that replaced it. The
other 10 were byte-identical to what main measured dead in two passes with the
page, the bare path AND the site root each checked, which is stronger than
this file's own "one probe is one reading" — so those removals stand, four of
them overturning a decision taken here to keep a dead pivot "disclosing".
**Both sessions had measured the same fact** (this branch's own note says
PMDA's search "answers the form page to every POST constructible here" and the
Assemblée's "404s with no form"); they differed only on the policy, and the
tree's own precedent settles it, from the FAA case: "Three sources removed
rather than left registered and failing every run." The three stand-in rows
that replaced them survive, so no coverage is lost. **A sweep's verdict is
about the URL it sent, not about the row's name** — re-read a removal against
the row as it stands now.

**The registered count is 57,905** (2026-10-28, the merge of the two
readings below). MEASURED on the merged tree with `make source-count`, and
only then cross-checked, which is the order this file accepts: 57,921 less
the twenty twin-group rows, plus one, plus batch 56's three = 57,905, and the
binary's own registry agrees at 57,905 with 0 duplicates, 0 dropped and 0
never-runs. Both floors re-recorded from that build. Neither side's number was
wrong and neither was the merge's: a count is a reading of one tree, so two
branches that each measured correctly still cannot be added together.

**The registered count is 57,924** (2026-10-27, `make source-count`, both
floors re-recorded from that build): 57,921 plus **batch 56's three rows**,
the rest of the New Zealand Charities Register beside its organisations —
every officer (589,506), every annual return (430,937) and the charity groups
(357), `collectors/pivot/table/hp3b56_nzcharities.c`. The organisations row
itself had read the same first 100 of 46,144 charities every run, its links
had all redirected to the search page, and the register's other 22 OData sets
were unread. **A DECLARED TOTAL CAN BE A STRING**: OData v2's verbose JSON
writes `d.__count: "46144"`, and both declared-total lists accepted numbers
only, so every row on such a service read "the upstream declares no total"
with the number in its envelope. `pw_odata_v2_count()` (lib/pagewalk.c) reads
it, digits only, for both lists; `hptest` 58–58c, 58 and 58c checked to fail
without it.

**The registered count is 57,901** (2026-10-25): 57,921 less the twenty rows
of the nineteen twin groups below — one read registered twice, nineteen times
over. MEASURED from that build: `make source-count` 57,901 and the binary's own
`--list-sources` 57,901 with 0 duplicates, 0 dropped and 0 never-runs, both
floors re-recorded at 57,901. The number was predicted by arithmetic and then
read, which is the only order this file accepts.

**Measured 2026-10-25 on the merged tree at 005e4ee8** (main's 57,923 plus the two IOC sea-level retirements rebased onto it, `sci-ocean-ioc-sealevel-tab` and `geo-ioc-sealevel-stations`): registry 57,921, `make source-count` 57,921, both floors re-recorded at 57,921 from that build, not by arithmetic.

**Main's count was 57,394** (2026-10-25): 57,393 plus `US_FCC_ENFORCEMENT`
back as a hand-written collector (`collectors/sources/fcc_enforcement.c`) on
the FCC Content API's server-side title search (`search_string`, a PREFIX
match: "Verizon" 401 documents, an impossible term none). 57,393 was 57,395
less two — `US_FCC_ENFORCEMENT` in its old form, a search whose query never
reached the server (rule 4d), and `gov-hk-legco-bills`, a second reading of
the LegCo bills feed (`docs/removed-sources-2026-10-25-legco.tsv`). 57,395 was
57,529 less **134 dead HTML rows** found by its reachability sweep — the page
and its bare path 404/410 while the site answers, or the domain gone — each
with its evidence in `docs/removed-sources-2026-10-25-html-dead.tsv` (the
sweep itself: `docs/html-reachability-2026-10.md`).

**The registered count is 58,055** (2026-10-23): 58,054 plus `cyb-levelblue`,
the successor of a press feed whose host now redirects to another company —
see rule 1's note on a feed that has stopped being a feed.

**The registered count was 58,054** (2026-10-23): 58,047 plus **batch 55's
seven rows**, the Austrian Parliament's whole materials index read as seven complete
slices (`collectors/pivot/table/hp3b55_atparl.c`,
`docs/verified-sources-batch55.tsv`, `docs/at-parlament-filter-api-2026-10-23.md`).
It closes open item B2's last line, and the answer to "one measured sort
parameter away" was **there is no sort parameter**, with the row shipping anyway.
Four things in it generalise:

* **Probe a CONTROL before believing a parameter works.** 26 sort spellings were
  sent in the query string and 14 "moved" the first page — which is what the
  2026-10-21 triage reported as "six appeared to work". Then `frobnicate=7` and
  `sortColumn=99999` moved it exactly the same way, and no reading was monotonic
  in the column it named. The cause: **this endpoint ignores its query string
  entirely** — the filter is a POST JSON body (`{"GP":["XXVII"]}` answers 76,457
  where `?GP=XXVII` answers the unfiltered 391,354) — so every sort spelling was
  discarded before the server looked at it. An unknown key in the BODY is
  silently ignored too, so the same control was needed on both sides.
* **A complete single-request read beats a sort.** `page` really pages (page
  19,568 returns the last 14 rows) and really drifts: page 1 and page 2 of ONE
  filtered query overlap by **8 of 20 rows**, and two reads of `page=1` return
  disjoint sets. But `showAll=true` plus a filter answers `pages: 1` with
  `rows == count`, byte-identical across two reads — so the row is a PARTITION
  and no ordering is involved, the ENA `limit=0` shape again.
* **Enumerate the dimension you slice on, then check the slices SUM to the
  whole.** `GP` is the legislative period; enumerating I..XXVIII gives 24
  non-empty codes summing to 389,139 against 391,354 — **2,215 rows no Roman
  numeral reaches**. Sampling 30 pages for the missing codes found `KN` (1,897)
  and `PN` (316) and **missed `BR` (2)**, which only a direct count found. A
  sample finds the common gaps, not the rare ones; the 27 codes then sum to
  391,354 exactly.
* **Three keys agreeing on the same wrong answer is not corroboration.**
  `WENTRY_ID` (the site's own web-item id), `HIS_URL` and `GP_CODE+ITYP+INR` all
  give 75,485 of XXVII's 76,457, which invites writing the 972 off as the
  upstream's own duplicates — and all 972 **differ**, in exactly one column,
  `NRBR`: a material that went through both chambers is served once per chamber
  under one web-item id. `GP_CODE+ITYP+INR+NRBR` is 76,457 of 76,457 and holds on
  every slice. Measured through the engine: **391,357 emitted, 391,357 stored,
  7 requests, 222 MB, zero notices.**

**The registered count was 58,047** (2026-10-23): main's 57,529 merged with this
branch's 58,045, MEASURED with `make source-count`. Third merge of main in a
day; what it cost and what each side won is in `docs/SOURCE_COUNT_LEDGER.md`.

**This branch's count was 58,045** (2026-10-22): main's 57,527 merged with this
branch's 58,055 on the **57,536** base they share, MEASURED with `make
source-count` rather than computed — and the arithmetic cross-checks it to the
row: 57,536 + this branch's 519, less main's 9 `&amp;` duplicates, less **1**
for `JO34_CKANRES_CKAN_PUBLISHING_SERVICE_GOV_UK`, which main removed and this
merge takes. That last row is the merge's one real decision: this branch had
flagged it as "could be made to work and should not be" and left it
registered, while main measured both ends — `ckan.publishing.service.gov.uk`
and `data.gov.uk` answer 59,371 and 59,370 datasets within the same hour, so
the first host IS the CKAN instance behind the second and exposes no drafts it
hides. **Keep the canonical public endpoint, not the backend**: `data.gov.uk`
is the name that survives an internal re-platform, and it is the row that was
re-pointed and measured. A flag is not a decision; main's is.

**This branch's count was 58,055** (2026-10-22): 58,052 plus the three rows
open item B2's long tail ends with, each a publisher's own bulk file standing in
for a search that is gone —
`JP_PMDA_NEW_DRUG_APPROVALS` (the annual drug-approval workbook, 95 of 95 over
four monthly sheets), `FR_AN_QUESTIONS_ECRITES` (19,165 of 19,165 written
questions out of one 50 MB ZIP) and `JP_KANPO_DAILY_CONTENTS` (the official
gazette's daily table of contents, 25 of 25 carrying 43 notice titles). The
three dead pivots they stand in for are KEPT, disclosing: PMDA's search answers
the form page to every POST constructible here, the Assemblée's search path
404s with no form on the homepage, and the gazette's search is behind a paid
member login that no `key_env` opens.

**The registered count was 58,052** (2026-10-22): 58,019 plus **batch 54's 32
rows** and `FR_AN_DEPUTY_STAFF`. Batch 54 exists because of a correction:
**batch 45's staging set was ALREADY REGISTERED** — 5,392 of its 5,643
endpoints are in the tree and 5,392 is batch 49's own row count, so batch 49
*is* that staging set, while the file was still headed "NOT generated, NOT
registered". Believing that header would have created 5,392 duplicate
endpoints; it is a record now, not a to-do list. (Batch 37's file claimed
"still unprobed" for a fortnight after it was probed — same trap, second
time.) Of the 251 genuinely absent, 203 are machinery the sweep rejects by
design, 1 is the Miyawaka casino takeover main already removed, and 48 were
real content rejected by **two over-broad alternatives in one regex, both
found by measuring**: `form` was unanchored so it matched in|form|ation,
silently dropping the whole `information` family including a 1,074-record
member-company register, and `^xo_` (written for a slider plugin) also matched
`xo_event`, an event calendar. Details:
`native/collectors/OSINT_SOURCES_BATCH_54_JPWPLEFTOVERS.md`.

**The registered count was 58,019** (2026-10-21): 58,016 plus three rows from
open item B2's re-pointing pass — the ECB workbook's LSIs sheet (2,175 banks
`HP_XLSX`'s one-sheet read would have dropped at the seam) and two Stortinget
exports the tree was not reading. Five more rows were re-pointed rather than
added. `docs/b2-repoint-emit-2026-10-21.tsv`.

**The registered count was 58,016** (2026-10-21): main's 57,536 merged with
this branch's 57,892 on the 57,415 base they share, MEASURED with `make
source-count` rather than computed. The arithmetic cross-checks it exactly and
is worth writing down, because a merge of two branches that both REMOVED rows
double-subtracts: 57,415 + this branch's 477 + main's 121 = 58,013, **+2** for
the two FAA rows restored below and **+1** because `FAA_NNUMBER_REGISTRY` was
subtracted by both sides. Both readings are kept below, each
labelled with the branch whose reading it is.

**The three `registry.faa.gov` rows: this branch removed them and main revived
two, so main is right and the removal is overturned.** This branch measured
five agents against that host, concluded it refuses any agent carrying a
parenthetical contact route, and removed all three rather than drop our contact
route — explicitly declining to present as a browser, which `httpclient.h` then
forbade. Main measured the SAME host and changed the rule instead, for an
explicit per-host list. Re-measured here on the merge, one request each:
the browser profile answers **200, 54,752 bytes, 52 data rows** carrying
N-number, serial, make/model and owner, where the engine agent answers 403;
the N-number detail page answers 200 with 66 field pairs. So the premise of
the removal ("recovering these rows would mean dropping our contact route")
was true and the CONCLUSION was not, because a third option existed that this
branch had ruled out on policy rather than on measurement. `FAA_REGISTRY` is
restored by reverse-applying its own removal, `US_FAA_AIRCRAFT_REGISTRY` comes
back in main's repaired form, and `FAA_NNUMBER_REGISTRY` stays removed for
main's reason, which is the stronger finding of the two: once the 403 lifted it
stored the page's NAVIGATION LINKS as aircraft. **A row that was failing
honestly can start answering wrongly the moment a fix lets it through — read
what it stores, not that it stores.**

**Main's count was 57,529** (2026-10-22): main's 57,527 (below)
plus this branch's two on the same 57,536 base — the four tuberculosis
windows and the Elia imbalance-price row, less the three OpenDataSoft rows
whose datasets no longer exist (the cycle-advance sweep and the ODS title
sweep, both under "Cadence policy" / `page_resume`, and in the ledger). None
of main's nine `&amp;` duplicates was a row this branch touched.

**Main's count was 57,527** (2026-10-22): 57,536 less nine rows that
duplicated another row once their URL's `&amp;` was decoded. **A literal
`&amp;` in a row URL is not always a bug to fix**: it sends `amp;type=…`,
which the server ignores, and 1,076 rows had one. Decoded and re-measured
(`docs/amp-decode-2026-10-22.tsv`), 714 store as much or more (16 of them
only once decoded), but 206 stored
LESS — a newspaper search whose real filter finds nothing, a Joomla row then
served the Atom it asked for against an RSS declaration, a CMS honouring a
small `displayCount` — and keep the `&amp;` with a marker on the row. Measure
both URLs before changing one. Ledger: `docs/SOURCE_COUNT_LEDGER.md`.

**AND WHAT A DECODE LEAVES BEHIND IS A TWIN NO DUPLICATE CHECK CAN SEE**
(2026-10-25, `docs/removed-sources-2026-10-25-twin-rows.tsv`). A
`?feed=rss2&amp;` decodes to `?feed=rss2&`, whose trailing ampersand still
distinguishes two rows to a STRING comparison — so `check_dup_endpoint`, which
groups on host + path + query textually, could not group them, and neither
could the limit-pair harvest, which requires every other parameter to match.
**19 groups, 38 rows, 19 redundant registrations**, each group inside a single
file: a generator emitting one source twice, which noticed the NAMES collided
and appended `_2` to the second id rather than concluding it had one source.
They were found by fingerprinting the READ rather than comparing names —
normalised URL + array_path + id_keys + title_keys + record_type + detail_url +
detail_key + mode + csv_delim + post_body + next_path — and 31 groups became 19
once two over-broad normalisations were corrected, a `page` that is a CONTENT
selector and a dropped fragment that is a SHEET selector. The corrections only
ever shrink the claim.

Three things the repair measured that the finding had assumed, and each one
mattered:

* **the ampersand does not "parse to nothing on the wire" — it is a 301** to
  the canonical URL on 16 of the 18 hosts, so the duplicate row cost a redirect
  hop as well as a duplicate request. The server canonicalises the character
  rather than ignoring it.
* **one pair was never the same read at all**, which is why each was measured
  instead of the family argued. `ordinefarmacistiviterbo.it` answers the plain
  spelling with 7 items and the `&` spelling with HTTP **200 and a THREE-BYTE
  body** — a UTF-8 BOM, nothing else — with no redirect. So that row fetched 3
  bytes and stored nothing on every run since it shipped: rule 1's invisible
  nothing, hidden behind a sibling reading the same site correctly. A
  per-family argument would have retired it for the wrong reason and written
  down the wrong fact.
* **and the population carrying the artifact was 24 rows across six files, not
  25 across seven.** `hp2_eu_dach`'s
  `data.bka.gv.at/.../Judikatur?Applikation=Justiz&` is the FIRST PART of a
  multi-part C string literal and does not end that URL at all; a grep for the
  bytes `&"` cannot see the concatenation and reports it as a trailing
  ampersand. **Concatenate a multi-part literal before reading its last
  character** — this is the same shape as every other miscount in this file,
  where the tool could not see its real input.

The six genuine singles keep theirs, and that is measured too: each answers
both spellings with the same status, the same byte count and the same record
count, so the artifact is harmless there as a reading rather than as the
finding's assumption. Changing a live row's URL to save nothing is how a
validated row becomes unvalidated.

Which half survives is a per-row judgement and the removal file makes it: the
suffixed `_2` id is the generator's second emission, so the base id is the one
with stored history and other rows' references. For the one group that is not
this artifact — three World Bank country rows, two generated and one curated —
the curated row survives, and nothing is lost because
`api.worldbank.org/v2/country` answers `{"pages":1,"total":296}` at that row's
own `per_page=300`: one page is the whole collection, measured through the
engine at 296 emitted / 296 stored. Rows a removal leaves behind in an OLD
database are what `tools/cleanup_stale_uids.py` is for; it reports by default
and deletes only rows whose replacement is proven to exist.

**Main's count was 57,536** (2026-10-21): main's 57,415 (below) plus
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

**Main's count was 57,415** (2026-10-21): 57,409 plus the six batch-49 rows

**This branch's count was 57,892** (2026-10-21), on main's 57,415 merged
with this branch's 57,886, less the rows each side had already counted. The
57,423 was 57,403 plus batch 52's 19 rows and the four jpcorpwp rows that a TLS
chain fault had rejected, less the three `registry.faa.gov` rows — a removal this merge
OVERTURNS, see the browser-profile paragraph below —
and seven CKAN rows come back in this merge that main had removed on a reading
taken before this branch's UA_OVERRIDE entries existed (below). The 57,403 is
the sum of three
batches authored at the same time, two of which had to be renumbered at merge
time. On main's 48,346:

* **main's batch 49, 5,393 measured rows** — batch 45's staging set registered
  (`docs/verified-sources-batch49.tsv`);
* **main's batch 50, 3,247 government and institutional RSS/Atom feeds** for
  Japan, the US, the UK, France and China
  (`collectors/sources/{jp,us,uk,fr,cn,intl}_feeds_batch50.c`,
  `docs/verified-sources-batch50.tsv`, 3,237 OK on the emit sweep, 54,800
  emitted / 54,802 stored), less six `jp_feeds_batch43/44` feeds measured dead
  and 25 rows of batches 44/47/48 whose official domains now publish gambling
  spam (below). It was authored as batch 45 and renumbered TWICE, to 49 and
  then 50;
* **this branch's batch 46 (164 ArcGIS layers) and batch 53 (290)**, batch 53
  being the rest of batch 46's own probed pool
  (`docs/verified-sources-batch53.tsv`). It was generated as 49, renumbered to
  50 when main's PR #55 took 49, and to 51 when main's PR #58 renumbered its
  own 49 to 50 — so BOTH branches moved to the same free number, and the
  add/add on `verified-sources-batch50.tsv` is the collision this rule names,
  hit for the fifth time — and for the SIXTH on this merge, main having
  claimed 51 for `cn_feeds_batch51.c` and its own
  `verified-sources-batch51.tsv`, so the ArcGIS beat was renumbered again,
  to 53, BEFORE merging;
* then a net −5 from a day spent measuring rows rather than adding them: +1 KKJ
  prefecture that had been missing, +5 Setagaya layers recovered from a rename,
  −2 IDESCA San Carlos layers measured as withdrawn, −10 behind a closed
  WordPress REST API with nothing behind it, and −2 from the batch-34/35 emit
  audit: a `data.gov.uk` row that is a PROXY in front of the
  `ckan.publishing.service.gov.uk` row beside it (same 59,370, same `help`
  URL), and an FDSN event row on a node that serves dataselect and not event;
* and +2 for two backfill siblings measured into existence, `PLOS_CORPUS` and
  `ENERGINET_PRODCONSUM_SETTLEMENT_HISTORY`.

**And the UA word filter was found TWICE, independently, on 55 hosts with no
overlap**: this branch bisected six US municipal CKAN portals plus
planelogger.com, main bisected 48 French commune, small US city and Scottish
NHS hosts. Same word in the agent, same remedy, different hosts. Two
independent measurements make it a product people deploy rather than a few
local policies, so bisect the agent before writing off any 403/444/502 row.

**The registered count is 57,415** (2026-10-21): 57,409 plus the six batch-49 rows
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

**The registered count was 48,346** (2026-10-19; 48,343 plus the three
embedding maintenance pods of the infrastructure audit): main's 22,408 — its own
`jp_feeds_batch41..44.c` beats, PR #48's paging cycle and PR #50's batch 45 of
619 WordPress collections — merged with PR #47's 47,178, less six news
feeds both sessions had added (main merged first, so its rows stay). **Batch numbers 43 and 44 now name two things**: main's
`collectors/sources/jp_feeds_batch4N.c` beats and PR #47's `hp3b4N_*`
tables with `JO4N_` ids — no file or id collides, but read the BEAT, as with
34 and 35. **And PR #47's batches 45 and 46 are 47 and 48**: main merged
its own batch 45 (PR #50, `hp3b45_jpwp*`, `verified-sources-batch45.tsv`) while
that branch waited on CI, a direct file collision, so the public-body beats
(`hp3b47_{itgov,frgov,wdgov}`, `JO47_`) and the press beats
(`hp3b48_{igo,news}`, `JO48_`) were renumbered at merge time — ids, tables,
manifests, TSVs, docs and collector keys — exactly as the batch-41 note below
predicts. The count ledger — every
batch from 37 to 48, main's 45 and batch 46 included, what seeded it, what it measured and what it taught — is
`docs/SOURCE_COUNT_LEDGER.md`; this file keeps the rules and the current number.
Both floors are re-recorded from `make source-count` after every merge, and
the number above is that reading rather than an addition — main reached
57,393/57,394 by its own removals on 2026-10-25 and the 57,536 this line once
stated was two merges stale, which is exactly why it is measured each time.

**And a THIRD branch hit the same renumbering, which is why batch 46 is the
ArcGIS beat.** PR #51 generated it as batch 45 when 44 was the highest
number in the tree; by the time it came to merge, main had merged PR #50's batch
45 AND renumbered its own 45 and 46 to 47 and 48 — so 45, 47 and 48 were all
taken and only 46 was free. `docs/verified-sources-batch45.tsv` was a direct
collision, the same file this rule's own note predicts. Renumbering BEFORE
merging main, rather than after, is what kept it to one pass: the merge then had
no both-added file to resolve. Three sessions have now been renumbered by this,
so treat the check as a merge-time step and not a generation-time one.

The lessons from batches 43–48 that a new batch must not relearn:

* **Ask the endpoint what it SERVES, rather than probing the paths you can
  name.** Batch 42 found this with WordPress route tables and batch 46 repeated
  it on a different family: the tree already reached 195 ArcGIS service roots,
  each by ONE row naming ONE layer, and asking each root for its own directory
  (`?f=json`) found thousands of services — one root alone published 899. The
  method generalises; the family does not matter.
* **Attribution keyed on a HOST is wrong wherever the host is multi-tenant.**
  `services1.arcgis.com` is ArcGIS Online's front for hundreds of unrelated
  organisations, told apart only by a 32-character org id in the PATH, so batch
  46's generator named a Wyoming EMS layer after "Wakayama City traffic accident
  record points". Live endpoint, real records, agreeing counts, false name — and
  no record-counting gate can see it. Key on the whole root, and where the
  publisher cannot be established, name the HOST (a fact about where the data
  came from) rather than guessing whose it is.
* **Classify on what the publisher named the collection.** Inferring `category`
  from COLUMN names filed a bridge layer carrying `FLOODPRONE` under `safety`
  and a power-outage layer under `health`. A column is an attribute of one
  record and says nothing about the whole, and a category is user-visible
  wherever sources are grouped. A guess read off a column is not a measurement.
* **An ArcGIS layer's `objectIdField` and `maxRecordCount` are its own**, not
  `OBJECTID` and not a constant: several batch-46 layers key on `FID`, and the
  published page size ranges from 1,000 to 16,000. `resultOffset` is a record
  OFFSET so `page_size` belongs on it, and a cycling layer needs
  `orderByFields=<oid> ASC` because ArcGIS promises no stable ordering without
  one. **`returnCentroid` works only on POLYGON layers** — ArcGIS says so with
  HTTP *200* and a 216-byte error body, so 18 polyline rows came back
  `PATH_UNRESOLVED` from a rule tested on four layers that happened to all be
  polygons. Polygon rings cost 64x the payload (125 KB -> 8.0 MB per 200
  records) and a centroid 1.7% of that; polyline paths cost 1.3-9.3x, so they
  keep full geometry. Measure per geometry type.

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
  their beats. **"Never rebuild the binary while an audit runs" is now
  ENFORCED rather than remembered**: `audit_registry_emit.py` copies the binary
  at startup and runs the copy (`--no-pin` opts out). It was a discipline
  nobody can hold for the two hours a 290-row sweep takes — a `make` in another
  shell replaces the file underneath it and every remaining row comes back
  shell `rc=126`, reported as `NO_RUN_LINE`, "the process died before the run
  returned". Measured 2026-10-20: **229 of 290 batch-53 rows read that way**
  from one unrelated rebuild made in the middle, and the output looks exactly
  like a batch of broken sources.

  **And when the GATE and the ENGINE disagree, the upstream can be the answer.**
  Batch 51's audit called `JO53_AGS_GIS_COASTAL_COVERAGE_AREA` `EMITS_NOTHING`
  while a direct `--run` stored 371 of 371 — the dangerous direction, since a
  gate that contradicts the engine is how a working row gets "fixed".
  Reproducing the audit's exact invocation (its `ulimit`, its env, its scratch
  database) showed the cause was neither tool: `gis.charttools.noaa.gov` FLAPS,
  answering HTTP **200** carrying `code=500 "Could not find service. Service
  may be stopped or it may not be configured."` It served 371 twice and that
  twice, inside ten minutes. **Four readings, two outcomes, one endpoint** — so
  reproduce the TOOL's invocation before believing either of them about the
  row, and a single verdict is still not a diagnosis.

  The other 289 of that batch account for themselves in
  `docs/verified-sources-batch53.tsv`: 259 store exactly their upstream count,
  25 cycle because their layer exceeds one cycle's 40,000, and 5 store slightly
  MORE because the discovery-pass count is a day stale and the layers grew
  (`returnCountOnly` re-asked agrees with the engine exactly). **1,706,171
  records stored in one pass** against 25,132,703 the layers declare.

  One of those 259 is a reminder that **`SLOW` means unmeasured, not failed**:
  it had stored 22,000 of 35,204 when the tool's 200s default kill line hit,
  and at 600s it stores 35,204 of 35,204 in 298.8s. A kill line is a property
  of the tool.

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
`docs/rejected-sources-batch37.<beat>.tsv`). **It has since been probed and
generated** — this file said "still not generated into C, still unprobed" for
a fortnight after it was, which would have cost a session a re-probe of a
settled set. Commit `5d5e1f99`: **15 of the 80 pass**, 16 rows measured into
`collectors/pivot/table/hp3b37_{eu,fr,jp,row,us}.c`, 70 rejected into the
per-beat TSVs. Before picking `<N>`:
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

**AND IT HAPPENED AGAIN TO 87 ROWS, THROUGH THE OPPOSITE MISTAKE: THEY SAID
`csv_delim = "comma"`** (2026-10-24). The resolver named `tab`, `pipe`, `semi`,
`ws` and `lit:<token>` and took the **first character** of anything else — so
`comma` meant **`c`**. The Japanese crime and accident files these rows read
contain no ASCII `c` at all, so every line became ONE FIELD whose NAME was the
entire header line and whose value was the entire row: 20 columns of offence,
method, police station, municipality, date, hour, place, victim age and lock
status, in one unqueryable cell, with a correct record count on every run.
77 rows in `hp3b25_jppolice.c`, 8 in `hp3b28_jppolice.c`, 2 in
`hp3b25_jpinfra.c`.

**What makes this worth a paragraph rather than a line is that a careful hand
triage of those very rows walked straight past it.** The 2026-09-08 note at the
head of `hp3b25_jppolice.c` works through the collision verdicts, fetches four
of the files, matches the duplicate-line count to the loss exactly, and
concludes "two incidents that match on all thirteen columns are, in the
published file, the same thirteen bytes" — reasoning about thirteen columns
while the engine was reading one. It reached a true conclusion about the FILE
and never asked what the engine had made of it. The cheap check its own
predecessor paragraph prescribes — read one record's `properties` — would have
shown it in a second.

So: **`comma` is accepted now**, a single character is still taken literally,
`lit:` is still how a multi-character delimiter is declared, and a WORD that
names nothing reads as the default comma with a line on stderr rather than as
its first letter. `make lint-sources`' `csv-delim-name` fails the build on the
shape (checked by introducing one: 0 findings clean, 1 and FAILED with
`"kamma"`), and `hptest` 55 and 55b pin both halves on a body containing
neither `c` nor `k`, so either mistake would leave the whole line in one field.

Measured after, through the engine: `JO28_JCG_VESSEL_ACCIDENT_CSV` 19,370 of
19,370 with **22 named fields** a record, the Gifu bicycle-theft file 1,630 of
1,630 with **20**, and both JEPX power-market rows 9,984 and 9,953 complete
with 24 and 14. Every shape notice those rows had been filing about unmatched
title keys is gone, because the keys now match.

**The two JEPX rows then showed the next layer**: with their fields finally
readable, `id_keys = "受渡日"` is the DELIVERY DAY on a file of half-hour slots
— a dimension, kept honest only by the content hash, which means an upstream
price CORRECTION would have landed as a second row instead of updating the
first. `受渡日+時刻コード` and `年月日+時刻コード` are 9,984 of 9,984 and 9,953
of 9,953. A delimiter fix is not the end of a row's repair; it is the first
time anyone can see the row.

**AND THE SAME FILES NEEDED A SECOND, INDEPENDENT FIX: `occurrence_rows = 1`.**
Once the columns were readable the collision came back smaller and real — 1,615
of Gifu's 1,630 — because **a register of EVENTS whose columns are all
categorical publishes two different events as the same bytes.** Three bicycles
taken from one parking area in Ōgaki on 2024-05-17 at 12:00, all unlocked,
victims in their twenties: three crimes, one row in the database. Japan Coast
Guard's file has one tuple appearing **forty times** — forty pleasure-boat
drift incidents in September 2018, in harbour, mooring failure, under typhoon
conditions.

The guard's content hash is exactly right for a CATALOGUE — `sec-nga-tod`
serves one product 14 times byte for byte and storing it once is real dedupe —
and exactly wrong here. **No record count can tell a catalogue from an
occurrence register**, so the row's author declares which it is
(`hp_source.occurrence_rows`, `hptest` 54–54e). The flag keys a repeat on its
occurrence number WITHIN the file, which is a fact about the published document
and not an invented identity; the first of a group keeps the uid it already
had, so turning it on adds rows and re-keys none. 85 rows carry it — every
HP_CSV row in those two files, because the flag states what the FILE is rather
than what one sampled run showed.

**That reverses the September decision, and the argument is in the file rather
than silently applied.** Its objection was that distinguishing the rows would
mean fabricating an identity; its own last paragraph is the answer — "the
incident count is therefore the EMITTED number, not the stored one, and the run
line prints both". A run line is not what anyone queries. Rule 2 says what was
left unused is reported AS DATA, and here it was not even that: the database
held fewer crimes than the police published, with nothing in it saying so.

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

* **strict set — 0 findings across 342 files** (2026-10-20; 308 on 2026-10-07,
  256 on 2026-09-27):
  `collectors/pivot/table/hp*_*.c` plus the generated deep-record tables
  `collectors/feed/generated/hp1[0-9]_*.c`. This is the part the Makefile
  gates on, and it is held clean. Run `make audit-sources`
  after adding a table: batch 18 introduced two `single-page` findings here (a
  paged endpoint declared without `page_param`) and they had to be fixed before
  the gate would pass again.
* **the rest of the tree — 0 findings** (1,744 files scanned, 2026-10-22; 1,678
  files on 2026-10-07; it first reached zero on 2026-08-24, from ~127 across
  ~74 files). **It had drifted to 50 findings across 27 files and nobody
  noticed**, which is the exact property this entry exists to protect — and
  TWO sessions found the same 50 on the same day, independently: 49 URLs handed
  to `pw_walk_max`, `od_walk_rows_max`, `od_ckan_walk` or `od_ods_catalog_walk`
  — walkers the scanner did not name — plus the `.url` copy of each in its
  `source_def`. See the note below. Every first-only, single-page, record-cap, loop-break,
  limit-one and dedupe-ring finding has been read and closed one of three ways:
  the discard was real and was fixed, the line carries an `exhaustive-ok`
  marker with a reason, or it was a scanner false positive and is marked as
  such. The recoveries were not small — IRS exempt-orgs went from 5,000 rows to
  278,014, ESMA from 1 to 1,377, Homebrew from 300 to 23,133, and twelve
  scheduled feeds were asking their upstream for `limit=1`.

  So "zero audit findings" is now true of the whole tree, not just the strict
  set — which makes any NEW finding a regression rather than a number in a
  backlog. Keep it that way: `make audit-sources` is cheap and takes seconds.

  **And it DID drift, for thirteen days, because the scanner stopped seeing
  what the tree had learned.** On 2026-10-20 it reported 50 findings across 27
  files, every one a false positive, in two shapes:

  * `pw_walk_max` and `od_walk_rows_max` — the explicit-page-ceiling variants
    added on **2026-10-12**, in the same pass that fixed those very portals —
    never reached `_WALKER_CALL`, whose `\bpw_walk\s*\(` cannot match
    `pw_walk_max(`. Nor did `od_ckan_walk` / `od_ods_catalog_walk`, whose
    bodies hand the caller's URL straight to the walker. 29 findings. This is
    the same shape as `gen_hp_batch.py`'s forgotten opt table: **a new piece
    reached the engine and the collectors without reaching the tool that
    checks them.** When you add a walker, add it there in the same change.
  * a `source_def`'s **`.url` is the human-facing portal**, as `source.h` says
    and as rule 3 above repeats — not the request, which a bespoke collector
    names in its own `#define`. Flagging it was a category error worth 20
    findings, several in files whose request line is handed to a walker a dozen
    lines below.

  Both are fixtures now (5 and 6 in `SELFTEST`), and the fixtures were checked
  against the OLD behaviour rather than only the new: restoring it fails
  exactly the five shapes they cover. The 50th finding was `int cap = 0` in
  `lib/htmlparse.c` — a row's GROWING allocated capacity, the opposite of a
  record cap — which now carries an `exhaustive-ok` marker on its own line.

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

  **And the worst instances of it are a PROBE's limit that shipped** (2026-10-20,
  `docs/probe-sized-pages-2026-10-20.md`). Found while measuring something else:
  `us-legistar-seattle-matters` carried `$top=2` and read **40 records over 60
  requests**. Asking how many rows share the shape found **39** — `$top=1`,
  `$top=2` (17 rows), `limit=3`, `count=3`, `per_page=5`, `size=2`, `rows=3` —
  and **every one is an `hp16_*` detail-hop row**, where a tiny limit is right
  in the authoring PROBE and was carried into the row verbatim. None of them is
  in the 2026-10-11 file, which covered only the 42 candidates whose URL is a
  complete literal, so this was unexamined rather than triaged.

  Two things generalise. **A generated row inherits its probe's assumptions**,
  and a probe is written to be cheap — the same mistake as a date frozen into a
  URL at authoring time (43 rows) and batch 36's generator keying every row on
  the record's first field. **And the right page size is set by the DETAIL
  BUDGET, not by the API's ceiling**: `hp_detail_admit()` spends 1,000 hops a
  run, so a page size raised to the maximum with `page_max` left at 20 stores
  20,000 records of which 1,000 are deepened and 19,000 are stamped
  `_detail_pending` — and never deepened again, because the budget is spent in
  page order so the same first thousand get the hops every run. Each row is set
  to page size x `page_max` ~ 1,000 instead, every record stored complete.
  Nominatim's `limit=5` on a single structured address stays (a ranked geocode:
  the limit IS the question) and so does `us-sf-311`'s `page_size=5`, whose host
  429s at page 20 even under a 1,200 ms gap.

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

  **AND THE SAME SHAPE WAS STILL IN THE TREE FIVE MONTHS LATER, FOUND FROM A
  COLLISION COUNT** (2026-10-25). `geo-copernicus-odata-s2` declared no
  `$orderby`, and an OData archive's unordered default is near INSERTION order
  — so its twenty pages were an arbitrary slice of 2015 products, re-stored
  every hour while 40 million accumulated behind them. The only symptom was
  2,000 emitted / 1,703 stored, and that is the harmless half: an unordered
  walk that serves one record twice skips another, and **the skip leaves no
  trace**. So a collision count is worth reading as a question about ORDER, not
  only about identity.
  Three things the fix measured, one request each. The endpoint VALIDATES its
  query string (`frobnicate=7` is HTTP 400), which is what makes an accepted
  `$orderby` evidence rather than a guess — batch 55's control, in the
  direction that licenses trusting a parameter. `$top` is honoured to 1,000 and
  422s at 2,000, and `@odata.nextLink` had been paging the row all along, so
  the defect was never the paging. And ASCENDING is the half that matters:
  Sentinel-2 publishes 1,915 products an hour, new products land at the TAIL of
  an ascending walk where the walk has not been yet, and a three-page read of a
  two-hour window came back **2,737 distinct of 2,737**. Descending would have
  kept the drift. The window is `{ago:25200}` — seven hours, not the row's
  declared one — because **a row's declared interval is not the cadence that
  runs**: the system-wide policy above it defaults to six, so a window sized on
  3600 drops five hours of publications on an unmodified install. With
  `$count=true` the window states its own total, so an overflow discloses
  itself instead of looking like a quiet hour. Measured: 8,620 emitted, 8,620
  stored, 9 requests, no notices, records dated yesterday and today instead of
  2015.

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

  **And a walk's FIRST page can sit on a different ordering from the rest, for
  the plainest reason there is: it did not ask.** A row declaring
  `page_param`/`page_size` makes request 1 with the URL AS WRITTEN and only
  then starts advancing the parameter — so if the URL does not bind it, page 1
  is "whatever you serve by default" while pages 2..N are offsets into
  something else. Measured 2026-10-22 on the four Lechner Hungary INSPIRE
  layers: GetFeature WITHOUT `startIndex` is not the same page as
  `startIndex=0`, and `HU_INSPIRE_CADASTRAL_PARCELS` served
  `CP.CadastralParcels.107` on BOTH page 1 and page 2 — the same feature on
  every one of four readings — reporting 1,774 emitted / 1,773 stored. One
  parcel of Hungary's cadastre served twice and one never served at all, every
  run, with a one-record collision as the only symptom. The full 18-page walk
  by hand: as shipped 1,773 distinct, with an explicit `startIndex=0` 1,774,
  with a declared `sortBy` and no `startIndex=0` 1,774. **Bind the offset in
  the row's own URL and declare the sort.**

  **AND WHERE THE UPSTREAM PUBLISHES A KEYSET CURSOR, THE ANSWER IS NOT A
  SORT AT ALL** (2026-10-25, `collectors/pivot/table/hp3_inat.c`). The three
  iNaturalist rows read COLLISION in the sweep — 375 of 4,000, 200 of 4,000,
  116 of 2,000 — and the cause was the drift this file names: page 1 read twice
  a second apart is byte-identical, and **page 1 against page 2 overlaps by 10
  of 200**, because that API takes on the order of 4,000 observations an hour
  and a descending order over a live head moves under the walk. No ordering
  fixes that: ascending would read the 2008 observations for ever, and
  descending is the point of the rows.

  iNaturalist publishes `id_below` for exactly this, and it measures clean: 200
  ids strictly descending, then `id_below` of that page's last id returning 200
  more whose maximum is one below it, **overlap 0** — with the control
  `frobnicate=7` returning the identical set, so the cursor is doing the work
  rather than any added parameter moving the answer. The rows read down it now:
  4,000 / 4,000, 4,000 / 4,000 and 2,000 / 2,000, with every record carrying
  its coordinates (`latlon_key = "location"`, 4,000 of 4,000 with a latitude).

  **`page_resume` is deliberately NOT used there**, and the reason generalises:
  a resumed keyset would march backwards through 385,217,394 observations and
  never return to the head, which is what the cycling rules already exclude for
  a row whose purpose is recency. Each run starts at the head again and walks
  twenty pages down from it. The remainder is disclosed from the envelope's own
  `total_results`.

  Three things it cost, each worth knowing:

  * **`next_path` accepted a STRING only, so a numeric cursor was invisible** —
    and an auto-increment row id is the commonest keyset cursor there is. The
    row declared `next_path = "results.$last.id"` and made ONE request, storing
    200 of a declared 385 million. This is the blindness `hptest`'s `T_NUM_ID`
    already pins for identifier FIELDS ("cJSON_IsString alone was blind to
    every one of them"), in the paging path, found the same way: by watching a
    walk that should have paged make one request. A number is accepted now
    **only with a `next_tmpl`**, because without one the value is used AS the
    next URL and a bare number is not one; an integral value prints as an
    integer, never `4.1e+08`. `hptest` 56–56d, including that control.
  * **A `.url` built from a `#define` reads to `lint_sources.py` as the bare
    host and path**, so three rows whose queries differ only by a filter looked
    like one duplicate of a bespoke collector on the same endpoint
    (`dup-endpoint`, 1 above baseline). Inlining the URLs fixed it. The lint is
    right to normalise, and a row's endpoint is worth spelling out where a
    human will read it anyway.
  * **A move must not quietly change a row's cost.** Raising the research-grade
    row to 200 a page doubled it to 205 MB and 143 s a run against an hourly
    cadence; it keeps the 100 it had, measured at 101 MB. Fixing the drift was
    the job, not re-sizing the row.

  **A sort parameter can be IGNORED, which is rule 4d wearing different
  clothes.** Two of the eleven NADA microdata catalogues — DataFirst South
  Africa and Armenia — return identical first rows for `sort_order=asc`, for
  `desc` and for a different `sort_by` entirely. Those rows had been declaring
  an ordering the server does not give for as long as they existed. Ask twice
  with opposite directions before believing a sort, and drop the parameters
  where it is ignored rather than leaving the row claiming it.

  **Asking twice is not enough where the ANSWER ITSELF drifts: send a control.**
  The Austrian Parliament's filter API (batch 55, 2026-10-23) made 14 of 26 sort
  spellings look like they worked — and `frobnicate=7` reordered the first page
  identically, because that endpoint ignores its query string entirely and its
  index has no stable ordering. A parameter no implementation could honour is
  one request, and it is the only thing that separates "this sort works" from
  "this answer moves".

  **AND A PARAMETER THAT VALIDATES IS NOT A PARAMETER THAT WORKS** (2026-10-24,
  `gov-dataeuropa-search`). An hour after using "a bad spelling answers 400, so
  this endpoint checks its query string" as the evidence that UK Parliament's
  `_sort` is real — where it held — data.europa.eu's search showed the other
  half. Its GET ignores `sort` outright; its POST form **type-checks the key and
  then discards the value**: `{"sort":"id"}` answers a 400 naming the schema
  ("input don't match type ARRAY"), which reads like proof the parameter is
  live, and `{"sort":["id+asc"]}`, `["id+desc"]` and the control
  `["frobnicate+asc"]` then all return the identical first three records. A 400
  tells you a SHAPE was rejected; only an unknown field inside a well-typed
  value tells you the ordering is honoured.

  **The cost of not asking is a record count that measures the drift, not the
  source.** That row paged on the portal's own 0-based `page`, verified in
  September as "page=0 and page=1 are 100 distinct ids" — one lucky draw. Page 0
  IS stable across two reads; **page 1 is not**. So the walk's length came to
  depend on whether a reshuffled page happened to come back byte-identical and
  trip the repeat guard, and three CONSECUTIVE runs of the unchanged row
  emitted **500, 200 and 100 records, storing 103, 102 and 101** — same binary,
  same URL, same minute. It reads one stable page now (`pw_walk_max(…, 1)`,
  100 of 100 stored, one request, two runs identical) and discloses the rest.

  Closing it found three things in shared code, each the "other copies" lesson:

  * **VJSON could not disclose a declared remainder at all.**
    `jsonlist_emit_paged`'s notice fires on OUR ceiling or a full last page with
    no cursor — and the commonest other ending, the repeat-page guard, `break`s
    out before either is set. So a row whose upstream said it held 9,079 and
    which read 100 said NOTHING. That is the same gap hpengine closed on
    2026-10-20, in the other engine. There is a third trigger now
    (`available > fetched`), with `more_pages_pending: false` because the walk
    held no next page, and `records_available_basis` beside it. **Measured
    quiet**: of 14 healthy rows sampled from the registry sweep, 13 file no
    notice and the 14th was already filing one for a page-3 HTTP 500.
  * **`lib/pagewalk.c` keeps a THIRD declared-total list** beside jsonlist's and
    hpengine's XML one, and it reached neither `result.count` (CKAN's and
    data.europa's envelope) nor `result.totalResults` (the Linked Data API's) —
    so its notice read `records_available: null` with the number in the body.
    Both spellings were added to both lists in one change.
  * **A remedy must not name a knob that cannot move**, which this file already
    says and pagewalk still did: "give this source an offset/page parameter" is
    wrong for a row that declares a one-page ceiling ON PURPOSE. A
    `page_max_declared == 1` walk now says so and points at the row's comment.

  **A CEILING CAN BE THE UPSTREAM'S, SERVED AS AN EMPTY SUCCESS — and then
  "raise page_max" is the wrong advice for 130 rows** (2026-10-24, Japan
  Search's `jps-cross` item API; the family note is in
  `collectors/pivot/table/hp3b30_jplibrary.c`). The sweep read 39 of those rows
  COLLISION, about a fifth of each row's 2,000 records, and neither half of the
  cause is a property of any row:

  * **`from + size <= 2000` is a hard bound.** `from=1999&size=1` answers
    normally; `from=2000&size=1`, `from=1995&size=10` and `from=1900&size=200`
    each answer HTTP **200 with `hit: 0` and an empty list**. So every row's
    `page_max = 4` at `size = 500` is the API's ceiling and not ours, and the
    engine CANNOT tell the difference — it stops before asking for the page
    that would have refused. hpengine's remedy no longer asserts the knob
    works: it says to request one record past the ceiling and read an empty
    success as the upstream's own window, with slicing the QUERY as the route
    to the rest.
  * **The index reorders between requests and no sort pins it.** Two readings
    of the same four pages disagreed — in one, `from=0` and `from=500` were
    disjoint; in another they shared **266 of 500**. Six sort spellings
    (`sort=id`, `-id`, `common.title`, `-common.lastUpdatedDate`, `sortBy=id`,
    `order=id`) each returned exactly what the controls `sort=frobnicate` and
    `frobnicate=7` returned. The 2,000 records hold 1,732 distinct ids and
    **1,732 distinct whole records**, so the surplus is byte-identical and
    nothing distinguishable is lost at the sink — but a record the shuffle kept
    out of all four pages was never fetched.

  So the rows are a 2,000-record sample of a larger collection, redrawn each
  run, and **`hit` had to be added to the declared-total lists** before any of
  them could say so: it is the whole match set in that API (5,409 beside a
  500-record page), singular, so it does not share bare `count`'s ambiguity. A
  row that read 2,000 of 5,409 in silence now files "2,000 of 5,409 — upstream
  declared this total" every run. **Do not re-key these rows**: `id` is the
  item id and is already right.

  **And DCAT titles are language maps, which `pick()` was built to skip.**
  `title: {"en": …, "fr": …}` is an object, so the title picker skipped it, and
  `description` is the same shape, so nothing further down the list resolved
  either: 100 of 100 records a day stored as "procurement &lt;slug&gt;", with the
  engine's own shape notice saying so and nobody reading it.
  `lang_map_pick()` reads one such object — `en`, else the first member — and
  the discriminator is deliberately narrow, because `pick()`'s whole reason for
  skipping objects is that `{"lat":…,"lon":…}` must not become a title: at
  least two members, every value a non-empty STRING, every key a bare language
  subtag (two or three lower-case letters, optional region), and `id` excluded
  by name because it is two letters. `tests/unit/test_jsonlist_title.c` pins all
  of that plus the additive guarantee, and the 14-row sample re-run after the
  change is identical row for row.

  **And read the body on a 200 before trusting it: `sortBy=id` is a BACKEND
  400** on inspire.lechnerkozpont.hu, reported inside an HTTP **200** as a PHP
  warning page naming `localhost:8085`. That host is a proxy in front of a
  local GeoServer, so every upstream error arrives as a successful fetch of an
  error page — the Archive-It shape in a new place. The feature id is not a
  sortable attribute there; the sort key has to be measured per layer, with its
  own uniqueness (`kod,area,perimeter` is 5,000 distinct triples over 5,000
  consecutive land-cover features, where `kod` alone is 20 values).

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
  generated deep-record tables `collectors/feed/generated/hp1[0-9]_*.c` — 342
  files as of 2026-10-20 (290 on 2026-10-05, 262 when this was written),
  0 findings. `--strict` had to be made repeatable to say that honestly:
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

**A SECOND HOP IS A CLAIM, AND SIXTEEN OF THEM WERE FALSE** (2026-10-25,
`docs/detail-self-hops-2026-10-25.tsv`). The open item on
`us-sf-311-open311-requests` was "measure a hostgate gap for the detail hops",
because the row's own notice reported them refused — and the prior question is
what the hop BRINGS BACK. Open311 GeoReport v2 defines ONE representation of a
service request, so `/requests/<id>.json` is its single-resource form and not a
deeper record: asked of four SF cases chosen to differ (open and closed, with
and without `media_url`, with and without `status_notes`), the list and detail
representations are the same 13 keys with identical values every time, and the
other Open311 server in the tree answers the same way on 12 keys. **A gap was
about to be measured to pace a hop that had nothing to fetch.**

**The cost is set by the per-host GAP, because the budget is TIME.**
`hp_detail_admit()` spends 1,000 hops OR 120 s, so at this host's 1,200 ms gate
120 s buys about 48 hops: 48 of 1,074 records were deepened, always the first 48
in page order because the budget is spent in that order, and the other 1,026
carried `_detail_pending` — a disclosure that there was more behind the record
when there was not. 48 of the run's 59 requests. The row is 4 requests now, and
the 1,200 ms gap STAYS because it was measured for the page walk.

**Then the other copies, and the scan cost nothing.** A hop that re-reads its
own url is visible without asking any server, so the question went to all 627
rows declaring a `detail_url`: **16 have a detail_url that IS their url.**
`make lint-sources` fails the shape now (`detail-self-hop`, with a
`detail-self-hop-ok: <reason>` marker for a row that means it, as
`page-offset-ok` has). Four things about it generalise:

* **"Your detail_url is your own endpoint" is NECESSARY for a worthless hop
  and not SUFFICIENT**, so each of the nine outside the Open311 family was
  measured, one request. Three verdicts: **SELF** (every record's
  `detail_key` equals the id already in the url — `us-ma-ocpf-report-list`
  asked for THIS LIST fifty times a run), **NO-KEY** (the key resolves on no
  record, which is not a free no-op: `hp_deepen()` spends the budget first and
  then stamps `_detail_error: list record carries no detail key`, so three
  rows disclosed a failed detail fetch 7, 9 and 53 times a run for a detail
  that does not exist), and **WRONG NAMESPACE**.
* **"It reaches another id" is not "it reaches another record."**
  `openalex-institution-detail` first measured as REACHES — 25 distinct key
  values, none the url's own — which reads like a legitimate hop. The 25
  records are the institution's `topics`, so `id` is a TOPIC id and the hop
  asked the INSTITUTIONS endpoint for it: `institutions/T10009` is HTTP 404.
  25 requests, 25 404s, 25 `_detail_error` stamps a run. The id has to belong
  to the endpoint's namespace, and only a request says whether it does.
* **The check's first predicate was OVER-BROAD and the tree said so at once:
  110 findings**, the first ones read being CKAN `package_search` beside
  `package_show?id={v}` — a correct hop in a defect bucket, which is the one
  thing this file's verdicts must not do (`POD`, `GATED`, `WINDOW_EMPTY`
  again). On a query URL the last `/`-segment is `package_search?rows=100`,
  whose `100` passed a "contains four digits" test for an identifier.
  Narrowed to identical query strings, the difference in the last PATH
  segment, and that segment digit-DENSE. Ten fixtures run before the check on
  every lint run, and **the over-broad version was re-applied to them: it gets
  3 of the 10 wrong**, so the set discriminates rather than merely passing.
* **FOURTEEN OF THE SIXTEEN ALSO CARRIED ONE ENTITY FROZEN INTO THE URL on a
  schedule** (the two exceptions are the SF and Brookline list rows), which is
  the openFDA shape this file records under rule 3, found by the same scan: the
  authoring probe's URL shipped as the row, re-reading one subject for ever and
  emitting that record's nested array (`historiskenavn`, `sideactivities`,
  `altlabel`, `topics`, `proceedingsandorder`). Eleven are pivots now —
  `{q}` in the path plus `interval = 0`, no collector relabel — and
  **declaring `id_keys` was the fix rather than polish**: none of them had one,
  and Web NDL Authorities is JSON-LD whose only top-level scalars are
  `@context`, `created`, `label`, `modified` and `uri`, so the no-`id_keys`
  fallback ends at the record's FIRST SCALAR — `@context`, identical on every
  record in the API. Every person that pivot was ever asked about would have
  collapsed onto one uid, silently. **A scheduled row reading ONE entity can
  never show that; a pivot shows it on the second question.** One of them paid
  rule 4d back for free: `reportList/16928` answers an EMPTY items array where
  a server ignoring its path parameter would have returned filer 15021's fifty
  reports.

Three not converted, and the reason is a decision rather than a measurement:
`no-brreg-underenhet-detail`, `br-cnpja-open-office` and `br-cnpjws-publica`
mine a nested array of their one frozen record, so a conversion has to settle
what the record should be AND whether a company register that answers for any
CNPJ belongs on a 24 h schedule at all. One of the nine,
`br-ibge-municipio-detalhe`, is converted on the row's own authoring evidence
and **not on a reading** — its host timed out twice from the measuring
container, which is written down rather than filed as a verdict.

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
make rsstest         # a dead feed is a failed fetch, an empty feed is not
make lint-sources    # dup ids/endpoints, quarantine-empty, snprintf guards
make source-floor    # fails if a collector stopped registering (tools/source-floor.txt)
make registry-floor  # the same against the binary's registry (tools/registry-floor.txt)
make audit-sources   # scan every collector for discard patterns
tools/ci_concurrency_gate.sh ./bin/japanosint 1.0   # the event loop stays free under load
```

That is the native job in `.github/workflows/ci.yml`; CI also runs TSan and
ASan builds and the client's `vitest` and `vite build`. Run gates with `JO_DB`
pointing at a scratch file — several of them open the default DB otherwise.

**`make unit` is ONE PARTIAL LINK now, not 84 full ones** (2026-10-23). Every
test `#include`s the .c it tests, so the link line is every object except
`main.o` and that one — and at 1,860 objects that cost **35.688 s per test**,
about fifty minutes of pure linking a suite, which is what pushed the TSan job
through its 90-minute ceiling (the duration series is in `ci.yml`).
`tests/unit/run.sh` now `ld -r`s the 1,807 objects that NO test excludes into
one relocatable object once (1.2 s) and links each test against that plus the
53 objects some test does exclude: **2.674 s**, a 13x saving, for a binary
4 KB different from the old one. The whole suite runs end to end in **494 s**.

**An `ar` archive is the wrong tool here, and it passes the test while being
wrong** — worth knowing because it is the obvious thing to reach for and it
measured 0.274 s, another 10x. Archive members are pulled only to satisfy an
undefined symbol, and this tree's collectors self-register from constructors
that nothing references, so the archive link produced a **10.5 MB binary
against 128.3 MB** and the test ran against an EMPTY REGISTRY — `[db] seeded
sources from registry (0 new … 58,047 stale pruned)` — and still printed "all
passed", because the test it was checked with needs only *some* scheduled row.
A partial link keeps every section and every `.init_array` entry, so the
registry seeds the same rows it does without this. **A 130x speedup deserves
one suspicious look at what it stopped doing**; comparing the two binaries'
SIZES is what exposed it, before any assertion did.

It keeps the duplicate-symbol check `prune_orphans` exists for, verified rather
than assumed: `ld -r` over two objects defining one function reports "multiple
definition" and exits 1, exactly as the final link does — so a stale object
that redefines a symbol still fails the build, once instead of 84 times. And
run.sh now removes each test binary after running it: at 128 MB a test, the old
script left **~10 GB** in `$TMPDIR` until the suite ended.

**AND LINKING WAS NOT THE WHOLE BILL: THE JOB CAME IN AT 82.8 MIN, NOT THE ~45
THAT PREDICTED** (2026-10-24, CI run 554, all six jobs green). `ci.yml`'s own
note had set the test — "if a run still comes in over ~70 min, the ceiling is
the wrong lever a second time" — so the next thing was to find out where the
time goes rather than to raise it again. **A GitHub job log timestamps every
line, so `run.sh`'s own `--- test_x ---` markers ARE a profile**; nothing had to
be instrumented. The gaps say:

    compile 1,860 TSan objects + the one `ld -r`      3.1 min
    88 test slots, marker to first output (the LINK)  2.7 min, median 1.6 s
    test_alert_tenancy                               21.1 min
    test_source_category                             14.8 min
    test_service_vec                                  6.9 min
    test_sourceindexapi                               3.5 min
    the other 84 tests                               33.3 min, median 31.1 s

So the partial link worked exactly as measured, under TSan too — 2.7 min of
82.8, not worth another look. What remained was per-test EXECUTION, and inside
it ONE line repeats: `[db] seeded sources from registry`. **A seed that inserts
NOTHING costs 16 s under TSan** (30 s when it inserts all 58,055), because
`db_seed_sources()` is two prepared-statement steps per source and the
sanitiser instruments all 116,110 of them. test_alert_tenancy's 21 minutes is
82 of those lines; the median test's 31 s is mostly one.

Two changes, each measured:

* **`core/db.c` stamps a fingerprint of what the seed WOULD write** — a FNV-1a
  over the five values it writes (id, name, type, category, url) in registry
  order — into `_registry_seed`, beside the row count the table had when the
  last full pass finished, and skips the pass when both still match. A warm
  `db_open` goes **0.63 s → 0.07 s** natively; a fresh one is unchanged. The
  same shape as `_fts_meta`, which already keys the FTS rebuild on a
  fingerprint of the index it would rebuild.

  What it gives up is stated rather than hoped: a `name`/`type`/`category`/
  `url` edited BY HAND outside that function is no longer reverted on the next
  boot, and `JO_DB_RESEED=1` forces the full pass for exactly that. The row
  count is the half that catches drift on the database's side — a row inserted
  or deleted by anything else makes `count(*)` disagree and the next boot does
  the full pass, prune included. The count is stamped AFTER the prune, so an
  install holding a stale row for its history still reaches the fast path.
* **`tests/unit/run.sh` seeds ONE template database and copies the file** per
  test (`tests/unit/seed_template.c`, one `db_open` and nothing else), so no
  test derives it from scratch and each one's own `db_open` then matches the
  stamp. A failure to build the template is NOT fatal: every test creates its
  own database exactly as before, which is slow and correct.

Local suite **494 s → 455 s** (the saving is small here because a native seed
is only 0.75 s; the point is the 20-30x sanitiser multiplier), 86 tests, all
passing, with **129 of the 152 opens taking the fast path** and
test_alert_tenancy taking it 82 times out of 82. The 23 that still do a full
pass are tests that open a second database of their own.
`tests/unit/test_db_seed.c` pins the fingerprint field by field, the three ways
of not matching a stamp, the stale-row case and both readings of
`JO_DB_RESEED`.

**CI then measured it: the TSan job is ~40 min, from 82.8** — 36.0, 36.9 and
45.4 over the next three green runs (2026-10-24), and three readings rather
than one because this runner varies: `asan` on the same runs was 12.3, 10.2 and
14.4 min on work that did not change. **A single CI duration is a draw from a
distribution**, so quoting one is the same error as believing one probe. The
whole workflow got cheaper with it, because `build-and-test` runs the same
suite without the sanitiser: **34.7 min → ~10.5 min**. Both levers were found
by reading the job's own timestamps rather than by guessing, and the first (the
link) would have looked like the whole answer if the second had not been
measured against it.

**The next driver is already named, so nobody has to find it twice**:
`test_source_category` spends 765 s on a 15-nearest vote over 42,255 declared
sources and `test_service_vec` indexes all 58,055 source cards five times at
~65 s each. Those are O(registry) BY DESIGN, so making them cheap is a question
about what they should cover — not a build fix.

One trap met on the way, and it is the one `prune_orphans` exists for: linking
a single test by hand with `find obj -name '*.o'` picked up four stale
`hp3b4*_arcgislayers.o` objects whose `.c` files no longer exist, and the
binary **segfaulted in the registration constructors** after printing
`[hp] NOT REGISTERED … '(no id)'` for rows that read as garbage. Nothing was
wrong with the tree; the object list was. Build a one-off test binary through
run.sh's own pruning, not through a bare `find`.

**A macOS job the runner cancels at 15 minutes is the runner's, not the
PR's** (seen 2026-10-21 on `main`'s own run for PR #57 and on two PRs the
same day; the owner's rule of 2026-10-22): when the other five jobs are green,
the same cancellation shows on `main`'s own run and the native change was
macOS-green on a previous head of the PR, merge and say so in one comment on
the PR. Never an empty commit to re-trigger it; a real code change may.

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
  (`hp3b25_jpmedia`), not `osint`. Every source has a collector; 593 keys
  (2026-10-20; 554 at round 4 on 2026-10-19); `core/collectors.def` labels and groups them (below).
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
* **The table name is the wrong collector for a REGISTER** (2026-10-24). Half
  the registry read as Government because three tables were one collector
  each: `hp3b47_frgov` 9,619 rows, `hp3b47_itgov` 5,716, `hp3b48_news` 3,992,
  every row `category = "government"` / `record_type = "government-notice"`
  (or `news`). The rows themselves said otherwise — 6,450 "Mairie - X", 1,163
  CCAS and care services, 2,725 Italian state schools and 821 professional
  orders filed as public administration, 1,096 US newspapers in one world
  bucket. Every row of the three tables now names its collector explicitly
  (`.collector`, the field the default fills) and the category of its body
  type, derived from the body's NAME by ordered regex rules in
  `tools/split_beats_by_body_type.py` (`plan` → `apply` → `manifest`): France
  by the type prefix the service-public.fr directory writes first, Italy by the
  name plus the IPA category in parentheses, the press by the `(CC)` country
  code into seven regions. 29 collectors replace 3 — `frgov_mairies` 6,450,
  `frgov_social` 1,163, `itgov_scuole` 2,725 (education, under Culture),
  `itgov_ordini` 821 (Legal), `news_europe_west` 1,251, `news_northam` 1,146 …
  — and a row no rule types goes to an honest residue (`itgov_altri`, 181),
  never a guessed type. Measured over the census: Government 29,746 → 24,222
  (51.7% → 42.1%), Culture 751 → 3,650, Legal 980 → 1,931, Health 2,542 →
  2,987, Infrastructure 633 → 1,096; 621 collectors, 0 unassigned; the id set
  is byte-identical to main's (57,529 both ways), no URL or key changed, so no
  emit measurement is invalidated. Both batch notes carry the per-collector
  table. **When a beat is a register of everything a country's public sector
  publishes, the file name is not a collector — read the names before
  shipping it as one.** The 118 singleton collectors are the hand-written
  one-family beats and stay: a collector is the beat a source was authored and
  probed in, and that provenance is what a singleton records.
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
* **A new beat's group goes in `tools/gen_collectors_def.py`, NOT in
  `core/collectors.def`** — the def is GENERATED, so a hand-written line there
  is overwritten by the next `make collectors-def` and meanwhile
  `collectors-def-check` still fails, because the check regenerates from the
  census and never reads the file. Hit 2026-10-22 with batch 54: three def
  lines added by hand, the build clean, the def containing the key, and the
  check still reporting `UNASSIGNED: hp3b54_jpwporg`. Two of the three keys
  passed only because KEYWORDS matched the substrings "media" and "corp"; the
  third's dominant category is `organization`, which no KEYWORD or
  CATEGORY_GROUP rule covers. The fix is an `OVERRIDES` entry (and a label) in
  the script, then `make collectors-def` and a rebuild.

* **Collectors are grouped** (2026-10-17): `core/collectors.def` is GENERATED
  by `tools/gen_collectors_def.py` from the collector census and gives every
  collector key a `parent` among 23 `grp_*` groups (government, legal,
  corporate, procurement, open data, science, health, environment, space,
  cyber, telecom, infrastructure, transport, media, social, geo, safety,
  sanctions, culture, education, surveillance, OSINT pivots, internal).
  Education is the 23rd (2026-10-24): the `education` category had filed
  Italy's 2,725 state schools under Culture while batch 49's Japanese
  education associations sat under Science, the same kind of body in two
  groups. Schools, adult education and education associations are Education;
  universities, grandes écoles and research institutes stay under Science. **A new beat
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
* **declared** — zeros saved, each row as authored.

**The compiled default is global 6 h** (`CAD_DEFAULT_GLOBAL_SEC`, the owner's
decision of 2026-10-22: "this interval system should be osint software wide").
An empty database with a silent environment runs every scheduled row every
21600 s and the pages say `source: default`; "declared" is a choice an operator
makes by saving zeros from the Sources page, not the silence of an unset knob.
A source that must be fresher is set from its own page (Settings › Collection
interval), which outranks the policy. Measured when it shipped (2026-10-22, the registry at 57,529 with 55,174
scheduled rows): 594,367 runs/day as declared against 220,696 under this
default — the figure `GET /api/sources/cadence` reports on an empty database. `JO_SCHED_INTERVAL_SEC` / `JO_SCHED_MIN_INTERVAL_SEC` /
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
native/data/tls/                   the intermediates our upstreams omit (core/catrust.c)
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

**NO LONGER TRUE AS WRITTEN — read the next paragraph before acting on this
one.** The dispatcher stopped asking about the collector label when the `osint`
collector was removed (round 5, 2026-10-20): `is_entity_pivot()` in
`core/osint_dispatch.c` is now a one-line delegation to
`registry_is_entity_pivot()`, so there is ONE rule, it DOES test for an entity
token, and the label plays no part beyond being non-empty and not starting with
`_`. `registry.c` says why in its own comment — "the collector NAME is no
longer part of either rule: hpengine rows take their table's name as collector
(2026-10-12), so `collector != "osint"` would call 1,864 live pivots dead."
**So converting a scheduled row into a pivot needs `interval = 0` and a `{q`
token, and NOT a relabel.** This text cost a session a wrong plan on
2026-10-25 (it budgeted a collector relabel for `us-sf-311-open311-request-detail`
that the code does not require), which is the whole reason the history below is
kept rather than deleted: the FAILURE it records is still real and still the
best warning in this file about a repair that lands and does nothing. What has
changed is only the mechanism.

**The DISPATCHER did not test for an entity token, and a wrong `collector`
label silently retired 16 live pivots.** `is_entity_pivot()`
(`core/osint_dispatch.c`) asked `collector == "osint" && update_interval_sec == 0`
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
| `tools/audit_url_limits.py` | the seventh discard shape — a record limit asked of the UPSTREAM in a URL, with no paging to follow it. Read, not gated: 70 narrow candidates as of 2026-10-20 (103 when written, 89 after the 2026-10-11 pass), and a limit on a ranked pivot is legitimate |
| `tools/audit_cycle_health.py` | whether a `page_resume` row can actually ADVANCE its cycle. Three outcomes, one healthy: `CYCLING` (a position was stored), `COMPLETE` (the walk ended and cleared it — the measured no-op that makes page_resume safe to apply broadly), `STUCK` (a page failed, so no position was stored and the next run re-reads the same slice; the collection past that point is unreachable). Found as the generalisation of the ENA shape. It reports the failing page's STATUS and URL rather than a bare verdict, because a 400 naming a parameter is a property of the endpoint while a 429 is one reading. Walks to the real ceiling rather than probing shallow — the failure can be at any page — so it costs ~20 requests a row. Reads the `page_resume` rows out of the C, which is the maintained copy |
| `tools/audit_page_ceiling.py` | which scheduled rows stop at the PAGE CEILING, and how many pages their own upstream says they need. Probes SHALLOW (`--probe-pages`, default 3) and then does arithmetic, because the engine's truncation notice already carries the only number that settles it: a `records_available` whose basis is "upstream declared this total". `CEILING_DECLARED` is measured (pages_needed = total / page_size, the page size derived from the probe when the row declares none); `CEILING_UNKNOWN` means the upstream publishes no total and only a full walk can size it, which is a decision to take deliberately, not a finding. Reads `collector-fetch-failure` too, so a 429 is `FETCH_FAILED` and not "this source returns nothing" |
| `tools/audit_cycle_advance.py` | which `page_resume` rows can never advance. Runs every cycling row through the real binary under **`JO_PAGE_MAX=2`** (which brakes a declared `page_max` too since 2026-10-22: `jo_page_max_for`, hptest 40f) and reads the truncation notice back: `STUCK` (page 2 refused with a 4xx that is the endpoint's — re-point the paging), `CUT` (transport/5xx/429/wall — re-run, then a hostgate gap), `ADVANCES`, `COMPLETES`, `FETCH_FAILED`, `NEEDS_ENTITY`, `SLOW`. The engine has disclosed the stuck shape per run since 2026-10-18; this is the first thing that READS the disclosures across the registry. `--from-tables collectors` takes the ids from the C, `--ids-file` for a re-run |
| `tools/audit_page_param.py` | rows whose URL already binds their own `page_param`. The engine used to APPEND (`…&pagina=1&pagina=2`) and a server binding the first occurrence then served page 1 for the whole walk — N pages emitted, one stored, `rc=0`. Fixed in `hp_url_set_param` and pinned by `hptest` "9f-bis"; the lint stays because 103 of 1,431 paged rows are that shape and their paging depends on the replacement being right |
| `tools/audit_page_two.py` | whether page TWO actually returns something different from page 1 — the one question no record count asks. Page 1 is the row's URL as written; page 2 is that URL with the row's own parameter advanced the way `hp_url_set_param` advances it. `SAME_AS_PAGE1` is THE finding (an upstream that takes a page parameter and ignores it answers every page with page 1: N pages emitted, one stored, `rc=0`), `REFUSED` carries a remedy classification, and `PAGE1_IS_ALL` is the benign majority. **`LOCAL_EGRESS` is a verdict about THIS ENVIRONMENT and never about a source** — see the note below on what the first run did to itself. Full run: `docs/page-two-2026-10-20.tsv` |
| `tools/discover_search_endpoints.py` | where a stale SEARCH path went, asked of the HOST rather than guessed: its own search form with the field names a pivot must send, its API/developer links, its declared sitemaps. Batch 42's and 46's lesson one level over. **Read, never gated, and a candidate is not a fix** — of the eight it found for B2's 44 rows that answered 200 with a plausible page, SIX ignore the query parameter (`docs/b2-path-stale-triage-2026-10-21.md`). Everything it finds goes through `--check-filter` first |
| `tools/audit_batch_emit.py` | rule 4 below: runs each MANIFEST row through the real binary and reads back `emitted N of M`. `--timeout S` moves the kill line; a run that hits it is **`SLOW`**, carrying its partial counts — unmeasured, not failed. Each run gets a scratch DB from a warm template (it used to write the developer's live DB), and it exits non-zero on UNREGISTERED, UNPARSEABLE, NO_RUN_LINE or when nothing was measured |
| `tools/audit_registry_emit.py` | rule 4 **and** 4b for the whole REGISTRY, manifest or not — `--list-sources` is the source list, so nothing registered can hide. Measures emitted *and* stored, per run, against a fresh copy of a warm template DB. `--scheduled`/`--match`/`--only`/`--ids-file`, `--jobs`, `--timeout`, TSV out, `--resume`. A MAINTENANCE POD is reported `POD` and not run (2026-10-22): a collector whose name begins with `_` writes its own tables and nothing to `intel_items` by design, so the first registry-wide sweep reported eight of them — the embedding backfill, the translator, the camera stills lane, the vector indexers — as EMITS_NOTHING, which is this tool making the very confusion that verdict exists to expose. **`GATED`** is the same correction for a key-gated row (2026-10-23): it stores one `collector-status-notice` naming the env var it needs — rule 1's gated note — and until the database read mirrored the ENGINE's definition of a notice (`collector-%-notice` plus `collector-fetch-failure`, not a copied list of three kinds) the run line counted that row as a notice while this tool counted it as a RECORD, and the verdict was `SINK_MISMATCH`: "a collector emitting under another id, or a bug", about two rows behaving exactly as designed |
| `tools/diagnose_emit_keys.py` | why a row emitted nothing, and which `title_keys`/`id_keys` fix it |
| `tools/cleanup_stale_uids.py` | the rows identity fixes left in an OLD database: a plain-key row whose qualified siblings now hold the same record (the keyqual collectors and FRA), and wayback_jp's captures from before digest+timestamp+URL. Reports by default. `--delete` removes, in one transaction, only rows whose replacement is proven to exist and that nothing references. `--self-test` runs it on a scratch DB (also from `make unit`) |
| `tools/wp_routes_sweep.py` | the WordPress method as a tool: `sweep` asks each host for its REST root (the homepage's `Link rel="https://api.w.org/"`, else `/wp-json`), its post types and one page of each collection, writing routes.tsv/probe.tsv; `manifest` turns probe.tsv into host-type beats and a rejected TSV. Hosts are normalised (`www.` dropped, one row per REST root) because `funq.jp` and `www.funq.jp` reached the same site and only `batch_exclusions.py` noticed |
| `./bin/japanosint --host-agents` | the per-host User-Agent table (`core/host_agents.def`) as the engine resolved it: host, agent kind, the agent string, and the extra headers an `HA_BROWSER` host gets. The probe reads the same `.def`, so what it prints is what both send |
| `./bin/japanosint --discover-feeds <sites> [--jobs N]` | RSS/Atom autodiscovery in C (`core/feeddisc.c`, `test_feeddisc`): reads each homepage's own `<link rel="alternate">` declarations (relative hrefs resolve against the URL asked for, so a homepage redirecting to a squatter's domain yields nothing), skips WordPress comment feeds, decodes `&amp;`, then fetches every feed through the engine's HTTP client — same agent, per-host overrides and pacing — and prints a TSV verdict per feed: `PASS`, `TAKEOVER_SUSPECT` (gambling/pharma phrases or an unexpected script in the item titles — a question for a human, not a verdict), `REGISTERED` (any source's portal or hp row URL), or the failure. No database |
| `tools/gen_hp_batch.py` | manifest → C, one table per beat, `--prefix`/`--batch` so batches never collide — but the batch NUMBER is yours to keep unique: main shipped a batch 32 while a branch was open with its own, both created `docs/verified-sources-batch32.tsv`, and renumbering the branch to 33 after the fact meant touching the manifest, both TSVs, the table, its array name and two batch citations that had landed in `lib/hpengine.h` and `core/hostgate.c`. Rejects duplicate opts, non-integer int opts, and an opt whose value **swallowed the next one** through a stray `\;`. **Its opt table is a THIRD place a new engine field has to be added**, and it is the one that gets forgotten: `page_resume` reached the engine and 533 rows of C without reaching here, so batch 46 — the first batch to need it — had every such row rejected as an unknown opt. `page_walk` had been missed the same way before it. When you add an `hp_source` field, add it to `STR_OPTS`/`INT_OPTS` in the same change |

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
with ONE named top-level field. **`VJSON_IDKEYS(..., IDKEYS)`** takes a
COMPOSITE, with the same `+` semantics as hpengine's `id_keys`
(`collectors/sources/_vjson_idkeys.inc`, a shim sink that replaces the uid and
touches nothing else) — this paragraph used to say "there is no composite form:
hand-write a `run()`", which was true when it was written and has been false
since 2026-09-14. A stale capability claim costs a session the work of
hand-writing what a macro already does. The hand-written form is still there
for a row that needs more than an identity (`geo_tidesandcurrents_currents` at
the top of `collectors/feed/generated/vsrc_environment_3.c`: `pw_walk` +
`jsonlist_emit_ex`). Delete the `VJSON(...)` block you replace; leaving both
defines the source twice and breaks the build.

**And the precedence list finding NOTHING is the expensive case, because it
then keys on the record's first scalar.** Measured 2026-10-22 by the
registry-wide sweep: Catalonia's two climate atlases
(`eur-soc-cat-tqdx-tz5i`, `eur-soc-cat-9ehv-pmf5`) carry
`codi_municipi, nom_municipi, cap_municipi, variable, unitat, valor` and not one
name on the list, so every municipality's whole series collapsed onto the
municipality — 15,152 emitted / 4,615 stored and 3,788 / 1,265, **14,060
records a run**, rc=0 both times. The upstream was measured before the key was
touched, as a collision verdict requires: 947 municipalities (Catalonia's own
count) x 16 and x 4 variables = exactly the emitted counts, and
`codi_municipi+variable` is distinct on all of them. After:
15,152 / 15,152 and 3,788 / 3,788.

**The same sweep then found eleven more of it on one portal**, Colombia's
`datos.gov.co`, and ten are fixed for **16,545 more records a run** — 30,605
with Catalonia's two (`docs/OPEN_ITEMS_2026-10-20.md`, item E7). Three things
from that pass generalise:

* **The minimal key a search finds is not the record's identity.**
  `fechaobservacion` alone is distinct over 1,000 IDEAM weather observations and
  is the WRONG key: a station network records many stations at one instant, and
  the sample only looks unique because it is ordered by `:id`. The right key is
  `codigoestacion+codigosensor+fechaobservacion`, which is also 1,000 of 1,000.
* **A longer key is not automatically a better one.** The one row left unchanged
  is `lat-co-dispositivos-med`: its best reachable key is 961 of 1,000, no
  better than the 3.85% it already loses, and reaching 1,000 of 1,000 needs nine
  fields of which **539 of 1,000 rows are missing at least one** — a key that
  degrades for half the collection. INVIMA's device register does not expose its
  own record granularity, and writing that down is the result.
* **Measure the key over TWO pages, not one.** A key unique inside one page and
  repeating on the next is the exact shape rule 4b's comma-vs-plus note
  describes, and a single `--run` cannot see it.

**A THIRD portal family then taught the opposite lesson, from the same
symptom** (2026-10-22): 32 North American Socrata city rows reading COLLISION
for 24,536 records a run. Nineteen ship on the dataset's own documented row id
for **22,676 records a run** — 53,281 across the three families — and the three
findings that generalise are:

* **TWO PAGES CAN BE TOO FEW.** `id` is distinct on all 800 sampled Chicago
  energy-benchmarking records and collapses 4,500 of 8,000 in the engine,
  because the first 800 are all `data_year` 2019 and the dimension that breaks
  the key does not appear until page four. `id+data_year` stores 8,000 of 8,000.
  A sample that never reaches the breaking dimension reads exactly like a unique
  key.
* **THE CONTENT-HASH FALLBACK CAN BEAT A "REAL" ID.** Four of the 32 were
  changed, measured, and REVERTED because the id made it worse:
  `nam-chicago-crimes-recent` (`case_number` 6 collisions against the
  fallback's 2 — CPD files several offences under one case),
  `nam-chicago-towed-vehicles` (140 against 78),
  `nam-norfolk-streetlights-and-outages-reports` (168 against 155) and
  `nam-providenceri-providence-tree-dataset` (52 against 4). Where a record has
  no stable identity, VJSON's fallback hash of (title, link, date) separates
  rows that a weak id merges. **Compare the proposed key against the row's
  CURRENT loss and keep the fallback when it wins** — the opposite of what
  Catalonia and Colombia needed, reached from an identical starting symptom.
  Nine of the 32 keep their fallback for that reason, each with its arithmetic
  in `docs/OPEN_ITEMS_2026-10-20.md`.
* **A minimal-key search offered `shape_len` (a geometry LENGTH),
  `offense_date` (a timestamp) and `link` (a URL)** as unique keys on three of
  these datasets. Same rule as the IDEAM timestamps, three more times.

**TWO SIBLING ROWS ON ONE PORTAL, ONE NEEDING A KEY AND ONE NEEDING TO KEEP
THE FALLBACK** (2026-10-25, Colombia's SECOP I and II on `datos.gov.co`). Both
read COLLISION, and the reading that decided each is the row's CURRENT loss
rather than how good the candidate key looks:

* **`gov-co-secop1` lost 294 of 1,000 a run.** No field on a SECOP record is in
  VJSON's uid precedence list, so it fell back to hashing (title, link, date) —
  and no TITLE key matches either, so the label is the record's FIRST SCALAR,
  which is `entidad`: the contracting authority. Every procurement DANE
  publishes shared one uid. Measured over twenty pages, 1,000 records: 979
  distinct whole records and `id_del_proceso` is **979**, equal to it, so it is
  the identity and the 21 repeats are the upstream's own bytes. Keyed:
  **707 → 981 stored** (979 records plus two notices).
* **`gov-co-secop2` lost 2 of 1,000 and KEEPS the fallback.** Same portal, same
  shape, opposite answer: 1,000 records, 1,000 distinct whole, and the best
  field key is `numero_del_contrato` at **943** —
  `codigo_entidad_en_secop+numero_del_contrato` and
  `numero_de_proceso+numero_del_contrato` are also 943. Declaring the "real" id
  would lose 57 records a run where the content hash loses 2. **A contract
  number is not unique in that register and the register publishes nothing that
  is**, which is the result, and the row says so.

Two process notes from doing it, both of which nearly cost a wrong change.
`numero_del_contrato` is 250 of 250 over 250 records and 943 of 1,000 over a
thousand — **"two pages can be too few" is not a special case, it is the normal
case**. And the engine run that "proved" the first fix stored 705, two fewer
than before: there was a COMPILE ERROR in the file, so the binary was the
previous one. `VJSON_IDKEYS` lives in its own include
(`collectors/sources/_vjson_idkeys.inc`), not in `_verified_macros.inc`, and a
file using it has to pull it in — the error names a column in the macro's
argument list, which reads like a bad argument count rather than a missing
definition. **Check the build's own exit before believing a measurement taken
after it.**

**A FIELD CALLED "UNIQUE IDENTIFYING NUMBER" IS UNIQUE WITHIN A SESSION**
(2026-10-24, `collectors/pivot/table/hp3b20_parliament.c`). UK Parliament's
linked-data API gives every question a `uin`, which is what Hansard calls it
and what the row keyed on — and it RESTARTS each parliamentary session, so it
identifies a question within a session and not a record. The upstream proves it
on its own filter endpoint in one request: `commonsoralquestions.json?uin=901763`
answers `totalResults: 8`, eight different questions tabled between 2024-02-27
and 2026-10-22; `uin=HL1000` answers 11 Lords written questions. `_about` — the
resource URI the envelope gives every record — is distinct in each of those
answers and over a 10,000-record walk. **Ask the upstream for ONE value of the
key you are about to trust**; a name is not a measurement, and a dimension that
is genuinely unique per session looks unique in every sample taken from one.

Three things about it generalise past Westminster:

* **The fix already existed, in the same file, for one of seven rows.**
  `UKPARL_LDA_EDMS` carried `id_keys = "_about"` under the comment "edmNumber
  restarts every session" — written a month earlier, applied to that row, and
  not to its six siblings, four of which key on `uin`. This file's "look for
  the other copies" lesson at its smallest scale: the other copies were
  twenty lines away.
* **The symptom's SIZE depends on the page order, so it is not a measure of
  the defect.** The sweep read 1,018 of 10,000 collapsing under the upstream's
  default (newest-first) order; the ascending walk the cycle needs makes it
  **1,910**, because sorting by date groups each session's reused numbers
  together. A collision count is a signal that the key is wrong, never a
  budget of what it costs.
* **Three of the four rows read OK, and were just as broken.** Their first
  twenty pages are one chronological slice where a session's numbers do not
  repeat, so `stored == emitted` on run 1 of a cycle whose later runs would
  collapse. A `page_resume` row's key must be unique over the WHOLE
  collection; measuring run 1 cannot see that.

Measured after: 10,000 of 10,000 on all four, and 20,000 distinct over two
consecutive runs of the oral-questions row against one database with the
position advancing `_page=20` → `_page=40`.

**AND THE SAME LESSON AGAIN A DAY LATER, WITH ITS ANSWER TEN LINES AWAY**
(2026-10-25). `TX_TCEQ_NOTICES_OF_ENFORCEMENT` keyed on `noe_id`, and a NOTICE
of enforcement covers several investigations — so it identifies a notice and
not a record, exactly as a `uin` identifies a question within a session. The
upstream proves it in one request: noe_id 459,430,322,011,133.00 carries two
rows differing in `invstn_no` (920618 against 1120136) and in the docket's own
date suffix — two real investigations under one notice, not a repeat. Over the
whole 20,000-row read: 20,000 distinct WHOLE records against 19,595 distinct
noe_id, and the best composite the columns admit (`noe_id+invstn_no`) is still
three short. The fix is `:id`, Socrata's own row identifier, 20,000 of 20,000
distinct — **which the sibling `TX_TCEQ_VIOLATION_CITATIONS` ten lines below in
the same file had keyed on all along**, the `_about` case repeating at the same
tiny scale. It needs `$select=*,:id`: Socrata does not serve `:id` unless it is
asked for. Measured after: 20,000 emitted, 20,001 stored, the +1 being the
notice that discloses the 9,092 the cycle still owes.

**The opposite verdict on the same symptom, measured the same hour.**
`PUBDEV_ALL_PACKAGES` loses ~655 of 93,353 a run and its key is RIGHT:
`/api/package-names` declares exactly the 93,353 the row emits, and a registry
namespace cannot repeat a name. The cause is this file's newest-first hazard —
page 1 carries packages published today and page 900 carries 2019 — so each
package published during the four-minute walk is inserted at the HEAD and
shifts one record across every page boundary still below it; seven
publications with ~93 boundaries left each is the 655. No sort exists to pin
it, established with controls: four sort spellings AND `frobnicate=7` AND
`sort=notacolumn` each return a page byte-identical to the plain one, so the
endpoint discards its query string entirely. Re-keying would turn one
package's two sightings into two packages. **So a collision rate says nothing
about which of the two findings you have** — one key was a dimension and one
was correct, and only the upstream's own count told them apart.

**AND THE DECLARED TOTAL WAS ONE LEVEL DOWN IN THE ENVELOPE.** All seven rows
said "the upstream declares no total", which was true of what the ENGINE could
see: `lib/jsonlist.c`'s shared list carried `totalResults` at the document root
and `result.count`, not the Linked Data API's `result.totalResults`. Added
there rather than in a row, for the reason `response.numFound` is there — it is
the API family's envelope — and the rows now disclose their real remainder:
71,323 Commons oral questions, 649,660 Commons written, 107,034 Lords written,
604,935 answers, 45,235 early day motions. It also exposed a page ceiling that
had been invisible for exactly the reason this file names: **`_pageSize` is
IGNORED on the `edms` endpoint alone** (it echoes `itemsPerPage: 100` for every
value from 100 to 1,000, where `commonsoralquestions` honours 1,000), so the
row asked for 500, got 100, and twenty pages came to **2,000 — ceiling x page
size, never an upstream's own total** — against the 45,235 it now names. At 453
pages that is over the 200-page cost ceiling, so it keeps its cycle with
`page_max = 200`: 20,000 records a run and three runs to the whole series,
measured at 200 requests and 8.3 MB.

The two divisions rows are the **control** and keep `uin`: their collections
read whole (5,553 and 2,089, each equal to its own declared total) with every
uin distinct, so a division number IS a record identity. The sort parameter was
confirmed the way batch 55's was — `_sort=notacolumn` and `frobnicate=7` are
both HTTP 400, so this endpoint validates its query string and an accepted
`_sort` is being honoured rather than ignored.

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

**A UID-COLLISION of exactly one page size on a `page_walk` row is the walk
colliding with itself, not a key defect.** A walk whose URL carries no page
number fetches it bare and then asks for `page=1`, because which base the API
uses is not knowable from the URL (the probe under `page_walk` above); a
1-based server answers page 1 with the page already read, the repeat guard
recognises it and the walk goes on — but BOTH pages were emitted, so one page
size's worth of records collapse onto uids this run already wrote.
`us-sf-311-open311-requests` reported 107 collapsed across 39 pages and
**exactly 100 across 16** (measured 2026-10-24), and binding `&page=1` into the
URL took it to 14. So subtract one page size before reading a collision on such
a row, or bind the first page and remove the artifact — and do NOT widen
`id_keys` to chase it, which is the same mistake the inclusive-cursor note
makes one mechanism over.

**That shape needs THREE conditions together, and the fix is in the probe, not
in rows.** A full first page (a short page ends the walk, so no probe is ever
issued — `us-austin-apd-crime-arcgis` reads 297 of a declared 1,000 and is
therefore immune, which also means RAISING a page size below a collection's
true size can turn a safe row into an exposed one); no page number bound in the
URL; and a delay between the two requests long enough for the collection to
move, which on an hpengine row means a detail hop, because `hp_emit_record`
spends the detail budget per record INSIDE the page loop. Measured on the
registry: of 351 hp rows declaring `resultRecordCount`/`maxFeatures` with a
page cursor, only THREE also declare a `detail_url` — `CA_CALGEM_WELLS` and the
two `IMF_PORTWATCH_*` rows, all at `page_size=1000`, so the false collapse
would have been 1,000 records rather than 100. Their retained `fetch_log` shows
it never fired: every recorded run has `stored` at or just above
`records_fetched` (the surplus is notice uids). `log-retention` prunes that
table, so absence there is not proof of never — but it is the only evidence
available and it points one way.
The remedy is `jsonlist_page_fps()`: the probe's verdict is settled by record
IDENTITY — each record's own content hash, as a SET — because a reorder cannot
move a set, while the ordered 8-record `jsonlist_page_fp()` keeps its original
and correct job of asking "has the server re-served the page we just read".
And the re-served page is NOT skipped wholesale: a record that arrived between
the two requests is on that response and on nothing read so far, so dropping
the page would discard it. The probe emits the complement. `hptest` 28c (pure
reorder), 28d (a new record at the head of the re-served page) and 31e (the
same in `jsonlist_emit_paged`, because a shared decision needs both copies
fixed) pin all three halves.

**What is left after that subtraction is told apart by the run's DURATION, not
by the count.** The same row read 98 distinct of 100 in 26 s and 66 of 100 over
164 s: an index with no stable order reshuffles under a slower walk, so a key
defect gives the same number twice and a reshuffle does not. The `secs` column
in `audit_registry_emit.py`'s output therefore separates "the upstream
reordered under us" from "this row's identity is wrong" with no sort probe at
all — and it is the reason a fast probe cannot certify a slow walk. When the
answer is the reshuffle, check that a sort is actually available before
declaring it fixed: **a 200 is not evidence a sort landed.**
`mobile311.sfgov.org` accepts `sort`, `order` and `order_by`, answers 200 to
each, and honours none.

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
* **and a THIRD of the same family, at the transport layer**: Python's
  `ssl.create_default_context()` refuses handshakes the ENGINE completes.
  `yamaguchi-opendata.jp`, which carries 24 registered CKAN rows, answers
  `TLSV1_ALERT_INSUFFICIENT_SECURITY` to the default context and **HTTP 200 at
  `DEFAULT@SECLEVEL=1` with verification still on**, while the engine stores
  102 records from it across 3 requests. So the host is alive and only the tool
  could not see it. **`tools/jo_tls.py` is now THE context** for this
  directory, for rule 4c's reason — two copies of a thing diverge — and it
  ASSERTS that verification and hostname checking stay on, because lowering a
  cipher security level is a different act from skipping verification and must
  not become it.

  The historical cost of that strictness was then measured rather than assumed:
  eight batch-reject files name a TLS refusal, 51 candidates between them, and
  all 51 were re-asked (`docs/tls-rejects-revisited-2026-10-20.tsv`). **21
  alive, 30 still failing — and the cost to the registry is ZERO**: 20 of the
  21 are the `kkj.go.jp` corpus this session re-registered properly anyway, the
  21st serves five posts titled "Sample post5" from a demo WordPress install,
  and the 30 fail certificate verification for the engine too (measured on
  six: IODA x3, Dataverse UCLA, NADA Nepal, NI MARENA, each `rc=-1` with a
  fetch-failure record). Worth both numbers: the defect was real and was
  mislabelling 20 live registered rows as dead, and it had cost no source its
  place.

  **And that "30 still failing" was wrong about 24 of them, because it read one
  MESSAGE as one cause** (2026-10-21,
  `docs/tls-intermediates-2026-10-21.md`). `unable to get local issuer
  certificate` does not say the certificate is untrustworthy; it says no client
  here could find the certificate that SIGNED it. A public CA signs a leaf with
  an intermediate and the server is supposed to send that intermediate too —
  nine hosts send only the leaf, and neither OpenSSL nor libcurl will go and
  fetch the missing link, which is why a browser accepts the same page. The
  link is public and the leaf itself names it, in its AIA `CA Issuers`
  extension. Seven distinct intermediates, each verified against the SYSTEM
  root store before it was kept, now live in
  **`native/data/tls/extra-intermediates.pem`** with their provenance, loaded by the
  engine (`core/catrust.c`, through `CURLOPT_SSL_CTX_FUNCTION`) and by the
  tools (`tools/jo_tls.py`). Measured through `http_request()` itself: **nine
  of nine hosts go from `rc=1 status=0` to HTTP 200**, and the 24 candidates on
  them re-probe 24 of 24 PASS.

  **It cannot widen trust, and that is structural rather than promised.** Every
  entry is an intermediate — signed by another CA, never self-signed — and
  OpenSSL accepts a chain only when it ends at a self-signed certificate that
  was already trusted (`X509_V_FLAG_PARTIAL_CHAIN` is not set). So an entry can
  COMPLETE a chain and never ANCHOR one. `tests/unit/test_catrust.c` fails the
  build on a self-signed, expired or unchainable entry, checks the
  self-signed discriminator against 50 real roots so it cannot pass vacuously,
  and all three refusals were confirmed by hand rather than assumed.

  So: **read the TLS error, not the word "TLS".** "No local issuer",
  "expired", "hostname mismatch" and "self-signed in chain" are four findings
  with four remedies, and only one is about a certificate we should not trust.
  Of the original thirty, 24 were a missing intermediate, 2 re-point to another
  name (`www.ip.kyusan-u.ac.jp` → `www.kyusan-u.ac.jp`; `matrix.kde.org`
  delegates to `kde.modular.im`), 1 was this container's own egress, 1 was the
  already-refuted Yamaguchi 409, and **2 are genuinely unreachable** — an SNI
  mismatch at `dataverse.openscholar.org` and `nesstar.statssa.gov.za`, which
  only not checking the hostname would reach.

  **BUT CHECK WHOSE CERTIFICATE YOU ARE LOOKING AT FIRST, BECAUSE IN A PROXIED
  CONTAINER IT IS NEVER THE UPSTREAM'S** (2026-10-25). The AIA method above —
  read the leaf's `CA Issuers` extension, fetch the missing intermediate,
  verify it against the system root store — assumes the leaf on the wire is the
  publisher's. Here it is not. Measured with `openssl s_client` against six
  hosts including three that work perfectly (`github.com`, `laws.e-gov.go.jp`,
  `catalogue.dataspace.copernicus.eu`): **every one presents a certificate
  issued by `O = Anthropic, CN = Egress Gateway SDS Issuing CA (production)`**,
  and the proxy's CA is in the trust bundle (14 Anthropic entries of 148 in
  `/root/.ccr/ca-bundle.crt`, which is also why its own status reports
  `bundleCoversEveryHost: true`). TLS terminates at the egress gateway for
  every host, so the upstream's real chain is not observable from this
  container at all.
  So a `no_local_issuer` reading taken HERE is about the chain the gateway
  presented, never about the publisher's own, and following D1's remedy from
  such a reading would add intermediates that have nothing to do with the
  cause — a confident wrong fix, in the one area of this tree where a wrong
  certificate is worst. It does not retract D1: those seven intermediates were
  each verified against the system root store and took nine hosts from
  `rc=1 status=0` to HTTP 200, which is a measurement. It bounds it. The seven
  rows the complete sweep's re-read reported as `no_local_issuer` — five
  `sci-gwosc-*` on one host, plus `apdrc.soest.hawaii.edu` and
  `nhgeodata.unh.edu` — are therefore **unmeasured from here**, not diagnosed.
  Before trusting any TLS cause, run one `openssl s_client` against a host you
  know works and read the issuer.

  **THE ENGINE NOW TELLS THEM APART, which was open item D1-bis** (2026-10-23).
  The disclosure used to read "the request failed at the transport level",
  which is the paragraph above's own lesson un-learned at the one place a
  reader meets it: 24 of 30 hosts were a fixable missing intermediate and
  nothing in the record said so. `http_last_tls_cause()`
  (`core/httpclient.{c,h}`) names nine causes — `no_local_issuer`, `expired`,
  `not_yet_valid`, `hostname_mismatch`, `self_signed`, `revoked`,
  `unrecognized_name`, `handshake`, `other` — and both fetch-failure records
  (hpengine's and `core/scheduler.c`'s, since rule 1's "look for the other
  copies" applies here too) carry `tls_cause`, `tls_detail` and a cause-specific
  `remedy`. Three things about it are deliberate:

  * **the discriminator is OpenSSL's own verify result**
    (`CURLINFO_SSL_VERIFYRESULT`, an `X509_V_ERR_*` code), not the message
    text, because a reworded or translated libcurl string would silently
    reclassify every host. The text is read ONLY where there is no verify
    result — a hostname mismatch arrives that way on some backends — and
    `test_http_tls_cause.c` pins that the verify result wins when the two
    disagree;
  * **a handshake refused before any certificate was judged is not a trust
    finding** and must not be reported as one, so `CURLE_SSL_CONNECT_ERROR`
    with nothing to go on is `handshake`, whose remedy points at
    `tools/jo_tls.py`'s cipher security level rather than at a certificate;
  * **`unrecognized_name` is named apart from `handshake` although libcurl
    reports both as CURLE 35**, because the remedies are opposite. Measured
    2026-10-23: the two "genuinely unreachable" hosts above both answer
    `tlsv1 unrecognized name` — OpenSSL alert 112, the server saying it does
    not serve that hostname at all — and no cipher or trust change reaches
    that. A remedy that names a knob which cannot move is worse than none.

  The `[http]` log line now carries libcurl's own sentence
  (`CURLOPT_ERRORBUFFER`) beside the class, because `curl_easy_strerror` gives
  "SSL peer certificate … was not OK" where the buffer gives "unable to get
  local issuer certificate". The nine causes' labels and remedies were checked
  against the real messages on the wire the same day (badssl.com's expired,
  wrong-host, self-signed and untrusted-root hosts, plus the two SNI hosts).

* **`registry.faa.gov` is gone, and the measurement is why.** Three rows read
  it — `US_FAA_AIRCRAFT_REGISTRY`, `FAA_NNUMBER_REGISTRY` and `FAA_REGISTRY`
  (a bespoke collector in `collectors/sources/vehicles_world.c`) — and all
  three are removed as of 2026-10-21. One request per agent that day: the
  engine's `JO_USER_AGENT` 403, the same agent with `feed collector` stripped
  403, `curl/8.5.0` 403, **`JapanOSINT/1.0` alone 200**, no User-Agent at all
  200. So it is neither a one-word token filter nor a block on identifying
  ourselves: it refuses an agent that carries a parenthetical contact route.
  **Main's own remedy for a 403 was then tried here and refused too**: the
  `Mozilla/5.0 (compatible; JapanOSINT/1.0; +…)` header that recovered 491
  wall-blocked rows answers **403** on both faa.gov paths (measured
  2026-10-21), so this removal does not rest on one agent having been tried.
  Recovering those rows would mean dropping our contact route for that host,
  which is the one thing `core/httpclient.c`'s UA_OVERRIDE table is not for —
  and that table's own comment had already declined to list the host for a
  weaker version of the same reason. Three sources removed rather than left
  registered and failing every run.

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
* **A CONTAINER'S OTHER MEMBERS ARE RECORDS TOO: `xlsx_sheet_all = 1` and
  `zip_all_entries = 1`** (2026-10-22). One fetch buys the whole container and
  the engine was reading one thing out of it — the same shape twice, found while
  closing open item B2's long tail:
  * PMDA publishes its annual drug approvals as ONE WORKBOOK WITH ONE SHEET PER
    MONTH, and a new sheet appears every month, so a row pinned to a sheet name
    goes stale in weeks and a row pinned to index 0 reads May for ever. Neither
    failure shows in a record count. `xlsx_sheet_all` reads every sheet, each
    with its OWN header row — which matters because the two sheets of one
    workbook can have different geometry, as the ECB's do.
    `JP_PMDA_NEW_DRUG_APPROVALS` stores 95 of 95, the sum of its four sheets'
    data rows exactly.
  * The Assemblée nationale publishes its written questions as 19,165 JSON
    documents inside one 50 MB ZIP, so `zip_first_entry` was 19,164 records
    dropped — **and the engine's own `zip-extra-entries` notice had been saying
    so for as long as the row existed**, offering remedies ("a named-entry read,
    or one row per member") that do not reach 19,165 members. A notice nobody
    acts on is not a disclosure. `zip_all_entries` (HP_JSON / HP_NDJSON only,
    because concatenating CSV members would read the second header as a record)
    reads every member, one inflate at a time: `FR_AN_QUESTIONS_ECRITES` stores
    19,165 of 19,165 in one request. `lib/zipread.c`'s named and indexed lookups
    are ONE central-directory walk, not two.

  Both file a notice naming a member they could not read, and the records of
  every member land in ONE page so the collision guard sees the whole container.
  The row's `id_keys` must be unique across the whole container, not within one
  sheet or member. `hptest` 50–50e and 51–51c.

  **And looking for them found the old path lying.** HP_XLSX's single-sheet
  failure set `out = -1` under a comment calling it an honest failure, and
  `out = st.emitted` three lines below overwrites every mode driver's return —
  deliberately, so a later page's failure cannot erase the earlier pages' count.
  So a workbook that would not convert AT ALL reported `rc=0 records=0`. The
  run's own `hard_error` flag is what survives a mode driver's return value;
  `hptest` 50e fails under the old line.
* **`row_header_path` keys a JSON TABLE by name** (2026-10-21). Eduskunta's
  `avoindata` API and the Austrian Parliament's filter API both answer with an
  array of ARRAYS and put the column names in a sibling array, and the engine
  flattened such a record POSITIONALLY — so a row would key on positions whose
  meaning the upstream can change silently. It is the `csv_no_header` shape in
  JSON clothing. Name the sibling (`row_header_path = "columnNames"`) and the
  cells are named; a cell past the header's end keeps its positional name rather
  than being dropped. `hptest` 49–49f.
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
* **And `jl_declared_size()` used to consult a SECOND list that
  `jsonlist_next_page()` never walked — so "the engine knows this size" and
  "the engine can move this size" were two different facts that read as one.**
  `PAGERS[]` pairs a size spelling with a cursor and is the only list the walk
  iterates; an `EXTRA[]` list held the spellings "that have no cursor sibling
  this walk can move, because 'the page came back exactly full' is evidence of
  more data whether or not we know how to ask for it". A row whose size sat
  there made ONE request per run, forever, and disclosed the full page as
  truncation — honest, and the data past page 1 never fetched.
  `resultRecordCount`/`resultOffset` (ArcGIS REST) and
  `maxFeatures`/`startIndex` (WFS 1.x) are PAGERS families as of 2026-10-25,
  measured one URL per host first (`resultOffset=0` against `=2`, bodies then
  feature ids: disjoint on 47 hosts, overlapping on 4, ignored on 4, refused on
  5 — `docs/arcgis-resultoffset-probe-2026-10-24.tsv`), and EXTRA is empty and
  gone. **The lesson is not the two entries, it is how badly the population was
  mis-read before it was counted.** 659 scheduled rows declare one of those
  sizes, and the first pass concluded all 659 therefore read one page. They do
  not: 351 are hpengine rows that declare `page_param=resultOffset` themselves,
  300 are VGEO rows walked by `geojson_emit_paged()` — which is not
  `jsonlist_emit_paged()` and already advances `resultOffset` on ArcGIS's own
  `exceededTransferLimit` — and **three** rows reached the table. Those three
  were each reading a single page: two VJSON rows asking for
  `resultRecordCount=1` of 6,072 and of 29,208 records, and the one
  `page_walk=1` row asking for 2. So **read the row's own opts and its
  CONSTRUCT before concluding anything from its URL**: a size parameter says
  what the row asked the server for, never which engine will walk it.
  `tools/audit_url_limits.py` tells its reader to discount `--generated`
  because "the `vsrc*` fleet pages through `jsonlist_emit_paged()` at the macro
  level"; that is true for a size in PAGERS, was false for one in EXTRA, and is
  false for a VGEO row either way. **"The macro pages for it" is a claim about
  the PARAMETER and the CONSTRUCT, not about the macro.**
  Full re-derivation, both of its own defects included:
  `docs/jsonlist-nonadvanceable-page-size-2026-10-24.tsv`.
* **An offset walk needs a stable sort, and the sort field is the layer's to
  declare — across 639 ArcGIS layers the object id has TWELVE spellings.**
  `OBJECTID` 323, `ObjectId` 67, `FID` 49, `objectid` 35, `ESRI_OID` 7, `OID`
  6, `OBJECTID_1` 4, `fid` 2, plus `OBJECTID_12_13`, `OBJECTID_12`, `ObjectID`
  and `RowID`. A batch writing `orderByFields=OBJECTID` would have been wrong
  on 35% of them, and a wrong field name is an ArcGIS ERROR — it turns a row
  that reads one page into a row that reads nothing, which is worse than the
  bug being fixed. The layer publishes the answer (`<layer>?f=json` →
  `objectIdField`, `advancedQueryCapabilities.supportsPagination` /
  `supportsOrderBy`, and `maxRecordCount`, above which ArcGIS clamps silently
  so every page comes back short and a walk reads a short page as the end of
  the data). 110 layers publish no `objectIdField` key at all and **all of them
  declare one by TYPE** (a field whose `type` is `esriFieldTypeOID`), so the
  count of unsortable layers is zero — asking the second question is the
  difference between 110 "cannot be fixed" and 110 fixable.
  **And prophylaxis is not a repair.** 122 rows page by offset with no order
  clause; one row per host, four requests each, object ids compared, measured
  **ALREADY_STABLE on all 15 hosts that have a seam** (unsorted overlap 0 of
  50) with the sort accepted and honoured everywhere and **0 records
  recovered**. Adding the clause is still what this file asks for and it is
  free, but it must not be reported as data recovered. One honest limit on that
  reading: those requests completed in 10 s, and a collision on an unordered
  index grows with walk DURATION (below, rule 4b), so a fast probe is the fast
  case and cannot clear a walk that takes minutes.
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

  **`$JO_PAGE_MAX` is the operator's brake and reaches a row that declares its
  own ceiling — in both directions** (fixed 2026-10-22; it is one function,
  `jo_page_max_for(declared)` in `lib/pagewalk.c`, called by `pagewalk.c` and
  `hpengine.c` alike). It did not until that day: `hpengine.c` read
  `s->page_max > 0 ? s->page_max : jo_page_max(NULL)`, so the variable decided
  only what a row WITHOUT a ceiling got, while `hpengine.h`'s own comment said
  `page_max` was "raised or lowered for the whole tree by `$JO_PAGE_MAX`" and
  `pagewalk.c`'s `pw_page_max` implemented exactly that for its rows — **a
  capability two headers claimed and one of the two engines did not have**, the
  unification of the three ceilings having missed the DOWNWARD direction.
  It cost a whole run once (an attempt to shrink
  `JO34_FDSNEVT_SERVICE_SCEDC_CALTECH_EDU`'s declared 200 pages to 5 read
  **80 pages** before the clock stopped it, 2026-10-21) and, worse, it silently
  defeated `tools/audit_page_ceiling.py`, whose entire shallow-probe design is
  to bound a run by setting this variable: all 669 rows that cycle declare a
  `page_max`, so a probe whose flag said 3 pages would have walked up to 200
  per row. That is why the 114 SLOW verdicts of 2026-10-12 are SLOW. `hptest`
  40f pins the brake, the resume position it leaves behind, and the unset case.
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
  week per cycle against 1,440 new records a day. **And the same question
  asked of every ODS row the next day** (2026-10-22,
  `docs/measured-ods-titles-2026-10-22.tsv`: 237 datasets, 118 titles read,
  124 unanswered through this path, 3 gone and removed) caught the row
  re-pointed two days earlier: `ELIA_SYSTEM_IMBALANCE` had been moved onto
  ods134, whose own title is "Imbalance prices per quarter-hour"; the
  one-minute successor of ods045 is ods126 ("Current system imbalance
  (Historical data as of 22/05/2024)", 1,271,520 minutes, a day behind),
  and ods134 is `ELIA_IMBALANCE_PRICES_QH` now. A repair made from a
  dataset's FIELDS is still a guess about its subject; the title is one
  request. The
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
  (`hptest` 40b-iii).

  **All 530 cycling rows have now been asked that question** (2026-10-19,
  `docs/cycle-health-2026-10-19.tsv`, `tools/audit_cycle_health.py`): **318
  CYCLING, 190 COMPLETE, 12 STUCK, 6 FETCH_FAILED, 4 SLOW**. The 190 are the
  measured no-op that makes `page_resume` safe to apply broadly, now counted
  rather than sampled.

  **Read the STATUS, not the verdict.** Of the 12 STUCK, exactly ONE is stuck
  for a reason that is a property of its endpoint: `BC_MINERAL_TENURE`
  answering 400 with *"Cannot do natural order without a primary key, please
  add it or specify a manual sort over existing attributes"*. GeoServer refuses
  `startIndex` on a feature type with no primary key, so that row read **50 of
  42,234** for ever — the ascending-sort rule above, enforced by the server
  rather than by us. `sortBy=TENURE_NUMBER_ID` fixes it: 1,000 records a run
  over 20 pages, position advancing 1000 → 2000. **It was NOT applied to the
  16 other sortBy-less WFS `startIndex` rows in the tree** — they were measured
  too, and 12 of the 13 reachable ones serve page 2 fine, because GeoServer
  objects only where the type has no primary key. A blanket fix would have been
  a change made on reasoning.

  **Those 12 findings were RE-MEASURED on 2026-10-20 with the sweep paced one
  row at a time per host, and SEVENTEEN of the 22 non-healthy verdicts changed**
  (`docs/cycle-health-recheck-2026-10-20.tsv`). Six of the twelve STUCK rows
  were the sweep's own load and now read CYCLING — both FAA rows,
  `us-sf-311-open311-requests`, and two of the five `au-asic`. `audit_cycle_health.py`
  serialises per host now: `core/hostgate.c` paces requests inside ONE process
  and cannot pace twelve that each think they are alone, and a cycling row
  makes ~20 requests to one host. **A verdict that changes with `--jobs` is not
  a measurement** — the same reasoning as `audit_page_ceiling.py`'s scratch-database
  slot pool, one layer up. Only the findings needed re-reading, because load
  makes a page FAIL and so can only manufacture a false STUCK or FETCH_FAILED,
  never a false CYCLING or COMPLETE.

  **The `au-asic` family is the honest remainder.** Five rows share
  data.gov.au; host-paced, two cycle, two read FETCH_FAILED (403 on page 1) and
  one is still STUCK (403 on page 2). So those 403s are NOT purely concurrency
  — which is what the by-hand test suggested when data.gov.au served 8 rapid
  serial and then 12 concurrent requests, all 200. The difference between that
  test and these runs is the CLIENT, since the engine sends its own headers.
  That is the next thing to measure on that host, and it is written down rather
  than papered over with a hostgate entry.

  The other 11 were 429/403/500, one reading each, **and several were load the
  sweep itself created**: 12 jobs in parallel while a 6-job batch-46 emit audit
  hit `services6.arcgis.com`, where both failing FAA rows live.
  `us-sf-311-open311-requests` is already paced at 1,200 ms in
  `core/hostgate.c` and still 429'd at page 20 under that load, which says the
  same thing. The five `au-asic` 403s were re-tested by hand and are **not
  reproducible** — data.gov.au served 8 rapid serial requests and then 12
  concurrent ones, all 200 — so no pacing entry was added. A tool that adds a
  gap on an unreproducible reading is guessing.

  **A THIRD sweep manufactured its own findings, in a shape the second one's
  fix could not have caught** (2026-10-20, `docs/page-two-2026-10-20.tsv`).
  `tools/audit_page_two.py` asked page 2 of all 5,854 paged scheduled rows and
  reported **781 `PAGE1_FAILED`**. 569 of those are `Connection refused`, every
  one in the last 15% of the run and spread across wholly unrelated hosts — a
  Japanese university's WordPress, the EIB, NHTSA — and both of the two
  re-tested by hand answered HTTP 200 immediately afterwards. A socket that was
  never opened says nothing about the server at the other end.

  **And the second shape was one I first called correctly and then got wrong.**
  About a hundred more read `Connection reset by peer`, which LOOKS like the
  upstream's own refusal, so the first reading was that a reset belongs to the
  row. One command settled it the other way: `curl` reports HTTP 000 for the
  same hosts and the agent proxy names the cause itself —
  **`ws_closed_mid_exchange`**, "the tunnel to the egress proxy closed before
  the exchange completed", visible under
  `curl -sS "$HTTPS_PROXY/__agentproxy/status"`. The hosts are not denied by
  policy; each answers 200 on its own. **So in this container a peer reset can
  be OURS**, and a sweep that attributes it to the upstream writes down dead
  sources that are alive. Both errors are retried with a back-off and reported
  as `LOCAL_EGRESS` when they persist, worded so nobody can read one as a
  finding; a row that only ever comes back `LOCAL_EGRESS` is **unmeasured from
  here**, which is a thing to write down rather than a verdict to act on.

  **And that sweep's RECORD COUNTER was the array hijack this file already
  records for the engine, in a tool that never got the engine's fix.** It took
  the longest list ANYWHERE in the body, so an ArcGIS polygon's coordinate RING
  was counted as records: `EAD_PROTECTED_AREAS` holds 21 features — its own
  `returnCountOnly` says 21, with no `exceededTransferLimit` — and was counted
  as **3,748**. Its docstring excused this as "a floor, only ever compared with
  ITSELF", and both halves were false: the short-page rule compares the count
  with the page SIZE the URL asked for, and THAT comparison is what separates a
  benign `PAGE1_IS_ALL` from a `REFUSED` or `SAME_AS_PAGE1` finding, so
  over-counting turns a complete collection into a finding. It resolves the
  declared envelope first now, then the longest array of OBJECTS, like
  `hp_path`.

  The other half of the same bug FABRICATED. A body truncated at the tool's
  400 KB read cap will not parse, and the fallback `s.count("},") + 1` did not
  decline — it answered **1,232** for a CKAN page holding 48 packages whose own
  `result.count` says 48. The cap is 8 MB now and an uncountable body returns
  **-1 for UNKNOWN**, which every comparison guards on, rather than a number
  nobody can tell from a measurement. Verified after: 21, 48 and 300 where the
  tool had said 3,748, 1,232 and 1,232. **A count a tool cannot make is not a
  count it may estimate** — that is rule 1 applied to the gates themselves.

  A by-catch of the same pass: `CR_SANCARLOS_PREDIOS` had been documented dead
  since round 8 (2026-09-21) because GetCapabilities listed no `predio*` layer,
  "so the parcels were withdrawn, not renamed". That followed from **searching
  for the name we expected**. The server's 40 types include
  `geonode:Catastro_sc` — the same cadastre under its other Spanish name,
  99,222 features carrying `Propietario`. Re-pointed (plus WFS 2.0.0 for
  `count`/`startIndex`, plus `sortBy=id` for the same GeoServer refusal):
  measured **20,000 records a run** over two runs with no overlap, position
  advancing 20000 → 40000.

  **That row's real problem was not its paging flag: ENA honours NO offset at
  all**, and `{"message":"Unsupported param offset"}` is what the 400 says
  (measured 2026-10-19). There is no second page to ask for, so nothing in the
  paging section above could ever have reached the other 7,555,500 records.
  What that endpoint does honour is a DATE predicate, which is the next bullet.

* **`window_days = N` reads a collection whose only slicing axis is a DATE.**
  `page_resume` needs the upstream to page; some do not. With `window_days` the
  row carries `{wstart}` / `{wend}` tokens instead (rendering like `{date}`,
  `{wstart:%Y%m%d}` through strftime) and reads one window per run, the
  position stored through the same `resume_in`/`resume_out` as `w=<ISO date>`.
  ENA's `first_public>=A AND first_public<=B` with `limit=0` returns a window
  COMPLETE: 15,710 and 20,232 records stored on two consecutive runs, matching
  the endpoint's own `/count` for those two weeks exactly.

  **The walk goes FORWARD, from `window_from` toward today**, and that is the
  design rather than a preference — a backward walk would not see a record
  published this morning until it had finished the history, the same mistake as
  cycling a newest-first collection. Walking forward the cycle CATCHES UP and
  then pins to the trailing window, so one row is both the backfill and the live
  feed. A window that was NOT SERVED does not advance (skipping it drops its
  records permanently; re-reading one we have is cheap), which is the opposite
  of a failed PAGE clearing its position — a page can be reached again from the
  start of the same collection, a skipped window cannot.

  **A date predicate is only exhaustive where every record is dated, and that is
  the author's measurement, not something the engine can check.** ENA's
  influenza SAMPLE collection holds 61,537 records of which only 60,213 answer
  `first_public>=1990-01-01` and 0 answer `first_public<1990-01-01` — 1,324 carry
  no publication date, and no `window_days` would ever find them. That row reads
  its collection whole instead (`limit=0`, one request, 61,537 stored). **Ask the
  upstream for its own count both with and without the predicate before
  windowing a row.** `hptest` 41–41g pin the rest.

  533 rows cycle on pages, 50 on a date window. One shape to keep in mind:
  `interval = 2592000` is monthly, so a 500-page collection on that cadence
  takes about two years to come round — better than never, and the notice says
  where it is.

  **Sizing a window is a DAILY measurement, and a weekly one says the wrong
  thing in both directions.** The 47 KKJ prefecture rows are the case
  (2026-10-20). Weekly counts across eight whole prefecture-years put the peak
  week at 2.48x to 4.24x the average week, which suggested sizing every row so
  a 4.3x week fits. Measuring the three worst weeks day by day refuted it
  twice:
  * **Tokyo's peak is a SEASON, not a spike** — 978, 1,055 and 979 in three
    consecutive weeks, about 143 a weekday and nothing at weekends. So its
    rolling maxima are max3d 643, **max4d 806**, max5d 1,055: four days fit a
    1,000-record read and five do not, and the weekly ratio had been
    conservative by a third. Calibrating the burst factor on that one measured
    number (3.26) rather than on the weekly ratio took the whole set from 5,509
    windows to 4,142. Verified live at the measured peak: 806 of 806, complete.
  * **Toyama publishes in BULK** — single days of 1,449 and 1,161 notices,
    which is why its max3d and max7d are the SAME number. A record cap is per
    READ, so no window width can take such a day whole, and a narrow window
    there loses the same records at seven times the requests. **Check whether
    an overflow is a day before narrowing a window**; the notice's own
    arithmetic assumes an even spread and says so.

  A window size does not have to be perfect, and that is what makes this
  shippable on an estimate: **a window whose own count exceeds one read
  discloses itself.** The reason used to say "read its window in full" from one
  condition only — that the row was behind — so an under-sized window's
  overflow read as a tidy deferral. A deferred record comes back on a later
  run and an overflowed one never does.

  And **on a window row the upstream's count is the size of the SLICE**, not of
  the collection: `records_available_basis` said "upstream declared this total"
  there, inviting Toyama's 1,458 to be read as its 30,512. `hptest` 41h pins
  both halves.
* **`filter_query = 1` on a row the SERVER already filters discards the
  upstream's own matches.** It means "keep only records mentioning the entity",
  which is right for a bulk file filtered locally (OFAC's sdn.csv) and wrong
  the moment the URL carries `{q}` and the endpoint honours it. Measured
  2026-10-21: `US_DATA_GOV_CKAN` re-pointed at data.gov's search kept **1 of
  the 8** records the page returned, because only one dataset's link TEXT
  repeats the word. Worse on a gazette, where a notice is titled for the
  instrument and not the company — "Competition Act: approved merger —
  Barloworld SA (Pty) Ltd and Leatoy (Pty) Ltd" is a real hit for an entity
  its own slug never names, and `ZA_GOVERNMENT_GAZETTE` had carried the flag
  since it was written. Check which side filters before declaring it.

* **A row with no `.url` was dropped at REGISTRATION in silence**, and
  `index_url` is the trap: such a row still needs a `.url` (the engine replaces
  it each run with the link `index_href_must` finds). `hp_register()` skipped
  it with a bare `continue`, so two rows of one batch compiled, sat in their
  table, registered nothing and said nothing — the only symptom was `make
  registry-floor` counting one fewer source than the day before, which reads
  like somebody else's removal. It names the file, the row's index and the
  missing field now; `hptest` 48 and 48b pin that the unusable row stays out
  AND that its complete neighbour still registers, so one bad row cannot take
  a table down.

* **Run `make lint-sources` before believing a re-point is a NEW source.**
  Open item B2's triage named `data.stortinget.no/eksport/saker` as the
  replacement for a 404'd pivot; `dup-endpoint` answered at once, because
  `eur-stortinget-saker` has read exactly that URL every six hours all along
  (197 records, rc=0, measured). That would have been the `data.gov.uk` PROXY
  row again: two rows, one upstream, no new data. Asking the inverse question
  — which of a publisher's exports the tree does NOT read — found three live
  Stortinget endpoints missing, and all three are registered now.

* **An anchor COUNT is not the data, any more than a record count is.** The
  same triage recorded "288 anchors, the actual list of supervised entities"
  for the ECB page. They are site navigation (`/about/working-at-the-ecb` 23,
  `/press/other-publications` 10); the list is really a workbook renamed every
  month, over FOUR sheets (SIs 844, LSIs 2,175, Appendix 39, Changes 11), which
  is the `index_url`/`index_href_must` case and — because `HP_XLSX` reads one
  sheet — a deliberate PAIR of rows rather than 2,175 banks lost at a seam.
  Copernicus EMS was the same error: 38 anchors, all of them chrome, and the
  one feed the page declares is two news items rather than the activation
  register.

* **A spreadsheet laid out for HUMANS needs `csv_no_header = 1`, and a record
  count cannot see that it does not.** The ECB's supervised-entity workbook
  took four readings to parse (2026-10-21): the first run reported 831 records
  and `rc=0` with every field named `col6` and the workbook's TITLE BLOCK
  stored as supervised banks. Three things had to be measured, none of them
  guessable: the header is not row 1 (sheet row **12** on one sheet, **8** on
  the other) and `csv_skip_lines` counts PHYSICAL lines, so the header's own
  quoted `\r\n` — the cell reads "LEI / MFI code for branches" — makes it span
  two of them; **the two sheets of ONE workbook have different geometry** (LEI
  `col2` / name `col11` on `SIs`, `col1` / `col9` on `LSIs`), and declaring one
  sheet's positions on the other emitted 2,166 records titled by their LEI plus
  two shape notices saying 0 of 2,166 carried the declared keys; and the header
  must not be read as a header at all, because its names live in MERGED cells,
  so depending on which row the skip landed on the parser alternated between
  positional `colN` names and a field literally named `""`. `csv_no_header = 1`
  makes it deterministic. The cost is a row pinned to column POSITIONS, which a
  re-laid-out workbook would change without changing the record count — so the
  row says so.

* **A JSON TABLE needs `row_header_path`, or its row keys are POSITIONS.**
  Some APIs answer an array of ARRAYS with the column names in a sibling array
  — Finland's Eduskunta (`rowData` + `columnNames`) and the Austrian Parliament
  (`rows` + `header`, whose entries are objects carrying a `label`). The engine
  flattened such a record positionally (`.0`, `.1`, …), so a row's
  `title_keys`/`id_keys` would be positions whose meaning the upstream can
  change without anything here noticing — the hazard this file already records
  for batch 46's generator and for the ECB workbook, where two sheets of ONE
  file have different geometry. `row_header_path` names where the header is; a
  cell past its end keeps its positional name rather than being dropped, and a
  row that declares no header path behaves byte for byte as before. It is
  registered in `gen_hp_batch.py`'s `STR_OPTS` in the same change, which is the
  third place this file keeps saying gets forgotten. `hptest` 49–49f, six of
  the eight confirmed to fail without it and the two that pass being the
  controls for the additive guarantee.

  **Finland ships on it and Austria deliberately does not**, and the difference
  is one measurement. `FI_EDUSKUNTA_RECORDS` stores 1 of 1 for a real surname
  and an honest empty for an impossible one, its record carrying `personId`,
  `lastname`, `firstname` and `minister` by name. The Austrian filter API has
  **no stable ordering**: two identical requests for its unpaged first page
  returned different rows (`5,7,9,314,512`, then `1,3,4,8,2`) while `page=2`
  was stable across two reads, and of ten parameter spellings probed six
  appeared to "move" the answer with most moving it to the same place — which
  is what an unstable index looks like, not six working parameters. Its
  `showAll` is a PRESENCE and not a value (`showAll=false` behaves like `true`:
  4 pages of 100,000 at 58 MB, against 19,565 pages of 20 with the parameter
  omitted) and its `pageSize` is ignored. A 19,565-page cycle over a head that
  reshuffles is exactly the skip-with-a-collision-as-its-only-symptom shape
  this file calls the worst it records, so the row keeps failing honestly until
  a sort parameter is MEASURED.
  (`hptest` 40b-iii). Any other row in this shape says so on its next run —
  and **the sweep that reads those disclosures across the registry now
  exists** (2026-10-22, `tools/audit_cycle_advance.py`,
  `docs/measured-cycle-advance-2026-10-22.tsv`): every one of the 669
  `page_resume` rows run through the binary under `JO_PAGE_MAX=2` — the
  operator's brake, which reaches a declared ceiling too, so a run is page
  1 plus the attempt at page 2 (`jo_page_max_for`, hptest 40f) — and its
  notice read back. 600 advance, 20 complete inside two pages, 43 were
  refused page 1 from the measuring environment (39 of them on a second
  pass; ODS hosts and a few others reset this path), **2 answered 429 to
  page 2** and **4 could never advance**:
  * `BC_MINERAL_TENURE` — GeoServer refuses `startIndex` without `sortBy`,
    the very hazard noted above for BC's WFS, in a row that never got the
    sort. `sortBy=TENURE_NUMBER_ID`: startIndex 0/50/100 answer disjoint
    ascending pages, 150 of 150 across three pages in the engine, resumes.
  * the three **ENA Portal API** rows — `offset` answers HTTP 400
    "Unsupported param offset", full stop; no cycle can exist. The API's
    complete read is **`limit=0`**: ONE request, the whole result set.
    `ENA_PATHOGEN_INFLUENZA_SAMPLES` reads 61,625 of 61,625 (= its count,
    19 MB, 82 s in the engine). `ENA_PATHOGEN_SARSCOV2_RUNS` is 7.5 million
    runs (~2 GB), so it is a 30-day `first_public` window at `limit=0`
    (6,362 of 6,362 in the engine; 0 runs in the last week, 47,270 in a
    year — the trade is stated in the row). And `ENA_PATHOGEN_MTB_RUNS`
    found a third shape: **one `limit=0` read of 360,690 runs came back
    HTTP 200 with 331,580 rows — 8% short in EVERY date half**, a silent
    cut of an accession-ordered stream, not a tail. Split on `first_public`
    the same day, every block answers exactly its count (76,189 / 95,032 /
    85,283 / 57,476 / 46,710, summing to 360,690), so it is FIVE
    single-request TSV rows now: four static history blocks weekly and the
    live 90-day tail daily (10,255 of 10,255 in the engine), the 2025-on
    block ending at `{date-90d}` where the tail begins. **A 200 with fewer
    rows than the count endpoint is a cut; a window that equals its count
    is complete** — measure both before trusting a bulk read.
  * `ENERGINET_CAPACITY_MUNICIPALITY` and `TRONSCAN_TOP_ACCOUNTS` — 429 on
    page 2, so per-host gaps in `core/hostgate.c` (1,000 / 1,500 ms; both
    read three pages and resume now, Tronscan's 78 requests with its detail
    hops in 116 s). **Which found that the table's live-entry count was a
    hand-maintained `7` while it held nine** — the same `1`-for-two trap
    its own comment recorded in September — so the two new gaps were
    compiled in and never consulted; the count is sizeof arithmetic now.

  **That sweep is done: 669 cycling rows measured, SEVEN stalled, and all
  seven are fixed** (2026-10-22, `docs/measured-cycle-stalls-2026-10-22.tsv`;
  `tools/audit_page_ceiling.py` gained the `CYCLE_STALLED` verdict — a notice
  carrying `failed_page` on a `page_resume` row — and `WALK_CUT` for the same
  failed page on a row that does not cycle). 467 `CEILING_UNKNOWN`, 136
  `CEILING_DECLARED`, 15 COMPLETE, 43 `FETCH_FAILED`, 1 SLOW. **Read the
  FETCH_FAILED against the path, not the row**: the agent proxy's own
  `recentRelayFailures` listed tunnel closures for five of those hosts while
  the sweep ran.

  The seven say something the stall count does not: **a refused page 2 is
  rarely a page-2 problem.** Each needed a different fix, and five of them were
  wrong in a way no ceiling arithmetic could reach.
  * **ENA ×3** — `www.ebi.ac.uk` does not refuse `offset=1000`, it refuses
    `offset` at all: `offset=1` answers the same 400
    `{"message":"Unsupported param offset"}`. The note above was written from
    one boundary reading and generalised wrongly. **Both branches found this
    and wrote different rows**, and the merge keeps the one that reads the
    archive: a rolling 30-day `first_public` window is the live deposit tail
    and says so — its own author recorded that the 7,556,500 runs already in
    the archive are NOT backfilled by it, and that date tokens cannot express
    the backfill because they step back on failure rather than iterate. That
    is what `window_days` is: the two SARS-CoV-2 and MTB rows walk
    `first_public` FORWARD from 2020-01-01 and 2010-01-01 in 7- and 90-day
    slices, catch up, and then pin to the trailing window, so one row is both
    the backfill and the live feed. Measured both ways and kept as data:
    15,710 and 20,232 stored for two consecutive weeks (equal to ENA's own
    `/count` for those weeks), against 6,362 / 3,436 / 289 for a 30-day tail.
    The influenza row is NOT windowed in either reading, and that half is this
    file's own rule rather than a preference: 1,324 of its 61,537 samples
    carry no `first_public` at all, so a date predicate could never reach them
    — it reads whole at `limit=0`, 61,537 of 61,537. Main's `timeout_ms` is
    kept on all three: the SARS-CoV-2 query measured **38.6 s cold** against
    the engine's 20 s `HP_HTTP_TIMEOUT`, so the first fixed run timed out at
    `bytes=0` and filed a fetch-failure for a query that works. And main's
    `first_public` in the influenza row's FIELD list is kept too, for a reason
    worth generalising: a field absent from the list is absent from the stored
    record, so `date_keys` had nothing to order on.
  * **`BC_MINERAL_TENURE`** — the WFS refuses `startIndex` without `sortBy`
    ("Cannot do natural order without a primary key", the hazard this file
    already records from the other side). With `sortBy=TENURE_NUMBER_ID` and
    `count=10000`: 50 records → **42,327**, its own `numberMatched`.
  * **`ENERGINET_CAPACITY_MUNICIPALITY`** and **`TRONSCAN_TOP_ACCOUNTS`** both
    answered 429 and wanted OPPOSITE fixes. Energinet limits by request
    FREQUENCY, not size — 429 at `limit=1000` and `limit=10000`, then
    `limit=50000` served in full — so it stopped paging and reads its whole
    12,771-record dataset in one request. TronScan's cost is per RECORD (a
    detail hop per listed account, so a page of 50 is 51 requests in a burst):
    measured 2 of 10 requests 429 at the global 150 ms gap and 0 of 10 at
    1,000 ms, so it got that gap in `core/hostgate.c` and KEPT its cycle,
    because the per-run detail budget of 1,000 reaches all 10,000 accounts only
    if the slice moves.
  * **`SI_GURS_CADASTRAL_MUN`** was not broken: the 404 was transient and a
    re-read stores 2,698 / 2,698. One probe is one reading, in this direction
    too.

  **Two of the seven had no `id_keys` at all** (BC's GeoJSON features, and
  Energinet, which was keying on `Month` — a dimension every municipality in
  that month shares). A stalled cycle hides a key defect behind it, because a
  row that only ever reads one page never shows the collision.

  **Two sessions ran this sweep on the same day and merged the same evening**
  (PR #68 first, with `audit_page_ceiling.py`'s `CYCLE_STALLED`; then this
  branch's `audit_cycle_advance.py`, above). Where their fixes differed, the
  better-measured one stands: BC reads 10,000 a page in five pages (theirs),
  Energinet reads its 12,771 rows in one request (theirs), Tronscan's gap is
  the 1,000 ms measured ten-for-ten (theirs); the ENA influenza samples read
  the WHOLE set at `limit=0` (61,625 = the count, ours) and the tuberculosis
  runs read whole as five windows (ours), because a 30-day slice of a
  collection the API hands over complete in one request is a discard; the
  SARS-CoV-2 row is a 30-day window in both, at `limit=0` with their cold
  timeout. And `$JO_PAGE_MAX` now brakes a declared `page_max` (theirs,
  `jo_page_max_for`, hptest 40f), which is what this branch's
  `JO_PAGE_MAX_CAP` existed for — so that knob is gone and
  `audit_cycle_advance.py` sets `JO_PAGE_MAX=2`.

  533 rows cycled when this was written; 669 do on 2026-10-22, and the sweep
  above is how to know they still can. One shape to keep in mind: `interval = 2592000` is
  monthly, so a 500-page collection on that cadence takes about two years to
  come round — better than never, and the notice says where it is.
* **A deep-paging cap is the upstream's, and it is disclosed by name.** GLEIF
  refuses `page[number]=51` with HTTP 400 — 50 pages × 200 = 10,000 records —
  so `GLEIF_LEI_HK` reaches 10,000 of 13,419 and the rest needs a narrower
  filter per request. The notice says exactly that, including
  `failed_page_url`. Leaving `page_max` ABOVE the cap is deliberate there: the
  notice then names GLEIF's refusal rather than our own ceiling, for the price
  of one 400 a run.

  **A THIRD SPELLING: HTTP 408, AND A 408 THAT RETURNS IN 0.4 s IS NOT A
  TIMEOUT** (2026-10-25, `EBI_INTERPRO_PROTEINS_REVIEWED`). Walking
  `www.ebi.ac.uk/interpro/api/protein/reviewed/` down the API's OWN published
  `next` cursors: pages 1 to 10 each serve 500 rows in about 0.7 s, and page 11
  answers **408 in 0.4 s** — repeatably, for that cursor and every later
  attempt. So the cap is 5,000 records of the 573,015 it declares, wearing the
  status of the one thing it is not; the three spellings now on record are
  GLEIF's 400, the empty HTTP 200 of Japan Search and HUDOC, and this.

  **And `page_resume` on a capped cursor is WORSE than no cycle at all**, which
  is the half worth remembering. The engine correctly does not advance the
  position when a page fails — so run 1 stored the REFUSED cursor as its
  position, and run 2 resumed from it, read one record and cleared. The row
  would have ground over the same 5,000 for ever, and the 292-second first run
  was retries against the 408 rather than slow pages. Before giving a
  cursor-paged row a cycle, walk the cursor by hand to the ceiling: if the
  position it would store is refused, the row wants `page_max` plus the
  disclosure, not `page_resume`.
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

  **And until 2026-10-20 that authoritative signal could not fire for two
  whole classes of row.** The trigger was `page_walk && declared_total >
  available`, so a row making ONE request against an upstream that said it
  held fifty times more disclosed NOTHING; and `hp_run_json` was the only
  place a declared total was ever read, so no XML row in the tree could state
  a remainder whatever its upstream published — 27,258 scheduled XML rows with
  no paging mechanism at all. Both are fixed; `hp_xml_declared_total()` reads
  only the text BEFORE the first record, because a name like `TotalCount` can
  occur as a field OF a record and reading one record's field as the
  collection's size makes the disclosure itself a fabrication.

  Finding that first record needs the record loop's own rule that **a tag name
  is only that name when the next character ends it**: KKJ wraps
  `<SearchResult>` records in a `<SearchResults>` envelope and puts the count
  inside it, so a plain `strstr` for `"<SearchResult"` stops at the WRAPPER and
  the searchable span is the four lines above it. `hptest` 42, 42b and 42c.

* **Probe every offset spelling before concluding a row can page — or that it
  cannot.** 48 KKJ rows declared `page_param = "Start"` and walked 20 pages
  that each re-emitted page 1. `Start`, `Offset`, `Skip`, `From`, `Page`,
  `PageNo`, `Index`, `StartIndex`, `Start_Index`, `First`, `Begin` and
  `Position` were each sent with a real value and each returned the same first
  page, so the row's shape was decided by twelve requests rather than by a
  guess in either direction. **A remedy that names a knob which cannot move is
  worse than none**: "raise max_items/page_max" is right for a walk stopped at
  a ceiling and useless for a row that never walked, and a row with no paging
  is now told to ask the endpoint what it honours and to slice the QUERY where
  no offset works.

  A corollary worth the one request it costs: **count the members of a set
  that looks complete.** These rows covered 46 of Japan's 47 prefectures —
  三重県 was simply absent, and LG_Code=24 answers 5,468 notices.
* `next_path` accepts a `key=value` segment: write **`links.rel=next.href`**,
  not `links.1.href`. Indexing a hypermedia link array positionally breaks when
  a server reorders it, and the failure mode is the engine refetching page 1
  until the page ceiling — every later page lost, with the run still looking
  successful.
* **A KEY MAY CONTAIN A DOT, and a dotted-path resolver that always splits
  cannot see one.** `lib/jsonlist.c`'s `dotted()` split every path
  unconditionally, so `"@odata.nextLink"` — which sat in its next-link list,
  under a comment naming OData as a supported dialect — was looked up as
  `doc["@odata"]["nextLink"]`, which no OData server has ever published. Every
  OData v4 row in the tree stopped at page 1, with the capability claimed in a
  comment two lines above the code that could not deliver it; the same split
  hid `odata.count`, so no OData row could state its remainder either. Both
  resolvers now fall back to the literal key **after** the split fails, and
  only for a path that had a dot to split, which makes it strictly additive:
  every path that resolved before resolves to the same node. Both OData
  spellings of the total are in the declared-total list. Measured on LegCo
  Hong Kong, whose `$inlinecount=allpages` answers `odata.count: 551619`
  beside 200 rows.

  **And `hp_path` in `lib/hpengine.c` is the OTHER COPY**, fixed in the same
  change rather than left to be found later — it resolves the path a ROW
  declares, so `next_path = "@odata.nextLink"` would have split identically.
  This file already records the uid-collision guard existing in three record
  paths and being repaired in one; look for the other copies.

  `hptest` 47, 47b and 47c, all three confirmed to fail under the old
  behaviour — and 47's FIRST version passed under both, because `$top`/`$skip`
  let the cursor inference reach page 2 without the link at all. It names a
  URL with no size parameter now, so the published link is the only route
  there.
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
* **A record's `body` is text, never its JSON** — and the sink no longer
  trusts that it is. `socrata-sf-business-registrations` passed each
  licence's JSON as `body` while stamping the same "DataSF open data" /
  "San Francisco, CA" values on every record, and `core/simhash.c`
  fingerprinted all 368,735 as near-duplicates: one false cluster, and a band
  bucket each insert had to scan (23 records a second, until the body became
  the business's name, owner, address and industry; then 28 min for all of
  them, in 26,883 real clusters). 205 hand-written collectors pass the JSON
  as `body` (309 sites, 2026-10-25), so the sink now indexes and fingerprints
  a JSON body by its VALUES, the way it indexes `properties`
  (`fts_segment_body()`, core/fts_schema.c, in all five places a body is
  segmented); the stored body is unchanged. Measured on two of them, the
  JSON body alone made no false cluster — IRS exempt orgs' largest cluster
  is 267 chapters of "PTA NEW YORK CONGRESS" before and after, and New
  Zealand charities is 100 records in 100 clusters — so what the change buys
  is keys out of search and fingerprints; a collector that stamps constant
  values on every record still needs a real text body of its own.
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
**THEY HAVE NOW BEEN MEASURED ON THE PRODUCTION MODEL** (2026-10-23 — this
paragraph said "none has been measured" until then, and the values had been
chosen from typical multilingual-e5 scores and tested against a stub). A real
server was stood up for it: llama.cpp built from source and
`multilingual-e5-large-q8_0.gguf`, the file this tree's own notes name as the
production model, answering `/v1/embeddings` with 1024-d unit vectors and
WITHOUT the e5 `query:`/`passage:` prefixes, because `core/service_vec.c`
measured those worse here and defaults them off. Full readings:
`docs/measured-thresholds-2026-10-23.{md,tsv}`. Three results:

* **`story_tau = 0.85` is right, and now measured.** 200 positive pairs (two
  items sharing a simhash `cluster_id`) against 200 negatives (different
  cluster AND different source) separate COMPLETELY: positives 0.9395 at the
  10th percentile and 1.0 at the median, negatives topping out at 0.8372. Best
  F1 cut-off 0.8372 at F1 = 1.000, and the value in force achieves precision
  1.000 and recall 1.000 with headroom on both sides.
* **The UNRELATED band on this model starts at 0.69, and SIX of the eleven
  cut-offs are below it.** 300 question ↔ unrelated-record pairs across 30
  sources score **0.6874 to 0.8448** (median 0.7641) — so `relevance_sim_min`
  0.40, `suggest_min_cos` 0.50, `route_min_cos` 0.55 and `resolve_emb_min_cos`
  0.60 admit **100%** of unrelated pairs, `repair_sem_cos_floor` 0.70 admits
  99.3%, and `corpus_vec_max_distance`'s L2 ceiling of √2 admits 100% (the
  observed unrelated L2 range is 0.5572–0.7906, so √2 is twice the maximum).
  `route_min_cos` was re-measured in its OWN space — a question against a
  source CARD, not a record — and the minimum of 300 such pairs is 0.7261
  against a cut-off of 0.55, so a question with no entity routes to every
  scheduled source. `entity_vec_cos_floor` 0.80 admits 14% and
  `alert_sem_threshold` 0.82 admits 2%: those two are real filters.
* **What that does NOT license.** It says a cosine cut-off on this model must be
  **above ~0.85** before it rejects anything, because that is where the
  unrelated band ends (0.8448) and the known-same band begins (0.9395). It does
  not say what each value should be: that needs positives — pairs a human marked
  "these two SHOULD match" — which exist nowhere in this tree, and the tool
  reads them from a `--labels` file exactly so nobody invents them. **Nothing
  was changed.** `docs/threshold-labels.jsonl` is the next step and it is human
  work.

Running the harness for real also found three defects in it, the worst being
that **its negative set was EMPTY on a real database**: `pairs_story` read a
prefix `order by cluster_id` and then required two different sources, and on a
412,521-row database holding 31 sources every row in that prefix came from ONE
source — `n_neg = 0`, "ONE CLASS ONLY", after a full embedding run. A sample
taken off the head of a collection is not a sample of the collection, which is
the same error this tree recorded the same day for a record key measured on an
800-row sample. It samples per source now. Also: `in_force` was blank on every
key because nothing resolved the DEFAULT MACRO out of `core/thresholds.c`, so
the two columns that judge the value we ship could not be computed at all; and
only **six of the eleven defaults are in a header** — the other five are in
`core/thresholds.c` itself — which this paragraph used to assert the other way
round.

**The harness itself** (2026-10-22,
`tools/measure_thresholds.py`): it reads the eleven keys out of
`core/thresholds.c` rather than copying the table, asks a real server on the
same `/v1/embeddings` contract `core/llm.c` uses, and reports each key's
positive/negative score distributions, the best-F1 cut-off, and the precision
and recall the value IN FORCE achieves. It refuses to run without a real
server and says why — a stub is what produced the numbers it exists to
replace. Three keys have a label that is not circular (`story_tau` from
simhash `cluster_id`, since `story_id` was assigned BY `story_tau`;
`entity_vec_cos_floor` from `norm_key`; `camera_dup_cosine_min` from
`sha256`); the other eight judge a query against a record, no stored human
verdict exists, and they read UNLABELLED with a distribution only. No label
file is shipped on purpose: invented verdicts fed to the tool would turn the
guess into a "measurement". `resolve_emb_margin` is a GAP between a best match
and a runner-up, so it cannot be measured from pairs at all and says so.
`--self-test` runs from `make unit` and pins the one error that inverts a
reading silently — that an L2 distance is swept the opposite way to a cosine.
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
  **ffmpeg's own sockets are boxed too** (2026-10-22, `core/netjail.{c,h}`):
  every network run of ffmpeg/ffprobe goes through a per-child forward proxy
  (http/https/hls) or a pinned relay (rtsp/rtsps) that resolves each name once,
  judges every answer under the camera policy and connects to what it judged —
  so a live playlist's later segments and a re-resolved name are judged, not
  just the first URL. On Linux the child also runs in its own network
  namespace (a user namespace when not root) whose only listener is that
  proxy, so an RTSP redirect or a `tcp://` segment has no route at all. Where
  namespaces are unavailable it degrades to proxy-only, and
  `GET /api/admin/ffmpeg/capabilities` → `network_sandbox` says which.
  rtmp/tcp/udp network inputs have no boxed path and are refused.
  `test_netjail.c` decodes a real HLS frame through the box when ffmpeg is
  installed (CI installs it), as root and as an unprivileged user.
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
