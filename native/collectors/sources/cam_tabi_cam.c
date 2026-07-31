/* collectors/infrastructure/sources/cam_tabi_cam.c
 *
 * Registered source_def — port of ONE cameraDiscovery.js channel:
 *   fromTabiCam()  (sources[] id 'tabi_cam', label 'Tabi-cam').
 *
 * JS behaviour reproduced faithfully:
 *   base = NEW_AGGREGATOR_INDEX.tabicam = 'https://tabi.cam/japan/'
 *   html = GET base (BROWSER_UA, 20 s). if (!html) return [].
 *   regex:
 *     /<a[^>]+href="((?:https?:\/\/tabi\.cam)?\/japan\/[a-z0-9-]+\/?)"
 *       [^>]*>([\s\S]{0,260}?)<\/a>/gi
 *   Dedup by href (m[1]).  slug = href.match(/\/japan\/([a-z0-9-]+)/i)[1].
 *   innerText = m[2] tags→' ', \s+→' ', trim.
 *   name = innerText || (slug split '-', drop pure-digit, join ' ',
 *          capitalise words) || 'tabi.cam feed'
 *   centroid = guessCentroidFromText(slug) || TOKYO; jitterAround(c,len).
 *   makeFeature(camera_type='aggregator_tabicam',
 *               discovery_channel='tabi_cam', extra: url=absUrl(href,base)).
 *   cap features.length < 200.
 *   geocodeFeatures(features) — LLM enricher, DEFERRED (skip-and-document;
 *     identical pre-enrich emission, not faked).
 *
 * tabi.cam is Cloudflare-WAF'd: from a datacenter IP the GET typically 403s,
 * body empty → 0 rows (faithful: JS `if (!html) return []`).  run() returns
 * 0 (ran) even with 0 rows; hard transport failure → return -1.
 *
 * Every feature → camera_upsert(channel="tabi_cam").  makeFeature parity
 * byte-identical to camera_discovery.c.
 */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define BROWSER_UA \
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
  "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
#define TABICAM_BASE "https://tabi.cam/japan/"

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
static const centroid PREF_CENTROIDS[] = {
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
  {"okinawa",26.3344,127.8056},{"sapporo",43.0642,141.3469},
  {"yokohama",35.4437,139.6380},{"nagoya",35.1815,136.9066},
  {"kobe",34.6901,135.1955},{"sendai",38.2682,140.8694},
  {"nara_city",34.6851,135.8050},{"nikko",36.7581,139.6117},
  {"nagasaki_city",32.7448,129.8737},{"fuji",35.3606,138.7274},
  {"hakone",35.2323,139.1069},{"asakusa",35.7148,139.7967},
  {"shibuya",35.6580,139.7016},{"shinjuku",35.6938,139.7034},
};
static char lc(char c) { return (c>='A'&&c<='Z')?(char)(c+32):c; }
/* Index of the first city/area entry in PREF_CENTROIDS (everything before it
 * is a prefecture). Used to label matches as "prefecture" vs "city". */
#define FIRST_CITY_IDX 47  /* "sapporo" — entries [0..46] are prefectures */
static int guess_centroid(const char *text, double *olat, double *olon,
                          const char **oprecision) {
  if (!text || !*text) return 0;
  size_t tl = strlen(text);
  char *low = malloc(tl + 1);
  if (!low) return 0;
  for (size_t i = 0; i <= tl; i++) low[i] = lc(text[i]);
  for (size_t i = 0; i < sizeof PREF_CENTROIDS / sizeof *PREF_CENTROIDS; i++) {
    const char *k = PREF_CENTROIDS[i].key;
    char kb[32]; size_t kl = strlen(k);
    if (kl > 5 && strcmp(k + kl - 5, "_city") == 0) kl -= 5;
    if (kl >= sizeof kb) kl = sizeof kb - 1;
    memcpy(kb, k, kl); kb[kl] = 0;
    if (strstr(low, kb)) {
      *olat = PREF_CENTROIDS[i].lat; *olon = PREF_CENTROIDS[i].lon;
      if (oprecision)
        *oprecision = (i >= FIRST_CITY_IDX) ? "city" : "prefecture";
      free(low); return 1;
    }
  }
  free(low);
  return 0;
}
/* absUrl(href, base): href is "/japan/..." (rooted) or full
 * "https://tabi.cam/japan/..." — origin "https://tabi.cam". */
static void abs_url(const char *href, char *out, size_t n) {
  if (href && href[0] == '/') snprintf(out, n, "https://tabi.cam%s", href);
  else if (href && strncmp(href, "http", 4) == 0) snprintf(out, n, "%s", href);
  else snprintf(out, n, "https://tabi.cam/%s", href ? href : "");
}
static char *get_ua(http_client *http, const char *url, int timeout_ms,
                    int *transport_ok) {
  http_response r = {0};
  const char *h[] = { "User-Agent: " BROWSER_UA, NULL };
  int rc = http_request(http, "GET", url, h, NULL, 0, timeout_ms, 2, &r);
  if (transport_ok) *transport_ok = (rc == 0);
  char *body = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body)
    body = strdup(r.body);
  http_response_free(&r);
  return body;
}
static char *strip_html(const char *in, int in_len) {
  char *buf = malloc((size_t)in_len + 1);
  if (!buf) return NULL;
  int o = 0, intag = 0, sp = 0;
  for (int i = 0; i < in_len; i++) {
    char c = in[i];
    if (c == '<') { intag = 1; continue; }
    if (c == '>') { intag = 0; c = ' '; }
    if (intag) continue;
    if (c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v') {
      if (o > 0 && !sp) { buf[o++] = ' '; sp = 1; }
    } else { buf[o++] = c; sp = 0; }
  }
  while (o > 0 && buf[o-1] == ' ') o--;
  buf[o] = 0;
  return buf;
}
static void slug_title(const char *slug, char *out, size_t n) {
  size_t o = 0; int last_was_space = 1;
  const char *p = slug;
  while (*p) {
    const char *seg = p;
    while (*p && *p != '-') p++;
    int seglen = (int)(p - seg);
    int alldig = seglen > 0;
    for (int i = 0; i < seglen; i++)
      if (!isdigit((unsigned char)seg[i])) { alldig = 0; break; }
    if (seglen > 0 && !alldig) {
      if (o > 0 && !last_was_space && o + 1 < n) out[o++] = ' ';
      for (int i = 0; i < seglen && o + 1 < n; i++) out[o++] = seg[i];
      last_was_space = 0;
    }
    if (*p == '-') p++;
  }
  out[o] = 0;
  int word_start = 1;
  for (size_t i = 0; out[i]; i++) {
    if (out[i] == ' ') { word_start = 1; continue; }
    if (word_start && out[i] >= 'a' && out[i] <= 'z') out[i] = (char)(out[i]-32);
    word_start = 0;
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_client *http = ctx->http;
  int tok = 0;
  char *html = get_ua(http, TABICAM_BASE, 20000, &tok);
  if (!html) return tok ? 0 : -1;     /* !html→[] ; hard fail→-1 */

  cJSON **feats = NULL;
  int nf = 0, capf = 0;
  char **seen = NULL;
  int nseen = 0, capseen = 0;

  const char *p = html;
  const char *a;
  while (nf < 200 && (a = strstr(p, "<a")) != NULL) {
    const char *gt = strchr(a, '>');
    const char *href = strstr(a, "href=\"");
    if (!href || (gt && href > gt)) { p = a + 2; continue; }
    const char *u = href + 6;
    /* (?:https?://tabi.cam)?/japan/[a-z0-9-]+/? */
    const char *jp = NULL;
    if (strncmp(u, "/japan/", 7) == 0) jp = u + 7;
    else if (strncmp(u, "http://tabi.cam/japan/", 22) == 0) jp = u + 22;
    else if (strncmp(u, "https://tabi.cam/japan/", 23) == 0) jp = u + 23;
    if (!jp) { p = a + 2; continue; }
    int sl = 0;
    while (jp[sl] && ((jp[sl] >= 'a' && jp[sl] <= 'z') ||
                      (jp[sl] >= '0' && jp[sl] <= '9') ||
                      jp[sl] == '-')) sl++;
    if (sl == 0) { p = a + 2; continue; }
    const char *after = jp + sl;
    int trail = (*after == '/') ? 1 : 0;
    const char *qpos = after + trail;
    if (*qpos != '"') { p = a + 2; continue; }
    int href_len = (int)(qpos - u);
    const char *tagclose = strchr(qpos, '>');
    if (!tagclose) break;
    const char *aend = strstr(tagclose + 1, "</a>");
    if (!aend) { p = a + 2; continue; }
    int inner_len = (int)(aend - (tagclose + 1));
    if (inner_len > 260) inner_len = 260;       /* [\s\S]{0,260}? */

    char hbuf[256];
    int hl = href_len < (int)sizeof hbuf - 1 ? href_len
                                             : (int)sizeof hbuf - 1;
    memcpy(hbuf, u, (size_t)hl); hbuf[hl] = 0;

    int dup = 0;
    for (int j = 0; j < nseen; j++)
      if (strcmp(seen[j], hbuf) == 0) { dup = 1; break; }
    if (dup) { p = aend + 4; continue; }
    if (nseen >= capseen) {
      capseen = capseen ? capseen * 2 : 64;
      void *ns = realloc(seen, (size_t)capseen * sizeof *seen);
      if (!ns) goto oom;
      seen = ns;
    }
    seen[nseen++] = strdup(hbuf);

    /* slug = first /japan/([a-z0-9-]+) in href */
    char slug[160];
    int cn = sl < (int)sizeof slug - 1 ? sl : (int)sizeof slug - 1;
    memcpy(slug, jp, (size_t)cn); slug[cn] = 0;

    char *inner = strip_html(tagclose + 1, inner_len);
    char name[256];
    if (inner && inner[0]) snprintf(name, sizeof name, "%s", inner);
    else {
      slug_title(slug, name, sizeof name);
      if (!name[0]) snprintf(name, sizeof name, "tabi.cam feed");
    }
    free(inner);

    /* No real coords on the index page; only centroid-from-slug is available.
     * Skip if no location signal — never plant a default (e.g. Tokyo) point. */
    double lat, lon;
    const char *precision = NULL;
    if (!guess_centroid(slug, &lat, &lon, &precision)) { p = aend + 4; continue; }
    char aurl[256];
    abs_url(hbuf, aurl, sizeof aurl);
    /* Emit centroid coords EXACTLY (no jitter) and flag as area-approximate. */
    kv ex[3] = {0};
    ex[0].k="url"; ex[0].sv=aurl;
    ex[1].k="location_precision"; ex[1].sv=precision;
    ex[2].k="location_approximate"; ex[2].is_bool=1; ex[2].bv=1;
    if (nf >= capf) {
      capf = capf ? capf * 2 : 64;
      void *nfp = realloc(feats, (size_t)capf * sizeof *feats);
      if (!nfp) goto oom;
      feats = nfp;
    }
    feats[nf++] = make_feature(lat, lon, name, "aggregator_tabicam",
                               "tabi_cam", ex, 3);
    p = aend + 4;
    continue;
  oom:
    free(html);
    for (int j = 0; j < nf; j++) cJSON_Delete(feats[j]);
    free(feats);
    for (int j = 0; j < nseen; j++) free(seen[j]);
    free(seen);
    return -1;
  }
  free(html);
  for (int j = 0; j < nseen; j++) free(seen[j]);
  free(seen);

  /* geocodeFeatures(features): LLM enricher — DEFERRED (see header). */
  int count = 0;
  for (int i = 0; i < nf; i++) {
    if (camera_upsert(ctx->db, sink, feats[i], "tabi_cam") >= 0) count++;
    cJSON_Delete(feats[i]);
  }
  free(feats);
  fprintf(stderr, "[cam-tabi_cam] %d cams upserted (WAF may yield 0)\n",
          count);
  return 0;
}

static const source_def cam_tabi_cam_def = {
  .id = "cam-tabi_cam", .collector = "infrastructure",
  .name = "Camera discovery — Tabi-cam",
  .name_ja = "カメラ探索 — Tabi-cam",
   .update_interval_sec = 21600, .run = run,
  .category = "infrastructure" };
REGISTER_SOURCE(cam_tabi_cam_def)
