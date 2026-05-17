/* lib/feedlib.h — shared source toolkit. P4: HTTP-GET-JSON. RSS/Atom, GTFS-RT
 * (protobuf-c), HTML scrape, CSV are added here as P5 waves need them — each
 * source.c stays tiny by leaning on this. */
#ifndef JO_FEEDLIB_H
#define JO_FEEDLIB_H
#include "../third_party/cJSON.h"
#include "../core/httpclient.h"

/* GET url, parse JSON. Returns cJSON* (caller cJSON_Delete) or NULL. */
cJSON *feed_get_json(http_client *http, const char *url, int timeout_ms);

/* Like feed_get_json but with extra request headers (NULL-terminated array of
 * "Key: value" strings, or NULL). For API-key'd JSON endpoints. */
cJSON *feed_get_json_h(http_client *http, const char *url,
                       const char *const *headers, int timeout_ms);

/* GET url, return raw body (malloc'd, caller frees) or NULL. */
char *feed_get_text(http_client *http, const char *url, int timeout_ms);

/* POST `body` to url with extra headers (NULL-terminated "K: v" array, set
 * Content-Type there), parse JSON reply. cJSON* (caller cJSON_Delete) or
 * NULL. Backs the POST+JSON-body collectors (osv-dev, quake360-jp, …). */
cJSON *feed_post_json(http_client *http, const char *url, const char *body,
                      const char *const *headers, int timeout_ms);

/* Port of intelHelpers.intelHashKey: sha1( for each non-null part: part,"|" )
 * → first 20 hex chars into out (>=21 bytes). NULL parts skipped. */
void feed_hash_key(char *out21, const char *const *parts, int n);

#endif
