/* collectors/sources/mar_nga_maritime_advisories.c
 * NGA MSI — Special Warnings / maritime advisory messages (SMAPS): underwater
 * operations, military exercises, piracy and threat advisories.
 *
 * Endpoint: https://msi.nga.mil/api/publications/smaps?output=json
 * Emits (all fetched): msgID, msgSqncNumber, msgType, category, navArea,
 *   usNavArea, oceans, subRegion, dncRegion, status, createdOn, ingestDate,
 *   cancelledOn and the full msgText.
 * Keyless. Licence: US Government work (NGA), public domain.
 *
 * parse_notes trap — "No coordinate fields; has_geo 0. 'oceans' and
 * 'subRegion' are ocean-scale descriptors and must never be turned into a
 * point." They are emitted as text attributes only; no row sets has_geo (R2).
 * parse_notes — "status=INFORCE/CANCELLED drives whether the advisory is
 * current"; status is carried verbatim and surfaced in the summary.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define SMAPS_URL "https://msi.nga.mil/api/publications/smaps?output=json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SMAPS_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[nga-maritime-advisories] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "smaps");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  cJSON *m;
  cJSON_ArrayForEach(m, arr) {
    if (!cJSON_IsObject(m)) continue;
    const char *msgid = jo_sv(m, "msgID");
    const char *text  = jo_sv(m, "msgText");
    if (!msgid || !text) continue;          /* no fetched content -> no row */

    const char *cat  = jo_sv(m, "category");
    const char *type = jo_sv(m, "msgType");
    const char *stat = jo_sv(m, "status");
    const char *ocean = jo_sv(m, "oceans");
    const cJSON *seq = cJSON_GetObjectItem(m, "msgSqncNumber");

    cJSON *p = cJSON_CreateObject();
    jo_copy_str(p, m, "msgID");
    jo_copy_str(p, m, "category");
    jo_copy_str(p, m, "navArea");
    jo_copy_str(p, m, "usNavArea");
    jo_copy_str(p, m, "oceans");
    jo_copy_str(p, m, "subRegion");
    jo_copy_str(p, m, "dncRegion");
    jo_copy_str(p, m, "msgType");
    jo_copy_str(p, m, "status");
    jo_copy_str(p, m, "createdOn");
    jo_copy_str(p, m, "ingestDate");
    jo_copy_str(p, m, "cancelledOn");
    jo_copy_num(p, m, "msgSqncNumber");
    cJSON_AddStringToObject(p, "geo_note",
      "ocean/sub-region descriptors are area-scale labels, not point locations");
    cJSON_AddStringToObject(p, "source", "NGA Maritime Safety Information");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220];
    if (cJSON_IsNumber(seq))
      snprintf(title, sizeof title, "%s %d — %s",
               type ? type : "NGA advisory", (int)seq->valuedouble,
               cat ? cat : "maritime advisory");
    else
      snprintf(title, sizeof title, "%s — %s",
               type ? type : "NGA advisory", cat ? cat : "maritime advisory");

    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s",
             stat ? stat : "", (stat && ocean) ? " · " : "", ocean ? ocean : "");

    intel_item it = {0};
    it.remote_key      = msgid;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = text;
    it.link            = "https://msi.nga.mil/NavWarnings";
    it.lang            = "en";
    it.published_at    = jo_sv(m, "ingestDate");
    it.record_type     = "maritime-advisory";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"advisory\",\"nga\"]";
    /* no has_geo (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[nga-maritime-advisories] emitted %d\n", n);
  return 0;
}

static const source_def mar_nga_maritime_advisories_def = {
  .id = "nga-maritime-advisories", .collector = "maritime",
  .name = "NGA MSI — Special Warnings / maritime advisories (SMAPS)",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api", .url = SMAPS_URL,
  .description = "NGA maritime advisory messages by category (underwater operations, military exercises, piracy/threat advisories) with the ocean/sub-region affected, the full message text and in-force status.",
  .license = "US Government work (NGA), public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_nga_maritime_advisories_def)
