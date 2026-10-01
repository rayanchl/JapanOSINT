/* collectors/sources/country_gov2.c
 * OSINT service — UK Parliament members search (keyless), pivot on ctx->entity
 * (a name). Honest-empty on failure.
 *   UK_PARLIAMENT  members-api.parliament.uk/api/Members/Search?Name= */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CGV_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  if (strcmp(ctx->source_id ? ctx->source_id : "", "UK_PARLIAMENT") != 0) return 0;
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url, "https://members-api.parliament.uk/api/Members/Search?Name=%s&take=20", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", CGV_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "UK_PARLIAMENT");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *items = cJSON_GetObjectItem(root, "items");
  int n = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *it0;
    cJSON_ArrayForEach(it0, items) {
      const cJSON *v = cJSON_GetObjectItem(it0, "value");
      if (!v) continue;
      const cJSON *idn = cJSON_GetObjectItem(v, "id");
      const char *name = jo_sv(v, "nameDisplayAs");
      const cJSON *party = cJSON_GetObjectItem(v, "latestParty");
      const char *pn = party ? jo_sv(party, "name") : NULL;
      const cJSON *house = cJSON_GetObjectItem(v, "latestHouseMembership");
      const char *from = house ? jo_sv(house, "membershipFrom") : NULL;   /* constituency or house */
      if (!name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", name);
      if (pn) cJSON_AddStringToObject(d, "party", pn);
      if (from) cJSON_AddStringToObject(d, "membership_from", from);
      cJSON_AddStringToObject(d, "source", "UK Parliament");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "UK_PARLIAMENT"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char rk[64], link[128] = {0};
      if (idn && cJSON_IsNumber(idn)) { snprintf(rk, sizeof rk, "ukmp:%.0f", idn->valuedouble); snprintf(link, sizeof link, "https://members.parliament.uk/member/%.0f", idn->valuedouble); }
      else snprintf(rk, sizeof rk, "ukmp:%s", name);
      char summ[160]; snprintf(summ, sizeof summ, "%s%s%s", pn ? pn : "", from ? " · " : "", from ? from : "");
      intel_item it = {0};
      it.remote_key = rk; it.title = name; it.summary = summ[0] ? summ : NULL;
      it.body = bj; it.link = link[0] ? link : NULL; it.lang = "en";
      it.record_type = "uk-mp"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"UK_PARLIAMENT\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[UK_PARLIAMENT] emitted %d\n", n);
  return 0;
}

static const source_def uk_parliament_def = {
  .id = "UK_PARLIAMENT", .collector = "osint",
  .name = "UK Parliament Members", .name_ja = "UK Parliament Members",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://members-api.parliament.uk",
  .description = "UK Parliament keyless member search by name (party, constituency/house).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(uk_parliament_def)
