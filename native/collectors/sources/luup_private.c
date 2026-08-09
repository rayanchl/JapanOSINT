/* collectors/transport/sources/luup_private.c
 * Port of server/src/collectors/luupPorts.js (intelEnvelope, env-gated).
 * env LUUP_MOBILE_TOKEN. No token → ONE portal-pointer intel row (fetchHead
 * probe of luup.sc). With token → per-bbox /v1/ports read → one intel row
 * per port. The intel envelope IS the product; curated _meta dropped. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define HOMEPAGE  "https://luup.sc/"
#define PORTS_API "https://api.luup.sc/v1/ports"

struct bbox { const char *city; double minLat, maxLat, minLon, maxLon; };
static const struct bbox BBOXES[] = {
  { "tokyo",    35.50, 35.80, 139.55, 139.90 },
  { "osaka",    34.60, 34.78, 135.40, 135.60 },
  { "kyoto",    34.95, 35.07, 135.69, 135.82 },
  { "yokohama", 35.40, 35.52, 139.55, 139.70 },
};

/* p.lat ?? p.latitude  (nullish: number/string accepted, NaN-rejected) */
static double num_of(cJSON *o, const char *a, const char *b, const char *c) {
  const char *keys[3] = { a, b, c };
  for (int i = 0; i < 3; i++) {
    if (!keys[i]) continue;
    cJSON *v = cJSON_GetObjectItem(o, keys[i]);
    if (!v || cJSON_IsNull(v)) continue;            /* ?? skips null/undef */
    if (cJSON_IsNumber(v)) return v->valuedouble;
    if (cJSON_IsString(v) && v->valuestring) return atof(v->valuestring);
  }
  return NAN;
}

/* p.x ?? p.y → string (for id / vehicle_count etc.); NULL if all nullish. */
static cJSON *first_present(cJSON *o, const char *a, const char *b) {
  cJSON *v = cJSON_GetObjectItem(o, a);
  if (v && !cJSON_IsNull(v)) return v;
  if (b) { v = cJSON_GetObjectItem(o, b); if (v && !cJSON_IsNull(v)) return v; }
  return NULL;
}

static int emit_row(intel_sink *sink, intel_item *it) {
  return sink->emit(sink, it);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *token = getenv("LUUP_MOBILE_TOKEN");
  int has_key = token && *token;
  int portal_live = probe_head(ctx->http, HOMEPAGE);
  char now[32]; probe_iso_now(now, sizeof now);

  if (!has_key) {
    /* Gated: log it and emit NOTHING. This used to publish a "portal-pointer"
     * row whose entire content was a link plus "Set LUUP_MOBILE_TOKEN to
     * enable" — a row that reads as a result in every list and carries no
     * measurement. An honest empty is the correct output for a source that
     * cannot run; 0 (not -1) so the scheduler doesn't quarantine it. */
    (void)portal_live; (void)now;
    fprintf(stderr, "[luup-private] gated (no LUUP_MOBILE_TOKEN) — "
                    "per-port reads need a bearer from LUUP mobile sign-up\n");
    return 0;
  }

  char authh[512];
  snprintf(authh, sizeof authh, "Authorization: Bearer %s", token);
  const char *hdrs[] = {
    authh,
    "User-Agent: JapanOSINT/1.0 (defensive-infra-research; read-only)",
    "Accept: application/json",
    NULL,
  };

  int n = 0;
  for (size_t bi = 0; bi < sizeof(BBOXES) / sizeof(BBOXES[0]); bi++) {
    const struct bbox *bb = &BBOXES[bi];
    char url[320];
    snprintf(url, sizeof url,
      "%s?lat_min=%g&lat_max=%g&lng_min=%g&lng_max=%g",
      PORTS_API, bb->minLat, bb->maxLat, bb->minLon, bb->maxLon);
    cJSON *j = feed_get_json_h(ctx->http, url, hdrs, 10000);
    if (!j) continue;

    /* ports = Array.isArray(j) ? j : (j.ports || j.data || []) */
    cJSON *ports = NULL;
    if (cJSON_IsArray(j)) ports = j;
    else {
      ports = cJSON_GetObjectItem(j, "ports");
      if (!cJSON_IsArray(ports)) ports = cJSON_GetObjectItem(j, "data");
    }
    if (!cJSON_IsArray(ports)) { cJSON_Delete(j); continue; }

    int idx = 0;
    cJSON *pt;
    cJSON_ArrayForEach(pt, ports) {
      if (idx++ >= 500) break;                      /* slice(0,500) */
      double lat = num_of(pt, "lat", "latitude", NULL);
      double lon = num_of(pt, "lng", "longitude", "lon");
      if (!isfinite(lat) || !isfinite(lon)) continue;

      cJSON *idv = first_present(pt, "id", "port_id");
      cJSON *nmv = cJSON_GetObjectItem(pt, "name");
      const char *name = (nmv && cJSON_IsString(nmv) && nmv->valuestring &&
                          nmv->valuestring[0]) ? nmv->valuestring : NULL;
      cJSON *vcv = first_present(pt, "vehicle_count", "available");

      /* uid = intelUid(SOURCE_ID, `${city}|${id ?? port_id ?? `lat,lon`}`) */
      char idstr[64];
      if (idv) {
        if (cJSON_IsString(idv) && idv->valuestring)
          snprintf(idstr, sizeof idstr, "%s", idv->valuestring);
        else if (cJSON_IsNumber(idv))
          snprintf(idstr, sizeof idstr, "%g", idv->valuedouble);
        else snprintf(idstr, sizeof idstr, "%g,%g", lat, lon);
      } else {
        snprintf(idstr, sizeof idstr, "%g,%g", lat, lon);
      }
      char rk[96];
      snprintf(rk, sizeof rk, "%s|%s", bb->city, idstr);

      /* title = p.name || `LUUP port ${p.id ?? ''}` */
      char title[128];
      if (name) snprintf(title, sizeof title, "%s", name);
      else {
        char idt[48] = "";
        if (idv && cJSON_IsString(idv) && idv->valuestring)
          snprintf(idt, sizeof idt, "%s", idv->valuestring);
        else if (idv && cJSON_IsNumber(idv))
          snprintf(idt, sizeof idt, "%g", idv->valuedouble);
        snprintf(title, sizeof title, "LUUP port %s", idt);
      }

      /* summary = `${vc ?? '?'} vehicles @ ${name||city}` */
      char vcs[32] = "?";
      if (vcv) {
        if (cJSON_IsNumber(vcv)) snprintf(vcs, sizeof vcs, "%g", vcv->valuedouble);
        else if (cJSON_IsString(vcv) && vcv->valuestring)
          snprintf(vcs, sizeof vcs, "%s", vcv->valuestring);
      }
      char summary[192];
      snprintf(summary, sizeof summary, "%s vehicles @ %s",
               vcs, name ? name : bb->city);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("mobility"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("luup"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("scooter"));
      cJSON_AddItemToArray(tags, cJSON_CreateString(bb->city));
      char *tj = cJSON_PrintUnformatted(tags);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      cJSON_AddStringToObject(p, "operator", "Luup, Inc.");
      cJSON_AddStringToObject(p, "city", bb->city);
      cJSON_AddItemToObject(p, "port_id",
        idv ? cJSON_Duplicate(idv, 1) : cJSON_CreateNull());
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddItemToObject(p, "vehicle_count",
        vcv ? cJSON_Duplicate(vcv, 1) : cJSON_CreateNull());
      cJSON *cap = cJSON_GetObjectItem(pt, "capacity");
      cJSON_AddItemToObject(p, "capacity",
        (cap && !cJSON_IsNull(cap)) ? cJSON_Duplicate(cap, 1)
                                    : cJSON_CreateNull());
      cJSON *stt = cJSON_GetObjectItem(pt, "status");
      cJSON_AddItemToObject(p, "status",
        (stt && !cJSON_IsNull(stt)) ? cJSON_Duplicate(stt, 1)
                                    : cJSON_CreateNull());
      char *pj = cJSON_PrintUnformatted(p);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = title;
      it.summary        = summary;
      it.lang           = "ja";
      it.published_at   = now;
      it.tags_json      = tj;
      it.properties_json = pj;
      if (emit_row(sink, &it) >= 0) n++;

      free(tj); free(pj);
      cJSON_Delete(tags); cJSON_Delete(p);
    }
    cJSON_Delete(j);
  }
  fprintf(stderr, "[luup-private] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def luup_private_def = {
  .id = "luup-private", .collector = "transport",
  .name = "LUUP scooters / e-bikes (ports)",
  .name_ja = "LUUP \xE9\x9B\xBB\xE5\x8B\x95\xE3\x82\xAD\xE3\x83\x83\xE3\x82\xAF\xE3\x83\x9C\xE3\x83\xBC\xE3\x83\x89/\xE8\x87\xAA\xE8\xBB\xA2\xE8\xBB\x8A",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(luup_private_def)
