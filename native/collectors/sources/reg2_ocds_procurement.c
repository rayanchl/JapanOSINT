/* collectors/sources/reg2_ocds_procurement.c
 *
 * Two national procurement publishers that speak OCDS 1.1 natively.
 *
 *  za-etenders-ocds — South Africa National Treasury eTenders
 *    GET https://ocds-api.etenders.gov.za/api/OCDSReleases?PageNumber=1&PageSize=50
 *        &dateFrom=YYYY-MM-DD&dateTo=YYYY-MM-DD      (both are REQUIRED)
 *    We ask for a rolling 14-day window ending today. Response is a standard
 *    OCDS release package {"releases":[...]}. Emits ocid, tender.title,
 *    tender.description, tender.status, tender.province, tender.deliveryLocation,
 *    tender.value.{amount,currency}, buyer name and the count of attached
 *    tender.documents[]. deliveryLocation is free text and province is a name —
 *    neither is coordinates, so has_geo stays false (R2).
 *    Licence: declared in the payload — Open Data Commons PDDL 1.0 (National
 *    Treasury).
 *
 *  uy-comprasestatales-ocds — Uruguay ARCE (Agencia Reguladora de Compras
 *    Estatales). Two-step, exactly as the publisher exposes it:
 *      1. GET https://www.comprasestatales.gub.uy/ocds/rss/YYYY/MM
 *         → one RSS <item> per release, each with <guid>llamado-NNNNNNN</guid>
 *      2. GET https://www.comprasestatales.gub.uy/ocds/release/<guid>
 *         → the full OCDS release package
 *    (/ocds/release-package/YYYY/MM is 404 and is not used.) Only the most
 *    recent UY_MAX guids are dereferenced per run so a scheduled sweep stays
 *    polite. Emits ocid, release tag, publishedDate, tender title/description
 *    /status/value where present, and parties[] as {name, legalName, roles,
 *    contactPoint.email}.
 *    Licence: declared inside every release (gub.uy ARCE open-data licence).
 *
 * Keyless. Zero releases in the window is a clean empty (return 0).
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

#define UY_MAX 15

/* ------------------------------------------------------ South Africa eTenders */

static int za_run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL), from = now - 14 * 24 * 3600;
  struct tm gt, gf;
  gmtime_r(&now, &gt);
  gmtime_r(&from, &gf);
  char d_to[16], d_from[16];
  strftime(d_to, sizeof d_to, "%Y-%m-%d", &gt);
  strftime(d_from, sizeof d_from, "%Y-%m-%d", &gf);

  char url[320];
  snprintf(url, sizeof url,
           "https://ocds-api.etenders.gov.za/api/OCDSReleases"
           "?PageNumber=1&PageSize=50&dateFrom=%s&dateTo=%s", d_from, d_to);

  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[za-etenders-ocds] fetch/parse failed\n"); return -1; }

  const cJSON *rel = cJSON_GetObjectItem(doc, "releases");
  if (!cJSON_IsArray(rel)) {
    fprintf(stderr, "[za-etenders-ocds] unexpected payload shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rel) {
    if (!cJSON_IsObject(r)) continue;
    const char *ocid = jo_sv(r, "ocid");
    const cJSON *t   = cJSON_GetObjectItem(r, "tender");
    const char *title = t ? jo_sv(t, "title") : NULL;
    const char *desc  = t ? jo_sv(t, "description") : NULL;
    if (!title && !desc && !ocid) continue;

    const char *status = t ? jo_sv(t, "status") : NULL;
    const char *prov   = t ? jo_sv(t, "province") : NULL;
    const char *loc    = t ? jo_sv(t, "deliveryLocation") : NULL;
    const char *date   = jo_sv(r, "date");

    const char *buyer = NULL;
    const cJSON *b = cJSON_GetObjectItem(r, "buyer");
    if (b) buyer = jo_sv(b, "name");

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "za-etenders-ocds");
    cJSON_AddStringToObject(props, "source", "ocds-api.etenders.gov.za");
    if (ocid)   cJSON_AddStringToObject(props, "ocid", ocid);
    if (title)  cJSON_AddStringToObject(props, "tender_title", title);
    if (desc)   cJSON_AddStringToObject(props, "tender_description", desc);
    if (status) cJSON_AddStringToObject(props, "tender_status", status);
    if (prov)   cJSON_AddStringToObject(props, "province", prov);
    if (loc)    cJSON_AddStringToObject(props, "delivery_location", loc);
    if (buyer)  cJSON_AddStringToObject(props, "buyer", buyer);
    if (date)   cJSON_AddStringToObject(props, "release_date", date);
    if (t) {
      const cJSON *val = cJSON_GetObjectItem(t, "value");
      if (val) {
        const cJSON *am = cJSON_GetObjectItem(val, "amount");
        if (cJSON_IsNumber(am))
          cJSON_AddNumberToObject(props, "value_amount", am->valuedouble);
        if (jo_sv(val, "currency"))
          cJSON_AddStringToObject(props, "value_currency", jo_sv(val, "currency"));
      }
      const cJSON *docs = cJSON_GetObjectItem(t, "documents");
      if (cJSON_IsArray(docs))
        cJSON_AddNumberToObject(props, "document_count", cJSON_GetArraySize(docs));
      if (jo_sv(t, "id")) cJSON_AddStringToObject(props, "tender_id", jo_sv(t, "id"));
    }
    cJSON_AddStringToObject(props, "window_start", d_from);
    cJSON_AddStringToObject(props, "window_end", d_to);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char titlebuf[500];
    snprintf(titlebuf, sizeof titlebuf, "%s", title ? title : (desc ? desc : ocid));

    char summary[500];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             buyer ? buyer : "", (buyer && prov) ? " · " : "", prov ? prov : "",
             ((buyer || prov) && desc) ? " · " : "", desc ? desc : "");

    intel_item it = {0};
    it.remote_key      = ocid ? ocid : titlebuf;
    it.title           = titlebuf;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = url;
    it.lang            = "en";
    it.published_at    = date;
    it.record_type     = "procurement-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"ocds\",\"south-africa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[za-etenders-ocds] emitted %d (%s..%s)\n", n, d_from, d_to);
  return 0;
}

/* --------------------------------------------------------- Uruguay — ARCE */

/* Emit every release inside one OCDS release package. */
static int uy_emit_package(const cJSON *pkg, const char *guid,
                           const char *url, intel_sink *sink) {
  const char *published = jo_sv(pkg, "publishedDate");
  const cJSON *rel = cJSON_GetObjectItem(pkg, "releases");
  if (!cJSON_IsArray(rel)) return 0;

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rel) {
    if (!cJSON_IsObject(r)) continue;
    const char *ocid = jo_sv(r, "ocid");
    const cJSON *t = cJSON_GetObjectItem(r, "tender");
    const char *title = t ? jo_sv(t, "title") : NULL;
    const char *desc  = t ? jo_sv(t, "description") : NULL;

    /* Fall back to the first named party when the tender has no title. */
    const char *party0 = NULL;
    const cJSON *parties = cJSON_GetObjectItem(r, "parties");
    if (cJSON_IsArray(parties)) {
      const cJSON *p;
      cJSON_ArrayForEach(p, parties) {
        party0 = jo_sv(p, "name");
        if (party0) break;
      }
    }
    if (!title && !desc && !party0) continue;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "uy-comprasestatales-ocds");
    cJSON_AddStringToObject(props, "source", "comprasestatales.gub.uy");
    cJSON_AddStringToObject(props, "guid", guid);
    if (ocid)      cJSON_AddStringToObject(props, "ocid", ocid);
    if (published) cJSON_AddStringToObject(props, "published_date", published);
    if (jo_sv(pkg, "license"))
      cJSON_AddStringToObject(props, "declared_license", jo_sv(pkg, "license"));
    if (title) cJSON_AddStringToObject(props, "tender_title", title);
    if (desc)  cJSON_AddStringToObject(props, "tender_description", desc);
    if (t) {
      if (jo_sv(t, "status")) cJSON_AddStringToObject(props, "tender_status", jo_sv(t, "status"));
      const cJSON *val = cJSON_GetObjectItem(t, "value");
      if (val) {
        const cJSON *am = cJSON_GetObjectItem(val, "amount");
        if (cJSON_IsNumber(am)) cJSON_AddNumberToObject(props, "value_amount", am->valuedouble);
        if (jo_sv(val, "currency"))
          cJSON_AddStringToObject(props, "value_currency", jo_sv(val, "currency"));
      }
    }
    const cJSON *tags = cJSON_GetObjectItem(r, "tag");
    if (cJSON_IsArray(tags)) {
      cJSON *a = cJSON_CreateArray();
      const cJSON *v;
      cJSON_ArrayForEach(v, tags)
        if (cJSON_IsString(v) && v->valuestring)
          cJSON_AddItemToArray(a, cJSON_CreateString(v->valuestring));
      cJSON_AddItemToObject(props, "tags", a);
    }
    if (cJSON_IsArray(parties)) {
      cJSON *a = cJSON_CreateArray();
      const cJSON *p;
      cJSON_ArrayForEach(p, parties) {
        const char *nm = jo_sv(p, "name");
        if (!nm) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", nm);
        const cJSON *ident = cJSON_GetObjectItem(p, "identifier");
        if (ident && jo_sv(ident, "legalName"))
          cJSON_AddStringToObject(o, "legal_name", jo_sv(ident, "legalName"));
        const cJSON *cp = cJSON_GetObjectItem(p, "contactPoint");
        if (cp && jo_sv(cp, "email"))
          cJSON_AddStringToObject(o, "email", jo_sv(cp, "email"));
        const cJSON *roles = cJSON_GetObjectItem(p, "roles");
        if (cJSON_IsArray(roles)) {
          cJSON *ra = cJSON_CreateArray();
          const cJSON *v;
          cJSON_ArrayForEach(v, roles)
            if (cJSON_IsString(v) && v->valuestring)
              cJSON_AddItemToArray(ra, cJSON_CreateString(v->valuestring));
          cJSON_AddItemToObject(o, "roles", ra);
        }
        cJSON_AddItemToArray(a, o);
      }
      cJSON_AddItemToObject(props, "parties", a);
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char titlebuf[500];
    snprintf(titlebuf, sizeof titlebuf, "%s",
             title ? title : (desc ? desc : party0));

    intel_item it = {0};
    it.remote_key      = ocid ? ocid : guid;
    it.title           = titlebuf;
    it.summary         = party0;
    it.body            = pj;
    it.link            = url;
    it.lang            = "es";
    it.published_at    = published;
    it.record_type     = "procurement-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"ocds\",\"uruguay\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int uy_run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  struct tm g;
  gmtime_r(&now, &g);
  char idx[128];
  snprintf(idx, sizeof idx,
           "https://www.comprasestatales.gub.uy/ocds/rss/%04d/%02d",
           g.tm_year + 1900, g.tm_mon + 1);

  const char *hdrs[] = { "Accept: application/rss+xml, application/xml", NULL };
  char *rss = jo_get(ctx, idx, hdrs, "uy-comprasestatales-ocds");
  if (!rss) { fprintf(stderr, "[uy-comprasestatales-ocds] index fetch failed\n"); return -1; }

  /* Collect the most recent guids from the RSS index. */
  char *guids[UY_MAX];
  int ng = 0;
  const char *cur = rss;
  while (ng < UY_MAX) {
    const char *hit = jo_next_entry(&cur);
    if (!hit) break;
    const char *close = strstr(hit, "</item>");
    if (!close) break;
    const char *c2 = hit;
    char *g_ = jo_tag_inner(&c2, "guid");
    if (g_ && c2 <= close + 7 && strncmp(g_, "llamado-", 8) == 0) guids[ng++] = g_;
    else free(g_);
    cur = close + 7;
  }
  free(rss);

  int n = 0;
  for (int i = 0; i < ng; i++) {
    char url[256];
    snprintf(url, sizeof url,
             "https://www.comprasestatales.gub.uy/ocds/release/%s", guids[i]);
    const char *jh[] = { "Accept: application/json", NULL };
    char *body = jo_get(ctx, url, jh, "uy-comprasestatales-ocds");
    if (body) {
      cJSON *pkg = cJSON_Parse(body);
      free(body);
      if (pkg) {
        n += uy_emit_package(pkg, guids[i], url, sink);
        cJSON_Delete(pkg);
      }
    }
    free(guids[i]);
  }

  fprintf(stderr, "[uy-comprasestatales-ocds] %d guids, emitted %d\n", ng, n);
  return 0;
}

static const source_def reg2_za_etenders_def = {
  .id = "za-etenders-ocds", .collector = "government",
  .name = "South Africa National Treasury eTenders (OCDS)",
  .update_interval_sec = 21600, .run = za_run,
  .category = "government", .type = "api",
  .url = "https://ocds-api.etenders.gov.za/api/OCDSReleases",
  .description = "Every South African government tender as an OCDS release — "
                 "title, buying department, province, delivery address, value "
                 "and attached documents. Keyless.",
  .license = "Open Data Commons PDDL 1.0 (National Treasury), as declared in "
             "the payload.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_za_etenders_def)

static const source_def reg2_uy_ocds_def = {
  .id = "uy-comprasestatales-ocds", .collector = "government",
  .name = "Uruguay ARCE public procurement (OCDS)",
  .update_interval_sec = 10800, .run = uy_run,
  .category = "government", .type = "api",
  .url = "https://www.comprasestatales.gub.uy/ocds/",
  .description = "Uruguay's national procurement agency publishes full OCDS 1.1 "
                 "releases: tenders, parties, awards and contracts for every "
                 "state purchase. Keyless.",
  .license = "Declared inside every release: gub.uy agencia-reguladora-compras-"
             "estatales open-data licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_uy_ocds_def)
