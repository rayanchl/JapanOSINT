/* collectors/cyber/sources/hudson_rock_jp.c
 * Port of server/src/collectors/hudsonRockJp.js (createThreatIntelCollector).
 * Keyless. HudsonRock Cavalier per-JP-domain infostealer summary, TOKYO points. */
#include "source.h"
#include "lib/threatintel.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HR_BASE "https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-domain"

static const char *const DEFAULT_DOMAINS[] = {
  "mufg.jp", "smbc.co.jp", "mizuho-fg.co.jp", "rakuten-bank.co.jp", "japanpost.jp",
  "ntt.co.jp", "docomo.ne.jp", "kddi.com", "softbank.jp", "iij.ad.jp", "rakuten.co.jp",
  "sony.co.jp", "panasonic.com", "fujitsu.com", "nec.com", "hitachi.co.jp",
  "mitsubishielectric.co.jp", "toshiba.co.jp", "canon.jp", "ricoh.co.jp",
  "toyota.co.jp", "honda.co.jp", "nissan.co.jp", "subaru.co.jp", "mazda.co.jp",
  "jal.co.jp", "ana.co.jp", "jr-east.co.jp", "jr-central.co.jp",
  "meti.go.jp", "mod.go.jp", "kantei.go.jp", "mofa.go.jp", "soumu.go.jp",
};
#define N_DEFAULT_DOMAINS (int)(sizeof DEFAULT_DOMAINS / sizeof DEFAULT_DOMAINS[0])

/* Copy data[k] into p under the same name, when upstream actually has it.
 * Missing keys are simply absent rather than a wall of JSON nulls. */
static void cp(cJSON *p, cJSON *data, const char *k) {
  cJSON *v = data ? cJSON_GetObjectItem(data, k) : NULL;
  if (v && !cJSON_IsNull(v)) cJSON_AddItemToObject(p, k, cJSON_Duplicate(v, 1));
}

/* data[k] as a long long, or -1 when absent/not a number. */
static long long num(cJSON *data, const char *k) {
  cJSON *v = data ? cJSON_GetObjectItem(data, k) : NULL;
  return (v && cJSON_IsNumber(v)) ? (long long)v->valuedouble : -1;
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < N_DEFAULT_DOMAINS; i++) {
    const char *domain = DEFAULT_DOMAINS[i];
    char url[320];
    snprintf(url, sizeof url, "%s?domain=%s", HR_BASE, domain);
    const char *hdrs[] = { "Accept: application/json", NULL };
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 15000);
    if (!data) {
      /* A failed lookup has nothing to say about the domain. Emitting a row of
       * JSON nulls with err="fetch_failed" (what the JS port did) buries a
       * title-less, value-less record in the map layer; log and skip instead. */
      fprintf(stderr, "[hudson-rock-jp] %s fetch failed\n", domain);
      continue;
    }

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
     * (35.6895, 139.6917). What this source reports has no location,
     * and stacking every row on one pin is the fabrication the
     * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
     * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
     * lib/geojson.c handles an absent geometry correctly — do NOT
     * reintroduce a fallback coordinate. */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "idx", i);
    cJSON_AddStringToObject(p, "domain", domain);
    /* Stable identity. Without a NATIVE_ID_KEY, lib/geojson.c uids the feature
     * by sha1({g,p}) — and p now carries live counts, so every changed count
     * would mint a NEW row instead of updating the domain's row. Keying on the
     * domain makes the upsert do its job. (One-time re-key on deploy.) */
    cJSON_AddStringToObject(p, "uid", domain);

    /* Title. lib/geojson.c picks title|name|name_ja|label off the properties;
     * this feature had none of them, so every row persisted with title=NULL
     * and was unreadable in the UI (the NO_TITLE verdict). Built from the
     * fetched counts, never from a constant. */
    long long emp = num(data, "employees"), usr = num(data, "users");
    long long tot = num(data, "total");
    char title[192];
    if (tot >= 0)
      snprintf(title, sizeof title,
               "%s — %lld compromised credentials (%lld employee, %lld user)",
               domain, tot, emp < 0 ? 0 : emp, usr < 0 ? 0 : usr);
    else
      snprintf(title, sizeof title, "%s — infostealer exposure", domain);
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddStringToObject(p, "link", url);   /* provenance: what we fetched */

    /* The v2 search-by-domain response uses total/totalStealers/totalUrls and
     * the *Passwords/stealerFamilies/thirdPartyDomains breakdowns. The port
     * asked for total_employees/total_users/stealers/total_external_domains/
     * external_domains/message, none of which exist in v2 — so five of the
     * nine carried fields were permanently JSON null while the real counts,
     * the compromise dates and the stealer-family breakdown were dropped on
     * the floor. Carry what the API actually returns. ("data" is skipped on
     * purpose: it is the 564-URL dump, ~20 KB per domain.) */
    static const char *const KEYS[] = {
      "total", "totalStealers", "employees", "users", "third_parties",
      "totalUrls", "is_shopify", "last_employee_compromised",
      "last_user_compromised", "stats", "employeePasswords", "userPasswords",
      "stealerFamilies", "thirdPartyDomains", "applications", "antiviruses",
      NULL };
    for (int k = 0; KEYS[k]; k++) cp(p, data, KEYS[k]);

    /* timeline placement from the most recent compromise we were told about */
    cJSON *le = cJSON_GetObjectItem(data, "last_employee_compromised");
    cJSON *lu = cJSON_GetObjectItem(data, "last_user_compromised");
    const char *pub = NULL;
    if (le && cJSON_IsString(le) && le->valuestring[0]) pub = le->valuestring;
    if (lu && cJSON_IsString(lu) && lu->valuestring[0] &&
        (!pub || strcmp(lu->valuestring, pub) > 0)) pub = lu->valuestring;
    if (pub) cJSON_AddStringToObject(p, "published_at", pub);

    cJSON_AddStringToObject(p, "source", "hudsonrock_osint");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    cJSON_Delete(data);
  }
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def hudson_rock_jp_def = {
  .id = "hudson-rock-jp", .collector = "cyber",
  .name = "HudsonRock (JP corps)", .name_ja = "HudsonRock 日本企業",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(hudson_rock_jp_def)
