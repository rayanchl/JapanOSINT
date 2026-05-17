/* collectors/transport/sources/odpt_flight.c
 * Port of server/src/collectors/odptRealtime.js::collectOdptFlight.
 * Live ODPT v4 odpt:FlightInformationArrival + ...Departure when an ODPT
 * token is set; honest empty without one. Each flight is placed at its
 * airport's hard-coded coordinate (HND/NRT/KIX/CTS only; others skipped). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *BASES[] = {
  "https://api.odpt.org/api/v4/",
  "https://api-challenge.odpt.org/api/v4/",
};

static const char *odpt_tok(void) {
  const char *t = getenv("ODPT_TOKEN");
  if (t && *t) return t;
  t = getenv("ODPT_CONSUMER_KEY");
  if (t && *t) return t;
  t = getenv("ODPT_CHALLENGE_TOKEN");
  if (t && *t) return t;
  return NULL;
}

static cJSON *odpt_get(http_client *http, const char *rdf, const char *tok) {
  for (size_t i = 0; i < sizeof BASES / sizeof BASES[0]; i++) {
    char url[512];
    snprintf(url, sizeof url, "%s%s?acl:consumerKey=%s", BASES[i], rdf, tok);
    cJSON *d = feed_get_json(http, url, 20000);
    if (d && cJSON_IsArray(d) && cJSON_GetArraySize(d) > 0) return d;
    if (d) cJSON_Delete(d);
  }
  return NULL;
}

struct ap { const char *code; double lat, lon; };
static const struct ap AIRPORTS[] = {
  { "HND", 35.5494, 139.7798 }, { "NRT", 35.7720, 140.3929 },
  { "KIX", 34.4320, 135.2304 }, { "CTS", 42.7752, 141.6923 },
};

static const struct ap *find_ap(const char *code) {
  for (size_t i = 0; i < sizeof AIRPORTS / sizeof AIRPORTS[0]; i++)
    if (strcmp(AIRPORTS[i].code, code) == 0) return &AIRPORTS[i];
  return NULL;
}

/* String(x).replace(/^prefix/, '') */
static const char *strip_prefix(const char *s, const char *pfx) {
  size_t pl = strlen(pfx);
  if (s && strncmp(s, pfx, pl) == 0) return s + pl;
  return s ? s : "";
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = odpt_tok();
  if (!tok) {
    fprintf(stderr, "[odpt-flight] gated (no ODPT token)\n");
    return 0;
  }
  const char *RDF[] = {
    "odpt:FlightInformationArrival", "odpt:FlightInformationDeparture",
  };
  cJSON *features = cJSON_CreateArray();
  for (size_t ri = 0; ri < sizeof RDF / sizeof RDF[0]; ri++) {
    cJSON *rows = odpt_get(ctx->http, RDF[ri], tok);
    if (!rows) continue;
    cJSON *r;
    cJSON_ArrayForEach(r, rows) {
      cJSON *apv = cJSON_GetObjectItem(r, "odpt:airport");
      const char *apraw = (apv && cJSON_IsString(apv)) ? apv->valuestring : "";
      char apId[64];
      snprintf(apId, sizeof apId, "%s", strip_prefix(apraw, "odpt.Airport:"));
      const struct ap *ap = find_ap(apId);
      if (!ap) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *c = cJSON_CreateArray();
      cJSON_AddItemToArray(c, cJSON_CreateNumber(ap->lon));
      cJSON_AddItemToArray(c, cJSON_CreateNumber(ap->lat));
      cJSON_AddItemToObject(g, "coordinates", c);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON *same = cJSON_GetObjectItem(r, "owl:sameAs");
      cJSON *atid = cJSON_GetObjectItem(r, "@id");
      if (same && cJSON_IsString(same) && same->valuestring[0])
        cJSON_AddStringToObject(p, "flight_uid", same->valuestring);
      else if (atid && cJSON_IsString(atid) && atid->valuestring[0])
        cJSON_AddStringToObject(p, "flight_uid", atid->valuestring);
      else
        cJSON_AddItemToObject(p, "flight_uid", cJSON_CreateNull());

      /* (r['odpt:flightNumber'] || []).join(',') || null */
      cJSON *fn = cJSON_GetObjectItem(r, "odpt:flightNumber");
      char fnbuf[256]; fnbuf[0] = 0; size_t fl = 0;
      if (fn && cJSON_IsArray(fn)) {
        cJSON *e; int first = 1;
        cJSON_ArrayForEach(e, fn) {
          if (!cJSON_IsString(e)) continue;
          int w = snprintf(fnbuf + fl, sizeof fnbuf - fl, "%s%s",
                            first ? "" : ",", e->valuestring);
          if (w > 0 && (size_t)w < sizeof fnbuf - fl) fl += (size_t)w;
          first = 0;
        }
      }
      if (fnbuf[0]) cJSON_AddStringToObject(p, "flight_number", fnbuf);
      else cJSON_AddItemToObject(p, "flight_number", cJSON_CreateNull());

      cJSON_AddStringToObject(p, "airport", apId);

      cJSON *st = cJSON_GetObjectItem(r, "odpt:flightStatus");
      const char *sts = strip_prefix(
        (st && cJSON_IsString(st)) ? st->valuestring : "", "odpt.FlightStatus:");
      if (sts && sts[0]) cJSON_AddStringToObject(p, "status", sts);
      else cJSON_AddItemToObject(p, "status", cJSON_CreateNull());

      cJSON *sd = cJSON_GetObjectItem(r, "odpt:scheduledDepartureTime");
      cJSON *sa = cJSON_GetObjectItem(r, "odpt:scheduledArrivalTime");
      if (sd && cJSON_IsString(sd) && sd->valuestring[0])
        cJSON_AddStringToObject(p, "scheduled", sd->valuestring);
      else if (sa && cJSON_IsString(sa) && sa->valuestring[0])
        cJSON_AddStringToObject(p, "scheduled", sa->valuestring);
      else
        cJSON_AddItemToObject(p, "scheduled", cJSON_CreateNull());

      cJSON_AddStringToObject(p, "source", "odpt_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(rows);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[odpt-flight] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def odpt_flight_def = {
  .id = "odpt-flight", .collector = "transport",
  .name = "ODPT Flight Data", .name_ja = "ODPT 航空データ",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(odpt_flight_def)
