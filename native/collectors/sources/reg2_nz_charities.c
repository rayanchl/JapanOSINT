/* collectors/sources/reg2_nz_charities.c
 *
 * New Zealand Charities Register (Charities Services / DIA), OData service.
 *   GET https://www.odata.charities.govt.nz/Organisations?$format=json
 *       &$orderby=OrganisationId&$top=5000&$skip=N   (the whole register)
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
#include "_timefmt.inc"

#define NZ_BASE "https://www.odata.charities.govt.nz/Organisations"
/* The WHOLE register, in pages. This read `$top=100` once — the same 100 of
 * 46,144 charities every run, with nothing saying so. The service honours
 * $top up to at least 5,000, $skip, and $orderby (a stable walk), and
 * $inlinecount=allpages states its own total, which the walk is checked
 * against (measured 2026-10-25: 46,144). */
#define NZ_PAGE 5000
#define NZ_PAGES_GUARD 200   /* exhaustive-ok: runaway guard on a self-declared total (46,144 = 10 pages); biting files a truncation notice */

/* "/Date(1371427200000)/" -> ISO date. Returns 1 when it converted.
 * The milliseconds come from atoll() over an upstream string, so they are
 * whatever the register (or a mangled response) put there. Claiming success
 * for a date that did not render would publish stack contents as a charity's
 * registration date; returning 0 makes the caller fall back to the raw
 * "/Date(…)/" text, which is at least what was actually received. */
static int nz_odata_date(const char *v, char *out, size_t n) {
  if (!v || strncmp(v, "/Date(", 6) != 0) return 0;
  long long ms = atoll(v + 6);
  time_t secs = (time_t)(ms / 1000);
  if (!jo_time_fmt(secs, "%Y-%m-%d", out, n)) return 0;
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

/* One page's organisations -> intel_items. Returns how many were emitted. */
static int nz_emit_rows(const cJSON *arr, intel_sink *sink) {
  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, arr) {
    if (!cJSON_IsObject(row)) continue;
    const char *name = jo_sv(row, "Name");

    char rb[64], cb[64], db[64], eb[64], cityb[64], ab[64];
    const char *regno  = nz_text(row, "CharityRegistrationNumber", rb, sizeof rb);
    const char *cono   = nz_text(row, "CompaniesOfficeNumber", cb, sizeof cb);
    const char *regd   = nz_text(row, "DateRegistered", db, sizeof db);
    const char *dereg  = nz_text(row, "DeregistrationDate", eb, sizeof eb);
    const char *city   = nz_text(row, "StreetAddressCity", cityb, sizeof cityb);
    const char *acct   = nz_text(row, "AccountId", ab, sizeof ab);

    /* The register publishes a few organisations with NO name — 4 of 46,144
     * on 2026-10-25, all deregistered, one carrying the Commission's note that
     * the trust asked for its information to be restricted. They are real
     * records (deregistration date and reason, Companies Office number, NZBN),
     * so they are stored, titled by what the register DID publish rather than
     * by an invented name. A row with nothing at all to key on is the only one
     * skipped, and the run discloses it (n < rows). */
    const cJSON *oid = cJSON_GetObjectItem(row, "OrganisationId");
    char orgk[48] = "";
    if (cJSON_IsNumber(oid)) snprintf(orgk, sizeof orgk, "%.0f", oid->valuedouble);
    char untitled[96];
    if (!name) {
      if (!regno && !orgk[0]) continue;
      snprintf(untitled, sizeof untitled, "Charity with no published name (%s%s)",
               regno ? regno : "OrganisationId ", regno ? "" : orgk);
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "nz-charities-register");
    cJSON_AddStringToObject(props, "source", "odata.charities.govt.nz");
    if (!name) cJSON_AddBoolToObject(props, "name_published", 0);
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

    /* The register's page is keyed on the AccountId GUID. It used to be built
     * from the registration number, which redirects to the search page
     * (measured 2026-10-25: accountId=CC49286 -> /Search, the GUID -> that
     * charity's summary), so every stored link was a dead end. */
    char link[200];
    if (acct)
      snprintf(link, sizeof link,
               "https://register.charities.govt.nz/CharitiesRegister/ViewCharity?accountId=%s",
               acct);
    else
      snprintf(link, sizeof link, "%s", NZ_BASE);

    intel_item it = {0};
    /* The registration number, else the service's own OrganisationId: a
     * name is not an identity (two charities can share one). */
    char orgkey[56] = "";
    if (orgk[0]) snprintf(orgkey, sizeof orgkey, "org:%s", orgk);
    it.remote_key      = regno ? regno : (orgkey[0] ? orgkey : name);
    it.title           = name ? name : untitled;
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

  return n;
}

static int nz_run(const source_ctx *ctx, intel_sink *sink) {
  long total = -1, rows = 0;
  int n = 0, page = 0, pages_ok = 0, failed = 0, cancelled = 0;
  char url[512];
  for (long skip = 0; page < NZ_PAGES_GUARD; skip += NZ_PAGE, page++) {
    if (ctx->cancel && *ctx->cancel) { cancelled = 1; break; }
    snprintf(url, sizeof url,
             NZ_BASE "?$format=json&$orderby=OrganisationId&$top=%d&$skip=%ld%s",
             NZ_PAGE, skip, page == 0 ? "&$inlinecount=allpages" : "");
    cJSON *doc = feed_get_json(ctx->http, url, 120000);
    if (!doc) {
      fprintf(stderr, "[nz-charities-register] page %d: fetch/parse failed\n", page + 1);
      if (page == 0) return -1;                   /* nothing read: an error */
      failed = 1;
      break;
    }
    const cJSON *d = cJSON_GetObjectItem(doc, "d");
    const cJSON *arr = NULL;
    if (cJSON_IsArray(d)) arr = d;
    else if (cJSON_IsObject(d)) {
      const cJSON *res = cJSON_GetObjectItem(d, "results");
      if (cJSON_IsArray(res)) arr = res;
      const cJSON *c = cJSON_GetObjectItem(d, "__count");
      if (page == 0 && cJSON_IsString(c)) total = strtol(c->valuestring, NULL, 10);
    }
    if (!arr) {
      fprintf(stderr, "[nz-charities-register] page %d: unexpected payload shape\n", page + 1);
      cJSON_Delete(doc);
      if (page == 0) return -1;
      failed = 1;
      break;
    }
    int got = cJSON_GetArraySize(arr);
    pages_ok++;
    rows += got;
    n += nz_emit_rows(arr, sink);
    cJSON_Delete(doc);
    if (got < NZ_PAGE) break;                     /* a short page: the end */
  }
  fprintf(stderr, "[nz-charities-register] emitted %d from %ld rows over %d page(s) "
          "(register total %ld)\n", n, rows, pages_ok, total);
  int cut = failed || cancelled || page >= NZ_PAGES_GUARD || (total >= 0 && rows < total);
  if (cut || n < rows) {
    /* Two different shortfalls, told apart in the data: a walk that stopped
     * before the register's own total (a re-run reads the rest), and rows
     * that were READ and not stored (a re-run changes nothing). */
    cJSON *x = cJSON_CreateObject();
    cJSON_AddNumberToObject(x, "rows_read", (double) rows);
    cJSON_AddNumberToObject(x, "rows_not_stored", (double) (rows - n));
    cJSON_AddNumberToObject(x, "pages_read", (double) pages_ok);
    cJSON_AddBoolToObject(x, "more_records_pending", cut);
    jo_truncation_notice_ex(sink, "nz-charities-register", NULL, n, total,
        failed ? "a later page failed; the pages read were emitted"
        : cancelled ? "the run was cancelled mid-walk"
        : page >= NZ_PAGES_GUARD ? "the walk reached its page guard"
        : cut ? "the walk ended short of the register's own total"
        : "every page was read and some rows were not stored: no name, "
          "registration number or OrganisationId to key on, or refused by the sink",
        cut ? "re-run; the register is read whole every run"
            : "read the run's stderr for EMIT-FAILED, and the register for rows "
              "with no OrganisationId",
        x);
  }
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
