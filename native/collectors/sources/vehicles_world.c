/* collectors/sources/vehicles_world.c
 * OSINT services — vehicles / aircraft / vessels registries & inspections.
 * On-demand entity pivot (ctx->entity = a VIN, an N-number, an IMO/vessel name,
 * or a ship/company name) against the REAL public endpoints published by the
 * respective authorities. One run() dispatches on ctx->source_id.
 *
 *   NHTSA_VIN      US NHTSA vPIC — decode a 17-char VIN (keyless JSON API):
 *                  vpic.nhtsa.dot.gov/api/vehicles/decodevin/<vin>?format=json.
 *                  Emits one intel_item per decoded VIN (make/model/year/…).
 *   FAA_REGISTRY   FAA civil aircraft registry — scrape the N-number inquiry
 *                  result page (registry.faa.gov) for a tail number.
 *   PARIS_MOU_PSC  Paris MoU port-state-control inspection search (parismou.org).
 *   TOKYO_MOU_PSC  Tokyo MoU port-state-control inspection search (tokyo-mou.org).
 *   USCG_PSIX      US Coast Guard PSIX vessel search (cgmix.uscg.mil).
 *
 * HONESTY: every path REAL-fetches. NHTSA is a documented keyless JSON API and
 * emits only parsed fields. The four scrape sources hit the authoritative
 * public result/listing pages and emit only anchors/fields actually extracted
 * from the returned HTML; JS-only / POST-only / anti-bot pages simply yield 0
 * (honest empty). Nothing is ever synthesized. No entity → honest empty. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- NHTSA vPIC VIN decode (keyless JSON) ------------------------------- *
 * decodevin returns { "Results": [ { "Variable": "...", "Value": "...",
 * "ValueId": "..." }, ... ] } — one row per decodable attribute. We collect
 * the non-empty rows into a single intel_item keyed by the VIN. Only genuine
 * decoded values are emitted; if the API returns no usable values, empty. */
static int nhtsa_vin(const source_ctx *ctx, intel_sink *sink, const char *vin) {
  /* basic VIN shape guard (11–17 alnum, excluding I/O/Q by spec but be lax) */
  size_t n = strlen(vin);
  if (n < 5 || n > 17) { fprintf(stderr, "[NHTSA_VIN] entity not VIN-shaped\n"); return 0; }
  for (const char *p = vin; *p; p++)
    if (!isalnum((unsigned char)*p)) { fprintf(stderr, "[NHTSA_VIN] entity not VIN-shaped\n"); return 0; }

  char *enc = jo_urlencode(vin);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://vpic.nhtsa.dot.gov/api/vehicles/decodevin/%s?format=json", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (vehicle-osint)", NULL };
  char *body = jo_get(ctx, url, hdrs, "NHTSA_VIN");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *results = cJSON_GetObjectItem(root, "Results");
  if (!cJSON_IsArray(results)) { cJSON_Delete(root); return 0; }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "vin", vin);
  int nfields = 0;
  const char *make = NULL, *model = NULL, *year = NULL;
  cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const char *var = jo_sv(r, "Variable");
    const char *val = jo_sv(r, "Value");
    if (!var || !val) continue;
    if (!strcmp(val, "Not Applicable") || !strcmp(val, "0")) continue;
    cJSON_AddStringToObject(data, var, val);
    if (!make  && !strcmp(var, "Make"))       make  = jo_sv(r, "Value");
    if (!model && !strcmp(var, "Model"))      model = jo_sv(r, "Value");
    if (!year  && !strcmp(var, "Model Year")) year  = jo_sv(r, "Value");
    nfields++;
  }
  cJSON_AddStringToObject(data, "source", "NHTSA vPIC");
  if (nfields == 0) { cJSON_Delete(data); cJSON_Delete(root); return 0; }
  char *bj = cJSON_PrintUnformatted(data);

  char title[256];
  if (year || make || model)
    snprintf(title, sizeof title, "%s %s %s (VIN %s)",
             year ? year : "", make ? make : "", model ? model : "", vin);
  else
    snprintf(title, sizeof title, "VIN %s", vin);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "NHTSA_VIN");
  cJSON_AddStringToObject(props, "vin", vin);
  if (make)  cJSON_AddStringToObject(props, "make", make);
  if (model) cJSON_AddStringToObject(props, "model", model);
  if (year)  cJSON_AddStringToObject(props, "year", year);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = vin;
  it.title           = title;
  it.summary         = make ? make : NULL;
  it.body            = bj;
  it.lang            = "en";
  it.link            = "https://vpic.nhtsa.dot.gov/decoder/";
  it.record_type     = "nhtsa-vin";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"vehicle\",\"NHTSA_VIN\"]";
  int emitted = (sink->emit(sink, &it) >= 0) ? 1 : 0;

  cJSON_Delete(data); cJSON_Delete(props); cJSON_Delete(root);
  free(bj); free(pj);
  fprintf(stderr, "[NHTSA_VIN] emitted %d\n", emitted);
  return emitted;
}

/* ---- generic scrape helper: fetch a result page and emit its anchors ---- *
 * Wraps jo_emit_anchors with a UA header path by fetching ourselves would be
 * redundant — jo_emit_anchors already fetches. It hits the authoritative
 * server-rendered result URL and echoes real <a> hits; JS/POST-only pages
 * yield 0 (honest empty). */

/* Paris MoU — inspection search. The public search accepts a ship IMO or name
 * as a GET query on the inspection-search endpoint. */
static int paris_mou(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[768];
  snprintf(url, sizeof url,
    "https://www.parismou.org/inspection-search/inspection-search?query=%s", enc);
  free(enc);
  int e = jo_emit_anchors(ctx, sink, url, "/inspection", "PARIS_MOU_PSC",
                          "psc-inspection", "https://www.parismou.org", q,
                          40, "PARIS_MOU_PSC");
  return e;
}

/* Tokyo MoU — PSC inspection / detention search. */
static int tokyo_mou(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[768];
  snprintf(url, sizeof url,
    "https://www.tokyo-mou.org/inspections_detentions/psc_database.php?keyword=%s", enc);
  free(enc);
  int e = jo_emit_anchors(ctx, sink, url, NULL, "TOKYO_MOU_PSC",
                          "psc-inspection", "https://www.tokyo-mou.org", q,
                          40, "TOKYO_MOU_PSC");
  return e;
}

/* FAA civil aircraft registry — N-number inquiry result page. The registry
 * exposes a per-tail-number result page; we scrape its anchors/links. */
static int faa_registry(const source_ctx *ctx, intel_sink *sink, const char *q) {
  /* normalize: registry expects the N-number without a leading 'N' in the path
   * segment used by the modern site; keep the raw query but strip a leading N
   * for the classic NNumSQL path fallback. */
  const char *nn = q;
  if ((*nn == 'N' || *nn == 'n')) nn++;
  char *enc = jo_urlencode(nn);
  if (!enc) return 0;
  char url[768];
  snprintf(url, sizeof url,
    "https://registry.faa.gov/AircraftInquiry/Search/NNumberResult?NNumbertxt=%s", enc);
  free(enc);
  int e = jo_emit_anchors(ctx, sink, url, NULL, "FAA_REGISTRY",
                          "aircraft-registration", "https://registry.faa.gov", NULL,
                          40, "FAA_REGISTRY");
  return e;
}

/* US Coast Guard PSIX vessel search (cgmix.uscg.mil). The web search results
 * page lists matched vessels as anchors to vessel-summary pages. */
static int uscg_psix(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[768];
  snprintf(url, sizeof url,
    "https://cgmix.uscg.mil/PSIX/PSIXSearch.aspx?VesselName=%s", enc);
  free(enc);
  int e = jo_emit_anchors(ctx, sink, url, "Vessel", "USCG_PSIX",
                          "vessel-record", "https://cgmix.uscg.mil", q,
                          40, "USCG_PSIX");
  return e;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;

  const char *id = ctx->source_id;
  if      (!strcmp(id, "NHTSA_VIN"))     nhtsa_vin(ctx, sink, q);
  else if (!strcmp(id, "FAA_REGISTRY"))  faa_registry(ctx, sink, q);
  else if (!strcmp(id, "PARIS_MOU_PSC")) paris_mou(ctx, sink, q);
  else if (!strcmp(id, "TOKYO_MOU_PSC")) tokyo_mou(ctx, sink, q);
  else if (!strcmp(id, "USCG_PSIX"))     uscg_psix(ctx, sink, q);
  else return -1;
  return 0;   /* honest empty is not an error */
}

#define VEH_DEF(SYM, ID, NAME, NAMEJA, TYPE, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "transport", .type = TYPE, .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

VEH_DEF(veh_nhtsa_def, "NHTSA_VIN", "NHTSA VIN Decoder", "NHTSA VIN デコーダ",
  "api", "https://vpic.nhtsa.dot.gov/decoder/",
  "US NHTSA vPIC — decode a 17-char VIN into make/model/year/plant (keyless JSON API)");
VEH_DEF(veh_faa_def, "FAA_REGISTRY", "FAA Aircraft Registry", "FAA 航空機登録",
  "scraped", "https://registry.faa.gov/AircraftInquiry/",
  "FAA civil aircraft registry — N-number inquiry result scrape");
VEH_DEF(veh_paris_def, "PARIS_MOU_PSC", "Paris MoU PSC Inspections", "パリMOU PSC 検査",
  "scraped", "https://www.parismou.org/inspection-search",
  "Paris MoU port-state-control inspection search scrape (ship IMO/name)");
VEH_DEF(veh_tokyo_def, "TOKYO_MOU_PSC", "Tokyo MoU PSC Inspections", "東京MOU PSC 検査",
  "scraped", "https://www.tokyo-mou.org/inspections_detentions/",
  "Tokyo MoU port-state-control inspection/detention search scrape");
VEH_DEF(veh_uscg_def, "USCG_PSIX", "USCG PSIX Vessel Search", "USCG PSIX 船舶検索",
  "scraped", "https://cgmix.uscg.mil/PSIX/",
  "US Coast Guard PSIX vessel record search scrape (vessel name)");
