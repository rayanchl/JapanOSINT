/* collectors/sources/reg2_sk_rpvs.c
 *
 * Slovakia — Register of Public Sector Partners (RPVS), the statutory register
 * every company contracting with the Slovak state must appear in, together with
 * its declared ultimate beneficial owners. Two on-demand entity pivots against
 * the Ministry of Justice OData v4 open-data service.
 *
 *  SK_RPVS_PARTNERS  (pivot: company name)
 *    https://rpvs.gov.sk/opendatav2/PartneriVerejnehoSektora?$filter=contains(ObchodneMeno,'<name>')&$format=json
 *    Emits: ObchodneMeno (registered partner name), Ico (company id),
 *           FormaOsoby (legal form), PlatnostOd / PlatnostDo (validity window),
 *           plus every other scalar field the record carried.
 *
 *  SK_RPVS_UBO       (pivot: person name)
 *    https://rpvs.gov.sk/opendatav2/KonecniUzivateliaVyhod?$filter=Priezvisko%20eq%20'<surname>'&$format=json
 *    Emits: Meno / Priezvisko (given name / surname), DatumNarodenia,
 *           JeVerejnyCinitel (public-official flag), Ico, validity window.
 *    The OData filter is an exact surname match, so a multi-token entity is
 *    reduced to its LAST token (the surname) before querying.
 *
 * Payload shape: {"@odata.context":...,"value":[ ... ]}. Personal-name fields
 * are null for legal persons — those rows simply carry no name.
 *
 * Keyless LIVE. Wrong-shaped / empty entity, or an upstream failure, is an
 * honest empty (return 0). Nothing is synthesized; no geo is claimed (the
 * register publishes addresses, never coordinates).
 *
 * Licence: statutory public register (Act 315/2016) published as open data by
 * the Ministry of Justice SR; public by design, no stated restriction on
 * programmatic access. This is public-record corporate-transparency data.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define RPVS_MAX 100

/* Double any single quote so it survives an OData string literal, then
 * %-encode the whole thing. malloc'd; caller frees. */
static char *rpvs_odata_arg(const char *s) {
  size_t n = strlen(s);
  char *q = (char *)malloc(n * 2 + 1);
  if (!q) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    q[j++] = s[i];
    if (s[i] == '\'') q[j++] = '\'';
  }
  q[j] = 0;
  char *enc = jo_urlencode(q);
  free(q);
  return enc;
}

/* Copy every scalar field of an OData record into `props`. */
static void rpvs_copy_scalars(cJSON *props, const cJSON *row) {
  for (const cJSON *f = row->child; f; f = f->next) {
    if (!f->string || f->string[0] == '@') continue;   /* @odata.* metadata */
    if (cJSON_IsString(f) && f->valuestring && f->valuestring[0])
      cJSON_AddStringToObject(props, f->string, f->valuestring);
    else if (cJSON_IsNumber(f))
      cJSON_AddNumberToObject(props, f->string, f->valuedouble);
    else if (cJSON_IsBool(f))
      cJSON_AddBoolToObject(props, f->string, cJSON_IsTrue(f) ? 1 : 0);
  }
}

/* Plausible company/person name pivot: no URL/email punctuation, >= 3 bytes. */
static int rpvs_name_ok(const char *s) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 160) return 0;
  if (strchr(s, '@') || strchr(s, '/') || strchr(s, '?') || strchr(s, '&'))
    return 0;
  return 1;
}

/* ------------------------------------------------------------------ partners */
static int rpvs_partners_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!rpvs_name_ok(q)) return 0;                /* wrong shape -> no-op */

  char *arg = rpvs_odata_arg(q);
  if (!arg) return 0;
  char url[900];
  snprintf(url, sizeof url,
           "https://rpvs.gov.sk/opendatav2/PartneriVerejnehoSektora"
           "?$filter=contains(ObchodneMeno,'%s')&$format=json", arg);
  free(arg);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "sk_rpvs_partners");
  if (!body) return 0;                           /* upstream said nothing */
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) { fprintf(stderr, "[sk_rpvs_partners] unparseable payload\n"); return -1; }

  const cJSON *arr = cJSON_GetObjectItem(doc, "value");
  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, arr) {
    if (n >= RPVS_MAX) break;
    if (!cJSON_IsObject(row)) continue;
    const char *name = jo_sv(row, "ObchodneMeno");
    if (!name) continue;                         /* no real name -> no row */

    char ico[32] = "";
    const cJSON *icov = cJSON_GetObjectItem(row, "Ico");
    if (cJSON_IsString(icov) && icov->valuestring) snprintf(ico, sizeof ico, "%s", icov->valuestring);
    else if (cJSON_IsNumber(icov)) snprintf(ico, sizeof ico, "%.0f", icov->valuedouble);

    const char *forma = jo_sv(row, "FormaOsoby");
    const char *od    = jo_sv(row, "PlatnostOd");
    const char *doo   = jo_sv(row, "PlatnostDo");

    char key[128];
    const cJSON *idv = cJSON_GetObjectItem(row, "Id");
    if (cJSON_IsNumber(idv)) snprintf(key, sizeof key, "rpvs-%.0f", idv->valuedouble);
    else if (cJSON_IsString(idv) && idv->valuestring) snprintf(key, sizeof key, "rpvs-%s", idv->valuestring);
    else snprintf(key, sizeof key, "rpvs-%s-%s", ico[0] ? ico : "noico", name);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "SK_RPVS_PARTNERS");
    cJSON_AddStringToObject(props, "source", "rpvs.gov.sk");
    cJSON_AddStringToObject(props, "query", q);
    rpvs_copy_scalars(props, row);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[400];
    snprintf(summary, sizeof summary, "RPVS partner%s%s%s%s%s%s%s%s",
             ico[0] ? " · ICO " : "", ico[0] ? ico : "",
             forma ? " · " : "", forma ? forma : "",
             od ? " · od " : "", od ? od : "",
             doo ? " · do " : "", doo ? doo : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = name;
    it.summary         = summary;
    it.body            = pj;
    it.link            = url;
    it.lang            = "sk";
    it.published_at    = od;
    it.record_type     = "sk-public-sector-partner";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"registry\",\"slovakia\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[sk_rpvs_partners] emitted %d\n", n);
  return 0;
}

/* ----------------------------------------------------------------- UBO/owners */
static int rpvs_ubo_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!rpvs_name_ok(q)) return 0;

  /* The OData filter is an exact surname match — take the last token. */
  const char *surname = q;
  for (const char *p = q; *p; p++)
    if (*p == ' ' || *p == '\t') surname = p + 1;
  if (!surname || strlen(surname) < 2) return 0;

  char *arg = rpvs_odata_arg(surname);
  if (!arg) return 0;
  char url[900];
  snprintf(url, sizeof url,
           "https://rpvs.gov.sk/opendatav2/KonecniUzivateliaVyhod"
           "?$filter=Priezvisko%%20eq%%20'%s'&$format=json", arg);
  free(arg);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "sk_rpvs_ubo");
  if (!body) return 0;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) { fprintf(stderr, "[sk_rpvs_ubo] unparseable payload\n"); return -1; }

  const cJSON *arr = cJSON_GetObjectItem(doc, "value");
  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, arr) {
    if (n >= RPVS_MAX) break;
    if (!cJSON_IsObject(row)) continue;
    const char *meno = jo_sv(row, "Meno");
    const char *prie = jo_sv(row, "Priezvisko");
    if (!prie && !meno) continue;                /* legal-person row: no name */

    char title[256];
    snprintf(title, sizeof title, "%s%s%s", meno ? meno : "",
             (meno && prie) ? " " : "", prie ? prie : "");

    const char *dob = jo_sv(row, "DatumNarodenia");
    const char *od  = jo_sv(row, "PlatnostOd");
    const cJSON *pub = cJSON_GetObjectItem(row, "JeVerejnyCinitel");
    int is_official = cJSON_IsBool(pub) ? (cJSON_IsTrue(pub) ? 1 : 0) : -1;

    char ico[32] = "";
    const cJSON *icov = cJSON_GetObjectItem(row, "Ico");
    if (cJSON_IsString(icov) && icov->valuestring) snprintf(ico, sizeof ico, "%s", icov->valuestring);
    else if (cJSON_IsNumber(icov)) snprintf(ico, sizeof ico, "%.0f", icov->valuedouble);

    /* This is the DEDUPE key, so truncating it silently merges distinct
     * people: `title` is up to 255 bytes and the buffer was 160, so two
     * beneficial owners whose names share a long prefix would collapse onto
     * one row. That is the SSLBL_GLOBAL failure the audit found (200 SHA1s
     * collapsed onto 27 rows because the key was cut short) — a truncated
     * key loses data with no error anywhere. Size the buffer to the real
     * worst case instead. */
    char key[352];
    const cJSON *idv = cJSON_GetObjectItem(row, "Id");
    if (cJSON_IsNumber(idv)) snprintf(key, sizeof key, "rpvs-ubo-%.0f", idv->valuedouble);
    else if (cJSON_IsString(idv) && idv->valuestring) snprintf(key, sizeof key, "rpvs-ubo-%s", idv->valuestring);
    else snprintf(key, sizeof key, "rpvs-ubo-%s-%s", title, dob ? dob : "");

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "SK_RPVS_UBO");
    cJSON_AddStringToObject(props, "source", "rpvs.gov.sk");
    cJSON_AddStringToObject(props, "query", q);
    cJSON_AddStringToObject(props, "surname_queried", surname);
    rpvs_copy_scalars(props, row);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[400];
    snprintf(summary, sizeof summary,
             "Beneficial owner%s%s%s%s%s",
             ico[0] ? " of ICO " : "", ico[0] ? ico : "",
             dob ? " · born " : "", dob ? dob : "",
             is_official == 1 ? " · PUBLIC OFFICIAL" : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.body            = pj;
    it.link            = url;
    it.lang            = "sk";
    it.published_at    = od;
    it.record_type     = "sk-beneficial-owner";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"beneficial-ownership\",\"slovakia\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[sk_rpvs_ubo] emitted %d\n", n);
  return 0;
}

static const source_def reg2_sk_rpvs_partners_def = {
  .id = "SK_RPVS_PARTNERS", .collector = "osint",
  .name = "Slovakia Register of Public Sector Partners (partners)",
  .update_interval_sec = 0, .run = rpvs_partners_run,
  .category = "government", .type = "api",
  .url = "https://rpvs.gov.sk/opendatav2/PartneriVerejnehoSektora",
  .description = "Every company that contracts with the Slovak state must be "
                 "listed in RPVS; the OData v4 feed returns the registered "
                 "partner, its ICO, legal form and validity window. Keyless.",
  .license = "Statutory public register published as open data by the Ministry "
             "of Justice SR; no stated restriction on programmatic access.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_sk_rpvs_partners_def)

static const source_def reg2_sk_rpvs_ubo_def = {
  .id = "SK_RPVS_UBO", .collector = "osint",
  .name = "Slovakia RPVS beneficial owners",
  .update_interval_sec = 0, .run = rpvs_ubo_run,
  .category = "government", .type = "api",
  .url = "https://rpvs.gov.sk/opendatav2/KonecniUzivateliaVyhod",
  .description = "Declared ultimate beneficial owners behind every partner of "
                 "the Slovak public sector, including a public-official flag — "
                 "a statutory beneficial-ownership register. Keyless.",
  .license = "Statutory public register (Act 315/2016) published as open data; "
             "the register is legally public by design.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_sk_rpvs_ubo_def)
