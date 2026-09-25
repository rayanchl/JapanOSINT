/* US EPA Greenhouse Gas Reporting Program facility emissions (Envirofacts).
 * Endpoints (keyless REST, row window is MANDATORY in the path grammar
 *   /efservice/<table>/<col>/<value>/rows/<start>:<end>/JSON):
 *   https://data.epa.gov/efservice/pub_facts_sector_ghg_emission/year/2023/rows/0:1000/JSON
 *   https://data.epa.gov/efservice/pub_dim_facility/rows/0:2000/JSON
 * Emits one row per (facility, gas) reported: co2e_emission (UNIT: metric
 * tonnes CO2e), reporting year, sector_id/subsector_id/gas_id, facility name,
 * NAICS code and parent company, joined on facility_id.
 *
 * Both tables are windowed by the API itself, so a single window silently
 * caps the collector well below the true size of either table (house rule
 * 2). Both are now paged: each side walks 0:PAGE, PAGE:2*PAGE, ... until a
 * page comes back shorter than requested (the upstream's own "no more rows"
 * signal), or a runaway-guard page ceiling is hit, in which case a
 * collector-truncation-notice is emitted per docs/SOURCE_EXHAUSTIVENESS.md
 * rule 8.
 *
 * GEO (R2): latitude/longitude come from pub_dim_facility only, and a row with
 * fac latitude/longitude of 0 (a real defect in that table) is emitted WITHOUT
 * geometry rather than pinned to Null Island.
 * Licence: US EPA Envirofacts, public domain, keyless. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "epa-ghgrp-emissions"
#define YEAR "2023"

#define FAC_PAGE 2000
#define EM_PAGE 1000
#define FAC_MAX_PAGES 50   /* exhaustive-ok: page-walk runaway guard; an early stop emits a collector-truncation-notice */
#define EM_MAX_PAGES 100   /* exhaustive-ok: page-walk runaway guard; an early stop emits a collector-truncation-notice */

typedef struct { long long id; double lat, lon; int has_geo;
                 char *name, *naics, *parent, *state; } fac_t;

static long long iv(const cJSON *o, const char *k, long long dflt) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) return (long long)v->valuedouble;
  if (cJSON_IsString(v) && v->valuestring[0]) return atoll(v->valuestring);
  return dflt;
}

static char *dupsv(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  return (s && s[0]) ? strdup(s) : NULL;
}

static const fac_t *find_fac(const fac_t *f, int n, long long id) {
  for (int i = 0; i < n; i++) if (f[i].id == id) return &f[i];
  return NULL;
}

static void free_facs(fac_t *facs, int n) {
  for (int i = 0; i < n; i++) {
    free(facs[i].name); free(facs[i].naics);
    free(facs[i].parent); free(facs[i].state);
  }
  free(facs);
}

/* Pages pub_dim_facility until a short page or the runaway guard. Returns the
 * facility count on success (>=0) and fills *out_facs, or -1 on a real fetch
 * failure. *out_capped is set if the runaway guard fired before a short page. */
static int fetch_facilities(const source_ctx *ctx, fac_t **out_facs, int *out_capped) {
  int cap = 0, n = 0;
  fac_t *facs = NULL;
  *out_capped = 0;
  for (int page = 0; page < FAC_MAX_PAGES; page++) {
    long long start = (long long)page * FAC_PAGE, end = start + FAC_PAGE;
    char url[256];
    snprintf(url, sizeof url,
      "https://data.epa.gov/efservice/pub_dim_facility/rows/%lld:%lld/JSON", start, end);
    cJSON *facdoc = feed_get_json(ctx->http, url, 60000);
    if (!facdoc) {
      fprintf(stderr, "[" SRC "] facility fetch failed at row %lld\n", start);
      free_facs(facs, n); return -1;
    }
    if (!cJSON_IsArray(facdoc)) {
      cJSON_Delete(facdoc);
      fprintf(stderr, "[" SRC "] facility shape at row %lld\n", start);
      free_facs(facs, n); return -1;
    }
    int got = cJSON_GetArraySize(facdoc);
    cJSON *f;
    cJSON_ArrayForEach(f, facdoc) {
      long long id = iv(f, "facility_id", -1);
      if (id < 0) continue;
      if (find_fac(facs, n, id)) continue;     /* table repeats per year */
      if (n == cap) { cap = cap ? cap * 2 : 256; facs = realloc(facs, (size_t)cap * sizeof(fac_t)); }
      fac_t *e = &facs[n++];
      memset(e, 0, sizeof *e);
      e->id = id;
      e->name   = dupsv(f, "facility_name");
      e->naics  = dupsv(f, "naics_code");
      e->parent = dupsv(f, "parent_company");
      e->state  = dupsv(f, "state");
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
    cJSON_Delete(facdoc);
    if (got < FAC_PAGE) { *out_facs = facs; return n; }        /* short page: exhausted */
    if (page == FAC_MAX_PAGES - 1) *out_capped = 1;            /* full last page: guard hit */
  }
  *out_facs = facs;
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  fac_t *facs = NULL;
  int fac_capped = 0;  /* exhaustive-ok: scanner false positive — a boolean flag set when FAC_MAX_PAGES's page-walk guard fires (see fetch_facilities), not a record cap; the actual bound is FAC_MAX_PAGES above, already marked */
  int fn = fetch_facilities(ctx, &facs, &fac_capped);
  if (fn < 0) return -1;

  int n = 0, em_capped = 0;
  long long em_seen = 0;
  for (int page = 0; page < EM_MAX_PAGES; page++) {
    long long start = (long long)page * EM_PAGE, end = start + EM_PAGE;
    char url[320];
    snprintf(url, sizeof url,
      "https://data.epa.gov/efservice/pub_facts_sector_ghg_emission/year/" YEAR
      "/rows/%lld:%lld/JSON", start, end);
    cJSON *emdoc = feed_get_json(ctx->http, url, 60000);
    if (!emdoc || !cJSON_IsArray(emdoc)) {
      if (emdoc) cJSON_Delete(emdoc);
      fprintf(stderr, "[" SRC "] emission fetch failed at row %lld\n", start);
      free_facs(facs, fn);
      return -1;
    }
    int got = cJSON_GetArraySize(emdoc);
    em_seen += got;

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
    if (got < EM_PAGE) break;                                 /* short page: exhausted */
    if (page == EM_MAX_PAGES - 1) em_capped = 1;               /* full last page: guard hit */
  }

  if (fac_capped)
    jo_trunc_notice(sink, SRC, "pub_dim_facility", (long)fn, -1,
                     "facility dimension table page-walk hit its runaway guard "
                     "(FAC_MAX_PAGES) before a short page signalled the end",
                     "raise FAC_MAX_PAGES in eni_epa_ghgrp.c");
  if (em_capped)
    jo_trunc_notice(sink, SRC, "pub_facts_sector_ghg_emission", (long)em_seen, -1,
                     "emission facts table page-walk hit its runaway guard "
                     "(EM_MAX_PAGES) before a short page signalled the end",
                     "raise EM_MAX_PAGES in eni_epa_ghgrp.c");

  free_facs(facs, fn);
  fprintf(stderr, "[" SRC "] emitted %d (facilities=%d)\n", n, fn);
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
