<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Rule 1 (never fabricate data): fetch-failure disclosure, refusal shapes, registry health sweep. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

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

