/* collectors/telecom/sources/amateur_radio_repeaters.c
 * Port of server/src/collectors/amateurRadioRepeaters.js (tryLive path).
 * RepeaterBook CSV export for Japan → GeoJSON Point Features.
 * The SEED_REPEATERS / generateSeedData fallback is dropped (rule 7);
 * only live upstream rows are emitted. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define API_URL "https://www.repeaterbook.com/api/export.php?country=Japan"

/* JS String(Number): shortest decimal that round-trips (V8 ToString). We
 * approximate with the shortest %.<p>g (p=1..17) that re-parses identically,
 * then strip a trailing ".0"-style — matches integer/typical-decimal cases
 * (callsign+freq+lat+lon hash key parity). */
static void js_num(double d, char *out, size_t n) {
  if (d == (long long)d && fabs(d) < 1e15) {
    snprintf(out, n, "%lld", (long long)d);
    return;
  }
  for (int p = 1; p <= 17; p++) {
    char buf[40];
    snprintf(buf, sizeof buf, "%.*g", p, d);
    if (strtod(buf, NULL) == d) { snprintf(out, n, "%s", buf); return; }
  }
  snprintf(out, n, "%.17g", d);
}

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  size_t L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\r'||s[L-1]=='\n')) s[--L]=0;
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *text = feed_get_text(ctx->http, API_URL, 15000);
  if (!text || strlen(text) < 40) { free(text); return -1; }

  cJSON *features = cJSON_CreateArray();
  char *save = text;
  char *line;
  while ((line = strsep(&save, "\n")) != NULL) {
    /* JS: filter l.trim() && !startsWith('#') */
    char *raw = line;
    char rt[8192];
    snprintf(rt, sizeof rt, "%s", raw);
    char *tl = trim(rt);
    if (!*tl || tl[0] == '#') continue;

    /* split(',') — index into a working copy */
    char work[8192];
    snprintf(work, sizeof work, "%s", line);
    char *fld[16]; int nf = 0;
    char *p = work;
    while (nf < 16) {
      fld[nf++] = p;
      char *c = strchr(p, ',');
      if (!c) break;
      *c = 0; p = c + 1;
    }
    if (nf < 9) continue;

    char *call = trim(fld[0]);
    char *freqs = fld[1];
    char *lats  = fld[7];
    char *lons  = fld[8];
    char *e;
    double freq = strtod(freqs, &e); int fok = (e != freqs);
    double lat  = strtod(lats, &e);  int lok = (e != lats);
    double lon  = strtod(lons, &e);  int gok = (e != lons);
    if (!*call || !fok || !lok || !gok) continue;
    if (isnan(lat) || isnan(lon) || isnan(freq)) continue;

    char sf[40], sla[40], slo[40];
    js_num(freq, sf, sizeof sf);
    js_num(lat, sla, sizeof sla);
    js_num(lon, slo, sizeof slo);
    const char *parts[4] = { call, sf, sla, slo };
    char hk[21]; feed_hash_key(hk, parts, 4);
    char rid[64]; snprintf(rid, sizeof rid, "RPT_LIVE_%s", hk);

    /* name = `${call} ${parts[5]||''}`.trim() */
    char nm[8192];
    char *p5 = (nf > 5) ? trim(fld[5]) : (char *)"";
    snprintf(nm, sizeof nm, "%s %s", call, p5);
    char *name = trim(nm);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *pr = cJSON_CreateObject();          /* EXACT JS key order */
    cJSON_AddStringToObject(pr, "repeater_id", rid);
    cJSON_AddStringToObject(pr, "callsign", call);
    cJSON_AddStringToObject(pr, "name", name);
    cJSON_AddNumberToObject(pr, "freq_mhz", freq);
    cJSON_AddStringToObject(pr, "mode", "FM");
    cJSON_AddStringToObject(pr, "kind", "repeater");
    cJSON_AddStringToObject(pr, "country", "JP");
    cJSON_AddStringToObject(pr, "source", "repeaterbook_live");
    cJSON_AddItemToObject(f, "properties", pr);
    cJSON_AddItemToArray(features, f);
  }
  free(text);

  if (cJSON_GetArraySize(features) == 0) {     /* JS: return null → not live */
    cJSON_Delete(features);
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[amateur-radio-repeaters] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def amateur_radio_repeaters_def = {
  .id = "amateur-radio-repeaters", .collector = "telecom",
  .name = "Amateur Radio Repeaters", .name_ja = "アマチュア無線レピータ",
   .update_interval_sec = 2592000, .run = run,
};
REGISTER_SOURCE(amateur_radio_repeaters_def)
