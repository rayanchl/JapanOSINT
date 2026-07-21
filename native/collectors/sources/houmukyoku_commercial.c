/* collectors/government/sources/houmukyoku_commercial.c
 * Port of server/src/collectors/houmukyokuCommercial.js (intelEnvelope).
 * Gated on HOUMUKYOKU_API_KEY (paid contracted API). Optional
 * HOUMUKYOKU_API_BASE override + HOUMUKYOKU_WATCHLIST (comma list). No key
 * or empty watchlist → 0 rows. uid = houmukyoku-commercial|<id>. No seed. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void uric(const char *s, char *out, size_t n) {
  size_t o = 0;
  for (; *s && o + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
      out[o++] = (char)c;
    } else {
      snprintf(out + o, n - o, "%%%02X", c);
      o += 3;
    }
  }
  out[o] = '\0';
}

static const char *sv(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

/* r.<k> ?? null (string/number passthrough) */
static cJSON *nn(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}

/* Emit one record `r` for watchlist entry `target`. Returns emit rc. */
static int emit_record(intel_sink *sink, cJSON *r, const char *target) {
  const char *id = sv(r, "corporateNumber");
  if (!id) id = sv(r, "registrationNumber");
  if (!id) id = sv(r, "id");
  if (!id) id = target;

  const char *title = sv(r, "companyName");
  if (!title) title = sv(r, "name");
  if (!title) title = id;
  const char *summ = sv(r, "address");
  if (!summ) summ = sv(r, "headOfficeAddress");

  char body[2048]; body[0] = '\0'; int bw = 0;
  const char *bparts[4] = {
    sv(r, "companyName"), sv(r, "address"),
    sv(r, "representative"), sv(r, "businessPurpose")
  };
  for (int k = 0; k < 4; k++) {
    if (bparts[k]) {
      if (bw) strncat(body, "\n", sizeof body - strlen(body) - 1);
      strncat(body, bparts[k], sizeof body - strlen(body) - 1);
      bw = 1;
    }
  }
  const char *bodyp = bw ? body : NULL;
  const char *link = sv(r, "detailUrl");
  const char *pub = sv(r, "registrationDate");
  if (!pub) pub = sv(r, "lastUpdated");

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("houmukyoku"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("commercial-registry"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("corporate"));
  char *tj = cJSON_PrintUnformatted(tags);

  cJSON *p = cJSON_CreateObject();                 /* EXACT JS key order */
  cJSON_AddItemToObject(p, "corporate_number", nn(r, "corporateNumber"));
  cJSON_AddItemToObject(p, "registration_number", nn(r, "registrationNumber"));
  cJSON_AddItemToObject(p, "representative", nn(r, "representative"));
  cJSON_AddItemToObject(p, "capital", nn(r, "capital"));
  cJSON_AddItemToObject(p, "status", nn(r, "status"));
  cJSON_AddStringToObject(p, "query", target);
  cJSON_AddStringToObject(p, "source", "houmukyoku_api");
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key     = id;
  it.title          = title;
  it.summary        = summ;
  it.body           = bodyp;
  it.link           = link;
  it.lang           = "ja";
  it.published_at   = pub;
  it.record_type    = "houmukyoku-commercial";
  it.properties_json = pj;
  it.tags_json      = tj;
  int rc = sink->emit(sink, &it);

  free(pj); free(tj);
  cJSON_Delete(p); cJSON_Delete(tags);
  return rc;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *apiKey = getenv("HOUMUKYOKU_API_KEY");
  if (!apiKey || !*apiKey) {
    fprintf(stderr, "[houmukyoku-commercial] gated (no HOUMUKYOKU_API_KEY)\n");
    return 0;
  }
  const char *base = getenv("HOUMUKYOKU_API_BASE");
  if (!base || !*base) base = "https://www1.touki.or.jp/api/v1/commercial";

  const char *wl = getenv("HOUMUKYOKU_WATCHLIST");
  if (!wl || !*wl) {
    fprintf(stderr, "[houmukyoku-commercial] no watchlist — unavailable\n");
    return -1;
  }
  char *wlcopy = strdup(wl);
  int n = 0, gotAny = 0;

  char *save = NULL;
  for (char *tok = strtok_r(wlcopy, ",", &save); tok;
       tok = strtok_r(NULL, ",", &save)) {
    while (*tok && isspace((unsigned char)*tok)) tok++;
    char *end = tok + strlen(tok);
    while (end > tok && isspace((unsigned char)end[-1])) *--end = '\0';
    if (!*tok) continue;
    const char *target = tok;

    char tenc[256];
    uric(target, tenc, sizeof tenc);
    char url[768];
    snprintf(url, sizeof url, "%s/registration?key=%s&query=%s",
             base, apiKey, tenc);
    cJSON *data = feed_get_json(ctx->http, url, 20000);
    if (!data) continue;
    gotAny = 1;

    /* records = data.records[] || data.results[] || (data.record?[record]:[]) */
    cJSON *records = cJSON_GetObjectItem(data, "records");
    if (!cJSON_IsArray(records)) records = cJSON_GetObjectItem(data, "results");
    if (cJSON_IsArray(records)) {
      cJSON *r;
      cJSON_ArrayForEach(r, records)
        if (emit_record(sink, r, target) >= 0) n++;
    } else {
      cJSON *single = cJSON_GetObjectItem(data, "record");
      if (single && cJSON_IsObject(single))
        if (emit_record(sink, single, target) >= 0) n++;
    }
    cJSON_Delete(data);
  }
  free(wlcopy);

  if (!gotAny) {
    fprintf(stderr, "[houmukyoku-commercial] unavailable\n");
    return -1;
  }
  fprintf(stderr, "[houmukyoku-commercial] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def houmukyoku_commercial_def = {
  .id = "houmukyoku-commercial", .collector = "government",
  .name = "Houmukyoku Commercial Registry (officers)",
  .name_ja = "\xE6\xB3\x95\xE5\x8B\x99\xE5\xB1\x80 \xE5\x95\x86\xE6\xA5\xAD\xE7\x99\xBB\xE8\xA8\x98",
   .update_interval_sec = 604800, .run = run,
};
REGISTER_SOURCE(houmukyoku_commercial_def)
