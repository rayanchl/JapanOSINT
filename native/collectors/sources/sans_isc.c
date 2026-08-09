/* collectors/cyber/sources/sans_isc.c — port of
 * server/src/collectors/sansIscFeeds.js. SANS Internet Storm Center: handler
 * diary RSS (parseFeed, first 20, tags ['sans-isc','cyber','diary'], lang en)
 * + a synthetic global-infocon status row appended from the JSON API. The
 * diary feed is parsed via the shared rss_collect; the infocon row is the
 * intel-envelope status item the JS explicitly pushes (uid
 * sans-isc-feeds|infocon-current) and is ported here. The _meta/seed envelope
 * is dropped. JS SOURCE_ID is 'sans-isc-feeds'; registry id is 'sans-isc' —
 * the infocon uid is kept verbatim to JS. */
#include "../../source.h"
#include "../../lib/rss_atom.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define URL_RSS     "https://isc.sans.edu/rssfeed.xml"
#define URL_INFOCON "https://isc.sans.edu/api/infocon?json"
#define TIMEOUT_MS  12000

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0;
  int rss_n = rss_collect(ctx, sink, URL_RSS, "en",
                          "[\"sans-isc\",\"cyber\",\"diary\"]");
  if (rss_n > 0) total += rss_n;

  /* infocon: { status } or [ { status } ] || 'unknown' */
  cJSON *infocon = feed_get_json(ctx->http, URL_INFOCON, TIMEOUT_MS);
  int infocon_ok = infocon != NULL;
  if (infocon) {
    const char *status = "unknown";
    cJSON *st = NULL;
    if (cJSON_IsArray(infocon)) {
      cJSON *e0 = cJSON_GetArrayItem(infocon, 0);
      if (e0) st = cJSON_GetObjectItem(e0, "status");
    }
    if (!st || !cJSON_IsString(st)) st = cJSON_GetObjectItem(infocon, "status");
    if (st && cJSON_IsString(st) && st->valuestring && st->valuestring[0])
      status = st->valuestring;

    char iso[32];
    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S.000Z", &tmv);

    char title[128], tags[128];
    snprintf(title, sizeof title, "Infocon level: %s", status);
    snprintf(tags, sizeof tags,
             "[\"sans-isc\",\"infocon\",\"status:%s\"]", status);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "kind", "infocon");
    cJSON_AddItemToObject(props, "infocon", cJSON_Duplicate(infocon, 1));
    char *props_s = cJSON_PrintUnformatted(props);

    intel_item it = {0};
    it.uid = "sans-isc-feeds|infocon-current";
    it.title = title;
    it.summary = "SANS ISC global threat-level indicator";
    it.lang = "en";
    it.published_at = iso;
    it.record_type = "article";
    it.properties_json = props_s ? props_s : "{}";
    it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) total++;

    free(props_s);
    cJSON_Delete(props);
    cJSON_Delete(infocon);
  }

  fprintf(stderr, "[sans-isc] emitted %d\n", total);
  /* STATUS code, not a row count. A quiet diary feed with the infocon endpoint
   * up is an honest empty (0); only losing BOTH fetches is a real error. */
  return (rss_n >= 0 || infocon_ok) ? 0 : -1;
}

static const source_def sans_isc_def = {
  .id = "sans-isc", .collector = "cyber",
  .name = "SANS Internet Storm Center", .name_ja = "SANS ISC",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(sans_isc_def)
