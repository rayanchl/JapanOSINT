/* collectors/sources/mar_nga_navarea_warnings.c
 * NGA Maritime Safety Information — NAVAREA / HYDROLANT / HYDROPAC /
 * HYDROARC broadcast navigational warnings as issued to mariners.
 *
 * Endpoint: https://msi.nga.mil/api/publications/broadcast-warn?output=json
 * Emits (all fetched): msgYear, msgNumber, navArea, subregion, status,
 *   issueDate, authority, cancelDate/cancelNavArea and the full warning text.
 * Keyless. Licence: US Government work (NGA), public domain — no stated
 * restriction on reuse. Undocumented but stable REST API; OpenAPI at
 * https://msi.nga.mil/api/v3/api-docs.
 *
 * parse_notes trap — "There is NO numeric latitude/longitude field on this
 * record — positions exist only inside the free-text 'text' block as DDD-MM.MN
 * strings. has_geo must be 0 unless you write a DMS text extractor; do not
 * geocode navArea (an ocean-sized region) as a point." No row here sets
 * has_geo, and navArea is emitted as a text attribute only (R2).
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define BW_URL "https://msi.nga.mil/api/publications/broadcast-warn?output=json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, BW_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[nga-navarea-warnings] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "broadcast-warn");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  cJSON *w;
  cJSON_ArrayForEach(w, arr) {
    if (!cJSON_IsObject(w)) continue;
    const cJSON *yr = cJSON_GetObjectItem(w, "msgYear");
    const cJSON *no = cJSON_GetObjectItem(w, "msgNumber");
    const char *area = jo_sv(w, "navArea");
    const char *text = jo_sv(w, "text");
    if (!cJSON_IsNumber(yr) || !cJSON_IsNumber(no)) continue;
    if (!text) continue;                    /* no fetched content -> no row */

    char key[64];
    snprintf(key, sizeof key, "%s-%d-%d", area ? area : "NA",
             (int)yr->valuedouble, (int)no->valuedouble);

    cJSON *p = cJSON_CreateObject();
    jo_copy_num(p, w, "msgYear");
    jo_copy_num(p, w, "msgNumber");
    jo_copy_str(p, w, "navArea");
    jo_copy_str(p, w, "subregion");
    jo_copy_str(p, w, "status");
    jo_copy_str(p, w, "issueDate");
    jo_copy_str(p, w, "authority");
    jo_copy_str(p, w, "cancelDate");
    jo_copy_str(p, w, "cancelNavArea");
    jo_copy_str(p, w, "cancelMsgYear");
    jo_copy_str(p, w, "cancelMsgNumber");
    cJSON_AddStringToObject(p, "geo_note",
      "no coordinate field upstream; positions appear only inside the warning text");
    cJSON_AddStringToObject(p, "source", "NGA Maritime Safety Information");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[200];
    snprintf(title, sizeof title, "NAVAREA %s %d/%d broadcast warning",
             area ? area : "?", (int)no->valuedouble, (int)yr->valuedouble);

    char summary[256];
    snprintf(summary, sizeof summary, "%.240s", text);
    for (char *q = summary; *q; q++) if (*q == '\n' || *q == '\r') *q = ' ';

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.body            = text;
    it.link            = "https://msi.nga.mil/NavWarnings";
    it.lang            = "en";
    it.published_at    = jo_sv(w, "issueDate");
    it.record_type     = "navigational-warning";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"navwarning\",\"nga\"]";
    /* no has_geo (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[nga-navarea-warnings] emitted %d\n", n);
  return 0;
}

static const source_def mar_nga_navarea_warnings_def = {
  .id = "nga-navarea-warnings", .collector = "maritime",
  .name = "NGA Maritime Safety Information — NAVAREA/HYDROLANT broadcast warnings",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api", .url = BW_URL,
  .description = "US NGA broadcast navigational warnings (NAVAREA IV/XII, HYDROLANT, HYDROPAC, HYDROARC) — live-fire areas, derelicts, missile tests, buoy outages and drilling-rig positions, as issued to mariners.",
  .license = "US Government work (NGA), public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_nga_navarea_warnings_def)
