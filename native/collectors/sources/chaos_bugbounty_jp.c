/* collectors/cyber/sources/chaos_bugbounty_jp.c
 * Port of server/src/collectors/chaosBugbountyJp.js (createThreatIntelCollector).
 * Keyless. Chaos index.json (array) → JP-program substring filter → program
 * Features at TOKYO. The CHAOS_DOWNLOAD_FULL subdomain-expansion path defaults
 * OFF (env-gated, multi-fetch) — we emit the default (program-only) output. */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define INDEX_URL "https://chaos-data.projectdiscovery.io/index.json"

static const char *JP_PROGRAMS[] = {
  "line", "mercari", "cybozu", "sansan", "freee", "money forward", "smarthr",
  "recruit", "rakuten", "cookpad", "gmo", "pixiv", "dwango", "klab", "paypay",
  "kakaku", "mufg", "mizuho", "smbc", "sbi", "softbank", "kddi", "ntt",
  "sony", "nintendo", "sega", "square enix", "capcom", "bandai", "konami",
  "nikkei", "asahi", "mainichi", "yomiuri", NULL };

static void lc(const char *s, char *o, size_t n) {
  size_t j = 0;
  if (!s) { o[0] = '\0'; return; }
  for (; *s && j + 1 < n; s++) o[j++] = (char)tolower((unsigned char)*s);
  o[j] = '\0';
}

static int is_jp_program(const char *name) {
  char n[512];
  lc(name, n, sizeof n);
  for (int i = 0; JP_PROGRAMS[i]; i++)
    if (strstr(n, JP_PROGRAMS[i])) return 1;
  return 0;
}

/* p.URL || null  → string or JSON null */
static cJSON *url_or_null(cJSON *p) {
  cJSON *v = cJSON_GetObjectItem(p, "URL");
  if (!v || cJSON_IsNull(v) || (cJSON_IsString(v) && !v->valuestring[0]))
    return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}
/* p.k ?? null  → value (incl. false/0/"") or JSON null only for null/absent */
static cJSON *nullish(cJSON *p, const char *k) {
  cJSON *v = cJSON_GetObjectItem(p, k);
  if (!v || cJSON_IsNull(v)) return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}
/* p.k || null  → JSON null for falsy (null/absent/""/0/false) */
static cJSON *or_null(cJSON *p, const char *k) {
  cJSON *v = cJSON_GetObjectItem(p, k);
  if (!v || cJSON_IsNull(v) ||
      (cJSON_IsString(v) && !v->valuestring[0]) ||
      (cJSON_IsBool(v) && !cJSON_IsTrue(v)) ||
      (cJSON_IsNumber(v) && v->valuedouble == 0))
    return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *index = feed_get_json_h(ctx->http, INDEX_URL, hdrs, 15000);
  if (!index) return NULL;

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(index)) {
    cJSON *p;
    cJSON_ArrayForEach(p, index) {
      cJSON *nm = cJSON_GetObjectItem(p, "name");
      if (!is_jp_program((nm && cJSON_IsString(nm)) ? nm->valuestring : NULL))
        continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      cJSON_AddStringToObject(pr, "kind", "program");
      cJSON *pn = cJSON_GetObjectItem(p, "name");
      cJSON_AddItemToObject(pr, "program_name",
        pn ? cJSON_Duplicate(pn, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(pr, "program_url", url_or_null(p));
      cJSON_AddItemToObject(pr, "bounty", nullish(p, "bounty"));
      cJSON_AddItemToObject(pr, "platform", nullish(p, "platform"));
      cJSON *du = cJSON_GetObjectItem(p, "URL");      /* download_url = p.URL */
      cJSON_AddItemToObject(pr, "download_url",
        du ? cJSON_Duplicate(du, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(pr, "last_updated", or_null(p, "last_updated"));
      cJSON_AddItemToObject(pr, "change", nullish(p, "change"));
      cJSON *dom = cJSON_GetObjectItem(p, "domains");
      cJSON_AddItemToObject(pr, "domains",
        cJSON_IsArray(dom) ? cJSON_Duplicate(dom, 1) : cJSON_CreateArray());
      cJSON_AddStringToObject(pr, "source", "chaos_index");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(index);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def chaos_bugbounty_jp_def = {
  .id = "chaos-bugbounty-jp", .collector = "cyber",
  .name = "Chaos (JP bug bounty)", .name_ja = "Chaos (日本バグバウンティ)",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(chaos_bugbounty_jp_def)
