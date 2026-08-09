/* collectors/government/sources/houjin_bangou.c
 * Port of server/src/collectors/houjinBangou.js (intelEnvelope).
 * NTA Houjin Bangou /4/diff (JSON type=12), rolling 7-day UTC window.
 * Gated on HOUJIN_BANGOU_KEY (JS: no key → empty → 0 rows).
 * uid = houjin-bangou|<cn || changeDate||updateDate>. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* a || b (string-truthy). */
static const char *sor(const cJSON *o, const char *a, const char *b) {
  const char *x = jo_sv(o, a);
  return x ? x : jo_sv(o, b);
}

/* corporateNumber may arrive as number or string → string-truthy form. */
static const char *cn_of(const cJSON *c, char *buf, size_t bn) {
  const cJSON *v = cJSON_GetObjectItem(c, "corporateNumber");
  if (!(v && ((cJSON_IsString(v) && v->valuestring && v->valuestring[0])
              || cJSON_IsNumber(v))))
    v = cJSON_GetObjectItem(c, "corporate_number");
  if (!v) return NULL;
  if (cJSON_IsString(v))
    return (v->valuestring && v->valuestring[0]) ? v->valuestring : NULL;
  if (cJSON_IsNumber(v)) {
    if (v->valuedouble == (double)(long long)v->valuedouble)
      snprintf(buf, bn, "%lld", (long long)v->valuedouble);
    else snprintf(buf, bn, "%g", v->valuedouble);
    return buf;
  }
  return NULL;
}

/* PROC code → label, else NULL (so caller falls back to proc itself). */
static const char *proc_label(const char *p) {
  if (!p) return NULL;
  if (!strcmp(p, "01")) return "new";
  if (!strcmp(p, "11")) return "name-change";
  if (!strcmp(p, "21")) return "address-change";
  if (!strcmp(p, "71")) return "closed";
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("HOUJIN_BANGOU_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[houjin-bangou] gated (no HOUJIN_BANGOU_KEY)\n");
    return 0;
  }

  /* to = now; from = now - 7d; ymd = toISOString().slice(0,10) (UTC). */
  time_t now = time(NULL);
  time_t from = now - 7 * 86400;
  struct tm gt, gf;
  gmtime_r(&now, &gt);
  gmtime_r(&from, &gf);
  char to_s[11], from_s[11];
  snprintf(to_s, sizeof to_s, "%04d-%02d-%02d",
           gt.tm_year + 1900, gt.tm_mon + 1, gt.tm_mday);
  snprintf(from_s, sizeof from_s, "%04d-%02d-%02d",
           gf.tm_year + 1900, gf.tm_mon + 1, gf.tm_mday);

  char url[512];
  snprintf(url, sizeof url,
    "https://api.houjin-bangou.nta.go.jp/4/diff"
    "?id=%s&from=%s&to=%s&type=12&history=0", key, from_s, to_s);
  const char *hdrs[] = { "Accept: application/json", NULL };

  cJSON *json = feed_get_json_h(ctx->http, url, hdrs, 20000);
  if (!json) return -1;

  /* corps = json.corporations || (Array.isArray(json)?json:[]) || json.data */
  cJSON *corps = cJSON_GetObjectItem(json, "corporations");
  if (!cJSON_IsArray(corps)) {
    if (cJSON_IsArray(json)) corps = json;
    else corps = cJSON_GetObjectItem(json, "data");
  }

  int n = 0, i = 0;
  if (cJSON_IsArray(corps)) {
    cJSON *c;
    cJSON_ArrayForEach(c, corps) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      i++;
      char cnbuf[40];
      const char *cn   = cn_of(c, cnbuf, sizeof cnbuf);
      const char *proc = jo_sv(c, "process");    /* c.process || null */
      const char *plab = proc_label(proc);
      const char *cdate= jo_sv(c, "changeDate");
      const char *udate= jo_sv(c, "updateDate");

      const char *pref = sor(c, "prefectureName", "prefecture_name");
      const char *city = sor(c, "cityName", "city_name");
      const char *name = jo_sv(c, "name");

      /* uid: intelUid(SOURCE_ID, cn, c.changeDate||c.updateDate) */
      const char *rk = cn ? cn : (cdate ? cdate : udate);
      if (!rk) continue;

      /* title = c.name || `Corporate #${cn}` */
      char titlebuf[64]; const char *title = name;
      if (!title) {
        snprintf(titlebuf, sizeof titlebuf, "Corporate #%s",
                 cn ? cn : "");
        title = titlebuf;
      }

      /* summary = [PROC[proc]||proc, pref, city].filter(Boolean)
       *           .join(' · ') || null */
      const char *procDisp = plab ? plab : proc;   /* PROC[proc]||proc */
      char summary[256]; summary[0] = 0; int wrote = 0;
      const char *parts[3] = { procDisp, pref, city };
      for (int k = 0; k < 3; k++) {
        if (parts[k] && parts[k][0]) {
          if (wrote) strncat(summary, " \xc2\xb7 ",
                             sizeof summary - strlen(summary) - 1);
          strncat(summary, parts[k],
                  sizeof summary - strlen(summary) - 1);
          wrote = 1;
        }
      }
      const char *summ = wrote ? summary : NULL;

      /* published_at = updateDate||update_date||changeDate||change_date||null */
      const char *pub = jo_sv(c, "updateDate");
      if (!pub) pub = jo_sv(c, "update_date");
      if (!pub) pub = cdate;
      if (!pub) pub = jo_sv(c, "change_date");

      /* tags = ['corporate-registry',
       *         proc ? `process:${PROC[proc]||proc}` : null].filter */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("corporate-registry"));
      if (proc) {
        char tb[96];
        snprintf(tb, sizeof tb, "process:%s", procDisp ? procDisp : "");
        cJSON_AddItemToArray(tags, cJSON_CreateString(tb));
      }
      char *tj = cJSON_PrintUnformatted(tags);

      /* properties — EXACT JS key order */
      cJSON *p = cJSON_CreateObject();
      cJSON_AddItemToObject(p, "corporate_number",
        cn ? cJSON_CreateString(cn) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "process",
        proc ? cJSON_CreateString(proc) : cJSON_CreateNull());
      const char *nkana = jo_sv(c, "furigana");
      cJSON_AddItemToObject(p, "name_kana",
        nkana ? cJSON_CreateString(nkana) : cJSON_CreateNull());
      const char *nen = sor(c, "nameEn", "name_en");
      cJSON_AddItemToObject(p, "name_en",
        nen ? cJSON_CreateString(nen) : cJSON_CreateNull());
      const char *knd = jo_sv(c, "kind");
      cJSON_AddItemToObject(p, "kind",
        knd ? cJSON_CreateString(knd) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "prefecture_name",
        pref ? cJSON_CreateString(pref) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "city_name",
        city ? cJSON_CreateString(city) : cJSON_CreateNull());
      const char *strno = sor(c, "streetNumber", "street_number");
      cJSON_AddItemToObject(p, "street_number",
        strno ? cJSON_CreateString(strno) : cJSON_CreateNull());
      const char *pcode = sor(c, "postCode", "post_code");
      cJSON_AddItemToObject(p, "post_code",
        pcode ? cJSON_CreateString(pcode) : cJSON_CreateNull());
      const char *cldt = sor(c, "closeDate", "close_date");
      cJSON_AddItemToObject(p, "close_date",
        cldt ? cJSON_CreateString(cldt) : cJSON_CreateNull());
      const char *clca = sor(c, "closeCause", "close_cause");
      cJSON_AddItemToObject(p, "close_cause",
        clca ? cJSON_CreateString(clca) : cJSON_CreateNull());
      const char *succ = sor(c, "successorCorporateNumber",
                               "successor_corporate_number");
      cJSON_AddItemToObject(p, "successor_corp",
        succ ? cJSON_CreateString(succ) : cJSON_CreateNull());
      char *pj = cJSON_PrintUnformatted(p);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = title;
      it.summary        = summ;
      it.lang           = "ja";
      it.published_at   = pub;
      it.record_type    = "houjin-bangou";
      it.properties_json = pj;
      it.tags_json      = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj);
      cJSON_Delete(p); cJSON_Delete(tags);
    }
  }
  cJSON_Delete(json);
  fprintf(stderr, "[houjin-bangou] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def houjin_bangou_def = {
  .id = "houjin-bangou", .collector = "government",
  .name = "Houjin Bangou (corp registry)", .name_ja = "法人番号 (国税庁)",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(houjin_bangou_def)
