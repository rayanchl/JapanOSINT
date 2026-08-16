/* collectors/sources/tsp_pch_ixp_directory.c
 * Packet Clearing House global IXP directory — PCH's census of every internet
 * exchange point on the planet, including defunct and unregistered ones, which
 * is what you want when tracing how a country's interconnection changed.
 * Endpoint: https://www.pch.net/api/ixp/directory   (keyless)
 * Emits: exchange id, name, country, city, region, website, status, founding
 *   date, prefix count, connected ports, peak/average traffic, IPv6 average,
 *   PCH presence, last update, the four PCH ranks and the IATA city code.
 *
 * parse_notes honoured, quoted:
 *  - "ALL values are strings including lat/lon and traffic counters": every
 *    numeric read goes through strtod on the string.
 *  - "Coordinates are CITY-level (many exchanges in the same city share
 *    -34.57000/-58.42000) — real upstream values but coarse, so set
 *    properties.geo_precision='city-centroid'": done. R2 is satisfied because
 *    the coordinate IS what PCH returned; the precision claim is downgraded
 *    rather than the point being invented.
 *  - "'stat' must be filtered: Defunct and 'Not an exchange' rows are present":
 *    'Not an exchange' rows are dropped; Defunct exchanges are KEPT (they are
 *    the historical signal this source exists for) and their status is carried
 *    on the row so nothing reads as live that is not.
 *  - "'date' is YYYYMMDD with 0 for unknown": a '0' date is treated as absent.
 * Licence: PCH publishes the IXP directory as a public research resource —
 *   free to use with attribution to Packet Clearing House, no key.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCH_URL "https://www.pch.net/api/ixp/directory"

static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    return v->valuestring;
  if (v && cJSON_IsNumber(v)) return NULL;   /* handled by numeric readers */
  return NULL;
}
static int num_of(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e = NULL;
    double d = strtod(v->valuestring, &e);
    if (e && e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}
static void add_num(cJSON *dst, const cJSON *src, const char *srck, const char *k) {
  double d;
  if (num_of(src, srck, &d)) cJSON_AddNumberToObject(dst, k, d);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, PCH_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[pch-ixp-directory] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[pch-ixp-directory] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0, skipped = 0;
  cJSON *ix;
  cJSON_ArrayForEach(ix, doc) {
    const char *name = sv(ix, "name");
    const char *id   = sv(ix, "id");
    if (!name) continue;                          /* no title -> no row (R1) */
    const char *stat = sv(ix, "stat");
    if (stat && strstr(stat, "Not an exchange")) { skipped++; continue; }

    const char *ctry = sv(ix, "ctry");
    const char *city = sv(ix, "cit");
    const char *date = sv(ix, "date");
    if (date && strcmp(date, "0") == 0) date = NULL;   /* 0 = unknown */

    /* R2: PCH's own coordinates, declared as city-level precision. */
    double lat = 0, lon = 0;
    int has_geo = 0;
    if (num_of(ix, "lat", &lat) && num_of(ix, "lon", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "ixp_id", id);
    jo_add_str(pr, "name", name);
    jo_add_str(pr, "country", ctry);
    jo_add_str(pr, "city", city);
    jo_add_str(pr, "region", sv(ix, "reg"));
    jo_add_str(pr, "region_country", sv(ix, "regct"));
    jo_add_str(pr, "website", sv(ix, "url"));
    jo_add_str(pr, "status", stat);
    jo_add_str(pr, "founded_yyyymmdd", date);
    jo_add_str(pr, "iata", sv(ix, "iata"));
    jo_add_str(pr, "updated", sv(ix, "updt"));
    jo_add_str(pr, "pch_presence", sv(ix, "pch"));
    add_num(pr, ix, "prfs", "prefix_count");
    add_num(pr, ix, "prts", "connected_ports");
    add_num(pr, ix, "traf", "peak_traffic_bps");
    add_num(pr, ix, "avg",  "average_traffic_bps");
    add_num(pr, ix, "trgh", "traffic_high_bps");
    add_num(pr, ix, "ipv6_avg", "ipv6_average_bps");
    add_num(pr, ix, "tc_rank", "tc_rank");
    add_num(pr, ix, "tw_rank", "tw_rank");
    add_num(pr, ix, "pc_rank", "pc_rank");
    add_num(pr, ix, "pr_rank", "pr_rank");
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "latitude", lat);
      cJSON_AddNumberToObject(pr, "longitude", lon);
      cJSON_AddStringToObject(pr, "geo_precision", "city-centroid");
      cJSON_AddStringToObject(pr, "geo_note",
        "PCH publishes a city-level coordinate; exchanges in the same city share it");
    }
    cJSON_AddStringToObject(pr, "source", "Packet Clearing House IXP directory");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    double ports = 0, prefixes = 0;
    int has_ports = num_of(ix, "prts", &ports);
    int has_prefs = num_of(ix, "prfs", &prefixes);
    char title[288], summary[288], key[96];
    snprintf(key, sizeof key, "%s", id ? id : name);
    snprintf(title, sizeof title, "%s%s%s%s%s", name,
             city ? " — " : "", city ? city : "",
             ctry ? ", " : "", ctry ? ctry : "");
    char ptxt[48] = "", xtxt[48] = "";
    if (has_ports) snprintf(ptxt, sizeof ptxt, " · %.0f ports", ports);
    if (has_prefs) snprintf(xtxt, sizeof xtxt, " · %.0f prefixes", prefixes);
    snprintf(summary, sizeof summary, "%s%s%s",
             stat ? stat : "status n/a", ptxt, xtxt);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = sv(ix, "url");
    it.lang            = "en";
    it.record_type     = "internet-exchange-point";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"infrastructure\",\"internet\",\"ixp\",\"pch\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[pch-ixp-directory] emitted %d (%d 'not an exchange' rows skipped)\n",
          n, skipped);
  return 0;
}

static const source_def tsp_pch_ixp_directory_def = {
  .id = "pch-ixp-directory", .collector = "infrastructure",
  .name = "Packet Clearing House global IXP directory",
  .update_interval_sec = 86400, .run = run,
  .category = "infrastructure", .type = "api", .url = PCH_URL,
  .description = "PCH's census of every internet exchange point worldwide — city, coordinates, founding date, status, connected ports, prefix count and peak/average traffic, including defunct exchanges.",
  .license = "Packet Clearing House IXP directory — public research resource, free to use with attribution to PCH, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_pch_ixp_directory_def)
