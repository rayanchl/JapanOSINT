/* collectors/sources/tsp_rocketlaunch_live.c
 * RocketLaunch.Live — the free "next launches" JSON feed, an independent
 * cross-check against Launch Library 2 (the two disagree regularly on Chinese
 * and classified launches).
 * Endpoint: https://fdo.rocketlaunch.live/json/launches/next/5  (keyless; the
 *   response carries "valid_auth":false, confirming the free tier is intended,
 *   and the free tier returns the next 5 launches)
 * Emits: launch id, COSPAR id, name, provider, vehicle, pad name and pad
 *   location name/country, mission names and description, t0, window open/close,
 *   estimated date, tags, weather summary, quicktext.
 * Geo: NONE. parse_notes: "Pad/location are names only — no lat/lon anywhere in
 *   the payload, so emit without geo" (R2).
 * Licence: RocketLaunch.Live publishes this free JSON feed for public use with
 *   attribution.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RLL_URL "https://fdo.rocketlaunch.live/json/launches/next/5"

static const char *subname(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return cJSON_IsObject(v) ? jo_sv(v, "name") : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, RLL_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[rocketlaunch-live-next] fetch failed\n");
    return -1;
  }
  cJSON *res = cJSON_GetObjectItem(doc, "result");
  if (!cJSON_IsArray(res)) {
    fprintf(stderr, "[rocketlaunch-live-next] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *l;
  cJSON_ArrayForEach(l, res) {
    const cJSON *idv = cJSON_GetObjectItem(l, "id");
    const char *name = jo_sv(l, "name");
    if (!name) continue;                          /* no title -> no row (R1) */

    const char *prov  = subname(l, "provider");
    const char *veh   = subname(l, "vehicle");
    const char *t0    = jo_sv(l, "t0");
    const cJSON *padv = cJSON_GetObjectItem(l, "pad");
    const char *pad   = cJSON_IsObject(padv) ? jo_sv(padv, "name") : NULL;
    const char *site = NULL, *country = NULL;
    if (cJSON_IsObject(padv)) {
      const cJSON *loc = cJSON_GetObjectItem(padv, "location");
      if (cJSON_IsObject(loc)) {
        site = jo_sv(loc, "name");
        country = jo_sv(loc, "country");
      }
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "name", name);
    if (cJSON_IsNumber(idv)) cJSON_AddNumberToObject(pr, "launch_id", idv->valuedouble);
    const char *s;
    if ((s = jo_sv(l, "cospar_id")))         cJSON_AddStringToObject(pr, "cospar_id", s);
    if (prov)    cJSON_AddStringToObject(pr, "provider", prov);
    if (veh)     cJSON_AddStringToObject(pr, "vehicle", veh);
    if (pad)     cJSON_AddStringToObject(pr, "pad", pad);
    if (site)    cJSON_AddStringToObject(pr, "pad_location", site);
    if (country) cJSON_AddStringToObject(pr, "pad_country", country);
    if (t0)      cJSON_AddStringToObject(pr, "t0", t0);
    if ((s = jo_sv(l, "win_open")))          cJSON_AddStringToObject(pr, "window_open", s);
    if ((s = jo_sv(l, "win_close")))         cJSON_AddStringToObject(pr, "window_close", s);
    if ((s = jo_sv(l, "est_date")))          cJSON_AddStringToObject(pr, "est_date", s);
    if ((s = jo_sv(l, "sort_date")))         cJSON_AddStringToObject(pr, "sort_date_epoch", s);
    if ((s = jo_sv(l, "mission_description")))
      cJSON_AddStringToObject(pr, "mission_description", s);
    if ((s = jo_sv(l, "weather_summary")))   cJSON_AddStringToObject(pr, "weather_summary", s);
    if ((s = jo_sv(l, "quicktext")))         cJSON_AddStringToObject(pr, "quicktext", s);

    const cJSON *missions = cJSON_GetObjectItem(l, "missions");
    if (cJSON_IsArray(missions)) {
      cJSON *ml = cJSON_CreateArray();
      const cJSON *m;
      cJSON_ArrayForEach(m, missions) {
        const char *mn = jo_sv(m, "name");
        if (mn) cJSON_AddItemToArray(ml, cJSON_CreateString(mn));
      }
      cJSON_AddItemToObject(pr, "missions", ml);
    }
    const cJSON *tags = cJSON_GetObjectItem(l, "tags");
    if (cJSON_IsArray(tags)) {
      cJSON *tl = cJSON_CreateArray();
      const cJSON *t;
      cJSON_ArrayForEach(t, tags) {
        const char *tn = jo_sv(t, "text");
        if (!tn && cJSON_IsString(t)) tn = t->valuestring;
        if (tn) cJSON_AddItemToArray(tl, cJSON_CreateString(tn));
      }
      cJSON_AddItemToObject(pr, "tags", tl);
    }
    cJSON_AddStringToObject(pr, "geo_note",
      "feed publishes pad and site names only, no coordinates");
    cJSON_AddStringToObject(pr, "source", "RocketLaunch.Live");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[320], key[32];
    if (cJSON_IsNumber(idv)) snprintf(key, sizeof key, "%.0f", idv->valuedouble);
    else                     snprintf(key, sizeof key, "%.24s", name);
    snprintf(title, sizeof title, "%s%s%s%s%s",
             prov ? prov : "", prov ? " " : "",
             veh ? veh : "", (prov || veh) ? " — " : "", name);
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
             t0 ? "T-0 " : "", t0 ? t0 : "no T-0 published",
             pad ? " · " : "", pad ? pad : "",
             site ? " · " : "", site ? site : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.lang            = "en";
    it.published_at    = t0;
    it.record_type     = "orbital-launch";
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"launch\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[rocketlaunch-live-next] emitted %d\n", n);
  return 0;
}

static const source_def tsp_rocketlaunch_live_def = {
  .id = "rocketlaunch-live-next", .collector = "satellite",
  .name = "RocketLaunch.Live — next launches feed",
  .update_interval_sec = 10800, .run = run,
  .category = "satellite", .type = "api", .url = RLL_URL,
  .description = "Independent near-term launch manifest with COSPAR id, provider, vehicle, pad, mission notes and pad weather — a cross-check against Launch Library 2.",
  .license = "RocketLaunch.Live free public JSON feed, keyless tier, attribution requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_rocketlaunch_live_def)
