/* collectors/cyber/sources/npa_cyber_threat_obs.c
 * Port of server/src/collectors/npaCyberThreatObs.js (fetchText scrape).
 * GET observation.html -> 1 feature pin at NPA HQ + 1 intel item.
 *  summary = first <p ...>INNER</p>, tag-stripped, .slice(0,500)  (or null)
 *  graphs  = all <img ... src="X.(png|jpg|svg)"> whose src matches
 *            /observation|access|honey|sensor|detect|port|country/i,
 *            resolved to absolute URL.
 * When fetch fails JS emits 0 features + 0 intel; we emit nothing.
 * Feature uid/title derived by the geojson sink; intel item uid =
 * intelUid(SOURCE_ID,'dashboard') -> remote_key 'dashboard'. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define URL_PAGE "https://www.npa.go.jp/bureau/cyber/koho/observation.html"
#define NPA_HQ_LON 139.7531
#define NPA_HQ_LAT 35.6749

/* slice(0,500) on the stripped text, not splitting a UTF-8 char. */
static char *extract_summary(const char *html) {
  if (!html) return NULL;
  char raw[8192];
  if (!html_tag(html, "p", raw, sizeof raw)) return NULL;   /* no <p> -> null */
  char *txt = html_strip(raw);     /* replace(/<[^>]+>/g,' ').\s+.trim() */
  if (!txt) return NULL;
  size_t cut = strlen(txt);
  if (cut > 500) {
    cut = 500;
    while (cut > 0 && ((unsigned char)txt[cut] & 0xC0) == 0x80) cut--;
    txt[cut] = 0;
  }
  return txt;                       /* may be "" (JS: '' here) */
}

static int ci_has(const char *s, const char *needle) {
  size_t nl = strlen(needle);
  for (; *s; s++) {
    size_t i = 0;
    while (i < nl && s[i] &&
           tolower((unsigned char)s[i]) == tolower((unsigned char)needle[i]))
      i++;
    if (i == nl) return 1;
  }
  return 0;
}

/* /observation|access|honey|sensor|detect|port|country/i */
static int graph_keyword(const char *src) {
  static const char *kw[] = { "observation", "access", "honey", "sensor",
                              "detect", "port", "country", 0 };
  for (int i = 0; kw[i]; i++) if (ci_has(src, kw[i])) return 1;
  return 0;
}

/* JS: /<img[^>]+src="([^"]+\.(?:png|jpg|svg))"/gi */
static void extract_graphs(const char *html, cJSON *arr) {
  if (!html) return;
  const char *s = html;
  for (;;) {
    const char *a = strstr(s, "<img");
    const char *b = strstr(s, "<IMG");
    if (!a && !b) break;
    s = (!a || (b && b < a)) ? b : a;
    /* find a tag boundary; src must occur before the '>' (\[^>]+) */
    const char *gt = strchr(s, '>');
    const char *p = s + 4;
    const char *hit = NULL;
    /* case-insensitive search for src=" within [s+? .. gt) */
    for (const char *q = p; q && *q && (!gt || q < gt); q++) {
      if ((q[0]=='s'||q[0]=='S') && (q[1]=='r'||q[1]=='R') &&
          (q[2]=='c'||q[2]=='C') && q[3]=='=' && q[4]=='"') { hit = q + 5; break; }
    }
    if (!hit) { s = gt ? gt + 1 : s + 4; continue; }
    const char *end = strchr(hit, '"');
    if (!end) break;
    size_t L = (size_t)(end - hit);
    if (L < 4) { s = gt ? gt + 1 : s + 4; continue; }
    char src[1024];
    if (L >= sizeof src) L = sizeof src - 1;
    memcpy(src, hit, L); src[L] = 0;
    /* must end .png/.jpg/.svg */
    size_t sl = strlen(src);
    int ext_ok = sl >= 4 && src[sl-4] == '.' &&
      ((tolower((unsigned char)src[sl-3])=='p' && tolower((unsigned char)src[sl-2])=='n' && tolower((unsigned char)src[sl-1])=='g') ||
       (tolower((unsigned char)src[sl-3])=='j' && tolower((unsigned char)src[sl-2])=='p' && tolower((unsigned char)src[sl-1])=='g') ||
       (tolower((unsigned char)src[sl-3])=='s' && tolower((unsigned char)src[sl-2])=='v' && tolower((unsigned char)src[sl-1])=='g'));
    if (ext_ok && graph_keyword(src)) {
      char full[1280];
      if (strncmp(src, "http", 4) == 0)
        snprintf(full, sizeof full, "%s", src);
      else if (src[0] == '/')
        snprintf(full, sizeof full, "https://www.npa.go.jp%s", src);
      else
        snprintf(full, sizeof full,
                 "https://www.npa.go.jp/bureau/cyber/koho/%s", src);
      cJSON_AddItemToArray(arr, cJSON_CreateString(full));
    }
    s = gt ? gt + 1 : end + 1;
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char iso[32];
  time_t now = time(NULL);
  struct tm tmv; gmtime_r(&now, &tmv);
  strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S.000Z", &tmv);

  char *html = feed_get_text(ctx->http, URL_PAGE, 8000);
  if (!html) return -1;             /* JS: 0 features + 0 intel */

  char *summary = extract_summary(html);   /* NULL or stripped slice */
  cJSON *graphs = cJSON_CreateArray();
  extract_graphs(html, graphs);
  free(html);

  int graph_count = cJSON_GetArraySize(graphs);

  /* ---- feature (geojson sink derives uid/title) ---- */
  cJSON *features = cJSON_CreateArray();
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(NPA_HQ_LON));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(NPA_HQ_LAT));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
  cJSON_AddStringToObject(p, "id", "CYBER_OBS_NPA");
  cJSON_AddStringToObject(p, "name", "NPA Cyber Threat Observation");
  /* summary || '...' (empty string is falsy in JS) */
  cJSON_AddStringToObject(p, "summary",
    (summary && summary[0]) ? summary
    : "\xe8\xad\xa6\xe5\xaf\x9f\xe5\xba\x81\xe3\x82\xb5\xe3\x82\xa4\xe3\x83\x90\xe3\x83\xbc\xe8\xad\xa6\xe5\xaf\x9f\xe5\xb1\x80 \xe4\xb8\x8d\xe5\xae\xa1\xe3\x82\xa2\xe3\x82\xaf\xe3\x82\xbb\xe3\x82\xb9\xe8\xa6\xb3\xe6\xb8\xac\xe3\x83\x80\xe3\x83\x83\xe3\x82\xb7\xe3\x83\xa5\xe3\x83\x9c\xe3\x83\xbc\xe3\x83\x89 (live)");
  cJSON_AddStringToObject(p, "dashboard_url", URL_PAGE);
  cJSON_AddNumberToObject(p, "graph_count", (double)graph_count);
  cJSON_AddStringToObject(p, "source", ctx->source_id);
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(features, f);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);

  /* ---- intel item: uid intelUid(SOURCE_ID,'dashboard') ---- */
  cJSON *ip = cJSON_CreateObject();             /* {dashboard_url, graph_urls} */
  cJSON_AddStringToObject(ip, "dashboard_url", URL_PAGE);
  cJSON_AddItemToObject(ip, "graph_urls", cJSON_Duplicate(graphs, 1));
  char *ipj = cJSON_PrintUnformatted(ip);

  intel_item it = {0};
  it.remote_key = "dashboard";
  it.title = "NPA Cyber Threat Observation Dashboard";
  it.summary = (summary && summary[0]) ? summary
    : "Live darknet / honeypot scan traffic into Japan, refreshed hourly.";
  it.link = URL_PAGE;
  it.lang = "ja";
  it.published_at = iso;
  it.tags_json = "[\"cyber\",\"observation\",\"npa\",\"live\"]";
  it.properties_json = ipj;
  if (sink->emit(sink, &it) >= 0 && n >= 0) n++;

  free(ipj); cJSON_Delete(ip); cJSON_Delete(graphs); free(summary);
  fprintf(stderr, "[npa-cyber-threat-obs] graphs=%d emitted %d\n",
          graph_count, n);
  return n >= 0 ? 0 : -1;
}

static const source_def npa_cyber_threat_obs_def = {
  .id = "npa-cyber-threat-obs", .collector = "cyber",
  .name = "NPA Cyber Threat Observation",
  .name_ja = "\xe8\xad\xa6\xe5\xaf\x9f\xe5\xba\x81 \xe3\x82\xb5\xe3\x82\xa4\xe3\x83\x90\xe3\x83\xbc\xe8\xa6\xb3\xe6\xb8\xac",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(npa_cyber_threat_obs_def)
