/* collectors/cyber/sources/crtsh_historical.c
 * Port of server/src/collectors/crtshHistorical.js (createThreatIntelCollector).
 * Keyless. crt.sh historical certs for a curated JP target list, geometry null. */
#include "source.h"
#include "lib/threatintel.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DEFAULT_TARGETS (process.env.CRTSH_TARGETS comma list overrides). */
static const char *const DEFAULT_TARGETS[] = {
  "%.go.jp", "%.gov.jp",
  "%.mod.go.jp", "%.kantei.go.jp",
  "%.mufg.jp", "%.smbc.co.jp", "%.mizuho-fg.co.jp", "%.japanpost.jp",
  "%.rakuten.co.jp", "%.line.me", "%.softbank.jp",
  "%.toyota.co.jp", "%.sony.co.jp",
};
#define N_DEFAULT_TARGETS (int)(sizeof DEFAULT_TARGETS / sizeof DEFAULT_TARGETS[0])

/* minimal RFC3986 component encoder (encodeURIComponent-ish for our inputs). */
static void url_encode(const char *s, char *out, size_t cap) {
  static const char *hex = "0123456789ABCDEF";
  size_t o = 0;
  for (; *s && o + 4 < cap; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
      out[o++] = (char)c;
    } else {
      out[o++] = '%';
      out[o++] = hex[c >> 4];
      out[o++] = hex[c & 0xF];
    }
  }
  out[o] = 0;
}

static cJSON *dup_or_null(cJSON *r, const char *k) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  return (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  const char *pe = getenv("CRTSH_PER_TARGET");
  long per_target = (pe && *pe) ? strtol(pe, NULL, 10) : 60;
  if (per_target <= 0) per_target = 60;

  cJSON *features = cJSON_CreateArray();
  for (int qi = 0; qi < N_DEFAULT_TARGETS; qi++) {
    const char *target = DEFAULT_TARGETS[qi];
    char enc[256], url[384];
    url_encode(target, enc, sizeof enc);
    snprintf(url, sizeof url, "https://crt.sh/?output=json&q=%s", enc);
    cJSON *j = feed_get_json(ctx->http, url, 20000);
    if (!cJSON_IsArray(j)) { if (j) cJSON_Delete(j); continue; }

    int taken = 0;
    cJSON *c;
    cJSON_ArrayForEach(c, j) {
      if (taken >= per_target) break;
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "target", target);
      cJSON_AddItemToObject(p, "cert_id", dup_or_null(c, "id"));
      cJSON_AddItemToObject(p, "common_name", dup_or_null(c, "common_name"));
      cJSON_AddItemToObject(p, "name_value", dup_or_null(c, "name_value"));
      cJSON_AddItemToObject(p, "issuer_name", dup_or_null(c, "issuer_name"));
      cJSON_AddItemToObject(p, "not_before", dup_or_null(c, "not_before"));
      cJSON_AddItemToObject(p, "not_after", dup_or_null(c, "not_after"));
      cJSON_AddItemToObject(p, "entry_timestamp", dup_or_null(c, "entry_timestamp"));
      cJSON_AddItemToObject(p, "serial_number", dup_or_null(c, "serial_number"));
      cJSON_AddStringToObject(p, "source", "crt_sh_history");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      taken++;
    }
    cJSON_Delete(j);
  }
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def crtsh_historical_def = {
  .id = "crtsh-historical", .collector = "cyber",
  .name = "crt.sh historical (.jp)", .name_ja = "crt.sh 履歴 (.jp)",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(crtsh_historical_def)
