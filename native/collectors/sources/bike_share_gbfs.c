/* collectors/transport/sources/bike_share_gbfs.c
 * Port of server/src/collectors/bikeShareGbfs.js. Two GBFS operators
 * (HelloCycling, DOCOMO Cycle Tokyo): discovery → station_information +
 * station_status → one intel row per station (slice 0..500). The
 * intelEnvelope wrapper/_meta is dropped (rule 7); only the live rows. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *OP_NAME[2]  = { "HelloCycling", "docomo-bike-share-tokyo" };
static const char *OP_DISC[2]  = {
  "https://api-public.odpt.org/api/v4/gbfs/hellocycling/gbfs.json",
  "https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-tokyo/gbfs.json",
};

static const cJSON *get(const cJSON *o, const char *k) {
  return o ? cJSON_GetObjectItem(o, k) : NULL;
}

/* JS: x ?? null  → emit value unless null/undefined */
static cJSON *nullish(const cJSON *v) {
  if (!v || cJSON_IsNull(v)) return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char iso[32];
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S", &g);
  size_t L = strlen(iso); snprintf(iso + L, sizeof iso - L, ".000Z");

  int n = 0;
  for (int oi = 0; oi < 2; oi++) {
    const char *op = OP_NAME[oi];
    cJSON *root = feed_get_json(ctx->http, OP_DISC[oi], 8000);
    cJSON *info = NULL, *status = NULL;
    if (root) {
      const cJSON *data = get(root, "data");
      const cJSON *ja = get(data, "ja");
      const cJSON *feeds = get(ja, "feeds");
      if (!cJSON_IsArray(feeds)) {
        const cJSON *en = get(data, "en");
        feeds = get(en, "feeds");
      }
      const char *si_url = NULL, *ss_url = NULL;
      if (cJSON_IsArray(feeds)) {
        const cJSON *fe;
        cJSON_ArrayForEach(fe, feeds) {
          const cJSON *nm = get(fe, "name");
          const cJSON *u  = get(fe, "url");
          if (!cJSON_IsString(nm) || !cJSON_IsString(u)) continue;
          if (!strcmp(nm->valuestring, "station_information")) si_url = u->valuestring;
          else if (!strcmp(nm->valuestring, "station_status")) ss_url = u->valuestring;
        }
      }
      if (si_url) info   = feed_get_json(ctx->http, si_url, 8000);
      if (ss_url) status = feed_get_json(ctx->http, ss_url, 8000);
    }

    const cJSON *stations = get(get(info, "data"), "stations");
    const cJSON *sstations = get(get(status, "data"), "stations");

    int i = 0;
    if (cJSON_IsArray(stations)) {
      const cJSON *s;
      cJSON_ArrayForEach(s, stations) {
        if (i++ >= 500) break;                 /* slice(0,500) */
        const cJSON *sid = get(s, "station_id");
        const char *sids = (sid && cJSON_IsString(sid)) ? sid->valuestring
                          : NULL;
        char sidbuf[64];
        if (!sids && sid && cJSON_IsNumber(sid)) {
          snprintf(sidbuf, sizeof sidbuf, "%.0f", cJSON_GetNumberValue(sid));
          sids = sidbuf;
        }

        /* statusByStation.get(station_id) */
        const cJSON *st = NULL;
        if (cJSON_IsArray(sstations) && sids) {
          const cJSON *q;
          cJSON_ArrayForEach(q, sstations) {
            const cJSON *qid = get(q, "station_id");
            if (qid && cJSON_IsString(qid) && !strcmp(qid->valuestring, sids)) {
              st = q; break;
            }
          }
        }

        const cJSON *snm = get(s, "name");
        char rk[256], title[512];
        snprintf(rk, sizeof rk, "%s|%s", op, sids ? sids : "");
        if (snm && cJSON_IsString(snm) && snm->valuestring[0])
          snprintf(title, sizeof title, "%s", snm->valuestring);
        else
          snprintf(title, sizeof title, "%s station %s", op, sids ? sids : "");

        char summary[128];
        if (st) {
          const cJSON *nb = get(st, "num_bikes_available");
          const cJSON *nd = get(st, "num_docks_available");
          char b[24], d[24];
          if (nb && cJSON_IsNumber(nb)) snprintf(b, sizeof b, "%.0f", cJSON_GetNumberValue(nb));
          else snprintf(b, sizeof b, "?");
          if (nd && cJSON_IsNumber(nd)) snprintf(d, sizeof d, "%.0f", cJSON_GetNumberValue(nd));
          else snprintf(d, sizeof d, "?");
          snprintf(summary, sizeof summary, "%s bikes / %s docks", b, d);
        } else {
          snprintf(summary, sizeof summary, "GBFS station");
        }

        /* tags: ['mobility','bike-share','gbfs', op] */
        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("mobility"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("bike-share"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("gbfs"));
        cJSON_AddItemToArray(tags, cJSON_CreateString(op));
        char *tj = cJSON_PrintUnformatted(tags);

        /* properties — EXACT JS key order */
        cJSON *pr = cJSON_CreateObject();
        cJSON_AddStringToObject(pr, "operator", op);
        cJSON_AddItemToObject(pr, "station_id",
          sids ? cJSON_CreateString(sids) : cJSON_CreateNull());
        cJSON_AddItemToObject(pr, "lat", nullish(get(s, "lat")));
        cJSON_AddItemToObject(pr, "lon", nullish(get(s, "lon")));
        {
          const cJSON *cap = get(s, "capacity");
          int truthy = cap && ((cJSON_IsNumber(cap) && cJSON_GetNumberValue(cap) != 0)
                       || (cJSON_IsString(cap) && cap->valuestring[0])
                       || (cJSON_IsBool(cap) && cJSON_IsTrue(cap)));
          cJSON_AddItemToObject(pr, "capacity",
            truthy ? cJSON_Duplicate(cap, 1) : cJSON_CreateNull());
        }
        cJSON_AddItemToObject(pr, "num_bikes_available",
          nullish(st ? get(st, "num_bikes_available") : NULL));
        cJSON_AddItemToObject(pr, "num_docks_available",
          nullish(st ? get(st, "num_docks_available") : NULL));
        cJSON_AddItemToObject(pr, "is_renting",
          nullish(st ? get(st, "is_renting") : NULL));
        cJSON_AddItemToObject(pr, "is_returning",
          nullish(st ? get(st, "is_returning") : NULL));
        char *pj = cJSON_PrintUnformatted(pr);

        intel_item it = {0};
        it.remote_key = rk;                    /* uid bike-share-gbfs|op|id */
        it.title = title;
        it.summary = summary;
        it.lang = "ja";
        it.published_at = iso;
        it.tags_json = tj;
        it.properties_json = pj;
        if (sink->emit(sink, &it) >= 0) n++;

        free(tj); free(pj);
        cJSON_Delete(tags); cJSON_Delete(pr);
      }
    }
    if (info) cJSON_Delete(info);
    if (status) cJSON_Delete(status);
    if (root) cJSON_Delete(root);
  }
  fprintf(stderr, "[bike-share-gbfs] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def bike_share_gbfs_def = {
  .id = "bike-share-gbfs", .collector = "transport",
  .name = "Bike-share GBFS (HelloCycling, DOCOMO Cycle)",
  .name_ja = "シェアサイクル GBFS",
   .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(bike_share_gbfs_def)
