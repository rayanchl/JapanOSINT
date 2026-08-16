/* collectors/sources/cyi_stopforumspam_ip.c
 * OSINT service — STOPFORUMSPAM_IP. IP pivot giving keyless abuse reputation
 * with a real frequency count, last-seen date, a Tor-exit flag and a confidence
 * score — the AbuseIPDB question answered without AbuseIPDB's API key.
 * Endpoint: https://api.stopforumspam.org/api?ip=<ip>&json            (keyless)
 * TRAPS (parse_notes): "Append the bare '&json' flag or you get XML back" — the
 * flag is appended with no value. "appears=0 is a genuine 'not listed' answer,
 * not an error — emit the negative result or nothing, but return 0 (R3)." The
 * negative result IS emitted here (a clean IP is a real finding) and the run
 * still returns 0. The &email=/&username= parameters are deliberately not used.
 * Emits ONE row: ip, appears, frequency, lastseen, torexit, asn, country,
 * confidence. StopForumSpam returns no coordinates -> has_geo 0 (R2).
 * Licence: StopForumSpam free API; CC BY-SA data, courtesy rate limits apply.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int looks_like_ip(const char *s) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 45) return 0;
  if (strchr(s, '/')) return 0;
  if (strchr(s, ':')) {
    for (const char *q = s; *q; q++)
      if (!(isxdigit((unsigned char)*q) || *q == ':')) return 0;
    return 1;
  }
  int parts = 0;
  const char *p = s;
  while (*p) {
    int v = 0, d = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; d++; if (v > 255) return 0; }
    if (!d || d > 3) return 0;
    parts++;
    if (*p == '.') { p++; if (!*p) return 0; }
    else if (*p) return 0;
  }
  return parts == 4;
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_ip(q)) return 0;                 /* wrong shape -> no-op */

  char url[160];
  snprintf(url, sizeof url, "https://api.stopforumspam.org/api?ip=%s&json", q);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[STOPFORUMSPAM_IP] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[STOPFORUMSPAM_IP] unparseable body\n"); return -1; }

  const cJSON *ipo = cJSON_GetObjectItem(root, "ip");
  if (!cJSON_IsObject(ipo)) {
    cJSON_Delete(root);
    fprintf(stderr, "[STOPFORUMSPAM_IP] no ip object for %s\n", q);
    return 0;
  }

  double appears = 0, frequency = 0, confidence = 0, asn = 0, torexit = 0;
  int ha = jo_numf(ipo, "appears", &appears);
  int hf = jo_numf(ipo, "frequency", &frequency);
  int hc = jo_numf(ipo, "confidence", &confidence);
  int hn = jo_numf(ipo, "asn", &asn);
  int ht = jo_numf(ipo, "torexit", &torexit);
  const char *lastseen = jo_sv(ipo, "lastseen");
  const char *country = jo_sv(ipo, "country");
  const char *value = jo_sv(ipo, "value");

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "STOPFORUMSPAM_IP");
  cJSON_AddStringToObject(p, "ip", value ? value : q);
  if (ha) cJSON_AddBoolToObject(p, "listed", appears > 0);
  if (hf) cJSON_AddNumberToObject(p, "frequency", frequency);
  if (hc) cJSON_AddNumberToObject(p, "confidence", confidence);
  if (hn) cJSON_AddNumberToObject(p, "asn", asn);
  if (ht) cJSON_AddBoolToObject(p, "tor_exit", torexit > 0);
  if (lastseen) cJSON_AddStringToObject(p, "last_seen", lastseen);
  if (country)  cJSON_AddStringToObject(p, "country", country);
  const cJSON *ok = cJSON_GetObjectItem(root, "success");
  if (cJSON_IsNumber(ok)) cJSON_AddNumberToObject(p, "api_success", ok->valuedouble);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char summary[320];
  if (ha && appears > 0)
    snprintf(summary, sizeof summary,
             "listed · %.0f reports · confidence %.2f%s%s%s",
             frequency, confidence,
             lastseen ? " · last seen " : "", lastseen ? lastseen : "",
             (ht && torexit > 0) ? " · Tor exit" : "");
  else
    snprintf(summary, sizeof summary,
             "not listed on StopForumSpam (clean lookup)%s%s",
             country ? " · " : "", country ? country : "");

  /* ISO-8601 for the intel envelope; upstream gives "YYYY-MM-DD HH:MM:SS" */
  char iso[40] = "";
  if (lastseen && strlen(lastseen) >= 19 && lastseen[10] == ' ')
    snprintf(iso, sizeof iso, "%.10sT%.8sZ", lastseen, lastseen + 11);

  intel_item it = {0};
  it.remote_key      = value ? value : q;
  it.title           = value ? value : q;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.published_at    = iso[0] ? iso : NULL;
  it.record_type     = "ip-reputation";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"ip\",\"reputation\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[STOPFORUMSPAM_IP] emitted 1 (%s, appears=%.0f)\n", q, appears);
  return 0;                        /* appears=0 is a clean result, not an error */
}

static const source_def cyi_stopforumspam_ip_def = {
  .id = "STOPFORUMSPAM_IP", .collector = "osint",
  .name = "StopForumSpam IP reputation",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.stopforumspam.org/api",
  .description = "Keyless abuse reputation with a real frequency count, last-seen date, Tor-exit flag and confidence score — the AbuseIPDB question without the key.",
  .license = "StopForumSpam free API; CC BY-SA data, courtesy rate limits apply.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_stopforumspam_ip_def)
