/* collectors/environment/sources/jamstec_argo.c   (category: ocean -> environment)
 * FEED source — port of server/src/collectors/jamstecArgo.js.
 * Argovis API: Argo profiling-float positions, last ~14 days, Japan polygon.
 * -> FeatureCollection. Honest empty on failure — never fabricated. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define ARGOVIS "https://argovis-api.colorado.edu/argo"
/* Japan-region polygon (lon,lat) closed ring, URL-encoded. */
#define POLYGON_ENC "%5B%5B122%2C24%5D%2C%5B146%2C24%5D%2C%5B146%2C46%5D%2C%5B122%2C46%5D%2C%5B122%2C24%5D%5D"

static void iso_utc(time_t t, char *out, size_t n) {
  struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%S.000Z", &g);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t end = time(NULL);
  time_t start = end - 14 * 24 * 3600;
  char s_iso[40], e_iso[40];
  iso_utc(start, s_iso, sizeof s_iso);
  iso_utc(end, e_iso, sizeof e_iso);

  char url[512];
  snprintf(url, sizeof url,
    "%s?startDate=%s&endDate=%s&polygon=%s",
    ARGOVIS, s_iso, e_iso, POLYGON_ENC);

  cJSON *data = feed_get_json(ctx->http, url, 20000);
  if (!data || !cJSON_IsArray(data)) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[jamstec-argo] upstream unavailable\n");
    return -1;
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *p;
  cJSON_ArrayForEach(p, data) {
    cJSON *geoloc = cJSON_GetObjectItem(p, "geolocation");
    cJSON *coords = geoloc ? cJSON_GetObjectItem(geoloc, "coordinates") : NULL;
    if (!coords || !cJSON_IsArray(coords) || cJSON_GetArraySize(coords) < 2)
      continue;
    cJSON *c0 = cJSON_GetArrayItem(coords, 0);
    cJSON *c1 = cJSON_GetArrayItem(coords, 1);
    double lon = c0 && cJSON_IsNumber(c0) ? c0->valuedouble
               : (c0 && cJSON_IsString(c0) ? strtod(c0->valuestring, NULL) : NAN);
    double lat = c1 && cJSON_IsNumber(c1) ? c1->valuedouble
               : (c1 && cJSON_IsString(c1) ? strtod(c1->valuestring, NULL) : NAN);
    if (!isfinite(lon) || !isfinite(lat)) continue;

    cJSON *f = gj_point_feature(lon, lat);

    cJSON *pr = cJSON_CreateObject();            /* EXACT JS key order */
    cJSON *id = cJSON_GetObjectItem(p, "_id");
    cJSON_AddItemToObject(pr, "profile_id",
      id ? cJSON_Duplicate(id, 1) : cJSON_CreateNull());
    cJSON *cyc = cJSON_GetObjectItem(p, "cycle_number");
    cJSON_AddItemToObject(pr, "cycle_number",
      cyc ? cJSON_Duplicate(cyc, 1) : cJSON_CreateNull());
    cJSON *ts = cJSON_GetObjectItem(p, "timestamp");
    cJSON_AddItemToObject(pr, "observed_at",
      ts ? cJSON_Duplicate(ts, 1) : cJSON_CreateNull());
    cJSON *bas = cJSON_GetObjectItem(p, "basin");
    cJSON_AddItemToObject(pr, "basin",
      bas ? cJSON_Duplicate(bas, 1) : cJSON_CreateNull());
    cJSON *pd = cJSON_GetObjectItem(p, "profile_direction");
    cJSON_AddItemToObject(pr, "profile_direction",
      pd ? cJSON_Duplicate(pd, 1) : cJSON_CreateNull());
    /* data_sources = source.flatMap(s => s.source || []) */
    cJSON *ds = cJSON_CreateArray();
    cJSON *src = cJSON_GetObjectItem(p, "source");
    if (src && cJSON_IsArray(src)) {
      cJSON *s;
      cJSON_ArrayForEach(s, src) {
        cJSON *ss = cJSON_GetObjectItem(s, "source");
        if (ss && cJSON_IsArray(ss)) {
          cJSON *e;
          cJSON_ArrayForEach(e, ss)
            cJSON_AddItemToArray(ds, cJSON_Duplicate(e, 1));
        }
      }
    }
    cJSON_AddItemToObject(pr, "data_sources", ds);
    cJSON_AddStringToObject(pr, "source", "argovis_argo");

    /* --- fields the JS port dropped -------------------------------------
     * lib/geojson.c derives title/link/published_at from property keys, so
     * without these every Argo row landed in the UI with no title at all and
     * no way to reach the underlying profile. All values below come from the
     * Argovis response; nothing is synthesised except the display string. */
    const char *pid = (id && cJSON_IsString(id)) ? id->valuestring : NULL;
    /* _id is "<platform>_<cycle>"; the platform part is the WMO float number */
    if (pid) {
      char plat[64] = {0};
      const char *us = strchr(pid, '_');
      size_t pl = us ? (size_t)(us - pid) : strlen(pid);
      if (pl >= sizeof plat) pl = sizeof plat - 1;
      memcpy(plat, pid, pl);
      cJSON_AddStringToObject(pr, "platform_number", plat);
      /* profile_id is not a NATIVE_ID_KEY, so the uid was a hash of the
       * whole property bag and churned on every schema tweak. */
      cJSON_AddStringToObject(pr, "uid", pid);
      char title[160];
      if (cyc && cJSON_IsNumber(cyc))
        snprintf(title, sizeof title, "Argo float %s cycle %d", plat,
                 (int)cyc->valuedouble);
      else
        snprintf(title, sizeof title, "Argo float %s", plat);
      cJSON_AddStringToObject(pr, "title", title);
    }
    /* source[0].url is the DAC NetCDF profile — the provenance link */
    if (src && cJSON_IsArray(src) && cJSON_GetArraySize(src) > 0) {
      cJSON *s0 = cJSON_GetArrayItem(src, 0);
      cJSON *u = s0 ? cJSON_GetObjectItem(s0, "url") : NULL;
      if (u && cJSON_IsString(u) && u->valuestring[0])
        cJSON_AddStringToObject(pr, "url", u->valuestring);
      cJSON *du = s0 ? cJSON_GetObjectItem(s0, "date_updated") : NULL;
      if (du && cJSON_IsString(du))
        cJSON_AddStringToObject(pr, "dac_updated_at", du->valuestring);
    }
    cJSON *dua = cJSON_GetObjectItem(p, "date_updated_argovis");
    if (dua && cJSON_IsString(dua))
      cJSON_AddStringToObject(pr, "argovis_updated_at", dua->valuestring);
    cJSON *vss = cJSON_GetObjectItem(p, "vertical_sampling_scheme");
    if (vss && cJSON_IsString(vss))
      cJSON_AddStringToObject(pr, "vertical_sampling_scheme", vss->valuestring);
    cJSON *gqc = cJSON_GetObjectItem(p, "geolocation_argoqc");
    if (gqc) cJSON_AddItemToObject(pr, "geolocation_qc", cJSON_Duplicate(gqc, 1));
    cJSON *tqc = cJSON_GetObjectItem(p, "timestamp_argoqc");
    if (tqc) cJSON_AddItemToObject(pr, "timestamp_qc", cJSON_Duplicate(tqc, 1));
    /* data_info[0] is the list of variables this profile actually measured */
    cJSON *di = cJSON_GetObjectItem(p, "data_info");
    if (di && cJSON_IsArray(di) && cJSON_GetArraySize(di) > 0) {
      cJSON *vars = cJSON_GetArrayItem(di, 0);
      if (cJSON_IsArray(vars))
        cJSON_AddItemToObject(pr, "measured_variables", cJSON_Duplicate(vars, 1));
    }

    cJSON_AddItemToObject(f, "properties", pr);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jamstec-argo] emitted %d\n", n);
  return 0;      /* no floats surfaced in the window is an honest empty; the
                  * fetch failure above is the only real error (rc=-1) */
}

static const source_def jamstec_argo_def = {
  .id = "jamstec-argo", .collector = "environment",
  .name = "JAMSTEC Argo Floats", .name_ja = "JAMSTEC アルゴフロート",
  .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(jamstec_argo_def)
