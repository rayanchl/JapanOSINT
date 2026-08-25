/* collectors/sources/tsp_ised_spectrum_sites.c
 * ISED Canada terrestrial spectrum licence site data — a genuine keyless
 * cell/fixed-wireless site dataset (not an OSM proxy).
 * Endpoint: https://services.arcgis.com/wjcPoefzjpzCgffS/ArcGIS/rest/services/
 *   Spectrum_Licences_Site_Data/FeatureServer/0/query?where=1=1&outFields=*&f=json
 *   (keyless Esri FeatureServer)
 * Emits every attribute the layer returns — licence numbers, LICENSEE, SERVICE
 *   class (CELL/PCS/WCS/AWS...), TRANSMIT_FREQ / RECEIVE_FREQ / TRANSMIT_BW,
 *   LOCATION, PROV, site elevation, structure height, transmitter make/model/
 *   power, emission designator, antenna make/model/height/azimuth/elevation
 *   angle/gain and line loss — plus the per-site coordinates.
 *
 * parse_notes honoured, quoted:
 *  - "Esri REST: f=json returns {fields,features:[{attributes,geometry}]}";
 *    "spatialReference wkid 4326, geometry.x = longitude, geometry.y =
 *    latitude, duplicated in the LONGITUDE/LATITUDE attributes" — geometry.x/y
 *    is used and the LATITUDE/LONGITUDE attributes are kept as the cross-check
 *    (R2: this is the licensed SITE, not the carrier's head office).
 *  - "Paginate with resultOffset/resultRecordCount (server caps ~2,000/request,
 *    exceededTransferLimit flags more)": PAGE_SIZE 2000, PAGES pages, and the
 *    exceededTransferLimit flag is logged.
 *  - "LAST_UPLOAD_DATE is epoch ms and on sampled rows is 2016 — state the
 *    vintage in the row, the extract is not refreshed daily": the epoch is
 *    converted to an ISO timestamp and emitted as data_vintage.
 * Licence: ISED / Government of Canada spectrum data under the Open Government
 *   Licence - Canada, served via a public ArcGIS FeatureServer with no key.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include "_timefmt.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ISED_BASE \
  "https://services.arcgis.com/wjcPoefzjpzCgffS/ArcGIS/rest/services/" \
  "Spectrum_Licences_Site_Data/FeatureServer/0/query"
/* The layer's own `maxRecordCount` (measured 2026-08-24). Asking for more just
 * returns 1000 with exceededTransferLimit set, so requesting 2000 only made the
 * request and the reply disagree about how far the walk had got. */
#define PAGE_SIZE 1000
#define PAGES 3

/* epoch milliseconds -> ISO-8601 UTC */
/* The gmtime failure was handled; strftime's 0 was not — and on that path the
 * buffer is unspecified (no NUL written) yet this returned 1, so the caller
 * emitted stack bytes as a timestamp. jo_ms_iso checks both. */
static int epoch_ms_iso(double ms, char *out, size_t n) {
  return jo_ms_iso(ms, out, n) != NULL;
}

static int emit_features(cJSON *feats, intel_sink *sink) {
  int n = 0;
  cJSON *ft;
  cJSON_ArrayForEach(ft, feats) {
    cJSON *at = cJSON_GetObjectItem(ft, "attributes");
    cJSON *gm = cJSON_GetObjectItem(ft, "geometry");
    if (!cJSON_IsObject(at)) continue;

    const char *licno = jo_sv(at, "NEW_LICNO");
    const char *who   = jo_sv(at, "LICENSEE");
    const char *svc   = jo_sv(at, "SERVICE");
    const char *loc   = jo_sv(at, "LOCATION");
    const char *prov  = jo_sv(at, "PROV");
    if (!licno && !who) continue;                 /* no identity -> no row */

    /* R2: geometry.x/y from the layer itself (wkid 4326) */
    double lat = 0, lon = 0;
    int has_geo = 0;
    if (cJSON_IsObject(gm)) {
      double x, y;
      if (jo_num(gm, "x", &x) && jo_num(gm, "y", &y) &&
          y >= -90.0 && y <= 90.0 && x >= -180.0 && x <= 180.0 &&
          !(x == 0.0 && y == 0.0)) {
        lon = x; lat = y; has_geo = 1;
      }
    }
    if (!has_geo) {           /* fall back to the duplicated attributes only */
      double la, lo;
      if (jo_num(at, "LATITUDE", &la) && jo_num(at, "LONGITUDE", &lo) &&
          la >= -90.0 && la <= 90.0 && lo >= -180.0 && lo <= 180.0 &&
          !(la == 0.0 && lo == 0.0)) {
        lat = la; lon = lo; has_geo = 1;
      }
    }

    /* copy every attribute the service returned; all are fetched values */
    cJSON *pr = cJSON_CreateObject();
    const cJSON *a;
    cJSON_ArrayForEach(a, at) {
      if (!a->string) continue;
      if (cJSON_IsString(a) && a->valuestring && a->valuestring[0])
        cJSON_AddStringToObject(pr, a->string, a->valuestring);
      else if (cJSON_IsNumber(a))
        cJSON_AddNumberToObject(pr, a->string, a->valuedouble);
    }
    double lud;
    char vintage[40];
    if (jo_num(at, "LAST_UPLOAD_DATE", &lud) && epoch_ms_iso(lud, vintage, sizeof vintage))
      cJSON_AddStringToObject(pr, "data_vintage", vintage);
    else
      vintage[0] = '\0';
    cJSON_AddStringToObject(pr, "vintage_note",
      "static ISED extract — LAST_UPLOAD_DATE is the publisher's own upload "
      "timestamp, not the time of this fetch");
    if (has_geo) {
      cJSON_AddStringToObject(pr, "geo_subject", "licensed transmitter site");
      cJSON_AddStringToObject(pr, "geo_crs", "WGS84 (wkid 4326)");
    }
    cJSON_AddStringToObject(pr, "source", "ISED Canada Spectrum_Licences_Site_Data (ArcGIS)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    double txf = 0;
    int has_txf = jo_num(at, "TRANSMIT_FREQ", &txf);
    /* IDENTITY — house rule 4b, measured. `licence|lat|lon` is a DIMENSION,
     * not this record's identity: one licence number covers several emissions
     * at one site (different TRANSMIT_FREQ / azimuth / antenna), and they all
     * collapsed onto one uid. Measured on 2026-08-24:
     *   [sched] ised-spectrum-sites run rc=0 records=1000 stored=973
     *           UID-COLLISION: 27 of 1000 …
     * OBJECTID is the FeatureServer's own per-feature key. Falling back to the
     * old composite keeps a row that arrives without one rather than dropping
     * it — the fallback still collides, but a colliding row is strictly better
     * than no row, and in practice ArcGIS always serves OBJECTID. */
    double oid = 0;
    char title[288], summary[288], key[160];
    if (jo_num(at, "OBJECTID", &oid))
      snprintf(key, sizeof key, "objectid:%lld", (long long)oid);
    else
      snprintf(key, sizeof key, "%s|%.5f|%.5f", licno ? licno : (who ? who : ""),
               lat, lon);
    if (has_txf)
      snprintf(title, sizeof title, "%s %s %.4f MHz%s%s",
               who ? who : "licensee n/a", svc ? svc : "", txf,
               loc ? " — " : "", loc ? loc : "");
    else
      snprintf(title, sizeof title, "%s %s site%s%s",
               who ? who : "licensee n/a", svc ? svc : "",
               loc ? " — " : "", loc ? loc : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
             licno ? "licence " : "", licno ? licno : "",
             prov ? " · " : "", prov ? prov : "",
             vintage[0] ? " · extract " : "", vintage[0] ? vintage : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.ic.gc.ca/eic/site/smt-gst.nsf/eng/h_sf01516.html";
    it.lang            = "en";
    it.record_type     = "spectrum-licence-site";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"spectrum\",\"canada\",\"cell-site\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

/* returnCountOnly — how many features the layer actually holds. -1 when the
 * server does not answer, which jo_trunc_notice() writes out as "unknown"
 * rather than letting a guess stand in for a measurement. */
static long ised_total_count(const source_ctx *ctx) {
  char url[512];
  snprintf(url, sizeof url, "%s?where=1%%3D1&returnCountOnly=true&f=json", ISED_BASE);
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return -1;
  cJSON *c = cJSON_GetObjectItem(doc, "count");
  long n = (c && cJSON_IsNumber(c)) ? (long)c->valuedouble : -1;
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* THE STRIDE MUST BE WHAT THE SERVER RETURNED, NOT WHAT WE ASKED FOR.
   *
   * This walked `resultOffset = i * PAGE_SIZE` with PAGE_SIZE 2000 — but the
   * layer's own metadata says `maxRecordCount: 1000`, and a request for 2000
   * comes back with 1000 features and `exceededTransferLimit: true`. So the
   * offsets were 0, 2000, 4000 while each page delivered only 1000 rows, and
   * records 1000-1999 and 3000-3999 were never requested at all. Not a cap —
   * holes punched through the middle of the walk, with the run reporting
   * success.
   *
   * Advancing by the number of features actually received closes the holes
   * whatever the server's cap turns out to be, and needs no local constant to
   * agree with a remote one.
   *
   * The layer holds 843,979 features (measured 2026-08-24 via returnCountOnly).
   * A full walk is 844 requests and, at the sink's throughput, hours — so this
   * stays a BOUNDED walk on a daily cadence, and the bound is DISCLOSED as a
   * record carrying the real total rather than left in a log line. Raise
   * $JO_ISED_MAX_PAGES to take more per run. */
  const char *e = getenv("JO_ISED_MAX_PAGES");
  int max_pages = (e && *e) ? atoi(e) : PAGES;
  if (max_pages < 1) max_pages = 1;

  int total = 0, pages = 0, more = 0;
  long offset = 0;
  for (int i = 0; i < max_pages; i++) {
    char url[512];
    snprintf(url, sizeof url,
             "%s?where=1%%3D1&outFields=*&returnGeometry=true&outSR=4326"
             "&resultRecordCount=%d&resultOffset=%ld&f=json",
             ISED_BASE, PAGE_SIZE, offset);
    cJSON *doc = feed_get_json(ctx->http, url, 60000);
    if (!doc) break;
    cJSON *feats = cJSON_GetObjectItem(doc, "features");
    if (!cJSON_IsArray(feats)) { cJSON_Delete(doc); break; }
    pages++;
    int got = cJSON_GetArraySize(feats);
    total += emit_features(feats, sink);
    more = cJSON_IsTrue(cJSON_GetObjectItem(doc, "exceededTransferLimit"));
    cJSON_Delete(doc);
    if (got == 0) break;          /* walked off the end — nothing left */
    offset += got;                /* ← the fix: advance by what we RECEIVED */
    if (!more) break;             /* the server says this was the last page */
  }
  if (pages == 0) {
    fprintf(stderr, "[ised-spectrum-sites] fetch failed\n");
    return -1;
  }
  if (more) {
    long avail = ised_total_count(ctx);
    jo_trunc_notice(sink, "ised-spectrum-sites", ISED_BASE, total, avail,
                    "the layer is larger than one run's page budget; a full "
                    "walk of it is ~844 requests at the server's 1000-feature "
                    "maxRecordCount",
                    "raise $JO_ISED_MAX_PAGES, or narrow the query with a "
                    "`where` clause on province or licence class");
  }
  fprintf(stderr, "[ised-spectrum-sites] emitted %d over %d page(s)%s\n",
          total, pages, more ? " (bounded — truncation notice emitted)" : "");
  return 0;
}

static const source_def tsp_ised_spectrum_sites_def = {
  .id = "ised-spectrum-sites", .collector = "telecom",
  .name = "ISED Canada terrestrial spectrum licence site data",
  .update_interval_sec = 86400, .run = run,
  .category = "telecom", .type = "dataset", .url = ISED_BASE,
  .description = "Canada's cellular and fixed-wireless spectrum licence sites as points: carrier, licence number, service class, transmit/receive frequencies, bandwidth, emission designator, structure height, transmitter power, antenna model and azimuth, with per-site coordinates.",
  .license = "ISED / Government of Canada spectrum data — Open Government Licence - Canada, keyless ArcGIS FeatureServer.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_ised_spectrum_sites_def)
