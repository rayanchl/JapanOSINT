/* collectors/cyber/sources/chaos_bugbounty_jp.c
 * Port of server/src/collectors/chaosBugbountyJp.js (createThreatIntelCollector).
 * Keyless. Chaos index.json (array) → JP-program substring filter → program
 * Features. The CHAOS_DOWNLOAD_FULL subdomain-expansion path defaults
 * OFF (env-gated, multi-fetch) — we emit the default (program-only) output.
 * These rows carry NO geometry: Chaos publishes no coordinates and a bug
 * bounty program is not a place (see the note in run_fetch). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INDEX_URL "https://chaos-data.projectdiscovery.io/index.json"

static const char *JP_PROGRAMS[] = {
  "line", "mercari", "cybozu", "sansan", "freee", "money forward", "smarthr",
  "recruit", "rakuten", "cookpad", "gmo", "pixiv", "dwango", "klab", "paypay",
  "kakaku", "mufg", "mizuho", "smbc", "sbi", "softbank", "kddi", "ntt",
  "sony", "nintendo", "sega", "square enix", "capcom", "bandai", "konami",
  "nikkei", "asahi", "mainichi", "yomiuri", NULL };

static int is_jp_program(const char *name) {
  char n[512];
  jo_lower_buf(name, n, sizeof n);
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
      /* NO geometry. The JS port pinned every program at Tokyo Station
       * (139.6917, 35.6895). A bug-bounty program is not a physical place,
       * and stacking N invented pins on Tokyo Station is worse than no pin:
       * it is a coordinate the upstream never supplied. Chaos publishes no
       * location, so this source honestly has none. The key is omitted
       * entirely (a JSON null would persist the string "null" as geometry). */

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

      /* lib/geojson.c reads title/link/uid off the properties bag. Without
       * them the row had no title, no provenance link, and a uid that was a
       * hash of the whole bag (so it churned whenever `change` or
       * `last_updated` moved). All three come from the fetched record. */
      const char *pname = (pn && cJSON_IsString(pn)) ? pn->valuestring : NULL;
      if (pname) {
        cJSON_AddStringToObject(pr, "uid", pname);   /* stable dedupe key */
        cJSON *bo = cJSON_GetObjectItem(p, "bounty");
        cJSON *pf = cJSON_GetObjectItem(p, "platform");
        const char *plat = (pf && cJSON_IsString(pf) && pf->valuestring[0])
                             ? pf->valuestring : NULL;
        char title[320];
        if (plat)
          snprintf(title, sizeof title, "%s bug bounty (%s%s)", pname, plat,
                   cJSON_IsTrue(bo) ? ", paid" : "");
        else
          snprintf(title, sizeof title, "%s bug bounty%s", pname,
                   cJSON_IsTrue(bo) ? " (paid)" : "");
        cJSON_AddStringToObject(pr, "title", title);
      }
      cJSON *pu = cJSON_GetObjectItem(p, "URL");
      if (pu && cJSON_IsString(pu) && pu->valuestring[0])
        cJSON_AddStringToObject(pr, "url", pu->valuestring);
      cJSON *lu = cJSON_GetObjectItem(p, "last_updated");
      if (lu && cJSON_IsString(lu) && lu->valuestring[0])
        cJSON_AddStringToObject(pr, "published_at", lu->valuestring);

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
