/* collectors/infrastructure/sources/cam_webcamera24.c
 *
 * Registered source_def — faithful port of the `webcamera24` discovery
 * channel from server/src/collectors/cameraDiscovery.js (fromWebcamera24()).
 *
 * JS: fetch base + ?page=2..6 (plain fetchText, browser UA), join('\n'),
 * scan /<a[^>]+href="(/fr/camera/japan/[a-z0-9-]+/?)"[^>]*>([\s\S]*?)<\/a>/gi
 * (dedupe by href, cap 200), label=html_strip(inner) (fallback
 * 'Webcamera24 feed'), centroid = guessCentroidFromText(label) || TOKYO,
 * jitterAround(idx), makeFeature(camera_type='aggregator_webcamera24',
 * discovery_channel='webcamera24', extra: url).
 *
 * NOT ported (post-list passes): upgradeYouTubeStreamUrls(features,4) (a
 * concurrent per-detail-page YouTube-iframe resolver that rewrites url and
 * re-keys camera_uid) and geocodeFeatures (LLM). Per the porting rule the
 * per-detail-page concurrency can't be faithfully expressed with
 * htmlparse+sequential, so the list-page scrape is ported and url/coords
 * stay exactly as fromWebcamera24 produces them pre-upgrade (identical to an
 * unported upstream — no fabrication).
 *
 * Each Feature → camera_upsert(...,"webcamera24").
 */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { const char *k; const char *sv; int is_num; double nv;
                 int is_null; int is_bool; int bv; } kv;

static double round4(double v) { return floor(v * 1e4 + 0.5) / 1e4; }

static void uid_tail(const char *url, const char *name, char *out,
                     size_t outsz) {
  const char *src = (url && *url) ? url : (name ? name : "");
  size_t i = 0;
  for (; src[i] && i < 60 && i + 1 < outsz; i++) {
    unsigned char c = (unsigned char)src[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  out[i] = 0;
}

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
  snprintf(lats, sizeof lats, "%.4f", round4(lat));
  snprintf(lons, sizeof lons, "%.4f", round4(lon));
  uid_tail(url, name, tail, sizeof tail);
  snprintf(uid, sizeof uid, "%s:%s:%s", lats, lons, tail);

  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

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

static void to_lower_buf(const char *in, char *out, size_t n) {
  size_t i = 0;
  for (; in && in[i] && i + 1 < n; i++) {
    unsigned char c = (unsigned char)in[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  out[i] = 0;
}

/* The first 47 entries are prefectures; the rest are city/locality anchors. */
#define N_PREFECTURES 47

/* Returns 1 on match, setting *lat/*lon to the exact centroid and *precision
 * to "prefecture" or "city". No jitter — the centroid is an honest area anchor,
 * not the camera's GPS. */
static int guess_centroid(const char *text, double *lat, double *lon,
                          const char **precision) {
  if (!text || !*text) return 0;
  char low[1024];
  to_lower_buf(text, low, sizeof low);
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
      *precision = (i < N_PREFECTURES) ? "prefecture" : "city";
      return 1;
    }
  }
  return 0;
}

static void abs_url(const char *href, const char *base, char *out, size_t n) {
  if (href && (strncmp(href, "http://", 7) == 0 ||
                strncmp(href, "https://", 8) == 0)) {
    snprintf(out, n, "%s", href);
    return;
  }
  const char *p = base;
  int slashes = 0;
  while (*p) { if (*p == '/') { slashes++; if (slashes == 3) break; } p++; }
  size_t hostlen = (size_t)(p - base);
  if (href && href[0] == '/')
    snprintf(out, n, "%.*s%s", (int)hostlen, base, href);
  else
    snprintf(out, n, "%.*s/%s", (int)hostlen, base, href ? href : "");
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

/* JS path gate: /fr/camera/japan/[a-z0-9-]+/?  (no extra segments/query) */
static int href_path_ok(const char *href) {
  const char *pfx = "/fr/camera/japan/";
  size_t pl = strlen(pfx);
  if (strncmp(href, pfx, pl) != 0) return 0;
  const char *p = href + pl;
  int n = 0;
  while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                *p == '-')) { p++; n++; }
  if (n == 0) return 0;
  if (*p == '/') p++;
  return *p == 0;
}

static const char *WC24_BASE = "https://webcamera24.com/fr/countries/japan/";

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* pageUrls = [base, ...[2,3,4,5,6].map(p => base?page=p)] */
  const char *urls[6];
  char ub[5][256];
  urls[0] = WC24_BASE;
  for (int p = 2; p <= 6; p++) {
    snprintf(ub[p - 2], sizeof ub[p - 2], "%s?page=%d", WC24_BASE, p);
    urls[p - 1] = ub[p - 2];
  }

  char *htmls[6] = {0};
  size_t total = 1;
  int any = 0;
  for (int i = 0; i < 6; i++) {
    htmls[i] = feed_get_text(ctx->http, urls[i], 8000);
    if (htmls[i] && htmls[i][0]) { total += strlen(htmls[i]) + 1; any = 1; }
  }
  if (!any) {
    for (int i = 0; i < 6; i++) free(htmls[i]);
    fprintf(stderr, "[cam-webcamera24] 0 (no html — likely WAF/empty)\n");
    return 0;
  }
  char *html = malloc(total);
  if (!html) { for (int i = 0; i < 6; i++) free(htmls[i]); return -1; }
  html[0] = 0;
  for (int i = 0; i < 6; i++) {
    if (htmls[i] && htmls[i][0]) {
      if (html[0]) strcat(html, "\n");
      strcat(html, htmls[i]);
    }
    free(htmls[i]);
  }

  char **seen = NULL;
  int nseen = 0, seencap = 0;
  int count = 0;
  const char *cur = html;
  char href[512];
  char *inner;
  while (count < 200 &&
         (cur = next_anchor(cur, href, sizeof href, &inner)) != NULL) {
    if (!inner) continue;
    if (!href_path_ok(href)) { free(inner); continue; }

    int dup = 0;
    for (int s = 0; s < nseen; s++)
      if (strcmp(seen[s], href) == 0) { dup = 1; break; }
    if (dup) { free(inner); continue; }
    if (nseen == seencap) {
      seencap = seencap ? seencap * 2 : 32;
      seen = realloc(seen, (size_t)seencap * sizeof *seen);
    }
    seen[nseen++] = strdup(href);

    char *label = html_strip(inner);
    free(inner);

    /* NO-FABRICATION: webcamera24 list pages carry no coordinates. Only emit
     * when a location can be derived from the camera's own real name text; the
     * blind TOKYO fallback (zero locational signal) would invent a point, so
     * skip those rather than seed a fake marker. The matched centroid is an
     * area anchor (prefecture/city), emitted exactly — no jitter — and flagged
     * approximate so consumers know it is not the camera's GPS. */
    double lat, lon;
    const char *precision = NULL;
    if (!guess_centroid(label ? label : "", &lat, &lon, &precision)) {
      free(label);
      continue;
    }

    char fullurl[640];
    abs_url(href, WC24_BASE, fullurl, sizeof fullurl);

    const char *nm = (label && label[0]) ? label : "Webcamera24 feed";
    kv ex[3] = {0};
    ex[0].k = "url"; ex[0].sv = fullurl;
    ex[1].k = "location_precision"; ex[1].sv = precision;
    ex[2].k = "location_approximate"; ex[2].is_bool = 1; ex[2].bv = 1;
    cJSON *f = make_feature(lat, lon, nm, "aggregator_webcamera24",
                            "webcamera24", ex, 3);
    if (camera_upsert(ctx->db, sink, f, "webcamera24") >= 0) count++;
    cJSON_Delete(f);
    free(label);
  }

  for (int s = 0; s < nseen; s++) free(seen[s]);
  free(seen);
  free(html);
  fprintf(stderr, "[cam-webcamera24] emitted %d\n", count);
  return 0;
}

static const source_def cam_webcamera24_def = {
  .id = "cam-webcamera24", .collector = "infrastructure",
  .name = "Camera Discovery: Webcamera24",
  .name_ja = "カメラ探索: Webcamera24",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_webcamera24_def)
