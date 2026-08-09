/* collectors/transport/sources/cycling_ports.c
 * Port of server/src/collectors/cyclingPorts.js.
 * Real DOCOMO Bike Share GBFS via the public ODPT GBFS gateway: per network
 * fetch gbfs.json discovery → resolve station_information feed url → emit a
 * Point per docked port. Honest empty on failure — no fabricated ports. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct net { const char *name, *discovery; };
static const struct net NETWORKS[] = {
  { "docomo-cycle-tokyo",
    "https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-tokyo/gbfs.json" },
  { "docomo-cycle-yokohama",
    "https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-yokohama/gbfs.json" },
  { "docomo-cycle-osaka",
    "https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-osaka/gbfs.json" },
};

/* root?.data?.ja?.feeds || root?.data?.en?.feeds || []  → url of
 * feeds[].name === 'station_information'. NULL if absent. */
static const char *find_station_info(cJSON *root) {
  cJSON *data = root ? cJSON_GetObjectItem(root, "data") : NULL;
  if (!data) return NULL;
  const char *langs[] = { "ja", "en" };
  for (int li = 0; li < 2; li++) {
    cJSON *lng = cJSON_GetObjectItem(data, langs[li]);
    cJSON *feeds = lng ? cJSON_GetObjectItem(lng, "feeds") : NULL;
    if (!cJSON_IsArray(feeds) || cJSON_GetArraySize(feeds) == 0) continue;
    cJSON *fe;
    cJSON_ArrayForEach(fe, feeds) {
      cJSON *nm = cJSON_GetObjectItem(fe, "name");
      cJSON *u  = cJSON_GetObjectItem(fe, "url");
      if (nm && cJSON_IsString(nm) &&
          strcmp(nm->valuestring, "station_information") == 0 &&
          u && cJSON_IsString(u))
        return u->valuestring;
    }
    return NULL;                 /* JS: first non-empty lang only (byName) */
  }
  return NULL;
}

static void s_or_null(cJSON *p, const char *outk, cJSON *s, const char *ink) {
  cJSON *v = cJSON_GetObjectItem(s, ink);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    cJSON_AddStringToObject(p, outk, v->valuestring);
  else
    cJSON_AddItemToObject(p, outk, cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();

  for (size_t ni = 0; ni < sizeof NETWORKS / sizeof NETWORKS[0]; ni++) {
    const struct net *net = &NETWORKS[ni];
    cJSON *root = feed_get_json(ctx->http, net->discovery, 8000);
    const char *si_url = find_station_info(root);
    cJSON *info = si_url ? feed_get_json(ctx->http, si_url, 8000) : NULL;
    cJSON *idata = info ? cJSON_GetObjectItem(info, "data") : NULL;
    cJSON *stations = idata ? cJSON_GetObjectItem(idata, "stations") : NULL;

    if (cJSON_IsArray(stations)) {
      cJSON *s;
      cJSON_ArrayForEach(s, stations) {
        cJSON *latv = cJSON_GetObjectItem(s, "lat");
        cJSON *lonv = cJSON_GetObjectItem(s, "lon");
        if (!latv || cJSON_IsNull(latv) || !lonv || cJSON_IsNull(lonv))
          continue;
        double lat = cJSON_IsNumber(latv) ? latv->valuedouble
                   : (cJSON_IsString(latv) ? strtod(latv->valuestring, 0) : 0);
        double lon = cJSON_IsNumber(lonv) ? lonv->valuedouble
                   : (cJSON_IsString(lonv) ? strtod(lonv->valuestring, 0) : 0);

        cJSON *f = gj_point_feature(lon, lat);

        cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
        cJSON_AddStringToObject(p, "network", net->name);
        cJSON *sidv = cJSON_GetObjectItem(s, "station_id");
        cJSON_AddItemToObject(p, "station_id",
          sidv ? cJSON_Duplicate(sidv, 1) : cJSON_CreateNull());
        s_or_null(p, "name", s, "name");
        cJSON *capv = cJSON_GetObjectItem(s, "capacity");
        cJSON_AddItemToObject(p, "capacity",
          capv ? cJSON_Duplicate(capv, 1) : cJSON_CreateNull());
        s_or_null(p, "address", s, "address");
        cJSON_AddStringToObject(p, "country", "JP");
        cJSON_AddStringToObject(p, "source", "docomo_bikeshare_gbfs");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(features, f);
      }
    }
    if (info) cJSON_Delete(info);
    if (root) cJSON_Delete(root);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[cycling-ports] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def cycling_ports_def = {
  .id = "cycling-ports", .collector = "transport",
  .name = "Bike Share Ports", .name_ja = "シェアサイクル ポート",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cycling_ports_def)
