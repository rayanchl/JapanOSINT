/* Hong Kong EPD Air Quality Health Index (past-24h XML report).
 * Endpoint: https://www.aqhi.gov.hk/epd/ddata/html/out/24aqhi_Eng.xml (keyless)
 * RSS-like XML: AQHI24HrReport -> item[] with type ("General Stations" /
 * "Roadside Stations"), StationName, DateTime and aqhi.
 * Emits one row per station: the NEWEST hour in the file for that station.
 *
 * UNIT TRAP: AQHI is a 1-10+ health INDEX, dimensionless — it is NOT a
 * concentration and is never labelled ug/m3. Concentrations for the same
 * stations live in the sibling 24pc_Eng.xml.
 * GEO (R2): the feed carries no station coordinates, so no geometry is emitted.
 * Licence: HKSAR EPD open data (data.gov.hk terms); the XML carries an explicit
 * EPD copyright line — attribution required. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "hk-epd-aqhi"
#define MAXST 64

typedef struct { char name[96]; char when[64]; char type[48]; double aqhi; } st_t;

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http,
    "https://www.aqhi.gov.hk/epd/ddata/html/out/24aqhi_Eng.xml", 30000);
  if (!xml) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  st_t st[MAXST]; int nst = 0;
  const char *cur = xml, *inner = NULL;
  int ilen = 0, saw_item = 0;
  while ((cur = html_block(cur, "item", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    saw_item = 1;
    char blk[1024];
    int copy = ilen < (int)sizeof blk - 1 ? ilen : (int)sizeof blk - 1;
    memcpy(blk, inner, (size_t)copy);
    blk[copy] = 0;

    char rawn[256], rawt[128], rawd[128], rawa[64];
    if (!html_tag(blk, "StationName", rawn, sizeof rawn)) continue;
    if (!html_tag(blk, "aqhi", rawa, sizeof rawa)) continue;
    if (!html_tag(blk, "DateTime", rawd, sizeof rawd)) continue;
    html_tag(blk, "type", rawt, sizeof rawt);

    char *nm = html_strip(rawn), *dt = html_strip(rawd);
    char *aq = html_strip(rawa), *ty = html_strip(rawt);
    if (!nm || !dt || !aq || !ty) { free(nm); free(dt); free(aq); free(ty); continue; }
    char *e = NULL;
    double v = strtod(aq, &e);
    int ok = (e != aq && aq[0]);
    if (ok) {
      int idx = -1;
      for (int i = 0; i < nst; i++)
        if (strcmp(st[i].name, nm) == 0) { idx = i; break; }
      if (idx < 0 && nst < MAXST) {
        idx = nst++;
        st[idx].name[0] = 0; st[idx].when[0] = 0;
        snprintf(st[idx].name, sizeof st[idx].name, "%s", nm);
      }
      /* keep the newest hour per station; the file is chronological, so the
       * later item for a station supersedes the earlier one */
      if (idx >= 0) {
        snprintf(st[idx].when, sizeof st[idx].when, "%s", dt);
        snprintf(st[idx].type, sizeof st[idx].type, "%s", ty);
        st[idx].aqhi = v;
      }
    }
    free(nm); free(dt); free(aq); free(ty);
  }
  free(xml);
  if (!saw_item) { fprintf(stderr, "[" SRC "] no item elements\n"); return -1; }

  int n = 0;
  for (int i = 0; i < nst; i++) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_name", st[i].name);
    if (st[i].type[0]) cJSON_AddStringToObject(p, "station_type", st[i].type);
    cJSON_AddNumberToObject(p, "aqhi", st[i].aqhi);
    cJSON_AddStringToObject(p, "unit", "index (dimensionless, 1-10+)");
    cJSON_AddStringToObject(p, "unit_warning",
      "AQHI is a health index, not a pollutant concentration");
    cJSON_AddStringToObject(p, "datetime_local", st[i].when);
    cJSON_AddStringToObject(p, "country", "HK");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[224];
    snprintf(title, sizeof title, "Hong Kong %s: AQHI %.0f", st[i].name, st[i].aqhi);

    intel_item row = {0};
    row.remote_key      = st[i].name;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://www.aqhi.gov.hk/en.html";
    row.record_type     = "air-quality-index";
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"air-quality\",\"hong-kong\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_hk_epd_aqhi_def = {
  .id = SRC, .collector = "environment",
  .name = "Hong Kong EPD Air Quality Health Index",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.aqhi.gov.hk/epd/ddata/html/out/24aqhi_Eng.xml",
  .description = "Hourly Air Quality Health Index for all Hong Kong general and roadside monitoring stations (dimensionless health index, not a concentration).",
  .license = "HKSAR EPD open data (data.gov.hk terms); attribution to EPD required.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_hk_epd_aqhi_def)
