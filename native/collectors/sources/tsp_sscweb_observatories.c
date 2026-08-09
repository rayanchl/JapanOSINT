/* collectors/sources/tsp_sscweb_observatories.c
 * NASA Satellite Situation Center (SSCWeb) spacecraft catalogue — NASA's
 * authoritative list of spacecraft whose ephemerides SSCWeb can produce,
 * including deep-space and heliophysics missions that never appear in NORAD's
 * catalogue.
 * Endpoint: https://sscweb.gsfc.nasa.gov/WS/sscr/2/observatories  (keyless XML)
 * Emits per spacecraft: Id, Name, Resolution (seconds between trajectory
 *   points), StartTime and EndTime of the available trajectory span, and
 *   ResourceId when present.
 * Geo: NONE. parse_notes: "The Id values feed /WS/sscr/2/locations for actual
 *   positions; this endpoint itself carries no coordinates" (R2).
 *
 * parse_notes honoured: "XML with default namespace
 *   http://sscweb.gsfc.nasa.gov/schema — a namespace-agnostic tag scan is
 *   simplest": that is exactly what the block scanner below does, rather than
 *   pulling in an XML parser. "Service asks that automated clients identify
 *   themselves": a descriptive User-Agent is sent.
 * Licence: NASA GSFC SSCWeb REST service, keyless, US Government public domain.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SSC_URL "https://sscweb.gsfc.nasa.gov/WS/sscr/2/observatories"
#define MAX_ROWS 2000

/* Inner text of <tag>…</tag> inside [blk, blkend). Returns length written. */
static size_t xml_field(const char *blk, const char *blkend, const char *tag,
                        char *out, size_t cap) {
  char open[64], close[64];
  snprintf(open, sizeof open, "<%s>", tag);
  snprintf(close, sizeof close, "</%s>", tag);
  const char *o = strstr(blk, open);
  if (!o || o >= blkend) { out[0] = '\0'; return 0; }
  o += strlen(open);
  const char *c = strstr(o, close);
  if (!c || c > blkend) { out[0] = '\0'; return 0; }
  size_t len = (size_t)(c - o);
  if (len >= cap) len = cap - 1;
  memcpy(out, o, len);
  out[len] = '\0';
  return len;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = {
    "User-Agent: JapanOSINT/1.0 (OSINT collector; contact via repository)",
    "Accept: application/xml", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", SSC_URL, hdrs, NULL, 0, 30000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[sscweb-observatories] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);

  int n = 0;
  const char *p = body;
  const char *s;
  while (n < MAX_ROWS && (s = strstr(p, "<Observatory>")) != NULL) {
    const char *e = strstr(s, "</Observatory>");
    if (!e) break;
    p = e + strlen("</Observatory>");

    char id[128], name[192], res[32], st[64], et[64], rid[128];
    xml_field(s, e, "Id", id, sizeof id);
    xml_field(s, e, "Name", name, sizeof name);
    xml_field(s, e, "Resolution", res, sizeof res);
    xml_field(s, e, "StartTime", st, sizeof st);
    xml_field(s, e, "EndTime", et, sizeof et);
    xml_field(s, e, "ResourceId", rid, sizeof rid);
    if (!id[0]) continue;                        /* no identity -> no row */

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "observatory_id", id);
    if (name[0]) cJSON_AddStringToObject(pr, "name", name);
    if (res[0]) {
      char *ep = NULL;
      double d = strtod(res, &ep);
      if (ep && ep != res) cJSON_AddNumberToObject(pr, "resolution_sec", d);
      else cJSON_AddStringToObject(pr, "resolution", res);
    }
    if (st[0])  cJSON_AddStringToObject(pr, "trajectory_start", st);
    if (et[0])  cJSON_AddStringToObject(pr, "trajectory_end", et);
    if (rid[0]) cJSON_AddStringToObject(pr, "resource_id", rid);
    cJSON_AddStringToObject(pr, "locations_endpoint",
      "https://sscweb.gsfc.nasa.gov/WS/sscr/2/locations");
    cJSON_AddStringToObject(pr, "geo_note",
      "this catalogue carries no coordinates; positions come from /locations");
    cJSON_AddStringToObject(pr, "source", "NASA GSFC SSCWeb");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[224], link[256];
    snprintf(title, sizeof title, "%.180s (SSCWeb %.60s)",
             name[0] ? name : id, id);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             st[0] ? "trajectory " : "no trajectory span published",
             st[0] ? st : "", (st[0] && et[0]) ? " to " : "", et[0] ? et : "",
             res[0] ? " · resolution published" : "");
    snprintf(link, sizeof link,
             "https://sscweb.gsfc.nasa.gov/cgi-bin/Locator.cgi");

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "spacecraft-ephemeris-catalog";
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"spacecraft\",\"nasa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[sscweb-observatories] emitted %d\n", n);
  return 0;
}

static const source_def tsp_sscweb_observatories_def = {
  .id = "sscweb-observatories", .collector = "satellite",
  .name = "NASA Satellite Situation Center spacecraft catalogue",
  .update_interval_sec = 604800, .run = run,
  .category = "satellite", .type = "api", .url = SSC_URL,
  .description = "NASA's list of spacecraft whose ephemerides SSCWeb can produce, with the exact time span and resolution of available trajectory data — including deep-space missions absent from the NORAD catalogue.",
  .license = "NASA GSFC SSCWeb REST service — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_sscweb_observatories_def)
