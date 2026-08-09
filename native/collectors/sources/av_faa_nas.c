/* collectors/sources/av_faa_nas.c
 * FAA National Airspace System status — three keyless endpoints, none of
 * which carries a coordinate.
 *
 *   faa-tfr-list       GET https://tfr.faa.gov/tfrapi/exportTfrList   (JSON
 *       array). Emits per TFR: notam_id, type (HAZARDS/SECURITY/VIP/SPORTS/
 *       EMERGENCY), facility (issuing ARTCC), state, description and
 *       creation_date. TFRs are the single best open indicator of wildfires,
 *       VIP movements, disasters and security events in US airspace.
 *   faa-airport-status GET https://nasstatus.faa.gov/api/airport-status-information
 *       (XML). Emits ground delay programs (ARPT, Reason, Avg, Max), general
 *       arrival/departure delays (ARPT, Reason, direction, Min, Max, Trend),
 *       ground stops and airport closures (ARPT, Reason, Start, Reopen), plus
 *       the document's Update_Time.
 *   faa-ops-plan       GET https://nasstatus.faa.gov/api/operations-plan (JSON).
 *       Emits every terminalPlanned[] and enRoutePlanned[] entry (time, event)
 *       from the ATCSCC daily operations plan, plus the advisory `link`.
 *
 * NO GEOMETRY ANYWHERE IN THIS FILE (R2), deliberately:
 *  - the TFR list carries only a two-letter `state` and a description
 *    embedding a bearing/distance from a named city ("24NM E BAKER CITY, OR").
 *    Geocoding either is precisely the fallback-point failure R2 describes.
 *    Real TFR polygons come from the separate faa-national-defense-tfr layer.
 *    (Probing /tfrapi/exportTfrDetail?tfrId=… for per-TFR geometry returns
 *    HTTP 404 — no such path exists.)
 *  - nasstatus identifies airports only by 3-letter FAA code, and the ops plan
 *    is free text. Neither is geocoded here.
 *
 * R3: an empty TFR list / a quiet NAS is an honest empty (return 0); -1 only
 * on a real fetch or parse failure.
 *
 * Licence: US Government work (FAA / FAA ATCSCC), public domain. Keyless.
 * tfr.faa.gov's JSON API is undocumented but stable behind the public site.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define FAA_LICENSE "US Government work (FAA / FAA ATCSCC) — public domain, keyless."

/* ------------------------------------------------------------ faa-tfr-list */
static int run_tfr(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, "https://tfr.faa.gov/tfrapi/exportTfrList", hdrs,
                      "faa-tfr-list");
  if (!body) { fprintf(stderr, "[faa-tfr-list] fetch failed\n"); return -1; }
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc || !cJSON_IsArray(doc)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[faa-tfr-list] unparseable JSON\n");
    return -1;
  }
  int n = 0;
  cJSON *t;
  cJSON_ArrayForEach(t, doc) {
    if (!cJSON_IsObject(t)) continue;
    const char *id   = jo_sv(t, "notam_id");
    const char *type = jo_sv(t, "type");
    const char *desc = jo_sv(t, "description");
    const char *fac  = jo_sv(t, "facility");
    const char *st   = jo_sv(t, "state");
    const char *when = jo_sv(t, "creation_date");
    if (!id) continue;

    cJSON *data = cJSON_CreateObject();
    av_copy(data, t, "notam_id");
    av_copy(data, t, "type");
    av_copy(data, t, "facility");
    av_copy(data, t, "state");
    av_copy(data, t, "description");
    av_copy(data, t, "creation_date");
    cJSON_AddStringToObject(data, "source", "FAA TFR list");
    char *bj = cJSON_PrintUnformatted(data);

    cJSON *props = cJSON_Duplicate(data, 1);
    cJSON_AddStringToObject(props, "record_type", "tfr");
    /* Explicit: the state code and the city named in `description` are NOT
     * turned into a coordinate. */
    cJSON_AddStringToObject(props, "geometry_note",
      "this endpoint publishes no coordinate; state/city text is deliberately "
      "not geocoded — see the faa-national-defense-tfr layer for polygons");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);
    cJSON_Delete(data);

    char title[320];
    snprintf(title, sizeof title, "TFR %s %s%s%s", id, type ? type : "",
             st ? " · " : "", st ? st : "");

    /* creation_date is MM/DD/YYYY; reformat (no value invented). */
    char pub[24] = {0};
    if (when) {
      int mm = 0, dd = 0, yy = 0;
      if (sscanf(when, "%d/%d/%d", &mm, &dd, &yy) == 3 && yy > 1900 &&
          yy <= 9999 && mm >= 1 && mm <= 12 && dd >= 1 && dd <= 31)
        snprintf(pub, sizeof pub, "%04d-%02d-%02d", yy, mm, dd);
    }

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = title;
    it.summary         = desc;
    it.body            = bj;
    it.lang            = "en";
    it.author          = fac;                  /* issuing ARTCC, as returned */
    it.published_at    = pub[0] ? pub : NULL;
    it.record_type     = "tfr";
    it.properties_json = pj;
    it.tags_json       = "[\"aviation\",\"airspace-restriction\",\"tfr\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[faa-tfr-list] emitted %d\n", n);
  return 0;
}

/* -------------------------------------------------------- faa-ops-plan --- */
static int ops_emit_list(const source_ctx *ctx, intel_sink *sink,
                         const cJSON *doc, const char *key, const char *label) {
  const cJSON *arr = cJSON_GetObjectItem(doc, key);
  if (!cJSON_IsArray(arr)) return 0;
  const char *link = jo_sv(doc, "link");
  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    const char *ev = jo_sv(e, "event");
    if (!ev) continue;
    const char *tm = jo_sv(e, "time");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "plan_section", label);
    if (tm) cJSON_AddStringToObject(data, "time", tm);
    cJSON_AddStringToObject(data, "event", ev);
    if (link) cJSON_AddStringToObject(data, "advisory", link);
    cJSON_AddStringToObject(data, "source", "FAA ATCSCC operations plan");
    char *bj = cJSON_PrintUnformatted(data);

    cJSON *props = cJSON_Duplicate(data, 1);
    cJSON_AddStringToObject(props, "record_type", "atcscc-ops-plan");
    cJSON_AddStringToObject(props, "geometry_note",
      "free-text event containing airport codes; deliberately not geocoded");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);
    cJSON_Delete(data);

    char uid[320];
    snprintf(uid, sizeof uid, "%s|%.200s", label, ev);
    char title[320];
    snprintf(title, sizeof title, "%s: %s%s%s", label, tm ? tm : "",
             tm && *tm ? " " : "", ev);

    intel_item it = {0};
    it.remote_key      = uid;
    it.title           = title;
    it.body            = bj;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "atcscc-ops-plan";
    it.properties_json = pj;
    it.tags_json       = "[\"aviation\",\"atc\",\"ops-plan\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  (void)ctx;
  return n;
}

static int run_ops(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, "https://nasstatus.faa.gov/api/operations-plan",
                      hdrs, "faa-ops-plan");
  if (!body) { fprintf(stderr, "[faa-ops-plan] fetch failed\n"); return -1; }
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) { fprintf(stderr, "[faa-ops-plan] unparseable JSON\n"); return -1; }
  int n = 0;
  n += ops_emit_list(ctx, sink, doc, "terminalPlanned", "TERMINAL PLANNED");
  n += ops_emit_list(ctx, sink, doc, "enRoutePlanned", "ENROUTE PLANNED");
  cJSON_Delete(doc);
  fprintf(stderr, "[faa-ops-plan] emitted %d\n", n);
  return 0;
}

/* --------------------------------------------------- faa-airport-status --- *
 * Small bounded XML scan — the document is a flat, fixed vocabulary, so a full
 * parser is not warranted. Every value emitted is text lifted out of the
 * document; nothing is synthesised. */
static void xml_unescape(char *s) {
  char *w = s;
  for (char *p = s; *p; ) {
    if (*p == '&') {
      if      (!strncmp(p, "&amp;", 5))  { *w++ = '&';  p += 5; continue; }
      else if (!strncmp(p, "&lt;", 4))   { *w++ = '<';  p += 4; continue; }
      else if (!strncmp(p, "&gt;", 4))   { *w++ = '>';  p += 4; continue; }
      else if (!strncmp(p, "&quot;", 6)) { *w++ = '"';  p += 6; continue; }
      else if (!strncmp(p, "&apos;", 6)) { *w++ = '\''; p += 6; continue; }
    }
    *w++ = *p++;
  }
  *w = 0;
}

/* Inner text of the first <tag>…</tag> strictly inside [b,e). 1 on success. */
static int xml_field(const char *b, const char *e, const char *tag, char *out,
                     size_t cap) {
  char open[48], close[48];
  snprintf(open, sizeof open, "<%s>", tag);
  snprintf(close, sizeof close, "</%s>", tag);
  const char *o = strstr(b, open);
  if (!o || o >= e) return 0;
  const char *s = o + strlen(open);
  const char *c = strstr(s, close);
  if (!c || c > e) return 0;
  size_t len = (size_t)(c - s);
  if (len >= cap) len = cap - 1;
  memcpy(out, s, len);
  out[len] = 0;
  xml_unescape(out);
  av_rtrim(out);
  return out[0] != 0;
}

/* Emit one NAS status row. All arguments are values read out of the XML. */
static int nas_row(intel_sink *sink, const char *kind, const char *arpt,
                   const char *reason, const char *update, cJSON *extra) {
  if (!arpt || !*arpt) { cJSON_Delete(extra); return 0; }
  cJSON *data = extra ? extra : cJSON_CreateObject();
  cJSON_AddStringToObject(data, "kind", kind);
  cJSON_AddStringToObject(data, "airport_faa_id", arpt);
  if (reason && *reason) cJSON_AddStringToObject(data, "reason", reason);
  if (update && *update) cJSON_AddStringToObject(data, "update_time", update);
  cJSON_AddStringToObject(data, "source", "FAA NAS status (nasstatus.faa.gov)");
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_Duplicate(data, 1);
  cJSON_AddStringToObject(props, "record_type", "nas-airport-status");
  cJSON_AddStringToObject(props, "geometry_note",
    "airports appear only as 3-letter FAA codes; deliberately not geocoded");
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);
  cJSON_Delete(data);

  char uid[96];
  snprintf(uid, sizeof uid, "%s|%s", kind, arpt);
  char title[256];
  snprintf(title, sizeof title, "%s %s%s%s", arpt, kind,
           (reason && *reason) ? " — " : "", (reason && *reason) ? reason : "");

  intel_item it = {0};
  it.remote_key      = uid;
  it.title           = title;
  it.body            = bj;
  it.lang            = "en";
  it.record_type     = "nas-airport-status";
  it.properties_json = pj;
  it.tags_json       = "[\"aviation\",\"atc\",\"delay\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Walk every <elem>…</elem> block in the document and hand it to `fn`. */
static int nas_blocks(intel_sink *sink, const char *xml, const char *elem,
                      const char *kind, const char *update,
                      int (*fn)(intel_sink *, const char *, const char *,
                                const char *, const char *)) {
  char open[48], close[48];
  snprintf(open, sizeof open, "<%s>", elem);
  snprintf(close, sizeof close, "</%s>", elem);
  int n = 0;
  const char *p = xml;
  while ((p = strstr(p, open)) != NULL) {
    const char *b = p + strlen(open);
    const char *e = strstr(b, close);
    if (!e) break;
    n += fn(sink, b, e, kind, update);
    p = e + strlen(close);
  }
  return n;
}

static int nas_ground_delay(intel_sink *sink, const char *b, const char *e,
                            const char *kind, const char *update) {
  char arpt[16] = {0}, reason[256] = {0}, avg[64] = {0}, mx[64] = {0};
  if (!xml_field(b, e, "ARPT", arpt, sizeof arpt)) return 0;
  xml_field(b, e, "Reason", reason, sizeof reason);
  cJSON *x = cJSON_CreateObject();
  if (xml_field(b, e, "Avg", avg, sizeof avg))
    cJSON_AddStringToObject(x, "avg_delay", avg);
  if (xml_field(b, e, "Max", mx, sizeof mx))
    cJSON_AddStringToObject(x, "max_delay", mx);
  return nas_row(sink, kind, arpt, reason, update, x);
}

static int nas_closure(intel_sink *sink, const char *b, const char *e,
                       const char *kind, const char *update) {
  char arpt[16] = {0}, reason[512] = {0}, start[64] = {0}, reopen[64] = {0};
  if (!xml_field(b, e, "ARPT", arpt, sizeof arpt)) return 0;
  xml_field(b, e, "Reason", reason, sizeof reason);
  cJSON *x = cJSON_CreateObject();
  if (xml_field(b, e, "Start", start, sizeof start))
    cJSON_AddStringToObject(x, "closed_from", start);
  if (xml_field(b, e, "Reopen", reopen, sizeof reopen))
    cJSON_AddStringToObject(x, "reopens", reopen);
  return nas_row(sink, kind, arpt, reason, update, x);
}

static int nas_delay(intel_sink *sink, const char *b, const char *e,
                     const char *kind, const char *update) {
  char arpt[16] = {0}, reason[256] = {0}, mn[64] = {0}, mx[64] = {0},
       tr[64] = {0};
  if (!xml_field(b, e, "ARPT", arpt, sizeof arpt)) return 0;
  xml_field(b, e, "Reason", reason, sizeof reason);
  cJSON *x = cJSON_CreateObject();
  if (xml_field(b, e, "Min", mn, sizeof mn))
    cJSON_AddStringToObject(x, "min_delay", mn);
  if (xml_field(b, e, "Max", mx, sizeof mx))
    cJSON_AddStringToObject(x, "max_delay", mx);
  if (xml_field(b, e, "Trend", tr, sizeof tr))
    cJSON_AddStringToObject(x, "trend", tr);
  /* Arrival_Departure carries Type="Arrival|Departure" as an attribute. */
  const char *t = strstr(b, "<Arrival_Departure Type=\"");
  if (t && t < e) {
    t += strlen("<Arrival_Departure Type=\"");
    const char *q = strchr(t, '"');
    if (q && q < e && (size_t)(q - t) < 32) {
      char dir[32];
      snprintf(dir, sizeof dir, "%.*s", (int)(q - t), t);
      cJSON_AddStringToObject(x, "direction", dir);
    }
  }
  return nas_row(sink, kind, arpt, reason, update, x);
}

static int nas_ground_stop(intel_sink *sink, const char *b, const char *e,
                           const char *kind, const char *update) {
  char arpt[16] = {0}, reason[256] = {0}, endt[64] = {0};
  if (!xml_field(b, e, "ARPT", arpt, sizeof arpt)) return 0;
  xml_field(b, e, "Reason", reason, sizeof reason);
  cJSON *x = cJSON_CreateObject();
  if (xml_field(b, e, "End_Time", endt, sizeof endt))
    cJSON_AddStringToObject(x, "end_time", endt);
  return nas_row(sink, kind, arpt, reason, update, x);
}

static int run_status(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/xml", AV_UA_HDR, NULL };
  char *xml = jo_get(ctx, "https://nasstatus.faa.gov/api/airport-status-information",
                     hdrs, "faa-airport-status");
  if (!xml) { fprintf(stderr, "[faa-airport-status] fetch failed\n"); return -1; }
  if (!strstr(xml, "AIRPORT_STATUS_INFORMATION")) {
    free(xml);
    fprintf(stderr, "[faa-airport-status] unexpected document\n");
    return -1;
  }
  char update[64] = {0};
  xml_field(xml, xml + strlen(xml), "Update_Time", update, sizeof update);

  int n = 0;
  n += nas_blocks(sink, xml, "Ground_Delay", "Ground Delay Program", update,
                  nas_ground_delay);
  n += nas_blocks(sink, xml, "Program", "Ground Stop", update, nas_ground_stop);
  n += nas_blocks(sink, xml, "Delay", "Delay", update, nas_delay);
  n += nas_blocks(sink, xml, "Airport", "Airport Closure", update, nas_closure);
  free(xml);
  fprintf(stderr, "[faa-airport-status] emitted %d (update %s)\n", n,
          update[0] ? update : "n/a");
  return 0;
}

static const source_def av_faa_tfr_def = {
  .id = "faa-tfr-list", .collector = "transport",
  .name = "FAA Active Temporary Flight Restrictions",
  .update_interval_sec = 1800, .run = run_tfr,
  .category = "transport", .type = "api",
  .url = "https://tfr.faa.gov/tfrapi/exportTfrList",
  .description = "Every Temporary Flight Restriction currently published by the FAA: NOTAM id, restriction type (HAZARDS, SECURITY, VIP, SPORTS, EMERGENCY), issuing ARTCC, state and the human description with reference point and window. No coordinates are published by this endpoint.",
  .license = FAA_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_faa_tfr_def)

static const source_def av_faa_status_def = {
  .id = "faa-airport-status", .collector = "transport",
  .name = "FAA Airport Delay & Ground-Stop Status",
  .update_interval_sec = 300, .run = run_status,
  .category = "transport", .type = "api",
  .url = "https://nasstatus.faa.gov/api/airport-status-information",
  .description = "Live FAA National Airspace System status: ground delay programs, ground stops, arrival/departure delays with cause and trend, and airport closures — near-real-time signal of weather, runway and staffing disruption across US aviation.",
  .license = FAA_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_faa_status_def)

static const source_def av_faa_ops_def = {
  .id = "faa-ops-plan", .collector = "transport",
  .name = "FAA ATCSCC Daily Operations Plan",
  .update_interval_sec = 900, .run = run_ops,
  .category = "transport", .type = "api",
  .url = "https://nasstatus.faa.gov/api/operations-plan",
  .description = "The FAA Command Center's forward-looking plan for the day — which airports are expected to get ground delay programs, reroutes or stops, and when. A forecast of disruption rather than a report of it.",
  .license = FAA_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_faa_ops_def)
