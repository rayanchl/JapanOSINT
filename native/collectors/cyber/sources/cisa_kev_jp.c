/* collectors/cyber/sources/cisa_kev_jp.c
 * Port of server/src/collectors/cisaKevJp.js (createThreatIntelCollector).
 * Keyless. CISA KEV JSON → JP-vendor filter → vendor-HQ geo (or TOKYO). */
#include "../../../source.h"
#include "../../../lib/threatintel.h"
#include "../../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define KEV_URL "https://www.cisa.gov/sites/default/files/feeds/known_exploited_vulnerabilities.json"

static const char *JP_VENDORS[] = {
  "trend micro", "trendmicro",
  "fujitsu", "nec", "hitachi", "mitsubishi", "toshiba", "panasonic",
  "sony", "canon", "ricoh", "kyocera", "sharp", "olympus",
  "yokogawa", "omron", "denso", "fanuc",
  "cybozu", "rakuten", "line", "softbank", "kddi", "ntt",
  "buffalo", "yamaha", "i-o data", "io-data", "iodata", "elecom",
  "corega", "planex", "logitec",
  "justsystem", "justsystems", "ichitaro",
  "baidu-jp", "r-soft", "sannet",
  "silex", "allied telesis", "atworks",
  "movabletype", "sixapart",
  "a10 networks", "a10networks", NULL };

/* VENDOR_GEO insertion order (Object.entries iteration order in JS) */
static const struct { const char *k; double lon, lat; } VENDOR_GEO[] = {
  {"toshiba",139.7595,35.6627}, {"fujitsu",139.7460,35.6810},
  {"nec",139.7568,35.6594}, {"hitachi",139.7648,35.6824},
  {"mitsubishi",139.7649,35.6810}, {"panasonic",135.5023,34.7250},
  {"sony",139.7409,35.6310}, {"canon",139.6203,35.6713},
  {"ricoh",139.7457,35.6328}, {"kyocera",135.7681,34.9850},
  {"sharp",135.5161,34.6515}, {"yokogawa",139.5953,35.6796},
  {"omron",135.7493,34.9926}, {"denso",137.0167,35.0467},
  {"fanuc",138.7625,35.4700}, {"cybozu",139.6961,35.6907},
  {"rakuten",139.6303,35.6360}, {"line",139.6993,35.6955},
  {"softbank",139.7530,35.6606}, {"kddi",139.7367,35.6679},
  {"ntt",139.7438,35.6831}, {"buffalo",136.9066,35.1815},
  {"yamaha",137.7261,34.7034}, {"iodata",136.6256,36.5947},
  {"elecom",135.5114,34.6863}, {"trendmicro",139.7517,35.6805},
  {"justsystems",134.5538,34.0666}, {"movabletype",139.7129,35.6593},
  {NULL,0,0} };

/* lowercase, strip non [a-z0-9] */
static void slug(const char *s, char *o, size_t n) {
  size_t j = 0;
  if (!s) { o[0] = '\0'; return; }
  for (; *s && j + 1 < n; s++) {
    unsigned char c = (unsigned char)tolower((unsigned char)*s);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o[j++] = (char)c;
  }
  o[j] = '\0';
}
/* lowercase only (keep all chars) */
static void lc(const char *s, char *o, size_t n) {
  size_t j = 0;
  if (!s) { o[0] = '\0'; return; }
  for (; *s && j + 1 < n; s++) o[j++] = (char)tolower((unsigned char)*s);
  o[j] = '\0';
}

static const char *S(cJSON *r, const char *k) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int vendor_matches(const char *vendor, const char *product) {
  char blob[1024];
  char lv[512], lp[512];
  lc(vendor, lv, sizeof lv);
  lc(product, lp, sizeof lp);
  snprintf(blob, sizeof blob, "%s %s", lv, lp);
  for (int i = 0; JP_VENDORS[i]; i++)
    if (strstr(blob, JP_VENDORS[i])) return 1;
  return 0;
}

static void vendor_geo(const char *vendor, double *lon, double *lat) {
  char v[256];
  slug(vendor, v, sizeof v);
  for (int i = 0; VENDOR_GEO[i].k; i++) {
    if (strstr(v, VENDOR_GEO[i].k)) {
      *lon = VENDOR_GEO[i].lon; *lat = VENDOR_GEO[i].lat; return;
    }
  }
  *lon = TOKYO_LON; *lat = TOKYO_LAT;
}

/* v.k || null (string) */
static void put_s(cJSON *p, const char *k, cJSON *r, const char *ik) {
  const char *s = S(r, ik);
  if (s && s[0]) cJSON_AddStringToObject(p, k, s);
  else cJSON_AddItemToObject(p, k, cJSON_CreateNull());
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *json = feed_get_json_h(ctx->http, KEV_URL, hdrs, 15000);
  if (!json) return NULL;

  cJSON *vulns = cJSON_GetObjectItem(json, "vulnerabilities");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(vulns)) {
    cJSON *v;
    cJSON_ArrayForEach(v, vulns) {
      const char *vp = S(v, "vendorProject");
      const char *pd = S(v, "product");
      if (!vendor_matches(vp, pd)) continue;

      double lon, lat;
      vendor_geo(vp, &lon, &lat);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      put_s(pr, "cve_id", v, "cveID");
      put_s(pr, "vendor", v, "vendorProject");
      put_s(pr, "product", v, "product");
      put_s(pr, "title", v, "vulnerabilityName");
      put_s(pr, "added", v, "dateAdded");
      put_s(pr, "due", v, "dueDate");
      const char *kr = S(v, "knownRansomwareCampaignUse");
      cJSON_AddItemToObject(pr, "ransomware",
        cJSON_CreateBool(kr && strcmp(kr, "Known") == 0));
      const char *sd = S(v, "shortDescription");
      if (sd) {
        char buf[401];
        size_t len = strlen(sd);
        if (len > 400) len = 400;
        memcpy(buf, sd, len);
        buf[len] = '\0';
        cJSON_AddStringToObject(pr, "description", buf);
      } else {
        cJSON_AddStringToObject(pr, "description", "");
      }
      put_s(pr, "required_action", v, "requiredAction");
      cJSON *cwes = cJSON_GetObjectItem(v, "cwes");
      cJSON_AddItemToObject(pr, "cwes",
        cJSON_IsArray(cwes) ? cJSON_Duplicate(cwes, 1) : cJSON_CreateArray());
      cJSON *notes = cJSON_GetObjectItem(v, "notes");
      cJSON_AddItemToObject(pr, "notes",
        (notes && !cJSON_IsNull(notes) &&
         !(cJSON_IsString(notes) && !notes->valuestring[0]))
          ? cJSON_Duplicate(notes, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(pr, "source", "cisa_kev");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def cisa_kev_jp_def = {
  .id = "cisa-kev-jp", .collector = "cyber",
  .name = "CISA KEV (JP vendors)", .name_ja = "CISA KEV (日本ベンダー)",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cisa_kev_jp_def)
