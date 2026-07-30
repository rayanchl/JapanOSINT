/* collectors/satellite/sources/jaxa_earth.c
 * Port of server/src/collectors/jaxaEarth.js.
 * Keyless JAXA Earth API catalog index (first candidate endpoint that yields
 * a non-empty list wins) → one intel item per dataset/collection.
 * Non-spatial catalog (has_geo=0). Honest empty on failure. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sstr(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

/* extractList: array | .collections | .datasets | .results | object values */
static cJSON *extract_list(cJSON *data, cJSON **own) {
  *own = NULL;
  if (!data) return NULL;
  if (cJSON_IsArray(data)) return data;
  cJSON *l;
  if ((l = cJSON_GetObjectItem(data, "collections")) && cJSON_IsArray(l))
    return l;
  if ((l = cJSON_GetObjectItem(data, "datasets")) && cJSON_IsArray(l))
    return l;
  if ((l = cJSON_GetObjectItem(data, "results")) && cJSON_IsArray(l))
    return l;
  if (cJSON_IsObject(data)) {
    cJSON *vals = cJSON_CreateArray();
    int ok = 1, any = 0;
    cJSON *c;
    cJSON_ArrayForEach(c, data) {
      any = 1;
      if (!cJSON_IsObject(c) && !cJSON_IsArray(c)) { ok = 0; break; }
      cJSON_AddItemToArray(vals, cJSON_Duplicate(c, 1));
    }
    if (ok && any) { *own = vals; return vals; }
    cJSON_Delete(vals);
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *endpoints[] = {
    "https://data.earth.jaxa.jp/api/collection/v1/all/",
    "https://data.earth.jaxa.jp/api/collection/",
    "https://data.earth.jaxa.jp/api/datasets/",
  };
  cJSON *data = NULL, *list = NULL, *own = NULL;
  for (int k = 0; k < 3; k++) {
    cJSON *d = feed_get_json(ctx->http, endpoints[k], 15000);
    cJSON *o = NULL;
    cJSON *l = extract_list(d, &o);
    if (l && cJSON_GetArraySize(l) > 0) {
      data = d; list = l; own = o; break;
    }
    if (o) cJSON_Delete(o);
    if (d) cJSON_Delete(d);
  }
  if (!list || cJSON_GetArraySize(list) == 0) {
    if (own) cJSON_Delete(own);
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[jaxa-earth] unavailable (catalog empty)\n");
    return -1;
  }

  int n = 0, i = 0;
  cJSON *d;
  cJSON_ArrayForEach(d, list) {
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
    char idbuf[32];
    const char *id = sstr(d, "id");
    if (!id) id = sstr(d, "identifier");
    if (!id) id = sstr(d, "collection_id");
    if (!id) id = sstr(d, "name");
    if (!id) { snprintf(idbuf, sizeof idbuf, "dataset-%d", i); id = idbuf; }

    const char *tt = sstr(d, "title");
    if (!tt) tt = sstr(d, "name");
    if (!tt) tt = sstr(d, "id");
    char ttbuf[32];
    if (!tt) { snprintf(ttbuf, sizeof ttbuf, "JAXA Earth dataset %d", i); tt = ttbuf; }

    const char *desc = sstr(d, "description");
    if (!desc) desc = sstr(d, "abstract");

    char title[320], summary[300], bodytxt[640];
    snprintf(title, sizeof title, "JAXA Earth: %s", tt);
    if (desc) {
      snprintf(summary, sizeof summary, "%.240s", desc);
      snprintf(bodytxt, sizeof bodytxt,
        "JAXA Earth API catalog entry \"%s\" (%s). %s", tt, id, desc);
    } else {
      snprintf(summary, sizeof summary, "JAXA Earth API dataset %s.", id);
      snprintf(bodytxt, sizeof bodytxt,
        "JAXA Earth API catalog entry \"%s\" (%s).", tt, id);
    }

    const char *updated = sstr(d, "updated");
    if (!updated) updated = sstr(d, "modified");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "dataset_id", id);
    cJSON *bb = cJSON_GetObjectItem(d, "bbox");
    if (!bb) bb = cJSON_GetObjectItem(d, "extent");
    cJSON_AddItemToObject(p, "bbox",
      bb ? cJSON_Duplicate(bb, 1) : cJSON_CreateNull());
    cJSON *tmp = cJSON_GetObjectItem(d, "temporal");
    if (!tmp) tmp = cJSON_GetObjectItem(d, "time_range");
    cJSON_AddItemToObject(p, "temporal",
      tmp ? cJSON_Duplicate(tmp, 1) : cJSON_CreateNull());
    cJSON *kw = cJSON_GetObjectItem(d, "keywords");
    cJSON_AddItemToObject(p, "keywords",
      kw ? cJSON_Duplicate(kw, 1) : cJSON_CreateNull());
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("jaxa"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("earth-observation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("catalog"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = id;              /* uid jaxa-earth|<id> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = "https://data.earth.jaxa.jp/";
    it.published_at = updated;
    it.record_type = "jaxa-earth";
    it.has_geo = 0;
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  if (own) cJSON_Delete(own);
  if (data) cJSON_Delete(data);
  fprintf(stderr, "[jaxa-earth] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def jaxa_earth_def = {
  .id = "jaxa-earth", .collector = "satellite",
  .name = "JAXA Earth Observation", .name_ja = "JAXA 地球観測",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(jaxa_earth_def)
