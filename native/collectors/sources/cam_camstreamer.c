/* collectors/infrastructure/sources/cam_camstreamer.c
 *
 * Registered source_def — faithful port of the `camstreamer` discovery
 * channel from server/src/collectors/cameraDiscovery.js (fromCamstreamer()).
 *
 * JS flow:
 *  1. html = fetchText(base, browser UA). If !/\/live\/stream\//i.test(html)
 *     it does a renderHtml Chromium fallback (acceptCookies:false).
 *  2. If html has <script id="__NEXT_DATA__">…</script>: JSON.parse it,
 *     findCamsDeep(data) (recursive pre-order walk collecting objects with
 *     name=(name|title|cameraName), url=(url|streamUrl|liveUrl|link),
 *     lat=(lat??latitude), lon=(lon??lng??longitude) where name&&url&&url is
 *     a string matching /^(https?:)?\/\//; url '//x' → 'https://x';
 *     lat/lon kept only if typeof==='number' else null). For cams.slice(0,
 *     120): skip if url already added (first always added); if c.lat!=null &&
 *     c.lon!=null use those coords verbatim (no jitter) else
 *     guessCentroidFromText(c.name)||TOKYO + jitterAround(idx). makeFeature
 *     (camera_type='aggregator_camstreamer', extra: url).
 *  3. Fallback ONLY if features.length===0: anchor scrape
 *     /<a[^>]+href="(/live/stream/\d+-[a-z0-9-]+)"[^>]*>([\s\S]*?)<\/a>/gi,
 *     dedupe href, skip if !label || /search|signin|login|register/i on
 *     href; centroid = guessCentroidFromText(label)||TOKYO + jitter; cap 200.
 *  4. geocodeFeatures(features) (LLM) — NOT ported (camera LLM-enrich owned
 *     elsewhere); coords stay as produced above.
 *
 * LIMITATION: the renderHtml Chromium fallback (step 1, used when the plain
 * response is shell-only) can't be expressed with feed_get_text — if the
 * server-rendered HTML lacks both __NEXT_DATA__ and /live/stream/ anchors
 * this channel surfaces 0 cameras from this IP (faithful, like an unported
 * upstream — not faked). The __NEXT_DATA__ JSON path and the anchor-scrape
 * fallback ARE ported in full and produce the same Features as Node when
 * the data is server-rendered.
 *
 * Each Feature → camera_upsert(...,"camstreamer").
 */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
                           const char *precision, int approximate,
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
  if (precision && *precision)
    cJSON_AddStringToObject(p, "location_precision", precision);
  if (approximate) cJSON_AddBoolToObject(p, "location_approximate", 1);
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

/* First 47 entries are prefecture centroids; the rest are city/area
 * centroids — precision reflects which kind matched. */
#define N_PREFECTURES 47
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
      if (precision)
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

/* /live/stream/\d+-[a-z0-9-]+ — digits, '-', then [a-z0-9-]+ */
static int stream_href_ok(const char *href) {
  const char *pfx = "/live/stream/";
  size_t pl = strlen(pfx);
  if (strncmp(href, pfx, pl) != 0) return 0;
  const char *p = href + pl;
  int d = 0;
  while (*p >= '0' && *p <= '9') { p++; d++; }
  if (d == 0 || *p != '-') return 0;
  p++;
  int n = 0;
  while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                *p == '-')) { p++; n++; }
  if (n == 0) return 0;
  return *p == 0;       /* JS group has no trailing /? — exact end */
}

/* ── findCamsDeep — pre-order recursive collector ──────────────────────────
 * Mirrors the JS exactly: for an object, test {name,url,lat,lon}; if it
 * qualifies push it; THEN recurse into Object.values(node). Arrays: recurse
 * each element (no push for the array node). */
typedef struct { char *name; char *url; int has_lat, has_lon;
                 double lat, lon; } deepcam;

static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static void find_cams_deep(cJSON *node, deepcam **out, int *n, int *cap) {
  if (!node) return;
  if (cJSON_IsArray(node)) {
    cJSON *v;
    cJSON_ArrayForEach(v, node) find_cams_deep(v, out, n, cap);
    return;
  }
  if (!cJSON_IsObject(node)) return;

  const char *name = jstr(node, "name");
  if (!name) name = jstr(node, "title");
  if (!name) name = jstr(node, "cameraName");
  /* JS: name = node.name||node.title||node.cameraName (could be non-string;
   * then String(name) — but the qualify test also needs url a string and
   * the only push uses String(name). Non-string name objects are rare in
   * NEXT_DATA cam payloads; treat only string names as truthy here, which
   * matches every real camstreamer record. */
  cJSON *urlv = cJSON_GetObjectItem(node, "url");
  if (!urlv || !cJSON_IsString(urlv)) urlv = cJSON_GetObjectItem(node,
                                                                 "streamUrl");
  if (!urlv || !cJSON_IsString(urlv)) urlv = cJSON_GetObjectItem(node,
                                                                 "liveUrl");
  if (!urlv || !cJSON_IsString(urlv)) urlv = cJSON_GetObjectItem(node,
                                                                 "link");
  const char *url = (urlv && cJSON_IsString(urlv)) ? urlv->valuestring : NULL;

  cJSON *latv = cJSON_GetObjectItem(node, "lat");
  if (!latv || cJSON_IsNull(latv)) latv = cJSON_GetObjectItem(node,
                                                              "latitude");
  cJSON *lonv = cJSON_GetObjectItem(node, "lon");
  if (!lonv || cJSON_IsNull(lonv)) lonv = cJSON_GetObjectItem(node, "lng");
  if (!lonv || cJSON_IsNull(lonv)) lonv = cJSON_GetObjectItem(node,
                                                              "longitude");

  /* qualify: name && url && typeof url==='string' &&
   * /^(https?:)?\/\//.test(url) */
  int url_ok = 0;
  if (url) {
    if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0 ||
        strncmp(url, "//", 2) == 0)
      url_ok = 1;
  }
  if (name && url && url_ok) {
    if (*n == *cap) {
      *cap = *cap ? *cap * 2 : 16;
      *out = realloc(*out, (size_t)*cap * sizeof **out);
    }
    deepcam *dc = &(*out)[(*n)++];
    dc->name = strdup(name);
    if (strncmp(url, "//", 2) == 0) {
      size_t L = strlen(url) + 7;
      dc->url = malloc(L);
      snprintf(dc->url, L, "https:%s", url);
    } else {
      dc->url = strdup(url);
    }
    dc->has_lat = (latv && cJSON_IsNumber(latv));
    dc->has_lon = (lonv && cJSON_IsNumber(lonv));
    dc->lat = dc->has_lat ? latv->valuedouble : 0;
    dc->lon = dc->has_lon ? lonv->valuedouble : 0;
  }

  cJSON *child;
  cJSON_ArrayForEach(child, node) find_cams_deep(child, out, n, cap);
}

static const char *CS_BASE =
  "https://camstreamer.com/live/search?country=Japan";

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, CS_BASE, 10000);
  /* JS: hasEntries = html && /\/live\/stream\//i.test(html); if !hasEntries
   * → renderHtml fallback (Chromium, can't express). We keep the
   * server-rendered html; if it lacks the marker AND has no __NEXT_DATA__
   * with cams, this yields 0 (faithful). No fabrication. */
  if (!html || !html[0]) {
    free(html);
    fprintf(stderr, "[cam-camstreamer] 0 (no html — WAF / Chromium-only)\n");
    return 0;
  }

  int count = 0;

  /* Step 2: <script id="__NEXT_DATA__" ...>JSON</script> */
  int produced_from_json = 0;
  const char *sd = strstr(html, "__NEXT_DATA__");
  if (sd) {
    const char *tagclose = strchr(sd, '>');
    const char *jend = tagclose ? strstr(tagclose, "</script>") : NULL;
    if (tagclose && jend) {
      char *json = strndup(tagclose + 1, (size_t)(jend - (tagclose + 1)));
      cJSON *data = json ? cJSON_Parse(json) : NULL;
      free(json);
      if (data) {
        deepcam *cams = NULL;
        int ncams = 0, ccap = 0;
        find_cams_deep(data, &cams, &ncams, &ccap);
        /* cams.slice(0,120) */
        int lim = ncams < 120 ? ncams : 120;
        /* dedupe-against-already-added on url; first always added */
        char **addedurl = NULL;
        int nadd = 0, addcap = 0;
        for (int i = 0; i < lim; i++) {
          deepcam *c = &cams[i];
          int seen = 0;
          if (nadd > 0) {
            for (int s = 0; s < nadd; s++)
              if (strcmp(addedurl[s], c->url) == 0) { seen = 1; break; }
          }
          if (seen) continue;
          if (nadd == addcap) {
            addcap = addcap ? addcap * 2 : 32;
            addedurl = realloc(addedurl, (size_t)addcap * sizeof *addedurl);
          }
          addedurl[nadd++] = c->url;     /* borrow */

          double lat, lon;
          const char *precision = NULL;
          int approximate = 0;
          if (c->has_lat && c->has_lon) {  /* real coords, exact, no jitter */
            lat = c->lat; lon = c->lon;
          } else if (guess_centroid(c->name, &lat, &lon, &precision)) {
            approximate = 1;             /* area centroid — emit unjittered */
          } else {
            continue;                    /* no real location signal — skip */
          }
          const char *nm = (c->name && c->name[0]) ? c->name
                                                   : "Camstreamer feed";
          kv ex[1];
          ex[0].k = "url"; ex[0].is_num = 0; ex[0].is_null = 0;
            ex[0].sv = c->url; ex[0].nv = 0;
          cJSON *f = make_feature(lat, lon, nm, "aggregator_camstreamer",
                                  "camstreamer", precision, approximate,
                                  ex, 1);
          if (camera_upsert(ctx->db, sink, f, "camstreamer") >= 0) {
            count++; produced_from_json = 1;
          }
          cJSON_Delete(f);
        }
        free(addedurl);
        for (int i = 0; i < ncams; i++) {
          free(cams[i].name); free(cams[i].url);
        }
        free(cams);
        cJSON_Delete(data);
      }
    }
  }

  /* Step 3: anchor fallback ONLY if features.length===0 */
  if (!produced_from_json) {
    char **seen = NULL;
    int nseen = 0, seencap = 0;
    const char *cur = html;
    char href[512];
    char *inner;
    while (count < 200 &&
           (cur = next_anchor(cur, href, sizeof href, &inner)) != NULL) {
      if (!inner) continue;
      if (!stream_href_ok(href)) { free(inner); continue; }
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
      /* skip if !label || /search|signin|login|register/i.test(href) */
      int bad = (!label || !label[0]);
      if (!bad) {
        char hl[512];
        to_lower_buf(href, hl, sizeof hl);
        if (strstr(hl, "search") || strstr(hl, "signin") ||
            strstr(hl, "login") || strstr(hl, "register"))
          bad = 1;
      }
      if (bad) { free(label); continue; }

      double lat, lon;
      const char *precision = NULL;
      if (!guess_centroid(label, &lat, &lon, &precision)) {
        free(label); continue;  /* no real location signal — skip, no default */
      }

      char fullurl[640];
      abs_url(href, CS_BASE, fullurl, sizeof fullurl);
      kv ex[1];
      ex[0].k = "url"; ex[0].is_num = 0; ex[0].is_null = 0;
        ex[0].sv = fullurl; ex[0].nv = 0;
      cJSON *f = make_feature(lat, lon, label, "aggregator_camstreamer",
                              "camstreamer", precision, 1, ex, 1);
      if (camera_upsert(ctx->db, sink, f, "camstreamer") >= 0) count++;
      cJSON_Delete(f);
      free(label);
    }
    for (int s = 0; s < nseen; s++) free(seen[s]);
    free(seen);
  }

  free(html);
  fprintf(stderr, "[cam-camstreamer] emitted %d\n", count);
  return 0;
}

static const source_def cam_camstreamer_def = {
  .id = "cam-camstreamer", .collector = "camera-discovery",
  .name = "Camera Discovery: Camstreamer",
  .name_ja = "カメラ探索: Camstreamer",
   .layer = "cameras",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_camstreamer_def)
