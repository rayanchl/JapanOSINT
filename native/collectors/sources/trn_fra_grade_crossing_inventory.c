/* US FRA — national highway-rail grade crossing inventory (Form 71).
 * Endpoint: https://data.transportation.gov/resource/m2f8-22s6.json?$limit=1000
 * Emits: one intel row per crossing — crossingid, operating railroad, state,
 * county and city, the crossing street, the railroad milepost, the nearest
 * timetable station, the inventory reason description and the revision date
 * under readable names, PLUS every column the row carried, verbatim (126 on
 * 2026-10-06: warning devices, track and train counts, speeds, traffic, school
 * bus use, reporting railroad and agency, the FRA crossing-report URL, the
 * filer-reported latitude/longitude …). Keyless.
 * Licence: US Federal Railroad Administration via data.transportation.gov
 * (Socrata) — US Government work, public domain.
 *
 * Parse notes: Socrata SoDA JSON. The current-inventory resource m2f8-22s6
 * DOES carry the crossing's position — `latitude`/`longitude` as decimal
 * strings and `geocoded_lat_long` as a GeoJSON Point, on the rows that have
 * one (full walk 2026-10-06: 362,264 of 438,854 crossings; a row without a
 * position simply omits the columns). An earlier note here said the resource
 * had no position, and the collector kept ten columns of 126, so every
 * crossing was stored without the position it was served with. The row's own latitude/longitude is now its geometry (tagged
 * geo_precision "form71-reported-crossing-position": the filer's figure, not
 * an FRA survey); a row without a valid pair stays has_geo = 0 — a position is
 * never synthesised from countyname/cityname (R2).
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
#include "lib/keyqual.h"
#include "trn_common.inc"

#define FRA_URL "https://data.transportation.gov/resource/m2f8-22s6.json?$limit=1000&$order=:id"
#define FRA_PAGE 1000
/* Measured 2026-09-21: `?$select=count(1)` returns 438,835. 400 pages of 1,000
 * therefore could not reach the end of this table even with a correct walk —
 * the guard bit at exactly 400,000 and fired its truncation notice, which is
 * the notice doing its job. Set above the measured count with headroom for the
 * inventory to grow, not to a round number that merely happens to fit today. */
#define FRA_MAX_PAGES 600   /* exhaustive-ok: page-walk runaway guard (438,835 rows measured 2026-09-21); an early stop emits a collector-truncation-notice */

/* IDENTITY. crossingid is the crossing's own DOT number and the key — but
 * the inventory holds a handful of crossings TWICE, as different rows.
 * Measured 2026-10-07 with `$group=crossingid&$having=count(*) > 1`: 11
 * crossingids carry two rows each (the 11 of 438,865 the sink folded). They
 * are not byte-identical copies: in every pair one row carries an agencyid
 * (1141, 230, 520, 627, 2458, 13250) and the other carries none, and in six
 * pairs the rows also differ in revisiondate, reasoncode/reasondescription
 * ("Change in Primary Operating RR" vs "Closed"), railroadmilepostnumber or
 * statenarrative. Keyed on crossingid alone, whichever row the walk met
 * second overwrote the first.
 *
 * Both rows are kept, and which one is which no longer depends on walk order
 * (lib/keyqual.h): every member of a colliding group is qualified by its
 * agencyid — `022128G|1141` — and the row without one by the literal
 * `no-agencyid`, so its uid does not move each time its content is revised
 * (keyqual's content hash would). Two rows of one crossing that share an
 * agencyid fall through to keyqual's content hash; a byte-identical repeat
 * is folded.
 *
 * keyqual needs the whole group before the first uid is decided, and this
 * walk is 439 pages of ~120-column rows — far too much to buffer. So the
 * census is asked of the UPSTREAM before the walk, in two small queries: the
 * crossingids that recur, then the agencyid of each of their rows. A unique
 * crossingid is not in the census and keeps its plain uid, exactly as before.
 * A census that fails is logged and the walk keys every row on crossingid as
 * it used to — the run line's UID-COLLISION then shows what folded. */
#define FRA_RES "https://data.transportation.gov/resource/m2f8-22s6.json"
#define FRA_CENSUS_CHUNK 100   /* exhaustive-ok: ids per `in (...)` query; every chunk is asked */

static const char *fra_disc(const cJSON *r) {
  const char *a = jo_sv(r, "agencyid");
  return a ? a : "no-agencyid";
}

/* An id safe to splice into a SoQL string literal: [0-9A-Za-z]. */
static int fra_id_ok(const char *s) {
  if (!s || !*s || strlen(s) > 32) return 0;
  for (const char *p = s; *p; p++)
    if (!((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') ||
          (*p >= 'a' && *p <= 'z'))) return 0;
  return 1;
}

/* One SoQL query against the resource: `kv` is {name, value, name, value, …,
 * NULL}; each value is percent-encoded. NULL unless the answer is an array. */
static cJSON *fra_soql(const source_ctx *ctx, const char *const *kv) {
  size_t cap = strlen(FRA_RES) + 2;
  for (int i = 0; kv[i] && kv[i + 1]; i += 2) cap += strlen(kv[i]) + 3 * strlen(kv[i + 1]) + 2;
  char *url = malloc(cap);
  if (!url) return NULL;
  size_t used = (size_t)snprintf(url, cap, "%s", FRA_RES);
  for (int i = 0; kv[i] && kv[i + 1]; i += 2) {
    char *ev = jo_urlencode(kv[i + 1]);
    if (!ev) { free(url); return NULL; }
    used += (size_t)snprintf(url + used, cap - used, "%c%s=%s",
                             i ? '&' : '?', kv[i], ev);
    free(ev);
  }
  cJSON *doc = feed_get_json(ctx->http, url, 120000);
  free(url);
  if (doc && !cJSON_IsArray(doc)) { cJSON_Delete(doc); doc = NULL; }
  return doc;
}

/* Pass 1 of keyqual, asked of the upstream. Returns the number of colliding
 * crossingids found, or -1 when the census could not be taken. */
static int fra_census(const source_ctx *ctx, keyqual *kq) {
  const char *const gq[] = { "$select", "crossingid,count(*) as n",
                             "$group", "crossingid", "$having", "count(*) > 1",
                             "$limit", "50000", NULL };
  cJSON *groups = fra_soql(ctx, gq);
  if (!groups) return -1;
  cJSON *ids = cJSON_CreateArray();
  const cJSON *g;
  cJSON_ArrayForEach(g, groups) {
    const char *cid = jo_sv(g, "crossingid");
    if (fra_id_ok(cid)) cJSON_AddItemToArray(ids, cJSON_CreateString(cid));
    else if (cid) fprintf(stderr, "[fra-grade-crossing-inventory] census: "
                          "crossingid \"%s\" cannot be quoted into SoQL; its "
                          "rows keep the plain key\n", cid);
  }
  cJSON_Delete(groups);
  int nid = cJSON_GetArraySize(ids), rc = nid;
  for (int i = 0; i < nid && rc >= 0; i += FRA_CENSUS_CHUNK) {
    size_t cap = 64 + (size_t)FRA_CENSUS_CHUNK * 40;
    char *where = malloc(cap);
    if (!where) { rc = -1; break; }
    size_t used = (size_t)snprintf(where, cap, "crossingid in (");
    for (int j = i; j < nid && j < i + FRA_CENSUS_CHUNK; j++)
      used += (size_t)snprintf(where + used, cap - used, "%s'%s'",
                               j > i ? "," : "",
                               cJSON_GetArrayItem(ids, j)->valuestring);
    snprintf(where + used, cap - used, ")");
    const char *const rq[] = { "$select", "crossingid,agencyid",
                               "$where", where, "$limit", "50000", NULL };
    cJSON *rows = fra_soql(ctx, rq);
    free(where);
    if (!rows) { rc = -1; break; }
    const cJSON *r;
    cJSON_ArrayForEach(r, rows) {
      const char *cid = jo_sv(r, "crossingid");
      if (cid) keyqual_add(kq, cid, fra_disc(r));
    }
    cJSON_Delete(rows);
  }
  cJSON_Delete(ids);
  keyqual_seal(kq);
  return rc;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0, capped = 0, mid_fail = 0, folded = 0;
  long seen = 0;
  keyqual kq = {0};
  int census = fra_census(ctx, &kq);
  if (census < 0)
    fprintf(stderr, "[fra-grade-crossing-inventory] census of recurring "
                    "crossingids failed; keying every row on crossingid\n");
  else
    fprintf(stderr, "[fra-grade-crossing-inventory] census: %d crossingid(s) "
                    "carry more than one row\n", census);
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
        keyqual_free(&kq);
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
      const char *rk = cid;
      char rkbuf[256];
      if (census > 0 && keyqual_count(&kq, cid) > 1) {
        char *raw = cJSON_PrintUnformatted(r);
        rk = keyqual_uid(&kq, cid, fra_disc(r), raw ? raw : "", rkbuf, sizeof rkbuf);
        free(raw);
        if (!keyqual_claim(&kq, cid, rk)) { folded++; continue; }
      }

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
      /* ...and every column the row carried, verbatim (house rule 2): the
       * readable names above are a view, not a reason to drop the other
       * ~115 columns of Form 71 this walk already paid for. Socrata's
       * ":"-prefixed internals and JSON nulls are skipped. */
      for (const cJSON *f = r->child; f; f = f->next) {
        if (!f->string || f->string[0] == ':' || cJSON_IsNull(f)) continue;
        if (cJSON_GetObjectItem(pr, f->string)) continue;
        cJSON_AddItemToObject(pr, f->string, cJSON_Duplicate(f, 1));
      }
      /* the row's own position, never a synthesised one (R2). Form 71
       * coordinates are reported by the railroad or state agency that filed
       * the inventory record, not surveyed by FRA. */
      double la, lo;
      int geo = trn_num(r, "latitude", &la) && trn_num(r, "longitude", &lo) &&
                trn_geo_ok(la, lo);
      if (geo) cJSON_AddStringToObject(pr, "geo_precision", "form71-reported-crossing-position");
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
      it.remote_key      = rk;
      it.title           = title;
      it.summary         = summary[0] ? summary : NULL;
      it.lang            = "en";
      it.published_at    = jo_sv(r, "revisiondate");
      it.record_type     = "rail-grade-crossing";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"transport\",\"rail\",\"us\",\"infrastructure\"]";
      if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
      /* FRA's own crossing-inventory page for this crossing, as served */
      it.link = jo_sv(cJSON_GetObjectItem(r, "url"), "url");
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
      cJSON_Delete(pr);
    }
    cJSON_Delete(doc);
    if (got < FRA_PAGE) break;                       /* short page: exhausted */
    if (page == FRA_MAX_PAGES - 1) capped = 1;        /* full last page: guard hit */
  }
  /* records_used is what was EMITTED (n), not rows served (`seen`): a row
   * with no crossingid is served and skipped, and counting it as used
   * overstated the run in the very record that exists to state it. */
  if (capped)
    jo_trunc_notice(sink, "fra-grade-crossing-inventory",
                     "data.transportation.gov/resource/m2f8-22s6.json", n, -1,
                     "page-walk hit its runaway guard (FRA_MAX_PAGES) before a "
                     "short page signalled the end of the Socrata table",
                     "raise FRA_MAX_PAGES in trn_fra_grade_crossing_inventory.c");
  if (mid_fail)
    jo_trunc_notice(sink, "fra-grade-crossing-inventory",
                     "data.transportation.gov/resource/m2f8-22s6.json", n, -1,
                     "a page fetch failed mid-walk before the Socrata table was "
                     "exhausted; the true total is unknown",
                     "re-run; a transient upstream failure should clear on retry");
  keyqual_free(&kq);
  fprintf(stderr, "[fra-grade-crossing-inventory] emitted %d of %ld served (%d "
          "byte-identical repeats folded)\n", n, seen, folded);
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
