/* collectors/sources/wikimedia_commons_jp.c
 * Wikimedia Commons — geotagged media around major Japanese cities.
 * Scheduled collector that calls the MediaWiki geosearch API (ns=6, File:)
 * for a fixed set of Japanese city coordinates. Each geotagged file becomes
 * one intel row with real coordinates. Keyless. Complements the entity-pivot
 * WIKIMEDIA_COMMONS in wikimedia_world.c (which is interval=0, on-demand). */
#include "source.h"
#include "lib/jocore.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *WC_UA =
  "User-Agent: JapanOSINT/1.0";

static const struct { double lat; double lon; const char *name; } JP_CITIES[] = {
  { 35.6762, 139.6503, "Tokyo"    },
  { 34.6937, 135.5023, "Osaka"    },
  { 35.0116, 135.7681, "Kyoto"    },
  { 33.5904, 130.4017, "Fukuoka"  },
  { 43.0621, 141.3544, "Sapporo"  },
  { 26.3344, 127.8056, "Naha"     },
  { 34.3853, 132.4553, "Hiroshima"},
  { 38.2682, 140.8694, "Sendai"   },
  { 35.1815, 136.9066, "Nagoya"   },
  { 35.3606, 138.7274, "Mt Fuji"  },
};
#define NCITIES ((int)(sizeof(JP_CITIES) / sizeof(JP_CITIES[0])))

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json", WC_UA, NULL };
  int total = 0;

  for (int i = 0; i < NCITIES; i++) {
    char url[512];
    snprintf(url, sizeof url,
      "https://commons.wikimedia.org/w/api.php?action=query"
      "&list=geosearch&gscoord=%.4f%%7C%.4f"
      "&gsradius=10000&gslimit=50&gsnamespace=6&format=json",
      JP_CITIES[i].lat, JP_CITIES[i].lon);

    char *body = jo_get(ctx, url, hdrs, "wikimedia_commons_jp");
    if (!body) continue;
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) continue;

    cJSON *query = cJSON_GetObjectItem(root, "query");
    cJSON *arr   = query ? cJSON_GetObjectItem(query, "geosearch") : NULL;
    if (cJSON_IsArray(arr)) {
      cJSON *pg;
      cJSON_ArrayForEach(pg, arr) {
        const char *title = NULL;
        cJSON *tv = cJSON_GetObjectItem(pg, "title");
        if (tv && cJSON_IsString(tv)) title = tv->valuestring;
        if (!title || !*title) continue;

        cJSON *cla = cJSON_GetObjectItem(pg, "lat");
        cJSON *clo = cJSON_GetObjectItem(pg, "lon");
        double lat = cJSON_IsNumber(cla) ? cla->valuedouble : 0;
        double lon = cJSON_IsNumber(clo) ? clo->valuedouble : 0;
        int has_geo = cJSON_IsNumber(cla) && cJSON_IsNumber(clo);

        cJSON *cdist = cJSON_GetObjectItem(pg, "dist");
        cJSON *cpid  = cJSON_GetObjectItem(pg, "pageid");

        char link[1024];
        char *enc = jo_urlencode(title);
        snprintf(link, sizeof link,
                 "https://commons.wikimedia.org/wiki/%s",
                 enc ? enc : title);
        free(enc);

        char rk[128];
        snprintf(rk, sizeof rk, "commons-jp-%d",
                 cpid && cJSON_IsNumber(cpid) ? (int)cpid->valuedouble : 0);

        cJSON *props = cJSON_CreateObject();
        cJSON_AddStringToObject(props, "region", JP_CITIES[i].name);
        if (has_geo) {
          cJSON_AddNumberToObject(props, "lat", lat);
          cJSON_AddNumberToObject(props, "lon", lon);
        }
        if (cdist && cJSON_IsNumber(cdist))
          cJSON_AddNumberToObject(props, "distance_m", cdist->valuedouble);
        if (cpid && cJSON_IsNumber(cpid))
          cJSON_AddNumberToObject(props, "pageid", cpid->valuedouble);
        char *pj = cJSON_PrintUnformatted(props);

        char tags[128];
        snprintf(tags, sizeof tags,
                 "[\"wikimedia\",\"commons\",\"japan\",\"region:%s\"]",
                 JP_CITIES[i].name);

        intel_item item = {0};
        item.remote_key      = rk;
        item.title           = title;
        item.link            = link;
        item.lang            = "ja";
        item.has_geo         = has_geo;
        item.lat             = lat;
        item.lon             = lon;
        item.record_type     = "wikimedia-photo";
        item.properties_json = pj;
        item.tags_json       = tags;
        if (sink->emit(sink, &item) >= 0) total++;

        free(pj);
        cJSON_Delete(props);
      }
    }
    cJSON_Delete(root);
  }
  fprintf(stderr, "[wikimedia-commons-jp] emitted %d\n", total);
  return 0;
}

static const source_def wikimedia_commons_jp_def = {
  .id = "wikimedia-commons-jp", .collector = "social",
  .name = "Wikimedia Commons (Japan Photos)",
  .name_ja = "ウィキメディア・コモンズ（日本の写真）",
  .update_interval_sec = 86400, .run = run,
  .category = "media", .type = "api",
  .url = "https://commons.wikimedia.org/w/api.php?action=query&list=geosearch&gsnamespace=6",
  .description = "Geotagged media files from Wikimedia Commons around major Japanese cities",
  .free_tier = 1,
};
REGISTER_SOURCE(wikimedia_commons_jp_def)
