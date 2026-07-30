/* collectors/infrastructure/sources/cam_camscape.c
 *
 * Registered source_def — port of ONE cameraDiscovery.js channel:
 *   fromCamscape()  (sources[] id 'camscape', label 'Camscape').
 *
 * JS behaviour reproduced faithfully:
 *   base = NEW_AGGREGATOR_INDEX.camscape = 'https://www.camscape.com/?s=japan'
 *   for page 1..6:
 *     url = page===1 ? base : `https://www.camscape.com/page/<page>/?s=japan`
 *     html = GET url (BROWSER_UA, 20 s); continue if empty.
 *     regex:
 *       /<a[^>]+href="(https?:\/\/www\.camscape\.com\/webcam\/([a-z0-9-]+)\/?)"
 *         [^>]*>([\s\S]{0,400}?)<\/a>/gi
 *     Dedup by detail URL (m[1]).  innerText = m[3] with HTML entities → ' ',
 *       tags → ' ', \s+ → ' ', trim.
 *     title = innerText.length>=3 ? innerText
 *           : (slug split '-', drop pure-digit, join ' ', capitalise words)
 *             || 'camscape Japan feed'
 *     centroid = guessCentroidFromText(title); NO match → SKIP (no fabricated
 *       location, no TOKYO default, no jitter — area centroid emitted exactly,
 *       flagged location_approximate / location_precision).
 *     makeFeature(camera_type='aggregator_camscape',
 *                 discovery_channel='camscape', extra: url=detailUrl).
 *     cap features.length < 200.
 *   geocodeFeatures(features) — LLM enricher, DEFERRED (skip-and-document;
 *     identical pre-enrich emission, not faked — see cam_webcamendirect_list.c).
 *
 * camscape.com is Cloudflare-WAF'd: from a datacenter IP the GET typically
 * 403s, the body is empty, and this channel contributes 0 rows — that is the
 * faithful outcome (JS `if (!html) continue;`).  run() still returns 0 (ran).
 * A hard transport failure on every page → return -1.
 *
 * Every feature → camera_upsert(channel="camscape").  makeFeature parity
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
#define CAMSCAPE_BASE "https://www.camscape.com/?s=japan"

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
/* Returns 1 and the matched centroid coords if a place-name in `text` matches
 * a PREF_CENTROIDS key.  `*precision` is set to "city" for a city/landmark key
 * (those use a "_city" suffix or are named municipalities) and "prefecture"
 * otherwise — both are AREA centroids, never the camera's real GPS. */
static int guess_centroid(const char *text, double *olat, double *olon,
                          const char **precision) {
  if (!text || !*text) return 0;
  size_t tl = strlen(text);
  char *low = malloc(tl + 1);
  if (!low) return 0;
  for (size_t i = 0; i <= tl; i++) low[i] = lc(text[i]);
  for (size_t i = 0; i < sizeof PREF_CENTROIDS / sizeof *PREF_CENTROIDS; i++) {
    const char *k = PREF_CENTROIDS[i].key;
    char kb[32]; size_t kl = strlen(k);
    int is_city = (kl > 5 && strcmp(k + kl - 5, "_city") == 0);
    if (is_city) kl -= 5;
    if (kl >= sizeof kb) kl = sizeof kb - 1;
    memcpy(kb, k, kl); kb[kl] = 0;
    if (strstr(low, kb)) {
      *olat = PREF_CENTROIDS[i].lat; *olon = PREF_CENTROIDS[i].lon;
      if (precision) *precision = is_city ? "city" : "prefecture";
      free(low); return 1;
    }
  }
  free(low);
  return 0;
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

/* m[3] cleanup: /&#?[a-z0-9]+;/gi → ' ', /<[^>]+>/g → ' ', /\s+/g → ' ',
 * trim.  in/in_len = raw inner. */
static char *clean_inner(const char *in, int in_len) {
  char *buf = malloc((size_t)in_len + 1);
  if (!buf) return NULL;
  int o = 0;
  for (int i = 0; i < in_len; ) {
    char c = in[i];
    if (c == '&') {
      /* &#?[a-z0-9]+;  (case-insensitive) */
      int j = i + 1;
      if (in[j] == '#') j++;
      int st = j;
      while (j < in_len && (isalnum((unsigned char)in[j]))) j++;
      if (j > st && j < in_len && in[j] == ';') {
        buf[o++] = ' '; i = j + 1; continue;
      }
      buf[o++] = c; i++; continue;
    }
    if (c == '<') {
      int j = i + 1;
      while (j < in_len && in[j] != '>') j++;
      if (j < in_len) { buf[o++] = ' '; i = j + 1; continue; }
      buf[o++] = c; i++; continue;
    }
    buf[o++] = c; i++;
  }
  buf[o] = 0;
  /* \s+ → ' ' */
  char *out = malloc((size_t)o + 1);
  if (!out) { free(buf); return NULL; }
  int oo = 0, sp = 0;
  for (int i = 0; i < o; i++) {
    char c = buf[i];
    if (c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v') {
      if (oo > 0 && !sp) { out[oo++] = ' '; sp = 1; }
    } else { out[oo++] = c; sp = 0; }
  }
  while (oo > 0 && out[oo-1] == ' ') oo--;
  out[oo] = 0;
  free(buf);
  return out;
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
  cJSON **feats = NULL;
  int nf = 0, capf = 0;
  char **seen = NULL;          /* dedup by detail URL */
  int nseen = 0, capseen = 0;
  int any_transport_ok = 0, any_body = 0;

  for (int page = 1; page <= 6 && nf < 200; page++) {
    char url[160];
    if (page == 1) snprintf(url, sizeof url, "%s", CAMSCAPE_BASE);
    else snprintf(url, sizeof url,
                  "https://www.camscape.com/page/%d/?s=japan", page);
    int tok = 0;
    char *html = get_ua(http, url, 20000, &tok);
    if (tok) any_transport_ok = 1;
    if (!html) continue;
    any_body = 1;

    const char *p = html;
    const char *a;
    while (nf < 200 && (a = strstr(p, "<a")) != NULL) {
      const char *gt = strchr(a, '>');
      const char *href = strstr(a, "href=\"");
      if (!href || (gt && href > gt)) { p = a + 2; continue; }
      const char *u = href + 6;
      /* https?://www.camscape.com/webcam/([a-z0-9-]+)/? */
      const char *m = NULL;
      if (strncmp(u, "http://www.camscape.com/webcam/", 31) == 0) m = u + 31;
      else if (strncmp(u, "https://www.camscape.com/webcam/", 32) == 0)
        m = u + 32;
      if (!m) { p = a + 2; continue; }
      int sl = 0;
      while (m[sl] && ((m[sl] >= 'a' && m[sl] <= 'z') ||
                       (m[sl] >= '0' && m[sl] <= '9') ||
                       m[sl] == '-')) sl++;
      if (sl == 0) { p = a + 2; continue; }
      const char *after = m + sl;
      int trail = (*after == '/') ? 1 : 0;     /* optional /? */
      const char *qpos = after + trail;
      if (*qpos != '"') { p = a + 2; continue; }
      int durl_len = (int)(qpos - u);
      const char *tagclose = strchr(qpos, '>');
      if (!tagclose) break;
      const char *aend = strstr(tagclose + 1, "</a>");
      if (!aend) { p = a + 2; continue; }
      int inner_len = (int)(aend - (tagclose + 1));
      if (inner_len > 400) inner_len = 400;     /* [\s\S]{0,400}? */

      char durl[256];
      int dl = durl_len < (int)sizeof durl - 1 ? durl_len
                                               : (int)sizeof durl - 1;
      memcpy(durl, u, (size_t)dl); durl[dl] = 0;

      int dup = 0;
      for (int j = 0; j < nseen; j++)
        if (strcmp(seen[j], durl) == 0) { dup = 1; break; }
      if (dup) { p = aend + 4; continue; }
      if (nseen >= capseen) {
        capseen = capseen ? capseen * 2 : 64;
        void *ns = realloc(seen, (size_t)capseen * sizeof *seen);
        if (!ns) goto oom;
        seen = ns;
      }
      seen[nseen++] = strdup(durl);

      char *inner = clean_inner(tagclose + 1, inner_len);
      char title[256];
      if (inner && (int)strlen(inner) >= 3) {
        snprintf(title, sizeof title, "%s", inner);
      } else {
        char slug[160];
        int cn = sl < (int)sizeof slug - 1 ? sl : (int)sizeof slug - 1;
        memcpy(slug, m, (size_t)cn); slug[cn] = 0;
        slug_title(slug, title, sizeof title);
        if (!title[0])
          snprintf(title, sizeof title, "camscape Japan feed");
      }
      free(inner);

      /* No real coordinates are present on the search-result listing, so the
       * only location signal is a place-name centroid.  If none matches we have
       * NO honest location — skip the camera rather than plant it at a default
       * point.  The matched centroid is an AREA anchor, flagged approximate. */
      double lat, lon;
      const char *precision = NULL;
      if (!guess_centroid(title, &lat, &lon, &precision)) {
        p = aend + 4;
        continue;
      }
      kv ex[3] = {0};
      ex[0].k="url"; ex[0].sv=durl;
      ex[1].k="location_precision"; ex[1].sv=precision;
      ex[2].k="location_approximate"; ex[2].is_bool=1; ex[2].bv=1;
      if (nf >= capf) {
        capf = capf ? capf * 2 : 64;
        void *nfp = realloc(feats, (size_t)capf * sizeof *feats);
        if (!nfp) goto oom;
        feats = nfp;
      }
      feats[nf++] = make_feature(lat, lon, title, "aggregator_camscape",
                                 "camscape", ex, 3);
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
  }
  for (int j = 0; j < nseen; j++) free(seen[j]);
  free(seen);

  if (!any_transport_ok && !any_body && nf == 0) { free(feats); return -1; }

  /* geocodeFeatures(features): LLM enricher — DEFERRED (see header). */
  int count = 0;
  for (int i = 0; i < nf; i++) {
    if (camera_upsert(ctx->db, sink, feats[i], "camscape") >= 0) count++;
    cJSON_Delete(feats[i]);
  }
  free(feats);
  fprintf(stderr, "[cam-camscape] %d cams upserted (WAF may yield 0)\n",
          count);
  return 0;
}

static const source_def cam_camscape_def = {
  .id = "cam-camscape", .collector = "camera-discovery",
  .name = "Camera discovery — Camscape",
  .name_ja = "カメラ探索 — Camscape",
   .layer = "cameras",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_camscape_def)
