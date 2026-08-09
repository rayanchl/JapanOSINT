/* Irish Rail (Iarnród Éireann) — station inventory with coordinates.
 * Endpoint: http://api.irishrail.ie/realtime/realtime.asmx/getAllStationsXML
 * Emits: one intel row per station — StationDesc, StationCode, StationId and
 * StationAlias, pinned on the station's own StationLatitude/StationLongitude.
 * Keyless.
 * Licence: Irish Rail open realtime API, no registration.
 *
 * Parse notes: same namespaced XML shape as the live train feed (default
 * xmlns, so the scan matches on local tag names only) and HTTP-only. Coverage
 * includes the Northern Ireland cross-border stations, not just the Republic.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define IE_STATIONS_URL \
  "http://api.irishrail.ie/realtime/realtime.asmx/getAllStationsXML"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, IE_STATIONS_URL, 25000);
  if (!xml) {
    fprintf(stderr, "[irish-rail-stations] fetch failed\n");
    return -1;
  }

  int n = 0;
  const char *cur = xml;
  char *rec;
  while ((rec = trn_xml_record(&cur, "objStation")) != NULL) {
    char *desc  = trn_xml_text(rec, "StationDesc");
    char *code  = trn_xml_text(rec, "StationCode");
    if ((!desc || !desc[0]) && (!code || !code[0])) {
      free(desc); free(code); free(rec); continue;
    }
    char *sid   = trn_xml_text(rec, "StationId");
    char *alias = trn_xml_text(rec, "StationAlias");

    double la, lo;
    int geo = trn_xml_num(rec, "StationLatitude", &la) &&
              trn_xml_num(rec, "StationLongitude", &lo) && trn_geo_ok(la, lo);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Iarnrod Eireann");
    trn_put_str(pr, "station_name", desc);
    trn_put_str(pr, "station_code", code);
    trn_put_str(pr, "station_id", sid);
    trn_put_str(pr, "station_alias", alias);
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key      = (code && code[0]) ? code : desc;
    it.title           = (desc && desc[0]) ? desc : code;
    it.summary         = (code && code[0]) ? code : NULL;
    it.lang            = "en";
    it.record_type     = "rail-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"ireland\",\"infrastructure\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); cJSON_Delete(pr);
    free(desc); free(code); free(sid); free(alias); free(rec);
  }
  free(xml);
  fprintf(stderr, "[irish-rail-stations] emitted %d\n", n);
  return 0;
}

static const source_def trn_irish_rail_stations_def = {
  .id = "irish-rail-stations", .collector = "transport",
  .name = "Irish Rail — station inventory with coordinates",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api",
  .url = IE_STATIONS_URL,
  .description = "Every Irish Rail station with code, numeric id and coordinates, covering both the Republic and the Northern Ireland cross-border stations.",
  .license = "Irish Rail open realtime API, no registration.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_irish_rail_stations_def)
