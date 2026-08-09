/* collectors/sources/cyi_mullvad_relays.c
 * Mullvad VPN relay fleet — every Mullvad exit IP with hosting provider and
 * real upstream coordinates.
 * Endpoint: https://api.mullvad.net/public/relays/wireguard/v2/       (keyless)
 * parse_notes: "v2 is a dict of locations (with lat/lon) plus a relays[] array
 * referencing location codes — join them; coordinates are city-level upstream
 * values, so set geo_precision=\"city\" per R2." Exactly that: the coordinates
 * come from the upstream `locations` map, are range-checked before use, and
 * every geo row is stamped geo_precision=city. A relay whose location code is
 * missing from the map gets NO coordinates rather than a guessed one.
 * Emits: hostname, ipv4_addr_in / ipv6_addr_in, provider, owned/active flags,
 * location code, city, country.
 * Licence: Mullvad public API, no key.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://api.mullvad.net/public/relays/wireguard/v2/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 30000);
  if (!doc) { fprintf(stderr, "[mullvad-relays] fetch/parse failed\n"); return -1; }

  const cJSON *locs = cJSON_GetObjectItem(doc, "locations");
  const cJSON *relays = cJSON_GetObjectItem(doc, "relays");

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, relays) {
    const char *host = jo_sv(r, "hostname");
    if (!host) continue;
    const char *loccode = jo_sv(r, "location");
    const cJSON *loc = (loccode && cJSON_IsObject(locs))
                         ? cJSON_GetObjectItem(locs, loccode) : NULL;
    const char *city = (loc && cJSON_IsObject(loc)) ? jo_sv(loc, "city") : NULL;
    const char *country = (loc && cJSON_IsObject(loc)) ? jo_sv(loc, "country") : NULL;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "hostname", host);
    const char *s;
    if ((s = jo_sv(r, "ipv4_addr_in"))) cJSON_AddStringToObject(p, "ipv4_addr_in", s);
    if ((s = jo_sv(r, "ipv6_addr_in"))) cJSON_AddStringToObject(p, "ipv6_addr_in", s);
    if ((s = jo_sv(r, "provider")))     cJSON_AddStringToObject(p, "provider", s);
    if (loccode) cJSON_AddStringToObject(p, "location_code", loccode);
    if (city)    cJSON_AddStringToObject(p, "city", city);
    if (country) cJSON_AddStringToObject(p, "country", country);
    const cJSON *act = cJSON_GetObjectItem(r, "active");
    if (cJSON_IsBool(act)) cJSON_AddBoolToObject(p, "active", cJSON_IsTrue(act));
    const cJSON *own = cJSON_GetObjectItem(r, "owned");
    if (cJSON_IsBool(own)) cJSON_AddBoolToObject(p, "owned", cJSON_IsTrue(own));
    cJSON_AddStringToObject(p, "network", "mullvad-vpn");

    intel_item it = {0};
    double lat = 0, lon = 0;
    if (loc && cJSON_IsObject(loc) &&
        jo_numf(loc, "latitude", &lat) && jo_numf(loc, "longitude", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      it.has_geo = 1; it.lat = lat; it.lon = lon;
      cJSON_AddStringToObject(p, "geo_precision", "city");
    }
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *ipv4 = jo_sv(r, "ipv4_addr_in");
    const char *prov = jo_sv(r, "provider");
    char summary[320];
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             ipv4 ? ipv4 : "", ipv4 ? " · " : "",
             city ? city : "", (city && country) ? ", " : "",
             country ? country : "",
             prov ? " · " : "", prov ? prov : "");

    it.remote_key      = host;
    it.title           = host;
    it.summary         = summary;
    it.link            = "https://mullvad.net/en/servers";
    it.lang            = "en";
    it.record_type     = "vpn-exit-node";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"vpn\",\"mullvad\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[mullvad-relays] emitted %d\n", n);
  return 0;
}

static const source_def cyi_mullvad_relays_def = {
  .id = "mullvad-relays", .collector = "cyber",
  .name = "Mullvad VPN relay fleet (with coordinates)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Every Mullvad exit IP with hosting provider and real upstream city coordinates — turns 'this came from a VPN' into a named exit node in a known city.",
  .license = "Mullvad public API, no key.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_mullvad_relays_def)
