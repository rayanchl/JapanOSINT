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
  /* Self-describing metadata. The 420 curated map collectors keep their rich
   * rows in source_registry.gen.c; sources NOT in that table (OSINT services
   * + a few map collectors) carry their metadata here, which src_meta_get()
   * synthesizes into a src_meta. All fields optional (designated initializers
   * leave them NULL/0). `layer` MUST stay NULL for entity-pivot services so
   * they never appear in /api/layers. */
  const char *category;       /* NULL → "investigation" when synthesized   */
  const char *type;           /* api|dataset|scraped|web_request; NULL→"api" */
  const char *url;            /* canonical/base or internal:// sentinel      */
  const char *description;
  const char *license;
  const char *layer;          /* map layer id; NULL for services             */
  int  free_tier;             /* 1 = free; 0 (default) = paid                */
} source_def;

/* registry.c */
void               registry_add(const source_def *def);
int                registry_count(void);
const source_def **registry_all(void);
const source_def  *registry_get(const char *id);

#define REGISTER_SOURCE(defsym) \
  __attribute__((constructor)) static void reg_##defsym(void) { \
    registry_add(&defsym); }

#endif
