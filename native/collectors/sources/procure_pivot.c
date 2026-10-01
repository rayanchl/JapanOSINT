/* collectors/sources/procure_pivot.c
 * OSINT services — public procurement, pivoted on what you actually want to
 * know rather than on the publishing country. The fleet already ingests OCDS
 * award feeds per jurisdiction (procurement_ocds.c, reg2_eu_procurement.c,
 * reg2_us_federal.c, cl/tw/es/ua …); what it had no way to answer was
 * "what is OPEN that I could bid on" and "what has THIS COMPANY won, and when
 * does it run out".
 *
 *   • SAM_GOV_OPPORTUNITIES  — open US federal solicitations matching a keyword.
 *                              Free api.data.gov key; gated on SAM_GOV_API_KEY.
 *   • TED_EU_TENDERS         — EU Tenders Electronic Daily notices. Keyless.
 *   • AWARD_SUPPLIER_SEARCH  — US federal awards won by a named recipient.
 *                              Keyless.
 *   • CONTRACT_EXPIRY_WATCH  — the same awards, filtered to those whose period of
 *                              performance ENDS inside a forward window. An
 *                              incumbent's end date is when the work is winnable.
 *                              Keyless.
 *
 * Endpoints:
 *   https://api.sam.gov/opportunities/v2/search      (GET, api_key query param)
 *   https://api.ted.europa.eu/v3/notices/search      (POST JSON)
 *   https://api.usaspending.gov/api/v2/search/spending_by_award/  (POST JSON)
 *
 * Licence/terms: all three are open government data published for reuse.
 * SAM.gov's key is free self-service, so free_tier stays 1.
 *
 * R2: none of these upstreams return coordinates — place of performance is a
 * name and a ZIP, not a position — so no row here sets has_geo. Resolving those
 * to a point would be exactly the invented geometry the audit removed. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Emit one row. `data` is TAKEN OVER (printed, then deleted). Returns 1 if
 * emitted. body == properties so the FTS mirror in core/intel.c indexes the
 * fetched fields — these services have no prose body. */
static int pp_emit(intel_sink *sink, const char *service, cJSON *data,
                   const char *record_type, const char *rk, const char *title,
                   const char *summary, const char *link, const char *published,
                   const char *tags) {
  if (!data) return 0;
  cJSON_AddStringToObject(data, "service", service);
  char *pj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);
  if (!pj) return 0;

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.link            = link;
  it.published_at    = published;
  it.lang            = "en";
  it.record_type     = record_type;
  it.body            = pj;
  it.properties_json = pj;
  it.tags_json       = tags;
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* "2026-08-09" → "08/09/2026". SAM.gov rejects ISO dates on postedFrom/To. */
static void pp_iso_to_us(const char *iso, char *out, size_t n) {
  if (!iso || strlen(iso) < 10) { if (n) out[0] = 0; return; }
  snprintf(out, n, "%.2s/%.2s/%.4s", iso + 5, iso + 8, iso);
}

/* ------------------------------------------------------------------ paging
 *
 * All three upstreams here are PAGED and all three were being read once.
 * Measured 2026-08-24:
 *   TED         `FT~"siemens"` since 2026-01-01 → totalNoticeCount 2,518, and
 *               the collector took the first 50. `limit` maxes at 250 (251 is
 *               a SEARCH_EXCEEDS_MAX 400) and `page` walks cleanly with no
 *               overlap between pages.
 *   USAspending "Boeing" → page_metadata.hasNext true at page 1, 2 and 3 with
 *               100 results each; the collector took page 1 and stopped.
 *   SAM.gov     `limit=50` with a `totalRecords` in the reply and an `offset`
 *               to advance.
 * Each walk now runs to the upstream's own end signal, with a ceiling that is
 * DISCLOSED as a collector-truncation-notice rather than a silent stop. These
 * are on-demand entity pivots, so the ceiling is per-query, not per-day. */
#define PP_PAGE_MAX 20   /* exhaustive-ok: page-walk ceiling; a stop with pages left emits a collector-truncation-notice, and $JO_PROCURE_PAGE_MAX raises it */

static int pp_page_max(void) {
  const char *e = jo_env("JO_PROCURE_PAGE_MAX");
  if (e && *e) { int v = atoi(e); if (v > 0) return v; }
  return PP_PAGE_MAX;
}

/* The upstream's own declared total for a search, or -1 when it declines to
 * say. Never estimated — an invented "available" is worse than none. */
static long pp_declared_total(const cJSON *root) {
  static const char *const KEYS[] = { "totalNoticeCount", "totalRecords",
                                      "total_records", "totalResults", NULL };
  for (int i = 0; KEYS[i]; i++) {
    const cJSON *v = cJSON_GetObjectItem(root, KEYS[i]);
    if (cJSON_IsNumber(v)) return (long) v->valuedouble;
  }
  return -1;
}

/* ------------------------------------------------- SAM_GOV_OPPORTUNITIES */

static int run_sam(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_looks_like_keyword(ctx->entity)) return 0;
  const char *key = jo_env("SAM_GOV_API_KEY");
  if (!key) {
    fprintf(stderr, "[sam-gov] gated (no SAM_GOV_API_KEY)\n");
    return 0;
  }

  /* SAM caps the posted window at one year and wants MM/dd/yyyy. */
  char from_iso[16], to_iso[16], from_us[16], to_us[16];
  jo_days_ago_iso(364, from_iso, sizeof from_iso);
  jo_days_ago_iso(0, to_iso, sizeof to_iso);
  pp_iso_to_us(from_iso, from_us, sizeof from_us);
  pp_iso_to_us(to_iso, to_us, sizeof to_us);

  char *enc = jo_urlencode(ctx->entity);
  char *ekey = jo_urlencode(key);
  if (!enc || !ekey) { free(enc); free(ekey); return 0; }

  const char *hdrs[] = { "Accept: application/json", NULL };
  const int page_max = pp_page_max();
  const int page_size = 100;              /* SAM's documented per-request max */
  int n = 0, pages = 0, truncated = 0;
  long available = -1;
  char disclose[512];
  snprintf(disclose, sizeof disclose,          /* the api_key never goes in a stored record */
           "https://api.sam.gov/opportunities/v2/search"
           "?title=%s&postedFrom=%s&postedTo=%s&limit=%d", enc, from_us, to_us,
           page_size);

  for (; pages < page_max; pages++) {
    char url[1300];
    snprintf(url, sizeof url,
             "https://api.sam.gov/opportunities/v2/search"
             "?api_key=%s&title=%s&postedFrom=%s&postedTo=%s&limit=%d&offset=%d",
             ekey, enc, from_us, to_us, page_size, pages * page_size);

    cJSON *root = feed_get_json_h(ctx->http, url, hdrs, 30000);
    if (!root) {
      if (pages == 0) { free(enc); free(ekey);
                        fprintf(stderr, "[sam-gov] fetch failed\n"); return -1; }
      truncated = 1;
      break;
    }
    if (available < 0) available = pp_declared_total(root);

    cJSON *arr = cJSON_GetObjectItem(root, "opportunitiesData");
    int got = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    cJSON *o;
    cJSON_ArrayForEach(o, arr) {
      const char *title = jo_sv(o, "title");
      if (!title) continue;                    /* no title → no row (R1) */
      const char *notice = jo_sv(o, "noticeId");
      const char *posted = jo_sv(o, "postedDate");
      const char *link   = jo_sv(o, "uiLink");

      cJSON *data = cJSON_Duplicate(o, 1);
      if (!data) continue;
      char rk[300];
      snprintf(rk, sizeof rk, "samgov:%s", notice ? notice : title);
      n += pp_emit(sink, "SAM_GOV_OPPORTUNITIES", data, "gov-opportunity", rk,
                   title, jo_sv(o, "type"), link, posted,
                   "[\"osint-search\",\"procurement\",\"opportunity\"]");
    }
    cJSON_Delete(root);

    /* A short page is SAM saying it has no more; asking again would be us
     * inventing a page it never offered. */
    if (got < page_size) { pages++; break; }
    if (pages + 1 >= page_max) truncated = 1;
  }
  free(enc); free(ekey);

  if (truncated)
    jo_trunc_notice(sink, "SAM_GOV_OPPORTUNITIES", disclose, n, available,
                    "page ceiling reached, or a mid-walk fetch failed, while "
                    "SAM.gov was still returning full pages",
                    "raise $JO_PROCURE_PAGE_MAX, or narrow the title keyword");

  fprintf(stderr, "[sam-gov] emitted %d across %d page(s)%s\n",
          n, pages, truncated ? " (TRUNCATED — notice emitted)" : "");
  return 0;
}

/* ------------------------------------------------------- TED_EU_TENDERS */

/* TED returns most human-readable values as a MULTILINGUAL OBJECT keyed by
 * ISO-639-3 language: {"eng":["Enterprise Ireland"]} — measured 2026-08-09.
 * Prefer English, else the first language present. Returns a borrowed pointer
 * into the document, or NULL. */
static const char *ted_ml(const cJSON *o, const char *key) {
  const cJSON *v = cJSON_GetObjectItem(o, key);
  if (!v) return NULL;
  if (cJSON_IsString(v)) return v->valuestring[0] ? v->valuestring : NULL;
  if (!cJSON_IsObject(v)) return NULL;
  const cJSON *pick = cJSON_GetObjectItem(v, "eng");
  if (!pick) pick = v->child;
  if (!pick) return NULL;
  if (cJSON_IsString(pick)) return pick->valuestring[0] ? pick->valuestring : NULL;
  if (cJSON_IsArray(pick)) {
    /* A DISPLAY pick, not a filter: run_ted duplicates the whole notice into
     * `data` (every language, every element of every multilingual array) and
     * adds this one as `buyer_name_resolved` alongside it, so nothing TED
     * returned is decided away here. */
    const cJSON *first = cJSON_GetArrayItem(pick, 0);  /* exhaustive-ok: display pick; the full multilingual object is duplicated into the emitted record */
    if (first && cJSON_IsString(first) && first->valuestring[0])
      return first->valuestring;
  }
  return NULL;
}

static int run_ted(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_looks_like_keyword(ctx->entity)) return 0;

  /* Expert-search syntax. The date bound is load-bearing: the API returns hits
   * in index order, NOT newest-first, and rejects any `sort` key (verified —
   * "Unrecognized field \"sort\""), so an unbounded query hands back notices
   * from 2016. Bound it to the recent window instead. */
  char since[16];
  jo_days_ago_iso(180, since, sizeof since);
  char expert[512];
  snprintf(expert, sizeof expert,
           "FT~\"%s\" AND publication-date>=%.4s%.2s%.2s",
           ctx->entity, since, since + 5, since + 8);

  const int page_max = pp_page_max();
  const int page_size = 250;      /* TED's maximum: 251 is a SEARCH_EXCEEDS_MAX 400 */
  int n = 0, pages = 0, truncated = 0;
  long available = -1;

  for (; pages < page_max; pages++) {
  cJSON *q = cJSON_CreateObject();
  cJSON_AddStringToObject(q, "query", expert);
  cJSON_AddNumberToObject(q, "limit", page_size);
  cJSON_AddNumberToObject(q, "page", pages + 1);
  cJSON *fields = cJSON_CreateArray();
  /* Exactly the fields the v3 API returns for these notices. `notice-title` is
   * NOT among them — requesting it is accepted and silently absent from every
   * row — so the title below is composed from what does come back. */
  static const char *const FIELDS[] = {
    "publication-number", "publication-date", "buyer-name",
    "buyer-country", "notice-type", "links", NULL
  };
  for (int i = 0; FIELDS[i]; i++)
    cJSON_AddItemToArray(fields, cJSON_CreateString(FIELDS[i]));
  cJSON_AddItemToObject(q, "fields", fields);
  char *body = cJSON_PrintUnformatted(q);
  cJSON_Delete(q);
  if (!body) break;

  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json", NULL };
  cJSON *root = feed_post_json(ctx->http,
                               "https://api.ted.europa.eu/v3/notices/search",
                               body, hdrs, 30000);
  free(body);
  if (!root) {
    if (pages == 0) { fprintf(stderr, "[ted-eu] fetch failed\n"); return -1; }
    truncated = 1;
    break;
  }
  if (available < 0) available = pp_declared_total(root);

  cJSON *arr = cJSON_GetObjectItem(root, "notices");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "results");
  int got = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;

  cJSON *o;
  cJSON_ArrayForEach(o, arr) {
    const char *pub = jo_sv(o, "publication-number");
    if (!pub) continue;                          /* no identity → no row (R1) */
    const char *buyer = ted_ml(o, "buyer-name");
    const char *type  = jo_sv(o, "notice-type");

    /* publication-date arrives as "2026-06-01+02:00"; keep the date part. */
    const char *raw = jo_sv(o, "publication-date");
    char date[16] = {0};
    if (raw && strlen(raw) >= 10) snprintf(date, sizeof date, "%.10s", raw);

    /* The canonical English detail page, taken from the payload rather than
     * assembled, so a URL scheme change upstream cannot leave us fabricating. */
    const cJSON *links = cJSON_GetObjectItem(o, "links");
    const char *link = jo_sv(cJSON_GetObjectItem(links, "html"), "ENG");

    cJSON *data = cJSON_Duplicate(o, 1);
    if (!data) continue;
    if (buyer) cJSON_AddStringToObject(data, "buyer_name_resolved", buyer);

    char rk[300], title[480];
    snprintf(rk, sizeof rk, "ted:%s", pub);
    snprintf(title, sizeof title, "EU tender %s — %s%s%s", pub,
             buyer ? buyer : "buyer not stated",
             type ? " / " : "", type ? type : "");
    n += pp_emit(sink, "TED_EU_TENDERS", data, "gov-opportunity", rk, title,
                 buyer, link, date[0] ? date : NULL,
                 "[\"osint-search\",\"procurement\",\"opportunity\"]");
  }
  cJSON_Delete(root);

  /* A short page is TED saying it is finished. */
  if (got < page_size) { pages++; break; }
  if (pages + 1 >= page_max) truncated = 1;
  }

  if (truncated)
    jo_trunc_notice(sink, "TED_EU_TENDERS",
                    "https://api.ted.europa.eu/v3/notices/search", n, available,
                    "page ceiling reached, or a mid-walk fetch failed, while "
                    "TED was still returning full pages of notices",
                    "raise $JO_PROCURE_PAGE_MAX, or narrow the full-text query "
                    "or its publication-date bound");

  fprintf(stderr, "[ted-eu] emitted %d across %d page(s)%s\n",
          n, pages, truncated ? " (TRUNCATED — notice emitted)" : "");
  return 0;
}

/* ------------------------------- USAspending: shared fetch for the two pivots */

/* POST one page of the award search for `recipient`. Returns the parsed reply
 * (caller cJSON_Delete) or NULL on fetch failure. */
static cJSON *usa_awards(const source_ctx *ctx, const char *recipient, int page) {
  cJSON *body = cJSON_CreateObject();
  cJSON *filters = cJSON_CreateObject();

  /* A/B/C/D = the definitive contract award types. */
  cJSON *types = cJSON_CreateArray();
  static const char *const T[] = { "A", "B", "C", "D", NULL };
  for (int i = 0; T[i]; i++) cJSON_AddItemToArray(types, cJSON_CreateString(T[i]));
  cJSON_AddItemToObject(filters, "award_type_codes", types);

  cJSON *rs = cJSON_CreateArray();
  cJSON_AddItemToArray(rs, cJSON_CreateString(recipient));
  cJSON_AddItemToObject(filters, "recipient_search_text", rs);
  cJSON_AddItemToObject(body, "filters", filters);

  cJSON *fields = cJSON_CreateArray();
  static const char *const F[] = {
    "Award ID", "Recipient Name", "Award Amount", "Awarding Agency",
    "Awarding Sub Agency", "Start Date", "End Date", "Description",
    "generated_internal_id", NULL
  };
  for (int i = 0; F[i]; i++) cJSON_AddItemToArray(fields, cJSON_CreateString(F[i]));
  cJSON_AddItemToObject(body, "fields", fields);

  /* `limit` is USAspending's documented per-request maximum. NOTE: the API
   * 400s when `sort` names a field that is not in `fields`, so the two lists
   * must stay in step. */
  cJSON_AddNumberToObject(body, "page", page);
  cJSON_AddNumberToObject(body, "limit", 100);
  cJSON_AddStringToObject(body, "sort", "Award Amount");
  cJSON_AddStringToObject(body, "order", "desc");

  char *bj = cJSON_PrintUnformatted(body);
  cJSON_Delete(body);
  if (!bj) return NULL;

  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json", NULL };
  cJSON *root = feed_post_json(ctx->http,
      "https://api.usaspending.gov/api/v2/search/spending_by_award/",
      bj, hdrs, 30000);
  free(bj);
  return root;
}

/* Shared row builder. When `horizon` is non-NULL only awards whose End Date
 * falls in [today, horizon] are emitted (ISO dates compare lexicographically). */
static int usa_emit(intel_sink *sink, cJSON *root, const char *service,
                    const char *record_type, const char *tags,
                    const char *today, const char *horizon) {
  cJSON *arr = cJSON_GetObjectItem(root, "results");
  int n = 0;
  cJSON *o;
  cJSON_ArrayForEach(o, arr) {
    const char *name = jo_sv(o, "Recipient Name");
    const char *aid  = jo_sv(o, "Award ID");
    if (!name && !aid) continue;                /* no label → no row (R1) */
    const char *start = jo_sv(o, "Start Date");
    const char *end   = jo_sv(o, "End Date");

    if (horizon) {
      if (!end) continue;                       /* cannot judge → do not claim */
      if (strcmp(end, today) < 0 || strcmp(end, horizon) > 0) continue;
    }

    cJSON *data = cJSON_Duplicate(o, 1);
    if (!data) continue;
    if (end) cJSON_AddStringToObject(data, "period_end", end);

    const char *gid = jo_sv(o, "generated_internal_id");
    char rk[320], link[420] = {0}, title[480];
    snprintf(rk, sizeof rk, "usaward:%s", aid ? aid : (gid ? gid : name));
    if (gid)
      snprintf(link, sizeof link, "https://www.usaspending.gov/award/%s", gid);
    if (horizon)
      snprintf(title, sizeof title, "Contract expiring %s — %s (%s)",
               end ? end : "?", name ? name : "recipient", aid ? aid : "no id");
    else
      snprintf(title, sizeof title, "Federal award %s — %s",
               aid ? aid : "(no id)", name ? name : "recipient");

    n += pp_emit(sink, service, data, record_type, rk, title,
                 jo_sv(o, "Awarding Agency"), link[0] ? link : NULL,
                 start, tags);
  }
  return n;
}

/* Walk every page USAspending offers for `recipient`, emitting each through
 * usa_emit. Both pivots share it so they page identically. Returns rows
 * emitted, or -1 when the FIRST page failed (a dead endpoint stays an error).
 *
 * `page_metadata.hasNext` is the upstream's own end signal — verified live:
 * "Boeing" reports hasNext true through pages 1, 2 and 3 at 100 results each,
 * and the collector used to take page 1 and stop. USAspending publishes no
 * total, so a ceiling stop reports an unknown remainder, honestly. */
static int usa_walk(const source_ctx *ctx, intel_sink *sink,
                    const char *service, const char *record_type,
                    const char *tags, const char *today, const char *horizon,
                    const char *tag) {
  const int page_max = pp_page_max();
  int n = 0, pages = 0, truncated = 0;

  for (; pages < page_max; pages++) {
    cJSON *root = usa_awards(ctx, ctx->entity, pages + 1);
    if (!root) {
      if (pages == 0) { fprintf(stderr, "[%s] fetch failed\n", tag); return -1; }
      truncated = 1;
      break;
    }
    n += usa_emit(sink, root, service, record_type, tags, today, horizon);

    const cJSON *meta = cJSON_GetObjectItem(root, "page_metadata");
    const cJSON *has = cJSON_IsObject(meta)
                         ? cJSON_GetObjectItem(meta, "hasNext") : NULL;
    int more = cJSON_IsTrue(has);
    cJSON_Delete(root);
    if (!more) { pages++; break; }              /* upstream is exhausted */
    if (pages + 1 >= page_max) truncated = 1;
  }

  if (truncated)
    jo_trunc_notice(sink, service,
                    "https://api.usaspending.gov/api/v2/search/"
                    "spending_by_award/", n, -1,
                    "page ceiling reached, or a mid-walk fetch failed, while "
                    "USAspending still reported page_metadata.hasNext (it "
                    "publishes no award total)",
                    "raise $JO_PROCURE_PAGE_MAX, or narrow the recipient name");

  fprintf(stderr, "[%s] emitted %d across %d page(s)%s\n", tag, n, pages,
          truncated ? " (TRUNCATED — notice emitted)" : "");
  return n;
}

static int run_award_supplier(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_looks_like_keyword(ctx->entity)) return 0;
  int n = usa_walk(ctx, sink, "AWARD_SUPPLIER_SEARCH", "gov-award",
                   "[\"osint-search\",\"procurement\",\"award\"]", NULL, NULL,
                   "award-supplier");
  return n < 0 ? -1 : 0;
}

static int run_contract_expiry(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_looks_like_keyword(ctx->entity)) return 0;

  int days = 365;
  const char *cfg = jo_env("JO_CONTRACT_EXPIRY_DAYS");
  if (cfg) {
    int v = atoi(cfg);
    if (v > 0 && v <= 3650) days = v;
  }
  char today[16], horizon[16];
  jo_days_ago_iso(0, today, sizeof today);
  jo_days_ago_iso(-days, horizon, sizeof horizon);   /* negative → forward */

  int n = usa_walk(ctx, sink, "CONTRACT_EXPIRY_WATCH", "gov-award-expiring",
                   "[\"osint-search\",\"procurement\",\"expiring\"]",
                   today, horizon, "contract-expiry");
  if (n < 0) return -1;
  fprintf(stderr, "[contract-expiry] window %s..%s\n", today, horizon);
  return 0;
}

/* ------------------------------------------------------------- definitions */

static const source_def sam_gov_def = {
  .id = "SAM_GOV_OPPORTUNITIES", .collector = "osint",
  .name = "SAM.gov Contract Opportunities", .name_ja = "米国連邦調達公告",
  .update_interval_sec = 0, .run = run_sam,
  .category = "government", .type = "api",
  .url = "internal://osint/sam-gov-opportunities",
  .description = "Open US federal solicitations whose title matches a keyword, from "
                 "the last year (needs free SAM_GOV_API_KEY).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sam_gov_def)

static const source_def ted_eu_def = {
  .id = "TED_EU_TENDERS", .collector = "osint",
  .name = "TED EU Tenders", .name_ja = "EU官報 入札公告",
  .update_interval_sec = 0, .run = run_ted,
  .category = "government", .type = "api",
  .url = "internal://osint/ted-eu-tenders",
  .description = "EU Tenders Electronic Daily notices matching a full-text query — "
                 "buyer, notice type, publication date. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ted_eu_def)

static const source_def award_supplier_def = {
  .id = "AWARD_SUPPLIER_SEARCH", .collector = "osint",
  .name = "US Federal Awards by Supplier", .name_ja = "米国連邦調達 受注企業検索",
  .update_interval_sec = 0, .run = run_award_supplier,
  .category = "government", .type = "api",
  .url = "internal://osint/award-supplier-search",
  .description = "US federal contract awards won by a named recipient — award id, "
                 "amount, awarding agency, period of performance. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(award_supplier_def)

static const source_def contract_expiry_def = {
  .id = "CONTRACT_EXPIRY_WATCH", .collector = "osint",
  .name = "Incumbent Contract Expiry", .name_ja = "現行契約 満了ウォッチ",
  .update_interval_sec = 0, .run = run_contract_expiry,
  .category = "government", .type = "api",
  .url = "internal://osint/contract-expiry-watch",
  .description = "US federal awards held by a recipient whose period of performance "
                 "ends within the next year (JO_CONTRACT_EXPIRY_DAYS) — i.e. when the "
                 "incumbent's work becomes contestable. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(contract_expiry_def)
