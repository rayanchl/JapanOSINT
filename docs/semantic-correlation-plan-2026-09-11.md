# Plan — semantic routing, catalogue metadata, imagery, correlation

*Written 2026-09-11 against the working tree at `e1c848f` + the uncommitted
2026-09-04/06 fix pass. Every "today" line below is a `file:line` that was read,
not remembered. Where the brief's premise does not match this repository, the
mismatch is stated first and the item is re-planned around what is actually
here — per house rule 1, a plan that cites a struct we do not have is the same
failure as a collector that emits a record it did not fetch.*

---

## 0. State of the tree today (measured, not assumed)

| check | result |
|---|---|
| full build, `-Wall -Wextra` | 0 errors, 0 warnings |
| registered sources | 16,366 |
| `selftest` | PASS (integrity ok, 176 schema objects, llama down → degrades) |
| `unit` / `hptest` / `pagewalktest` | pass |
| `lint-sources` | OK (geo-precision 430, improved from baseline 432) |
| `audit-sources` | **0 findings, whole tree** — 8 findings introduced by the fix pass were closed today (see §9) |
| web client | `vite build` clean, 62/62 vitest pass |
| uncommitted | 206 modified + 35 untracked files — an entire audit/fix pass and a web feature set, never committed |

---

## 1. Premise check — three items are already done, two do not exist here

**A (service dispatch).** Partly done, and the drift is smaller than described.

* There is **no `SERVICE_*` enum and no 112-handler table.** Services and feeds
  are one registry: `registry.c:29` (`g_srcs[]`), fed by `REGISTER_SOURCE`
  (`source.h:141`) and `HP_REGISTER_TABLE` (`lib/hpengine.c:2850`). An
  "entity-pivot service" is a *filter* over it — `is_entity_pivot()`,
  `core/osint_dispatch.c:86` (collector `osint` + interval 0). That filter
  currently selects ~1,838 rows.
* **"Retrieve top-20 service cards instead of pasting all 112" is built and
  wired.** `core/service_vec.{c,h}` embeds each service's `id: name.
  description` into a `vec0` table and returns the K nearest to the query;
  `core/pipeline.c:475` calls it, `pipeline.c:476` falls back to
  `osint_services_list_bounded()` when it returns NULL, and `pipeline.c:529`
  pairs the *same* ids into the response schema via
  `osint_analysis_schema_dynamic_ids()` so the model is never briefed on
  services it was forbidden to name. Default K = 60 (`service_vec.c:18`).
  It is **inert today** because `JO_EMBED_URL` is unset (§2).
* **The GBNF is not four-way drift — it is dead.** `grammars/osint_analysis.gbnf`
  carries 48 hand-written names, and **nothing loads it**: every
  `grammar_load()` call site asks for `entity_extraction`
  (`core/entity_enrich.c:81`) or `suggestions` (`core/pipeline.c:884`). The live
  constraint is the JSON schema, and `osint_schema_with_ids()`
  (`osint_dispatch.c:253-282`) **deletes both hand-written enums and rewrites
  them from the registry on every request**. The 107 names in
  `grammars/osint_analysis.schema.json` only reach a model if both dynamic
  paths return NULL (`pipeline.c:533`).
* Genuinely open: the **name-miss fallback** (`osint_dispatch.c:402-406` →
  `error="not_implemented"`, no fuzzy match), the **stale vocabulary files**
  (48 + 107 + 91 service strings quoted across 76 few-shot examples in
  `core/prompts.c:173-390`, none of them checked against the registry by any
  gate), and **few-shot selection** (static block, no retrieval).

**B (a 2,315-row catalogue with `category`, `entity_types`, `country_region`
across 155 jurisdictions, `penetrancy`).** **No such file exists in this repo.**
Searched every `.tsv/.csv/.txt/.json` under `docs/`, `data/`, `native/`,
`client/`, `ios/`. What exists:

* `docs/verified-sources-manifest.tsv` — 2,890 rows, columns
  `id url kind items name name_ja collector category lang tags interval
  description`. **Not inert**: 10 of 10 sampled ids appear verbatim as
  registered ids in `collectors/feed/generated/vsrc*.c`. It is the input the
  registry was generated *from*.
* `docs/candidate-sources-batch*.tsv|.txt` — the batch pipeline's own inputs,
  consumed by `tools/gen_hp_batch.py`.
* `docs/osint-sources-1000.md` — 1,000 entries of keyless prose
  (`N. Name — domain — what it provides`). **This one is genuinely inert** and
  is the only real untapped list.
* `entity_types` appears nowhere as source metadata — only as an *alert filter*
  in `client/src/components/alerts/AlertsPage.jsx`. "penetrancy" appears only
  in prose and as a comment about `detail_max` in `lib/hpengine.h`. There is no
  "155 jurisdictions" anywhere.

So the real finding is stronger than the brief: **the live registry carries no
jurisdiction, entity-type or penetrancy field at all.** `source_def`
(`source.h:78-106`) and `hp_source` (`lib/hpengine.h:86-286`) both stop at
`category` + free-text `tags`; the `sources` table (`core/schema.sql:455-473`)
mirrors exactly those four columns via `db_seed_sources()` (`core/db.c:114`);
`/api/status` (`core/statusapi.c:521`) can only project what is there. The work
is not "index a spreadsheet" — it is "give the registry the dimensions, from
evidence, then route and disclose on them" (§4).

**C (imagery: ONNX/YOLOv11/Places365/SSD faces/Tesseract, `ir_detected_face_t`,
`SERVICE_FACE_RECOGNITION` in the enum).** **None of it is in this repo.**
`grep -rn "ir_detected_face\|SERVICE_FACE\|onnx\|opencv\|yolo\|places365"` over
`native/` returns nothing but shell-out comments. `pkg-config --modversion
opencv4` fails in the build host; there is no ONNX runtime vendored or
installed; the tree is C-only with no C++ rule in the Makefile, and
`FaceRecognizerSF` is a C++ API. What does exist:

* `core/media.{h,c}` — pure-C EXIF + dimension + magic-byte parsing, plus
  **pHash** (`media.h:549-560`) and **OCR** (`media.h:571`) implemented as
  *shell-outs* to `magick`/`convert`/`gm` and `tesseract`. Neither binary is
  installed on this host, so both are inert and every `media_assets.phash` is
  NULL in practice.
* `media_assets` (`media.h:259-265`) already has the columns a vision pipeline
  needs: `sha256, phash, exif_json, ocr_text, ocr_conf, width, height`.
* `docs/archive/OSINT_ENGINE_STATUS.md:41` records that `image_analysis` was
  **deliberately not ported** from the Node original ("OpenCV/Tesseract — not
  pure-C"), and dispatches as `not_implemented`.

So C is net-new construction, not wiring-up. §5 plans it in three honest steps.

**D (`people_finder.c:2572`, confidence `70 + found*6`).** The file is
`collectors/sources/people_finder.c` and it is 509 lines with no confidence
math. The real site is **`core/osint_dispatch.c:467`**:
`out->confidence = out->success ? 70 : 0;  /* JS default */` — a binary
success flag printed as a number, surfaced per service at `pipeline.c:294`.
The rest of D is accurate: results are appended flat, one element per service
(`pipeline.c:434-436`), with **no cross-service reconciliation**. Two real
mechanisms do exist and must be reused rather than re-invented: entity merge
(`core/entity_enrich.c:196`, `core/entitystore.c:621`) and record near-dup
clustering (`core/simhash.c`, called at `core/intel.c:397`, exposed as
`?collapse=1` in `core/intelapi.c:576`).

---

## 2. P0 — turn the embedding stack on. Everything else is downstream.

**Today.** The machinery is complete and unreachable. `embed_pod.c` (intel
vectors), `service_vec.c` (routing), `/api/intel/semantic`
(`core/semsearchapi.c:164`, RRF over vec0 + FTS5, tenant-scoped, coverage in
every response) all require `JO_EMBED_URL`, which `launch.sh:385-404` only sets
when `models/bge-m3-Q8_0.gguf` exists. **It does not exist on this machine.**
What does: `~/jo_audit_2026_09_03/models/multilingual-e5-large-q8_0.gguf`
(603 MB, 1024-d) in WSL — the e5 model the brief names.

**Do.**
1. Put the e5 model where `launch.sh` looks (`models/`), or start the embed pod
   with `LLAMA_EMBED_MODEL` pointed at it; bring `llama-embed` up on :8082 and
   export `JO_EMBED_URL`.
2. Build both indexes and record the numbers: `service_vec_build()` over the
   1,838 pivots, then let `embed_pod` sweep `intel_items`.
3. Measure the routing change end to end: for ~20 real queries, log the top-K
   ids with and without `JO_ROUTE_SEMANTIC`, and record how often a
   hand-written pivot (`DOMAIN_WHOIS`, `IP_GEOLOCATION`, `DNS_RECORDS` — the
   ones registry order truncates away) enters the menu.
4. Write the dimension into the ops notes: e5-large is **1024-d**, bge-m3 is
   1024-d as well, but `embed_pod` refuses to mix spaces — a model swap is a
   rebuild, never a comparison.

**Verify.** `/api/intel/semantic` stops answering 503; `coverage` reports a
non-zero `embedded_count`; the service-index KNN returns query-dependent menus
(the existing `tests/unit/test_service_vec.c` asserts this against a stub — now
assert it against the real model, on real queries).

**Why first.** A2, A4, E, F, G, H and the whole of §5's C3 are all "use an
embedding"; none of them can be measured while the server is down. This is
hours of work, not days, and it is the difference between a feature that is
*written* and a feature that *runs*.

---

## 3. A — service dispatch

### A1. Top-K service cards — **done**, needs measurement (see P0).
Keep `JO_ROUTE_TOPK` tunable; record in-band how the subset was chosen
(already done: `osint_catalogue_note`, `pipeline.c:514`).

### A2. Nearest-neighbour fallback on a name miss
**Today.** `osint_dispatch.c:402-406`: unknown name → `not_implemented`, full
stop. A model that writes `DOMAIN_WHOIS_LOOKUP` or `whois_domain` gets nothing.

**Do.** A two-stage resolver in front of `osint_lookup()`:
1. **Deterministic normalisation first** (works with no embed server):
   case/underscore/hyphen fold, strip a `_LOOKUP`/`_SEARCH` suffix, then
   bounded edit distance (≤2) over registered pivot ids. One exact winner wins.
2. **Embedding KNN second**, reusing `service_vec`'s index with the *name* as
   the query, accepted only above a similarity floor and only when the runner-up
   is clearly behind.
3. **Never silent.** The result carries `resolved_from: "<what the model
   said>"`, `resolver: "edit-distance"|"embedding"`, and the similarity. If
   nothing clears the floor, the answer stays `not_implemented` — an honest
   miss, which is what house rule 1 requires.

**Verify.** A unit test that feeds 30 mangled names (real misses harvested from
logs, plus typos) and asserts: correct resolution where a clear winner exists,
`not_implemented` where two candidates are close, and the substitution always
stated in the output.

### A3. Stop the vocabulary from rotting — make drift a CI failure, not a habit
**Today.** Three hand-maintained lists name services: `osint_analysis.gbnf`
(48, unloaded), `osint_analysis.schema.json` (107 × 2 blocks, overwritten at
runtime, live only as last-resort fallback), and `prompts.c`'s 76 few-shot
examples (91 distinct service strings). Nothing checks any of them.

**Do.**
1. **Delete `grammars/osint_analysis.gbnf`** or regenerate it — it is not
   loaded, and `prompts.c:846-853` records *why* raw GBNF was abandoned for this
   prompt. Deleting a file that lies is a fix.
2. **Generate** the static schema's two enums from the registry with a small
   tool (`tools/gen_service_enums.py`, run like the other generators), so the
   fallback path cannot name a service that no longer exists.
3. **A gate, ~40 lines**: a unit test that extracts every uppercase service
   string from `prompts.c` and from `osint_analysis.schema.json` and asserts
   each resolves via `registry_get()`. This is the cheap half of the item and
   it is what actually stops the drift — a generator without a gate rots the
   moment someone hand-edits.

### A4 / F. Few-shot retrieval
**Today.** `prompt_analysis()` (`prompts.c:173-390`) pastes the same 76
examples into every request regardless of the query.

**Do.** Move the examples out of C string literals into a data file
(`grammars/fewshot_analysis.jsonl`, one `{query, response, services[]}` per
line — losslessly, all 76), embed them once into a `fewshot_vec` index, and
select the top ~8 by similarity to the live query, always including a small
fixed "spine" set that teaches the output *shape*. Inert-safe: no embed server
→ the current static block, byte for byte.

**Verify.** Token count of the phase-1 prompt before/after; and an A/B on a
held-out query set measuring whether the model's chosen services survive
`registry_get()` (the A2 miss rate is the honest proxy for prompt quality).

---

## 4. B — give the registry the dimensions it does not have

**Today.** `category` + free-text `tags`, nothing else (§1). The dispatcher
cannot prefer Japanese sources for a Japanese question, cannot say "we covered
12 of 41 jurisdictions that hold this kind of record", and cannot order an
integration backlog by anything but a human's guess.

**Do, in four steps, each independently useful.**

1. **Add three declared fields** to `source_def` and `hp_source`:
   `jurisdiction` (ISO-3166 alpha-2, or `INT` for supranational, or `NULL` =
   undeclared), `entity_types` (a small bitmask: person, company, domain/IP,
   vessel, aircraft, place, document), `depth` (list-only / list+detail /
   bulk — this is what "penetrancy" actually means here, and
   `hp_source.detail_max` already encodes part of it). Mirror them into the
   `sources` table in `db_seed_sources()` (`core/db.c:114`) and project them in
   `/api/status`.
2. **Populate from evidence, never from vibes.** Three provenances, and the row
   records which one it used: `declared` (written in the manifest), `derived`
   (host TLD `.go.jp` → JP, `europa.eu` → INT/EU; collector-name prefix
   `vsrc17_ke_aid_1` → KE), `unknown`. A derived value is displayed as derived.
   That distinction is the whole difference between this being metadata and
   being fabrication.
3. **Jurisdiction routing**: feed the field into the service menu — a query
   whose entities or text carry a country signal boosts that jurisdiction's
   pivots, *without* hiding the others, and the menu note states the boost.
4. **Coverage disclosure**: `/api/search` answers already bound their view;
   extend the note to "N services run, covering J of K jurisdictions that hold
   this record type", which is only expressible once (1) exists.

**And the genuinely inert asset:** run `docs/osint-sources-1000.md` through the
batch pipeline — parse the prose into a manifest, dedupe with
`tools/batch_exclusions.py --bin ./bin/japanosint` (it normalises `{q}`/`%s` and
sees runtime-composed endpoints), probe with `probe_hp_batch.py --check-filter`,
generate. Expect most of the 1,000 to be already registered; the residue is a
real batch.

**Verify.** `make lint-sources` extended with a "jurisdiction declared or
derived-with-provenance" count; a query with an explicit country producing a
measurably different menu; the batch's own gates (probe → emit → registry
sweep) for the wishlist.

---

## 5. C — imagery, built from what is actually here

**C1 (hours). Make the two features that already exist actually run.**
`media_phash_file()` and `media_ocr_file()` shell out to ImageMagick and
Tesseract; neither is installed, so `media_assets.phash`/`ocr_text` are
uniformly NULL. Install them in the runtime image, backfill the existing
`media_assets` rows, and report coverage ("pHash on N of M stored images") the
way `embed_coverage_json()` does. Zero new code, and it turns a dead column
into a working one.

**C2 (a day). Near-duplicate image detection on the pHash we then have.**
`media_phash_distance()` exists; what is missing is an index. Reuse the
**banded-LSH pattern already written for text** in `core/simhash.c` (4 bands,
zero false negatives to Hamming 3) over the 64-bit pHash, and expose
near-duplicate groups the same way `?collapse=1` does for items
(`intelapi.c:576`). This answers "have we seen this image before" across the
corpus with no new dependency at all.

**C3 (the real one). A sidecar encoder pod, not a link dependency.**
The blocker on CLIP and face descriptors is that adding ONNX Runtime or OpenCV
C++ breaks the project's no-new-link-dependency rule. The project already has
the answer to that problem and uses it for text: **llama-server is a sidecar,
and `llm_embed()` talks to it over HTTP.** Do the same for vision.

* A small `imgvec` pod (Python + onnxruntime, or llama.cpp's CLIP support)
  exposing `POST /embed-image` → a float vector, launched by `launch.sh` with
  the same "missing model → skip silently, feature stays inert" contract as
  `cmd_llama_embed` (`launch.sh:387`).
* Store descriptors in an `image_vec` `vec0` table — the same shape as
  `service_vec` (`service_vec.c:269`) and `intel_vec` (`embed_pod.c:236`).
* That single pod makes **both** unbuilt features buildable and honest:
  reverse-image search = KNN over `image_vec` *within our own corpus* (never
  pretending to be a web-scale reverse search), and face matching = a second
  model producing face descriptors, with `media_faces(asset_id, bbox,
  confidence, descriptor)` as the table the brief's `ir_detected_face_t` was
  reaching for.
* Face recognition is identity-adjacent: it needs the same tenant scoping the
  rest of the corpus has, and a stated similarity threshold on every match —
  "0.71 similar to 3 stored faces" is a finding; "same person" is a claim we
  cannot make.

**Verify.** C1: non-null coverage numbers. C2: a seeded set of known duplicates
recovered at distance ≤3, and a stated false-positive rate. C3: inert without
the pod (the `service_vec` test is the template), and KNN recall on a labelled
handful.

---

## 6. D — result correlation and a confidence number that means something

**Today.** `osint_dispatch.c:467` — `confidence = success ? 70 : 0`. Twelve
services can return the same fact and it is still 70; one service can return a
single weak hit and it is also 70. `pipeline.c:434-436` appends results flat,
no reconciliation.

**Do.**
1. **Replace the number with a structure, and keep the number honest.** Per
   result: `records`, `fields_present`, `source_depth` (from §4's `depth`), and
   — once (2) exists — `corroborating_sources`. If a single scalar must be
   shown, derive it from those and ship the components alongside, so the UI can
   explain it. Never show a percentage whose inputs we cannot name.
2. **A fusion step between dispatch and response.** Group results across
   services on normalised keys the tree already knows how to build:
   `entitystore`'s `norm_key` for names/companies, exact keys for
   domain/IP/email/phone, and `simhash` cluster ids for free text. Output
   groups, not a flat list: "4 services report this address; 2 disagree on the
   postcode", with every contributing `source_id` kept. **Merging must never
   drop a differing value** — that is house rule 2 at the response layer, and
   it is the same discipline as the in-page collision guard (`+` composes,
   content-hash disambiguates, nothing that differs is merged).
3. Wire the group ids through to the client so the map and list can collapse
   consistently with §8.

**Verify.** A fixture query with deliberately overlapping services: assert the
group count, assert no input record disappears, assert the disagreement is
reported rather than resolved.

---

## 7. E / G / H — planning, caching, suggestions

**E. Chain planning.** `pipeline.c:619-730` loops rounds on pure LLM reasoning.
Feed round *n+1*'s service menu from `service_vec` queried with **the entities
discovered in round n**, not the original question — that is a two-line change
at the call site plus a note stating what drove the second menu. Cheap, and it
is the difference between a chain and two independent searches.

**G. Cache.** There is **no LLM response cache** — only llama's own
`cache_prompt: 1` KV hint (`core/llm.c:143`) and a static grammar-file cache
(`prompts.c:788`). Do it in two steps: (i) an **exact** cache keyed on
`(model, prompt_sha256)` with a TTL, in SQLite, which is safe and needs no
embeddings; (ii) optionally a **semantic** hit above a high similarity floor —
and if a semantic hit is served, say so in-band with the similarity and the
age. An unlabelled semantic cache hit is a fabricated answer to a question
nobody asked.

**H. Suggestions.** `searchapi.c:159` → `osint_suggest()` (`pipeline.c:876`)
issues a full LLM completion per request. Put (G)(i) in front of it, then add a
retrieval arm over past queries and `intel_items` titles — both are already
embedded once P0 is on.

---

## 8. I — fusion-map dedup

**Today.** `core/dataapi.c:427` (`dataapi_layer_fc`) runs one `SELECT` per
layer and emits one Feature per row. Bounded-view disclosure is present
(`dataapi.c:280-286`); **dedup is absent** — no `cluster_id` join, no geometry
proximity, no content hash. Two sources reporting the same incident are two
pins.

**Do.** Reuse what `/api/intel/items` already offers rather than inventing a
second mechanism: an opt-in `?collapse=1` on `/api/data/<layer>` that groups by
`intel_items.cluster_id` and, for rows without one, by rounded coordinate +
record type. Every group states its member count and keeps every member's
`source_id` in the feature's properties, so collapsing never destroys
attribution. Default stays uncollapsed — changing what an existing client
renders by default is a contract change, not a repair.

---

## 9. Unification — the same structure written four times

This repo's own history is the argument: the uid-collision bug existed
independently in `jsonlist.c`, `hpengine.c`, `geojson.c` and `rss_atom.c`, and
fixing three left the fourth losing data. The same shape is now appearing in the
embedding layer.

1. **`lib/vecindex.{c,h}` — one vector index.** `embed_pod.c` and
   `service_vec.c` each implement: create a `vec0` table, probe the server's
   dimension, refuse to mix models, build atomically, promote a state flag,
   KNN-query. `fewshot_vec`, `image_vec` and a query cache would each be a
   fourth and fifth copy. Extract it once, with the two hard-won lessons baked
   in — **never `ALTER TABLE RENAME` a vec0 table** (shadow tables do not
   follow; `service_vec.c` records the incident) and **probe the live server
   for the dimension**, never another index's metadata.
2. **One bounded-view note.** `osint_catalogue_note` (dispatch),
   `collector-truncation-notice` (pagewalk/hpengine), `shown/total/truncated`
   (`intelapi`), `_meta.records_available` (`dataapi`), `coverage`
   (`embed_pod`) are five spellings of "here is what you are not seeing".
   One struct, one serializer, one place to get the wording right.
3. **One id resolution path.** `osint_canon` + `registry_get` + A2's resolver,
   used by dispatch, the API and `--dispatch`, so a name that resolves in one
   resolves in all three.
4. **One provenance/confidence record** (§6.1 and §4.2 are the same shape:
   a value, where it came from, and how sure we are).

---

## 10. Leftovers from the previous pass — Track 1, still open

* **989 registered sources still defective** (612 `EMITS_NOTHING`, 370
  `COLLISION`, 5 `SLOW`, 2 resolved manually), measured cumulatively across all
  sweeps; the round-1 baseline was 1,770 and 783 have been repaired. Remaining
  collision loss ≈ 66,197 records/pass, of which ~24,932 is proven-correct
  dedupe (`gr-diavgeia-positions`) → **real loss ≈ 41,265 per pass**. Working
  list regenerated today; three agents are on the top 213 ids.
* **Engine follow-ups** recorded but not done: `jsonlist.c` wrapper descent for
  `{member:{…}}`; `digitraffic` gzip (`httpclient.c` sets
  `CURLOPT_ACCEPT_ENCODING ""`); **three sources share the id `"real-estate"`**
  and `lint-sources` does not catch it; `drone-nofly` emits 0 by design;
  `eco-energy-charts-{gr,no,ro,se}` are columnar and should re-point at the
  existing handler; NCBI esearch rows need an esummary hop;
  `OASIS_PUBLISHED_STANDARDS` 520s under the per-host UA override.
* **`/api/status` is 20.5 MB / 11.6 s** and on both clients' startup path
  (sibling `/api/intel/sources` ≈ 10 MB). This wants a summary projection — an
  API contract change touching iOS and web, so it is a decision, not a repair.
* ~~**The generated files carry hand edits their manifests do not.**~~
  **Closed 2026-09-11.** It was measured before it was decided: regenerating
  every batch manifest and diffing against the committed C showed **48 of 79
  beats already differ — 62,461 lines** — and the drift is always the same
  direction, the C ahead of the manifest, carrying repairs with the live counts
  that justify them. One manifest (`batch19.latam`) will not even generate any
  more: it holds a `\;` swallow the generator now rejects by name.

  So the model was already dead in practice, and the resolution is to say so
  rather than to chase 62k lines of mirroring into a format that cannot hold an
  evidence comment: **a manifest scaffolds a beat once; after that the C is the
  maintained copy and the manifest is the record of how the beat was probed.**
  Both generators now refuse to overwrite an existing file (reporting how many
  lines would have changed) and take `--force` for the case where the manifest
  really is newer; the 93 + 408 already-generated files had their "Edit the
  manifest, not this file" header replaced with what is actually true. CLAUDE.md
  §"Batch tooling" carries the correction and the measurement behind it.

* **The tree is uncommitted.** 206 modified + 35 untracked files carrying the
  entire fix pass, `service_vec`, the httpd worker fix and a web feature set.
  All seven gates pass on it today. It should be committed before anything else
  lands on top.

---

## 10b. Done on 2026-09-11, with the measurements that drove it

P0 was not just planned, it was run, and running it changed three of the items
above. The embedding server (`multilingual-e5-large-q8_0.gguf`, 1024-d, on
:8082 — see the memory note for the exact invocation) was brought up and the
service index built against the real model, twice.

**1. The index build is 4.5 minutes, and it was happening inside a search
request.** `service_vec_catalogue()` built on demand: measured **273.5 s and
280.1 s** for 1,838 services. Moved to `core/service_vec_pod.c`, a `_maint` pod
on interval 900 — the scheduler's thread, where minutes are normal. While it
builds, routing falls back to registry order with the bound stated, which is
the documented behaviour, not a new degradation. `JO_ROUTE_BUILD_INLINE=1`
restores the inline build for a CLI run or a test.

**2. Semantic-only routing missed the service the feature exists to rescue.**
At the production K of 60, `"who owns example.com"` returned `DNS_RECORDS` at
rank 57 and **no `DOMAIN_WHOIS` at all**. So §3 A1 was wrong to call this done.
A lexical arm (fts5 over the same service cards, built in the same pass, fused
with the vector arm by RRF — the pattern `semsearchapi.c` already uses for
intel) now runs alongside it. Same query, same K, after:

| query | before (vector only) | after (fused) |
|---|---|---|
| who owns example.com | DNS_RECORDS 57; no WHOIS, no IP_GEOLOCATION | IP_GEOLOCATION 6, ASN_LOOKUP 8, RIPESTAT_HISTORICAL_WHOIS 34, DNS_RECORDS 35 |
| whois for example.com | — | **DOMAIN_WHOIS 1**, and the whole WHOIS/RDAP family in the top 10 |
| sanctions on a shipping company in Dubai | OS_DEBARMENT 1, CN_MOFCOM 2 | EU_SANCTIONS 1, UN_SANCTIONS 2, AE_DED_LICENCE 3, SANCTIONS_CHECK 7 |
| company registration and corporate filings | COMPANY_LOOKUP 1 | COMPANY_LOOKUP 2, eight national registries above rank 14 |

The honest limit, stated because it decides the next item: the lexical arm can
only match words the user actually typed. `"who owns example.com"` never says
"whois", so **richer service cards** — a line of aliases per service, "who owns
a domain, registrant, registrar" — is now the highest-value routing change
left, ahead of any further ranking work. Japanese queries arrive as one
unicode61 token and the lexical arm contributes nothing for them; the vector
arm serves those, as before.

**3. Vocabulary drift is now a gate.** `tests/unit/test_service_names.c` asserts
that every service name in the static schema's two enums (107 each, identical)
and every service name inside the few-shot examples in `prompts.c` (627
mentions) resolves in the live registry. All pass today.
`grammars/osint_analysis.gbnf` was deleted: nothing loaded it, and 25 of its
100 names were services that no longer exist. The test asserts it stays gone.

**4. The biggest single find of the day came out of the repair round, not the
plan: `page_param` APPENDED instead of replacing.** `lib/hpengine.c` built the
next page as `snprintf("%s&%s=%ld", url, param, n)`, so a row whose URL already
bound its own page parameter — the shape of every API that *requires* it on the
first request (PNCP answers 400 without `pagina`) — asked for
`…&pagina=1&pagina=2`. A server that binds the first occurrence, which Spring
and JAX-RS both do, then served page 1 for the entire walk: N pages emitted,
one page stored, run `rc=0`, every gate in this repository green.
`BR_PNCP_CONTRATOS` lost 4,499 of 5,000 records that way.

It now replaces in place, on a real parameter boundary (`?key=`/`&key=`, so a
row paging on `page` does not rewrite `page_size`), and `tests/hpengine_test.c`
pins it with fixtures that fail if the parameter is ever duplicated again.
**A scan of the tree found 103 rows of that exact shape** (of 1,431 declaring
`page_param`) — stated as the number of rows the defect could touch, not as
proven loss: whether a given upstream binds the first or the last occurrence
decides whether it lost anything, and only PNCP was measured directly.
The scanner is `scratchpad/scan_pageparam.py`; it belongs in `tools/` as a
lint rule, which is the cheapest way to keep this from coming back.

Also from the source-repair round: `data.boston.gov` joined the per-host UA
table in `core/httpclient.c` (bisected — it is the word "collector" again,
exactly as at INE; 35 sources were storing nothing), and `lib/hpengine.c` now
discloses a new condition, `id-keys-truncated`: a declared `id_keys` that was
unreachable because the record's flatten hit the 2,048-property cap. Those
records silently fell back to their first scalar — a dimension label on a
statistical document — and collapsed at the sink while the run reported an
ordinary uid collision (found on `IE_CSO_COLLECTION`: 13,008 records, 13,008
distinct declared keys, 9 lost).

## 11. Order of work

| # | item | why here | rough size |
|---|---|---|---|
| 1 | commit the verified tree (§10) | a week of green work is unprotected | minutes |
| 2 | **P0** embed pod up, both indexes built, routing measured (§2) | unblocks A2/A4/E/G/H/C3 | hours |
| 3 | A3 drift gate + delete the dead GBNF (§3) | cheapest permanent win | hours |
| 4 | A2 name-miss resolver (§3) | turns silent `not_implemented` into a stated resolution | 1 day |
| 5 | D fusion + honest confidence (§6) | the biggest user-visible quality gap | 2–3 days |
| 6 | §9.1 `lib/vecindex` extraction | do it before the 4th and 5th copy exist | 1 day |
| 7 | B registry dimensions + provenance (§4) | unlocks routing *and* disclosure | 2–3 days |
| 8 | A4/F few-shot retrieval, E chain menu, G exact cache, H suggestions | all ride on P0 + vecindex | 1 day each |
| 9 | C1 → C2 → C3 imagery (§5) | C1/C2 are cheap and real; C3 is the new pod | 1 day, 1 day, ~1 week |
| 10 | I map collapse (§8) | after D, so both use one grouping | 1 day |
| 11 | the 989 sources (§10) | continuous, 3 agents per round | ongoing |

Nothing above is blocked on a decision except `/api/status` pagination (an API
contract change) and whether C3's sidecar pod is acceptable as a dependency
shape. Both are called out where they occur.
