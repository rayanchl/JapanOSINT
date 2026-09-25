/* _ncbi_esummary.inc — NCBI E-utilities rows: the esearch list, then the
 * esummary record behind every id in it. Include AFTER _verified_macros.inc.
 *
 * WHY. esearch answers {esearchresult:{count:"1162247", idlist:["5917840",…]}}
 * — bare uid STRINGS. The generated VJSON rows pointed at
 * "esearchresult.idlist" fetched a live page of 100 ids on every run and emitted
 * nothing (13 rows, 0 records each on 2026-09-15): a list of strings is not a
 * list of records. The record behind each uid is one hop away, at
 * esummary.fcgi?db=<db>&retmode=json&id=<uid,uid,…>, which answers
 * {result:{uids:[…], "<uid>":{uid, title, …}}} — measured for books, medgen,
 * pmc, clinvar, dbvar, pubmed, bioproject, biosample, gds, nuccore,
 * pcsubstance, sra and structure on 2026-09-15.
 *
 * HOW. One esearch page (the row's own retmax, advanced by retstart while the
 * upstream's own count says more exists) and one esummary call for that page's
 * ids. Each summary object is emitted whole. A summary with no `title` gets one
 * copied from its db's own label field (bioproject `project_title`, structure
 * `pdbdescr`, …) — a value already on the record, relabelled so the emitter can
 * name it. The walk stops at JO_PAGE_MAX pages (default 20, the same ceiling
 * lib/pagewalk.c uses) and discloses the remainder against the upstream's
 * count; a failed page is disclosed too. Requests are spaced 350 ms apart
 * because NCBI allows 3 requests a second without an API key. */
#ifndef JO_NCBI_ESUMMARY_INC
#define JO_NCBI_ESUMMARY_INC
#include "_verified_macros.inc"
#include "lib/jocore.h"
#include <time.h>

static inline void ncbi_pause(void) {
  struct timespec ts = { 0, 350000000L };
  nanosleep(&ts, NULL);
}

static inline void ncbi_label(cJSON *rec) {
  static const char *const LABELS[] = { "project_title", "pdbdescr", "caption",
                                        "scientificname", "accession", NULL };
  if (cJSON_GetObjectItemCaseSensitive(rec, "title")) return;
  for (int i = 0; LABELS[i]; i++) {
    const char *v =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(rec, LABELS[i]));
    if (v && *v) { cJSON_AddStringToObject(rec, "title", v); return; }
  }
}

static inline int ncbi_esummary_walk(const source_ctx *c, intel_sink *s,
                                     const char *id, const char *esearch,
                                     const char *cat, const char *lang,
                                     const char *tags) {
  char db[32] = "";
  const char *q = strstr(esearch, "?db=");
  if (!q) q = strstr(esearch, "&db=");
  if (q) {
    size_t n = strcspn(q + 4, "&");
    if (n >= sizeof db) n = sizeof db - 1;
    memcpy(db, q + 4, n);
    db[n] = '\0';
  }
  if (!*db) { fprintf(stderr, "[%s] no db= in %s\n", id, esearch); return -1; }
  long retmax = jsonlist_query_int(esearch, "retmax");
  if (retmax <= 0) retmax = 20;                       /* esearch's own default */
  int page_max = 20;   /* exhaustive-ok: JO_PAGE_MAX runaway ceiling (pagewalk's default), remainder disclosed as a truncation notice */
  const char *pm = getenv("JO_PAGE_MAX");
  if (pm && atoi(pm) > 0) page_max = atoi(pm);

  size_t cap = strlen(esearch) + 40;
  char *url = malloc(cap);
  if (!url) return -1;
  long count = -1, off = 0;
  int pages = 0, emitted = 0, done = 0, failed = 0;
  while (!done && pages < page_max) {   /* exhaustive-ok: JO_PAGE_MAX ceiling, remainder disclosed below */
    snprintf(url, cap, "%s&retstart=%ld", esearch, off);
    if (pages) ncbi_pause();
    cJSON *sr = feed_get_json(c->http, url, 25000);
    cJSON *er = sr ? cJSON_GetObjectItemCaseSensitive(sr, "esearchresult") : NULL;
    cJSON *idl = er ? cJSON_GetObjectItemCaseSensitive(er, "idlist") : NULL;
    if (!cJSON_IsArray(idl)) {
      cJSON_Delete(sr);
      if (pages == 0) { free(url); fprintf(stderr, "[%s] esearch failed\n", id); return -1; }
      failed = 1;
      break;
    }
    const char *cs = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(er, "count"));
    if (cs) count = atol(cs);
    int nids = cJSON_GetArraySize(idl);
    pages++;
    if (nids == 0) { cJSON_Delete(sr); done = 1; break; }

    size_t scap = (size_t)nids * 24 + 128;
    char *su = malloc(scap);
    if (!su) { cJSON_Delete(sr); failed = 1; break; }
    int hw = snprintf(su, scap,
        "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esummary.fcgi?db=%s&retmode=json&id=", db);
    if (hw < 0 || (size_t)hw >= scap) { free(su); cJSON_Delete(sr); failed = 1; break; }
    size_t w = (size_t)hw;
    int first = 1;
    cJSON *u;
    cJSON_ArrayForEach(u, idl) {
      const char *v = cJSON_GetStringValue(u);
      if (!v) continue;
      int k = snprintf(su + w, scap - w, "%s%s", first ? "" : ",", v);
      if (k < 0 || (size_t)k >= scap - w) break;   /* cannot happen: scap is 24 bytes an id */
      w += (size_t)k;
      first = 0;
    }
    cJSON_Delete(sr);
    ncbi_pause();
    cJSON *sm = feed_get_json(c->http, su, 25000);
    free(su);
    cJSON *res = sm ? cJSON_GetObjectItemCaseSensitive(sm, "result") : NULL;
    cJSON *uids = res ? cJSON_GetObjectItemCaseSensitive(res, "uids") : NULL;
    if (!cJSON_IsArray(uids)) {
      cJSON_Delete(sm);
      fprintf(stderr, "[%s] esummary failed for retstart=%ld\n", id, off);
      failed = 1;
      break;
    }
    cJSON *wrap = cJSON_CreateObject();
    cJSON *arr = wrap ? cJSON_AddArrayToObject(wrap, "records") : NULL;
    if (arr) {
      cJSON_ArrayForEach(u, uids) {
        const char *v = cJSON_GetStringValue(u);
        cJSON *rec = v ? cJSON_GetObjectItemCaseSensitive(res, v) : NULL;
        if (!cJSON_IsObject(rec)) continue;
        cJSON *d = cJSON_Duplicate(rec, 1);
        if (!d) continue;
        ncbi_label(d);
        cJSON_AddItemToArray(arr, d);
      }
      int n = jsonlist_emit_ex(s, id, wrap, "records", cat, lang, tags, NULL);
      if (n > 0) emitted += n;
    }
    cJSON_Delete(wrap);
    cJSON_Delete(sm);
    off += nids;
    if (nids < retmax || (count >= 0 && off >= count)) done = 1;
  }
  free(url);
  if (!done) {
    char why[160];
    snprintf(why, sizeof why, failed
             ? "an E-utilities page failed after %d page(s); the walk stopped there"
             : "the JO_PAGE_MAX ceiling stopped the walk after %d page(s)", pages);
    jo_truncation_notice_ex(s, id, NULL, emitted, count, why,
                            failed ? "re-run; the failed url is in the run log"
                                   : "raise $JO_PAGE_MAX — see docs/SOURCE_EXHAUSTIVENESS.md",
                            NULL);
  }
  fprintf(stderr, "[%s] emitted %d esummary record(s) across %d page(s) of %ld\n",
          id, emitted, pages, count);
  return emitted;
}

/* As VJSON, for an esearch.fcgi URL: every listed uid is fetched as a record. */
#define VNCBI(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    return ncbi_esummary_walk(c, s, ID, URL, CAT, LANG, TAGS) < 0 ? -1 : 0; } \
  static const source_def SYM = {                                            \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,             \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = CAT, .type = "api", .url = URL,                               \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

#endif /* JO_NCBI_ESUMMARY_INC */
