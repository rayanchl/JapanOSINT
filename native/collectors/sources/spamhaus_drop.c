/* collectors/cyber/sources/spamhaus_drop.c
 * Two JSONL endpoints (drop_v4 + asndrop); rows w/o cidr (drop) / asn
 * (asndrop) skipped (the leading metadata line).
 *
 * AUDIT NOTE (slice a3): every one of the ~2,100 rows used to be emitted as a
 * Point at the Tokyo Station coordinate (139.6917, 35.6895). A CIDR block and
 * an ASN have no location, and Spamhaus publishes none, so that was ~2,100
 * invented pins stacked on one spot in Tokyo — a map layer that reads as
 * "2,086 threats in Tokyo" and means nothing. Geometry is now null.
 * uid is now the natural key (cidr:<cidr> / asn:<asn>) instead of a sha1 over
 * {geometry, properties} that embedded the row's list POSITION, which made
 * every uid churn whenever Spamhaus added or dropped a line. */
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

/* A CIDR / ASN has no coordinate; emit no geometry rather than a fake one.
 * The "geometry" key is OMITTED, not set to JSON null: lib/geojson.c:235 does
 * `geom ? cJSON_PrintUnformatted(geom) : NULL`, so an explicit null is
 * serialised and persisted into the geometry column as the literal 4-byte
 * string "null" — which reads as "has geometry" to every consumer. */
static void no_geometry(cJSON *feat) { (void)feat; }

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *db = feed_get_text(ctx->http, URL_DROP, 20000);
  char *ab = feed_get_text(ctx->http, URL_ASN, 20000);
  cJSON *drop = jsonl(db); cJSON *asn = jsonl(ab);
  free(db); free(ab);

  cJSON *features = cJSON_CreateArray();
  cJSON *row;
  cJSON_ArrayForEach(row, drop) {
    cJSON *cidr = cJSON_GetObjectItem(row, "cidr");
    if (!cidr || !cJSON_IsString(cidr) || !cidr->valuestring[0]) continue;
    cJSON *catv = cJSON_GetObjectItem(row, "category");
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    no_geometry(f);
    cJSON *p = cJSON_CreateObject();
    char uid[96], title[192];
    snprintf(uid, sizeof uid, "cidr:%s", cidr->valuestring);
    snprintf(title, sizeof title, "Spamhaus DROP %s%s%s", cidr->valuestring,
             (catv && cJSON_IsString(catv) && catv->valuestring[0]) ? " — " : "",
             (catv && cJSON_IsString(catv) && catv->valuestring[0]) ? catv->valuestring : "");
    cJSON_AddStringToObject(p, "uid", uid);
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddStringToObject(p, "kind", "cidr");
    cJSON_AddItemToObject(p, "cidr", cJSON_Duplicate(cidr,1));
    cJSON_AddItemToObject(p, "sblid", or_null(row, "sblid"));
    cJSON_AddItemToObject(p, "category", or_null(row, "category"));
    cJSON_AddStringToObject(p, "link", "https://www.spamhaus.org/blocklists/do-not-route-or-peer/");
    cJSON_AddStringToObject(p, "source", "spamhaus_drop");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_ArrayForEach(row, asn) {
    cJSON *asnv = cJSON_GetObjectItem(row, "asn");
    if (!asnv || cJSON_IsNull(asnv)) continue;
    char cc[8] = {0};
    cJSON *ccv = cJSON_GetObjectItem(row, "country_code");
    if (ccv && cJSON_IsString(ccv)) {
      for (int k = 0; ccv->valuestring[k] && k < 7; k++) {
        char c = ccv->valuestring[k];
        cc[k] = (c >= 'a' && c <= 'z') ? c - 32 : c;
      }
    }
    long long asnum = cJSON_IsNumber(asnv) ? (long long)asnv->valuedouble
                    : (cJSON_IsString(asnv) ? atoll(asnv->valuestring) : 0);
    if (!asnum) continue;
    cJSON *namev = cJSON_GetObjectItem(row, "asname");
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    no_geometry(f);
    cJSON *p = cJSON_CreateObject();
    char uid[64], title[224];
    snprintf(uid, sizeof uid, "asn:%lld", asnum);
    snprintf(title, sizeof title, "Spamhaus ASN-DROP AS%lld%s%s%s%s", asnum,
             (namev && cJSON_IsString(namev) && namev->valuestring[0]) ? " — " : "",
             (namev && cJSON_IsString(namev) && namev->valuestring[0]) ? namev->valuestring : "",
             cc[0] ? " / " : "", cc[0] ? cc : "");
    cJSON_AddStringToObject(p, "uid", uid);
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddStringToObject(p, "kind", "asn");
    cJSON_AddItemToObject(p, "asn", cJSON_Duplicate(asnv,1));
    cJSON_AddItemToObject(p, "as_name", or_null(row, "asname"));
    if (cc[0]) cJSON_AddStringToObject(p, "country", cc);
    else cJSON_AddNullToObject(p, "country");
    cJSON_AddBoolToObject(p, "is_jp", strcmp(cc, "JP") == 0);
    cJSON_AddStringToObject(p, "link", "https://www.spamhaus.org/blocklists/do-not-route-or-peer/");
    cJSON_AddStringToObject(p, "source", "spamhaus_asndrop");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_Delete(drop); cJSON_Delete(asn);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[spamhaus-drop] emitted %d\n", n);
  return 0;                     /* an empty blocklist is honest, not an error */
}

static const source_def spamhaus_drop_def = {
  .id = "spamhaus-drop", .collector = "cyber",
  .name = "Spamhaus DROP", .name_ja = "Spamhaus DROP",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(spamhaus_drop_def)
