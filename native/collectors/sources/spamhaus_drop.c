/* collectors/cyber/sources/spamhaus_drop.c
 * Port of server/src/collectors/spamhausDrop.js. Two JSONL endpoints
 * (drop_v4 + asndrop); rows w/o cidr (drop) / asn (asndrop) skipped (the
 * leading metadata line). Features at TOKYO, sha1 hash-fallback uid;
 * properties built in exact JS key order incl. idx = drop.length+i for asn. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/intel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define URL_DROP "https://www.spamhaus.org/drop/drop_v4.json"
#define URL_ASN  "https://www.spamhaus.org/drop/asndrop.json"

/* row.x || null : truthy string/number passthrough else JSON null */
static cJSON *or_null(cJSON *row, const char *k) {
  cJSON *v = cJSON_GetObjectItem(row, k);
  if (!v) return cJSON_CreateNull();
  if (cJSON_IsString(v)) return v->valuestring[0] ? cJSON_Duplicate(v,1) : cJSON_CreateNull();
  if (cJSON_IsNumber(v)) return v->valuedouble != 0 ? cJSON_Duplicate(v,1) : cJSON_CreateNull();
  if (cJSON_IsBool(v))   return cJSON_IsTrue(v) ? cJSON_Duplicate(v,1) : cJSON_CreateNull();
  return cJSON_CreateNull();
}

/* parse JSONL body → cJSON array of objects (skips blank/unparseable). */
static cJSON *jsonl(char *body) {
  cJSON *arr = cJSON_CreateArray();
  if (!body) return arr;
  for (char *p = body, *nl; p && *p; p = nl ? nl + 1 : NULL) {
    nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    while (len && (p[len-1]=='\r'||p[len-1]==' ')) len--;
    while (len && (*p==' '||*p=='\t')) { p++; len--; }
    if (!len) continue;
    char *line = malloc(len + 1); memcpy(line, p, len); line[len] = 0;
    cJSON *o = cJSON_Parse(line); free(line);
    if (o) cJSON_AddItemToArray(arr, o);
  }
  return arr;
}

static void tokyo_point(cJSON *feat) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(139.6917));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(35.6895));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(feat, "geometry", g);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *db = feed_get_text(ctx->http, URL_DROP, 20000);
  char *ab = feed_get_text(ctx->http, URL_ASN, 20000);
  cJSON *drop = jsonl(db); cJSON *asn = jsonl(ab);
  free(db); free(ab);
  int dropn = cJSON_GetArraySize(drop);

  cJSON *features = cJSON_CreateArray();
  int i = 0; cJSON *row;
  cJSON_ArrayForEach(row, drop) {
    cJSON *cidr = cJSON_GetObjectItem(row, "cidr");
    if (!cidr || !cJSON_IsString(cidr) || !cidr->valuestring[0]) { i++; continue; }
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    tokyo_point(f);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "idx", i);
    cJSON_AddStringToObject(p, "kind", "cidr");
    cJSON_AddItemToObject(p, "cidr", cJSON_Duplicate(cidr,1));
    cJSON_AddItemToObject(p, "sblid", or_null(row, "sblid"));
    cJSON_AddItemToObject(p, "category", or_null(row, "category"));
    cJSON_AddStringToObject(p, "source", "spamhaus_drop");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    i++;
  }
  i = 0;
  cJSON_ArrayForEach(row, asn) {
    cJSON *asnv = cJSON_GetObjectItem(row, "asn");
    if (!asnv || cJSON_IsNull(asnv)) { i++; continue; }
    char cc[8] = {0};
    cJSON *ccv = cJSON_GetObjectItem(row, "country_code");
    if (ccv && cJSON_IsString(ccv)) {
      for (int k = 0; ccv->valuestring[k] && k < 7; k++) {
        char c = ccv->valuestring[k];
        cc[k] = (c >= 'a' && c <= 'z') ? c - 32 : c;
      }
    }
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    tokyo_point(f);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "idx", dropn + i);
    cJSON_AddStringToObject(p, "kind", "asn");
    cJSON_AddItemToObject(p, "asn", cJSON_Duplicate(asnv,1));
    cJSON_AddItemToObject(p, "as_name", or_null(row, "asname"));
    if (cc[0]) cJSON_AddStringToObject(p, "country", cc);
    else cJSON_AddNullToObject(p, "country");
    cJSON_AddBoolToObject(p, "is_jp", strcmp(cc, "JP") == 0);
    cJSON_AddStringToObject(p, "source", "spamhaus_asndrop");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    i++;
  }
  cJSON_Delete(drop); cJSON_Delete(asn);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[spamhaus-drop] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def spamhaus_drop_def = {
  .id = "spamhaus-drop", .collector = "cyber",
  .name = "Spamhaus DROP", .name_ja = "Spamhaus DROP",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(spamhaus_drop_def)
