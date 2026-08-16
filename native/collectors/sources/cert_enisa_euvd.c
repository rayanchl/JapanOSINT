/* cert_enisa_euvd.c — ENISA EU Vulnerability Database (EUVD), the NIS2-mandated
 * European vulnerability register. Three keyless endpoints, three source_defs,
 * one table-driven run().
 *
 *   enisa-euvd-search     https://euvdservices.enisa.europa.eu/api/search?size=100&page=0
 *   enisa-euvd-latest     https://euvdservices.enisa.europa.eu/api/lastvulnerabilities
 *   enisa-euvd-exploited  https://euvdservices.enisa.europa.eu/api/exploitedvulnerabilities
 *
 * SCOPE: the corpus is 372,036 records (doc.total on /api/search). This
 * collector deliberately does NOT page the whole corpus — it takes the recent
 * window only: one page of /api/search (100 records, page=0), plus the two
 * small newest-first digests. On an OSINT pivot (ctx->entity set) the search
 * endpoint is re-pointed at ?text=<entity> instead, which is the same window
 * narrowed to the analyst's term rather than a deeper crawl.
 *
 * Emits, all read out of the response body: EUVD id (remote_key), description,
 * datePublished/dateUpdated, CVSS baseScore + baseScoreVersion + baseScoreVector
 * (v4 vectors, e.g. CVSS:4.0/AV:N/AC:L/...), EPSS where present, assigner,
 * aliases, and the first upstream reference URL as `link`. No URL is
 * constructed: if the record carries no reference, the row carries no link (R1).
 *
 * Parse notes honoured: /api/search returns {items:[...], total:N}; the other
 * two return a BARE JSON ARRAY at the document root, so both shapes are
 * handled. datePublished is passed through only when it already looks ISO-8601
 * (some EUVD builds emit "Jun 24, 2025, 5:15:22 PM"); the raw value is always
 * kept in properties.date_published_raw so nothing fetched is discarded.
 *
 * R2: no geometry — a vulnerability record has no location.
 * R3: a quiet hour on /lastvulnerabilities is not an error; a successful fetch
 * returns 0 at zero rows, and -1 only when the fetch/parse itself failed.
 *
 * Keyless. Licence: ENISA publishes EUVD as a public service; no key required. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EUVD_SEARCH    "https://euvdservices.enisa.europa.eu/api/search?size=100&page=0"
#define EUVD_SEARCH_Q  "https://euvdservices.enisa.europa.eu/api/search?size=50&page=0&text="
#define EUVD_LATEST    "https://euvdservices.enisa.europa.eu/api/lastvulnerabilities"
#define EUVD_EXPLOITED "https://euvdservices.enisa.europa.eu/api/exploitedvulnerabilities"

/* RFC-3986 %-encode for the ctx->entity pivot. malloc'd; caller frees. */
static char *urlenc(const char *s) {
  static const char hx[] = "0123456789ABCDEF";
  if (!s) s = "";
  size_t n = strlen(s);
  char *o = (char *)malloc(n * 3 + 1);
  if (!o) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o[j++] = (char)c;
    else { o[j++] = '%'; o[j++] = hx[c >> 4]; o[j++] = hx[c & 15]; }
  }
  o[j] = 0;
  return o;
}

/* First http(s) URL out of `references`, which EUVD serves either as a
 * newline-separated string or as an array of strings. Never fabricated. */
static void first_ref_url(cJSON *rec, char *out, size_t n) {
  out[0] = 0;
  cJSON *r = cJSON_GetObjectItem(rec, "references");
  const char *blob = NULL;
  if (r && cJSON_IsString(r)) blob = r->valuestring;
  else if (r && cJSON_IsArray(r)) {
    cJSON *e;
    cJSON_ArrayForEach(e, r)
      if (cJSON_IsString(e) && e->valuestring) { blob = e->valuestring; break; }
  }
  if (!blob) return;
  const char *p = strstr(blob, "http");
  if (!p) return;
  size_t i = 0;
  while (p[i] && p[i] != '\n' && p[i] != '\r' && p[i] != ' ' &&
         p[i] != '\t' && i + 1 < n) { out[i] = p[i]; i++; }
  out[i] = 0;
}

/* Copy an upstream value into properties only when it is really there. */
static void put_str(cJSON *p, const char *k, cJSON *rec, const char *rk) {
  const char *v = jo_sv(rec, rk);
  if (v) cJSON_AddStringToObject(p, k, v);
}
static void put_num(cJSON *p, const char *k, cJSON *rec, const char *rk) {
  cJSON *v = cJSON_GetObjectItem(rec, rk);
  if (v && cJSON_IsNumber(v)) cJSON_AddNumberToObject(p, k, v->valuedouble);
}

static int euvd_emit(intel_sink *sink, cJSON *rec, const char *tags) {
  if (!cJSON_IsObject(rec)) return 0;
  const char *id = jo_sv(rec, "id");
  if (!id) return 0;                 /* no upstream id -> no row (R1) */
  const char *desc = jo_sv(rec, "description");
  const char *pub  = jo_sv(rec, "datePublished");

  char title[600];
  if (desc) snprintf(title, sizeof title, "%s: %s", id, desc);
  else      snprintf(title, sizeof title, "%s", id);
  jo_utf8_trunc(title, 320);

  char link[600];
  first_ref_url(rec, link, sizeof link);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "euvd_id", id);
  put_str(p, "enisa_uuid",         rec, "enisaUuid");
  put_str(p, "assigner",           rec, "assigner");
  put_str(p, "base_score_version", rec, "baseScoreVersion");
  put_str(p, "base_score_vector",  rec, "baseScoreVector");
  put_num(p, "base_score",         rec, "baseScore");
  put_num(p, "epss",               rec, "epss");
  put_str(p, "date_updated",       rec, "dateUpdated");
  if (pub) cJSON_AddStringToObject(p, "date_published_raw", pub);
  cJSON *al = cJSON_GetObjectItem(rec, "aliases");
  if (al && cJSON_IsString(al) && al->valuestring[0])
    cJSON_AddStringToObject(p, "aliases", al->valuestring);
  else if (al && cJSON_IsArray(al))
    cJSON_AddItemToObject(p, "aliases", cJSON_Duplicate(al, 1));
  cJSON_AddStringToObject(p, "source", "enisa_euvd");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  intel_item it = {0};
  it.remote_key      = id;
  it.title           = title;
  it.body            = desc;
  it.summary         = desc;
  it.link            = link[0] ? link : NULL;
  it.lang            = "en";
  it.published_at    = jo_looks_iso(pub) ? pub : NULL;
  it.record_type     = "vulnerability";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = tags;
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Both shapes: {items:[...]} (search) and a bare array (the two digests). */
static int euvd_walk(cJSON *doc, intel_sink *sink, const char *tags) {
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "items");
  int n = 0;
  if (!cJSON_IsArray(arr)) return 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) n += euvd_emit(sink, e, tags);
  return n;
}

static int run(const source_ctx *c, intel_sink *s) {
  if (!c || !c->source_id) return -1;
  const char *url = NULL, *tags = NULL;
  char *built = NULL;

  if (!strcmp(c->source_id, "enisa-euvd-search")) {
    tags = "[\"vulnerability\",\"cyber\",\"euvd\",\"enisa\"]";
    if (c->entity && *c->entity) {
      char *q = urlenc(c->entity);
      if (q) {
        size_t n = strlen(EUVD_SEARCH_Q) + strlen(q) + 1;
        built = (char *)malloc(n);
        if (built) snprintf(built, n, "%s%s", EUVD_SEARCH_Q, q);
        free(q);
      }
    }
    url = built ? built : EUVD_SEARCH;
  } else if (!strcmp(c->source_id, "enisa-euvd-latest")) {
    url = EUVD_LATEST;
    tags = "[\"vulnerability\",\"cyber\",\"euvd\",\"enisa\",\"latest\"]";
  } else if (!strcmp(c->source_id, "enisa-euvd-exploited")) {
    url = EUVD_EXPLOITED;
    tags = "[\"vulnerability\",\"cyber\",\"euvd\",\"enisa\",\"exploited\"]";
  } else {
    fprintf(stderr, "[cert_enisa_euvd] unknown source id %s\n", c->source_id);
    return -1;
  }

  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, url, hdrs, 25000);
  free(built);
  if (!doc) {
    fprintf(stderr, "[%s] fetch/parse failed\n", c->source_id);
    return -1;                                       /* the fetch failed (R3) */
  }
  int n = euvd_walk(doc, s, tags);
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", c->source_id, n);
  return 0;                                /* fetched fine; 0 rows is OK (R3) */
}

static const source_def cert_euvd_search_def = {
  .id = "enisa-euvd-search", .collector = "cyber",
  .name = "ENISA EU Vulnerability Database (EUVD) - search",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://euvdservices.enisa.europa.eu/api/search",
  .description = "ENISA's NIS2-mandated EU Vulnerability Database: EUVD ids, "
                 "CVSS v4 base scores and vectors, aliases and cross-references. "
                 "Recent window only (one page); pivots on an entity via ?text=.",
  .license = "ENISA publishes EUVD as a public service; no key required.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_euvd_search_def)

static const source_def cert_euvd_latest_def = {
  .id = "enisa-euvd-latest", .collector = "cyber",
  .name = "ENISA EUVD - latest vulnerabilities",
  .update_interval_sec = 1800, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://euvdservices.enisa.europa.eu/api/lastvulnerabilities",
  .description = "Newest-first slice of the EU Vulnerability Database: a cheap "
                 "polling endpoint for freshly published EUVD entries with "
                 "CVSS v4 scoring.",
  .license = "ENISA publishes EUVD as a public service; no key required.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_euvd_latest_def)

static const source_def cert_euvd_exploited_def = {
  .id = "enisa-euvd-exploited", .collector = "cyber",
  .name = "ENISA EUVD - exploited vulnerabilities",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://euvdservices.enisa.europa.eu/api/exploitedvulnerabilities",
  .description = "ENISA's EU-side known-exploited list, the European "
                 "counterpart to CISA KEV, flagging vulnerabilities with "
                 "confirmed exploitation.",
  .license = "ENISA publishes EUVD as a public service; no key required.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_euvd_exploited_def)
