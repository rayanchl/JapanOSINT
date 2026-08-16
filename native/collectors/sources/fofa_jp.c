/* collectors/cyber/sources/fofa_jp.c
 * Port of server/src/collectors/fofaJp.js (intelEnvelope).
 * FOFA /api/v1/search/all, country="JP", positional rows decoded via FIELDS.
 * Gated on FOFA_API_KEY (JS: no key → empty → 0 rows); FOFA_EMAIL optional.
 * uid = fofa-jp|<ip>_<port>_<i>. published_at = r.lastupdatetime passthrough
 * (JS new Date(x).toISOString() is tz-coupled, not in uid/props). */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* base64(`country="JP"`) — DEFAULT_QUERY constant (FOFA_QUERY env not honored
 * here; JS default only). */
#define QBASE64 "Y291bnRyeT0iSlAi"

/* FOFA returns positional arrays; FIELDS order is load-bearing. */
static const char *const FIELDS[] = {
  "ip", "port", "protocol", "host", "domain", "title",
  "server", "product", "banner", "country_name", "city",
  "asn", "org", "lastupdatetime"
};
#define NF ((int)(sizeof FIELDS / sizeof FIELDS[0]))

static const char B64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* standard base64 (Buffer.from(s).toString('base64')) into malloc'd buffer. */
static char *b64(const char *s) {
  size_t n = strlen(s);
  size_t outn = 4 * ((n + 2) / 3);
  char *o = malloc(outn + 1);
  size_t j = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned a = (unsigned char)s[i];
    unsigned b = i + 1 < n ? (unsigned char)s[i + 1] : 0;
    unsigned c = i + 2 < n ? (unsigned char)s[i + 2] : 0;
    unsigned v = (a << 16) | (b << 8) | c;
    o[j++] = B64[(v >> 18) & 63];
    o[j++] = B64[(v >> 12) & 63];
    o[j++] = (i + 1 < n) ? B64[(v >> 6) & 63] : '=';
    o[j++] = (i + 2 < n) ? B64[v & 63] : '=';
  }
  o[j] = 0;
  return o;
}

/* arr[i] ?? null : non-null cJSON string → its text; else NULL. */
static const char *cell(const cJSON *row, int i) {
  const cJSON *v = cJSON_GetArrayItem(row, i);
  if (!v || cJSON_IsNull(v)) return NULL;
  if (cJSON_IsString(v)) return v->valuestring;
  return NULL;
}
/* whole-cell as a display string ("null" when JS would template undefined). */
static void cell_or_null(const cJSON *row, int i, char *buf, size_t bn) {
  const char *s = cell(row, i);
  snprintf(buf, bn, "%s", s ? s : "null");
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("FOFA_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[fofa-jp] gated (no FOFA_API_KEY)\n");
    return 0;
  }
  const char *email = getenv("FOFA_EMAIL");
  if (!email) email = "";

  /* params order: email, key, qbase64, size, fields */
  char fieldscsv[256]; fieldscsv[0] = 0;
  for (int i = 0; i < NF; i++) {
    if (i) strncat(fieldscsv, ",", sizeof fieldscsv - strlen(fieldscsv) - 1);
    strncat(fieldscsv, FIELDS[i], sizeof fieldscsv - strlen(fieldscsv) - 1);
  }
  char url[768];
  snprintf(url, sizeof url,
    "https://fofa.info/api/v1/search/all"
    "?email=%s&key=%s&qbase64=%s&size=100&fields=%s",
    email, key, QBASE64, fieldscsv);
  const char *hdrs[] = { "Accept: application/json", NULL };

  cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 20000);
  if (!data) return -1;

  /* data.error → JS throws → 0 items. */
  cJSON *err = cJSON_GetObjectItem(data, "error");
  if (err && ((cJSON_IsBool(err) && cJSON_IsTrue(err)) ||
              (cJSON_IsString(err) && err->valuestring && err->valuestring[0]))) {
    cJSON_Delete(data);
    fprintf(stderr, "[fofa-jp] FOFA error response; 0 rows\n");
    return 0;
  }

  cJSON *results = cJSON_GetObjectItem(data, "results");
  int n = 0, i = 0;
  if (cJSON_IsArray(results)) {
    cJSON *row;
    cJSON_ArrayForEach(row, results) {
      /* field indices per FIELDS order */
      const char *proto= cell(row, 2);
      const char *host = cell(row, 3);
      const char *title= cell(row, 5);
      const char *prod = cell(row, 7);
      const char *banner = cell(row, 8);
      const char *asn  = cell(row, 11);
      const char *lupd = cell(row, 13);

      /* uid: intelUid(SOURCE_ID, `${ip}_${port}_${i}`) — single candidate,
       * JS template renders null as "null". */
      char ipb[64], portb[64];
      cell_or_null(row, 0, ipb, sizeof ipb);
      cell_or_null(row, 1, portb, sizeof portb);
      char rk[160];
      snprintf(rk, sizeof rk, "%s_%s_%d", ipb, portb, i);

      /* title = r.host || r.title || `${ip}:${port}` */
      char tfallback[140]; const char *ttl = host ? host : title;
      if (!ttl) {
        snprintf(tfallback, sizeof tfallback, "%s:%s", ipb, portb);
        ttl = tfallback;
      }

      /* body = r.banner ? slice(0,1000) : null */
      char *body = NULL;
      if (banner) {
        size_t L = strlen(banner);
        if (L > 1000) L = 1000;
        body = malloc(L + 1);
        memcpy(body, banner, L);
        body[L] = 0;
      }

      /* summary = [product, title].filter(Boolean).join(' — ') || null */
      char summary[512]; summary[0] = 0; int wrote = 0;
      if (prod && prod[0]) { strncat(summary, prod,
            sizeof summary - 1); wrote = 1; }
      if (title && title[0]) {
        if (wrote) strncat(summary, " \xe2\x80\x94 ",
            sizeof summary - strlen(summary) - 1);
        strncat(summary, title, sizeof summary - strlen(summary) - 1);
        wrote = 1;
      }
      const char *summ = wrote ? summary : NULL;

      /* link = r.host ? fofa.info/result?qbase64=base64(`host="${host}"`) : null */
      char *link = NULL;
      if (host) {
        /* `host` is upstream text going into FOFA's own query syntax. A host
         * carrying a `"` or a `\` used to close the literal early and produce a
         * link that resolves to a different (or invalid) query — escape both,
         * the way reg_ua_prozorro.c does for its POST body. */
        size_t qn = strlen(host) * 2 + 10;
        char *q = malloc(qn);
        if (q) {
          size_t qi = 0;
          memcpy(q, "host=\"", 6); qi = 6;
          for (const char *p = host; *p; p++) {
            if (*p == '"' || *p == '\\') q[qi++] = '\\';
            q[qi++] = *p;
          }
          q[qi++] = '"'; q[qi] = 0;
          char *qb = b64(q);
          if (qb) {
            size_t ln = strlen(qb) + 48;
            link = malloc(ln);
            if (link) snprintf(link, ln,
              "https://fofa.info/result?qbase64=%s", qb);
            free(qb);
          }
          free(q);
        }
      }

      /* tags = ['fofa', proto?'proto:'+proto:null, asn?'asn:'+asn:null] */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("fofa"));
      if (proto && proto[0]) {
        char tb[96]; snprintf(tb, sizeof tb, "proto:%s", proto);
        cJSON_AddItemToArray(tags, cJSON_CreateString(tb));
      }
      if (asn && asn[0]) {
        char tb[96]; snprintf(tb, sizeof tb, "asn:%s", asn);
        cJSON_AddItemToArray(tags, cJSON_CreateString(tb));
      }
      char *tj = cJSON_PrintUnformatted(tags);

      /* properties — EXACT JS key order; each value = arr[idx] ?? null */
      cJSON *p = cJSON_CreateObject();
      const struct { const char *k; int idx; } pm[] = {
        {"ip",0},{"port",1},{"protocol",2},{"host",3},{"domain",4},
        {"server",6},{"product",7},{"city",10},{"asn",11},{"org",12}
      };
      for (int k = 0; k < (int)(sizeof pm / sizeof pm[0]); k++) {
        const char *cv = cell(row, pm[k].idx);
        cJSON_AddItemToObject(p, pm[k].k,
          cv ? cJSON_CreateString(cv) : cJSON_CreateNull());
      }
      char *pj = cJSON_PrintUnformatted(p);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = ttl;
      it.body           = body;
      it.summary        = summ;
      it.link           = link;
      it.lang           = "en";
      it.published_at   = lupd;          /* r.lastupdatetime (passthrough) */
      it.record_type    = "fofa-jp";
      it.properties_json = pj;
      it.tags_json      = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj); free(body); free(link);
      cJSON_Delete(p); cJSON_Delete(tags);
      i++;
    }
  }
  cJSON_Delete(data);
  fprintf(stderr, "[fofa-jp] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def fofa_jp_def = {
  .id = "fofa-jp", .collector = "cyber",
  .name = "FOFA (Japan)", .name_ja = "FOFA 日本",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(fofa_jp_def)
