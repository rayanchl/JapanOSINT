/* US EPA Greenhouse Gas Reporting Program facility emissions (Envirofacts).
 * Endpoints (keyless REST, row window is MANDATORY in the path grammar
 *   /efservice/<table>/<col>/<value>/rows/<start>:<end>/JSON):
 *   https://data.epa.gov/efservice/pub_facts_sector_ghg_emission/year/2023/rows/0:1000/JSON
 *   https://data.epa.gov/efservice/pub_dim_facility/rows/0:2000/JSON
 * Emits one row per (facility, gas) reported: co2e_emission (UNIT: metric
 * tonnes CO2e), reporting year, sector_id/subsector_id/gas_id, facility name,
 * NAICS code and parent company, joined on facility_id.
 *
 * GEO (R2): latitude/longitude come from pub_dim_facility only, and a row with
 * fac latitude/longitude of 0 (a real defect in that table) is emitted WITHOUT
 * geometry rather than pinned to Null Island.
 * Licence: US EPA Envirofacts, public domain, keyless. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "epa-ghgrp-emissions"
#define YEAR "2023"

typedef struct { long long id; double lat, lon; int has_geo;
                 const char *name, *naics, *parent, *state; } fac_t;

static long long iv(const cJSON *o, const char *k, long long dflt) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) return (long long)v->valuedouble;
  if (cJSON_IsString(v) && v->valuestring[0]) return atoll(v->valuestring);
  return dflt;
}

static const fac_t *find_fac(const fac_t *f, int n, long long id) {
  for (int i = 0; i < n; i++) if (f[i].id == id) return &f[i];
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *facdoc = feed_get_json(ctx->http,
    "https://data.epa.gov/efservice/pub_dim_facility/rows/0:2000/JSON", 60000);
  if (!facdoc) { fprintf(stderr, "[" SRC "] facility fetch failed\n"); return -1; }
  if (!cJSON_IsArray(facdoc)) { cJSON_Delete(facdoc); fprintf(stderr, "[" SRC "] facility shape\n"); return -1; }

  int nfac = cJSON_GetArraySize(facdoc);
  fac_t *facs = calloc(nfac > 0 ? (size_t)nfac : 1, sizeof(fac_t));
  if (!facs) { cJSON_Delete(facdoc); return -1; }
  int fn = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, facdoc) {
    long long id = iv(f, "facility_id", -1);
    if (id < 0) continue;
    if (find_fac(facs, fn, id)) continue;     /* table repeats per year */
    fac_t *e = &facs[fn++];
    e->id = id;
    e->name   = jo_sv(f, "facility_name");
    e->naics  = jo_sv(f, "naics_code");
    e->parent = jo_sv(f, "parent_company");
    e->state  = jo_sv(f, "state");
    cJSON *la = cJSON_GetObjectItem(f, "latitude");
    cJSON *lo = cJSON_GetObjectItem(f, "longitude");
    if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
      e->lat = la->valuedouble; e->lon = lo->valuedouble;
      /* zero coordinates are a known defect in this table — not a location */
      if (e->lat != 0.0 && e->lon != 0.0 &&
          e->lat >= -90 && e->lat <= 90 && e->lon >= -180 && e->lon <= 180)
        e->has_geo = 1;
    }
  }

  cJSON *emdoc = feed_get_json(ctx->http,
    "https://data.epa.gov/efservice/pub_facts_sector_ghg_emission/year/" YEAR
    "/rows/0:1000/JSON", 60000);
  if (!emdoc || !cJSON_IsArray(emdoc)) {
    if (emdoc) cJSON_Delete(emdoc);
    cJSON_Delete(facdoc); free(facs);
    fprintf(stderr, "[" SRC "] emission fetch failed\n");
    return -1;
  }

  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, emdoc) {
    long long fid = iv(e, "facility_id", -1);
    cJSON *q = cJSON_GetObjectItem(e, "co2e_emission");
    if (fid < 0 || !cJSON_IsNumber(q)) continue;   /* no measurement, no row */
    const fac_t *fa = find_fac(facs, fn, fid);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "facility_id", (double)fid);
    if (fa && fa->name)   cJSON_AddStringToObject(p, "facility_name", fa->name);
    if (fa && fa->state)  cJSON_AddStringToObject(p, "state", fa->state);
    if (fa && fa->naics)  cJSON_AddStringToObject(p, "naics_code", fa->naics);
    if (fa && fa->parent) cJSON_AddStringToObject(p, "parent_company", fa->parent);
    cJSON_AddNumberToObject(p, "co2e_emission", q->valuedouble);
    cJSON_AddStringToObject(p, "unit", "tonnes CO2e");
    cJSON_AddNumberToObject(p, "reporting_year", (double)iv(e, "year", 0));
    cJSON_AddNumberToObject(p, "sector_id", (double)iv(e, "sector_id", -1));
    cJSON_AddNumberToObject(p, "subsector_id", (double)iv(e, "subsector_id", -1));
    cJSON_AddNumberToObject(p, "gas_id", (double)iv(e, "gas_id", -1));
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[288];
    snprintf(key, sizeof key, "%lld|%s|%lld", fid, YEAR, iv(e, "gas_id", -1));
    snprintf(title, sizeof title, "%s (GHGRP %lld) %s: %.1f t CO2e",
             fa && fa->name ? fa->name : "US GHGRP facility", fid, YEAR,
             q->valuedouble);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://www.epa.gov/ghgreporting";
    row.record_type     = "emitting-facility";
    row.has_geo         = fa ? fa->has_geo : 0;
    row.lat = fa ? fa->lat : 0; row.lon = fa ? fa->lon : 0;
    row.properties_json = pj;
    row.tags_json       = "[\"industry\",\"emissions\",\"epa\",\"usa\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(emdoc);
  cJSON_Delete(facdoc);
  free(facs);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_epa_ghgrp_def = {
  .id = SRC, .collector = "industry",
  .name = "US EPA GHGRP facility emissions (Envirofacts)",
  .update_interval_sec = 604800, .run = run,
  .category = "industry", .type = "api",
  .url = "https://data.epa.gov/efservice/pub_facts_sector_ghg_emission/year/2023/rows/0:1000/JSON",
  .description = "Reported annual greenhouse gas emissions (tonnes CO2e) for large US industrial emitters, joined to facility name, NAICS, parent company and coordinates.",
  .license = "US EPA Envirofacts, public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_epa_ghgrp_def)
