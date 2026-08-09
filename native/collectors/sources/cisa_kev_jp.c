/* collectors/cyber/sources/cisa_kev_jp.c
 * Keyless. CISA KEV JSON → JP-vendor filter.
 *
 * AUDIT NOTE (slice a3): this used to pin every row. A CVE is not a place, and
 * CISA publishes no coordinate, so the pin came from a static VENDOR_GEO table
 * compiled into the binary: the vendor's head office, falling back to Tokyo
 * Station for any vendor not in the table. That produced 46 "exploited
 * vulnerability" markers sitting on ~10 Tokyo office buildings — a map that
 * asserts the exploitation happened at Fujitsu HQ, which is not what KEV says.
 * Both the table and the fallback were invented locations, so no geometry is
 * emitted now; `vendor` carries the real (non-spatial) attribution.
 *
 * Also: the row uid was the sha1 hash fallback over {geometry, properties},
 * and properties led with "idx" = the row's POSITION in the filtered list. Any
 * KEV insertion re-numbered every later row and thus changed its uid, so
 * re-runs inserted duplicates instead of updating. Keyed on cve_id now. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

static const char *S(cJSON *r, const char *k) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int vendor_matches(const char *vendor, const char *product) {
  char blob[1024];
  char lv[512], lp[512];
  jo_lower_buf(vendor, lv, sizeof lv);
  jo_lower_buf(product, lp, sizeof lp);
  snprintf(blob, sizeof blob, "%s %s", lv, lp);
  for (int i = 0; JP_VENDORS[i]; i++)
    if (strstr(blob, JP_VENDORS[i])) return 1;
  return 0;
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
  if (cJSON_IsArray(vulns)) {
    cJSON *v;
    cJSON_ArrayForEach(v, vulns) {
      const char *vp = S(v, "vendorProject");
      const char *pd = S(v, "product");
      if (!vendor_matches(vp, pd)) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* no "geometry" key at all: CISA publishes no coordinate for a KEV entry.
       * NB do NOT use cJSON_CreateNull() here — lib/geojson.c:235 serialises
       * whatever it finds under "geometry", so an explicit JSON null lands in
       * the geometry column as the 4-byte string "null". */

      cJSON *pr = cJSON_CreateObject();
      const char *cve = S(v, "cveID");
      cJSON_AddStringToObject(pr, "uid", (cve && cve[0]) ? cve : "");
      put_s(pr, "cve_id", v, "cveID");
      put_s(pr, "vendor", v, "vendorProject");
      put_s(pr, "product", v, "product");
      put_s(pr, "title", v, "vulnerabilityName");
      put_s(pr, "added", v, "dateAdded");
      put_s(pr, "due", v, "dueDate");
      /* dateAdded is the only date KEV carries; without it every row landed on
       * the timeline at the ingest time instead of when CISA catalogued it. */
      put_s(pr, "published_at", v, "dateAdded");
      const char *kr = S(v, "knownRansomwareCampaignUse");
      cJSON_AddItemToObject(pr, "ransomware",
        cJSON_CreateBool(kr && strcmp(kr, "Known") == 0));
      /* shortDescription used to be hard-cut at 400 bytes; KEV publishes the
       * whole thing and the tail is the part that names the affected versions. */
      const char *sd = S(v, "shortDescription");
      cJSON_AddStringToObject(pr, "description", sd ? sd : "");
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
