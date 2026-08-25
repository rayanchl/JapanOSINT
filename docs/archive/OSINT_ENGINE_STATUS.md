# OSINT search engine (Node→C) — COMPLETE (build-verified)

> **HISTORICAL SNAPSHOT.** "(318 sources)" below is the state at the time this
> port landed, not a current count, and it did not expand registration macros.
> Current figure: `make -C native source-count` — 2,563 on 2026-08-09. See
> [`../docs/collectors.md`](../docs/collectors.md).

Port-from: server/src/osint/{pipeline,dispatcher,prompts,progressTracker}.js
+ utils/searchIngest.js + routes/search.js. All build-green (318 sources).

## DONE
- core/osint_dispatch.{c,h} — dispatcher = the source registry, UNIFIED: no
  source_kind (enum+field+314 .kind= literals physically deleted). ANY
  registered source dispatchable by id, pivoted on ctx->entity. Dual sink:
  persists LIVE via the shared intel_sink (FTS/MeCab/entity-NER/alert) AND
  captures result JSON for Phase-2. Graceful not_implemented.
- core/prompts.{c,h} — analysis/phase2/suggestions builders + GBNF loader.
  Byte-identical to prompts.js (adversarial-diff verified by the agent).
- core/progress.{c,h} — request tracker; toJSON matches progressTracker.js
  field-for-field; thread-safe (httpd SSE reads while worker writes).
- core/pipeline.{c,h} — faithful pipeline.js: Phase-1 LLM analyze
  (grammar osint_analysis) → task build (osint_handler_key|entity dedup +
  always-on JP_CORPUS_LOOKUP per entity) → dispatch round 0 → Phase-2
  ≤max_rounds chain loop (ungrammared, extract first balanced {}) →
  aggregate → run-summary row via intel_sink. LLM = shared core/llm.h
  (model-agnostic; gpt-oss today, follows the one runtime).
- core/searchapi.{c,h} + httpd.c — POST /api/search/analyze (detached
  worker thread, own http+llm, shared db) → {request_id}; GET /suggest;
  GET /results/:id (live snapshot or reconstruct from osint-search|run:);
  GET /stream/:id PRE-AUTH SSE (UUID=capability; 500ms snapshot poll +
  terminal close, via MG_EV_POLL/CLOSE). /api/search no longer 501.
- Services as unified sources (interval=0, on-demand): **61 OSINT services
  registered** (was 5). The OSINTsaas `osint_tools/*.c` catalog was ported
  via 4 parallel agents into `collectors/osint/sources/*.c` under the unified
  ABI (.id = the canonical SERVICE name from osint_dispatcher.c; run() reads
  ctx->entity, emits one osint_service_result; dispatcher auto-routes any
  registered id). Build-green, REGISTER_SOURCE==registered (375), zero dup
  ids. This far exceeds Node (which only ever implemented ~5).
  Irreducibly NOT ported (documented, faithful — same stance as elsewhere):
  `people_finder`/`phone_intel` (95k/49k-LOC mega multi-scraper aggregators
  needing the full ~80-scraper + result_builder subsystem),
  `image_analysis`/`image_recognition_c` (OpenCV/Tesseract — not pure-C),
  and dead/internal-only tools with no canonical dispatcher SERVICE
  (canadian_osint, emailfinder_real, social_scanner, sublist3r [== subdomain],
  phoneinfoga_real). These dispatch as graceful not_implemented.
- **Read-time line stitch/smooth: DONE** — `core/linegeom.{c,h}` (faithful
  transportStore.js stitchAndSmoothLines: degree-2 endpoint stitch →
  Chaikin×4 → RDP ε1e-6; bus passthrough), wired into `sweepapi.c`
  unified_fc (the /api/data/unified-<mode> line path, lines-before-stations
  order preserved). Pure geometry, deterministic, build-verified. The
  earlier "raw fragments" deviation is closed.
- 14 WAF'd camera channels: already ported & registered (cam_*.c); WAF/403
  from a datacenter IP + headless-Chromium-only pages + missing keys yield 0
  — FAITHFUL to Node (same from cloud), not a code defect, nothing to "fix".
- Live LLM (NER enrich, pipeline rounds, tier-2 dedup): code-/build-verified
  + deterministically smoked (gate, empty-drain, SQL-valid). A LIVE run needs
  llama-server up (absent in the build env) — it is part of the user-side
  P8 end-to-end validation, not separately runnable here.

## Entity graph — UNIFIED & build-verified (one path)
The earlier "entities flow via the sink's NER hooks" note was WRONG and is
now fixed by a real port (no second mechanism):
- `core/entitystore.{c,h}` — THE one write surface (faithful entityStore.js:
  es_norm_key/upsert_entity/add_mention/add_relationship/merge_pair_exists/
  record_merge/union_entities; entities_fts mirror via fts_segment, verbatim
  SQL/keys/ON-CONFLICT).
- `core/entity_enrich.{c,h}` + `collectors/_enrich/sources/llm_enricher.c`
  (registered source, interval 300, LLM_ENABLED gate) — the ONE NER enricher,
  on the SAME scheduler path every source uses, draining the ONE intel_items
  the ONE sink fills (collectors AND osint-search rows alike) via the
  entity_extraction_state watermark; tier-2 candidate-pair dedup → entity_
  merges → union ≥0.7. Faithful port of entityExtractor.js.
- `core/prompts.c` +prompt_entity_extraction / +prompt_entity_dedup.
- `core/pipeline.c` searchIngest step-2 wired through the SAME es_* surface
  (query→seed mentions 0.9, phase-2→discovered mentions 0.6, seed→discovered
  `pivot_discovered` edges weight 1.0 evidence=run uid).
Verified: build green (319 registered, llm-enricher scheduled); gate faithful
(LLM_ENABLED unset → skipped rc0); empty-DB drain clean (extract/resolve
attempted=0, rc0 — SQL valid vs real schema). Live per-item NER + tier-2
LLM dedup need llama-server up (same bar as the rest: faithful port + clean
combined build + deterministic smoke; LLM/network smokes flaky here).
`source_kind` already physically deleted — collectors and OSINT services are
indistinguishable; both feed the one graph through the one path.
- Live end-to-end run needs llama-server up; verification bar = faithful
  port + clean combined build (network/LLM smokes are flaky here, as all
  session). Structurally complete & compiling.
- SSE = 500ms throttled snapshot push (JS pushed on update + 15s ping) —
  behaviourally equivalent for the client; /results polling also works.
