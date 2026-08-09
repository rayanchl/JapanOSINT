/* collectors/environment/sources/wolfx_eew.c
 * Port of server/src/collectors/wolfxEew.js. api.wolfx.jp/jma_eew.json is a
 * single EEW object → at most one Feature. Keyless. SEED branch (d==null)
 * dropped → 0 features when upstream down. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://api.wolfx.jp/jma_eew.json"

/* x ?? null (any JSON type) */
static void coalesce_null(cJSON *p, const char *k, cJSON *v) {
  cJSON_AddItemToObject(p, k,
    (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

/* JS Number(x): number → itself, numeric string → parsed, else NaN. */
static double jnum(cJSON *v, int *ok) {
  if (v && cJSON_IsNumber(v)) { *ok = 1; return v->valuedouble; }
  if (v && cJSON_IsString(v) && v->valuestring[0]) {
    char *e; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *ok = 1; return d; }
  }
  *ok = 0;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *d = feed_get_json(ctx->http, API_URL, 6000);

  cJSON *features = cJSON_CreateArray();
  cJSON *latv = d ? cJSON_GetObjectItem(d, "Latitude") : NULL;
  cJSON *lonv = d ? cJSON_GetObjectItem(d, "Longitude") : NULL;
  if (latv && !cJSON_IsNull(latv) && lonv && !cJSON_IsNull(lonv)) {
    int ok1, ok2;
    double lon = jnum(lonv, &ok1);
    double lat = jnum(latv, &ok2);

    cJSON *f = gj_point_feature(lon, lat);

    cJSON *p = cJSON_CreateObject();               /* EXACT JS key order */
    coalesce_null(p, "event_id", cJSON_GetObjectItem(d, "EventID"));
    coalesce_null(p, "serial", cJSON_GetObjectItem(d, "Serial"));
    cJSON *mg = cJSON_GetObjectItem(d, "Magunitude");
    if (!mg || cJSON_IsNull(mg)) mg = cJSON_GetObjectItem(d, "Magnitude");
    coalesce_null(p, "magnitude", mg);
    coalesce_null(p, "depth_km", cJSON_GetObjectItem(d, "Depth"));
    coalesce_null(p, "max_intensity", cJSON_GetObjectItem(d, "MaxIntensity"));
    coalesce_null(p, "hypocenter", cJSON_GetObjectItem(d, "Hypocenter"));
    coalesce_null(p, "announced_time", cJSON_GetObjectItem(d, "AnnouncedTime"));
    coalesce_null(p, "origin_time", cJSON_GetObjectItem(d, "OriginTime"));
    coalesce_null(p, "is_final", cJSON_GetObjectItem(d, "isFinal"));
    coalesce_null(p, "is_cancel", cJSON_GetObjectItem(d, "isCancel"));
    coalesce_null(p, "is_warn", cJSON_GetObjectItem(d, "isWarn"));
    cJSON_AddStringToObject(p, "source", "wolfx_jma_eew");
    /* Readable identity + timeline placement. geojson_emit_features reads
     * properties.title / summary / link / published_at; without them an EEW
     * row renders as a blank pin. Everything below comes from the fetched
     * object — OriginTime is JST ("YYYY/MM/DD HH:MM:SS"), normalised to ISO
     * so the timeline can order it. */
    {
      cJSON *hy = cJSON_GetObjectItem(d, "Hypocenter");
      cJSON *mi = cJSON_GetObjectItem(d, "MaxIntensity");
      const char *hyp = (hy && cJSON_IsString(hy) && hy->valuestring[0])
                      ? hy->valuestring : NULL;
      int okm; double mag = jnum(mg, &okm);
      char title[256];
      int o = snprintf(title, sizeof title, "\xe7\xb7\x8a\xe6\x80\xa5\xe5\x9c\xb0\xe9\x9c\x87\xe9\x80\x9f\xe5\xa0\xb1"); /* 緊急地震速報 */
      /* `hyp` is upstream text of unbounded length; a bare `o += snprintf(...)`
       * leaves the cursor past the buffer on truncation and the next
       * `sizeof title - o` wraps. Clamp to the terminator. */
      #define ET_ADV(expr) do { int w_ = (expr); \
        if (w_ > 0) o += w_; \
        if (o > (int)sizeof title - 1) o = (int)sizeof title - 1; } while (0)
      if (hyp) ET_ADV(snprintf(title + o, sizeof title - (size_t)o, " %s", hyp));
      if (okm) ET_ADV(snprintf(title + o, sizeof title - (size_t)o, " M%.1f", mag));
      #undef ET_ADV
      if (mi && cJSON_IsString(mi) && mi->valuestring[0])
        snprintf(title + o, sizeof title - (size_t)o,
                 " \xe6\x9c\x80\xe5\xa4\xa7\xe9\x9c\x87\xe5\xba\xa6%s", mi->valuestring); /* 最大震度 */
      cJSON_AddStringToObject(p, "title", title);
      cJSON_AddStringToObject(p, "link", "https://api.wolfx.jp/jma_eew.json");
      cJSON *ot = cJSON_GetObjectItem(d, "OriginTime");
      if (ot && cJSON_IsString(ot)) {
        int Y, M, D, h, m, s;
        if (sscanf(ot->valuestring, "%d/%d/%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) == 6) {
          char iso[40];
          snprintf(iso, sizeof iso, "%04d-%02d-%02dT%02d:%02d:%02d+09:00",
                   Y, M, D, h, m, s);
          cJSON_AddStringToObject(p, "published_at", iso);
        }
      }
    }
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  if (d) cJSON_Delete(d);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wolfx-eew] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wolfx_eew_def = {
  .id = "wolfx-eew", .collector = "environment",
  .name = "Wolfx JMA Earthquake Early Warning", .name_ja = "Wolfx 緊急地震速報",
   .update_interval_sec = 10, .run = run,
};
REGISTER_SOURCE(wolfx_eew_def)
