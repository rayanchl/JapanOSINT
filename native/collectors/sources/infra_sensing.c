/* collectors/osint/sources/infra_sensing.c
 * OSINT services — sensing / recon gaps not already covered by the existing
 * infra collectors (dam levels, lightning, grid demand, airport/port infra all
 * already exist, so they are intentionally NOT duplicated here).
 *   • ZoomEye        — internet-wide scan engine, real API (ZOOMEYE_API_KEY).
 *   • Hunter.io      — corporate email patterns, real API (HUNTER_API_KEY).
 *   • Radio stations — 総務省 無線局等情報検索 (anchor scrape).
 *   • Drone registry — DIPS 無人航空機登録 (anchor scrape).
 *   • Hazmat sites   — 危険物・高圧ガス施設 (anchor scrape).
 * Real fetch or honest empty; key-gated APIs return empty without their key. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- ZoomEye host search (NEEDS_KEY) ------------------------------------- */
static int run_zoomeye(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  const char *key = jo_env("ZOOMEYE_API_KEY");
  if (!key) { fprintf(stderr, "[zoomeye] gated (no ZOOMEYE_API_KEY)\n"); return 0; }
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://api.zoomeye.org/host/search?query=%s+country:JP", q);
  free(q);
  char hdr[160]; snprintf(hdr, sizeof hdr, "API-KEY: %s", key);
  const char *hdrs[] = { hdr, "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "zoomeye");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *matches = cJSON_GetObjectItem(root, "matches");
  cJSON *m = NULL;
  cJSON_ArrayForEach(m, matches) {
    cJSON *ipo = cJSON_GetObjectItem(m, "ip");
    const char *ip = (ipo && cJSON_IsString(ipo)) ? ipo->valuestring : NULL;
    if (!ip) continue;
    char *pj = cJSON_PrintUnformatted(m);
    intel_item it = {0};
    it.remote_key = ip; it.title = ip; it.lang = "en";
    it.record_type = "zoomeye-host"; it.body = pj;
    it.properties_json = "{\"service\":\"ZoomEye\",\"success\":true}";
    it.tags_json = "[\"osint-search\",\"cyber\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[zoomeye] emitted %d\n", emitted);
  return 0;
}

/* ---- Hunter.io domain search (NEEDS_KEY) --------------------------------- */
static int run_hunter(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  const char *key = jo_env("HUNTER_API_KEY");
  if (!key) { fprintf(stderr, "[hunter] gated (no HUNTER_API_KEY)\n"); return 0; }
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://api.hunter.io/v2/domain-search?domain=%s&api_key=%s", q, key);
  free(q);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "hunter");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *data = cJSON_GetObjectItem(root, "data");
  cJSON *emails = cJSON_GetObjectItem(data, "emails");
  cJSON *e = NULL;
  cJSON_ArrayForEach(e, emails) {
    const char *addr = jo_sv(e, "value");
    if (!addr) continue;
    const char *first = jo_sv(e, "first_name");
    const char *last = jo_sv(e, "last_name");
    const char *pos = jo_sv(e, "position");
    char *pj = cJSON_PrintUnformatted(e);
    char title[256];
    snprintf(title, sizeof title, "%s%s%s", first ? first : "",
             (first && last) ? " " : "", last ? last : "");
    intel_item it = {0};
    it.remote_key = addr; it.title = title[0] ? title : addr;
    it.summary = pos; it.body = pj; it.lang = "en";
    it.record_type = "corporate-email";
    it.properties_json = "{\"service\":\"Hunter.io\",\"success\":true}";
    it.tags_json = "[\"osint-search\",\"email\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[hunter] emitted %d\n", emitted);
  return 0;
}

/* ---- scrape-backed registries (radio / drone / hazmat) ------------------- */
typedef struct { const char *id, *url, *base, *service, *rectype; } scrape_row;
static const scrape_row ROWS[] = {
  { "RADIO_STATIONS", "https://www.tele.soumu.go.jp/musen/SearchServlet?pageID=2",
    "https://www.tele.soumu.go.jp", "Radio Station Licenses", "radio-station" },
  { "DRONE_REGISTRY", "https://www.dips-reg.mlit.go.jp/drs/",
    "https://www.dips-reg.mlit.go.jp", "Drone Registry", "drone-registration" },
  { "HAZMAT_FACILITIES", "https://www.fdma.go.jp/relocation/neuter/topics/houkokusho/",
    "https://www.fdma.go.jp", "Hazardous-material Facilities", "hazmat-facility" },
};
static int run_scrape(const source_ctx *ctx, intel_sink *sink) {
  for (size_t i = 0; i < sizeof ROWS / sizeof ROWS[0]; i++) {
    if (strcmp(ctx->source_id, ROWS[i].id) != 0) continue;
    const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
    jo_emit_anchors(ctx, sink, ROWS[i].url, NULL, ROWS[i].service,
                    ROWS[i].rectype, ROWS[i].base, q, 30, ROWS[i].id);
    return 0;
  }
  return -1;
}

#include "_source_macros.inc"

DEFR(zoomeye_def, "ZOOMEYE", "ZoomEye Host Search", "ZoomEye ホスト検索", run_zoomeye,
     "cyber", "api", "https://www.zoomeye.org/", "Internet-wide device/banner scan engine for JP (needs ZOOMEYE_API_KEY)", 0);
DEFR(hunter_def, "HUNTER_IO", "Hunter.io Email Finder", "Hunter.io メール検索", run_hunter,
     "cyber", "api", "https://hunter.io/", "Corporate email patterns & verified contacts by domain (needs HUNTER_API_KEY)", 0);
DEFR(radio_def, "RADIO_STATIONS", "Radio Station Licenses", "総務省 無線局等情報検索", run_scrape,
     "telecom", "scraped", "https://www.tele.soumu.go.jp/", "Licensed transmitters: licensee, location, frequency (entity pivot)", 1);
DEFR(drone_def, "DRONE_REGISTRY", "Drone Registry (DIPS)", "無人航空機 登録 (DIPS)", run_scrape,
     "safety", "scraped", "https://www.dips-reg.mlit.go.jp/", "Registered drones & owners (entity pivot)", 1);
DEFR(hazmat_def, "HAZMAT_FACILITIES", "Hazmat Facilities", "危険物・高圧ガス施設", run_scrape,
     "infrastructure", "scraped", "https://www.fdma.go.jp/", "Hazardous-material facility locations & operators", 1);
