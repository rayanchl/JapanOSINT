/* lib/arcgis_dir.c — see arcgis_dir.h for why this exists.
 *
 * Extracted 2026-09-14 from the sci-noaa-mapsvc-* collectors once the same
 * defect turned up in seven more directory rows across four files
 * (lat-geo-arcgis-sgc/-snirh, sas-arc-nsw-env/-qld/-tas/-wa-slip,
 * sci-noaa-coast-services): one copy of the walk instead of five. */
#include "arcgis_dir.h"
#include "feedlib.h"
#include "jsonlist.h"
#include "jocore.h"
#include <stdio.h>
#include <string.h>

static int dir_emit(intel_sink *s, const char *source_id, const char *base,
                    cJSON *doc, const char *record_type, const char *lang,
                    const char *tags_json) {
  cJSON *arr = cJSON_GetObjectItemCaseSensitive(doc, "services");
  if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) == 0) return 0;
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    if (!cJSON_IsObject(rec)) continue;
    cJSON *nm = cJSON_GetObjectItemCaseSensitive(rec, "name");
    cJSON *ty = cJSON_GetObjectItemCaseSensitive(rec, "type");
    if (!cJSON_IsString(nm) || !nm->valuestring ||
        !cJSON_IsString(ty) || !ty->valuestring) continue;
    char buf[768];
    if (!cJSON_GetObjectItemCaseSensitive(rec, "id")) {
      snprintf(buf, sizeof buf, "%s/%s", nm->valuestring, ty->valuestring);
      cJSON_AddStringToObject(rec, "id", buf);
    }
    if (!cJSON_GetObjectItemCaseSensitive(rec, "service_url")) {
      snprintf(buf, sizeof buf, "%s/%s/%s", base, nm->valuestring, ty->valuestring);
      cJSON_AddStringToObject(rec, "service_url", buf);
    }
  }
  int n = jsonlist_emit_ex(s, source_id, doc, "services", record_type, lang,
                           tags_json, NULL);
  return n > 0 ? n : 0;
}

int arcgis_dir_walk(const source_ctx *c, intel_sink *s, const char *source_id,
                    const char *base, const char *record_type,
                    const char *lang, const char *tags_json) {
  if (!c || !s || !source_id || !base) return -1;
  char url[768];
  snprintf(url, sizeof url, "%s?f=json", base);
  cJSON *root = feed_get_json(c->http, url, 25000);
  if (!root) { fprintf(stderr, "[%s] fetch failed\n", source_id); return -1; }
  int n = dir_emit(s, source_id, base, root, record_type, lang, tags_json);
  int folders = 0, failed = 0;
  cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "folders");
  cJSON *f;
  cJSON_ArrayForEach(f, list) {
    if (!cJSON_IsString(f) || !f->valuestring) continue;
    folders++;
    snprintf(url, sizeof url, "%s/%s?f=json", base, f->valuestring);
    cJSON *sub = feed_get_json(c->http, url, 25000);
    if (!sub) {
      failed++;
      fprintf(stderr, "[%s] folder listing failed: %s\n", source_id, url);
      continue;
    }
    n += dir_emit(s, source_id, base, sub, record_type, lang, tags_json);
    cJSON_Delete(sub);
  }
  cJSON_Delete(root);
  if (failed) {
    char why[160];
    snprintf(why, sizeof why, "%d of %d folder listing(s) failed to fetch",
             failed, folders);
    jo_truncation_notice_ex(s, source_id, NULL, n, -1, why,
                            "re-run; each failed folder url is in the run log",
                            NULL);
  }
  fprintf(stderr, "[%s] emitted %d service(s) across %d folder(s)\n",
          source_id, n, folders);
  return (n == 0 && failed) ? -1 : 0;
}
