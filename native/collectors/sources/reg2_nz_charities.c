/* collectors/sources/reg2_nz_charities.c
 *
 * New Zealand Charities Register (Charities Services / DIA), OData service.
 *   GET https://www.odata.charities.govt.nz/vOrganisations?$format=json
 *       &$orderby=OrganisationId&$top=5000&$skip=N   (the whole register)
 *   GET …/Organisations?$select=OrganisationId,ControlledByName,GroupId
 *       &$filter=ControlledByName ne null or GroupId ne null   (one request)
 *
 * vOrganisations is the register's own view of the same 46,144 organisations
 * with the lookups resolved: activities, sectors, beneficiaries, areas of
 * operation, sources of funds, legal structure, group name and the main
 * sector/activity/beneficiary, as text. It is also SMALLER than Organisations
 * (19 MB against 22 MB per 5,000, measured 2026-10-25), because it carries no
 * navigation links. It lacks two fields Organisations has — ControlledByName
 * (998 charities) and GroupId (2,155) — so those come from the second request
 * (2,976 rows, 0.8 MB) and are merged in by OrganisationId. One field differs
 * in spelling only: the view writes ExemptionComment where Organisations
 * wrote ExcemptionComment.
 *
 * Officers (589,390), annual returns (430,815) and groups are separate rows,
 * in collectors/pivot/table/hp3b56_nzcharities.c.
 *
 * OData v2 "verbose" JSON: the payload is {"d":[...]} or, with
 * $inlinecount, {"d":{"results":[...],"__count":"N"}} — both shapes are
 * accepted. Any navigation property ({"__deferred":{...}}) is skipped and
 * only the scalar fields are read.
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
#include "lib/pagewalk.h"     /* jo_page_max_for(): the operator's page brake */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"
#include "_timefmt.inc"

#define NZ_HOST "https://www.odata.charities.govt.nz"
#define NZ_BASE NZ_HOST "/vOrganisations"
#define NZ_EXTRA_URL NZ_HOST "/Organisations?$format=json" \
  "&$select=OrganisationId,ControlledByName,GroupId" \
  "&$filter=ControlledByName%20ne%20null%20or%20GroupId%20ne%20null" \
  "&$orderby=OrganisationId&$top=5000&$inlinecount=allpages"
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

/* The records array of one OData v2 page, either shape. */
static const cJSON *nz_rows(const cJSON *doc) {
  const cJSON *d = cJSON_GetObjectItem(doc, "d");
  if (cJSON_IsArray(d)) return d;
  if (cJSON_IsObject(d)) {
    const cJSON *res = cJSON_GetObjectItem(d, "results");
    if (cJSON_IsArray(res)) return res;
  }
  return NULL;
}

/* ControlledByName / GroupId for the organisations that have either — the two
 * fields vOrganisations does not carry — sorted by OrganisationId so each
 * charity finds its own with a binary search. */
typedef struct { long oid; char *controlled; long group; int has_group; } nz_extra;
typedef struct { nz_extra *v; size_t n, cap; long rows, total; int failed; } nz_extras;

static int nz_extra_cmp(const void *a, const void *b) {
  long x = ((const nz_extra *) a)->oid, y = ((const nz_extra *) b)->oid;
  return (x > y) - (x < y);
}

static void nz_extras_read(const source_ctx *ctx, int guard, nz_extras *ex) {
  memset(ex, 0, sizeof *ex);
  ex->total = -1;
  char url[600];
  for (long skip = 0, page = 0; page < guard; skip += NZ_PAGE, page++) {
    snprintf(url, sizeof url, "%s&$skip=%ld", NZ_EXTRA_URL, skip);
    cJSON *doc = feed_get_json(ctx->http, url, 120000);
    const cJSON *arr = doc ? nz_rows(doc) : NULL;
    if (!arr) {
      fprintf(stderr, "[nz-charities-register] ControlledByName/GroupId read: "
              "page %ld failed\n", page + 1);
      ex->failed = 1;
      cJSON_Delete(doc);
      return;
    }
    if (page == 0) ex->total = pw_odata_v2_count(doc);
    int got = cJSON_GetArraySize(arr);
    const cJSON *row;
    cJSON_ArrayForEach(row, arr) {
      const cJSON *oid = cJSON_GetObjectItem(row, "OrganisationId");
      if (!cJSON_IsNumber(oid)) continue;
      const char *cb = jo_sv(row, "ControlledByName");
      const cJSON *g = cJSON_GetObjectItem(row, "GroupId");
      if (ex->n == ex->cap) {
        size_t nc = ex->cap ? ex->cap * 2 : 4096;
        nz_extra *nv = realloc(ex->v, nc * sizeof *nv);
        if (!nv) { ex->failed = 1; cJSON_Delete(doc); return; }
        ex->v = nv; ex->cap = nc;
      }
      nz_extra *e = &ex->v[ex->n++];
      e->oid = (long) oid->valuedouble;
      e->controlled = cb ? strdup(cb) : NULL;
      e->has_group = cJSON_IsNumber(g);
      e->group = e->has_group ? (long) g->valuedouble : 0;
    }
    ex->rows += got;
    cJSON_Delete(doc);
    if (got < NZ_PAGE) break;                     /* a short page: the end */
  }
  qsort(ex->v, ex->n, sizeof *ex->v, nz_extra_cmp);
}

static const nz_extra *nz_extras_find(const nz_extras *ex, long oid) {
  nz_extra key = { .oid = oid };
  return ex->n ? bsearch(&key, ex->v, ex->n, sizeof *ex->v, nz_extra_cmp) : NULL;
}

static void nz_extras_free(nz_extras *ex) {
  for (size_t i = 0; i < ex->n; i++) free(ex->v[i].controlled);
  free(ex->v);
}

/* One page's organisations -> intel_items. Returns how many were emitted. */
static int nz_emit_rows(const cJSON *arr, const nz_extras *ex, intel_sink *sink) {
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
    if (orgk[0]) {
      const nz_extra *e = nz_extras_find(ex, (long) oid->valuedouble);
      if (e && e->controlled && !cJSON_GetObjectItem(props, "ControlledByName"))
        cJSON_AddStringToObject(props, "ControlledByName", e->controlled);
      if (e && e->has_group && !cJSON_GetObjectItem(props, "GroupId"))
        cJSON_AddNumberToObject(props, "GroupId", (double) e->group);
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
  /* The runaway guard, unless the operator's brake ($JO_PAGE_MAX) says less:
   * a bounded read discloses itself below like any other cut walk. */
  const int guard = jo_page_max_for(NZ_PAGES_GUARD);
  nz_extras ex;
  nz_extras_read(ctx, guard, &ex);

  long total = -1, rows = 0;
  int n = 0, page = 0, pages_ok = 0, failed = 0, cancelled = 0;
  char url[512];
  for (long skip = 0; page < guard; skip += NZ_PAGE, page++) {
    if (ctx->cancel && *ctx->cancel) { cancelled = 1; break; }
    snprintf(url, sizeof url,
             NZ_BASE "?$format=json&$orderby=OrganisationId&$top=%d&$skip=%ld%s",
             NZ_PAGE, skip, page == 0 ? "&$inlinecount=allpages" : "");
    cJSON *doc = feed_get_json(ctx->http, url, 120000);
    const cJSON *arr = doc ? nz_rows(doc) : NULL;
    if (!arr) {
      fprintf(stderr, "[nz-charities-register] page %d: %s\n", page + 1,
              doc ? "unexpected payload shape" : "fetch/parse failed");
      cJSON_Delete(doc);
      if (page == 0) { nz_extras_free(&ex); return -1; }  /* nothing read: an error */
      failed = 1;
      break;
    }
    if (page == 0) total = pw_odata_v2_count(doc);
    int got = cJSON_GetArraySize(arr);
    pages_ok++;
    rows += got;
    n += nz_emit_rows(arr, &ex, sink);
    cJSON_Delete(doc);
    if (got < NZ_PAGE) break;                     /* a short page: the end */
  }
  fprintf(stderr, "[nz-charities-register] emitted %d from %ld rows over %d page(s) "
          "(register total %ld; ControlledByName/GroupId for %zu of %ld)\n",
          n, rows, pages_ok, total, ex.n, ex.total);

  int cut = failed || cancelled || page >= guard || (total >= 0 && rows < total);
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
        : page >= guard ? "the walk reached its page ceiling ($JO_PAGE_MAX, "
                          "else the runaway guard)"
        : cut ? "the walk ended short of the register's own total"
        : "every page was read and some rows were not stored: no name, "
          "registration number or OrganisationId to key on, or refused by the sink",
        cut ? "re-run; the register is read whole every run"
            : "read the run's stderr for EMIT-FAILED, and the register for rows "
              "with no OrganisationId",
        x);
  }
  if (ex.failed || (ex.total >= 0 && ex.rows < ex.total)) {
    /* The records were stored, missing two fields. Said as data, because the
     * gap is otherwise invisible: a charity with no ControlledByName looks
     * exactly like one the register names no controller for. */
    cJSON *x = cJSON_CreateObject();
    cJSON *f = cJSON_AddArrayToObject(x, "fields_missing");
    cJSON_AddItemToArray(f, cJSON_CreateString("ControlledByName"));
    cJSON_AddItemToArray(f, cJSON_CreateString("GroupId"));
    cJSON_AddBoolToObject(x, "more_records_pending", 1);
    jo_truncation_notice_ex(sink, "nz-charities-register", "ControlledByName+GroupId",
        (long) ex.rows, ex.total,
        "the Organisations read that supplies ControlledByName and GroupId "
        "(vOrganisations carries neither) did not complete, so some of this "
        "run's charities lack those two fields",
        "re-run; it is one request of ~3,000 rows", x);
  }
  nz_extras_free(&ex);
  return 0;
}

static const source_def reg2_nz_charities_def = {
  .id = "nz-charities-register", .collector = "government",
  .name = "New Zealand Charities Register (OData)",
  .update_interval_sec = 86400, .run = nz_run,
  .category = "government", .type = "api",
  .url = "https://www.odata.charities.govt.nz/vOrganisations",
  .description = "Every registered and deregistered NZ charity with its "
                 "registration number, Companies Office number, addresses and "
                 "dates. Keyless.",
  .license = "Charities Services (DIA) open OData service; register is public "
             "by statute.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_nz_charities_def)
