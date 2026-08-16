/* collectors/sources/global_radiobrowser.c
 * OSINT service — RADIO_BROWSER. On-demand entity pivot; ctx->entity = a station
 * name (default) or a country name. Queries the community radio-browser.info
 * global directory (de1.api.radio-browser.info), keyless/LIVE. One intel_item
 * per matched station: name, country, stream url, codec, bitrate.
 *
 * Heuristic: if the (%-encoded) entity looks like a country name we hit
 * /stations/bycountry/, otherwise /stations/byname/. Both endpoints return the
 * same station-array shape, so parsing is shared. Real fetch → parse → emit;
 * fetch failure or no matches → honest empty (return 0). Nothing synthesized.
 * Header "User-Agent: JapanOSINT/1.0" is required by the API's usage policy. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one station object → one intel_item. Returns 1 if emitted. */
static int emit_station(intel_sink *sink, const cJSON *s) {
  if (!s) return 0;
  const char *name    = jo_sv(s, "name");
  const char *country = jo_sv(s, "country");
  const char *url     = jo_sv(s, "url_resolved");
  if (!url) url       = jo_sv(s, "url");
  const char *codec   = jo_sv(s, "codec");
  const char *uuid    = jo_sv(s, "stationuuid");
  const char *homepage = jo_sv(s, "homepage");
  const char *tags    = jo_sv(s, "tags");
  const char *lang    = jo_sv(s, "language");
  if (!name && !url) return 0;

  const cJSON *br = cJSON_GetObjectItem(s, "bitrate");
  int bitrate = (br && cJSON_IsNumber(br)) ? br->valueint : 0;

  cJSON *data = cJSON_CreateObject();
  if (name)     cJSON_AddStringToObject(data, "name", name);
  if (country)  cJSON_AddStringToObject(data, "country", country);
  if (url)      cJSON_AddStringToObject(data, "url", url);
  if (codec)    cJSON_AddStringToObject(data, "codec", codec);
  if (bitrate)  cJSON_AddNumberToObject(data, "bitrate", bitrate);
  if (lang)     cJSON_AddStringToObject(data, "language", lang);
  if (tags)     cJSON_AddStringToObject(data, "tags", tags);
  if (homepage) cJSON_AddStringToObject(data, "homepage", homepage);
  if (uuid)     cJSON_AddStringToObject(data, "stationuuid", uuid);
  cJSON_AddStringToObject(data, "source", "radio-browser.info");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "RADIO_BROWSER");
  cJSON_AddStringToObject(props, "source", "radio-browser.info");
  if (country) cJSON_AddStringToObject(props, "country", country);
  if (codec)   cJSON_AddStringToObject(props, "codec", codec);
  if (bitrate) cJSON_AddNumberToObject(props, "bitrate", bitrate);
  if (url)     cJSON_AddStringToObject(props, "stream_url", url);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = uuid ? uuid : (url ? url : name);
  it.title           = name ? name : url;
  it.summary         = country;
  it.body            = bj;
  it.link            = homepage ? homepage : url;
  it.record_type     = "radio-station";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"RADIO_BROWSER\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Small closed set of common country names → route to /bycountry/. Anything
 * else is treated as a station-name search. Case-insensitive on the raw entity. */
static int looks_like_country(const char *q) {
  static const char *countries[] = {
    "japan","united states","usa","united kingdom","uk","germany","france",
    "italy","spain","canada","australia","brazil","mexico","russia","china",
    "india","netherlands","sweden","norway","poland","switzerland","austria",
    "belgium","portugal","greece","turkey","argentina","chile","ireland",
    "denmark","finland","czechia","czech republic","hungary","romania",
    "south korea","korea","thailand","indonesia","philippines","vietnam",
    "new zealand","south africa","egypt","ukraine","colombia", NULL
  };
  for (int i = 0; countries[i]; i++)
    if (strcasecmp(q, countries[i]) == 0) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  const char *path = looks_like_country(q) ? "bycountry" : "byname";

  char url[1024];
  snprintf(url, sizeof url,
    "https://de1.api.radio-browser.info/json/stations/%s/%s?limit=25&hidebroken=true",
    path, enc);
  free(enc);

  const char *hdrs[] = {
    "User-Agent: JapanOSINT/1.0",
    "Accept: application/json",
    NULL
  };
  char *body = jo_get(ctx, url, hdrs, "radio_browser");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  if (cJSON_IsArray(root)) {
    cJSON *s; cJSON_ArrayForEach(s, root) emitted += emit_station(sink, s);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[radio_browser] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def radio_browser_def = {
  .id = "RADIO_BROWSER", .collector = "osint",
  .name = "Radio Browser Station Search", .name_ja = "ラジオブラウザ 局検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/radio-browser",
  .description = "radio-browser.info — global internet radio station directory by name/country (keyless).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(radio_browser_def)
