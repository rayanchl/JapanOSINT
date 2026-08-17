/* lib/pager.c — see pager.h. Moved verbatim out of lib/jsonlist.c so hpengine
 * can share it; the behaviour is the one jsonlist already shipped. */
#include "pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Dotted lookup that tolerates a missing level, for probing candidate keys. */
static const cJSON *dotted(const cJSON *doc, const char *path) {
  char buf[128];
  snprintf(buf, sizeof buf, "%s", path);
  const cJSON *cur = doc;
  char *save = NULL;
  for (char *tok = strtok_r(buf, ".", &save); tok && cur;
       tok = strtok_r(NULL, ".", &save))
    cur = cJSON_GetObjectItemCaseSensitive(cur, tok);
  return cur;
}

char *pager_next_link(const cJSON *doc) {
  /* Ordered most- to least-specific: `links.next` is the JSON:API/CKAN
   * spelling, `@odata.nextLink` is OData, the rest are common house styles. */
  static const char *const KEYS[] = {
    "links.next", "next", "next_url", "nextUrl", "nextPageUrl",
    "meta.next", "paging.next", "@odata.nextLink", "next_page", NULL };
  if (!doc) return NULL;
  for (int i = 0; KEYS[i]; i++) {
    const cJSON *v = dotted(doc, KEYS[i]);
    if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0] &&
        !strncmp(v->valuestring, "http", 4))
      return strdup(v->valuestring);
    /* CKAN and some JSON:API servers nest it as {next: {href: "..."}}. */
    if (v && cJSON_IsObject(v)) {
      const cJSON *h = cJSON_GetObjectItemCaseSensitive(v, "href");
      if (h && cJSON_IsString(h) && h->valuestring &&
          !strncmp(h->valuestring, "http", 4))
        return strdup(h->valuestring);
    }
  }
  return NULL;
}

long pager_declared_total(const cJSON *doc) {
  static const char *const KEYS[] = {
    "total_count", "totalCount", "total", "count", "meta.count",
    "numberMatched", "totalResults", "result.count", "meta.total",
    "totalElements", "recordsTotal", NULL };
  if (!doc) return -1;
  for (int i = 0; KEYS[i]; i++) {
    const cJSON *v = dotted(doc, KEYS[i]);
    if (v && cJSON_IsNumber(v) && v->valuedouble >= 0)
      return (long)v->valuedouble;
  }
  return -1;
}

long pager_query_int(const char *url, const char *name) {
  if (!url || !name) return -1;
  const char *q = strchr(url, '?');
  if (!q) return -1;
  size_t nlen = strlen(name);
  for (const char *p = q + 1; p && *p; ) {
    if (!strncmp(p, name, nlen) && p[nlen] == '=') {
      char *end = NULL;
      long v = strtol(p + nlen + 1, &end, 10);
      return (end && end != p + nlen + 1) ? v : -1;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
  return -1;
}

char *pager_query_set(const char *url, const char *name, long value) {
  if (!url || !name) return NULL;
  size_t cap = strlen(url) + strlen(name) + 48;
  char *out = malloc(cap);
  if (!out) return NULL;
  const char *q = strchr(url, '?');
  size_t nlen = strlen(name);
  const char *hit = NULL;
  if (q) {
    for (const char *p = q + 1; p && *p; ) {
      if (!strncmp(p, name, nlen) && p[nlen] == '=') { hit = p; break; }
      p = strchr(p, '&');
      if (p) p++;
    }
  }
  if (!hit) {
    snprintf(out, cap, "%s%c%s=%ld", url, q ? '&' : '?', name, value);
    return out;
  }
  const char *tail = strchr(hit, '&');
  size_t head = (size_t)(hit - url);
  memcpy(out, url, head);
  int w = snprintf(out + head, cap - head, "%s=%ld", name, value);
  if (tail) snprintf(out + head + w, cap - head - w, "%s", tail);
  return out;
}

/* A page-size parameter the URL already declares, paired with the cursor
 * parameter that upstream family uses to advance. The pairing is what makes
 * the arithmetic safe: we only ever move a cursor whose page-size sibling is
 * present, so a URL with no declared page size is never paginated by guess. */
struct pager_pair { const char *size_param, *cursor_param; int page_numbered; };
static const struct pager_pair PAGERS[] = {
  { "per_page",  "page",        1 },   /* CKAN/dane.gov.pl, GitHub, uData     */
  { "page_size", "page",        1 },   /* DRF                                 */
  { "pageSize",  "page",        1 },   /* ArcGIS Hub, many .NET APIs          */
  /* Deliberately NOT here: NADA/IHSN's `ps` page-size parameter. Its cursor is
   * probably `page`, but "probably" is the wrong standard for a table whose
   * whole job is to advance only on evidence — a host that declares `ps` and
   * ignores `page` would hand back page 1 for every step of the walk. Rows on
   * that platform can declare page_param themselves once somebody has watched
   * one paginate. */
  { "rows",      "start",       0 },   /* Solr / CKAN package_search          */
  { "limit",     "offset",      0 },   /* Socrata, ODS, most REST             */
  { "$top",      "$skip",       0 },   /* OData                               */
  { "maxRecords","offset",      0 },   /* Airtable-style                       */
  { NULL, NULL, 0 }
};

char *pager_advance(const char *url, int got) {
  if (!url || got <= 0) return NULL;
  for (int i = 0; PAGERS[i].size_param; i++) {
    long size = pager_query_int(url, PAGERS[i].size_param);
    /* Short page = the upstream is finished. Following it would be us
     * inventing a page that was never offered. */
    if (size <= 0 || (long)got < size) continue;
    long cur = pager_query_int(url, PAGERS[i].cursor_param);
    long nextval = PAGERS[i].page_numbered ? (cur > 0 ? cur + 1 : 2)
                                           : (cur >= 0 ? cur + size : size);
    return pager_query_set(url, PAGERS[i].cursor_param, nextval);
  }
  return NULL;
}
