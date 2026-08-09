/* collectors/sources/cyi_proxyscrape_free_proxies.c
 * ProxyScrape live open-proxy list, geo/ASN enriched.
 * Endpoint: https://api.proxyscrape.com/v4/free-proxy-list/get
 *           ?request=display_proxies&proxy_format=protocolipport&format=json
 *           &limit=500                                                (keyless)
 * parse_notes: "Rows in proxies[]; ip_data is an embedded ip-api.com result and
 * its lat/lon are upstream values (R2-safe) ... alive=false entries are
 * historical — filter." Both honoured: only alive proxies are emitted, and the
 * coordinates come from ip_data and are range-checked before use (a blind
 * numeric parse is what stacks rows on Null Island).
 * Emits: ip, port, protocol, anonymity, alive, uptime, first/last seen, and the
 * upstream ip_data as/country/city/hosting.
 * Licence: ProxyScrape free tier, keyless; no stated redistribution terms —
 * cite the source.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://api.proxyscrape.com/v4/free-proxy-list/get?request=display_proxies&proxy_format=protocolipport&format=json&limit=500"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 30000);
  if (!doc) { fprintf(stderr, "[proxyscrape-free-proxies] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_GetObjectItem(doc, "proxies");
  int n = 0, skipped_dead = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *ip = jo_sv(r, "ip");
    if (!ip) continue;
    const cJSON *alive = cJSON_GetObjectItem(r, "alive");
    if (cJSON_IsBool(alive) && !cJSON_IsTrue(alive)) { skipped_dead++; continue; }

    double port = 0;
    int hp = jo_numf(r, "port", &port);
    const char *proto = jo_sv(r, "protocol");
    const char *anon = jo_sv(r, "anonymity");
    const cJSON *ipd = cJSON_GetObjectItem(r, "ip_data");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", ip);
    if (hp) cJSON_AddNumberToObject(p, "port", port);
    if (proto) cJSON_AddStringToObject(p, "protocol", proto);
    if (anon)  cJSON_AddStringToObject(p, "anonymity", anon);
    cJSON_AddBoolToObject(p, "alive", 1);
    double up = 0;
    if (jo_numf(r, "uptime", &up)) cJSON_AddNumberToObject(p, "uptime_percent", up);
    const char *s;
    if ((s = jo_sv(r, "first_seen"))) cJSON_AddStringToObject(p, "first_seen", s);
    if ((s = jo_sv(r, "last_seen")))  cJSON_AddStringToObject(p, "last_seen", s);

    const char *asname = NULL, *country = NULL, *city = NULL;
    if (cJSON_IsObject(ipd)) {
      if ((asname = jo_sv(ipd, "as")))      cJSON_AddStringToObject(p, "as", asname);
      if ((country = jo_sv(ipd, "country"))) cJSON_AddStringToObject(p, "country", country);
      if ((city = jo_sv(ipd, "city")))      cJSON_AddStringToObject(p, "city", city);
      const cJSON *hosting = cJSON_GetObjectItem(ipd, "hosting");
      if (cJSON_IsBool(hosting)) cJSON_AddBoolToObject(p, "hosting", cJSON_IsTrue(hosting));
    }

    intel_item it = {0};
    double lat = 0, lon = 0;
    if (cJSON_IsObject(ipd) &&
        jo_numf(ipd, "lat", &lat) && jo_numf(ipd, "lon", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      it.has_geo = 1; it.lat = lat; it.lon = lon;
      cJSON_AddStringToObject(p, "geo_precision", "city");
    }
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[96];
    if (hp) snprintf(title, sizeof title, "%s:%.0f", ip, port);
    else    snprintf(title, sizeof title, "%s", ip);
    char summary[320];
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             proto ? proto : "proxy",
             anon ? " · " : "", anon ? anon : "",
             city ? " · " : "", city ? city : "",
             country ? ", " : "", country ? country : "");

    it.remote_key      = title;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://proxyscrape.com/free-proxy-list";
    it.lang            = "en";
    it.record_type     = "open-proxy";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"proxy\",\"anonymity\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[proxyscrape-free-proxies] emitted %d alive (%d historical skipped)\n",
          n, skipped_dead);
  return 0;
}

static const source_def cyi_proxyscrape_free_proxies_def = {
  .id = "proxyscrape-free-proxies", .collector = "cyber",
  .name = "ProxyScrape live open-proxy list (geo-enriched)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.proxyscrape.com/v4/free-proxy-list/get",
  .description = "Currently-alive open proxies with protocol, anonymity level, uptime history and upstream geo/ASN — the relay layer used to launder scanning and credential stuffing.",
  .license = "ProxyScrape free tier, keyless; no stated redistribution terms — cite the source.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_proxyscrape_free_proxies_def)
