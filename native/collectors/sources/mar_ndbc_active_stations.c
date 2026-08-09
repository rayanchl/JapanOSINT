/* collectors/sources/mar_ndbc_active_stations.c
 * NOAA NDBC — registry of all currently active moored buoys, C-MAN stations,
 * DART tsunami buoys and partner platforms.
 *
 * Endpoint: https://www.ndbc.noaa.gov/activestations.xml
 * Emits (all fetched): station id, name, owner, pgm (programme), type, elev,
 *   met/currents/waterquality/dart capability flags, the snapshot 'created'
 *   timestamp from the root element, and the deployed mooring position from
 *   the station element's lat/lon attributes.
 * Keyless. Licence: NOAA / US Government work, public domain.
 *
 * parse_notes — "Coordinate is the station element's lat / lon attributes
 * (decimal degrees) — deployed mooring position. dart=\"y\" flags a
 * tsunami-detection buoy. The 'created' attribute on the root element
 * timestamps the snapshot." A station whose lat/lon attributes are missing or
 * unparseable is emitted with has_geo=0 rather than given a coordinate (R2).
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define NDBC_URL "https://www.ndbc.noaa.gov/activestations.xml"

/* Copy the value of attribute `name` out of the XML element text [el, end).
 * Returns 1 and NUL-terminates `out` on success. */
static int xml_attr(const char *el, const char *end, const char *name,
                    char *out, size_t outn) {
  char pat[48];
  snprintf(pat, sizeof pat, " %s=\"", name);
  const char *p = strstr(el, pat);
  if (!p || p >= end) return 0;
  p += strlen(pat);
  const char *q = strchr(p, '"');
  if (!q || q > end) return 0;
  size_t len = (size_t)(q - p);
  if (len >= outn) len = outn - 1;
  memcpy(out, p, len);
  out[len] = 0;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, NDBC_URL, 30000);
  if (!xml) {
    fprintf(stderr, "[ndbc-active-stations] fetch failed\n");
    return -1;
  }

  char created[64] = {0};
  const char *root = strstr(xml, "<stations");
  const char *root_end = root ? strchr(root, '>') : NULL;
  if (root && root_end)
    xml_attr(root, root_end, "created", created, sizeof created);

  int n = 0;
  const char *p = xml;
  while ((p = strstr(p, "<station ")) != NULL) {
    const char *end = strchr(p, '>');
    if (!end) break;

    char id[64] = {0}, name[192] = {0}, owner[192] = {0}, pgm[192] = {0};
    char type[64] = {0}, lats[48] = {0}, lons[48] = {0}, elev[32] = {0};
    char met[8] = {0}, cur[8] = {0}, wq[8] = {0}, dart[8] = {0};
    xml_attr(p, end, "id", id, sizeof id);
    xml_attr(p, end, "name", name, sizeof name);
    xml_attr(p, end, "owner", owner, sizeof owner);
    xml_attr(p, end, "pgm", pgm, sizeof pgm);
    xml_attr(p, end, "type", type, sizeof type);
    xml_attr(p, end, "lat", lats, sizeof lats);
    xml_attr(p, end, "lon", lons, sizeof lons);
    xml_attr(p, end, "elev", elev, sizeof elev);
    xml_attr(p, end, "met", met, sizeof met);
    xml_attr(p, end, "currents", cur, sizeof cur);
    xml_attr(p, end, "waterquality", wq, sizeof wq);
    xml_attr(p, end, "dart", dart, sizeof dart);
    p = end + 1;
    if (!id[0]) continue;

    int geo = 0; double lat = 0, lon = 0;
    if (lats[0] && lons[0]) {
      char *e1 = NULL, *e2 = NULL;
      lat = strtod(lats, &e1);
      lon = strtod(lons, &e2);
      if (e1 != lats && e2 != lons &&
          lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0)
        geo = 1;
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "station_id", id);
    if (name[0])  cJSON_AddStringToObject(pr, "name", name);
    if (owner[0]) cJSON_AddStringToObject(pr, "owner", owner);
    if (pgm[0])   cJSON_AddStringToObject(pr, "pgm", pgm);
    if (type[0])  cJSON_AddStringToObject(pr, "type", type);
    if (elev[0])  cJSON_AddStringToObject(pr, "elev_m", elev);
    if (met[0])   cJSON_AddStringToObject(pr, "met", met);
    if (cur[0])   cJSON_AddStringToObject(pr, "currents", cur);
    if (wq[0])    cJSON_AddStringToObject(pr, "waterquality", wq);
    if (dart[0])  cJSON_AddStringToObject(pr, "dart", dart);
    if (created[0]) cJSON_AddStringToObject(pr, "snapshot_created", created);
    if (geo) {
      cJSON_AddNumberToObject(pr, "lat", lat);
      cJSON_AddNumberToObject(pr, "lon", lon);
      cJSON_AddStringToObject(pr, "geo_precision", "mooring-position");
    }
    cJSON_AddStringToObject(pr, "source", "NOAA NDBC");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[260], summary[260], link[160];
    snprintf(title, sizeof title, "NDBC %s%s%s", id,
             name[0] ? " — " : "", name[0] ? name : "");
    snprintf(summary, sizeof summary, "%s%s%s%s", type[0] ? type : "station",
             owner[0] ? " · " : "", owner[0] ? owner : "",
             (dart[0] == 'y') ? " · DART tsunami buoy" : "");
    snprintf(link, sizeof link,
             "https://www.ndbc.noaa.gov/station_page.php?station=%s", id);

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "ocean-observing-station";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"buoy\",\"ndbc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  free(xml);
  fprintf(stderr, "[ndbc-active-stations] emitted %d\n", n);
  return 0;
}

static const source_def mar_ndbc_active_stations_def = {
  .id = "ndbc-active-stations", .collector = "maritime",
  .name = "NOAA NDBC — active station registry",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "dataset", .url = NDBC_URL,
  .description = "Registry of all currently active moored buoys, C-MAN stations, DART tsunami buoys and partner platforms, with owner, programme, position and what each measures.",
  .license = "NOAA / US Government work, public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_ndbc_active_stations_def)
