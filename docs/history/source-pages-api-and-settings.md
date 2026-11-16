<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Source pages API, per-source settings. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

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

