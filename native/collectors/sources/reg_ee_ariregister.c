/* collectors/sources/reg_ee_ariregister.c
 * OSINT service — EE_ARIREGISTER. On-demand entity pivot against Estonia's
 * e-Business Register (Äriregister, run by RIK — Centre of Registers and
 * Information Systems). Keyless and LIVE via the public autocomplete endpoint
 *   GET https://ariregister.rik.ee/est/api/autocomplete?q=<entity>
 * which returns JSON candidates for a company name or 8-digit registry code.
 *
 * We emit one intel_item per matched company: registry code (registrikood),
 * name (nimi), and legal status when the API supplies it. Only real, parsed
 * fields are emitted; fetch failure / no matches → honest empty (return 0). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one company object → one intel_item. Returns 1 if emitted. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!cJSON_IsObject(c)) return 0;

  static const char *name_keys[] = { "nimi", "name", "ariregistri_nimi", "label", NULL };
  static const char *code_keys[] = { "registrikood", "reg_code", "code", "ariregistrikood", NULL };
  static const char *stat_keys[] = { "staatus", "status", "oiguslik_seisund", "seisund", NULL };

  const char *name = jo_first_sv(c, name_keys);
  const char *code = jo_first_sv(c, code_keys);
  const char *stat = jo_first_sv(c, stat_keys);
  if (!name && !code) return 0;

  /* registry code may arrive as a JSON number */
  char codebuf[32] = {0};
  if (!code) {
    const cJSON *rn = cJSON_GetObjectItem(c, "registrikood");
    if (!rn || !cJSON_IsNumber(rn)) rn = cJSON_GetObjectItem(c, "reg_code");
    if (rn && cJSON_IsNumber(rn)) {
      snprintf(codebuf, sizeof codebuf, "%lld", (long long)rn->valuedouble);
      code = codebuf;
    }
  }

  cJSON *data = cJSON_CreateObject();
  if (name) cJSON_AddStringToObject(data, "name", name);
  if (code) cJSON_AddStringToObject(data, "registry_code", code);
  if (stat) cJSON_AddStringToObject(data, "status", stat);
  cJSON_AddStringToObject(data, "country", "EE");
  cJSON_AddStringToObject(data, "source", "Ariregister");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EE_ARIREGISTER");
  cJSON_AddStringToObject(props, "source", "Ariregister");
  cJSON_AddStringToObject(props, "country", "EE");
  if (code) cJSON_AddStringToObject(props, "registry_code", code);
  if (stat) cJSON_AddStringToObject(props, "status", stat);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  /* real public profile URL, resolvable from the registry code */
  char link[128] = {0};
  if (code) snprintf(link, sizeof link, "https://ariregister.rik.ee/est/company/%s", code);

  intel_item it = {0};
  it.remote_key      = code ? code : name;
  it.title           = name ? name : code;
  it.summary         = stat;
  it.body            = bj;
  it.lang            = "et";
  it.link            = code ? link : NULL;
  it.record_type     = "ee-ariregister-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"EE_ARIREGISTER\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Walk an arbitrary JSON node, emitting any object that looks like a company
 * (has a name and/or registry code). Handles both a bare array response and a
 * wrapper object with a results/data/items array. */
static int walk(intel_sink *sink, const cJSON *node, int depth, int *count) {
  if (!node || depth > 4 || *count >= 30) return 0;
  int emitted = 0;
  if (cJSON_IsArray(node)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, node) {
      if (*count >= 30) break;
      if (cJSON_IsObject(e)) {
        int e1 = emit_company(sink, e);
        emitted += e1; *count += e1;
        if (!e1) emitted += walk(sink, e, depth + 1, count);
      } else if (cJSON_IsArray(e)) {
        emitted += walk(sink, e, depth + 1, count);
      }
    }
  } else if (cJSON_IsObject(node)) {
    const cJSON *ch;
    cJSON_ArrayForEach(ch, node) {
      if (*count >= 30) break;
      if (cJSON_IsArray(ch) || cJSON_IsObject(ch))
        emitted += walk(sink, ch, depth + 1, count);
    }
  }
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://ariregister.rik.ee/est/api/autocomplete?q=%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "ee-ariregister");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int count = 0;
  int emitted = walk(sink, root, 0, &count);
  cJSON_Delete(root);
  fprintf(stderr, "[ee-ariregister] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def ee_ariregister_def = {
  .id = "EE_ARIREGISTER", .collector = "osint",
  .name = "Estonia e-Business Register", .name_ja = "エストニア企業登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://ariregister.rik.ee",
  .description = "Estonia Äriregister (RIK) — keyless company lookup: registry code, name, status",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ee_ariregister_def)
