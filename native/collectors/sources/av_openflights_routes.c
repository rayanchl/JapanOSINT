/* collectors/sources/av_openflights_routes.c
 * OpenFlights global airline route network.
 * Endpoint: https://raw.githubusercontent.com/jpatokal/openflights/master/data/routes.dat
 * (headerless CSV, ~68k rows, parsed with lib/csv.h).
 *
 * Columns, in file order: airline_iata, airline_id, source_airport,
 * source_airport_id, dest_airport, dest_airport_id, codeshare, stops,
 * equipment. Every one of those is emitted verbatim. "\N" is the file's null
 * marker and is dropped rather than emitted as a literal.
 *
 * NO COORDINATES (R2): airports appear only as IATA/ICAO codes and OpenFlights
 * numeric ids. has_geo is always 0 and no route is placed at an invented
 * midpoint; if a route ever needs mapping it must be joined to two real
 * airport coordinates and drawn as two endpoints.
 *
 * DATA CURRENCY: this is a HISTORICAL SNAPSHOT — the OpenFlights route table
 * is no longer frequently updated and must be described as such, never as
 * current schedules. That caveat is carried on every row in
 * properties.data_currency.
 *
 * R3: fetch/parse failure → -1; a parsed file with no usable row → 0.
 *
 * Licence: OpenFlights route database is published under the Open Database
 * License (ODbL) — attribution and share-alike apply to derived databases.
 * "OpenFlights.org" must be cited wherever the data is surfaced, and is
 * carried on every row in properties.attribution.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define OF_ATTRIB "OpenFlights.org (ODbL)"

/* Cell i of a headerless row, or NULL when absent/empty/"\N". */
static const char *of_cell(const cJSON *row, int i) {
  const cJSON *c = cJSON_GetArrayItem(row, i);
  if (!c || !cJSON_IsString(c) || !c->valuestring || !c->valuestring[0])
    return NULL;
  if (strcmp(c->valuestring, "\\N") == 0) return NULL;   /* file null marker */
  return c->valuestring;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *text = feed_get_text(ctx->http,
      "https://raw.githubusercontent.com/jpatokal/openflights/master/data/routes.dat",
      60000);
  if (!text || strlen(text) < 64) {
    free(text);
    fprintf(stderr, "[openflights-routes] fetch failed\n");
    return -1;
  }
  cJSON *rows = csv_parse(text, 0);       /* headerless → array of arrays */
  free(text);
  if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) == 0) {
    cJSON_Delete(rows);
    fprintf(stderr, "[openflights-routes] no rows parsed\n");
    return -1;
  }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    if (!cJSON_IsArray(r) || cJSON_GetArraySize(r) < 6) continue;
    const char *airline = of_cell(r, 0);
    const char *src     = of_cell(r, 2);
    const char *dst     = of_cell(r, 4);
    if (!airline || !src || !dst) continue;
    const char *airline_id = of_cell(r, 1);
    const char *src_id  = of_cell(r, 3);
    const char *dst_id  = of_cell(r, 5);
    const char *share   = of_cell(r, 6);
    const char *stops   = of_cell(r, 7);
    const char *equip   = of_cell(r, 8);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "airline_iata", airline);
    if (airline_id) cJSON_AddStringToObject(data, "airline_id", airline_id);
    cJSON_AddStringToObject(data, "source_airport", src);
    if (src_id) cJSON_AddStringToObject(data, "source_airport_id", src_id);
    cJSON_AddStringToObject(data, "dest_airport", dst);
    if (dst_id) cJSON_AddStringToObject(data, "dest_airport_id", dst_id);
    cJSON_AddBoolToObject(data, "codeshare", share && share[0] == 'Y');
    if (stops) cJSON_AddStringToObject(data, "stops", stops);
    if (equip) cJSON_AddStringToObject(data, "equipment", equip);
    cJSON_AddStringToObject(data, "attribution", OF_ATTRIB);
    char *bj = cJSON_PrintUnformatted(data);

    cJSON *props = cJSON_Duplicate(data, 1);
    cJSON_AddStringToObject(props, "record_type", "airline-route");
    cJSON_AddStringToObject(props, "data_currency",
      "historical snapshot — the OpenFlights route table is no longer "
      "frequently updated; not current schedules");
    cJSON_AddStringToObject(props, "geometry_note",
      "airports appear only as codes; the route is not placed at a midpoint");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);
    cJSON_Delete(data);

    char uid[96];
    snprintf(uid, sizeof uid, "%s|%s|%s|%s", airline, src, dst,
             equip ? equip : "");
    char title[160];
    snprintf(title, sizeof title, "%s %s→%s%s%s", airline, src, dst,
             equip ? " · " : "", equip ? equip : "");

    intel_item it = {0};
    it.remote_key      = uid;
    it.title           = title;
    it.body            = bj;
    it.lang            = "en";
    it.record_type     = "airline-route";
    it.properties_json = pj;
    it.tags_json       = "[\"aviation\",\"route-network\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  cJSON_Delete(rows);
  fprintf(stderr, "[openflights-routes] emitted %d\n", n);
  return 0;
}

static const source_def av_openflights_routes_def = {
  .id = "openflights-routes", .collector = "transport",
  .name = "OpenFlights Global Airline Route Network",
  .update_interval_sec = 2592000, .run = run,
  .category = "transport", .type = "dataset",
  .url = "https://raw.githubusercontent.com/jpatokal/openflights/master/data/routes.dat",
  .description = "Airline city-pairs with operating carrier, codeshare flag, stop count and equipment type — a baseline network graph for spotting a route that should not exist, or an operator flying somewhere unusual. Historical snapshot, not current schedules.",
  .license = "OpenFlights route database under the Open Database License (ODbL) — attribution and share-alike apply to derived databases; cite OpenFlights.org. Historical snapshot, not current schedules.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_openflights_routes_def)
