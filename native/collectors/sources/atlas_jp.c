/* collectors/cyber/sources/atlas_jp.c
 * Port of server/src/collectors/atlasJp.js.
 * RIPE Atlas public API (key-free), connected probes in JP, paginated up to
 * MAX_PAGES → FeatureCollection. Honest empty on failure. No seed. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PAGES 12   /* exhaustive-ok: page-walk runaway guard */

/* p.<k> ?? null  (number or string passthrough) */
static cJSON *nn(cJSON *p, const char *k) {
  cJSON *v = cJSON_GetObjectItem(p, k);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  /* 1024, and there is no longer a separate `nexturl` staging buffer. The
   * pagination cursor arrived from RIPE as `next`, was copied into a
   * 1024-byte nexturl and then into this 768-byte url — a second, narrower
   * copy whose only effect was to be able to cut a next-page URL in half.
   * A truncated cursor is the worst shape of failure for a paged source: the
   * request still goes out, still returns 200-or-404, and every remaining page
   * is lost with the run reporting success (CLAUDE.md documents exactly this
   * for next_path). One buffer, one size, and the loop below refuses to fetch
   * a cursor that does not fit instead of fetching a mangled one. */
  char url[1024];
  /* AUDIT 2026-07-31: the v2 probe schema has no `latitude`/`longitude` or
   * `status_name` any more — coordinates live in `geometry` (GeoJSON Point,
   * [lon,lat]) and the status is an object. The old field list asked for
   * fields that no longer exist, the API silently omitted them, and the
   * lat/lon guard below then skipped EVERY probe: 309 connected JP probes
   * fetched, 0 emitted, "EMPTY" every hour. */
  snprintf(url, sizeof url,
    "https://atlas.ripe.net/api/v2/probes/?country_code=JP&status=1"
    "&page_size=100&fields=id,address_v4,asn_v4,asn_v6,prefix_v4,geometry,"
    "status,is_anchor,first_connected,description,country_code");
  int pages = 0, gotAny = 0;

  while (url[0] && pages < MAX_PAGES) {
    cJSON *data = feed_get_json(ctx->http, url, 15000);
    cJSON *results = data ? cJSON_GetObjectItem(data, "results") : NULL;
    if (!data || !cJSON_IsArray(results)) {
      if (data) cJSON_Delete(data);
      break;
    }
    gotAny = 1;
    cJSON *pr;
    cJSON_ArrayForEach(pr, results) {
      double lat = 0, lon = 0;
      int have_ll = jo_num_of(cJSON_GetObjectItem(pr, "latitude"), &lat) &&
                    jo_num_of(cJSON_GetObjectItem(pr, "longitude"), &lon);
      if (!have_ll) {                       /* current schema: GeoJSON point */
        cJSON *gm = cJSON_GetObjectItem(pr, "geometry");
        cJSON *gc = gm ? cJSON_GetObjectItem(gm, "coordinates") : NULL;
        if (cJSON_IsArray(gc) && cJSON_GetArraySize(gc) >= 2 &&
            jo_num_of(cJSON_GetArrayItem(gc, 0), &lon) &&
            jo_num_of(cJSON_GetArrayItem(gc, 1), &lat))
          have_ll = 1;
      }
      if (!have_ll) continue;
      if (lat == 0 && lon == 0) continue;

      cJSON *f = gj_point_feature(lon, lat);

      cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
      /* Stable identity: with no "id" key the geojson toolkit falls back to
       * sha1(geometry+properties), and status_since changes on every probe
       * reconnect — so each poll would mint a new row instead of updating the
       * probe's pin. */
      {
        cJSON *iv = cJSON_GetObjectItem(pr, "id");
        char sid[64];
        if (iv && cJSON_IsNumber(iv))
          snprintf(sid, sizeof sid, "atlas-probe-%.0f", iv->valuedouble);
        else if (iv && cJSON_IsString(iv))
          snprintf(sid, sizeof sid, "atlas-probe-%.40s", iv->valuestring);
        else sid[0] = 0;
        if (sid[0]) cJSON_AddStringToObject(p, "id", sid);
      }
      cJSON_AddItemToObject(p, "probe_id", nn(pr, "id"));
      cJSON_AddItemToObject(p, "asn_v4", nn(pr, "asn_v4"));
      cJSON_AddItemToObject(p, "asn_v6", nn(pr, "asn_v6"));
      cJSON_AddItemToObject(p, "prefix_v4", nn(pr, "prefix_v4"));
      cJSON_AddItemToObject(p, "address_v4", nn(pr, "address_v4"));
      /* status is now {id,name,since}; keep accepting the old flat key too */
      {
        cJSON *st = cJSON_GetObjectItem(pr, "status");
        cJSON *sn = (st && cJSON_IsObject(st)) ? cJSON_GetObjectItem(st, "name")
                                               : NULL;
        if (sn && cJSON_IsString(sn))
          cJSON_AddStringToObject(p, "status", sn->valuestring);
        else
          cJSON_AddItemToObject(p, "status", nn(pr, "status_name"));
        cJSON *ss = (st && cJSON_IsObject(st)) ? cJSON_GetObjectItem(st, "since")
                                               : NULL;
        cJSON_AddItemToObject(p, "status_since",
          (ss && cJSON_IsString(ss)) ? cJSON_Duplicate(ss, 1) : cJSON_CreateNull());
      }
      cJSON_AddItemToObject(p, "description", nn(pr, "description"));
      cJSON_AddItemToObject(p, "country_code", nn(pr, "country_code"));
      cJSON *anch = cJSON_GetObjectItem(pr, "is_anchor");
      cJSON_AddBoolToObject(p, "is_anchor",
        anch ? cJSON_IsTrue(anch) : 0);
      cJSON *fc = cJSON_GetObjectItem(pr, "first_connected");
      /* `first_connected` is an epoch integer chosen by the upstream API, so
       * nothing here bounds it: gmtime_r() RETURNS NULL when the value cannot
       * be represented (its year would overflow int), and the old code ignored
       * that and formatted an uninitialised `struct tm`. That is where
       * -Wformat-truncation's "25 to 77 bytes into a region of size 32" came
       * from — with a wild tm_year, "%04d" is not four digits. Check the return
       * and emit an honest null for a timestamp we cannot render, rather than a
       * date assembled from stack garbage. The buffer is sized for the widest
       * legitimate rendering so a real far-future probe still round-trips. */
      if (fc && cJSON_IsNumber(fc)) {
        time_t t = (time_t)fc->valuedouble;
        struct tm gt;
        char iso[80];
        if (gmtime_r(&t, &gt)) {
          snprintf(iso, sizeof iso, "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
                   gt.tm_year + 1900, gt.tm_mon + 1, gt.tm_mday,
                   gt.tm_hour, gt.tm_min, gt.tm_sec);
          cJSON_AddStringToObject(p, "first_connected", iso);
        } else {
          cJSON_AddNullToObject(p, "first_connected");
        }
      } else {
        cJSON_AddNullToObject(p, "first_connected");
      }
      cJSON *idv = cJSON_GetObjectItem(pr, "id");
      char link[96];
      if (idv && cJSON_IsNumber(idv))
        snprintf(link, sizeof link,
                 "https://atlas.ripe.net/probes/%g/", idv->valuedouble);
      else if (idv && cJSON_IsString(idv))
        snprintf(link, sizeof link,
                 "https://atlas.ripe.net/probes/%s/", idv->valuestring);
      else
        snprintf(link, sizeof link, "https://atlas.ripe.net/probes/undefined/");
      cJSON_AddStringToObject(p, "link", link);
      /* geojson pickText needs title|name|name_ja|label; without one every
       * probe row would land with a NULL title. Built from the probe's own
       * id / operator description / ASN. */
      {
        cJSON *ds = cJSON_GetObjectItem(pr, "description");
        cJSON *a4 = cJSON_GetObjectItem(pr, "asn_v4");
        cJSON *a6 = cJSON_GetObjectItem(pr, "asn_v6");
        cJSON *asn = (a4 && cJSON_IsNumber(a4)) ? a4
                   : ((a6 && cJSON_IsNumber(a6)) ? a6 : NULL);
        char t[256];
        double pid = (idv && cJSON_IsNumber(idv)) ? idv->valuedouble : 0;
        if (ds && cJSON_IsString(ds) && ds->valuestring[0] && asn)
          snprintf(t, sizeof t, "RIPE Atlas probe #%.0f — %s (AS%.0f)",
                   pid, ds->valuestring, asn->valuedouble);
        else if (ds && cJSON_IsString(ds) && ds->valuestring[0])
          snprintf(t, sizeof t, "RIPE Atlas probe #%.0f — %s",
                   pid, ds->valuestring);
        else if (asn)
          snprintf(t, sizeof t, "RIPE Atlas probe #%.0f (AS%.0f)",
                   pid, asn->valuedouble);
        else
          snprintf(t, sizeof t, "RIPE Atlas probe #%.0f", pid);
        cJSON_AddStringToObject(p, "title", t);
      }
      cJSON_AddStringToObject(p, "source", "ripe_atlas_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }

    cJSON *next = cJSON_GetObjectItem(data, "next");
    if (next && cJSON_IsString(next) && next->valuestring[0]) {
      size_t nl = strlen(next->valuestring);
      if (nl >= sizeof url) {
        /* Stop, loudly. Fetching a half URL would either 404 or silently
         * re-serve page 1 until MAX_PAGES, and either way the pages we never
         * saw would be indistinguishable from "there were no more". */
        fprintf(stderr, "[atlas-jp] next-page cursor is %zu bytes, longer than "
                        "the %zu-byte URL buffer — stopping after %d page(s) "
                        "rather than fetching a truncated URL\n",
                nl, sizeof url, pages + 1);
        url[0] = '\0';
      } else {
        snprintf(url, sizeof url, "%s", next->valuestring);
      }
    } else {
      url[0] = '\0';
    }
    pages++;
    cJSON_Delete(data);
  }

  if (!gotAny) {
    cJSON_Delete(features);
    fprintf(stderr, "[atlas-jp] unavailable\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[atlas-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def atlas_jp_def = {
  .id = "atlas-jp", .collector = "cyber",
  .name = "RIPE Atlas Japan Probes",
  .name_ja = "RIPE Atlas \xE6\x97\xA5\xE6\x9C\xAC\xE3\x83\x97\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x96",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(atlas_jp_def)
