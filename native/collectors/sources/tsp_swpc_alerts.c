/* collectors/sources/tsp_swpc_alerts.c
 * NOAA SWPC space weather alerts, watches and warnings — the operational alert
 * stream for radio and satellite operators.
 * Endpoint: https://services.swpc.noaa.gov/products/alerts.json  (keyless)
 * Emits: product_id, issue_datetime, the full alert message, and the fields
 *   parsed out of it — Space Weather Message Code, Serial Number, Issue Time
 *   and NOAA Scale — plus the product family derived from the product_id
 *   prefix (K geomagnetic, P proton, EF electron, R radio blackout).
 * Geo: none — these are global/heliospheric alerts (R2).
 *
 * parse_notes honoured: "Flat JSON array, newest first. The useful content is
 *   inside 'message', a CRLF-delimited plain-text block: parse 'Space Weather
 *   Message Code:', 'Serial Number:', 'Issue Time:' and 'NOAA Scale:' lines."
 *   The line scanner below stops at CR or LF so a CRLF body does not leak a
 *   stray \r into the stored value.
 * Licence: NOAA SWPC — US Government public domain, keyless.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALERTS_URL "https://services.swpc.noaa.gov/products/alerts.json"
#define MAX_ROWS 200

/* Copy the value that follows `label` in the CRLF-delimited message block.
 * Returns 1 on success. Stops at CR or LF, trims surrounding spaces. */
static int msg_field(const char *msg, const char *label, char *out, size_t cap) {
  const char *h = strstr(msg, label);
  if (!h) return 0;
  const char *p = h + strlen(label);
  while (*p == ' ' || *p == '\t') p++;
  size_t j = 0;
  while (*p && *p != '\r' && *p != '\n' && j + 1 < cap) out[j++] = *p++;
  while (j && (out[j - 1] == ' ' || out[j - 1] == '\t')) j--;
  out[j] = '\0';
  return j > 0;
}

static const char *family_of(const char *pid) {
  if (!pid) return NULL;
  if (pid[0] == 'K') return "geomagnetic Kp";
  if (pid[0] == 'R') return "radio blackout";
  if (pid[0] == 'P') return "solar radiation / proton";
  if (pid[0] == 'E' && pid[1] == 'F') return "electron flux";
  if (pid[0] == 'A') return "alert";
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, ALERTS_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[swpc-alerts] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[swpc-alerts] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *a;
  cJSON_ArrayForEach(a, doc) {
    if (n >= MAX_ROWS) break;
    const char *pid  = jo_sv(a, "product_id");
    const char *issd = jo_sv(a, "issue_datetime");
    const char *msg  = jo_sv(a, "message");
    if (!msg || !issd) continue;                  /* nothing real -> no row */

    char code[128] = "", serial[64] = "", itime[64] = "", scale[128] = "";
    msg_field(msg, "Space Weather Message Code:", code, sizeof code);
    msg_field(msg, "Serial Number:", serial, sizeof serial);
    msg_field(msg, "Issue Time:", itime, sizeof itime);
    msg_field(msg, "NOAA Scale:", scale, sizeof scale);

    /* the first non-empty line makes a better human title than the code */
    char headline[220] = "";
    {
      const char *p = msg;
      while (*p == '\r' || *p == '\n' || *p == ' ') p++;
      size_t j = 0;
      while (*p && *p != '\r' && *p != '\n' && j + 1 < sizeof headline)
        headline[j++] = *p++;
      headline[j] = '\0';
    }

    cJSON *pr = cJSON_CreateObject();
    if (pid)  cJSON_AddStringToObject(pr, "product_id", pid);
    cJSON_AddStringToObject(pr, "issue_datetime", issd);
    if (code[0])   cJSON_AddStringToObject(pr, "message_code", code);
    if (serial[0]) cJSON_AddStringToObject(pr, "serial_number", serial);
    if (itime[0])  cJSON_AddStringToObject(pr, "issue_time", itime);
    if (scale[0])  cJSON_AddStringToObject(pr, "noaa_scale", scale);
    const char *fam = family_of(pid);
    if (fam) cJSON_AddStringToObject(pr, "product_family", fam);
    cJSON_AddStringToObject(pr, "message", msg);
    cJSON_AddStringToObject(pr, "source", "NOAA SWPC alerts.json");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[320], summary[256], key[128];
    snprintf(key, sizeof key, "%s|%s", pid ? pid : "SWPC", issd);
    if (headline[0])
      snprintf(title, sizeof title, "SWPC %s: %s", pid ? pid : "alert", headline);
    else
      snprintf(title, sizeof title, "SWPC %s alert %s", pid ? pid : "", issd);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             code[0] ? code : (fam ? fam : "space weather alert"),
             scale[0] ? " · NOAA Scale " : "", scale[0] ? scale : "",
             serial[0] ? " · serial " : "", serial[0] ? serial : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.body            = msg;
    it.link            = "https://www.swpc.noaa.gov/noaa-scales-explanation";
    it.lang            = "en";
    it.published_at    = issd;
    it.record_type     = "space-weather-alert";
    it.properties_json = pj;
    it.tags_json       = "[\"space-weather\",\"alert\",\"noaa\"]";
    /* R2: no geometry — these alerts are not localised. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[swpc-alerts] emitted %d\n", n);
  return 0;
}

static const source_def tsp_swpc_alerts_def = {
  .id = "swpc-alerts", .collector = "satellite",
  .name = "NOAA SWPC space weather alerts, watches and warnings",
  .update_interval_sec = 900, .run = run,
  .category = "satellite", .type = "api", .url = ALERTS_URL,
  .description = "Operational alert stream for radio and satellite operators: geomagnetic storm watches, radio blackout and solar radiation storm alerts and electron-flux warnings, with the exact threshold crossed and time.",
  .license = "NOAA SWPC — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_swpc_alerts_def)
