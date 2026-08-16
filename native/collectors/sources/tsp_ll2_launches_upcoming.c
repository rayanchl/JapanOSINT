/* collectors/sources/tsp_ll2_launches_upcoming.c
 * Launch Library 2 — upcoming and just-flown orbital launches.
 * Endpoint: https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=50&mode=list
 *   (keyless; anonymous tier throttled to roughly 15 requests/hour)
 * Emits: launch id/slug, vehicle+mission name, status, NET with precision,
 *   window_start/window_end, launch service provider, mission type, pad name,
 *   location name, landing outcome, last_updated.
 * Geo: NONE. parse_notes: "mode=list ... returns pad/location as plain strings
 *   with NO coordinates — do not invent geo here; join pad name to
 *   ll2-launch-pads if a marker is wanted." Every row is has_geo=0 (R2); the
 *   pad name is carried in properties so the join is possible downstream.
 * Licence: The Space Devs Launch Library 2 — free/open, attribution requested.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LL2_LAUNCH_URL \
  "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=50&mode=list"

/* Some LL2 fields are either a plain string or an object with .name. */
static const char *namev(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) return v->valuestring;
  if (cJSON_IsObject(v)) return jo_sv(v, "name");
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, LL2_LAUNCH_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[ll2-launches-upcoming] fetch failed (throttled?)\n");
    return -1;
  }
  cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) {
    fprintf(stderr, "[ll2-launches-upcoming] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *l;
  cJSON_ArrayForEach(l, res) {
    const char *name = jo_sv(l, "name");
    const char *id   = jo_sv(l, "id");
    if (!name) continue;                          /* no title -> no row (R1) */

    const char *status = namev(l, "status");
    const char *net    = jo_sv(l, "net");
    const char *pad    = namev(l, "pad");
    const char *loc    = namev(l, "location");
    const char *lsp    = jo_sv(l, "lsp_name");
    if (!lsp) lsp = namev(l, "launch_service_provider");

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "name", name);
    if (id)     cJSON_AddStringToObject(pr, "launch_id", id);
    if (status) cJSON_AddStringToObject(pr, "status", status);
    if (net)    cJSON_AddStringToObject(pr, "net", net);
    const char *s;
    if ((s = namev(l, "net_precision"))) cJSON_AddStringToObject(pr, "net_precision", s);
    if ((s = jo_sv(l, "window_start")))     cJSON_AddStringToObject(pr, "window_start", s);
    if ((s = jo_sv(l, "window_end")))       cJSON_AddStringToObject(pr, "window_end", s);
    if (lsp)    cJSON_AddStringToObject(pr, "launch_provider", lsp);
    if ((s = namev(l, "mission")))       cJSON_AddStringToObject(pr, "mission", s);
    if ((s = namev(l, "mission_type")))  cJSON_AddStringToObject(pr, "mission_type", s);
    if (pad)    cJSON_AddStringToObject(pr, "pad", pad);
    if (loc)    cJSON_AddStringToObject(pr, "location", loc);
    if ((s = jo_sv(l, "landing")))          cJSON_AddStringToObject(pr, "landing", s);
    const cJSON *ls = cJSON_GetObjectItem(l, "landing_success");
    if (cJSON_IsBool(ls))
      cJSON_AddBoolToObject(pr, "landing_success", cJSON_IsTrue(ls));
    if ((s = jo_sv(l, "last_updated")))     cJSON_AddStringToObject(pr, "last_updated", s);
    if ((s = jo_sv(l, "slug")))             cJSON_AddStringToObject(pr, "slug", s);
    cJSON_AddStringToObject(pr, "geo_note",
      "mode=list returns pad/location as names only; no coordinates published here");
    cJSON_AddStringToObject(pr, "source", "The Space Devs Launch Library 2");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char summary[320];
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             status ? status : "status n/a",
             net ? " · T-0 " : "", net ? net : "",
             lsp ? " · " : "", lsp ? lsp : "",
             loc ? " · " : "", loc ? loc : "");

    intel_item it = {0};
    it.remote_key      = id ? id : name;
    it.title           = name;
    it.summary         = summary;
    /* list mode publishes no per-launch permalink, so no link is asserted */
    it.lang            = "en";
    it.published_at    = net;
    it.record_type     = "orbital-launch";
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"launch\"]";
    /* R2: no coordinates in the list-mode payload. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[ll2-launches-upcoming] emitted %d\n", n);
  return 0;                       /* an empty manifest is not an error (R3) */
}

static const source_def tsp_ll2_launches_upcoming_def = {
  .id = "ll2-launches-upcoming", .collector = "satellite",
  .name = "Launch Library 2 — upcoming and just-flown launches",
  .update_interval_sec = 10800, .run = run,
  .category = "satellite", .type = "api", .url = LL2_LAUNCH_URL,
  .description = "Rolling schedule of orbital launches with provider, mission name and type, T-0 with precision, launch window, pad/location names and outcome.",
  .license = "The Space Devs Launch Library 2 — free/open, anonymous requests throttled, attribution requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_ll2_launches_upcoming_def)
