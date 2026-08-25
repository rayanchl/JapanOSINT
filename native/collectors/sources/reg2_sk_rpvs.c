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
 * Payload shape: {"@odata.context":...,"@odata.count":N,"value":[ ... 20 rows ],
 * "@odata.nextLink":...}. Personal-name fields are null for legal persons —
 * those rows simply carry no name.
 *
 * PAGED. The service hands over twenty records at a time and names the next
 * page itself; `$top` is refused with a 400, so following `@odata.nextLink` is
 * the only way to the rest. Both pivots walk it to the end and ask for
 * `$count=true` so the disclosure below can quote the upstream's own total
 * rather than a guess.
 *
 * Keyless LIVE. Wrong-shaped / empty entity, or an upstream failure, is an
 * honest empty (return 0). Nothing is synthesized; no geo is claimed (the
 * register publishes addresses, never coordinates).
 *
 * Licence: statutory public register (Act 315/2016) published as open data by
 * the Ministry of Justice SR; public by design, no stated restriction on
 * programmatic access. This is public-record corporate-transparency data.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* The RPVS OData service pages at TWENTY records and says so with an
 * `@odata.nextLink`; `$top` is rejected outright (HTTP 400), so the link is
 * the only way through. Measured 2026-08-24:
 *
 *   $filter=contains(ObchodneMeno,'a')&$count=true
 *     -> "@odata.count": 27393, "value": [20 rows], "@odata.nextLink": ...
 *
 * The old `#define RPVS_MAX 100` never even bit — the walk stopped at the
 * server's first page long before the cap, so a pivot that matched 27,393
 * partners stored 20 of them with nothing in the output to say so. The cap
 * below is a runaway guard on the WALK, not on the records, and hitting it is
 * disclosed as a collector-truncation-notice against the upstream's own
 * `@odata.count`. $JO_RPVS_PAGE_MAX raises it. */
#define RPVS_PAGE_MAX 60      /* exhaustive-ok: page-walk runaway guard; an early stop emits a collector-truncation-notice carrying @odata.count */

static int rpvs_page_max(void) {
  const char *e = getenv("JO_RPVS_PAGE_MAX");
  if (e && *e) { int v = atoi(e); if (v > 0) return v; }
  return RPVS_PAGE_MAX;
}

/* The upstream's own total for this filter, or -1 when it declined to say.
 * Never estimated — an invented "available" is worse than none (rule 8). */
static long rpvs_declared_count(const cJSON *doc) {
  const cJSON *c = cJSON_GetObjectItem(doc, "@odata.count");
  if (cJSON_IsNumber(c)) return (long) c->valuedouble;
  return -1;
}

/* `@odata.nextLink` is an absolute URL the server built; we follow it
 * verbatim rather than doing skiptoken arithmetic of our own. */
static char *rpvs_next_link(const cJSON *doc) {
  const cJSON *nl = cJSON_GetObjectItem(doc, "@odata.nextLink");
  if (cJSON_IsString(nl) && nl->valuestring && *nl->valuestring)
    return strdup(nl->valuestring);
  return NULL;
}

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
/* One page of `value`, emitted whole. `url` is the first-page URL and becomes
 * the row's link; `q` is the pivot term, recorded on every row. */
static int rpvs_emit_partners_page(intel_sink *sink, const cJSON *doc,
                                   const char *q, const char *url) {
  int n = 0;
  const cJSON *arr = cJSON_GetObjectItem(doc, "value");
  const cJSON *row;
  cJSON_ArrayForEach(row, arr) {
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
  return n;
}

static int rpvs_partners_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!rpvs_name_ok(q)) return 0;                /* wrong shape -> no-op */

  char *arg = rpvs_odata_arg(q);
  if (!arg) return 0;
  char url[900];
  snprintf(url, sizeof url,
           "https://rpvs.gov.sk/opendatav2/PartneriVerejnehoSektora"
           "?$filter=contains(ObchodneMeno,'%s')&$format=json&$count=true", arg);
  free(arg);

  const char *hdrs[] = { "Accept: application/json", NULL };
  const int page_max = rpvs_page_max();
  char *page = strdup(url);
  if (!page) return 0;

  int n = 0, pages = 0, truncated = 0;
  long available = -1;

  for (; page && pages < page_max; pages++) {
    char *body = jo_get(ctx, page, hdrs, "sk_rpvs_partners");
    if (!body) {
      /* A dead FIRST page is the upstream saying nothing, exactly as before.
       * A failure part-way through a walk is different: we already hold real
       * records, so we keep them and disclose the short walk below. */
      if (pages == 0) { free(page); return 0; }
      truncated = 1;
      break;
    }
    cJSON *doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
      fprintf(stderr, "[sk_rpvs_partners] unparseable payload\n");
      if (pages == 0) { free(page); return -1; }
      truncated = 1;
      break;
    }
    if (available < 0) available = rpvs_declared_count(doc);
    n += rpvs_emit_partners_page(sink, doc, q, url);

    char *next = rpvs_next_link(doc);
    cJSON_Delete(doc);
    free(page);
    page = next;
    if (page && pages + 1 >= page_max) truncated = 1;   /* ceiling, not upstream */
  }
  free(page);

  if (truncated)
    jo_trunc_notice(sink, "SK_RPVS_PARTNERS", url, n, available,
                    "the RPVS OData walk stopped before the upstream ran out "
                    "of pages (page ceiling, or a mid-walk fetch failure)",
                    "raise $JO_RPVS_PAGE_MAX, or narrow the ObchodneMeno filter");

  fprintf(stderr, "[sk_rpvs_partners] emitted %d across %d page(s)%s\n",
          n, pages, truncated ? " (TRUNCATED — notice emitted)" : "");
  return 0;
}

/* ----------------------------------------------------------------- UBO/owners */
static int rpvs_emit_ubo_page(intel_sink *sink, const cJSON *doc, const char *q,
                              const char *surname, const char *url) {
  int n = 0;
  const cJSON *arr = cJSON_GetObjectItem(doc, "value");
  const cJSON *row;
  cJSON_ArrayForEach(row, arr) {
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
  return n;
}

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
           "?$filter=Priezvisko%%20eq%%20'%s'&$format=json&$count=true", arg);
  free(arg);

  const char *hdrs[] = { "Accept: application/json", NULL };
  const int page_max = rpvs_page_max();
  char *page = strdup(url);
  if (!page) return 0;

  int n = 0, pages = 0, truncated = 0;
  long available = -1;

  for (; page && pages < page_max; pages++) {
    char *body = jo_get(ctx, page, hdrs, "sk_rpvs_ubo");
    if (!body) {
      if (pages == 0) { free(page); return 0; }
      truncated = 1;
      break;
    }
    cJSON *doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
      fprintf(stderr, "[sk_rpvs_ubo] unparseable payload\n");
      if (pages == 0) { free(page); return -1; }
      truncated = 1;
      break;
    }
    if (available < 0) available = rpvs_declared_count(doc);
    n += rpvs_emit_ubo_page(sink, doc, q, surname, url);

    char *next = rpvs_next_link(doc);
    cJSON_Delete(doc);
    free(page);
    page = next;
    if (page && pages + 1 >= page_max) truncated = 1;
  }
  free(page);

  if (truncated)
    jo_trunc_notice(sink, "SK_RPVS_UBO", url, n, available,
                    "the RPVS OData walk stopped before the upstream ran out "
                    "of pages (page ceiling, or a mid-walk fetch failure)",
                    "raise $JO_RPVS_PAGE_MAX, or query a rarer surname");

  fprintf(stderr, "[sk_rpvs_ubo] emitted %d across %d page(s)%s\n",
          n, pages, truncated ? " (TRUNCATED — notice emitted)" : "");
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
