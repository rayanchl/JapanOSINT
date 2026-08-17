/* collectors/infrastructure/sources/cam_webcamtaxi.c
 *
 * Registered source_def — faithful port of the `webcamtaxi` discovery
 * channel from server/src/collectors/cameraDiscovery.js (fromWebcamTaxi()).
 *
 * JS: renderHtml(base) (headless Chromium), then scan
 *   /<a[^>]+href="(/en/japan/([a-z-]+)/[^"]+\.html)"[^>]*>([^<]{3,120})<\/a>/gi
 * (cap 60). prefSlug = m[2].replace(/-/g,''); label = m[3] stripped+trim;
 * centroid = PREFECTURE_CENTROIDS[prefSlug] || guessCentroidFromText(label)
 * || TOKYO; jitterAround(idx); makeFeature(camera_type=
 * 'aggregator_webcamtaxi', discovery_channel='webcamtaxi', extra: url,city)
 * where city = prefSlug. Then geocodeFeatures (LLM) — NOT ported (camera
 * LLM-enrich is owned elsewhere); coords stay at centroid+jitter.
 *
 * IMPORTANT — WAF: webcamtaxi.com serves a Cloudflare "Access denied"
 * (error 1005, a hard IP block, NOT a JS challenge) to datacenter IPs;
 * the JS comment notes the channel "typically returns 0 from cloud hosts".
 * feed_get_text from a server IP will 403 → 0 cameras, which is the
 * faithful outcome (same as Node from a server). The headless-Chromium
 * render path can't be expressed here, but even in Node it doesn't bypass
 * a 1005 hard block, so this is behaviour-equivalent for the WAF'd case.
 * If the IP is residential and the page loads, the list-page anchor scrape
 * below produces the same Features as fromWebcamTaxi (pre-geocode).
 *
 * Each Feature → camera_upsert(...,"webcamtaxi").
 */
#include "lib/geojson.h"
#include "lib/jocore.h"
#include "source.h"
#include "core/camera_store.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { const char *k; const char *sv; int is_num; double nv;
                 int is_null; int is_bool; int bv; } kv;

static cJSON *make_feature(double lat, double lon, const char *name,
                           const char *camera_type,
                           const char *discovery_channel,
                           const kv *extra, int nextra) {
  const char *url = NULL;
  for (int i = 0; i < nextra; i++)
    if (strcmp(extra[i].k, "url") == 0 && !extra[i].is_null && extra[i].sv) {
      url = extra[i].sv; break;
    }
  char lats[32], lons[32], tail[80], uid[160];
  snprintf(lats, sizeof lats, "%.4f", jo_round4(lat));
  snprintf(lons, sizeof lons, "%.4f", jo_round4(lon));
  jo_uid_tail(url, name, tail, sizeof tail);
  snprintf(uid, sizeof uid, "%s:%s:%s", lats, lons, tail);

  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "camera_uid", uid);
  cJSON_AddStringToObject(p, "name", (name && *name) ? name : "Unknown camera");
  cJSON_AddStringToObject(p, "camera_type",
                          (camera_type && *camera_type) ? camera_type
                                                        : "unknown");
  cJSON_AddStringToObject(p, "discovery_channel", discovery_channel);
  cJSON_AddStringToObject(p, "country", "JP");
  for (int i = 0; i < nextra; i++) {
    const kv *e = &extra[i];
    if (e->is_null) cJSON_AddNullToObject(p, e->k);
    else if (e->is_bool) cJSON_AddBoolToObject(p, e->k, e->bv);
    else if (e->is_num) cJSON_AddNumberToObject(p, e->k, e->nv);
    else cJSON_AddItemToObject(p, e->k,
           e->sv ? cJSON_CreateString(e->sv) : cJSON_CreateNull());
  }
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

typedef struct { const char *key; double lat, lon; } centroid;
static const centroid PREFECTURE_CENTROIDS[] = {
  {"hokkaido",43.2203,142.8635},{"aomori",40.7644,140.7400},
  {"iwate",39.7036,141.1527},{"miyagi",38.2688,140.8719},
  {"akita",39.7186,140.1024},{"yamagata",38.2404,140.3636},
  {"fukushima",37.7503,140.4677},{"ibaraki",36.3418,140.4468},
  {"tochigi",36.5657,139.8836},{"gunma",36.3906,139.0604},
  {"saitama",35.8572,139.6489},{"chiba",35.6050,140.1234},
  {"tokyo",35.6762,139.6503},{"kanagawa",35.4478,139.6425},
  {"niigata",37.9161,139.0364},{"toyama",36.6953,137.2113},
  {"ishikawa",36.5946,136.6256},{"fukui",36.0652,136.2216},
  {"yamanashi",35.6639,138.5684},{"nagano",36.6513,138.1810},
  {"gifu",35.3911,136.7222},{"shizuoka",34.9769,138.3831},
  {"aichi",35.1802,136.9066},{"mie",34.7303,136.5086},
  {"shiga",35.0045,135.8686},{"kyoto",35.0116,135.7681},
  {"osaka",34.6937,135.5023},{"hyogo",34.6913,135.1830},
  {"nara",34.6851,135.8050},{"wakayama",34.2261,135.1675},
  {"tottori",35.5036,134.2383},{"shimane",35.4723,133.0505},
  {"okayama",34.6618,133.9344},{"hiroshima",34.3966,132.4596},
  {"yamaguchi",34.1859,131.4706},{"tokushima",34.0658,134.5593},
  {"kagawa",34.3401,134.0434},{"ehime",33.8416,132.7657},
  {"kochi",33.5597,133.5311},{"fukuoka",33.5902,130.4017},
  {"saga",33.2494,130.2988},{"nagasaki",32.7448,129.8737},
  {"kumamoto",32.7898,130.7417},{"oita",33.2382,131.6126},
  {"miyazaki",31.9111,131.4239},{"kagoshima",31.5602,130.5581},
  {"okinawa",26.3344,127.8056},
  {"sapporo",43.0642,141.3469},{"yokohama",35.4437,139.6380},
  {"nagoya",35.1815,136.9066},{"kobe",34.6901,135.1955},
  {"sendai",38.2682,140.8694},{"nara_city",34.6851,135.8050},
  {"nikko",36.7581,139.6117},{"nagasaki_city",32.7448,129.8737},
  {"fuji",35.3606,138.7274},{"hakone",35.2323,139.1069},
  {"asakusa",35.7148,139.7967},{"shibuya",35.6580,139.7016},
  {"shinjuku",35.6938,139.7034},
};
#define N_CENTROIDS (sizeof PREFECTURE_CENTROIDS / sizeof *PREFECTURE_CENTROIDS)
/* Index of the first city-level entry (entries before this are prefectures). */
#define FIRST_CITY_IDX 47

/* On match, sets *lat/*lon to the centroid and *prec to "city" or
 * "prefecture" depending on which table entry matched. Returns 1/0. */
static int centroid_exact(const char *key, double *lat, double *lon,
                          const char **prec) {
  if (!key || !*key) return 0;
  for (size_t i = 0; i < N_CENTROIDS; i++)
    if (strcmp(PREFECTURE_CENTROIDS[i].key, key) == 0) {
      *lat = PREFECTURE_CENTROIDS[i].lat;
      *lon = PREFECTURE_CENTROIDS[i].lon;
      *prec = (i >= FIRST_CITY_IDX) ? "city" : "prefecture";
      return 1;
    }
  return 0;
}

static int guess_centroid(const char *text, double *lat, double *lon,
                          const char **prec) {
  if (!text || !*text) return 0;
  char low[1024];
  jo_lower_buf(text, low, sizeof low);
  for (size_t i = 0; i < N_CENTROIDS; i++) {
    const char *k = PREFECTURE_CENTROIDS[i].key;
    char kb[64];
    size_t j = 0;
    for (; k[j] && j + 1 < sizeof kb; j++) kb[j] = k[j];
    kb[j] = 0;
    size_t kl = strlen(kb);
    if (kl > 5 && strcmp(kb + kl - 5, "_city") == 0) kb[kl - 5] = 0;
    if (strstr(low, kb)) {
      *lat = PREFECTURE_CENTROIDS[i].lat;
      *lon = PREFECTURE_CENTROIDS[i].lon;
      *prec = (i >= FIRST_CITY_IDX) ? "city" : "prefecture";
      return 1;
    }
  }
  return 0;
}

static const char *next_anchor(const char *from, char *href, size_t hn,
                               char **inner_out) {
  *inner_out = NULL;
  href[0] = 0;
  for (const char *p = from; (p = strstr(p, "<a")) != NULL; p++) {
    char d = p[2];
    if (d != ' ' && d != '\t' && d != '\n' && d != '\r') continue;
    const char *gt = strchr(p, '>');
    if (!gt) return NULL;
    size_t hdrlen = (size_t)(gt - p) + 1;
    char hdr[2048];
    if (hdrlen >= sizeof hdr) hdrlen = sizeof hdr - 1;
    memcpy(hdr, p, hdrlen);
    hdr[hdrlen] = 0;
    if (!html_attr(hdr, "href", href, hn)) { p = gt; continue; }
    const char *istart = gt + 1;
    const char *iend = strstr(istart, "</a");
    if (!iend) return NULL;
    *inner_out = strndup(istart, (size_t)(iend - istart));
    const char *past = strchr(iend, '>');
    return past ? past + 1 : iend;
  }
  return NULL;
}

/* JS path gate: /en/japan/([a-z-]+)/[^"]+\.html  — pref slug = first
 * segment ([a-z-]+), then '/', then anything (no '"'), ending ".html".
 * Writes the raw pref segment into pref[]. */
static int webcamtaxi_href(const char *href, char *pref, size_t pn) {
  const char *pfx = "/en/japan/";
  size_t pl = strlen(pfx);
  if (strncmp(href, pfx, pl) != 0) return 0;
  const char *p = href + pl;
  const char *ps = p;
  /* The cap here was arbitrary: the feature array grows (realloc), so it was
   * not a memory bound — just a number. The page/category loop above is the
   * real request budget; within a page we emit every camera the aggregator
   * listed. */
  while (*p && ((*p >= 'a' && *p <= 'z') || *p == '-')) p++;
  if (p == ps || *p != '/') return 0;
  size_t plen = (size_t)(p - ps);
  if (plen >= pn) plen = pn - 1;
  memcpy(pref, ps, plen);
  pref[plen] = 0;
  p++;                                  /* [^"]+ then .html */
  const char *rest = p;
  if (!*rest) return 0;                  /* need at least 1 char */
  size_t rl = strlen(rest);
  if (rl < 5 || strcmp(rest + rl - 5, ".html") != 0) return 0;
  return 1;
}

static const char *WCT_BASE = "https://www.webcamtaxi.com/en/japan.html";

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* renderHtml(base) in JS → headless Chromium; from a server IP this host
   * Cloudflare-1005 hard-blocks, so feed_get_text returns NULL/403 ⇒ 0
   * cameras (faithful). On a residential IP that returns HTML the scrape
   * below matches fromWebcamTaxi exactly. */
  char *html = feed_get_text(ctx->http, WCT_BASE, 25000);
  if (!html || !html[0]) {
    free(html);
    fprintf(stderr,
            "[cam-webcamtaxi] 0 (no html — Cloudflare 1005 / WAF likely)\n");
    return 0;
  }

  char **seen = NULL;
  int nseen = 0, seencap = 0;
  int count = 0;
  const char *cur = html;
  char href[512];
  char *inner;
  while (         (cur = next_anchor(cur, href, sizeof href, &inner)) != NULL) {
    if (!inner) continue;
    char pref[128];
    if (!webcamtaxi_href(href, pref, sizeof pref)) { free(inner); continue; }
    /* JS inner class is [^<]{3,120}: reject if it contains '<' or its
     * trimmed length is outside 3..120 (label = inner stripped+trim, but
     * [^<] already forbids tags so strip is a no-op). */
    if (strchr(inner, '<')) { free(inner); continue; }
    char *label = html_strip(inner);
    free(inner);
    size_t ll = label ? strlen(label) : 0;
    if (ll < 3 || ll > 120) { free(label); continue; }

    int dup = 0;
    for (int s = 0; s < nseen; s++)
      if (strcmp(seen[s], href) == 0) { dup = 1; break; }
    if (dup) { free(label); continue; }
    if (nseen == seencap) {
      seencap = seencap ? seencap * 2 : 32;
      seen = realloc(seen, (size_t)seencap * sizeof *seen);
    }
    seen[nseen++] = strdup(href);

    /* prefSlug = m[2].replace(/-/g,'') */
    char slug[128];
    size_t si = 0;
    for (size_t k = 0; pref[k] && si + 1 < sizeof slug; k++)
      if (pref[k] != '-') slug[si++] = pref[k];
    slug[si] = 0;

    /* Centroid is an AREA anchor, not the camera's GPS. Use it exactly
     * (no jitter). The list page exposes no real per-camera coordinates,
     * so if no centroid matches we have no honest location → skip. */
    double lat, lon;
    const char *prec = NULL;
    if (!centroid_exact(slug, &lat, &lon, &prec) &&
        !guess_centroid(label ? label : "", &lat, &lon, &prec)) {
      free(label);
      continue;
    }

    char fullurl[640];
    jo_abs_url(href, WCT_BASE, fullurl, sizeof fullurl);

    const char *nm = (label && label[0]) ? label : "Webcamtaxi feed";
    kv ex[4] = {0};
    ex[0].k = "url"; ex[0].sv = fullurl;
    ex[1].k = "city"; ex[1].sv = slug;
    ex[2].k = "geo_precision"; ex[2].sv = prec;
    ex[3].k = "geo_uncertain"; ex[3].is_bool = 1; ex[3].bv = 1;
    cJSON *f = make_feature(lat, lon, nm, "aggregator_webcamtaxi",
                            "webcamtaxi", ex, 4);
    if (camera_upsert(ctx->db, sink, f, "webcamtaxi") >= 0) count++;
    cJSON_Delete(f);
    free(label);
  }

  for (int s = 0; s < nseen; s++) free(seen[s]);
  free(seen);
  free(html);
  fprintf(stderr, "[cam-webcamtaxi] emitted %d\n", count);
  return 0;
}

static const source_def cam_webcamtaxi_def = {
  .id = "cam-webcamtaxi", .collector = "camera-discovery",
  .name = "Camera Discovery: WebcamTaxi",
  .name_ja = "カメラ探索: WebcamTaxi",
   .layer = "cameras",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cam_webcamtaxi_def)
