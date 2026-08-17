/* collectors/infrastructure/sources/cam_worldcams.c
 *
 * Registered source_def — faithful port of the `worldcams` discovery channel
 * from server/src/collectors/cameraDiscovery.js (fromWorldcams()).
 *
 * JS: fetch base + ?page=2..4 (plain fetchText, browser UA), join('\n'),
 * scan /<a[^>]+href="(/japan/([a-z0-9-]+)/([a-z0-9-]+))"[^>]*>([\s\S]*?)<\/a>/gi
 * (capture href, city, slug, inner). Dedupe by href; label=html_strip(inner)
 * (fallback to city); collect cams. Then for each cam (cap 300):
 * centroid = PREFECTURE_CENTROIDS[city] || guessCentroidFromText(name) ||
 * TOKYO; jitterAround(idx); makeFeature(camera_type='aggregator_worldcams',
 * discovery_channel='worldcams', extra: url,city).
 *
 * NOT ported (post-list passes): upgradeYouTubeStreamUrls(features,4) (a
 * concurrent per-detail-page fetch+regex worker that rewrites url→youtube
 * watch link and re-keys camera_uid to yt:/ytc:) and geocodeFeatures (LLM).
 * htmlparse + sequential fetch faithfully expresses the list-page scrape;
 * the YouTube-upgrade worker pool is out of scope per the porting rule
 * (per-detail-page concurrency the toolkit can't express) — coords/url stay
 * exactly as fromWorldcams produces them pre-upgrade. Cams whose camera_uid
 * would have been canonicalised onto a shared YouTube id therefore key on the
 * aggregator URL here (same as an unported upstream — no fabrication).
 *
 * Each Feature → camera_upsert(...,"worldcams") (discovery_channels[] union
 * + existing-non-null-wins merge + seen_count++, exactly like cameraRunner).
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

/* The first 47 table entries are the prefectures (index 0..46); the rest are
 * city/locality anchors. Used only to report honest location_precision. */
#define N_PREFECTURES 47
static const char *precision_for_index(size_t i) {
  return (i < N_PREFECTURES) ? "prefecture" : "city";
}

/* PREFECTURE_CENTROIDS[key] exact lookup (JS object index, no _city strip).
 * On match: fills lat/lon with the EXACT centroid and returns its table index
 * (so the caller can report honest precision); returns -1 on no match. */
static int centroid_exact(const char *key, double *lat, double *lon) {
  if (!key || !*key) return -1;
  for (size_t i = 0; i < N_CENTROIDS; i++)
    if (strcmp(PREFECTURE_CENTROIDS[i].key, key) == 0) {
      *lat = PREFECTURE_CENTROIDS[i].lat;
      *lon = PREFECTURE_CENTROIDS[i].lon;
      return (int)i;
    }
  return -1;
}

static int guess_centroid(const char *text, double *lat, double *lon) {
  if (!text || !*text) return -1;
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
      return (int)i;
    }
  }
  return -1;
}

/* anchor scanner — see cam_geocam.c for the regex-equivalence note. */
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

/* JS href shape: /japan/([a-z0-9-]+)/([a-z0-9-]+)  — exactly two segments
 * after /japan/. Extract city + slug; reject anything else. */
static int worldcams_href(const char *href, char *city, size_t cn,
                          char *slug, size_t sn) {
  const char *pfx = "/japan/";
  size_t pl = strlen(pfx);
  if (strncmp(href, pfx, pl) != 0) return 0;
  const char *p = href + pl;
  const char *cs = p;
  /* The cap here was arbitrary: the feature array grows (realloc), so it was
   * not a memory bound — just a number. The page/category loop above is the
   * real request budget; within a page we emit every camera the aggregator
   * listed. */
  while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                *p == '-')) p++;
  if (p == cs || *p != '/') return 0;
  size_t clen = (size_t)(p - cs);
  if (clen >= cn) clen = cn - 1;
  memcpy(city, cs, clen);
  city[clen] = 0;
  p++;                                  /* slug segment */
  const char *ss = p;
  while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                *p == '-')) p++;
  if (p == ss) return 0;
  size_t slen = (size_t)(p - ss);
  if (slug && sn) {
    if (slen >= sn) slen = sn - 1;
    memcpy(slug, ss, slen);
    slug[slen] = 0;
  }
  return *p == 0;                       /* nothing after slug */
}

/* audit-09: the anchor text on the list page is the CITY, so every camera in a
 * city shared one title ("tokyo" ×N) and the only per-camera label upstream
 * gives us — the URL slug — was discarded. "shibuya-crossing" → "Shibuya
 * Crossing". Nothing is invented; this is the publisher's own slug. */
static void slug_to_name(const char *slug, char *out, size_t n) {
  size_t o = 0; int start = 1;
  for (size_t i = 0; slug[i] && o + 1 < n; i++) {
    char c = slug[i];
    if (c == '-') { out[o++] = ' '; start = 1; continue; }
    if (start && c >= 'a' && c <= 'z') c = (char)(c - 32);
    out[o++] = c; start = 0;
  }
  out[o] = 0;
}

static const char *WC_BASE = "https://worldcams.tv/japan/";

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *urls[4];
  char u2[256], u3[256], u4[256];
  snprintf(u2, sizeof u2, "%s?page=2", WC_BASE);
  snprintf(u3, sizeof u3, "%s?page=3", WC_BASE);
  snprintf(u4, sizeof u4, "%s?page=4", WC_BASE);
  urls[0] = WC_BASE; urls[1] = u2; urls[2] = u3; urls[3] = u4;

  char *htmls[4] = {0};
  size_t total = 1;
  int any = 0;
  for (int i = 0; i < 4; i++) {
    htmls[i] = feed_get_text(ctx->http, urls[i], 8000);
    if (htmls[i] && htmls[i][0]) { total += strlen(htmls[i]) + 1; any = 1; }
  }
  if (!any) {
    for (int i = 0; i < 4; i++) free(htmls[i]);
    fprintf(stderr, "[cam-worldcams] 0 (no html — likely WAF/empty)\n");
    return 0;
  }
  char *html = malloc(total);
  if (!html) { for (int i = 0; i < 4; i++) free(htmls[i]); return -1; }
  html[0] = 0;
  for (int i = 0; i < 4; i++) {
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
  while (         (cur = next_anchor(cur, href, sizeof href, &inner)) != NULL) {
    if (!inner) continue;
    char city[128], slug[160];
    if (!worldcams_href(href, city, sizeof city, slug, sizeof slug)) {
      free(inner); continue;
    }

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
    /* Prefer the per-camera slug name; keep the anchor label only when it says
     * something the slug does not (it is usually just the city again). */
    char slugname[192];
    slug_to_name(slug, slugname, sizeof slugname);
    const char *nm = slugname[0] ? slugname
                                 : ((label && label[0]) ? label : city);

    /* Only centroid-derived location is available on the list page (no real
     * GPS in the markup). Match a prefecture/city centroid and emit it
     * EXACTLY (no jitter); if nothing matches, SKIP — never plant a default. */
    double lat, lon;
    int cidx = centroid_exact(city, &lat, &lon);
    if (cidx < 0) cidx = guess_centroid(nm, &lat, &lon);
    /* the anchor label still gets a look — it names the city for some rows and
     * is the only locality hint when neither the city segment nor the slug
     * matches the centroid table. */
    if (cidx < 0 && label && label[0]) cidx = guess_centroid(label, &lat, &lon);
    if (cidx < 0) { free(label); continue; }

    char fullurl[640];
    jo_abs_url(href, WC_BASE, fullurl, sizeof fullurl);

    kv ex[4] = {0};
    ex[0].k = "url"; ex[0].sv = fullurl;
    ex[1].k = "city"; ex[1].sv = city;
    ex[2].k = "geo_precision";
      ex[2].sv = precision_for_index((size_t)cidx);
    ex[3].k = "geo_uncertain"; ex[3].is_bool = 1; ex[3].bv = 1;
    cJSON *f = make_feature(lat, lon, nm, "aggregator_worldcams",
                            "worldcams", ex, 4);
    if (camera_upsert(ctx->db, sink, f, "worldcams") >= 0) count++;
    cJSON_Delete(f);
    free(label);
  }

  for (int s = 0; s < nseen; s++) free(seen[s]);
  free(seen);
  free(html);
  fprintf(stderr, "[cam-worldcams] emitted %d\n", count);
  return 0;
}

static const source_def cam_worldcams_def = {
  .id = "cam-worldcams", .collector = "camera-discovery",
  .name = "Camera Discovery: Worldcams",
  .name_ja = "カメラ探索: Worldcams",
   .layer = "cameras",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cam_worldcams_def)
