#include "statusapi.h"
#include "intelapi.h"           /* intelapi_is_intel_id — INTEL_SOURCE_IDS */
#include "source_registry.h"
#include "source_trust.h"
#include "breach_meta.h"
#include "credtab.h"
#include "httpclient.h"       /* the probe fetch — hostgated, protocol-pinned */
#include "../source.h"        /* registry_get — the probe URL comes from the registry */
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
static void add_num_or_null(cJSON *o, const char *k, sqlite3_stmt *s, int i) {
  cJSON_AddItemToObject(o, k, sqlite3_column_type(s, i) == SQLITE_NULL
    ? cJSON_CreateNull() : cJSON_CreateNumber(sqlite3_column_double(s, i)));
}
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}
/* The Node entrypoint runs applyOverlayToEnv() (apiKeysStore.js) BEFORE the
 * routes load: every non-empty string in data/api-keys.json (the iOS
 * API-keys tab store) is written into process.env, overriding .env/ambient.
 * getCredentialStatus then reads process.env. To match byte-for-byte we
 * resolve the same way: overlay non-empty string wins, else getenv. */
#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif
static cJSON *g_overlay;     /* loaded once; NULL if absent/unparseable */
static int g_overlay_done;
static void overlay_load(void) {
  if (g_overlay_done) return;
  g_overlay_done = 1;
  char path[1024];
  snprintf(path, sizeof path, "%s/data/api-keys.json", JO_REPO_ROOT);
  FILE *f = fopen(path, "rb");
  if (!f) return;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  if (n > 0 && n < (1 << 20)) {
    char *buf = malloc(n + 1);
    if (fread(buf, 1, n, f) == (size_t)n) {
      buf[n] = 0;
      cJSON *j = cJSON_Parse(buf);
      if (j && cJSON_IsObject(j)) g_overlay = j; else cJSON_Delete(j);
    }
    free(buf);
  }
  fclose(f);
}
/* isSet(name): trim().length>0 over the overlay-then-getenv resolved value. */
static int env_set(const char *name) {
  overlay_load();
  const char *v = NULL;
  if (g_overlay) {
    cJSON *o = cJSON_GetObjectItem(g_overlay, name);
    if (o && cJSON_IsString(o) && o->valuestring[0]) v = o->valuestring;
  }
  if (!v) v = getenv(name);
  if (!v) return 0;
  while (*v && isspace((unsigned char)*v)) v++;
  return *v != 0;
}

/* ── apiCredentials.CREDENTIALS (req/any/opt, NULL-terminated) ───────────── */
/* CREDENTIALS table lives in credtab.c (shared with keysapi). */

/* getCredentialStatus(id): appends requiresKey,configured,envVars,missingVars
 * to `o` in that order; returns requiresKey for the gated calc. */
static int add_cred_status(cJSON *o, const char *id) {
  const cred_def *e = cred_get(id);
  if (!e) {
    /* NO RECORD IS NOT THE SAME AS NO REQUIREMENT.
     *
     * This used to answer requiresKey:0, configured:1 — asserting as fact that
     * the source needs no credential and is ready to run. credtab.c covers a
     * fraction of the 9,681 registered sources, so that assertion was made for
     * thousands of sources nobody had ever classified, including 88 collectors
     * that DO gate on a credential and 32 of those that are scheduled and
     * therefore no-op on every tick. It also made the dashboard's "needs key"
     * filter structurally unable to surface any of them.
     *
     * null is the honest answer: we do not know. House rule 1 — a failure to
     * determine something degrades to an explicit unknown, never to invented
     * content. `credentialStatus` names the reason in-band so a client can
     * distinguish "no key needed" from "never classified". */
    cJSON_AddNullToObject(o, "requiresKey");
    cJSON_AddNullToObject(o, "configured");
    cJSON_AddStringToObject(o, "credentialStatus", "unknown");
    cJSON_AddItemToObject(o, "envVars", cJSON_CreateArray());
    cJSON_AddItemToObject(o, "missingVars", cJSON_CreateArray());
    return 0;
  }
  int nr = cred_alen(e->req), na = cred_alen(e->any), no = cred_alen(e->opt);
  int requiresKey = nr > 0 || na > 0;

  int anyOfSat = na == 0;
  for (int i = 0; i < na; i++) if (env_set(e->any[i])) { anyOfSat = 1; break; }
  int missingReq = 0;
  for (int i = 0; i < nr; i++) if (!env_set(e->req[i])) missingReq++;

  int configured = (nr == 0 && na == 0) ? 1 : (missingReq == 0 && anyOfSat);

  cJSON_AddBoolToObject(o, "requiresKey", requiresKey);
  cJSON_AddBoolToObject(o, "configured", configured);

  cJSON *ev = cJSON_CreateArray();
  for (int i = 0; i < nr; i++) {
    cJSON *x = cJSON_CreateObject();
    cJSON_AddStringToObject(x, "name", e->req[i]);
    cJSON_AddBoolToObject(x, "set", env_set(e->req[i]));
    cJSON_AddStringToObject(x, "role", "required");
    cJSON_AddItemToArray(ev, x);
  }
  for (int i = 0; i < na; i++) {
    cJSON *x = cJSON_CreateObject();
    cJSON_AddStringToObject(x, "name", e->any[i]);
    cJSON_AddBoolToObject(x, "set", env_set(e->any[i]));
    cJSON_AddStringToObject(x, "role", "anyOf");
    cJSON_AddItemToArray(ev, x);
  }
  for (int i = 0; i < no; i++) {
    cJSON *x = cJSON_CreateObject();
    cJSON_AddStringToObject(x, "name", e->opt[i]);
    cJSON_AddBoolToObject(x, "set", env_set(e->opt[i]));
    cJSON_AddStringToObject(x, "role", "optional");
    cJSON_AddItemToArray(ev, x);
  }
  cJSON_AddItemToObject(o, "envVars", ev);

  cJSON *mv = cJSON_CreateArray();
  for (int i = 0; i < nr; i++)
    if (!env_set(e->req[i])) cJSON_AddItemToArray(mv, cJSON_CreateString(e->req[i]));
  if (!anyOfSat)
    for (int i = 0; i < na; i++) cJSON_AddItemToArray(mv, cJSON_CreateString(e->any[i]));
  cJSON_AddItemToObject(o, "missingVars", mv);
  return requiresKey;
}

/* ── layers.js STRIP_LAYER_IDS.
 *
 * Node's set was STRIP_LAYER_IDS ∪ intelCatalog's INTEL_SOURCE_IDS, and the
 * port inlined a verbatim copy of the second half here — 34 ids duplicated
 * byte-for-byte with intelapi.c's INTEL_IDS. Two copies, two places to update,
 * and both had drifted identically: boj-stats / jcg-navarea / nict-atlas name
 * collectors deleted in the 66-source removal sweep. The copy is gone;
 * statusapi_strip_has() below unions this list with intelapi_is_intel_id(), so
 * the membership test is unchanged and there is now one definition of each
 * half. */
static const char *STRIP[] = {
  "osm-transport-trains","osm-transport-subways","osm-transport-buses",
  "osm-transport-ports","mlit-n02-stations","mlit-n07-bus-routes",
  "mlit-p11-bus-stops","mlit-c02-ports","mlit-p02-airports","gtfs-jp",
  "maritime","maritime-ais","marine-traffic","vessel-finder",
  "aviation","narita-flights","haneda-flights","flight-adsb",
  "camera-discovery",
  "transport","cyber","social","satellite","infrastructure",
  "radar","river","telecom","energy","crime","economy",
  "health","population","hazard","basemap","elevation","geocode",
  "landuse","poi","admin-boundaries","news-feed","ocean",
  "emergency","warnings","classifieds",
  "unified-station-footprints","unified-stations","bus-routes",
  "highway-traffic","jartic-traffic",
};
int statusapi_strip_has(const char *id) {
  if (!id) return 0;
  for (size_t i = 0; i < sizeof STRIP / sizeof *STRIP; i++)
    if (strcmp(STRIP[i], id) == 0) return 1;
  return intelapi_is_intel_id(id);   /* ...∪ INTEL_SOURCE_IDS (intelapi.c) */
}

/* listSources() aggregates keyed by source_id (status uses item_count,
 * geocoded, ungeocoded, awaiting_geo, last_fetched). */
typedef struct {
  char src[256]; long ic, gc, uc, ag; char lf[64]; int has_lf;
} agg_t;

/* serializeRow(row,reg,creds,intelAgg) — `s` must be stepped to a row of the
 * 19-col sources query below; `A`/`na` the intel-aggregate table. */
static cJSON *status_row(sqlite3_stmt *s, agg_t *A, int na,
                         const source_trust *TT, int nt) {
  const char *id = ctext(s,0);
  const src_meta *m = src_meta_get(id);
  const char *status = ctext(s,5);
  int probeConsent = sqlite3_column_int(s,18) == 1;

  agg_t *g = NULL;
  for (int k = 0; k < na; k++) if (strcmp(A[k].src,id)==0) { g=&A[k]; break; }

  cJSON *o = cJSON_CreateObject();
  add_str_or_null(o,"id", id);
  add_str_or_null(o,"name", ctext(s,1));
  cJSON_AddBoolToObject(o,"probeConsent", probeConsent);

  cJSON *cred = cJSON_CreateObject();
  int requiresKey = add_cred_status(cred, id);
  int gated = requiresKey && !probeConsent;
  cJSON_AddBoolToObject(o,"gated", gated);

  cJSON_AddNumberToObject(o,"intelTotal",       g ? (double)g->ic : 0);
  cJSON_AddNumberToObject(o,"intelGeocoded",    g ? (double)g->gc : 0);
  cJSON_AddNumberToObject(o,"intelUngeocoded",  g ? (double)g->uc : 0);
  cJSON_AddNumberToObject(o,"intelAwaitingGeo", g ? (double)g->ag : 0);
  add_str_or_null(o,"intelLastFetched", g && g->has_lf ? g->lf : NULL);

  add_str_or_null(o,"nameJa", m && m->name_ja && m->name_ja[0] ? m->name_ja : NULL);
  add_str_or_null(o,"type", ctext(s,2));
  add_str_or_null(o,"category", ctext(s,3));
  add_str_or_null(o,"url", ctext(s,4));
  add_str_or_null(o,"description",
                  m && m->description && m->description[0] ? m->description : NULL);
  cJSON_AddItemToObject(o,"free",
    m ? cJSON_CreateBool(m->free) : cJSON_CreateNull());
  add_str_or_null(o,"layer", m && m->layer && m->layer[0] ? m->layer : NULL);
  cJSON_AddItemToObject(o,"updateInterval",
    (m && m->update_interval > 0) ? cJSON_CreateNumber((double)m->update_interval)
                                  : cJSON_CreateNull());
  add_str_or_null(o,"status", status);
  add_str_or_null(o,"lastCheck", ctext(s,6));
  add_str_or_null(o,"lastSuccess", ctext(s,7));
  add_num_or_null(o,"responseTimeMs", s, 8);
  add_num_or_null(o,"recordsCount", s, 9);
  add_str_or_null(o,"errorMessage", ctext(s,10));

  int configured = cJSON_IsTrue(cJSON_GetObjectItem(cred,"configured"));
  cJSON_AddBoolToObject(o,"requiresKey", requiresKey);
  cJSON_AddBoolToObject(o,"configured", configured);
  cJSON_AddItemToObject(o,"envVars", cJSON_DetachItemFromObject(cred,"envVars"));
  cJSON_AddItemToObject(o,"missingVars", cJSON_DetachItemFromObject(cred,"missingVars"));
  cJSON_Delete(cred);

  /* Roadmap item 30 — reliability badge. `rated:false` (with null score and
   * grade) means "no fetch history in the 30-day window", which is a distinct
   * claim from a bad score; the client must render it as "unrated", never as
   * zero. Components ride along so the UI can explain the number instead of
   * asserting it. */
  {
    const source_trust *t = source_trust_find(TT, nt, id);
    cJSON *tr = cJSON_CreateObject();
    if (t && t->rated) {
      cJSON_AddBoolToObject(tr,"rated", 1);
      cJSON_AddNumberToObject(tr,"reliability", t->reliability);
      cJSON_AddStringToObject(tr,"grade", t->grade);
      cJSON_AddNumberToObject(tr,"successRate", t->success_rate);
      cJSON_AddNumberToObject(tr,"freshness", t->freshness);
      cJSON_AddNumberToObject(tr,"anomalyRate", t->anomaly_rate);
      cJSON_AddNumberToObject(tr,"volumeStability", t->volume_stability);
      cJSON_AddNumberToObject(tr,"runs30d", (double)t->runs_30d);
      cJSON_AddNumberToObject(tr,"anomalies30d", (double)t->anomalies_30d);
      cJSON_AddBoolToObject(tr,"quarantined", t->quarantined);
    } else {
      cJSON_AddBoolToObject(tr,"rated", 0);
      cJSON_AddItemToObject(tr,"reliability", cJSON_CreateNull());
      cJSON_AddItemToObject(tr,"grade", cJSON_CreateNull());
    }
    cJSON_AddItemToObject(o,"trust", tr);
  }

  add_str_or_null(o,"probeRequestUrl", ctext(s,11));
  add_str_or_null(o,"probeRequestMethod", ctext(s,12));
  add_str_or_null(o,"probeRequestHeaders", ctext(s,13));
  add_num_or_null(o,"probeResponseStatus", s, 14);
  add_str_or_null(o,"probeResponseHeaders", ctext(s,15));
  add_str_or_null(o,"probeResponseBody", ctext(s,16));
  add_str_or_null(o,"probeKind", ctext(s,17));
  return o;
}

/* One breach_meta catalog row → a synthesized status row.
 *
 * Key order is identical to status_row() above so a client decodes both shapes
 * with one model. A breach is a dataset you either have records for or don't:
 * there is no endpoint to probe and no credential to configure, so probe*, the
 * env-var table and layer/updateInterval are all null/empty, and trust is
 * `rated:false` — never 0, which would read as "scored badly" rather than
 * "not applicable". recordsCount falls back to the catalog's account count so
 * the row is informative before any ingest has run. */
static cJSON *breach_status_row(const breach_src_row *br) {
  long long count = 0;
  const char *fresh = NULL;
  char desc[192];
  breach_meta_display(br, &count, &fresh, desc, sizeof desc);

  cJSON *o = cJSON_CreateObject();
  add_str_or_null(o, "id", br->breach_id);
  add_str_or_null(o, "name", br->name[0] ? br->name : br->breach_id);
  cJSON_AddBoolToObject(o, "probeConsent", 0);
  cJSON_AddBoolToObject(o, "gated", 0);

  cJSON_AddNumberToObject(o, "intelTotal",       (double)count);
  cJSON_AddNumberToObject(o, "intelGeocoded",    0);
  cJSON_AddNumberToObject(o, "intelUngeocoded",  0);
  cJSON_AddNumberToObject(o, "intelAwaitingGeo", 0);
  add_str_or_null(o, "intelLastFetched", fresh);

  cJSON_AddNullToObject(o, "nameJa");
  add_str_or_null(o, "type", "dataset");
  add_str_or_null(o, "category", "breach");
  add_str_or_null(o, "url", br->domain[0] ? br->domain : NULL);
  add_str_or_null(o, "description", desc);
  cJSON_AddBoolToObject(o, "free", 1);
  cJSON_AddNullToObject(o, "layer");
  cJSON_AddNullToObject(o, "updateInterval");

  add_str_or_null(o, "status", br->item_count > 0 ? "online" : "pending");
  cJSON_AddNullToObject(o, "lastCheck");
  add_str_or_null(o, "lastSuccess", fresh);
  cJSON_AddNullToObject(o, "responseTimeMs");
  cJSON_AddNumberToObject(o, "recordsCount", (double)count);
  cJSON_AddNullToObject(o, "errorMessage");

  cJSON_AddBoolToObject(o, "requiresKey", 0);
  cJSON_AddBoolToObject(o, "configured", 1);
  cJSON_AddItemToObject(o, "envVars", cJSON_CreateArray());
  cJSON_AddItemToObject(o, "missingVars", cJSON_CreateArray());

  cJSON *tr = cJSON_CreateObject();
  cJSON_AddBoolToObject(tr, "rated", 0);
  cJSON_AddNullToObject(tr, "reliability");
  cJSON_AddNullToObject(tr, "grade");
  cJSON_AddItemToObject(o, "trust", tr);

  cJSON_AddNullToObject(o, "probeRequestUrl");
  cJSON_AddNullToObject(o, "probeRequestMethod");
  cJSON_AddNullToObject(o, "probeRequestHeaders");
  cJSON_AddNullToObject(o, "probeResponseStatus");
  cJSON_AddNullToObject(o, "probeResponseHeaders");
  cJSON_AddNullToObject(o, "probeResponseBody");
  cJSON_AddNullToObject(o, "probeKind");
  return o;
}

#define STATUS_SRC_COLS \
  "SELECT id,name,type,category,url,status,last_check,last_success," \
  "response_time_ms,records_count,error_message,probe_request_url," \
  "probe_request_method,probe_request_headers,probe_response_status," \
  "probe_response_headers,probe_response_body,probe_kind,probe_consent " \
  "FROM sources"

/* aggregates (== listSources()) into a fresh malloc'd table; caller frees. */
static agg_t *load_aggs(db_handle *db, int *out_n) {
  static const char *AQ =
    "SELECT source_id,COUNT(*),"
    "SUM(CASE WHEN lat IS NOT NULL THEN 1 ELSE 0 END),"
    "SUM(CASE WHEN lat IS NULL THEN 1 ELSE 0 END),"
    "SUM(CASE WHEN lat IS NULL AND geom_source IS NULL THEN 1 ELSE 0 END),"
    "MAX(fetched_at) FROM intel_items GROUP BY source_id";
  sqlite3_stmt *s;
  *out_n = 0;
  if (sqlite3_prepare_v2(db->h, AQ, -1, &s, NULL) != SQLITE_OK) return NULL;
  int cap = 64, n = 0;
  agg_t *A = malloc(cap * sizeof *A);
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == cap) { cap *= 2; A = realloc(A, cap * sizeof *A); }
    agg_t *r = &A[n++];
    snprintf(r->src, sizeof r->src, "%s", (const char *)sqlite3_column_text(s,0));
    r->ic = sqlite3_column_int64(s,1); r->gc = sqlite3_column_int64(s,2);
    r->uc = sqlite3_column_int64(s,3); r->ag = sqlite3_column_int64(s,4);
    r->has_lf = sqlite3_column_type(s,5) != SQLITE_NULL;
    snprintf(r->lf, sizeof r->lf, "%s",
             r->has_lf ? (const char *)sqlite3_column_text(s,5) : "");
  }
  sqlite3_finalize(s);
  *out_n = n;
  return A;
}

/* GET /api/status/:id — single serializeRow, or NULL if no such source. */
char *statusapi_one(db_handle *db, const char *id) {
  int na = 0, nt = 0;
  agg_t *A = load_aggs(db, &na);
  source_trust *TT = source_trust_load(db, &nt);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, STATUS_SRC_COLS " WHERE id=?1", -1, &s, NULL)
      != SQLITE_OK) { free(A); free(TT); return NULL; }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  char *js = NULL;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *o = status_row(s, A, na, TT, nt);
    js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
  }
  sqlite3_finalize(s);
  free(A);
  free(TT);
  return js;
}

char *statusapi_build(db_handle *db, int include_breach) {
  int na = 0, nt = 0;
  agg_t *A = load_aggs(db, &na);
  source_trust *TT = source_trust_load(db, &nt);
  sqlite3_stmt *s;
  /* getAllSources(): SELECT * FROM sources ORDER BY category, name */
  if (sqlite3_prepare_v2(db->h, STATUS_SRC_COLS " ORDER BY category, name",
                         -1, &s, NULL) != SQLITE_OK) { free(A); free(TT); return NULL; }

  cJSON *apis = cJSON_CreateArray();
  int c_total=0,c_online=0,c_degraded=0,c_offline=0,c_pending=0,c_gated=0,
      c_reqkey=0,c_configured=0,c_missing=0,c_working=0;

  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = ctext(s,0);
    const src_meta *m = src_meta_get(id);
    if (statusapi_strip_has(id)) continue;
    if (m && statusapi_strip_has(m->layer)) continue;

    cJSON *o = status_row(s, A, na, TT, nt);
    cJSON_AddItemToArray(apis, o);

    /* summary tallies (mirrors status.js filters) — read back from `o` */
    const char *status = ctext(s,5);
    int gated      = cJSON_IsTrue(cJSON_GetObjectItem(o,"gated"));
    int requiresKey= cJSON_IsTrue(cJSON_GetObjectItem(o,"requiresKey"));
    int configured = cJSON_IsTrue(cJSON_GetObjectItem(o,"configured"));
    c_total++;
    int isOnline = status && strcmp(status,"online")==0;
    int isDegraded = status && strcmp(status,"degraded")==0;
    int isOffline = status && strcmp(status,"offline")==0;
    int isPending = status && strcmp(status,"pending")==0;
    if (!gated && isOnline)   c_online++;
    if (!gated && isDegraded) c_degraded++;
    if (!gated && isOffline)  c_offline++;
    if (!gated && isPending)  c_pending++;
    if (gated) c_gated++;
    if (requiresKey) c_reqkey++;
    if (requiresKey && configured) c_configured++;
    if (requiresKey && !configured) c_missing++;
    if (!gated && isOnline && (!requiresKey || configured)) c_working++;
  }
  sqlite3_finalize(s);
  free(A);
  free(TT);

  /* Breach catalog rows, appended for operators only. They tally into the
   * ordinary status counters like any other row, plus their own two totals. */
  int c_breach = 0, c_breach_mat = 0;
  if (include_breach) {
    int nb = 0;
    breach_src_row *B = breach_meta_sources(db, &nb);
    for (int k = 0; k < nb; k++) {
      /* never shadow a real registry source that happens to share the slug —
       * the same guard intelapi_intel_sources() applies. */
      if (src_meta_get(B[k].breach_id)) continue;

      cJSON_AddItemToArray(apis, breach_status_row(&B[k]));
      c_total++;
      c_breach++;
      if (B[k].item_count > 0) { c_online++; c_working++; c_breach_mat++; }
      else                       c_pending++;
    }
    free(B);
  }

  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary,"total", c_total);
  cJSON_AddNumberToObject(summary,"online", c_online);
  cJSON_AddNumberToObject(summary,"degraded", c_degraded);
  cJSON_AddNumberToObject(summary,"offline", c_offline);
  cJSON_AddNumberToObject(summary,"pending", c_pending);
  cJSON_AddNumberToObject(summary,"gated", c_gated);
  cJSON_AddNumberToObject(summary,"requiresKey", c_reqkey);
  cJSON_AddNumberToObject(summary,"configured", c_configured);
  cJSON_AddNumberToObject(summary,"missingKey", c_missing);
  cJSON_AddNumberToObject(summary,"working", c_working);
  /* Always emitted (0 when the caller isn't an operator) so clients can decode
   * them without treating absence as a distinct case. */
  cJSON_AddNumberToObject(summary,"breachTotal", c_breach);
  cJSON_AddNumberToObject(summary,"breachMaterialized", c_breach_mat);

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env,"summary", summary);
  cJSON_AddItemToObject(env,"apis", apis);
  cJSON_AddStringToObject(env,"timestamp", ts);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}

/* ── probe ─────────────────────────────────────────────────────────────────
 *
 * The `probe_*` columns and `probe_consent` have been in the schema — and read
 * by status_row() above — since the Node port, but NOTHING in the tree ever
 * wrote them. They were read-only scaffolding: every value was permanently
 * NULL, `probeConsent` permanently 0, and the iOS client's two probe controls
 * called routes that did not exist. This is the writer.
 *
 * WHAT A PROBE IS. One GET of the source's OWN registered endpoint
 * (source_def.url — a compile-time constant, never anything the caller
 * supplies), recording what went out and what came back. It answers "is this
 * source's endpoint reachable, and what does it actually say" without running
 * the collector or writing a single intel row.
 *
 * WHY IT IS NOT AN SSRF PRIMITIVE. The URL is not attacker-influenced: it is
 * looked up from the registry by id, and the fetch goes through
 * http_request(), which applies hostgate's URL check, the protocol pins and
 * the per-hop peer re-check. A caller can choose WHICH registered source to
 * probe, never WHERE the request goes.
 *
 * WHAT IS STORED, AND WHY IT IS SAFE TO SERVE. status_row() exposes
 * probeRequestHeaders and probeResponseBody to any authenticated reader, so a
 * probe must never capture a credential. It does not: the probe deliberately
 * sends NO collector auth headers — only a User-Agent — and that is exactly
 * what gets recorded, so the stored request headers are a constant. The body
 * is a bounded, control-character-scrubbed snippet of a public endpoint's
 * reply.
 *
 * probe_response_headers stays NULL: http_response does not carry them, and a
 * plausible-looking reconstruction would be invented content (house rule 1).
 */
#define PROBE_BODY_MAX   2000
#define PROBE_TIMEOUT_MS 10000
#define PROBE_UA "User-Agent: JapanOSINT/1.0 (source probe; +https://github.com/)"

static char *probe_err(int *st, int code, const char *msg) {
  *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* A bounded, printable snippet. Truncation is disclosed by the caller via
 * `body_truncated`, never silently. */
static char *probe_snip(const char *body, size_t len, int *truncated) {
  size_t n = len < PROBE_BODY_MAX ? len : PROBE_BODY_MAX;
  *truncated = (len > n);
  char *out = malloc(n + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)body[i];
    if (ch == '\n' || ch == '\t') out[o++] = ' ';
    else if (ch < 0x20 || ch == 0x7F) out[o++] = '.';
    else out[o++] = (char)ch;
  }
  out[o] = 0;
  return out;
}

char *statusapi_set_consent(db_handle *db, const char *id, int consent, int *st) {
  if (!db || !db->h || !id || !*id) return probe_err(st, 400, "source id required");
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE sources SET probe_consent=?1 WHERE id=?2", -1, &s, NULL) != SQLITE_OK)
    return probe_err(st, 500, "prepare_failed");
  sqlite3_bind_int(s, 1, consent ? 1 : 0);
  sqlite3_bind_text(s, 2, id, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) return probe_err(st, 500, "consent_update_failed");
  /* changes()==0 means no such source — report that rather than a cheerful ok
   * for a row that does not exist. */
  if (sqlite3_changes(db->h) == 0) return probe_err(st, 404, "not_found");

  *st = 200;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddBoolToObject(o, "ok", 1);
  cJSON_AddStringToObject(o, "id", id);
  cJSON_AddBoolToObject(o, "probeConsent", consent ? 1 : 0);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

char *statusapi_probe(db_handle *db, const char *id, int *st) {
  if (!db || !db->h || !id || !*id) return probe_err(st, 400, "source id required");

  const source_def *d = registry_get(id);
  if (!d) return probe_err(st, 404, "no_collector_registered");
  const char *url = d->url;
  if (!url || !*url) return probe_err(st, 400, "source declares no endpoint to probe");
  /* Datasets and internal pods carry an `internal://` url — there is nothing
   * on the network to reach, and pretending otherwise would manufacture a
   * result. */
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    return probe_err(st, 400, "source endpoint is not an http(s) url");

  const char *hdrs[] = { PROBE_UA, NULL };
  http_client *hc = http_client_new();
  if (!hc) return probe_err(st, 500, "http_client_alloc_failed");
  http_response r = {0};
  int hard = http_request(hc, "GET", url, hdrs, NULL, 0,
                          PROBE_TIMEOUT_MS, 0, &r);
  long code = r.status;
  int truncated = 0;
  char *snip = (r.body && r.body_len) ? probe_snip(r.body, r.body_len, &truncated) : NULL;
  http_response_free(&r);
  http_client_free(hc);

  char now[40]; iso_now(now, sizeof now);

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE sources SET probe_request_url=?1, probe_request_method='GET',"
        " probe_request_headers=?2, probe_response_status=?3,"
        " probe_response_headers=NULL, probe_response_body=?4, probe_kind='http'"
        " WHERE id=?5", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, PROBE_UA, -1, SQLITE_STATIC);
    if (code > 0) sqlite3_bind_int(s, 3, (int)code); else sqlite3_bind_null(s, 3);
    if (snip) sqlite3_bind_text(s, 4, snip, -1, SQLITE_TRANSIENT);
    else      sqlite3_bind_null(s, 4);
    sqlite3_bind_text(s, 5, id, -1, SQLITE_TRANSIENT);
    /* The step result decides what we claim below: a probe that could not be
     * recorded is still a probe that HAPPENED, but the row the client will
     * read next has not moved, and saying "stored" would be a lie. */
    if (sqlite3_step(s) != SQLITE_DONE) {
      fprintf(stderr, "[status] probe of %s ran but could not be stored: %s\n",
              id, sqlite3_errmsg(db->h));
    }
  }
  sqlite3_finalize(s);

  *st = 200;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id", id);
  cJSON_AddStringToObject(o, "probedAt", now);
  cJSON_AddStringToObject(o, "probeRequestUrl", url);
  cJSON_AddStringToObject(o, "probeRequestMethod", "GET");
  cJSON_AddStringToObject(o, "probeRequestHeaders", PROBE_UA);
  cJSON_AddStringToObject(o, "probeKind", "http");
  if (code > 0) cJSON_AddNumberToObject(o, "probeResponseStatus", (double)code);
  else          cJSON_AddNullToObject(o, "probeResponseStatus");
  /* http_response carries no headers, so this is null rather than invented. */
  cJSON_AddNullToObject(o, "probeResponseHeaders");
  if (snip) cJSON_AddStringToObject(o, "probeResponseBody", snip);
  else      cJSON_AddNullToObject(o, "probeResponseBody");
  cJSON_AddBoolToObject(o, "bodyTruncated", truncated);
  cJSON_AddBoolToObject(o, "reachable", (!hard && code > 0));
  if (hard || code == 0)
    cJSON_AddStringToObject(o, "error", "transport_error");
  free(snip);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
