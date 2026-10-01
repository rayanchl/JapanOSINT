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
#include "cam_centroids.inc"

typedef struct { const char *k; const char *sv; int is_num; double nv;
                 int is_null; } kv;

/* camera_uid = "<lat>:<lon>:<tail>". The tail used to be a bare 60-char
 * prefix of the URL — but every Skyline camera in a city shares far more than
 * 60 characters of path
 *   https://www.skylinewebcams.com/fr/webcam/japan/kanto/tokyo/kabukicho.html
 *   https://www.skylinewebcams.com/fr/webcam/japan/kanto/tokyo/shinjuku-…
 * so they all collapsed onto ".../tokyo/k" and ".../tokyo/s" and overwrote
 * each other in the camera keyspace: 101 cameras scraped, 36 stored. Keep a
 * readable prefix but append a hash of the FULL source string so distinct
 * cameras stay distinct. (The same 60-char truncation is copy-pasted into 13
 * other cam_*.c collectors — see the audit report.) */
static void uid_tail(const char *url, const char *name, char *out,
                     size_t outsz) {
  const char *src = (url && *url) ? url : (name ? name : "");
  /* FNV-1a over the whole source string — deterministic across runs. */
  unsigned long long h = 1469598103934665603ULL;
  for (const char *p = src; *p; p++) {
    h ^= (unsigned char)*p;
    h *= 1099511628211ULL;
  }
  size_t i = 0;
  for (; src[i] && i < 44 && i + 18 < outsz; i++) {
    unsigned char c = (unsigned char)src[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  out[i] = 0;
  snprintf(out + i, outsz - i, "#%016llx", h);
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
  snprintf(lats, sizeof lats, "%.4f", jo_round4(lat));
  snprintf(lons, sizeof lons, "%.4f", jo_round4(lon));
  uid_tail(url, name, tail, sizeof tail);
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
    else if (e->is_num) cJSON_AddNumberToObject(p, e->k, e->nv);
    else cJSON_AddItemToObject(p, e->k,
           e->sv ? cJSON_CreateString(e->sv) : cJSON_CreateNull());
  }
  cJSON_AddItemToObject(f, "properties", p);
  return f;
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
    if (!cam_centroid_exact(cs2, &lat, &lon, &prec) &&
        !cam_centroid_find(label ? label : "", CAM_CENTROID_SCAN_1K,
                           &lat, &lon, &prec)) {
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
    ex[2].k = "geo_precision"; ex[2].is_num = 0; ex[2].is_null = 0;
      ex[2].sv = prec ? prec : "prefecture"; ex[2].nv = 0;
    ex[3].k = "geo_uncertain"; ex[3].is_num = 1; ex[3].is_null = 0;
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
  .id = "cam-skylinewebcams", .collector = "camera-discovery",
  .name = "Camera Discovery: SkylineWebcams",
  .name_ja = "カメラ探索: SkylineWebcams",
   .layer = "cameras",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cam_skylinewebcams_def)
