/* collectors/sources/cyi_mlab_ndt_servers.c
 * M-Lab Locate — which NDT measurement machines this vantage point is steered to.
 * Endpoint: https://locate.measurementlab.net/v2/nearest/ndt/ndt7   (keyless)
 * Emits per machine: machine name, hostname, location.city, location.country.
 * TRAPS (parse_notes): "location is city/country strings only — NO coordinates,
 * so has_geo must stay 0 (R2)", and "the urls[] values carry short-lived access
 * tokens: never persist them as row content" — so urls[] is deliberately not
 * copied into any emitted field; only the count of offered URLs is recorded.
 * Licence: M-Lab measurement data is open (CC0); the Locate API is keyless.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://locate.measurementlab.net/v2/nearest/ndt/ndt7"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 20000);
  if (!doc) { fprintf(stderr, "[mlab-ndt-servers] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(rows) && cJSON_IsArray(doc)) rows = doc;

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *machine = jo_sv(r, "machine");
    if (!machine) continue;
    const cJSON *loc = cJSON_GetObjectItem(r, "location");
    const char *city = cJSON_IsObject(loc) ? jo_sv(loc, "city") : NULL;
    const char *cc   = cJSON_IsObject(loc) ? jo_sv(loc, "country") : NULL;
    const cJSON *urls = cJSON_GetObjectItem(r, "urls");
    int nurls = cJSON_IsObject(urls) ? cJSON_GetArraySize(urls)
              : (cJSON_IsArray(urls) ? cJSON_GetArraySize(urls) : 0);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "machine", machine);
    const char *host = jo_sv(r, "hostname");
    if (host) cJSON_AddStringToObject(p, "hostname", host);
    if (city) cJSON_AddStringToObject(p, "city", city);
    if (cc)   cJSON_AddStringToObject(p, "country", cc);
    cJSON_AddNumberToObject(p, "offered_url_count", nurls);
    /* urls[] intentionally omitted: they embed short-lived access tokens. */
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s",
             city ? city : "", (city && cc) ? ", " : "", cc ? cc : "");

    intel_item it = {0};
    it.remote_key      = machine;
    it.title           = machine;
    it.summary         = summary;
    it.link            = "https://www.measurementlab.net/";
    it.lang            = "en";
    it.record_type     = "mlab-ndt-server";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"measurement\",\"m-lab\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[mlab-ndt-servers] emitted %d\n", n);
  return 0;
}

static const source_def cyi_mlab_ndt_servers_def = {
  .id = "mlab-ndt-servers", .collector = "cyber",
  .name = "M-Lab Locate — NDT measurement server fleet",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Which M-Lab measurement machines the collector's network is steered to, with city/country — the CDN-style steering path from our own vantage point.",
  .license = "M-Lab open data (CC0 for measurement data); Locate API keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_mlab_ndt_servers_def)
