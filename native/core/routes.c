/* core/routes.c — the HTTP route table (see routes.h for the contract).
 *
 * RULES FOR EDITING THIS FILE
 *   - A route is served only if it has a row here. Adding a matcher to
 *     httpd.c's chain without a row means the request is refused 404 before the
 *     chain runs; tests/unit/test_routes.c also fails the build by name.
 *   - Rows are matched in order, first hit wins: put a more specific pattern
 *     BEFORE the broader one that would otherwise swallow it (the same
 *     ordering discipline the chain needs, now checked in one place).
 *   - The gate here is the FLOOR. A handler may demand more (a role for one
 *     method, an owner of one object, the operator for a breach uid) and
 *     those finer checks stay in the handler.
 *   - Every /api/admin/* and /api/db/* row must be RG_OPERATOR; the unit test
 *     fails otherwise.
 */
#include "routes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../third_party/cJSON.h"

#define M_RW  (RM_PUT | RM_POST)
#define M_WR  (RM_PUT | RM_POST | RM_DELETE)

#define R(m, p, g, r, f, n)        { (m), (p), (g), (r), (f), (n), NULL }
#define RD(m, p, g, r, f, n, deny) { (m), (p), (g), (r), (f), (n), (deny) }

static const route_row ROUTES[] = {
  /* ---- pre-auth: the ONLY rows that need no credential ---------------- */
  R(RM_ANY,  "/api/health",              RG_PUBLIC, RR_LOOP, 0, "health"),
  /* The request id is an unguessable capability (EventSource cannot send
   * Authorization); a bearer token, when present, only scopes the stream. */
  R(RM_ANY,  "/api/search/stream/:id",   RG_PUBLIC, RR_LOOP, 0, "search-stream"),
  /* Mounted outside /api: it exists for when token verification is down. */
  R(RM_ANY,  "/admin/break-glass/login", RG_PUBLIC, RR_LOOP, 0, "break-glass-login"),

  /* ---- search --------------------------------------------------------- */
  R(RM_ANY,  "/api/search/analyze",      RG_TENANT, RR_LOOP,   0, "search-analyze"),
  R(RM_ANY,  "/api/search/suggest",      RG_AUTH,   RR_WORKER, 0, "search-suggest"),
  R(RM_ANY,  "/api/search/runs",         RG_TENANT, RR_LOOP,   0, "search-runs"),
  R(RM_ANY,  "/api/search/results/:id",  RG_TENANT, RR_LOOP,   0, "search-result"),

  /* ---- sources / collectors (registry pages) -------------------------- */
  /* GET reads the policy; PUT/POST sets it for the whole deployment. */
  R(M_RW,    "/api/sources/cadence",     RG_OPERATOR, RR_LOOP, 0, "cadence-set"),
  R(RM_ANY,  "/api/sources/cadence",     RG_AUTH,     RR_LOOP, 0, "cadence-get"),
  R(RM_ANY,  "/api/sources/stats",       RG_AUTH,   RR_LOOP,   0, "sources-stats"),
  R(RM_ANY,  "/api/sources/semantic",    RG_AUTH,   RR_WORKER, 0, "sources-semantic"),
  R(RM_ANY,  "/api/sources/categories",  RG_AUTH,   RR_LOOP,   0, "sources-categories"),
  R(RM_ANY,  "/api/sources/search",      RG_AUTH,   RR_WORKER, 0, "sources-search"),
  R(RM_ANY,  "/api/collectors",          RG_AUTH,   RR_WORKER, 0, "collectors"),
  R(RM_ANY,  "/api/collectors/:key",     RG_AUTH,   RR_WORKER, 0, "collector"),
  R(RM_ANY,  "/api/sources/:id/detail",  RG_AUTH,   RR_WORKER, 0, "source-detail"),
  R(RM_ANY,  "/api/sources/:id/summary", RG_AUTH,   RR_WORKER, 0, "source-summary"),
  /* Settings: the key set is closed, writing is an operator decision. Any
   * other method is 405 (the handler's own answer). */
  RD(RM_PUT, "/api/sources/:id/settings", RG_OPERATOR, RR_LOOP, 0,
     "source-settings-put", "{\"error\":\"operator_required\"}"),
  R(RM_GET,  "/api/sources/:id/settings", RG_AUTH,     RR_LOOP, RF_STRICT,
     "source-settings-get"),
  R(RM_ANY,  "/api/sources",             RG_AUTH,   RR_LOOP,   0, "sources-list"),
  R(RM_ANY,  "/api/layers",              RG_AUTH,   RR_LOOP,   0, "layers"),
  R(RM_ANY,  "/api/status",              RG_AUTH,   RR_WORKER, 0, "status"),
  R(RM_GET,  "/api/metrics",             RG_OPERATOR, RR_LOOP, RF_STRICT, "metrics"),
  R(RM_ANY,  "/api/intel/sources",       RG_AUTH,   RR_WORKER, 0, "intel-sources"),

  /* ---- intel ---------------------------------------------------------- */
  R(RM_ANY,  "/api/intel/items",         RG_TENANT, RR_LOOP,   0, "intel-items"),
  /* Item sub-routes: before the /:uid catch-all. Non-operators are
   * workspace-scoped inside the handler; breach rows stay operator-only. */
  R(RM_ANY,  "/api/intel/items/:uid/evidence", RG_AUTH,   RR_LOOP,   0, "item-evidence"),
  R(RM_ANY,  "/api/intel/items/:uid/entities", RG_AUTH,   RR_LOOP,   0, "item-entities"),
  R(RM_ANY,  "/api/intel/items/:uid/media",    RG_AUTH,   RR_LOOP,   0, "item-media"),
  R(RM_ANY,  "/api/intel/items/:uid/reveal",   RG_OPERATOR, RR_LOOP, 0, "item-reveal"),
  R(RM_ANY,  "/api/intel/items/:uid/similar",  RG_TENANT, RR_WORKER, 0, "item-similar"),
  R(RM_ANY,  "/api/stories/*",           RG_TENANT, RR_WORKER, 0, "story"),
  R(RM_ANY,  "/api/intel/items/*",       RG_TENANT, RR_LOOP,   0, "item"),
  /* GET reads how often a source runs; PUT/POST/DELETE set or clear it. */
  R(M_WR,    "/api/sources/:id/interval", RG_OPERATOR, RR_LOOP, 0, "interval-set"),
  R(RM_ANY,  "/api/sources/:id/interval", RG_AUTH,     RR_LOOP, 0, "interval-get"),
  R(RM_ANY,  "/api/sources/:id/schedule", RG_OPERATOR, RR_LOOP, 0, "schedule-set"),
  R(RM_ANY,  "/api/sources/:id/logs",    RG_AUTH,   RR_LOOP,   0, "source-logs"),
  R(RM_ANY,  "/api/sources/:id",         RG_AUTH,   RR_LOOP,   0, "source"),
  R(RM_POST, "/api/status/:id/probe",    RG_OPERATOR, RR_WORKER, RF_STRICT, "status-probe"),
  R(RM_POST, "/api/status/:id/consent",  RG_OPERATOR, RR_LOOP,   RF_STRICT, "status-consent"),
  R(RM_ANY,  "/api/status/:id",          RG_AUTH,   RR_LOOP,   0, "status-one"),
  R(RM_ANY,  "/api/layers/:id/geojson",  RG_AUTH,   RR_WORKER, 0, "layer-geojson"),
  R(RM_ANY,  "/api/intel/semantic",      RG_TENANT, RR_WORKER, 0, "intel-semantic"),
  R(RM_ANY,  "/api/intel/search",        RG_TENANT, RR_LOOP,   0, "intel-search"),
  /* Runs a collector now: operator, or owner/admin of a team workspace. The
   * handler adds the pod rule and the per-user budget. */
  R(RM_POST, "/api/intel/sources/:id/run", RG_RUN,  RR_WORKER, RF_STRICT, "source-run"),
  R(RM_ANY,  "/api/follow/recent",       RG_OPERATOR, RR_LOOP, 0, "follow-recent"),

  /* ---- /api/admin/* and /api/db/*: platform operator, every row ------- */
  R(RM_GET,  "/api/admin/routes",        RG_OPERATOR, RR_LOOP,   RF_STRICT, "admin-routes"),
  R(RM_ANY,  "/api/admin/translate/backfill",     RG_OPERATOR, RR_LOOP,   0, "admin-translate-backfill"),
  R(RM_ANY,  "/api/admin/layers/review",          RG_OPERATOR, RR_WORKER, 0, "admin-layers-review"),
  R(RM_ANY,  "/api/admin/cameras/duplicates",     RG_OPERATOR, RR_WORKER, 0, "admin-camera-duplicates"),
  R(RM_ANY,  "/api/admin/ffmpeg/capabilities",    RG_OPERATOR, RR_LOOP,   0, "admin-ffmpeg-capabilities"),
  R(RM_ANY,  "/api/admin/thresholds",             RG_OPERATOR, RR_LOOP,   0, "admin-thresholds"),
  R(RM_ANY,  "/api/admin/thresholds/:key",        RG_OPERATOR, RR_LOOP,   0, "admin-threshold"),
  R(RM_ANY,  "/api/admin/restart",                RG_OPERATOR, RR_LOOP,   0, "admin-restart"),
  R(RM_ANY,  "/api/admin/maintenance",            RG_OPERATOR, RR_LOOP,   0, "admin-maintenance"),
  R(RM_ANY,  "/api/admin/maintenance/lists/:name", RG_OPERATOR, RR_LOOP,  0, "admin-maintenance-list"),
  R(RM_ANY,  "/api/admin/maintenance/source/:id/fetch_log", RG_OPERATOR, RR_LOOP, 0, "admin-maintenance-fetch-log"),
  R(RM_ANY,  "/api/admin/maintenance/source/:id/anomalies", RG_OPERATOR, RR_LOOP, 0, "admin-maintenance-anomalies"),
  R(RM_ANY,  "/api/admin/maintenance/source/:id/repairs",   RG_OPERATOR, RR_LOOP, 0, "admin-maintenance-repairs"),
  R(RM_ANY,  "/api/admin/maintenance/source/:id", RG_OPERATOR, RR_LOOP,   0, "admin-maintenance-source"),
  R(RM_ANY,  "/api/admin/repairs/:id/approve",    RG_OPERATOR, RR_LOOP,   0, "admin-repair-approve"),
  R(RM_ANY,  "/api/admin/repairs/:id/reject",     RG_OPERATOR, RR_LOOP,   0, "admin-repair-reject"),
  R(RM_ANY,  "/api/admin/repairs/:id",            RG_OPERATOR, RR_LOOP,   0, "admin-repair"),
  R(RM_ANY,  "/api/admin/sources/:id/unquarantine",   RG_OPERATOR, RR_LOOP, 0, "admin-source-unquarantine"),
  R(RM_ANY,  "/api/admin/sources/:id/capture-evidence", RG_OPERATOR, RR_LOOP, 0, "admin-source-capture-evidence"),
  R(RM_ANY,  "/api/admin/cameras/:id/capture-stills", RG_OPERATOR, RR_LOOP, 0, "admin-camera-capture-stills"),
  R(RM_ANY,  "/api/admin/anomalies/:id/requeue",  RG_OPERATOR, RR_LOOP,   0, "admin-anomaly-requeue"),
  R(RM_ANY,  "/api/admin/breach/ingest",          RG_OPERATOR, RR_LOOP,   0, "admin-breach-ingest"),
  R(RM_ANY,  "/api/admin/breach/fetch",           RG_OPERATOR, RR_LOOP,   0, "admin-breach-fetch"),
  R(RM_ANY,  "/api/admin/breach/catalog/preview", RG_OPERATOR, RR_LOOP,   0, "admin-breach-catalog-preview"),
  R(RM_ANY,  "/api/admin/breach/catalog/load",    RG_OPERATOR, RR_LOOP,   0, "admin-breach-catalog-load"),
  R(RM_ANY,  "/api/admin/breach/jobs",            RG_OPERATOR, RR_LOOP,   0, "admin-breach-jobs"),
  R(RM_ANY,  "/api/admin/breach/jobs/:id",        RG_OPERATOR, RR_LOOP,   0, "admin-breach-job"),
  R(RM_ANY,  "/api/admin/pagination/probe",       RG_OPERATOR, RR_LOOP,   0, "admin-pagination-probe"),
  R(RM_ANY,  "/api/admin/pagination/jobs",        RG_OPERATOR, RR_LOOP,   0, "admin-pagination-jobs"),
  R(RM_ANY,  "/api/admin/pagination/jobs/:id",    RG_OPERATOR, RR_LOOP,   0, "admin-pagination-job"),
  R(RM_ANY,  "/api/db/tables",                    RG_OPERATOR, RR_LOOP,   0, "db-tables"),
  R(RM_ANY,  "/api/db/tables/*",                  RG_OPERATOR, RR_LOOP,   0, "db-table"),
  R(RM_ANY,  "/api/db/scheduler",                 RG_OPERATOR, RR_LOOP,   0, "db-scheduler"),

  /* ---- geo / reference data ------------------------------------------ */
  R(RM_ANY,  "/api/geocode",             RG_AUTH,   RR_WORKER, 0, "geocode"),
  R(RM_ANY,  "/api/geocode/reverse",     RG_AUTH,   RR_WORKER, 0, "geocode-reverse"),
  R(RM_ANY,  "/api/plateau/tilesets",    RG_AUTH,   RR_WORKER, 0, "plateau-tilesets"),
  R(RM_GET,  "/api/isochrone",           RG_AUTH,   RR_WORKER, RF_STRICT, "isochrone"),
  R(RM_GET,  "/api/vocab",               RG_TENANT, RR_LOOP,   RF_STRICT, "vocab"),
  R(RM_ANY,  "/api/breach/search",       RG_OPERATOR, RR_WORKER, 0, "breach-search"),

  /* ---- alerts --------------------------------------------------------- */
  /* Backtests a predicate over tenant intel: a viewer may not. */
  R(RM_POST, "/api/alerts/preview",      RG_ROLE_ANALYST, RR_LOOP, RF_STRICT, "alerts-preview"),
  R(RM_GET,  "/api/alert-events/:id/deliveries", RG_TENANT, RR_LOOP, RF_STRICT, "alert-event-deliveries"),
  R(RM_ANY,  "/api/alert-events",        RG_TENANT, RR_LOOP,   0, "alert-events"),
  R(RM_ANY,  "/api/alert-events/*",      RG_TENANT, RR_LOOP,   0, "alert-event"),
  R(RM_ANY,  "/api/alerts",              RG_TENANT, RR_LOOP,   0, "alerts"),
  R(RM_ANY,  "/api/alerts/*",            RG_TENANT, RR_LOOP,   0, "alert"),

  /* ---- exports, timeline, cases, annotations -------------------------- */
  R(RM_GET,  "/api/export/*",            RG_TENANT, RR_WORKER, RF_STRICT, "export"),
  R(RM_GET,  "/api/timeline",            RG_TENANT, RR_LOOP,   RF_STRICT, "timeline"),
  R(RM_POST, "/api/cases/:id/report",    RG_TENANT, RR_WORKER, RF_STRICT, "case-report"),
  R(RM_ANY,  "/api/cases",               RG_TENANT, RR_LOOP,   0, "cases"),
  R(RM_ANY,  "/api/cases/*",             RG_TENANT, RR_LOOP,   0, "case"),
  R(RM_ANY,  "/api/annotations",         RG_TENANT, RR_LOOP,   0, "annotations"),
  R(RM_ANY,  "/api/annotations/*",       RG_TENANT, RR_LOOP,   0, "annotation"),

  /* ---- evidence, media, cameras -------------------------------------- */
  R(RM_POST, "/api/evidence/gc",         RG_OPERATOR, RR_LOOP, RF_STRICT, "evidence-gc"),
  RD(RM_ANY, "/api/evidence/verify",     RG_OPERATOR, RR_LOOP, 0, "evidence-verify",
     "{\"error\":\"forbidden\"}"),
  /* The raw bytes are operator-only, decided inside evidence_raw(). */
  R(RM_ANY,  "/api/evidence/:id/raw",    RG_AUTH,   RR_LOOP,   0, "evidence-raw"),
  R(RM_ANY,  "/api/media/capabilities",  RG_OPERATOR, RR_LOOP, 0, "media-capabilities"),
  R(RM_ANY,  "/api/camera-stills/capabilities", RG_OPERATOR, RR_LOOP, 0, "camera-stills-capabilities"),
  R(RM_ANY,  "/api/cameras/visual-search", RG_AUTH, RR_WORKER, 0, "camera-visual-search"),
  R(RM_POST, "/api/cameras/visual-search/by-image", RG_TENANT, RR_WORKER, RF_STRICT, "camera-by-image"),
  R(RM_ANY,  "/api/cameras/:id/stills",  RG_AUTH,   RR_LOOP,   0, "camera-stills"),
  R(RM_ANY,  "/api/camera-stills/:id/raw", RG_AUTH, RR_LOOP,   0, "camera-still-raw"),

  /* ---- workspace-owned objects --------------------------------------- */
  R(RM_ANY,  "/api/aoi",                 RG_TENANT, RR_LOOP,   0, "aoi"),
  R(RM_ANY,  "/api/aoi/*",               RG_TENANT, RR_LOOP,   0, "aoi-item"),
  R(RM_ANY,  "/api/watchlists",          RG_TENANT, RR_LOOP,   0, "watchlists"),
  R(RM_ANY,  "/api/watchlists/*",        RG_TENANT, RR_LOOP,   0, "watchlist"),
  R(RM_POST, "/api/docmeta",             RG_AUTH,   RR_WORKER, RF_STRICT, "docmeta"),
  /* uploadapi() refuses a viewer for every method, reads included: an upload
   * in flight is unreleased evidence. begin / part / commit run on a worker. */
  R(RM_ANY,  "/api/uploads",             RG_ROLE_ANALYST, RR_WORKER, 0, "uploads"),
  R(RM_ANY,  "/api/uploads/*",           RG_ROLE_ANALYST, RR_WORKER, 0, "upload"),
  R(RM_ANY,  "/api/breach-monitors",     RG_TENANT, RR_LOOP,   0, "breach-monitors"),
  R(RM_ANY,  "/api/breach-monitors/*",   RG_TENANT, RR_LOOP,   0, "breach-monitor"),
  R(RM_ANY,  "/api/saved-searches",      RG_TENANT, RR_LOOP,   0, "saved-searches"),
  R(RM_ANY,  "/api/saved-searches/*",    RG_TENANT, RR_LOOP,   0, "saved-search"),
  R(RM_ANY,  "/api/search-history",      RG_TENANT, RR_LOOP,   0, "search-history"),
  /* Carries view state only and must never carry authorization: no tenant. */
  R(RM_ANY,  "/api/permalink",           RG_AUTH,   RR_LOOP,   0, "permalink"),
  R(RM_ANY,  "/api/permalink/*",         RG_AUTH,   RR_LOOP,   0, "permalink-token"),
  R(RM_ANY,  "/api/members",             RG_TENANT, RR_LOOP,   0, "members"),
  R(RM_ANY,  "/api/members/*",           RG_TENANT, RR_LOOP,   0, "member"),

  /* ---- credentials ---------------------------------------------------- */
  /* /api/keys* is the PLATFORM credential store. */
  R(RM_ANY,  "/api/keys",                RG_OPERATOR, RR_LOOP, 0, "keys"),
  R(RM_ANY,  "/api/keys/*",              RG_OPERATOR, RR_LOOP, 0, "key"),
  /* A workspace's own keys: tenant admins keep BYOK self-service, but the
   * DECRYPTED value of one (GET /:name) is an operator read. */
  R(RM_GET,  "/api/tenant-keys/policy",  RG_TENANT,   RR_LOOP, 0, "tenant-keys-policy"),
  R(RM_GET,  "/api/tenant-keys/:name",   RG_OPERATOR, RR_LOOP, 0, "tenant-key-reveal"),
  R(RM_ANY,  "/api/tenant-keys",         RG_TENANT,   RR_LOOP, 0, "tenant-keys"),
  R(RM_ANY,  "/api/tenant-keys/*",       RG_TENANT,   RR_LOOP, 0, "tenant-key"),

  /* ---- identity ------------------------------------------------------- */
  R(RM_ANY,  "/api/me",                  RG_TENANT, RR_LOOP,   0, "me"),
  R(RM_ANY,  "/api/audit",               RG_TENANT, RR_LOOP,   0, "audit"),
  R(RM_ANY,  "/api/audit/verify",        RG_TENANT, RR_WORKER, 0, "audit-verify"),

  /* ---- entities ------------------------------------------------------- */
  R(RM_ANY,  "/api/entities/stats",      RG_TENANT, RR_LOOP,   0, "entities-stats"),
  R(RM_ANY,  "/api/entities/search",     RG_TENANT, RR_LOOP,   0, "entities-search"),
  R(RM_ANY,  "/api/entities/*",          RG_TENANT, RR_LOOP,   0, "entity"),

  /* ---- layer data / cameras / transit -------------------------------- */
  R(RM_ANY,  "/api/data/cameras/discovery-feed", RG_AUTH, RR_LOOP,   0, "cameras-discovery-feed"),
  R(RM_ANY,  "/api/data/cameras/proxy",  RG_AUTH,   RR_WORKER, 0, "cameras-proxy"),
  R(RM_ANY,  "/api/data/cameras/run-status", RG_AUTH, RR_LOOP, 0, "cameras-run-status"),
  R(RM_POST, "/api/data/cameras/trigger", RG_RUN,   RR_WORKER, RF_STRICT, "cameras-trigger"),
  /* May run a collector on a cold cache; the run gate is applied there. */
  R(RM_ANY,  "/api/data/:id",            RG_AUTH,   RR_WORKER, 0, "layer-data"),
  R(RM_ANY,  "/api/transit*",            RG_AUTH,   RR_WORKER, 0, "transit"),
};

#define NROUTES (sizeof ROUTES / sizeof ROUTES[0])

const route_row *routes_table(size_t *n) {
  if (n) *n = NROUTES;
  return ROUTES;
}

/* ---- matching ---------------------------------------------------------- */

static int pat_match(const char *pat, const char *u, size_t ul) {
  size_t pl = strlen(pat);
  int prefix = pl > 0 && pat[pl - 1] == '*';
  if (prefix) pl--;
  size_t pi = 0, ui = 0;
  while (pi < pl) {
    if (pat[pi] == ':') {
      size_t pj = pi + 1;
      while (pj < pl && pat[pj] != '/') pj++;
      if (pj == pl) {                       /* final param: ONE segment */
        size_t uj = ui;
        while (uj < ul && u[uj] != '/') uj++;
        if (uj == ui) return 0;
        ui = uj; pi = pj;
      } else {                              /* param + literal tail: as seg() */
        size_t tl = pl - pj;
        if (ul < ui + 1 + tl) return 0;
        return memcmp(u + ul - tl, pat + pj, tl) == 0;
      }
    } else {
      if (ui >= ul || u[ui] != pat[pi]) return 0;
      pi++; ui++;
    }
  }
  return prefix ? 1 : ui == ul;
}

static unsigned method_bit(const char *m, size_t n) {
  if (n == 3 && !memcmp(m, "GET", 3))    return RM_GET;
  if (n == 4 && !memcmp(m, "POST", 4))   return RM_POST;
  if (n == 3 && !memcmp(m, "PUT", 3))    return RM_PUT;
  if (n == 5 && !memcmp(m, "PATCH", 5))  return RM_PATCH;
  if (n == 6 && !memcmp(m, "DELETE", 6)) return RM_DELETE;
  if (n == 4 && !memcmp(m, "HEAD", 4))   return RM_HEAD;
  return 0;
}

route_status routes_lookup(const char *method, size_t mlen,
                           const char *uri, size_t ulen,
                           const route_row **out) {
  unsigned mb = method_bit(method ? method : "", method ? mlen : 0);
  int soft = 0;
  for (size_t i = 0; i < NROUTES; i++) {
    const route_row *r = &ROUTES[i];
    if (!pat_match(r->pattern, uri, ulen)) continue;
    if ((r->methods & RM_ANY) || (r->methods & mb)) {
      if (out) *out = r;
      return ROUTE_OK;
    }
    if (r->flags & RF_STRICT) return ROUTE_METHOD_NOT_ALLOWED;
    soft = 1;
  }
  return soft ? ROUTE_METHOD_NOT_ALLOWED : ROUTE_NOT_FOUND;
}

/* ---- names ------------------------------------------------------------- */

const char *route_gate_name(route_gate g) {
  switch (g) {
    case RG_PUBLIC:       return "PUBLIC";
    case RG_AUTH:         return "AUTH";
    case RG_TENANT:       return "TENANT";
    case RG_ROLE_ANALYST: return "ROLE_ANALYST";
    case RG_ROLE_ADMIN:   return "ROLE_ADMIN";
    case RG_RUN:          return "RUN";
    case RG_OPERATOR:     return "OPERATOR";
  }
  return "?";
}

const char *route_runs_name(route_runs r) {
  return r == RR_WORKER ? "WORKER" : "LOOP";
}

const char *routes_methods_str(unsigned m, char *buf, size_t cap) {
  if (!cap) return buf;
  buf[0] = 0;
  if (m & RM_ANY) { snprintf(buf, cap, "ANY"); return buf; }
  static const struct { unsigned b; const char *n; } T[] = {
    { RM_GET, "GET" }, { RM_POST, "POST" }, { RM_PUT, "PUT" },
    { RM_PATCH, "PATCH" }, { RM_DELETE, "DELETE" }, { RM_HEAD, "HEAD" } };
  size_t len = 0;
  for (size_t i = 0; i < sizeof T / sizeof *T; i++) {
    if (!(m & T[i].b)) continue;
    int w = snprintf(buf + len, cap - len, "%s%s", len ? "," : "", T[i].n);
    if (w < 0 || (size_t) w >= cap - len) break;
    len += (size_t) w;
  }
  return buf;
}

int routes_role_rank(const char *r) {
  if (!r) return 0;
  if (!strcmp(r, "owner"))   return 4;
  if (!strcmp(r, "admin"))   return 3;
  if (!strcmp(r, "analyst")) return 2;
  if (!strcmp(r, "viewer"))  return 1;
  return 0;
}

char *routes_json(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(root, "data");
  int by[8] = {0};
  for (size_t i = 0; i < NROUTES; i++) {
    const route_row *r = &ROUTES[i];
    char mb[64];
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "name", r->name);
    cJSON_AddStringToObject(o, "method", routes_methods_str(r->methods, mb, sizeof mb));
    cJSON_AddStringToObject(o, "path", r->pattern);
    cJSON_AddStringToObject(o, "gate", route_gate_name(r->gate));
    cJSON_AddStringToObject(o, "runs_on", route_runs_name(r->runs));
    cJSON_AddItemToArray(arr, o);
    if ((int) r->gate >= 0 && (int) r->gate < 8) by[r->gate]++;
  }
  cJSON *meta = cJSON_AddObjectToObject(root, "meta");
  cJSON_AddNumberToObject(meta, "total", (double) NROUTES);
  cJSON *bg = cJSON_AddObjectToObject(meta, "by_gate");
  for (int g = RG_PUBLIC; g <= RG_OPERATOR; g++)
    cJSON_AddNumberToObject(bg, route_gate_name((route_gate) g), by[g]);
  char *js = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return js;
}

const char *routes_selfcheck(void) {
  for (size_t i = 0; i < NROUTES; i++) {
    const route_row *r = &ROUTES[i];
    if (!r->pattern || !*r->pattern || !r->name || !*r->name)
      return "a row has no pattern or no name";
    if (!r->methods) return "a row has an empty method set";
    for (size_t j = i + 1; j < NROUTES; j++)
      if (!strcmp(r->name, ROUTES[j].name)) return "duplicate route name";
    for (const char *p = r->pattern; *p; p++)
      if (*p == ':' && p != r->pattern && p[-1] != '/')
        return "':' must start a path segment";
  }
  return NULL;
}
