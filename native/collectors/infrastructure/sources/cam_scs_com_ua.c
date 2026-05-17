/* collectors/infrastructure/sources/cam_scs_com_ua.c
 *
 * Registered source_def — port of ONE cameraDiscovery.js channel:
 *   fromScsComUa()  (sources[] id 'scs_com_ua', label 'scs.com.ua').
 *
 * JS behaviour reproduced faithfully:
 *   baseRoot = NEW_AGGREGATOR_INDEX.scsComUa
 *            = 'https://webcam.scs.com.ua/en/asia/japan/'
 *   MAX = 1000.  for page 1..5 (break if features.length >= MAX):
 *     url = page===1 ? baseRoot : `${baseRoot}page-<page>/`
 *     html = GET (BROWSER_UA, 20 s); continue if empty.
 *     regex:
 *       /<a[^>]+href="(\/en\/asia\/japan\/[^"]+)"[^>]*>
 *         [\s\S]{0,160}?(?:alt|title)="([^"]{3,120})"/gi
 *     Skip href containing '/page-'  OR  '?'  OR  matching /\/(sort|filter)\b/i.
 *     Cross-page dedup by href (seenHref outside the page loop).
 *     label = m[2] tags-stripped + trim.
 *     centroid = guessCentroidFromText(label) || TOKYO; jitterAround(c,len).
 *     makeFeature(camera_type='aggregator_scs_com_ua',
 *                 discovery_channel='scs_com_ua', extra: url=absUrl(href,root)).
 *   upgradeYouTubeStreamUrls(features, 4) — ported SEQUENTIALLY (concurrency
 *     collapsed to one in-order pass; output identical).
 *   geocodeFeatures(features) — LLM enricher, DEFERRED (skip-and-document;
 *     identical pre-enrich emission, not faked).
 *
 * webcam.scs.com.ua is Cloudflare-WAF'd: a datacenter IP typically 403s, body
 * empty → 0 rows (faithful: JS `if (!html) continue;`).  run() returns 0
 * (ran) even with 0 rows; hard transport failure on every page → return -1.
 *
 * Every feature → camera_upsert(channel="scs_com_ua").  makeFeature parity
 * byte-identical to camera_discovery.c.
 */
#include "../../../source.h"
#include "../../../core/camera_store.h"
#include "../../../core/httpclient.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define BROWSER_UA \
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
  "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
#define SCS_ROOT "https://webcam.scs.com.ua/en/asia/japan/"
#define SCS_MAX 1000

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
static const double TOKYO_LAT = 35.6762, TOKYO_LON = 139.6503;
static char lc(char c) { return (c>='A'&&c<='Z')?(char)(c+32):c; }
static int guess_centroid(const char *text, double *olat, double *olon) {
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
      free(low); return 1;
    }
  }
  free(low);
  return 0;
}
static void jitter_around(double lat, double lon, int idx,
                          double *olat, double *olon) {
  double dx = ((idx * 17) % 31) * 0.003 - 0.045;
  double dy = ((idx * 13) % 29) * 0.003 - 0.04;
  *olat = lat + dy; *olon = lon + dx;
}
/* absUrl(href, baseRoot): href rooted "/en/asia/japan/..."; origin
 * "https://webcam.scs.com.ua". */
static void abs_url(const char *href, char *out, size_t n) {
  if (href && href[0] == '/')
    snprintf(out, n, "https://webcam.scs.com.ua%s", href);
  else if (href && strncmp(href, "http", 4) == 0)
    snprintf(out, n, "%s", href);
  else
    snprintf(out, n, "https://webcam.scs.com.ua/%s", href ? href : "");
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
/* label = m[2].replace(/<[^>]+>/g,'').trim()  — note: NO \s+→' ' here, matches
 * JS exactly (only tag-strip + trim). */
static void strip_tags_trim(const char *in, char *out, size_t n) {
  size_t o = 0; int intag = 0;
  for (size_t i = 0; in[i] && o + 1 < n; i++) {
    char c = in[i];
    if (c == '<') { intag = 1; continue; }
    if (c == '>') { intag = 0; continue; }
    if (intag) continue;
    out[o++] = c;
  }
  out[o] = 0;
  size_t st = 0;
  while (out[st]==' '||out[st]=='\t'||out[st]=='\n'||out[st]=='\r'||
         out[st]=='\f'||out[st]=='\v') st++;
  size_t en = strlen(out);
  while (en>st && (out[en-1]==' '||out[en-1]=='\t'||out[en-1]=='\n'||
                   out[en-1]=='\r'||out[en-1]=='\f'||out[en-1]=='\v')) en--;
  memmove(out, out+st, en-st); out[en-st]=0;
}

/* ── sequential YouTube URL upgrade (port of upgradeYouTubeStreamUrls) ─────*/
static const char *yt_video_id(const char *html, char *out11) {
  const char *p = html;
  while ((p = strstr(p, "youtu")) != NULL) {
    const char *id = NULL;
    if (strncmp(p, "youtu.be/", 9) == 0) id = p + 9;
    else if (strncmp(p, "youtube.com/", 12) == 0 ||
             strncmp(p, "youtube-nocookie.com/", 21) == 0) {
      const char *after = strchr(p, '/'); after = after ? after + 1 : NULL;
      if (after && strncmp(after, "embed/", 6) == 0) {
        const char *e = after + 6;
        if (strncmp(e, "live_stream", 11) != 0) id = e;
      } else if (after && strncmp(after, "watch?", 6) == 0) {
        const char *v = strstr(after, "v=");
        if (v) id = v + 2;
      }
    }
    if (id) {
      int k = 0;
      while (k < 11 && id[k] &&
             ((id[k] >= 'A' && id[k] <= 'Z') ||
              (id[k] >= 'a' && id[k] <= 'z') ||
              (id[k] >= '0' && id[k] <= '9') ||
              id[k] == '_' || id[k] == '-')) k++;
      if (k == 11) { memcpy(out11, id, 11); out11[11] = 0; return out11; }
    }
    p += 5;
  }
  return NULL;
}
static const char *yt_channel_id(const char *html, char *out24) {
  const char *p = html;
  while ((p = strstr(p, "embed/live_stream")) != NULL) {
    const char *ch = strstr(p, "channel=");
    if (ch && (ch - p) < 400) {
      ch += 8;
      if (ch[0] == 'U' && ch[1] == 'C') {
        int k = 2;
        while (k < 24 && ch[k] &&
               ((ch[k] >= 'A' && ch[k] <= 'Z') ||
                (ch[k] >= 'a' && ch[k] <= 'z') ||
                (ch[k] >= '0' && ch[k] <= '9') ||
                ch[k] == '_' || ch[k] == '-')) k++;
        if (k == 24) { memcpy(out24, ch, 24); out24[24] = 0; return out24; }
      }
    }
    p += 17;
  }
  return NULL;
}
static void upgrade_youtube(http_client *http, cJSON **feats, int nfeats) {
  for (int i = 0; i < nfeats; i++) {
    cJSON *props = cJSON_GetObjectItem(feats[i], "properties");
    cJSON *uj = props ? cJSON_GetObjectItem(props, "url") : NULL;
    if (!uj || !cJSON_IsString(uj) || !uj->valuestring[0]) continue;
    char url[512];
    snprintf(url, sizeof url, "%s", uj->valuestring);
    char *html = get_ua(http, url, 6000, NULL);
    if (!html) continue;
    char cid[32], vid[16];
    if (yt_channel_id(html, cid)) {
      cJSON_AddStringToObject(props, "original_page_url", url);
      char nu[128];
      snprintf(nu, sizeof nu,
               "https://www.youtube.com/embed/live_stream?channel=%s", cid);
      cJSON_DeleteItemFromObject(props, "url");
      cJSON_AddStringToObject(props, "url", nu);
      cJSON_AddStringToObject(props, "youtube_channel", cid);
      char cu[40]; snprintf(cu, sizeof cu, "ytc:%s", cid);
      cJSON_DeleteItemFromObject(props, "camera_uid");
      cJSON_AddStringToObject(props, "camera_uid", cu);
    } else if (yt_video_id(html, vid)) {
      cJSON_AddStringToObject(props, "original_page_url", url);
      char nu[80];
      snprintf(nu, sizeof nu, "https://www.youtube.com/watch?v=%s", vid);
      cJSON_DeleteItemFromObject(props, "url");
      cJSON_AddStringToObject(props, "url", nu);
      cJSON_AddStringToObject(props, "youtube_id", vid);
      char cu[24]; snprintf(cu, sizeof cu, "yt:%s", vid);
      cJSON_DeleteItemFromObject(props, "camera_uid");
      cJSON_AddStringToObject(props, "camera_uid", cu);
    }
    free(html);
  }
}

/* /\/(sort|filter)\b/i test against href. */
static int has_sort_filter(const char *h) {
  for (const char *q = h; *q; q++) {
    if (*q == '/') {
      const char *r = q + 1;
      if ((strncmp(r,"sort",4)==0 &&
           !(isalnum((unsigned char)r[4])||r[4]=='_')) ||
          (strncmp(r,"filter",6)==0 &&
           !(isalnum((unsigned char)r[6])||r[6]=='_')))
        return 1;
      /* case-insensitive: also check upper */
      char b[7]; int bl = 0;
      for (; bl < 6 && r[bl]; bl++) b[bl] = lc(r[bl]);
      b[bl] = 0;
      if ((strncmp(b,"sort",4)==0 &&
           !(isalnum((unsigned char)r[4])||r[4]=='_')) ||
          (strncmp(b,"filter",6)==0 &&
           !(isalnum((unsigned char)r[6])||r[6]=='_')))
        return 1;
    }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_client *http = ctx->http;
  cJSON **feats = NULL;
  int nf = 0, capf = 0;
  char **seen = NULL;
  int nseen = 0, capseen = 0;
  int any_transport_ok = 0, any_body = 0;

  for (int page = 1; page <= 5 && nf < SCS_MAX; page++) {
    char url[160];
    if (page == 1) snprintf(url, sizeof url, "%s", SCS_ROOT);
    else snprintf(url, sizeof url, "%spage-%d/", SCS_ROOT, page);
    int tok = 0;
    char *html = get_ua(http, url, 20000, &tok);
    if (tok) any_transport_ok = 1;
    if (!html) continue;
    any_body = 1;

    const char *p = html;
    const char *a;
    while (nf < SCS_MAX && (a = strstr(p, "<a")) != NULL) {
      const char *gt = strchr(a, '>');
      const char *href = strstr(a, "href=\"/en/asia/japan/");
      if (!href || (gt && href > gt)) { p = a + 2; continue; }
      const char *u = href + 6;                 /* at /en/asia/japan/ */
      const char *qe = strchr(u, '"');          /* [^"]+ */
      if (!qe || qe == u) { p = a + 2; continue; }
      int href_len = (int)(qe - u);
      char hbuf[512];
      int hl = href_len < (int)sizeof hbuf - 1 ? href_len
                                               : (int)sizeof hbuf - 1;
      memcpy(hbuf, u, (size_t)hl); hbuf[hl] = 0;

      /* The opening <a ...> ends at the next '>'. Then within
       * [\s\S]{0,160}? before the FIRST (alt|title)=" capture. */
      const char *tagclose = strchr(qe, '>');
      if (!tagclose) break;
      const char *scan = tagclose + 1;
      const char *limit = scan + 160;
      const char *altq = NULL; int is_alt = 0;
      for (const char *z = scan; z <= limit && *z; z++) {
        if (strncmp(z, "alt=\"", 5) == 0) { altq = z + 5; is_alt = 1; break; }
        if (strncmp(z, "title=\"", 7) == 0) { altq = z + 7; break; }
      }
      (void)is_alt;
      if (!altq) { p = tagclose + 1; continue; }
      const char *altend = strchr(altq, '"');
      if (!altend) { p = tagclose + 1; continue; }
      int ll = (int)(altend - altq);
      if (ll < 3 || ll > 120) { p = tagclose + 1; continue; } /* {3,120} */

      /* skip filters */
      if (strstr(hbuf, "/page-")) { p = altend; continue; }
      if (strchr(hbuf, '?')) { p = altend; continue; }
      if (has_sort_filter(hbuf)) { p = altend; continue; }

      int dup = 0;
      for (int j = 0; j < nseen; j++)
        if (strcmp(seen[j], hbuf) == 0) { dup = 1; break; }
      if (dup) { p = altend; continue; }
      if (nseen >= capseen) {
        capseen = capseen ? capseen * 2 : 64;
        void *ns = realloc(seen, (size_t)capseen * sizeof *seen);
        if (!ns) goto oom;
        seen = ns;
      }
      seen[nseen++] = strdup(hbuf);

      char rawlabel[256];
      int cl = ll < (int)sizeof rawlabel - 1 ? ll : (int)sizeof rawlabel - 1;
      memcpy(rawlabel, altq, (size_t)cl); rawlabel[cl] = 0;
      char label[256];
      strip_tags_trim(rawlabel, label, sizeof label);

      double clat, clon;
      if (!guess_centroid(label, &clat, &clon)) {
        clat = TOKYO_LAT; clon = TOKYO_LON;
      }
      double lat, lon;
      jitter_around(clat, clon, nf, &lat, &lon);
      char aurl[512];
      abs_url(hbuf, aurl, sizeof aurl);
      kv ex[1];
      ex[0].k="url"; ex[0].is_num=0; ex[0].is_null=0; ex[0].sv=aurl;
        ex[0].nv=0;
      const char *nm = label[0] ? label : "scs webcam"; /* label||'scs webcam' */
      if (nf >= capf) {
        capf = capf ? capf * 2 : 64;
        void *nfp = realloc(feats, (size_t)capf * sizeof *feats);
        if (!nfp) goto oom;
        feats = nfp;
      }
      feats[nf++] = make_feature(lat, lon, nm, "aggregator_scs_com_ua",
                                 "scs_com_ua", ex, 1);
      p = altend;
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

  upgrade_youtube(http, feats, nf);   /* sequential port */

  /* geocodeFeatures(features): LLM enricher — DEFERRED (see header). */
  int count = 0;
  for (int i = 0; i < nf; i++) {
    if (camera_upsert(ctx->db, sink, feats[i], "scs_com_ua") >= 0) count++;
    cJSON_Delete(feats[i]);
  }
  free(feats);
  fprintf(stderr, "[cam-scs_com_ua] %d cams upserted (WAF may yield 0)\n",
          count);
  return 0;
}

static const source_def cam_scs_com_ua_def = {
  .id = "cam-scs_com_ua", .collector = "infrastructure",
  .name = "Camera discovery — scs.com.ua",
  .name_ja = "カメラ探索 — scs.com.ua",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_scs_com_ua_def)
