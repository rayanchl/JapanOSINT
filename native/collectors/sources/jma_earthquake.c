/* collectors/environment/sources/jma_earthquake.c
 * FEED source — port of server/src/collectors/jmaEarthquake.js.
 * https://www.jma.go.jp/bosai/quake/data/list.json -> one intel row/quake.
 * uid = jma-earthquake|<eid> (matches Node featureUid earthquake_id key). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://www.jma.go.jp/bosai/quake/data/list.json"

/* ISO 6709 "+38.7+141.9-50000/" -> lat,lon and (optional) depth in metres
 * (third signed number, negative below sea level).
 *
 * ISO 6709 does NOT mandate decimal degrees. It permits three sexagesimal
 * layouts, distinguished ONLY by how many digits precede the decimal point:
 *
 *   latitude   ±DD.D      ±DDMM.M      ±DDMMSS.S      (2, 4, 6 int digits)
 *   longitude  ±DDD.D     ±DDDMM.M     ±DDDMMSS.S     (3, 5, 7 int digits)
 *
 * JMA publishes both the decimal and the degrees-minutes form — the routine
 * bulletins use "+32.6+130.7", while the significant-event entries use
 * "+3237.5+13040.7" for the SAME region. Reading the latter as decimal
 * degrees yielded lat 3237.5 / lon 13040.7, which is not on Earth: the M7.1
 * Kumamoto mainshock, the single most important row in the feed, was the one
 * being dropped off the map. Found by tests/contract/source_contract.py's
 * coordinate-range check (1 of 479 rows).
 *
 * Digit counting is the specified discriminator, so use it rather than
 * guessing from magnitude — a magnitude test cannot tell +12.5 (12.5°N)
 * from a malformed value, and gets DDMMSS wrong for small angles. */
static double iso6709_deg(const char *start, const char *end, int is_lon) {
  int intdig = 0;
  for (const char *p = start; p < end && *p != '.'; p++)
    if (*p >= '0' && *p <= '9') intdig++;

  double v = strtod(start, NULL);
  double a = v < 0 ? -v : v, sign = v < 0 ? -1.0 : 1.0;
  int base = is_lon ? 3 : 2;                 /* int digits in the degree part */

  if (intdig == base + 2) {                  /* DDMM.M   */
    double d = (double)(long)(a / 100.0);
    return sign * (d + (a - d * 100.0) / 60.0);
  }
  if (intdig == base + 4) {                  /* DDMMSS.S */
    double d = (double)(long)(a / 10000.0);
    double rem = a - d * 10000.0;
    double m = (double)(long)(rem / 100.0);
    return sign * (d + m / 60.0 + (rem - m * 100.0) / 3600.0);
  }
  return v;                                  /* plain decimal degrees */
}

static int parse_cod(const char *cod, double *lat, double *lon,
                     double *depth_m, int *has_depth) {
  if (has_depth) *has_depth = 0;
  if (!cod) return 0;
  char *end;
  const char *p1 = cod;
  double a = strtod(p1, &end);
  if (end == p1) return 0;
  double la = iso6709_deg(p1, end, 0);

  const char *p2 = end;
  double b = strtod(p2, &end);
  if (b == 0 && end == p2) return 0;
  double lo = iso6709_deg(p2, end, 1);
  (void)a; (void)b;

  /* Refuse an impossible fix rather than persisting one. A row with no
   * location is correct; a row at lat 3237 is a lie the map renders as fact. */
  if (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0) return 0;
  *lat = la; *lon = lo;

  /* Third field is depth in METRES, not an angle — never sexagesimal. */
  if (depth_m && has_depth) {
    const char *p3 = end;
    double d = strtod(p3, &end);
    if (end != p3) { *depth_m = d; *has_depth = 1; }
  }
  return 1;
}

/* list.json is newest-first and publishes SEVERAL bulletins per event id
 * (震度速報 → 震源に関する情報 → 震源・震度情報), all sharing `eid`. The uid
 * is per-event, so the old straight-through loop let the OLDEST (least
 * complete, often hypocentre-less) bulletin overwrite the final one. Keep the
 * FIRST occurrence of each eid — that is the latest bulletin. */
#define SEEN_CAP 2048
static int seen_has(char seen[][24], int n, const char *eid) {
  for (int i = 0; i < n; i++) if (!strcmp(seen[i], eid)) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *arr = feed_get_json(ctx->http, API_URL, 20000);
  if (!arr || !cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return -1; }
  int n = 0, nseen = 0;
  static char seen[SEEN_CAP][24];
  cJSON *eq;
  cJSON_ArrayForEach(eq, arr) {
    const cJSON *eid = cJSON_GetObjectItem(eq, "eid");
    if (!eid || !cJSON_IsString(eid) || !eid->valuestring[0]) continue;
    if (nseen < SEEN_CAP) {
      if (seen_has(seen, nseen, eid->valuestring)) continue;
      snprintf(seen[nseen++], 24, "%s", eid->valuestring);
    }

    const char *ttl = jo_sv(eq, "ttl"), *anm = jo_sv(eq, "anm");
    const char *mag = jo_sv(eq, "mag"), *at = jo_sv(eq, "at");
    const char *cod = jo_sv(eq, "cod"), *maxi = jo_sv(eq, "maxi");
    const char *ift = jo_sv(eq, "ift"), *ser = jo_sv(eq, "ser");
    const char *rdt = jo_sv(eq, "rdt"), *acd = jo_sv(eq, "acd");
    const char *en_ttl = jo_sv(eq, "en_ttl"), *en_anm = jo_sv(eq, "en_anm");
    const char *jsonf = jo_sv(eq, "json");

    double lat = 0, lon = 0, depth_m = 0; int has_depth = 0;
    int geo = parse_cod(cod, &lat, &lon, &depth_m, &has_depth);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "earthquake_id", eid->valuestring);
    if (mag) cJSON_AddStringToObject(props, "magnitude", mag);
    if (anm) cJSON_AddStringToObject(props, "place", anm);
    /* 最大震度 — the single most-read number on a JP quake bulletin, and it
     * was being dropped along with depth, report serial and the English
     * strings the upstream row already carries. */
    if (maxi)   cJSON_AddStringToObject(props, "max_intensity", maxi);
    if (has_depth)
      cJSON_AddNumberToObject(props, "depth_km", -depth_m / 1000.0);
    if (ift)    cJSON_AddStringToObject(props, "report_kind", ift);
    if (ser)    cJSON_AddStringToObject(props, "report_serial", ser);
    if (rdt)    cJSON_AddStringToObject(props, "reported_at", rdt);
    if (acd)    cJSON_AddStringToObject(props, "area_code", acd);
    if (ttl)    cJSON_AddStringToObject(props, "bulletin", ttl);
    if (en_ttl) cJSON_AddStringToObject(props, "bulletin_en", en_ttl);
    if (en_anm) cJSON_AddStringToObject(props, "place_en", en_anm);
    {
      const cJSON *iv = cJSON_GetObjectItem(eq, "int");
      if (iv && cJSON_IsArray(iv) && cJSON_GetArraySize(iv) > 0)
        cJSON_AddItemToObject(props, "intensity_areas", cJSON_Duplicate(iv, 1));
    }
    cJSON_AddStringToObject(props, "source", "jma.go.jp");
    char *pj = cJSON_PrintUnformatted(props);

    /* A readable headline instead of the bare bulletin type, plus the
     * provenance link to the JMA detail page (both were missing). */
    char title[300], link[256], summary[320];
    snprintf(title, sizeof title, "%s%s%s%s%s",
             anm ? anm : (ttl ? ttl : "地震"),
             mag ? " M" : "", mag ? mag : "",
             maxi ? " 最大震度" : "", maxi ? maxi : "");
    if (has_depth)
      snprintf(summary, sizeof summary, "%s — %s 深さ%.0fkm",
               ttl ? ttl : "地震情報", anm ? anm : "", -depth_m / 1000.0);
    else
      snprintf(summary, sizeof summary, "%s — %s",
               ttl ? ttl : "地震情報", anm ? anm : "");
    if (jsonf)
      snprintf(link, sizeof link,
               "https://www.jma.go.jp/bosai/quake/data/%s", jsonf);
    else
      snprintf(link, sizeof link,
               "https://www.jma.go.jp/bosai/map.html#contents=earthquake_map");

    intel_item it = {0};
    it.remote_key   = eid->valuestring;
    it.title        = title;
    it.summary      = summary;
    it.link         = link;
    it.published_at = at ? at : rdt;
    it.record_type  = "jma-earthquake";
    it.lang         = "ja";
    it.has_geo      = geo;
    it.lat = lat; it.lon = lon;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    cJSON_Delete(props);
  }
  cJSON_Delete(arr);
  fprintf(stderr, "[jma-earthquake] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def jma_earthquake_def = {
  .id = "jma-earthquake", .collector = "environment",
  .name = "JMA Earthquake", .name_ja = "気象庁 地震情報",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(jma_earthquake_def)
