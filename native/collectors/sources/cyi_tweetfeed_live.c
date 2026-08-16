/* collectors/sources/cyi_tweetfeed_live.c
 * TweetFeed live IOCs — indicators harvested from researcher posts, each with
 * the attributing account and campaign tags.
 * Endpoint: https://api.tweetfeed.live/v1/today                      (keyless)
 * parse_notes: "Flat array; type field selects the IOC kind so one collector
 * can emit url/domain/ip/hash rows. Attribution ('user') should be carried —
 * it is what makes a row auditable." Both are honoured: record_type follows the
 * upstream `type`, and `user` is always carried.
 * Emits: date, user, type, value, tags[]. No coordinates -> has_geo 0 (R2).
 * Licence: TweetFeed open project (MIT-style); indicators are third-party
 * researcher content.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://api.tweetfeed.live/v1/today"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 25000);
  if (!doc) { fprintf(stderr, "[tweetfeed-live] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "data");

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *value = jo_sv(r, "value");
    const char *type  = jo_sv(r, "type");
    if (!value) continue;                     /* no indicator -> no row */
    const char *user = jo_sv(r, "user");
    const char *date = jo_sv(r, "date");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "indicator", value);
    if (type) cJSON_AddStringToObject(p, "indicator_type", type);
    if (user) cJSON_AddStringToObject(p, "reported_by", user);
    if (date) cJSON_AddStringToObject(p, "reported_at", date);
    const cJSON *tags = cJSON_GetObjectItem(r, "tags");
    if (cJSON_IsArray(tags)) cJSON_AddItemToObject(p, "tags", cJSON_Duplicate(tags, 1));
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    /* record_type follows the upstream IOC kind */
    char rtype[48];
    snprintf(rtype, sizeof rtype, "ioc-%s", type ? type : "unknown");

    char summary[256];
    snprintf(summary, sizeof summary, "%s IOC%s%s",
             type ? type : "unclassified",
             user ? " reported by " : "", user ? user : "");
    char key[300];
    snprintf(key, sizeof key, "%s|%s", type ? type : "-", value);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = value;
    it.summary         = summary;
    it.link            = "https://tweetfeed.live/";
    it.lang            = "en";
    it.published_at    = date;
    it.record_type     = rtype;
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"ioc\",\"tweetfeed\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[tweetfeed-live] emitted %d\n", n);
  return 0;                       /* a quiet day is not an error (R3) */
}

static const source_def cyi_tweetfeed_live_def = {
  .id = "tweetfeed-live", .collector = "cyber",
  .name = "TweetFeed live IOCs",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "IOCs (url/domain/ip/sha256) harvested from researcher posts, each with the attributing account and campaign tags.",
  .license = "TweetFeed open project (MIT-style); indicators are third-party researcher content.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_tweetfeed_live_def)
