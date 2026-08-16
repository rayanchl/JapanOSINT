/* collectors/sources/reg2_nz_charities.c
 *
 * New Zealand Charities Register (Charities Services / DIA), OData service.
 *   GET https://www.odata.charities.govt.nz/Organisations?$top=100&$format=json
 *
 * OData v2 "verbose" JSON: the payload is {"d":[...]} (some deployments wrap it
 * as {"d":{"results":[...]}} — both shapes are accepted). Every navigation
 * property arrives as {"__deferred":{...}}; those objects are ignored and only
 * the scalar fields are read, so nothing is invented for the related
 * /Officers, /Activities, /Beneficiaries and /Groups sets.
 *
 * Emits per organisation, all straight from the record: Name,
 * CharityRegistrationNumber, CompaniesOfficeNumber (the pivot into the NZ
 * Companies Office), DateRegistered, DeregistrationDate, street/postal address
 * fields, Telephone1, IsIncorporated and every other scalar the row carried.
 * OData v2 dates may arrive as "/Date(1371427200000)/" — those are converted to
 * ISO from the epoch value the service itself supplied; anything else is passed
 * through unchanged.
 *
 * Keyless ($format=json is required, otherwise the service returns Atom XML).
 * The register publishes addresses, never coordinates, so no row claims geo (R2).
 * Licence: Charities Services (DIA) open OData service; the register is public
 * by statute.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

#define NZ_URL "https://www.odata.charities.govt.nz/Organisations?$top=100&$format=json"

/* "/Date(1371427200000)/" -> ISO date. Returns 1 when it converted. */
static int nz_odata_date(const char *v, char *out, size_t n) {
  if (!v || strncmp(v, "/Date(", 6) != 0) return 0;
  long long ms = atoll(v + 6);
  time_t secs = (time_t)(ms / 1000);
  struct tm g;
  gmtime_r(&secs, &g);
  strftime(out, n, "%Y-%m-%d", &g);
  return 1;
}

/* Scalar field as displayable text (handles the OData date wrapper). */
static const char *nz_text(const cJSON *row, const char *k, char *buf, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(row, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    if (nz_odata_date(v->valuestring, buf, n)) return buf;
    return v->valuestring;
  }
  if (cJSON_IsNumber(v)) { snprintf(buf, n, "%.10g", v->valuedouble); return buf; }
  return NULL;
}

static int nz_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, NZ_URL, 30000);
  if (!doc) { fprintf(stderr, "[nz-charities-register] fetch/parse failed\n"); return -1; }

  const cJSON *d = cJSON_GetObjectItem(doc, "d");
  const cJSON *arr = NULL;
  if (cJSON_IsArray(d)) arr = d;
  else if (cJSON_IsObject(d)) {
    const cJSON *res = cJSON_GetObjectItem(d, "results");
    if (cJSON_IsArray(res)) arr = res;
  }
  if (!arr) {
    fprintf(stderr, "[nz-charities-register] unexpected payload shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, arr) {
    if (!cJSON_IsObject(row)) continue;
    const char *name = jo_sv(row, "Name");
    if (!name) continue;                      /* no real name -> no row (R1) */

    char rb[64], cb[64], db[64], eb[64], cityb[64];
    const char *regno  = nz_text(row, "CharityRegistrationNumber", rb, sizeof rb);
    const char *cono   = nz_text(row, "CompaniesOfficeNumber", cb, sizeof cb);
    const char *regd   = nz_text(row, "DateRegistered", db, sizeof db);
    const char *dereg  = nz_text(row, "DeregistrationDate", eb, sizeof eb);
    const char *city   = nz_text(row, "StreetAddressCity", cityb, sizeof cityb);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "nz-charities-register");
    cJSON_AddStringToObject(props, "source", "odata.charities.govt.nz");
    for (const cJSON *f = row->child; f; f = f->next) {
      if (!f->string || !f->string[0]) continue;
      if (f->string[0] == '_') continue;                /* __metadata */
      if (cJSON_IsObject(f) || cJSON_IsArray(f)) continue; /* __deferred navs */
      if (cJSON_IsString(f) && f->valuestring && f->valuestring[0]) {
        char isod[32];
        if (nz_odata_date(f->valuestring, isod, sizeof isod))
          cJSON_AddStringToObject(props, f->string, isod);
        else
          cJSON_AddStringToObject(props, f->string, f->valuestring);
      } else if (cJSON_IsNumber(f)) {
        cJSON_AddNumberToObject(props, f->string, f->valuedouble);
      } else if (cJSON_IsBool(f)) {
        cJSON_AddBoolToObject(props, f->string, cJSON_IsTrue(f) ? 1 : 0);
      }
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[420];
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
             regno ? regno : "",
             cono ? " · Companies Office " : "", cono ? cono : "",
             city ? " · " : "", city ? city : "",
             dereg ? " · DEREGISTERED" : "");

    char link[200];
    if (regno)
      snprintf(link, sizeof link,
               "https://register.charities.govt.nz/CharitiesRegister/ViewCharity?accountId=%s",
               regno);
    else
      snprintf(link, sizeof link, "%s", NZ_URL);

    intel_item it = {0};
    it.remote_key      = regno ? regno : name;
    it.title           = name;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = regd;
    it.record_type     = "nz-charity";
    it.properties_json = pj;
    it.tags_json       = "[\"registry\",\"charity\",\"new-zealand\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[nz-charities-register] emitted %d\n", n);
  return 0;
}

static const source_def reg2_nz_charities_def = {
  .id = "nz-charities-register", .collector = "government",
  .name = "New Zealand Charities Register (OData)",
  .update_interval_sec = 86400, .run = nz_run,
  .category = "government", .type = "api",
  .url = "https://www.odata.charities.govt.nz/Organisations",
  .description = "Every registered and deregistered NZ charity with its "
                 "registration number, Companies Office number, addresses and "
                 "dates. Keyless.",
  .license = "Charities Services (DIA) open OData service; register is public "
             "by statute.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_nz_charities_def)
