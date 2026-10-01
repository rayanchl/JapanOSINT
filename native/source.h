/* native/source.h — the ONE data-acquisition ABI.
 *
 * Every data source is one .c file defining a `source_def`; a `collector` is
 * a .c module grouping sources. A JapanOSINT feed and an OSINTsaas service
 * are the SAME type — they differ only by `kind` and what run() does. All
 * emit the standard intel envelope into the single `intel_sink` chokepoint
 * (core/intel.c → upsert + FTS/MeCab + entity/alert hooks), exactly mirroring
 * intelStore.upsertItems semantics so the unification is real. */
#ifndef JO_SOURCE_H
#define JO_SOURCE_H

#include "core/db.h"
#include "core/httpclient.h"
#include "core/llm.h"

typedef struct intel_item {
  const char *uid;            /* NULL → sink derives "<source_id>|<key/hash>" */
  const char *remote_key;     /* natural id (earthquake_id, station_uid, …)  */
  const char *title, *body, *summary, *link, *author, *lang;
  const char *published_at;   /* ISO-8601 or NULL */
  int    has_geo; double lat, lon;
  const char *geometry_geojson;
  const char *record_type;
  const char *sub_source_id;
  const char *properties_json;/* "{}" if NULL */
  const char *tags_json;      /* "[]" if NULL */
} intel_item;

/* ── HOUSE RULE: EXHAUSTIVE USE (docs/SOURCE_EXHAUSTIVENESS.md) ────────────
 * A source that is called must be used exhaustively. If run() spends a request,
 * it emits EVERY record the response contained, with EVERY field of each record
 * in `properties`, follows pagination to the end, and follows the detail
 * endpoint behind each list hit. No implicit "first N", no hand-picked three
 * fields, no page-1-only read of a paged API.
 *
 * This is the twin of the no-fabrication rule: fabricating invents facts that
 * were never fetched, discarding throws away facts that were. Both misrepresent
 * what is known, and discarding is the more dangerous of the two because it
 * looks like a clean result.
 *
 * If something genuinely cannot be taken (a bulk file too large to hold, an
 * upstream hard cap), the shortfall is reported IN THE DATA — records_used vs
 * records_available — not left to a log line. `make audit-sources` scans for
 * the usual discard patterns. */

typedef struct intel_sink {
  void *ctx;
  /* returns 1 if a NEW row, 0 if updated, <0 on error.
   * (This was aspirational until the alert engine landed: the upsert's
   * sqlite3_changes() is 1 for an insert AND an update, so emit() always
   * returned 1. core/intel.c now probes for the uid first, so the
   * distinction is real — alert rules fire on new intel, not on every
   * scheduled re-fetch of an unchanged row.) */
  int (*emit)(struct intel_sink *, const intel_item *);
} intel_sink;

/* UNIFIED ABI: a source is a source. There is NO code distinction between a
 * JapanOSINT collector and an OSINTsaas service — no source_kind, no tag.
 * Every registered source is schedulable (when update_interval_sec>0) AND
 * dispatchable by the OSINT LLM search (any source, pivoted on an entity).
 * Polymorphism is purely via ctx->entity (set on an OSINT pivot, NULL on a
 * scheduled feed run). */

typedef struct source_ctx {
  const char  *source_id;
  const char  *config_json;
  const char  *entity;        /* OSINT-search pivot value; NULL on a scheduled
                               * feed run. Any source may read it. */
  const char  *entity_type;
  http_client *http;
  llm_client  *llm;
  db_handle   *db;
  const char  *tenant_id;
  volatile int *cancel;
  void (*progress)(const char *json);
} source_ctx;

typedef struct source_def {
  const char *id;
  const char *collector;
  const char *name;
  const char *name_ja;
  int  update_interval_sec;   /* >0 scheduled; 0 = on-demand (OSINT/explicit) */
  int  (*run)(const source_ctx *, intel_sink *);
  /* Self-describing metadata — THE place a source describes itself.
   *
   * These fields are the whole of `src_meta` (core/source_registry.h), so a
   * source needs nothing outside its own .c file to appear in /api/status,
   * /api/intel/sources and — when it declares a `layer` — /api/layers.
   * core/source_registry.gen.c is only an OVERLAY of Node-era prose (rich JA
   * names, long descriptions) laid on top per field by
   * core/source_registry_dyn.c; NEW SOURCES MUST NOT BE ADDED TO IT.
   *
   * All fields optional (designated initializers leave them NULL/0). `layer`
   * MUST stay NULL for entity-pivot services so they never appear in
   * /api/layers. Emit nothing rather than a plausible default: a NULL url or
   * description is serialized as JSON null, which a client can tell apart from
   * a guess. */
  const char *category;       /* NULL → "investigation" when synthesized   */
  const char *type;           /* api|dataset|scraped|web_request; NULL→"api" */
  const char *url;            /* canonical/base or internal:// sentinel      */
  const char *description;
  const char *license;
  const char *layer;          /* map layer id; NULL for services             */
  int  free_tier;             /* 1 = free; 0 (default) = paid                */
} source_def;

/* The `category` vocabulary — ONE list, for both registries.
 *
 * The two registries had grown two disjoint vocabularies: 28 values appeared
 * only in source_registry.gen.c rows, 19 only in inline `.category=`
 * literals, overlapping in just 16 — so /api/status already emitted the union
 * of 31 while neither half could be used as a reference. Reconciled by taking
 * THE UNION as canonical rather than by collapsing synonyms: `category` is a
 * free-form string in every client (ios/JapanOsintApp/Models.swift decodes it
 * as `String?` and falls back to "Other"), the values are user-visible group
 * headings, and merging e.g. news/media into intelligence would silently
 * re-bucket live sources for no gain. Nothing is renamed; the list is written
 * down so the next author picks from it instead of inventing a 32nd.
 *
 *   agriculture  breach       classifieds  commercial   crime
 *   culture      cyber        defense      economy      environment
 *   food         geospatial   government   health       industry
 *   infrastructure  intelligence  investigation  marketplace  media
 *   news         ocean        safety       satellite    seismic
 *   social       statistics   telecom      tourism      transport
 *   wildlife
 *
 * (`breach` is also synthesized for the breach_meta catalog rows that
 * /api/status and /api/intel/sources append for operators.) */

/* registry.c */
void               registry_add(const source_def *def);
int                registry_count(void);
/* >0 means some source id is defined twice and the second copy is
 * unreachable via registry_get/--run/the OSINT dispatcher. */
int                registry_duplicate_count(void);
const source_def **registry_all(void);
const source_def  *registry_get(const char *id);

#define REGISTER_SOURCE(defsym) \
  __attribute__((constructor)) static void reg_##defsym(void) { \
    registry_add(&defsym); }

#endif
