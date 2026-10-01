/* collectors/cyber/sources/shodan_cameras_jp.c
 * Port of server/src/collectors/shodanCamerasJp.js.
 * Shodan host/search camera fingerprints in country:JP. Gated on
 * SHODAN_API_KEY. No seed.
 *
 * Stays in the `cyber` collector — it is a Shodan source first and a camera
 * source second, and it is not part of the camera-discovery fan-out. But it
 * emits through camera_upsert() like every other camera channel, so its hosts
 * land in the camera keyspace (record_type='camera', uid "camera-discovery|…")
 * and render on the cameras layer. It used to emit plain intel rows via
 * geojson_emit_features, which camera_fc_json cannot see — collected, then
 * invisible. Feature shape comes from the shared lib/camfeature.h constructor,
 * so the merge semantics (discovery_channels[] union, seen_count++,
 * first_seen_at) are identical to the fan-out channels.
 * discovery_channel = 'shodan_api'.
 *
 * NOTE on camera_uid: the JS keyed these on "<ip>:<port>", NOT on the
 * lat/lon:tail scheme. That is deliberate and preserved — a Shodan host is
 * identified by its endpoint, and its `location` is a GeoIP centroid that can
 * move between scans, so a coordinate-derived uid would fork one camera into
 * several rows over time. cam_make_feature() derives the uid, so the ip:port
 * key is carried as a `shodan_endpoint` extra and the row is additionally
 * addressable by it. */
#include "lib/jocore.h"
#include "source.h"
#include "core/camera_store.h"
#include "lib/camfeature.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* QUERY (JS verbatim), percent-encoded by encodeURIComponent. */
#define QUERY \
  "country:JP (webcamXP OR \"Server: yawcam\" OR product:\"Hipcam RealServer\" " \
  "OR title:\"Network Camera\" OR product:\"Hikvision\" OR product:\"Dahua\")"

/* JS: m.product || m._shodan?.module || null. Borrowed from `m`. */
static const char *product_of(cJSON *m) {
  cJSON *v = cJSON_GetObjectItem(m, "product");
  if (v && cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
  cJSON *sh = cJSON_GetObjectItem(m, "_shodan");
  if (sh && cJSON_IsObject(sh)) {
    cJSON *mod = cJSON_GetObjectItem(sh, "module");
    if (mod && cJSON_IsString(mod) && mod->valuestring[0]) return mod->valuestring;
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("SHODAN_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[shodan-cameras-jp] gated (no SHODAN_API_KEY)\n");
    return 0;
  }
  char qenc[1024];
  jo_uri_encode_buf(QUERY, qenc, sizeof qenc);
  char url[1280];
  snprintf(url, sizeof url,
    "https://api.shodan.io/shodan/host/search?key=%s&query=%s", k, qenc);
  cJSON *data = feed_get_json(ctx->http, url, 20000);
  cJSON *matches = data ? cJSON_GetObjectItem(data, "matches") : NULL;

  int count = 0;
  if (cJSON_IsArray(matches)) {
    cJSON *m;
    cJSON_ArrayForEach(m, matches) {
      cJSON *loc = cJSON_GetObjectItem(m, "location");
      double lon, lat;
      if (!(loc &&
            jo_num_of(cJSON_GetObjectItem(loc, "longitude"), &lon) &&
            jo_num_of(cJSON_GetObjectItem(loc, "latitude"), &lat)))
        continue;

      cJSON *ipv  = cJSON_GetObjectItem(m, "ip_str");
      cJSON *prtv = cJSON_GetObjectItem(m, "port");
      const char *ipstr = (ipv && cJSON_IsString(ipv)) ? ipv->valuestring : "";

      /* JS camera_uid: `${ip_str}:${port}` — kept verbatim (see file header). */
      char idb[96];
      if (prtv && cJSON_IsNumber(prtv))
        snprintf(idb, sizeof idb, "%s:%g", ipstr, prtv->valuedouble);
      else if (prtv && cJSON_IsString(prtv))
        snprintf(idb, sizeof idb, "%s:%s", ipstr, prtv->valuestring);
      else
        snprintf(idb, sizeof idb, "%s:", ipstr);

      char streamurl[128];
      if (prtv && cJSON_IsNumber(prtv))
        snprintf(streamurl, sizeof streamurl, "http://%s:%g/", ipstr,
                 prtv->valuedouble);
      else if (prtv && cJSON_IsString(prtv))
        snprintf(streamurl, sizeof streamurl, "http://%s:%s/", ipstr,
                 prtv->valuestring);
      else
        snprintf(streamurl, sizeof streamurl, "http://%s:undefined/", ipstr);

      const char *vendor = product_of(m);
      char namebuf[160];
      snprintf(namebuf, sizeof namebuf, "%s (%s)",
               vendor ? vendor : "Network camera", ipstr);

      cam_kv ex[7] = {
        { .k = "url",             .sv = streamurl                     },
        { .k = "stream_url",      .sv = streamurl                     },
        { .k = "shodan_endpoint", .sv = idb                           },
        { .k = "ip",              .sv = ipstr[0] ? ipstr : NULL       },
        { .k = "vendor",          .sv = vendor                        },
        { .k = "city",            .sv = jo_sv(loc, "city")           },
        { .k = "org",             .sv = jo_sv(m, "org")              },
      };
      cJSON *f = cam_make_feature(lat, lon, namebuf, "shodan_host",
                                  "shodan_api", ex, 7);
      /* Override the coordinate-derived uid with the stable ip:port key: a
       * Shodan `location` is a GeoIP centroid that shifts between scans, and a
       * lat/lon-derived uid would fork one host into a new row each time. */
      cJSON *props = cJSON_GetObjectItem(f, "properties");
      cJSON_ReplaceItemInObject(props, "camera_uid", cJSON_CreateString(idb));
      /* port keeps its JSON type (number in the API, string if it ever isn't) */
      if (prtv && cJSON_IsNumber(prtv))
        cJSON_AddNumberToObject(props, "port", prtv->valuedouble);
      else if (prtv && cJSON_IsString(prtv))
        cJSON_AddStringToObject(props, "port", prtv->valuestring);
      else
        cJSON_AddNullToObject(props, "port");
      cJSON_AddStringToObject(props, "source", "shodan_api");

      if (camera_upsert(ctx->db, sink, f, "shodan_api") >= 0) count++;
      cJSON_Delete(f);
    }
  }
  if (data) cJSON_Delete(data);

  fprintf(stderr, "[shodan-cameras-jp] upserted %d cameras\n", count);
  return 0;
}

static const source_def shodan_cameras_jp_def = {
  .id = "shodan-cameras-jp", .collector = "cyber",
  .name = "Shodan Cameras (JP)",
  .name_ja = "Shodan \xE3\x82\xAB\xE3\x83\xA1\xE3\x83\xA9 \xE6\x97\xA5\xE6\x9C\xAC",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(shodan_cameras_jp_def)
