/* collectors/infrastructure/sources/cam_skylinewebcams.c
 *
 * Registered source_def — faithful port of the `skylinewebcams` discovery
 * channel from server/src/collectors/cameraDiscovery.js (fromSkyline()).
 *
 * JS: renderHtml(base,{acceptCookies:true,scrollPasses:6}) (headless
 * Chromium clicks "Accept all" + scrolls to force lazy-load), then scan
 *   /href="((?:\/|)?fr\/webcam\/japan\/[^"#?\s]+\.html)"[^>]*>([\s\S]*?)<\/a>/gi
 * Dedup by path; path = m[1].startsWith('/') ? m[1] : '/'+m[1]; label =
 * m[2] stripped+trim. parts = path.split('/').filter(Boolean);
 * citySlug = (parts[4]||parts[3]||'').replace(/-prefecture$/,'')
 *            .replace(/-/g,''); centroid = PREFECTURE_CENTROIDS[citySlug]
 * || guessCentroidFromText(label) || TOKYO; jitterAround(idx);
 * makeFeature(camera_type='aggregator_skyline',discovery_channel=
 * 'skylinewebcams', url=`https://www.skylinewebcams.com${path}`,
 * extra: url,city). Then upgradeYouTubeStreamUrls(features,4) +
 * geocodeFeatures.
 *
 * LIMITATION: the JS index sits behind a Funding-Choices consent wall and
 * the camera grid is lazy-loaded — fromSkyline relies on a headless
 * Chromium that clicks "Accept all" and scrolls 6× to materialise the
 * anchors. feed_get_text returns only the server-rendered (consent-walled,
 * pre-lazy-load) HTML; with no JS engine the lazy grid will usually not be
 * present, so this channel typically surfaces 0 cameras from this IP — the
 * same faithful "0 like an unported upstream" outcome. Any anchors that ARE
 * server-rendered are scraped exactly as fromSkyline does (pre-upgrade /
 * pre-geocode). The upgradeYouTubeStreamUrls concurrent detail-worker and
 * the LLM geocodeFeatures pass are out of scope per the porting rule
 * (per-detail-page concurrency / real-regex engine) and are NOT faked.
 *
 * Each Feature → camera_upsert(...,"skylinewebcams").
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
                 int is_null; } kv;

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
    else if (e->is_num) cJSON_AddNumberToObject(p, e->k, e->nv);
    else cJSON_AddItemToObject(p, e->k,
           e->sv ? cJSON_CreateString(e->sv) : cJSON_CreateNull());
  }
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

/* prec: "prefecture" for the 47 prefecture rows, "city" for the named
 * city/landmark anchors below them — emitted so the centroid point is
 * honestly flagged as an area, not the camera's GPS. */
typedef struct { const char *key; double lat, lon; const char *prec; } centroid;
static const centroid PREFECTURE_CENTROIDS[] = {
  {"hokkaido",43.2203,142.8635,"prefecture"},{"aomori",40.7644,140.7400,"prefecture"},
  {"iwate",39.7036,141.1527,"prefecture"},{"miyagi",38.2688,140.8719,"prefecture"},
  {"akita",39.7186,140.1024,"prefecture"},{"yamagata",38.2404,140.3636,"prefecture"},
  {"fukushima",37.7503,140.4677,"prefecture"},{"ibaraki",36.3418,140.4468,"prefecture"},
  {"tochigi",36.5657,139.8836,"prefecture"},{"gunma",36.3906,139.0604,"prefecture"},
  {"saitama",35.8572,139.6489,"prefecture"},{"chiba",35.6050,140.1234,"prefecture"},
  {"tokyo",35.6762,139.6503,"prefecture"},{"kanagawa",35.4478,139.6425,"prefecture"},
  {"niigata",37.9161,139.0364,"prefecture"},{"toyama",36.6953,137.2113,"prefecture"},
  {"ishikawa",36.5946,136.6256,"prefecture"},{"fukui",36.0652,136.2216,"prefecture"},
  {"yamanashi",35.6639,138.5684,"prefecture"},{"nagano",36.6513,138.1810,"prefecture"},
  {"gifu",35.3911,136.7222,"prefecture"},{"shizuoka",34.9769,138.3831,"prefecture"},
  {"aichi",35.1802,136.9066,"prefecture"},{"mie",34.7303,136.5086,"prefecture"},
  {"shiga",35.0045,135.8686,"prefecture"},{"kyoto",35.0116,135.7681,"prefecture"},
  {"osaka",34.6937,135.5023,"prefecture"},{"hyogo",34.6913,135.1830,"prefecture"},
  {"nara",34.6851,135.8050,"prefecture"},{"wakayama",34.2261,135.1675,"prefecture"},
  {"tottori",35.5036,134.2383,"prefecture"},{"shimane",35.4723,133.0505,"prefecture"},
  {"okayama",34.6618,133.9344,"prefecture"},{"hiroshima",34.3966,132.4596,"prefecture"},
  {"yamaguchi",34.1859,131.4706,"prefecture"},{"tokushima",34.0658,134.5593,"prefecture"},
  {"kagawa",34.3401,134.0434,"prefecture"},{"ehime",33.8416,132.7657,"prefecture"},
  {"kochi",33.5597,133.5311,"prefecture"},{"fukuoka",33.5902,130.4017,"prefecture"},
  {"saga",33.2494,130.2988,"prefecture"},{"nagasaki",32.7448,129.8737,"prefecture"},
  {"kumamoto",32.7898,130.7417,"prefecture"},{"oita",33.2382,131.6126,"prefecture"},
  {"miyazaki",31.9111,131.4239,"prefecture"},{"kagoshima",31.5602,130.5581,"prefecture"},
  {"okinawa",26.3344,127.8056,"prefecture"},
  {"sapporo",43.0642,141.3469,"city"},{"yokohama",35.4437,139.6380,"city"},
  {"nagoya",35.1815,136.9066,"city"},{"kobe",34.6901,135.1955,"city"},
  {"sendai",38.2682,140.8694,"city"},{"nara_city",34.6851,135.8050,"city"},
  {"nikko",36.7581,139.6117,"city"},{"nagasaki_city",32.7448,129.8737,"city"},
  {"fuji",35.3606,138.7274,"city"},{"hakone",35.2323,139.1069,"city"},
  {"asakusa",35.7148,139.7967,"city"},{"shibuya",35.6580,139.7016,"city"},
  {"shinjuku",35.6938,139.7034,"city"},
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

static int centroid_exact(const char *key, double *lat, double *lon,
                          const char **prec) {
  if (!key || !*key) return 0;
  for (size_t i = 0; i < N_CENTROIDS; i++)
    if (strcmp(PREFECTURE_CENTROIDS[i].key, key) == 0) {
      *lat = PREFECTURE_CENTROIDS[i].lat;
      *lon = PREFECTURE_CENTROIDS[i].lon;
      *prec = PREFECTURE_CENTROIDS[i].prec;
      return 1;
    }
  return 0;
}

static int guess_centroid(const char *text, double *lat, double *lon,
                          const char **prec) {
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
      *prec = PREFECTURE_CENTROIDS[i].prec;
      return 1;
    }
  }
  return 0;
}

/* path-pattern gate for the JS capture group 1:
 *   (?:\/|)?fr\/webcam\/japan\/[^"#?\s]+\.html
 * i.e. an OPTIONAL leading '/', then literal "fr/webcam/japan/", then one
 * or more chars excluding " # ? whitespace, ending in ".html". */
static int skyline_path_ok(const char *v) {
  const char *p = v;
  if (*p == '/') p++;
  const char *lit = "fr/webcam/japan/";
  size_t ll = strlen(lit);
  if (strncmp(p, lit, ll) != 0) return 0;
  p += ll;
  int n = 0;
  for (; *p; p++) {
    char ch = *p;
    if (ch == '"' || ch == '#' || ch == '?' || ch == ' ' || ch == '\t' ||
        ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') return 0;
    n++;
  }
  if (n == 0) return 0;
  size_t L = strlen(v);
  return L >= 5 && strcmp(v + L - 5, ".html") == 0;
}

/* Scanner for /href="(<skyline-path>)"[^>]*>([\s\S]*?)<\/a>/gi : the JS
 * regex anchors on href=, NOT on <a, so scan every href="..." whose value
 * passes skyline_path_ok, then capture INNER from the next '>' to the
 * first </a> (non-greedy [\s\S]*?). */
static const char *next_skyline(const char *from, char *path, size_t pn,
                                char **inner_out) {
  *inner_out = NULL;
  path[0] = 0;
  for (const char *p = from; (p = strstr(p, "href=\"")) != NULL; p++) {
    const char *v = p + 6;
    const char *ve = strchr(v, '"');
    if (!ve) return NULL;
    size_t vl = (size_t)(ve - v);
    if (vl == 0 || vl >= pn) { p = ve; continue; }
    char val[1024];
    if (vl >= sizeof val) { p = ve; continue; }
    memcpy(val, v, vl);
    val[vl] = 0;
    if (!skyline_path_ok(val)) { p = ve; continue; }
    const char *gt = strchr(ve, '>');
    if (!gt) return NULL;
    const char *iend = strstr(gt + 1, "</a");
    if (!iend) return NULL;
    memcpy(path, val, vl);
    path[vl] = 0;
    *inner_out = strndup(gt + 1, (size_t)(iend - (gt + 1)));
    const char *past = strchr(iend, '>');
    return past ? past + 1 : iend;
  }
  return NULL;
}

static const char *SKY_BASE =
  "https://www.skylinewebcams.com/fr/webcam/japan.html";

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* renderHtml(base) in JS → headless Chromium with consent-click + scroll.
   * feed_get_text only returns the consent-walled pre-lazy-load HTML; with
   * no JS engine the lazy grid is usually absent ⇒ 0 cameras (faithful,
   * like an unported upstream). Any server-rendered anchors are scraped. */
  char *html = feed_get_text(ctx->http, SKY_BASE, 30000);
  if (!html || !html[0]) {
    free(html);
    fprintf(stderr,
            "[cam-skylinewebcams] 0 (no html — consent wall / WAF likely)\n");
    return 0;
  }

  char **seen = NULL;                    /* dedupe by resolved path */
  int nseen = 0, seencap = 0;
  int count = 0;
  const char *cur = html;
  char rawpath[1024];
  char *inner;
  while ((cur = next_skyline(cur, rawpath, sizeof rawpath, &inner)) != NULL) {
    if (!inner) continue;
    /* path = m[1].startsWith('/') ? m[1] : `/${m[1]}` */
    char path[1100];
    if (rawpath[0] == '/') snprintf(path, sizeof path, "%s", rawpath);
    else snprintf(path, sizeof path, "/%s", rawpath);

    int dup = 0;
    for (int s = 0; s < nseen; s++)
      if (strcmp(seen[s], path) == 0) { dup = 1; break; }
    if (dup) { free(inner); continue; }
    if (nseen == seencap) {
      seencap = seencap ? seencap * 2 : 32;
      seen = realloc(seen, (size_t)seencap * sizeof *seen);
    }
    seen[nseen++] = strdup(path);

    char *label = html_strip(inner);
    free(inner);

    /* parts = path.split('/').filter(Boolean); citySlug =
     * (parts[4]||parts[3]||'').replace(/-prefecture$/,'').replace(/-/g,'') */
    char parts[8][128];
    int np = 0;
    const char *q = path;
    while (*q && np < 8) {
      while (*q == '/') q++;
      if (!*q) break;
      const char *s = q;
      while (*q && *q != '/') q++;
      size_t L = (size_t)(q - s);
      if (L >= sizeof parts[0]) L = sizeof parts[0] - 1;
      memcpy(parts[np], s, L);
      parts[np][L] = 0;
      np++;
    }
    const char *seg = "";
    if (np > 4 && parts[4][0]) seg = parts[4];
    else if (np > 3 && parts[3][0]) seg = parts[3];
    char cityslug[128];
    size_t cl = 0;
    for (size_t k = 0; seg[k] && cl + 1 < sizeof cityslug; k++)
      cityslug[cl++] = seg[k];
    cityslug[cl] = 0;
    /* strip trailing "-prefecture" */
    size_t csl = strlen(cityslug);
    const char *pfx = "-prefecture";
    size_t pfl = strlen(pfx);
    if (csl >= pfl && strcmp(cityslug + csl - pfl, pfx) == 0)
      cityslug[csl - pfl] = 0;
    /* remove all '-' */
    char cs2[128];
    size_t c2 = 0;
    for (size_t k = 0; cityslug[k] && c2 + 1 < sizeof cs2; k++)
      if (cityslug[k] != '-') cs2[c2++] = cityslug[k];
    cs2[c2] = 0;

    /* Coordinate = the REAL centroid of the prefecture/city named in the
     * scraped URL slug (or guessed from the visible label). The upstream JS
     * jittered points around the centroid and ran an LLM geocode pass to find
     * the true camera position — neither is ported, so we must not invent a
     * distinct position. We place the camera at the real centroid of its named
     * place, and SKIP cameras whose location cannot be honestly resolved (no
     * Tokyo fallback — that would fabricate a location). */
    double lat, lon;
    const char *prec = NULL;
    if (!centroid_exact(cs2, &lat, &lon, &prec) &&
        !guess_centroid(label ? label : "", &lat, &lon, &prec)) {
      free(label);
      continue;
    }

    char fullurl[1200];
    snprintf(fullurl, sizeof fullurl,
             "https://www.skylinewebcams.com%s", path);

    /* The point is the named place's centroid (area), NOT the camera's GPS —
     * flag it honestly so consumers don't treat it as precise. */
    const char *nm = (label && label[0]) ? label : "SkylineWebcams feed";
    kv ex[4];
    ex[0].k = "url"; ex[0].is_num = 0; ex[0].is_null = 0;
      ex[0].sv = fullurl; ex[0].nv = 0;
    ex[1].k = "city"; ex[1].is_num = 0; ex[1].is_null = 0;
      ex[1].sv = cs2; ex[1].nv = 0;
    ex[2].k = "location_precision"; ex[2].is_num = 0; ex[2].is_null = 0;
      ex[2].sv = prec ? prec : "prefecture"; ex[2].nv = 0;
    ex[3].k = "location_approximate"; ex[3].is_num = 1; ex[3].is_null = 0;
      ex[3].sv = NULL; ex[3].nv = 1;
    cJSON *f = make_feature(lat, lon, nm, "aggregator_skyline",
                            "skylinewebcams", ex, 4);
    if (camera_upsert(ctx->db, sink, f, "skylinewebcams") >= 0) count++;
    cJSON_Delete(f);
    free(label);
  }

  for (int s = 0; s < nseen; s++) free(seen[s]);
  free(seen);
  free(html);
  fprintf(stderr, "[cam-skylinewebcams] emitted %d\n", count);
  return 0;
}

static const source_def cam_skylinewebcams_def = {
  .id = "cam-skylinewebcams", .collector = "infrastructure",
  .name = "Camera Discovery: SkylineWebcams",
  .name_ja = "カメラ探索: SkylineWebcams",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_skylinewebcams_def)
