#include "statusapi.h"
#include "source_registry.h"
#include "credtab.h"
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
    cJSON_AddBoolToObject(o, "requiresKey", 0);
    cJSON_AddBoolToObject(o, "configured", 1);
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

/* ── layers.js STRIP_LAYER_IDS (static list + intelCatalog INTEL_SOURCE_IDS) */
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
  /* ...INTEL_SOURCE_IDS */
  "certstream-jp","bird-makeup-jp","chan-5ch","fofa-jp",
  "github-leaks-jp","grayhat-buckets","greynoise-jp","hatena-bookmark",
  "houjin-bangou","mercari-trending","misskey-timeline","note-com-trending",
  "urlscan-jp","wayback-jp",
  "edinet-filings","boj-stats","data-go-jp-ckan","egov-laws",
  "geospatial-jp-ckan","kyodo-rss","nhk-world-rss","jcg-navarea","nict-atlas",
  "ripestat-jp","wifi-hotspots-jcfw","wifi-hotspots-freespot",
  "jp-news-rss","nhk-news-rss","yahoo-news-jp-rss","ipa-alerts",
  "jpcert-alerts","phishing-feeds-jp","sans-isc-feeds","my-jvn",
};
int statusapi_strip_has(const char *id) {
  if (!id) return 0;
  for (size_t i = 0; i < sizeof STRIP / sizeof *STRIP; i++)
    if (strcmp(STRIP[i], id) == 0) return 1;
  return 0;
}

/* listSources() aggregates keyed by source_id (status uses item_count,
 * geocoded, ungeocoded, awaiting_geo, last_fetched). */
typedef struct {
  char src[256]; long ic, gc, uc, ag; char lf[64]; int has_lf;
} agg_t;

/* serializeRow(row,reg,creds,intelAgg) — `s` must be stepped to a row of the
 * 19-col sources query below; `A`/`na` the intel-aggregate table. */
static cJSON *status_row(sqlite3_stmt *s, agg_t *A, int na) {
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

  add_str_or_null(o,"probeRequestUrl", ctext(s,11));
  add_str_or_null(o,"probeRequestMethod", ctext(s,12));
  add_str_or_null(o,"probeRequestHeaders", ctext(s,13));
  add_num_or_null(o,"probeResponseStatus", s, 14);
  add_str_or_null(o,"probeResponseHeaders", ctext(s,15));
  add_str_or_null(o,"probeResponseBody", ctext(s,16));
  add_str_or_null(o,"probeKind", ctext(s,17));
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
  int na = 0;
  agg_t *A = load_aggs(db, &na);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, STATUS_SRC_COLS " WHERE id=?1", -1, &s, NULL)
      != SQLITE_OK) { free(A); return NULL; }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  char *js = NULL;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *o = status_row(s, A, na);
    js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
  }
  sqlite3_finalize(s);
  free(A);
  return js;
}

char *statusapi_build(db_handle *db) {
  int na = 0;
  agg_t *A = load_aggs(db, &na);
  sqlite3_stmt *s;
  /* getAllSources(): SELECT * FROM sources ORDER BY category, name */
  if (sqlite3_prepare_v2(db->h, STATUS_SRC_COLS " ORDER BY category, name",
                         -1, &s, NULL) != SQLITE_OK) { free(A); return NULL; }

  cJSON *apis = cJSON_CreateArray();
  int c_total=0,c_online=0,c_degraded=0,c_offline=0,c_pending=0,c_gated=0,
      c_reqkey=0,c_configured=0,c_missing=0,c_working=0;

  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = ctext(s,0);
    const src_meta *m = src_meta_get(id);
    if (statusapi_strip_has(id)) continue;
    if (m && statusapi_strip_has(m->layer)) continue;

    cJSON *o = status_row(s, A, na);
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

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env,"summary", summary);
  cJSON_AddItemToObject(env,"apis", apis);
  cJSON_AddStringToObject(env,"timestamp", ts);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}
