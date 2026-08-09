/* collectors/infrastructure/sources/cam_webcamendirect_list.c
 *
 * Registered source_def — port of ONE cameraDiscovery.js channel:
 *   fromWebcamendirectList()  (sources[] id 'webcamendirect_list',
 *   label 'Webcam-en-direct list').
 *
 * JS behaviour reproduced faithfully:
 *   base = NEW_AGGREGATOR_INDEX.webcamendirect = 'https://webcamendirect.net/japon'
 *   indexHtml = GET base (BROWSER_UA, 20 s). if (!indexHtml) return [].
 *   Discover category subpages: regex /href="(\/japon\/[a-z0-9-]+)"/gi,
 *   unique-preserve-order. if 0 → return [].
 *   For each category (until features.length >= 200):
 *     url = absUrl(cat, base); html = GET url (BROWSER_UA, 20 s); skip if empty.
 *     Tile regex:
 *       /<a[^>]+href="(https?:\/\/webcamendirect\.net\/webcam\/(\d+)-
 *         ([a-z0-9-]+)\.html)"[^>]*>\s*<img[^>]+alt="[^"]*\/\s*Jap(?:on|an)"/gi
 *     Dedup by id. Name: try /webcam/<id>-[^"]+\.html"[^>]*>\s*<h3[^>]*>
 *       ([^<]+)<\/h3>/ ; else slug→title (split '-', drop pure-digit parts,
 *       join ' ', capitalise each word's first char), else `webcamendirect <id>`.
 *     centroid = guessCentroidFromText(slug). HONEST DEVIATION from JS: no
 *     coordinate jitter, and no TOKYO/default fallback — if the slug yields no
 *     centroid match (and the tile carries no real per-camera coords), the
 *     camera is SKIPPED rather than planted at a fabricated point. A matched
 *     centroid is emitted exactly (un-jittered) with location_precision
 *     ("prefecture"|"city") and location_approximate=true so consumers know it
 *     is an area anchor, not the camera's GPS.
 *     makeFeature(camera_type='aggregator_webcamendirect',
 *                 discovery_channel='webcamendirect_list', extra: url=detailUrl).
 *   geocodeFeatures(features) — LLM enricher; DEFERRED (skip-and-document):
 *     cameraGeocode is an LLM pass, ported elsewhere as the camera enricher;
 *     omitting it here only defers re-geocoding.
 *
 * Every feature → camera_upsert(channel="webcamendirect_list").  makeFeature
 * parity byte-identical to camera_discovery.c.
 *
 * Index fetch failure → return -1 (fetch fail).  0 categories / 0 cams = 0
 * rows, return 0 (faithful: JS `return []`).
 */
#include "../../lib/geojson.h"
#include "../../lib/jocore.h"
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
#define WED_BASE "https://webcamendirect.net/japon"

/* ── makeFeature (verbatim from camera_discovery.c) ────────────────────────*/
typedef struct { const char *k; const char *sv; int is_num; double nv;
                 int is_null; } kv;
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
    else if (e->is_num) cJSON_AddNumberToObject(p, e->k, e->nv);
    else cJSON_AddItemToObject(p, e->k,
           e->sv ? cJSON_CreateString(e->sv) : cJSON_CreateNull());
  }
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

/* ── shared aggregator helpers ─────────────────────────────────────────────*/
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

/* Centroid keys that are city/locality anchors rather than whole prefectures.
 * Used only to honestly label the approximate point's precision. */
static int key_is_city(const char *k) {
  static const char *cities[] = {
    "sapporo","yokohama","nagoya","kobe","sendai","nara_city","nikko",
    "nagasaki_city","fuji","hakone","asakusa","shibuya","shinjuku", NULL };
  for (int i = 0; cities[i]; i++)
    if (strcmp(k, cities[i]) == 0) return 1;
  return 0;
}
static int guess_centroid(const char *text, double *olat, double *olon,
                          const char **precision) {
  if (!text || !*text) return 0;
  size_t tl = strlen(text);
  char *low = malloc(tl + 1);
  if (!low) return 0;
  for (size_t i = 0; i <= tl; i++) low[i] = jo_lc(text[i]);
  for (size_t i = 0; i < sizeof PREF_CENTROIDS / sizeof *PREF_CENTROIDS; i++) {
    const char *k = PREF_CENTROIDS[i].key;
    char kb[32]; size_t kl = strlen(k);
    if (kl > 5 && strcmp(k + kl - 5, "_city") == 0) kl -= 5;
    if (kl >= sizeof kb) kl = sizeof kb - 1;
    memcpy(kb, k, kl); kb[kl] = 0;
    if (strstr(low, kb)) {
      *olat = PREF_CENTROIDS[i].lat; *olon = PREF_CENTROIDS[i].lon;
      if (precision) *precision = key_is_city(k) ? "city" : "prefecture";
      free(low); return 1;
    }
  }
  free(low);
  return 0;
}
/* absUrl(cat, base): cat is "/japon/<slug>", base origin
 * "https://webcamendirect.net". */
static void abs_url(const char *href, char *out, size_t n) {
  if (href && href[0] == '/')
    snprintf(out, n, "https://webcamendirect.net%s", href);
  else if (href && strncmp(href, "http", 4) == 0)
    snprintf(out, n, "%s", href);
  else
    snprintf(out, n, "https://webcamendirect.net/%s", href ? href : "");
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

/* slug→title: split '-', drop pure-digit segments, join ' ', uppercase the
 * first letter of each word (JS .replace(/\b\w/g,c=>c.toUpperCase())). */
static void slug_title(const char *slug, char *out, size_t n) {
  size_t o = 0; int word_start = 1; int last_was_space = 1;
  /* first pass: emit non-pure-digit tokens joined by space */
  const char *p = slug;
  while (*p) {
    const char *seg = p;
    while (*p && *p != '-') p++;
    int seglen = (int)(p - seg);
    int alldig = seglen > 0;
    for (int i = 0; i < seglen; i++)
      if (!isdigit((unsigned char)seg[i])) { alldig = 0; break; }
    if (seglen > 0 && !alldig) {
      if (o > 0 && !last_was_space && o + 1 < n) { out[o++] = ' '; }
      for (int i = 0; i < seglen && o + 1 < n; i++) out[o++] = seg[i];
      last_was_space = 0;
    }
    if (*p == '-') p++;
  }
  out[o] = 0;
  /* second pass: uppercase first alnum of each word */
  word_start = 1;
  for (size_t i = 0; out[i]; i++) {
    if (out[i] == ' ') { word_start = 1; continue; }
    if (word_start && ((out[i] >= 'a' && out[i] <= 'z')))
      out[i] = (char)(out[i] - 32);
    word_start = 0;
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_client *http = ctx->http;
  int tok = 0;
  char *index = get_ua(http, WED_BASE, 20000, &tok);
  if (!index) return tok ? 0 : -1;     /* !indexHtml→[] (but hard fail→-1) */

  /* categories: /href="(\/japon\/[a-z0-9-]+)"/gi unique-preserve-order. */
  char (*cats)[128] = NULL;
  int ncats = 0, capcats = 0;
  const char *s = index;
  const char *h;
  while ((h = strstr(s, "href=\"/japon/")) != NULL) {
    const char *v = h + 6;                 /* at /japon/ */
    const char *slug = v + 7;              /* after "/japon/" */
    int sl = 0;
    while (slug[sl] && ((slug[sl] >= 'a' && slug[sl] <= 'z') ||
                        (slug[sl] >= '0' && slug[sl] <= '9') ||
                        slug[sl] == '-')) sl++;
    if (sl > 0 && slug[sl] == '"') {
      int total = 7 + sl;                  /* "/japon/" + slug */
      char cb[128];
      int cl = total < (int)sizeof cb - 1 ? total : (int)sizeof cb - 1;
      memcpy(cb, v, (size_t)cl); cb[cl] = 0;
      int dup = 0;
      for (int j = 0; j < ncats; j++)
        if (strcmp(cats[j], cb) == 0) { dup = 1; break; }
      if (!dup) {
        if (ncats >= capcats) {
          capcats = capcats ? capcats * 2 : 16;
          void *nc = realloc(cats, (size_t)capcats * sizeof *cats);
          if (!nc) { free(cats); free(index); return -1; }
          cats = nc;
        }
        snprintf(cats[ncats++], 128, "%s", cb);
      }
    }
    s = h + 6;
  }
  free(index);
  if (ncats == 0) { free(cats); return 0; }   /* return [] */

  cJSON **feats = NULL;
  int nf = 0, capf = 0;
  char (*seen_ids)[24] = NULL;
  int nseen = 0, capseen = 0;

  for (int ci = 0; ci < ncats && nf < 200; ci++) {
    char url[256];
    abs_url(cats[ci], url, sizeof url);
    char *html = get_ua(http, url, 20000, NULL);
    if (!html) continue;
    const char *p = html;
    const char *a;
    while (nf < 200 && (a = strstr(p, "<a")) != NULL) {
      const char *gt = strchr(a, '>');
      const char *href = strstr(a, "href=\"https://webcamendirect.net/webcam/");
      if (!href || (gt && href > gt)) { p = a + 2; continue; }
      const char *u = href + 6;          /* at https://... */
      const char *idp = u + strlen("https://webcamendirect.net/webcam/");
      char idbuf[24]; int il = 0;
      while (idp[il] >= '0' && idp[il] <= '9' && il < (int)sizeof idbuf - 1)
        { idbuf[il] = idp[il]; il++; }
      if (il == 0 || idp[il] != '-') { p = a + 2; continue; }
      idbuf[il] = 0;
      const char *slugp = idp + il + 1;
      int sl = 0;
      while (slugp[sl] && ((slugp[sl] >= 'a' && slugp[sl] <= 'z') ||
                           (slugp[sl] >= '0' && slugp[sl] <= '9') ||
                           slugp[sl] == '-')) sl++;
      if (sl == 0 || strncmp(slugp + sl, ".html\"", 6) != 0) {
        p = a + 2; continue;
      }
      int detail_len = (int)(slugp + sl + 5 - u);   /* incl ".html" */
      /* After the anchor's '>' must come optional ws then <img ... alt="..
       * / Jap(on|an)" */
      const char *tagclose = strchr(slugp + sl, '>');
      if (!tagclose) { p = a + 2; continue; }
      const char *q = tagclose + 1;
      while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') q++;
      if (strncmp(q, "<img", 4) != 0) { p = a + 2; continue; }
      const char *imgend = strchr(q, '>');
      const char *alt = strstr(q, "alt=\"");
      if (!alt || (imgend && alt > imgend)) { p = a + 2; continue; }
      alt += 5;
      const char *altend = strchr(alt, '"');
      if (!altend) { p = a + 2; continue; }
      /* alt="[^"]*\/\s*Jap(?:on|an)" — must end with "/ Jap(on|an)" possibly
       * with whitespace after the slash. Find last '/' before altend. */
      int alt_ok = 0;
      for (const char *z = altend - 1; z >= alt; z--) {
        if (*z == '/') {
          const char *w = z + 1;
          while (w < altend && (*w==' '||*w=='\t'||*w=='\n'||*w=='\r')) w++;
          if ((altend - w) == 5 &&
              (strncmp(w, "Japon", 5) == 0 || strncmp(w, "Japan", 5) == 0))
            alt_ok = 1;
          break;
        }
      }
      if (!alt_ok) { p = a + 2; continue; }

      int dup = 0;
      for (int j = 0; j < nseen; j++)
        if (strcmp(seen_ids[j], idbuf) == 0) { dup = 1; break; }
      if (dup) { p = altend; continue; }
      if (nseen >= capseen) {
        capseen = capseen ? capseen * 2 : 64;
        void *ns = realloc(seen_ids, (size_t)capseen * sizeof *seen_ids);
        if (!ns) goto oom;
        seen_ids = ns;
      }
      snprintf(seen_ids[nseen++], 24, "%s", idbuf);

      char detail[256];
      int dl = detail_len < (int)sizeof detail - 1 ? detail_len
                                                   : (int)sizeof detail - 1;
      memcpy(detail, u, (size_t)dl); detail[dl] = 0;

      /* name: search whole html for webcam/<id>-...html"...><h3..>NAME</h3> */
      char name[256]; name[0] = 0;
      char npat[64];
      snprintf(npat, sizeof npat, "webcam/%s-", idbuf);
      const char *np = strstr(html, npat);
      while (np) {
        const char *dot = strstr(np, ".html\"");
        if (dot) {
          const char *gt2 = strchr(dot, '>');
          if (gt2) {
            const char *r2 = gt2 + 1;
            while (*r2==' '||*r2=='\t'||*r2=='\n'||*r2=='\r') r2++;
            if (strncmp(r2, "<h3", 3) == 0) {
              const char *h3gt = strchr(r2, '>');
              if (h3gt) {
                const char *h3e = strstr(h3gt + 1, "</h3>");
                if (h3e) {
                  int ln = (int)(h3e - (h3gt + 1));
                  /* [^<]+ — must contain no '<' */
                  int ok = ln > 0;
                  for (int z = 0; z < ln; z++)
                    if (h3gt[1 + z] == '<') { ok = 0; break; }
                  if (ok) {
                    int cn = ln < (int)sizeof name - 1 ? ln
                                                       : (int)sizeof name - 1;
                    memcpy(name, h3gt + 1, (size_t)cn); name[cn] = 0;
                    /* JS .trim() */
                    int st = 0; while (name[st]==' '||name[st]=='\t'||
                                       name[st]=='\n'||name[st]=='\r') st++;
                    int en = (int)strlen(name);
                    while (en>st && (name[en-1]==' '||name[en-1]=='\t'||
                                     name[en-1]=='\n'||name[en-1]=='\r')) en--;
                    memmove(name, name+st, (size_t)(en-st)); name[en-st]=0;
                  }
                }
              }
            }
          }
        }
        break;
      }
      if (!name[0]) {
        char slug[160];
        int cn = sl < (int)sizeof slug - 1 ? sl : (int)sizeof slug - 1;
        memcpy(slug, slugp, (size_t)cn); slug[cn] = 0;
        slug_title(slug, name, sizeof name);
        if (!name[0])
          snprintf(name, sizeof name, "webcamendirect %s", idbuf);
      }

      char slug2[160];
      int cn2 = sl < (int)sizeof slug2 - 1 ? sl : (int)sizeof slug2 - 1;
      memcpy(slug2, slugp, (size_t)cn2); slug2[cn2] = 0;
      /* This tile scrape carries no real per-camera coordinates; the only
       * location signal is the place name in the slug. Without a centroid
       * match there is no honest location, so skip rather than plant a
       * default point. The matched centroid is an area anchor, not the
       * camera's GPS — emit it exactly (no jitter) and flag it approximate. */
      double lat, lon;
      const char *precision = NULL;
      if (!guess_centroid(slug2, &lat, &lon, &precision)) { p = altend; continue; }
      kv ex[2];
      ex[0].k="url"; ex[0].is_num=0; ex[0].is_null=0; ex[0].sv=detail;
        ex[0].nv=0;
      ex[1].k="geo_precision"; ex[1].is_num=0; ex[1].is_null=0;
        ex[1].sv=precision; ex[1].nv=0;
      if (nf >= capf) {
        capf = capf ? capf * 2 : 64;
        void *nfp = realloc(feats, (size_t)capf * sizeof *feats);
        if (!nfp) goto oom;
        feats = nfp;
      }
      cJSON *feat = make_feature(lat, lon, name,
                                 "aggregator_webcamendirect",
                                 "webcamendirect_list", ex, 2);
      /* honest flag: centroid is an area anchor, not the camera's GPS */
      cJSON *fp = cJSON_GetObjectItem(feat, "properties");
      if (fp) cJSON_AddBoolToObject(fp, "geo_uncertain", 1);
      feats[nf++] = feat;
      p = altend;
      continue;
    oom:
      free(html);
      for (int j = 0; j < nf; j++) cJSON_Delete(feats[j]);
      free(feats); free(seen_ids); free(cats);
      return -1;
    }
    free(html);
  }
  free(cats);
  free(seen_ids);

  /* geocodeFeatures(features): LLM enricher — DEFERRED (see header). */
  int count = 0;
  for (int i = 0; i < nf; i++) {
    if (camera_upsert(ctx->db, sink, feats[i], "webcamendirect_list") >= 0)
      count++;
    cJSON_Delete(feats[i]);
  }
  free(feats);
  fprintf(stderr, "[cam-webcamendirect_list] %d cats, %d cams upserted\n",
          ncats, count);
  return 0;
}

static const source_def cam_webcamendirect_list_def = {
  .id = "cam-webcamendirect_list", .collector = "camera-discovery",
  .name = "Camera discovery — Webcam-en-direct list",
  .name_ja = "カメラ探索 — Webcam-en-direct list",
   .layer = "cameras",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cam_webcamendirect_list_def)
