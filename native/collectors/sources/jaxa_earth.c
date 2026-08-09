/* collectors/satellite/sources/jaxa_earth.c
 * Keyless JAXA Earth database catalog → one intel item per dataset/collection.
 * Non-spatial catalog (has_geo=0; the bbox rides in properties).
 *
 * The three /api/collection|datasets endpoints this was ported from are all
 * 404 today — JAXA moved the catalog to STAC, which is what their own
 * en/datasets/ page reads (assets createList.js: catalog.json → rel=child
 * collection.json). We walk the same graph. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JAXA_STAC "https://data.earth.jaxa.jp/stac/cog/v1/catalog.json"

/* ".../je-pds/cog/v1/<ID>/collection.json" → <ID>. Returns 0 if the href does
 * not have that shape. */
static int id_from_href(const char *href, char *out, size_t n) {
  const char *tail = strstr(href, "/collection.json");
  if (!tail) return 0;
  const char *p = tail;
  while (p > href && p[-1] != '/') p--;
  size_t len = (size_t)(tail - p);
  if (!len || len >= n) return 0;
  memcpy(out, p, len); out[len] = 0;
  return 1;
}

/* extent.temporal.interval[0][1] (dataset end) or [0][0] (start). */
static const char *temporal_end(cJSON *ext) {
  cJSON *t = ext ? cJSON_GetObjectItem(ext, "temporal") : NULL;
  cJSON *iv = t ? cJSON_GetObjectItem(t, "interval") : NULL;
  cJSON *first = iv ? cJSON_GetArrayItem(iv, 0) : NULL;  /* exhaustive-ok: STAC temporal.interval is a single [start,end] pair */
  if (!first) return NULL;
  cJSON *end = cJSON_GetArrayItem(first, 1);
  if (end && cJSON_IsString(end) && end->valuestring[0]) return end->valuestring;
  cJSON *start = cJSON_GetArrayItem(first, 0);  /* exhaustive-ok: [0]=start, [1]=end both read here */
  if (start && cJSON_IsString(start) && start->valuestring[0]) return start->valuestring;
  return NULL;
}

/* summaries.<k> is a STAC array of values; copy it through if present. */
static void copy_summary(cJSON *p, cJSON *sum, const char *k) {
  cJSON *v = sum ? cJSON_GetObjectItem(sum, k) : NULL;
  if (v && !cJSON_IsNull(v)) cJSON_AddItemToObject(p, k, cJSON_Duplicate(v, 1));
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *cat = feed_get_json(ctx->http, JAXA_STAC, 15000);
  cJSON *links = cat ? cJSON_GetObjectItem(cat, "links") : NULL;
  if (!links || !cJSON_IsArray(links)) {
    if (cat) cJSON_Delete(cat);
    fprintf(stderr, "[jaxa-earth] catalog unreachable (%s)\n", JAXA_STAC);
    return -1;                       /* fetch really failed: an error */
  }

  int n = 0, i = 0, seen = 0;
  cJSON *l;
  cJSON_ArrayForEach(l, links) {
    /* (cap removed: every STAC child collection is walked —
     * docs/SOURCE_EXHAUSTIVENESS.md; i still bounds the per-run fetch log) */
    const char *rel = jo_sv(l, "rel");
    const char *href = jo_sv(l, "href");
    if (!rel || strcmp(rel, "child") != 0 || !href) continue;
    seen++;
    char id[192];
    if (!id_from_href(href, id, sizeof id)) continue;

    /* Each child is a STAC Collection with the real metadata: title,
     * description, keywords, spatial/temporal extent, license, providers and
     * the band assets. The old code only ever saw a flat list and therefore
     * dropped all of it. */
    cJSON *d = feed_get_json(ctx->http, href, 15000);
    if (!d) { fprintf(stderr, "[jaxa-earth] %s unreachable\n", id); continue; }
    i++;

    const char *tt = jo_sv(d, "title");
    if (!tt) tt = id;
    const char *desc = jo_sv(d, "description");

    char title[320], summary[300], bodytxt[900];
    snprintf(title, sizeof title, "JAXA Earth: %s", tt);
    if (desc) {
      snprintf(summary, sizeof summary, "%.240s", desc);
      snprintf(bodytxt, sizeof bodytxt,
        "JAXA Earth database collection \"%s\" (%s). %s", tt, id, desc);
    } else {
      snprintf(summary, sizeof summary, "JAXA Earth dataset %s.", id);
      snprintf(bodytxt, sizeof bodytxt,
        "JAXA Earth database collection \"%s\" (%s).", tt, id);
    }

    cJSON *ext = cJSON_GetObjectItem(d, "extent");
    const char *updated = temporal_end(ext);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "dataset_id", id);
    cJSON_AddStringToObject(p, "collection_url", href);
    cJSON *sp = ext ? cJSON_GetObjectItem(ext, "spatial") : NULL;
    cJSON *bb = sp ? cJSON_GetObjectItem(sp, "bbox") : NULL;
    if (bb) cJSON_AddItemToObject(p, "bbox", cJSON_Duplicate(bb, 1));
    cJSON *tm = ext ? cJSON_GetObjectItem(ext, "temporal") : NULL;
    if (tm) cJSON_AddItemToObject(p, "temporal", cJSON_Duplicate(tm, 1));
    cJSON *kw = cJSON_GetObjectItem(d, "keywords");
    if (kw) cJSON_AddItemToObject(p, "keywords", cJSON_Duplicate(kw, 1));
    const char *lic = jo_sv(d, "license");
    if (lic) cJSON_AddStringToObject(p, "license", lic);
    cJSON *pv = cJSON_GetObjectItem(d, "providers");
    if (pv) cJSON_AddItemToObject(p, "providers", cJSON_Duplicate(pv, 1));
    cJSON *sum = cJSON_GetObjectItem(d, "summaries");
    copy_summary(p, sum, "platform");
    copy_summary(p, sum, "instrument");
    /* band list: assets keyed by band name, each with a human title */
    cJSON *as = cJSON_GetObjectItem(d, "assets");
    if (as && cJSON_IsObject(as)) {
      cJSON *bands = cJSON_CreateArray();
      cJSON *a;
      cJSON_ArrayForEach(a, as)
        if (a->string) cJSON_AddItemToArray(bands, cJSON_CreateString(a->string));
      cJSON_AddItemToObject(p, "bands", bands);
    }
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("jaxa"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("earth-observation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("catalog"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* deep link into JAXA's own dataset browser (en/datasets/script.js routes
     * on #/id/<dataset_id>) */
    char link[320];
    snprintf(link, sizeof link,
             "https://data.earth.jaxa.jp/en/datasets/#/id/%s", id);

    intel_item it = {0};
    it.remote_key = id;              /* uid jaxa-earth|<id> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = link;
    it.published_at = updated;
    it.record_type = "jaxa-earth";
    it.has_geo = 0;                  /* catalog entry, not a place */
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags); cJSON_Delete(d);
  }
  cJSON_Delete(cat);
  fprintf(stderr, "[jaxa-earth] emitted %d of %d catalog children\n", n, seen);
  /* rc is a status: the catalog answered, so this is a successful run even if
   * every child fetch happened to fail. */
  return 0;
}

static const source_def jaxa_earth_def = {
  .id = "jaxa-earth", .collector = "satellite",
  .name = "JAXA Earth Observation", .name_ja = "JAXA 地球観測",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(jaxa_earth_def)
