/* collectors/sources/tsp_pskreporter_callsign.c
 * OSINT service — PSKREPORTER_CALLSIGN. On-demand entity pivot: given a
 * callsign (entity_type "callsign"), return who heard it and from where over
 * the last window. Answers "where in the world is this station being received
 * right now".
 * Endpoint: https://retrieve.pskreporter.info/query?senderCallsign=<CALL>
 *   &flowStartSeconds=-900&rronly=1   (keyless XML)
 * Emits one row per receptionReport: receiver callsign and locator, sender
 *   callsign and locator, exact received frequency in Hz, mode and SNR.
 *
 * parse_notes honoured, quoted:
 *  - "XML (not JSON) ... Data is attribute-based, not element text": the
 *    scanner below reads attributes off each <receptionReport .../> element.
 *  - "flowStartSeconds as a NEGATIVE number of seconds back from now (max
 *    -86400), optional rronly=1 to drop the activeReceiver block and cut the
 *    payload from ~1.5 MB to a few KB — use that": rronly=1 is used.
 *  - "Geo comes from 'locator' Maidenhead strings (4, 6 or 8 characters);
 *    decode to the grid-square centre and set
 *    properties.geo_precision='maidenhead-grid'": maiden() below returns the
 *    CENTRE of the square at the precision actually supplied. R2 holds because
 *    the locator is an upstream-published position and its coarseness is
 *    declared; a report with no decodable locator gets no geometry.
 *  - "Rate limit: one query per callsign per 5 minutes minimum. ... abusive
 *    polling gets blocked. Honour that or do not wire it up": this source is
 *    on-demand only (update_interval_sec = 0, never scheduled), it issues
 *    exactly ONE request per pivot, and it identifies itself with a
 *    descriptive User-Agent as the operator asks.
 * A NULL, empty or non-callsign-shaped entity returns 0 without any request.
 * Licence: PSKReporter retrieve API — keyless, subject to the operator's
 *   polling and identification requests noted above.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PSK_WINDOW_SEC 900
#define MAX_ROWS 800

/* Amateur callsigns: alnum plus '/' and '-', 3..16 chars, must contain a digit
 * and a letter. Anything else is not a callsign — honest miss. */
static int normalise_call(const char *in, char *out, size_t cap) {
  if (!in || !*in) return 0;
  size_t n = strlen(in);
  if (n < 3 || n > 15 || n + 1 > cap) return 0;
  int has_d = 0, has_a = 0;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == ' ') continue;
    if (c == '/' || c == '-') { out[j++] = (char)c; continue; }
    if (!isalnum(c)) return 0;
    if (isdigit(c)) has_d = 1; else has_a = 1;
    out[j++] = (char)toupper(c);
  }
  out[j] = '\0';
  return (j >= 3 && has_d && has_a);
}

static char *urlenc(const char *s) {
  size_t n = strlen(s);
  char *out = (char *)malloc(n * 3 + 1);
  if (!out) return NULL;
  static const char hx[] = "0123456789ABCDEF";
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[j++] = (char)c;
    else { out[j++] = '%'; out[j++] = hx[c >> 4]; out[j++] = hx[c & 15]; }
  }
  out[j] = '\0';
  return out;
}

/* Maidenhead locator -> CENTRE of the square, at the precision supplied. */
static int maiden(const char *loc, double *lat, double *lon) {
  if (!loc) return 0;
  size_t n = strlen(loc);
  if (n < 4) return 0;
  int A = toupper((unsigned char)loc[0]) - 'A';
  int B = toupper((unsigned char)loc[1]) - 'A';
  int C = loc[2] - '0', D = loc[3] - '0';
  if (A < 0 || A > 17 || B < 0 || B > 17 || C < 0 || C > 9 || D < 0 || D > 9)
    return 0;
  double lo = A * 20.0 - 180.0 + C * 2.0;
  double la = B * 10.0 - 90.0 + D * 1.0;
  double losz = 2.0, lasz = 1.0;
  if (n >= 6) {
    int E = toupper((unsigned char)loc[4]) - 'A';
    int F = toupper((unsigned char)loc[5]) - 'A';
    if (E >= 0 && E < 24 && F >= 0 && F < 24) {
      lo += E * (2.0 / 24.0);
      la += F * (1.0 / 24.0);
      losz = 2.0 / 24.0; lasz = 1.0 / 24.0;
      if (n >= 8 && isdigit((unsigned char)loc[6]) && isdigit((unsigned char)loc[7])) {
        lo += (loc[6] - '0') * (losz / 10.0);
        la += (loc[7] - '0') * (lasz / 10.0);
        losz /= 10.0; lasz /= 10.0;
      }
    }
  }
  *lon = lo + losz / 2.0;
  *lat = la + lasz / 2.0;
  return (*lat >= -90.0 && *lat <= 90.0 && *lon >= -180.0 && *lon <= 180.0);
}

/* Read attribute `name` from the XML element text [el, end). */
static int xml_attr(const char *el, const char *end, const char *name,
                    char *out, size_t cap) {
  char pat[64];
  snprintf(pat, sizeof pat, "%s=\"", name);
  const char *p = strstr(el, pat);
  if (!p || p >= end) { out[0] = '\0'; return 0; }
  p += strlen(pat);
  const char *q = strchr(p, '"');
  if (!q || q > end) { out[0] = '\0'; return 0; }
  size_t len = (size_t)(q - p);
  if (len >= cap) len = cap - 1;
  memcpy(out, p, len);
  out[len] = '\0';
  return len > 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char call[32];
  if (!normalise_call(ctx->entity, call, sizeof call)) return 0; /* honest miss */

  char *enc = urlenc(call);
  if (!enc) return -1;
  char url[320];
  snprintf(url, sizeof url,
           "https://retrieve.pskreporter.info/query?senderCallsign=%s"
           "&flowStartSeconds=-%d&rronly=1", enc, PSK_WINDOW_SEC);
  free(enc);

  const char *hdrs[] = {
    "User-Agent: JapanOSINT/1.0 (OSINT collector; one query per pivot)",
    "Accept: application/xml", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 30000, 0, &hr);
  if (rc != 0) {
    fprintf(stderr, "[PSKREPORTER_CALLSIGN] transport error for %s\n", call);
    http_response_free(&hr);
    return -1;
  }
  if (hr.status != 200 || !hr.body) {   /* throttled or unknown: honest empty */
    fprintf(stderr, "[PSKREPORTER_CALLSIGN] %s http status=%ld\n", call, hr.status);
    http_response_free(&hr);
    return 0;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);

  int n = 0;
  const char *p = body, *s;
  while (n < MAX_ROWS && (s = strstr(p, "<receptionReport")) != NULL) {
    const char *e = strchr(s, '>');
    if (!e) break;
    p = e + 1;

    char rxc[32], rxl[16], txc[32], txl[16], freq[32], mode[24], snr[16], flow[24];
    xml_attr(s, e, "receiverCallsign", rxc, sizeof rxc);
    xml_attr(s, e, "receiverLocator", rxl, sizeof rxl);
    xml_attr(s, e, "senderCallsign", txc, sizeof txc);
    xml_attr(s, e, "senderLocator", txl, sizeof txl);
    xml_attr(s, e, "frequency", freq, sizeof freq);
    xml_attr(s, e, "mode", mode, sizeof mode);
    xml_attr(s, e, "sNR", snr, sizeof snr);
    xml_attr(s, e, "flowStartSeconds", flow, sizeof flow);
    if (!rxc[0]) continue;                        /* no receiver -> no row */

    double lat = 0, lon = 0;
    int has_geo = rxl[0] ? maiden(rxl, &lat, &lon) : 0;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "service", "PSKREPORTER_CALLSIGN");
    cJSON_AddStringToObject(pr, "query", call);
    cJSON_AddStringToObject(pr, "receiver_callsign", rxc);
    if (rxl[0])  cJSON_AddStringToObject(pr, "receiver_locator", rxl);
    if (txc[0])  cJSON_AddStringToObject(pr, "sender_callsign", txc);
    if (txl[0])  cJSON_AddStringToObject(pr, "sender_locator", txl);
    if (mode[0]) cJSON_AddStringToObject(pr, "mode", mode);
    if (freq[0]) {
      char *end = NULL;
      double hz = strtod(freq, &end);
      if (end && end != freq) cJSON_AddNumberToObject(pr, "frequency_hz", hz);
      else cJSON_AddStringToObject(pr, "frequency", freq);
    }
    if (snr[0]) {
      char *end = NULL;
      double v = strtod(snr, &end);
      if (end && end != snr) cJSON_AddNumberToObject(pr, "snr_db", v);
    }
    if (flow[0]) cJSON_AddStringToObject(pr, "flow_start_seconds", flow);
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "latitude", lat);
      cJSON_AddNumberToObject(pr, "longitude", lon);
      cJSON_AddStringToObject(pr, "geo_precision", "maidenhead-grid");
      cJSON_AddStringToObject(pr, "geo_subject",
        "receiving station's grid square centre");
    }
    cJSON_AddNumberToObject(pr, "window_seconds", PSK_WINDOW_SEC);
    cJSON_AddStringToObject(pr, "source", "PSKReporter retrieve API");
    cJSON_AddBoolToObject(pr, "success", 1);
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[224], summary[224], key[128];
    snprintf(key, sizeof key, "%s|%s|%s|%s", txc[0] ? txc : call, rxc,
             freq, flow);
    if (freq[0]) {
      char *end = NULL;
      double hz = strtod(freq, &end);
      if (end && end != freq)
        snprintf(title, sizeof title, "%s heard by %s on %.6f MHz",
                 txc[0] ? txc : call, rxc, hz / 1e6);
      else
        snprintf(title, sizeof title, "%s heard by %s",
                 txc[0] ? txc : call, rxc);
    } else {
      snprintf(title, sizeof title, "%s heard by %s", txc[0] ? txc : call, rxc);
    }
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
             mode[0] ? mode : "mode n/a",
             snr[0] ? " · SNR " : "", snr[0] ? snr : "", snr[0] ? " dB" : "",
             rxl[0] ? " · grid " : "", rxl[0] ? rxl : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://pskreporter.info/pskmap.html";
    it.lang            = "en";
    it.record_type     = "reception-report";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"amateur-radio\",\"pskreporter\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[PSKREPORTER_CALLSIGN] emitted %d for %s\n", n, call);
  return 0;              /* nobody heard them in the window is not an error */
}

static const source_def tsp_pskreporter_callsign_def = {
  .id = "PSKREPORTER_CALLSIGN", .collector = "osint",
  .name = "PSKReporter reception reports by callsign",
  .update_interval_sec = 0, .run = run,
  .category = "telecom", .type = "api",
  .url = "https://retrieve.pskreporter.info/query",
  .description = "Given a callsign, returns who heard it and from where in the last 15 minutes, with the receiver's Maidenhead grid, the exact received frequency, mode and SNR.",
  .license = "PSKReporter retrieve API — keyless; the operator asks that a given query be repeated no more often than every 5 minutes and that clients identify themselves. This source is on-demand only and issues one request per pivot.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(tsp_pskreporter_callsign_def)
