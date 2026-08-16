/* collectors/sources/cyi_ioda_outage_alerts.c
 * IODA (Georgia Tech) global connectivity-loss alerts across BGP, active
 * probing (ping-slash24) and darknet telemetry (merit-nt).
 * Endpoint: https://api.ioda.inetintel.cc.gatech.edu/v2/outages/alerts
 *           ?from=<epoch>&until=<epoch>&limit=200                  (keyless)
 * parse_notes: from/until are unix seconds and are MANDATORY; a ~1h window is
 * enough at a 15-minute cadence. Different endpoint family from the existing
 * ioda-jp source (which queries the JP signal API).
 * Emits per alert: entity type/code/name, datasource, level, time, value and
 * historyValue — upstream fields only. IODA returns no coordinates here, so
 * has_geo stays 0 (R2).
 * Licence: Georgia Tech IODA public API; cite IODA.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* epoch seconds -> ISO-8601 UTC, or NULL when the value is implausible. */
static const char *sec_to_iso(double s, char *out, size_t n) {
  if (!(s > 946684800.0 && s < 4102444800.0)) return NULL;
  time_t t = (time_t)s;
  struct tm tmv;
#if defined(_WIN32)
  if (gmtime_s(&tmv, &t) != 0) return NULL;
#else
  if (!gmtime_r(&t, &tmv)) return NULL;
#endif
  if (strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tmv) == 0) return NULL;
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  long now = (long)time(NULL);
  char url[320];
  snprintf(url, sizeof url,
           "https://api.ioda.inetintel.cc.gatech.edu/v2/outages/alerts"
           "?from=%ld&until=%ld&limit=200", now - 3600, now);

  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) { fprintf(stderr, "[ioda-outage-alerts] fetch/parse failed\n"); return -1; }

  /* Payload is an envelope ("outages.alerts"); the rows live under data/alerts
   * depending on the wrapper version — take whichever array is present. */
  const cJSON *rows = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(rows)) rows = cJSON_GetObjectItem(doc, "alerts");
  if (!cJSON_IsArray(rows) && cJSON_IsObject(cJSON_GetObjectItem(doc, "data")))
    rows = cJSON_GetObjectItem(cJSON_GetObjectItem(doc, "data"), "alerts");
  if (!cJSON_IsArray(rows)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[ioda-outage-alerts] no alert array in payload\n");
    return 0;                             /* fetched fine, nothing to read */
  }

  int n = 0;
  const cJSON *a;
  cJSON_ArrayForEach(a, rows) {
    const cJSON *ent = cJSON_GetObjectItem(a, "entity");
    const char *etype = cJSON_IsObject(ent) ? jo_sv(ent, "type") : NULL;
    const char *ecode = cJSON_IsObject(ent) ? jo_sv(ent, "code") : NULL;
    const char *ename = cJSON_IsObject(ent) ? jo_sv(ent, "name") : NULL;
    const char *ds    = jo_sv(a, "datasource");
    const char *level = jo_sv(a, "level");
    if (!ecode && !ename) continue;       /* no fetched identity -> no row */

    char iso[40];
    const char *when = NULL;
    const cJSON *tv = cJSON_GetObjectItem(a, "time");
    if (cJSON_IsNumber(tv)) when = sec_to_iso(tv->valuedouble, iso, sizeof iso);
    else { const char *ts = jo_sv(a, "time"); if (ts) when = ts; }

    cJSON *p = cJSON_CreateObject();
    if (etype) cJSON_AddStringToObject(p, "entity_type", etype);
    if (ecode) cJSON_AddStringToObject(p, "entity_code", ecode);
    if (ename) cJSON_AddStringToObject(p, "entity_name", ename);
    if (ds)    cJSON_AddStringToObject(p, "datasource", ds);
    if (level) cJSON_AddStringToObject(p, "level", level);
    const cJSON *val = cJSON_GetObjectItem(a, "value");
    if (cJSON_IsNumber(val)) cJSON_AddNumberToObject(p, "value", val->valuedouble);
    const cJSON *hv = cJSON_GetObjectItem(a, "historyValue");
    if (cJSON_IsNumber(hv)) cJSON_AddNumberToObject(p, "history_value", hv->valuedouble);
    if (when) cJSON_AddStringToObject(p, "time", when);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[256];
    snprintf(title, sizeof title, "IODA outage alert: %s [%s]",
             ename ? ename : ecode, etype ? etype : "entity");
    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             ds ? ds : "", (ds && level) ? " · level " : "",
             level ? level : "", when ? " · " : "", when ? when : "");
    char key[192];
    snprintf(key, sizeof key, "%s|%s|%s|%s",
             etype ? etype : "-", ecode ? ecode : "-", ds ? ds : "-",
             when ? when : "-");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://ioda.inetintel.cc.gatech.edu/";
    it.lang            = "en";
    it.published_at    = when;
    it.record_type     = "internet-outage-alert";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"outage\",\"ioda\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[ioda-outage-alerts] emitted %d\n", n);
  return 0;                                /* a quiet hour is not an error */
}

static const source_def cyi_ioda_outage_alerts_def = {
  .id = "ioda-outage-alerts", .collector = "cyber",
  .name = "IODA global outage alerts",
  .update_interval_sec = 900, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.ioda.inetintel.cc.gatech.edu/v2/outages/alerts",
  .description = "Worldwide connectivity-loss alerts across BGP, active probing and darknet telemetry, keyed by country/region/ASN.",
  .license = "Georgia Tech IODA public API; cite IODA.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_ioda_outage_alerts_def)
