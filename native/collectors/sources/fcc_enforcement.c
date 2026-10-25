/* collectors/sources/fcc_enforcement.c — US_FCC_ENFORCEMENT
 *
 * FCC Commission documents (EDOCS) whose TITLE BEGINS WITH the entity: the
 * notices of apparent liability, forfeiture orders, consent decrees, citations
 * and notices of violation an investigator is after, beside every other
 * release titled after the same party. Each record carries the issuing bureau
 * and the document type the FCC filed it under, so enforcement actions are
 * legible without anything being dropped.
 *
 * A PREFIX match, not a substring one — measured, and the limit of this row:
 * all 401 "Verizon" hits start with "Verizon"; "erizon" answers ["no
 * results"]; "Proposes" finds 2 titles while "FCC Proposes" finds 592. So a
 * release titled "Cellco Partnership d/b/a Verizon Wireless" or "FCC Proposes
 * Fine Against Verizon" is NOT returned for "Verizon", and the Telnyx NAL and
 * the Lingo Telecom consent decree were absent for those names. What comes
 * back is about the entity; what is missing is said here and in the row's
 * description, not implied away.
 *
 * WHY THIS SURFACE (measured 2026-10-25 with the engine's TLS stack):
 *
 *   www.fcc.gov/api/content.json?type=edoc&search_string=X  — the FCC Content
 *     API. `search_string` is documented as a title search and IS honoured
 *     server-side (as a prefix, above): "Verizon" 401 documents, an
 *     impossible term the bare array ["no results"]. Its sibling `query=` (what the removed row's evidence
 *     measured) is ignored. JSON, `limit` capped at 200 by the server, `page`
 *     0-based, and the first array element is the envelope
 *     {count, pages, itemsPerPage, currentPage}.
 *     It has NO bureau or document-type filter: bureau=, field_bureau_office_
 *     category=, tid=, doctype=, edoc_doctype= all return the same 401. Nor
 *     would a bureau filter be the right scope: the Media Bureau issues
 *     forfeiture orders too (Castelli Media, Roseland Broadcasting, Elko
 *     Television District …), so "Enforcement Bureau only" would miss
 *     enforcement actions. The type each record states is the honest scope.
 *
 *   Rejected:
 *   - EDOCS search (www.fcc.gov/edocs/search-results?t=quick&search=X&
 *     bureaus=EB): the filter is honoured (Verizon+EB 644) but the surface is
 *     not usable by a collector — page 2 answered 503 after 60 s, a zero-hit
 *     search answers 503 (or hung 1,014 s), docTypes= answered 503 / hung
 *     869 s. A 503 for "nothing found" would file every clean entity as an
 *     error.
 *   - EDOCS public API (api2.fcc.gov/api/exp/v1.0.0/edocspublic): documents/
 *     {recId} is a clean JSON record, and documents?fileNumber= / docket= /
 *     daNumber= / reportNumber= are identifier lookups with bureaus= and
 *     docTypes= honoured as secondary filters — but there is no text or party
 *     search (search=, q=, title=, titleText= … all 400), and docTypes= alone
 *     times out at the gateway (60 s) for any common type. Its RSS
 *     (rss/bureaus/EB, rss/docTypes/NAL) carries only the latest release day.
 *   - www.fcc.gov/enforcement/orders?search_api_fulltext= ignores the term.
 *   - opendata.fcc.gov (Socrata): no enforcement or forfeiture dataset; the
 *     only enforcement-tagged one is the Pirate Radio Broadcasting Database.
 *   - ECFS (publicapi.fcc.gov/ecfs/filings): `q` is honoured but
 *     proceedings.bureau_code=EB is ignored, and filings are comments in
 *     proceedings, not enforcement actions.
 *   - Federal Register API (agency federal-communications-commission): the
 *     term filter is honoured (forfeiture 752, impossible 0) but NALs and
 *     forfeiture orders are not published there — hits are rulemakings,
 *     inflation adjustments and hearing designation orders.
 *   - GovInfo: no FCC Record collection (collections list checked).
 *
 * MULTI-VALUED FIELDS ARRIVE AS SEPARATE ROWS. With fields=all the API joins
 * each multi-valued field into its own row: "Verizon" is 530 rows for 401
 * documents, and duplicate rows of one nid differ only in which docket number
 * they carry (nid 94998: 03-157 on one row, 03-189 on the other). Keeping the
 * first row per nid would drop dockets; emitting each row would collapse at
 * the sink on the shared nid. So every row of a nid is MERGED — the union of
 * each fields.* array and of the taxonomy array — and one record is emitted
 * per document, after the whole walk, because a document's rows can straddle
 * a page boundary.
 *
 * Host agent: www.fcc.gov answers 403 to the engine's default agent and 200
 * to HA_CONTACT. Its core/host_agents.def entry is on fix/sec-agent (1642c4e3)
 * and not yet on main; until it lands this source fails with an explicit
 * error on every run (measured: rc=-1, "page 0: HTTP 403", status=error —
 * never an empty success). It does not set a User-Agent of its own: the table
 * is the one place a host's agent is decided. */
#include "source.h"
#include "core/httpclient.h"
#include "lib/jocore.h"
#include "lib/pagewalk.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FCC_ID       "US_FCC_ENFORCEMENT"
#define FCC_API      "https://www.fcc.gov/api/content.json"
#define FCC_LIMIT    200            /* the server's own cap on `limit` */
#define FCC_TIMEOUT  45000          /* fields=all pages measured 3.9 s cold, one 40 s+ */

static cJSON *fcc_fetch_page(const source_ctx *ctx, const char *enc, int page,
                             long *status) {
  char url[1024];
  snprintf(url, sizeof url,
           FCC_API "?type=edoc&search_string=%s&limit=%d&page=%d&fields=all",
           enc, FCC_LIMIT, page);
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, FCC_TIMEOUT, 2, &hr);
  *status = hr.status;
  cJSON *doc = (rc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (doc && !cJSON_IsArray(doc)) { cJSON_Delete(doc); doc = NULL; }
  return doc;
}

/* Append to `dst` each element of `src` it does not already hold. */
static void fcc_union(cJSON *dst, const cJSON *src) {
  const cJSON *e;
  cJSON_ArrayForEach(e, src) {
    int have = 0;
    const cJSON *d;
    cJSON_ArrayForEach(d, dst) if (cJSON_Compare(d, e, 1)) { have = 1; break; }
    if (!have) cJSON_AddItemToArray(dst, cJSON_Duplicate(e, 1));
  }
}

/* Fold row `src` (same nid) into the kept document `dst`. */
static void fcc_merge(cJSON *dst, const cJSON *src) {
  const cJSON *sf = cJSON_GetObjectItemCaseSensitive(src, "fields");
  cJSON *df = cJSON_GetObjectItemCaseSensitive(dst, "fields");
  if (cJSON_IsObject(sf)) {
    if (!cJSON_IsObject(df)) {
      cJSON_DeleteItemFromObjectCaseSensitive(dst, "fields");
      cJSON_AddItemToObject(dst, "fields", cJSON_Duplicate(sf, 1));
    } else {
      const cJSON *f;
      cJSON_ArrayForEach(f, sf) {
        cJSON *have = cJSON_GetObjectItemCaseSensitive(df, f->string);
        if (!have) cJSON_AddItemToObject(df, f->string, cJSON_Duplicate(f, 1));
        else if (cJSON_IsArray(have) && cJSON_IsArray(f)) fcc_union(have, f);
      }
    }
  }
  const cJSON *st = cJSON_GetObjectItemCaseSensitive(src, "taxonomy");
  cJSON *dt = cJSON_GetObjectItemCaseSensitive(dst, "taxonomy");
  if (cJSON_IsArray(st)) {
    if (cJSON_IsArray(dt)) fcc_union(dt, st);
    else cJSON_AddItemToObject(dst, "taxonomy", cJSON_Duplicate(st, 1));
  }
  /* Any top-level member the kept row lacks (never observed) is carried over
   * rather than lost. */
  const cJSON *m;
  cJSON_ArrayForEach(m, src)
    if (m->string && !cJSON_GetObjectItemCaseSensitive(dst, m->string))
      cJSON_AddItemToObject(dst, m->string, cJSON_Duplicate(m, 1));
}

/* "name" members of fields.<key>[], joined with ", " into out, and copied as a
 * plain string array into props[as]. */
static void fcc_names(const cJSON *fields, const char *key, const char *member,
                      cJSON *props, const char *as, char *out, size_t cap) {
  out[0] = 0;
  cJSON *arr = cJSON_AddArrayToObject(props, as);
  const cJSON *a = cJSON_GetObjectItemCaseSensitive(fields, key), *e;
  size_t o = 0;
  cJSON_ArrayForEach(e, a) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(e, member);
    if (!cJSON_IsString(n) || !n->valuestring[0]) continue;
    cJSON_AddItemToArray(arr, cJSON_CreateString(n->valuestring));
    if (o + 3 < cap)
      o += (size_t)snprintf(out + o, cap - o, "%s%s", o ? ", " : "", n->valuestring);
  }
}

static const char *fcc_first_value(const cJSON *fields, const char *key) {
  const cJSON *a = cJSON_GetObjectItemCaseSensitive(fields, key);
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(a, 0), "value");  /* exhaustive-ok: display text only; every value of every field is kept in properties (props = the whole merged doc) */
  return cJSON_IsString(v) && v->valuestring[0] ? v->valuestring : NULL;
}

static int fcc_emit_doc(intel_sink *sink, const char *entity, cJSON *doc) {
  const cJSON *nid = cJSON_GetObjectItemCaseSensitive(doc, "nid");
  const cJSON *ttl = cJSON_GetObjectItemCaseSensitive(doc, "title");
  const cJSON *url = cJSON_GetObjectItemCaseSensitive(doc, "webUrl");
  const cJSON *fields = cJSON_GetObjectItemCaseSensitive(doc, "fields");
  if (!cJSON_IsString(nid) || !nid->valuestring[0]) return 0;

  cJSON *props = cJSON_Duplicate(doc, 1);
  cJSON_AddStringToObject(props, "query", entity);
  char bureaus[256], types[256];
  fcc_names(fields, "field_bureau_office_category", "name", props, "bureaus",
            bureaus, sizeof bureaus);
  fcc_names(fields, "field_edoc_doctype", "name", props, "document_types",
            types, sizeof types);
  const char *edoc_id   = fcc_first_value(fields, "field_edoc_id");
  const char *full      = fcc_first_value(fields, "field_edoc_full_title");
  const char *desc      = fcc_first_value(fields, "field_edoc_short_desc");
  const char *released  = fcc_first_value(fields, "field_edoc_release_dt");
  const cJSON *created  = cJSON_GetObjectItemCaseSensitive(doc, "created");
  int eb = strstr(bureaus, "Enforcement") != NULL;
  cJSON_AddBoolToObject(props, "issued_by_enforcement_bureau", eb);

  char summary[600];
  snprintf(summary, sizeof summary, "%s%s%s%s%s",
           types[0] ? types : "FCC document",
           bureaus[0] ? " - " : "", bureaus,
           edoc_id ? " - " : "", edoc_id ? edoc_id : "");
  char date[16] = "";
  if (released && strlen(released) >= 10) { memcpy(date, released, 10); date[10] = 0; }
  else if (cJSON_IsString(created)) snprintf(date, sizeof date, "%.10s", created->valuestring);

  char body[2048];
  snprintf(body, sizeof body, "%s%s%s", full ? full : "", (full && desc) ? "\n\n" : "",
           desc ? desc : "");

  char rk[64];
  snprintf(rk, sizeof rk, "fcc-edoc:%s", nid->valuestring);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = cJSON_IsString(ttl) && ttl->valuestring[0] ? ttl->valuestring
                                                                   : (full ? full : rk);
  it.summary         = summary;
  it.body            = body[0] ? body : NULL;
  it.link            = cJSON_IsString(url) ? url->valuestring : NULL;
  it.published_at    = date[0] ? date : NULL;
  it.lang            = "en";
  it.record_type     = "us-fcc-edoc";
  it.properties_json = pj;
  it.tags_json       = eb ? "[\"us\",\"telecom\",\"fcc\",\"edocs\",\"enforcement-bureau\"]"
                          : "[\"us\",\"telecom\",\"fcc\",\"edocs\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return 0;          /* a pivot: nothing to ask about */
  char enc[600];
  jo_urlencode_buf(entity, enc, sizeof enc);

  cJSON *docs = cJSON_CreateArray();          /* one merged object per nid */
  long rows = 0, merged = 0, available = -1, status = 0;
  int pages = 1, page = 0, failed_page = -1;
  int ceiling = jo_page_max(NULL);

  for (page = 0; page < pages && page < ceiling; page++) {
    if (ctx->cancel && *ctx->cancel) break;
    cJSON *arr = fcc_fetch_page(ctx, enc, page, &status);
    if (!arr) {
      fprintf(stderr, "[" FCC_ID "] page %d: HTTP %ld or not a JSON array\n", page, status);
      if (page == 0) { cJSON_Delete(docs); return -1; }  /* dead endpoint: an error */
      failed_page = page;
      break;
    }
    const cJSON *head = cJSON_GetArrayItem(arr, 0);  /* exhaustive-ok: element 0 is the API's count envelope; every element after it is walked below */
    if (cJSON_IsString(head)) {               /* ["no results"]: an honest empty */
      cJSON_Delete(arr);
      break;
    }
    if (page == 0 && cJSON_IsObject(head)) {
      const cJSON *c = cJSON_GetObjectItemCaseSensitive(head, "count");
      const cJSON *p = cJSON_GetObjectItemCaseSensitive(head, "pages");
      if (cJSON_IsString(c)) available = strtol(c->valuestring, NULL, 10);
      else if (cJSON_IsNumber(c)) available = (long)c->valuedouble;
      if (cJSON_IsNumber(p) && p->valueint > 0) pages = p->valueint;
    }
    const cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      const cJSON *nid = cJSON_GetObjectItemCaseSensitive(r, "nid");
      if (!cJSON_IsString(nid)) continue;     /* the envelope */
      rows++;
      cJSON *have = NULL, *d;
      cJSON_ArrayForEach(d, docs) {
        const cJSON *dn = cJSON_GetObjectItemCaseSensitive(d, "nid");
        if (!strcmp(dn->valuestring, nid->valuestring)) { have = d; break; }
      }
      if (have) { fcc_merge(have, r); merged++; }
      else cJSON_AddItemToArray(docs, cJSON_Duplicate(r, 1));
    }
    cJSON_Delete(arr);
  }

  int emitted = 0;
  cJSON *d;
  cJSON_ArrayForEach(d, docs) emitted += fcc_emit_doc(sink, entity, d);
  cJSON_Delete(docs);
  fprintf(stderr, "[" FCC_ID "] %ld rows over %d page(s), %ld folded into their "
          "document, %d documents emitted\n", rows, page, merged, emitted);

  /* `available` counts ROWS (the join duplicates included), so it is compared
   * with rows read, not with documents emitted. */
  if (failed_page >= 0 || (page < pages && available > rows)) {
    char reason[256], endpoint[1024];
    snprintf(endpoint, sizeof endpoint,
             FCC_API "?type=edoc&search_string=%s&limit=%d&fields=all", enc, FCC_LIMIT);
    if (failed_page >= 0)
      snprintf(reason, sizeof reason, "page %d of %d failed (HTTP %ld); the "
               "documents on the pages read were emitted", failed_page + 1, pages, status);
    else
      snprintf(reason, sizeof reason, "the walk stopped at the %d-page ceiling of %d "
               "pages (rows counted with each multi-valued field's join duplicates)",
               ceiling, pages);
    jo_trunc_notice(sink, FCC_ID, endpoint, rows, available, reason,
                    failed_page >= 0 ? "re-run the pivot"
                                     : "raise JO_PAGE_MAX, or pivot on a more specific name");
  }
  return 0;
}

static const source_def fcc_enforcement_def = {
  .id = FCC_ID, .collector = "osint",
  .name = "FCC — Commission documents naming the entity (enforcement included)",
  .name_ja = "米国FCC — 執行措置を含む委員会文書",
  .update_interval_sec = 0, .run = run,
  .category = "telecom", .type = "api",
  .url = "https://www.fcc.gov/edocs",
  .description = "FCC Commission documents (EDOCS) whose title begins with the "
    "entity — notices of apparent liability, forfeiture orders, consent decrees, "
    "citations and notices of violation among them — each with the issuing "
    "bureau, the document type, the DA/FCC number, dockets and release date. "
    "Searched server-side through the FCC Content API, which matches title "
    "PREFIXES only: a release titled 'FCC Fines X' is not found for X. No key",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(fcc_enforcement_def)
