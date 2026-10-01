/* US FRA — national highway-rail grade crossing inventory (Form 71).
 * Endpoint: https://data.transportation.gov/resource/m2f8-22s6.json?$limit=1000
 * Emits: one intel row per crossing — crossingid, operating railroad, state,
 * county and city, the crossing street, the railroad milepost, the nearest
 * timetable station, the inventory reason description and the revision date.
 * Keyless.
 * Licence: US Federal Railroad Administration via data.transportation.gov
 * (Socrata) — US Government work, public domain.
 *
 * Parse notes: Socrata SoDA JSON. The current-inventory resource m2f8-22s6 does
 * NOT expose latitude/longitude in the fields it returns, so every row here is
 * emitted with has_geo = 0. Synthesising a position from countyname/cityname
 * would be exactly the invented geometry R2 forbids; the companion "Grade
 * Crossings" resource nw2s-ygjq is the geospatial one for anybody who needs it.
 *
 * Socrata's $limit is a page size, not a bound on the table: this resource
 * holds 438,835 rows (measured 2026-09-21 with `?$select=count(1)`, where an
 * earlier note here guessed "well over 200,000"), so a single $limit=1000
 * request (the previous shape of this collector) captured well under 1% of it,
 * silently, forever. Now paged via $offset AND $order=:id — the sort is part of
 * the paging, not decoration, see the note in the loop — until a page comes
 * back shorter than requested, or a runaway-guard page ceiling is hit, which
 * is disclosed as a collector-truncation-notice (2026-09-03 audit).
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define FRA_URL "https://data.transportation.gov/resource/m2f8-22s6.json?$limit=1000&$order=:id"
#define FRA_PAGE 1000
/* Measured 2026-09-21: `?$select=count(1)` returns 438,835. 400 pages of 1,000
 * therefore could not reach the end of this table even with a correct walk —
 * the guard bit at exactly 400,000 and fired its truncation notice, which is
 * the notice doing its job. Set above the measured count with headroom for the
 * inventory to grow, not to a round number that merely happens to fit today. */
#define FRA_MAX_PAGES 600   /* exhaustive-ok: page-walk runaway guard (438,835 rows measured 2026-09-21); an early stop emits a collector-truncation-notice */

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0, capped = 0, mid_fail = 0;
  long seen = 0;
  for (int page = 0; page < FRA_MAX_PAGES; page++) {
    char url[192];
    /* $order=:id is not optional on a Socrata $offset walk. Without a stable
     * sort the server may order differently between requests, so consecutive
     * windows overlap — the same row is served twice while another is never
     * served at all. FRA_URL above has always carried the clause; this loop
     * composed its own URL and dropped it, so the clause documented the source
     * without ever reaching the wire.
     *
     * Measured 2026-09-21: pages 0 and 1 of this resource share 80 crossingids
     * without $order and 0 with it, and the run emitted 400,000 while storing
     * 366,341 — `UID-COLLISION: 33659 of 400000`. The identity was never the
     * problem (crossingid is unique 1,000 of 1,000 within an ordered page); the
     * walk was re-serving rows it had already emitted. */
    snprintf(url, sizeof url,
      "https://data.transportation.gov/resource/m2f8-22s6.json"
      "?$limit=%d&$offset=%d&$order=:id",
      FRA_PAGE, page * FRA_PAGE);
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc || !cJSON_IsArray(doc)) {
      cJSON_Delete(doc);
      if (page == 0) {
        fprintf(stderr, "[fra-grade-crossing-inventory] fetch/parse failed\n");
        return -1;                                     /* the fetch failed (R3) */
      }
      fprintf(stderr, "[fra-grade-crossing-inventory] page at offset %d failed; "
                      "stopping with %d rows\n", page * FRA_PAGE, n);
      mid_fail = 1;
      break;
    }
    int got = cJSON_GetArraySize(doc);
    seen += got;

    cJSON *r;
    cJSON_ArrayForEach(r, doc) {
      const char *cid = jo_sv(r, "crossingid");
      if (!cid) continue;

      cJSON *pr = cJSON_CreateObject();
      cJSON_AddStringToObject(pr, "operator",
                              "US Federal Railroad Administration");
      cJSON_AddStringToObject(pr, "crossing_id", cid);
      trn_put_str(pr, "railroad_name", jo_sv(r, "railroadname"));
      trn_put_str(pr, "state_name", jo_sv(r, "statename"));
      trn_put_str(pr, "county_name", jo_sv(r, "countyname"));
      trn_put_str(pr, "city_name", jo_sv(r, "cityname"));
      trn_put_str(pr, "street", jo_sv(r, "street"));
      trn_put_str(pr, "railroad_milepost", jo_sv(r, "railroadmilepostnumber"));
      trn_put_str(pr, "nearest_timetable_station",
                  jo_sv(r, "nearesttimetablestation"));
      trn_put_str(pr, "reason_description", jo_sv(r, "reasondescription"));
      trn_put_str(pr, "revision_date", jo_sv(r, "revisiondate"));
      char *pj = cJSON_PrintUnformatted(pr);

      const char *street = jo_sv(r, "street");
      const char *rr = jo_sv(r, "railroadname");
      char title[320], summary[256];
      if (street && rr) snprintf(title, sizeof title, "%s at %s", rr, street);
      else snprintf(title, sizeof title, "Grade crossing %s", cid);
      const char *st = jo_sv(r, "statename"), *co = jo_sv(r, "countyname");
      if (st && co) snprintf(summary, sizeof summary, "%s / %s", st, co);
      else if (st)  snprintf(summary, sizeof summary, "%s", st);
      else summary[0] = 0;

      intel_item it = {0};
      it.remote_key      = cid;
      it.title           = title;
      it.summary         = summary[0] ? summary : NULL;
      it.lang            = "en";
      it.published_at    = jo_sv(r, "revisiondate");
      it.record_type     = "rail-grade-crossing";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"transport\",\"rail\",\"us\",\"infrastructure\"]";
      /* resource carries no coordinates → no geo (R2) */
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
      cJSON_Delete(pr);
    }
    cJSON_Delete(doc);
    if (got < FRA_PAGE) break;                       /* short page: exhausted */
    if (page == FRA_MAX_PAGES - 1) capped = 1;        /* full last page: guard hit */
  }
  if (capped)
    jo_trunc_notice(sink, "fra-grade-crossing-inventory",
                     "data.transportation.gov/resource/m2f8-22s6.json", seen, -1,
                     "page-walk hit its runaway guard (FRA_MAX_PAGES) before a "
                     "short page signalled the end of the Socrata table",
                     "raise FRA_MAX_PAGES in trn_fra_grade_crossing_inventory.c");
  if (mid_fail)
    jo_trunc_notice(sink, "fra-grade-crossing-inventory",
                     "data.transportation.gov/resource/m2f8-22s6.json", seen, -1,
                     "a page fetch failed mid-walk before the Socrata table was "
                     "exhausted; the true total is unknown",
                     "re-run; a transient upstream failure should clear on retry");
  fprintf(stderr, "[fra-grade-crossing-inventory] emitted %d\n", n);
  return 0;
}

static const source_def trn_fra_grade_crossing_inventory_def = {
  .id = "fra-grade-crossing-inventory", .collector = "transport",
  .name = "US FRA highway-rail grade crossing inventory",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = FRA_URL,
  .description = "The FRA Form 71 national inventory of public and private highway-rail grade crossings — railroad, milepost, street and warning-device detail.",
  .license = "US Government work, public domain (FRA via data.transportation.gov).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_fra_grade_crossing_inventory_def)
