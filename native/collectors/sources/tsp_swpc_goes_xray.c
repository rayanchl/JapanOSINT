/* collectors/sources/tsp_swpc_goes_xray.c
 * NOAA SWPC GOES X-ray flux (6-hour window) — the measurement that defines
 * solar flare class and therefore sudden-ionospheric-disturbance HF blackouts
 * on the sunlit hemisphere.
 * Endpoint: https://services.swpc.noaa.gov/json/goes/primary/xrays-6-hour.json
 *   (keyless)
 * Emits one row per energy band per run: the latest good sample (time_tag,
 *   satellite, flux, observed_flux, electron_correction) plus the six-hour peak
 *   flux and its time, and the derived flare class for the 0.1-0.8 nm band.
 * Geo: none — a solar measurement has no ground point (R2).
 *
 * parse_notes honoured:
 *  - "Flat JSON array with TWO interleaved series distinguished by 'energy'
 *    ('0.05-0.4nm' and '0.1-0.8nm')": the two series are separated by that key.
 *  - "Skip rows where electron_contaminaton is true. Note the upstream
 *    misspells 'contaminaton'": BOTH spellings are checked, so a corrected
 *    upstream would not silently reintroduce bad samples.
 *  - The manifest note says "1e-6 W/m2 = M1, 1e-5 = X1". That is one decade
 *    off the NOAA convention actually used by SWPC (A <1e-7, B 1e-7, C 1e-6,
 *    M 1e-5, X 1e-4 W/m2 in the 0.1-0.8 nm band), so the NOAA thresholds are
 *    used here and the convention is named in properties.
 * Licence: NOAA SWPC — US Government public domain, keyless.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XRAY_URL "https://services.swpc.noaa.gov/json/goes/primary/xrays-6-hour.json"

static int contaminated(const cJSON *o) {
  const char *keys[] = { "electron_contaminaton", "electron_contamination" };
  for (int i = 0; i < 2; i++) {
    const cJSON *v = cJSON_GetObjectItem(o, keys[i]);
    if (cJSON_IsTrue(v)) return 1;
  }
  return 0;
}
/* NOAA GOES flare class for the 0.1-0.8 nm band, W/m^2. */
static void flare_class(double f, char *out, size_t cap) {
  const char *letter; double base;
  if (f >= 1e-4)      { letter = "X"; base = 1e-4; }
  else if (f >= 1e-5) { letter = "M"; base = 1e-5; }
  else if (f >= 1e-6) { letter = "C"; base = 1e-6; }
  else if (f >= 1e-7) { letter = "B"; base = 1e-7; }
  else                { letter = "A"; base = 1e-8; }
  snprintf(out, cap, "%s%.1f", letter, f / base);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, XRAY_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[swpc-goes-xray] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[swpc-goes-xray] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *bands[2] = { "0.05-0.4nm", "0.1-0.8nm" };
  int n = 0;
  for (int b = 0; b < 2; b++) {
    const cJSON *latest = NULL, *peak = NULL;
    double latest_flux = 0, peak_flux = -1;
    int samples = 0, skipped = 0;
    const cJSON *s;
    cJSON_ArrayForEach(s, doc) {
      const char *en = jo_sv(s, "energy");
      if (!en || strcmp(en, bands[b]) != 0) continue;
      if (contaminated(s)) { skipped++; continue; }
      double flux;
      if (!jo_num(s, "flux", &flux)) continue;
      const char *tt = jo_sv(s, "time_tag");
      if (!tt) continue;
      samples++;
      latest = s; latest_flux = flux;       /* array is oldest-first */
      if (flux > peak_flux) { peak_flux = flux; peak = s; }
    }
    if (!latest || samples == 0) continue;   /* nothing usable for this band */

    const char *tt = jo_sv(latest, "time_tag");
    double sat = 0;
    int has_sat = jo_num(latest, "satellite", &sat);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "energy_band", bands[b]);
    cJSON_AddStringToObject(pr, "time_tag", tt);
    if (has_sat) cJSON_AddNumberToObject(pr, "goes_satellite", sat);
    cJSON_AddNumberToObject(pr, "flux_w_m2", latest_flux);
    double d;
    if (jo_num(latest, "observed_flux", &d))       cJSON_AddNumberToObject(pr, "observed_flux_w_m2", d);
    if (jo_num(latest, "electron_correction", &d)) cJSON_AddNumberToObject(pr, "electron_correction", d);
    cJSON_AddNumberToObject(pr, "peak_flux_6h_w_m2", peak_flux);
    if (peak) {
      const char *pt = jo_sv(peak, "time_tag");
      if (pt) cJSON_AddStringToObject(pr, "peak_flux_6h_time", pt);
    }
    cJSON_AddNumberToObject(pr, "samples_used", samples);
    cJSON_AddNumberToObject(pr, "samples_skipped_electron_contaminated", skipped);
    char cls[16] = "", pcls[16] = "";
    if (strcmp(bands[b], "0.1-0.8nm") == 0) {
      flare_class(latest_flux, cls, sizeof cls);
      flare_class(peak_flux, pcls, sizeof pcls);
      cJSON_AddStringToObject(pr, "flare_class", cls);
      cJSON_AddStringToObject(pr, "peak_flare_class_6h", pcls);
      cJSON_AddStringToObject(pr, "flare_class_convention",
        "NOAA GOES 0.1-0.8 nm: C=1e-6, M=1e-5, X=1e-4 W/m2");
    }
    cJSON_AddStringToObject(pr, "source", "NOAA SWPC GOES primary X-ray sensor");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[224], summary[224], key[96], ts[48];
    snprintf(key, sizeof key, "%s|%s", bands[b], tt);
    snprintf(ts, sizeof ts, "%s", tt);          /* already carries a Z suffix */
    if (cls[0])
      snprintf(title, sizeof title, "GOES X-ray %s: %s (%.3e W/m2) at %s",
               bands[b], cls, latest_flux, tt);
    else
      snprintf(title, sizeof title, "GOES X-ray %s: %.3e W/m2 at %s",
               bands[b], latest_flux, tt);
    snprintf(summary, sizeof summary, "6-hour peak %.3e W/m2%s%s · %d samples",
             peak_flux, pcls[0] ? " (" : "", pcls[0] ? pcls : "", samples);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.swpc.noaa.gov/products/goes-x-ray-flux";
    it.lang            = "en";
    it.published_at    = ts;
    it.record_type     = "solar-xray-flux";
    it.properties_json = pj;
    it.tags_json       = "[\"space-weather\",\"solar\",\"hf-propagation\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[swpc-goes-xray] emitted %d band row(s)\n", n);
  return 0;
}

static const source_def tsp_swpc_goes_xray_def = {
  .id = "swpc-goes-xray", .collector = "satellite",
  .name = "NOAA SWPC GOES X-ray flux (6-hour)",
  .update_interval_sec = 900, .run = run,
  .category = "satellite", .type = "api", .url = XRAY_URL,
  .description = "Live GOES soft X-ray flux in both energy bands with the six-hour peak and derived flare class — the measurement behind sudden-ionospheric-disturbance HF blackouts.",
  .license = "NOAA SWPC — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_swpc_goes_xray_def)
