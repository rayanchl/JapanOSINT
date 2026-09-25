/* France — national EV charging point registry (IRVE, via ODRE Opendatasoft).
 * Endpoint: https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/
 *           bornes-irve/records?limit=100&offset=<n>
 * Emits: one intel row per charge point — nom_station, nom_operateur,
 * nom_amenageur, id_station_itinerance and id_pdc_itinerance, adresse_station,
 * code_insee_commune, nbre_pdc and implantation_station, pinned on the site's
 * own published coordinates. Keyless.
 * Licence: Open Data Réseaux Énergies (ODRE), Licence Ouverte / Open Licence
 * (Etalab) v2.0.
 *
 * *** TRAP — SWAPPED COORDINATE KEYS ***
 * The `coordonneesxy` object's keys are swapped relative to their contents. The
 * probe row is Allego / La Roche-sur-Yon, which is at 46.69 N, -1.43 E, and the
 * feed publishes {"lon": 46.6902177, "lat": -1.4312283}: the field LABELLED
 * "lon" holds the LATITUDE and the field labelled "lat" holds the LONGITUDE.
 * Taking the keys at face value drops all 231,000 French charge points into the
 * Gulf of Guinea. The swap is applied below, and the result is additionally
 * rejected if it is not a valid WGS84 pair.
 *
 * Parse notes: Opendatasoft v2.1 — {total_count, results[]}; limit maxes out at
 * 100 per page and offset+limit may not exceed 10000, so this walks up to that
 * ceiling (100 pages) once a week rather than pretending to mirror the whole
 * 231k-row dataset in one run.
 */
#include "lib/jocore.h"
#include "lib/seenset.h"
#include "trn_common.inc"

#define IRVE_BASE "https://odre.opendatasoft.com/api/explore/v2.1/catalog/" \
                  "datasets/bornes-irve/records?limit=100&offset="
#define IRVE_MAX_OFFSET 9900
#define IRVE_ID "france-irve-charging-stations"

/* Read the coordinate honouring the documented key swap. Returns 1 on success. */
static int irve_coords(const cJSON *rec, double *lat, double *lon) {
  const cJSON *xy = cJSON_GetObjectItem(rec, "coordonneesxy");
  if (!xy) return 0;
  double a, b;
  if (cJSON_IsArray(xy)) {
    /* some ODS exports ship it as a plain [lon, lat] array — that form is NOT
     * key-labelled, so it follows the normal GeoJSON order. */
    const cJSON *p0 = cJSON_GetArrayItem(xy, 0), *p1 = cJSON_GetArrayItem(xy, 1);
    if (!trn_numv(p0, &a) || !trn_numv(p1, &b)) return 0;
    if (!trn_geo_ok(b, a)) return 0;
    *lat = b; *lon = a;
    return 1;
  }
  if (!trn_num(xy, "lon", &a) || !trn_num(xy, "lat", &b)) return 0;
  /* TRAP: the value under "lon" is the latitude and vice versa. */
  double la = a, lo = b;
  if (!trn_geo_ok(la, lo)) {
    /* only fall back to the literal reading when the swapped one is impossible */
    if (trn_geo_ok(b, a)) { la = b; lo = a; }
    else return 0;
  }
  *lat = la; *lon = lo;
  return 1;
}

static int emit_page(intel_sink *sink, cJSON *doc, seen_set *pdc_seen) {
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, cJSON_GetObjectItem(doc, "results")) {
    const char *pdc = jo_sv(r, "id_pdc_itinerance");
    const char *sta = jo_sv(r, "id_station_itinerance");
    const char *nom = jo_sv(r, "nom_station");
    const char *res = jo_sv(r, "datagouv_resource_id");
    if (!pdc && !sta && !nom) continue;

    cJSON *pr = cJSON_CreateObject();
    trn_put_str(pr, "station_name", nom);
    trn_put_str(pr, "operator", jo_sv(r, "nom_operateur"));
    trn_put_str(pr, "site_owner", jo_sv(r, "nom_amenageur"));
    trn_put_str(pr, "station_roaming_id", sta);
    trn_put_str(pr, "charge_point_roaming_id", pdc);
    trn_put_str(pr, "datagouv_resource_id", res);
    trn_put_str(pr, "address", jo_sv(r, "adresse_station"));
    trn_put_str(pr, "insee_commune", jo_sv(r, "code_insee_commune"));
    trn_put_str(pr, "implantation", jo_sv(r, "implantation_station"));
    trn_put_num(pr, "charge_point_count", r, "nbre_pdc");
    trn_put_num(pr, "power_kw", r, "puissance_nominale");
    trn_put_str(pr, "date_mise_en_service", jo_sv(r, "date_mise_en_service"));
    char *pj = cJSON_PrintUnformatted(pr);

    char title[320], summary[256];
    snprintf(title, sizeof title, "%s", nom ? nom : (sta ? sta : pdc));
    const char *op = jo_sv(r, "nom_operateur");
    const char *ad = jo_sv(r, "adresse_station");
    if (op && ad) snprintf(summary, sizeof summary, "%s · %s", op, ad);
    else if (ad)  snprintf(summary, sizeof summary, "%s", ad);
    else if (op)  snprintf(summary, sizeof summary, "%s", op);
    else summary[0] = 0;

    intel_item it = {0};
    /* Identity: the upstream's own charge-point roaming id — EXCEPT when that id
     * already appeared earlier in this run. The consolidated national file is a
     * union of per-submitter data.gouv resources, and one id_pdc_itinerance
     * recurs across resources as DIFFERENT rows. Measured 2026-09-15 on the
     * 10,000 records this walk reads: 10,000 byte-distinct rows but 9,463
     * distinct pdc ids; the worst id's three rows differ in operator, site
     * owner, station id and station name; id_pdc_itinerance +
     * datagouv_resource_id is unique on all 10,000. Keying on the pdc id alone
     * (emitted 10,000, stored 9,463) kept one submitter's row and silently
     * discarded the rest. A first occurrence keeps its plain id, so rows already
     * stored keep their uid; a repeat is qualified by its data.gouv resource
     * (or, lacking one, by a hash of the row's own bytes). */
    char keybuf[256];
    const char *rk = pdc ? pdc : (sta ? sta : nom);
    if (pdc && !seen_add(pdc_seen, pdc)) {
      if (res) {
        snprintf(keybuf, sizeof keybuf, "%s|%s", pdc, res);
      } else {
        char *raw = cJSON_PrintUnformatted(r);
        const char *parts[1] = { raw ? raw : "" };
        char h[21];
        feed_hash_key(h, parts, 1);
        snprintf(keybuf, sizeof keybuf, "%s|%s", pdc, h);
        free(raw);
      }
      rk = keybuf;
    }
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "fr";
    it.record_type     = "ev-charging-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"ev-charging\",\"france\"]";
    double la, lo;
    if (irve_coords(r, &la, &lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0, ok = 0, rows = 0, last_full = 0;
  long available = -1;
  seen_set pdc_seen = {0};
  for (int off = 0; off <= IRVE_MAX_OFFSET; off += 100) { /* exhaustive-ok: ODS refuses offset+limit > 10000; the unread remainder of total_count is disclosed below */
    char url[320];
    snprintf(url, sizeof url, "%s%d", IRVE_BASE, off);
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) break;
    ok = 1;
    const cJSON *tc = cJSON_GetObjectItem(doc, "total_count");
    if (cJSON_IsNumber(tc)) available = (long)tc->valuedouble;
    int got = cJSON_GetArraySize(cJSON_GetObjectItem(doc, "results"));
    rows += got;
    n += emit_page(sink, doc, &pdc_seen);
    cJSON_Delete(doc);
    last_full = (got >= 100);
    if (got < 100) break;                    /* last page */
  }
  seen_free(&pdc_seen);
  if (!ok) {
    fprintf(stderr, "[" IRVE_ID "] fetch/parse failed\n");
    return -1;
  }
  /* The records endpoint cannot page past offset 10,000, and the registry is
   * ~227,000 charge points (total_count, 2026-09-15). Say so in-band rather than
   * letting 10,000 read as the whole registry. */
  if (available > rows && (last_full || rows > 0)) {
    jo_truncation_notice_ex(sink, IRVE_ID, NULL, rows, available,
      "Opendatasoft's records API refuses offset+limit above 10,000, so the "
      "weekly walk reads the first 10,000 charge points of total_count",
      "the dataset's bulk export (/exports/json or /exports/csv on the same "
      "dataset) carries every row in one request", NULL);
  }
  fprintf(stderr, "[" IRVE_ID "] emitted %d of %d rows read (%ld declared)\n",
          n, rows, available);
  return 0;
}

static const source_def trn_france_irve_charging_stations_def = {
  .id = "france-irve-charging-stations", .collector = "transport",
  .name = "France — national EV charging point registry (IRVE)",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = "https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/bornes-irve/records",
  .description = "The official consolidated French IRVE charge-point registry — operator, site owner, address, INSEE commune, roaming id and coordinates.",
  .license = "Licence Ouverte / Open Licence (Etalab) v2.0 — Open Data Reseaux Energies.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_france_irve_charging_stations_def)
