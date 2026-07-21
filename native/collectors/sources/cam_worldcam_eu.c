/* collectors/infrastructure/sources/cam_worldcam_eu.c
 *
 * Registered source_def — port of ONE cameraDiscovery.js channel:
 *   server/src/collectors/cameraDiscovery.js  fromWorldcamEu()  (sources[]
 *   entry id 'worldcam_eu', label 'WorldCam.eu').
 *
 * JS behaviour reproduced faithfully:
 *   base = NEW_AGGREGATOR_INDEX.worldcam_eu
 *        = 'https://fr.worldcam.eu/webcams/asia/japan'
 *   pageUrls = [base, base/p/2 .. base/p/6]  (WORLDCAM_EU_MAX_PAGES = 6)
 *   GET each (BROWSER_UA, 10 s), join non-empty with "\n".
 *   Anchor regex (per JS):
 *     /<a[^>]+href="(\/webcams\/asia\/japan\/(\d+)-([a-z0-9-]+))"[^>]*>
 *       ([\s\S]*?)<\/a>/gi
 *   Dedup by id; label = html_strip(innerHtml); skip empty-label anchors.
 *   For each cam: GET detail page (absUrl(href,base), BROWSER_UA, 8 s) and
 *   read <meta property="og:latitude" content="-?\d+(\.\d+)?"> / og:longitude;
 *   if both present use them, else guessCentroidFromText(label) ||
 *   TOKYO_CENTROID then jitterAround(centroid, features.length).
 *   makeFeature(camera_type='aggregator_worldcam_eu',
 *               discovery_channel='worldcam_eu', extra: url).
 *   Then upgradeYouTubeStreamUrls(features, 4) — ported SEQUENTIALLY (the JS
 *   4-way Promise.all worker is collapsed to one in-order pass; output is
 *   identical, only wall-time differs).  No geocodeFeatures() call in this JS
 *   channel.
 *
 * Every feature is routed through camera_upsert(channel="worldcam_eu") so the
 * discovery_channels[] union + existing-non-null-wins merge + seen_count++
 * fire exactly as cameraRunner.js's `for (f) upsertCamera(f, channel)`.
 *
 * makeFeature parity is byte-identical to camera_discovery.c (same round4 /
 * uid_tail / js_num / kv / make_feature; the camera_uid + props key order are
 * verbatim).  camera_uid is later overwritten to yt:/ytc: by the YT-upgrade
 * pass exactly like the JS does.
 *
 * The site is NOT Cloudflare-WAF'd but is a foreign aggregator; on a fetch
 * failure of the index set (all pages empty) the channel contributes 0 rows
 * and run() still returns 0 (ran) — the JS `if (!combined) return []` shape.
 * A hard transport failure of the very first GET → return -1 (fetch fail).
 */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BROWSER_UA \
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
  "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
#define WORLDCAM_EU_BASE "https://fr.worldcam.eu/webcams/asia/japan"
#define WORLDCAM_EU_MAX_PAGES 6

/* ── makeFeature (verbatim from camera_discovery.c) ────────────────────────*/
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

/* (js_num intentionally omitted: worldcam_eu emits no numeric extras —
 * its only extra is the string `url`. The shared makeFeature shape is
 * otherwise byte-identical to camera_discovery.c.) */

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

/* ── shared aggregator helpers (port of cameraDiscovery.js helpers) ────────*/
/* NO-FABRICATION: the JS PREFECTURE_CENTROIDS table, guessCentroidFromText and
 * jitterAround (used only to invent coordinates when a camera detail page had
 * no og:lat/lon) have been removed — this collector now emits ONLY cameras
 * with a real scraped coordinate. */

/* JS absUrl(href,base) = new URL(href,base).href. The JS regex only ever
 * captures site-rooted paths ("/webcams/asia/japan/..."), so the resolution
 * is base-origin + path. base = https://fr.worldcam.eu/...  → origin
 * "https://fr.worldcam.eu". */
static void abs_url(const char *href, char *out, size_t n) {
  if (href && href[0] == '/') {
    snprintf(out, n, "https://fr.worldcam.eu%s", href);
  } else if (href && strncmp(href, "http", 4) == 0) {
    snprintf(out, n, "%s", href);
  } else {
    snprintf(out, n, "https://fr.worldcam.eu/%s", href ? href : "");
  }
}

/* html_strip equivalent: replace /<[^>]+>/g → ' ', /\s+/g → ' ', trim. */
static char *strip_html(const char *in, int in_len) {
  if (!in) { char *e = malloc(1); if (e) e[0] = 0; return e; }
  char *buf = malloc((size_t)in_len + 1);
  if (!buf) return NULL;
  int o = 0, intag = 0, prev_sp = 0;
  for (int i = 0; i < in_len; i++) {
    char c = in[i];
    if (c == '<') { intag = 1; continue; }
    if (c == '>') { intag = 0; c = ' '; }
    if (intag) continue;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
        c == '\f' || c == '\v') {
      if (o > 0 && !prev_sp) { buf[o++] = ' '; prev_sp = 1; }
      continue;
    }
    buf[o++] = c; prev_sp = 0;
  }
  while (o > 0 && buf[o - 1] == ' ') o--;
  buf[o] = 0;
  return buf;
}

/* GET with BROWSER_UA (mirrors fetchText({headers:{'User-Agent':BROWSER_UA}})).
 * Returns malloc'd body or NULL.  *transport_ok = 1 if the HTTP exchange
 * completed (any status), 0 on hard failure. */
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

/* ── per-cam record ────────────────────────────────────────────────────────*/
typedef struct {
  char id[24];
  char href[256];
  char *label;       /* malloc'd stripped anchor text */
  int  has_coord;
  double lat, lon;
} cam_rec;

/* Find first `attr="VALUE"` after position p within [s, s+len). Captures into
 * out (size n). Returns 1/0. Used for og:latitude / og:longitude meta. */
static int find_meta(const char *s, const char *prop, char *out, size_t n) {
  /* look for: <meta ... property="<prop>" ... content="<value>" ... > but the
   * JS regex is: property="og:latitude"\s+content="(-?\d+(?:\.\d+)?)" — i.e.
   * property immediately precedes content. We scan for the property token,
   * then the next content=" within the same tag. */
  char needle[64];
  snprintf(needle, sizeof needle, "property=\"%s\"", prop);
  const char *q = strstr(s, needle);
  if (!q) return 0;
  const char *cnt = strstr(q, "content=\"");
  if (!cnt) return 0;
  cnt += 9;
  size_t o = 0;
  /* value is -?\d+(\.\d+)? */
  if (*cnt == '-') { if (o + 1 < n) out[o++] = *cnt; cnt++; }
  int seen_digit = 0;
  while (*cnt && *cnt != '"') {
    char c = *cnt;
    if (c >= '0' && c <= '9') { seen_digit = 1; if (o + 1 < n) out[o++] = c; }
    else if (c == '.') { if (o + 1 < n) out[o++] = c; }
    else break;
    cnt++;
  }
  out[o] = 0;
  return seen_digit;
}

/* ── single-pass sequential YouTube URL upgrade (port of
 *    upgradeYouTubeStreamUrls; concurrency collapsed to 1, output identical).
 *    Mutates feature props url / youtube_id / youtube_channel /
 *    original_page_url / camera_uid exactly as the JS worker does. */
static const char *yt_video_id(const char *html, char *out11) {
  /* YT_ID_RE: youtube(-nocookie)?.com/(embed/(?!live_stream)|watch?...v=) |
   *           youtu.be/  then ([\w-]{11}). Implement by scanning candidates. */
  const char *p = html;
  while ((p = strstr(p, "youtu")) != NULL) {
    const char *id = NULL;
    if (strncmp(p, "youtu.be/", 9) == 0) {
      id = p + 9;
    } else if (strncmp(p, "youtube.com/", 12) == 0 ||
               strncmp(p, "youtube-nocookie.com/", 21) == 0) {
      const char *after = strchr(p, '/'); after = after ? after + 1 : NULL;
      if (after && strncmp(after, "embed/", 6) == 0) {
        const char *e = after + 6;
        if (strncmp(e, "live_stream", 11) != 0) id = e;   /* (?!live_stream) */
      } else if (after && strncmp(after, "watch?", 6) == 0) {
        const char *v = strstr(after, "v=");
        /* v= must be within the query, before a quote/space */
        if (v) {
          const char *stop = v;
          while (*stop && *stop != '"' && *stop != '\'' && *stop != ' ' &&
                 stop > after) break;
          id = v + 2;
        }
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
  /* YT_CHANNEL_LIVE_RE: youtube(-nocookie)?.com/embed/live_stream?...
   *                     channel=(UC[\w-]{22}) */
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
      char newurl[128];
      snprintf(newurl, sizeof newurl,
               "https://www.youtube.com/embed/live_stream?channel=%s", cid);
      cJSON_DeleteItemFromObject(props, "url");
      cJSON_AddStringToObject(props, "url", newurl);
      cJSON_AddStringToObject(props, "youtube_channel", cid);
      char cu[40]; snprintf(cu, sizeof cu, "ytc:%s", cid);
      cJSON_DeleteItemFromObject(props, "camera_uid");
      cJSON_AddStringToObject(props, "camera_uid", cu);
    } else if (yt_video_id(html, vid)) {
      cJSON_AddStringToObject(props, "original_page_url", url);
      char newurl[80];
      snprintf(newurl, sizeof newurl,
               "https://www.youtube.com/watch?v=%s", vid);
      cJSON_DeleteItemFromObject(props, "url");
      cJSON_AddStringToObject(props, "url", newurl);
      cJSON_AddStringToObject(props, "youtube_id", vid);
      char cu[24]; snprintf(cu, sizeof cu, "yt:%s", vid);
      cJSON_DeleteItemFromObject(props, "camera_uid");
      cJSON_AddStringToObject(props, "camera_uid", cu);
    }
    free(html);
  }
}

/* ── run() ─────────────────────────────────────────────────────────────────*/
static int run(const source_ctx *ctx, intel_sink *sink) {
  http_client *http = ctx->http;

  /* pageUrls = [base, base/p/2 .. base/p/6]; GET all, join non-empty "\n". */
  char *combined = NULL;
  size_t comb_len = 0;
  int any_transport_ok = 0;
  for (int pg = 1; pg <= WORLDCAM_EU_MAX_PAGES; pg++) {
    char url[256];
    if (pg == 1) snprintf(url, sizeof url, "%s", WORLDCAM_EU_BASE);
    else snprintf(url, sizeof url, "%s/p/%d", WORLDCAM_EU_BASE, pg);
    int tok = 0;
    char *body = get_ua(http, url, 10000, &tok);   /* .catch(()=>null) */
    if (tok) any_transport_ok = 1;
    if (body && body[0]) {
      size_t bl = strlen(body);
      char *nc = realloc(combined, comb_len + bl + 2);
      if (!nc) { free(body); free(combined); return -1; }
      combined = nc;
      if (comb_len) combined[comb_len++] = '\n';     /* join('\n') */
      memcpy(combined + comb_len, body, bl);
      comb_len += bl;
      combined[comb_len] = 0;
    }
    free(body);
  }
  if (!any_transport_ok && !combined) return -1;     /* hard fetch fail */
  if (!combined) return 0;                            /* if(!combined)→[] */

  /* Anchor scan: /<a[^>]+href="(\/webcams\/asia\/japan\/(\d+)-([a-z0-9-]+))"
   *               [^>]*>([\s\S]*?)<\/a>/gi  — dedup by id, skip empty label. */
  cam_rec *cams = NULL;
  int ncams = 0, capcams = 0;
  const char *s = combined;
  const char *anchor;
  while ((anchor = strstr(s, "<a")) != NULL) {
    /* require [^>]+ then href=" — find href within this opening tag */
    const char *gt = strchr(anchor, '>');
    const char *href = strstr(anchor, "href=\"");
    if (!href || (gt && href > gt)) { s = anchor + 2; continue; }
    href += 6;
    const char *prefix = "/webcams/asia/japan/";
    if (strncmp(href, prefix, strlen(prefix)) != 0) { s = anchor + 2; continue; }
    const char *idp = href + strlen(prefix);
    /* (\d+) */
    char idbuf[24]; int il = 0;
    while (idp[il] >= '0' && idp[il] <= '9' && il < (int)sizeof idbuf - 1) {
      idbuf[il] = idp[il]; il++;
    }
    if (il == 0 || idp[il] != '-') { s = anchor + 2; continue; }
    idbuf[il] = 0;
    /* -([a-z0-9-]+) then closing quote */
    const char *slugp = idp + il + 1;
    int sl = 0;
    while (slugp[sl] && slugp[sl] != '"' &&
           ((slugp[sl] >= 'a' && slugp[sl] <= 'z') ||
            (slugp[sl] >= '0' && slugp[sl] <= '9') ||
            slugp[sl] == '-')) sl++;
    if (sl == 0 || slugp[sl] != '"') { s = anchor + 2; continue; }
    /* href string captured = prefix+id+'-'+slug */
    int href_len = (int)(slugp + sl - href);
    /* find end of opening tag, then inner up to </a> */
    const char *tagclose = strchr(slugp + sl, '>');
    if (!tagclose) break;
    const char *aend = strstr(tagclose + 1, "</a>");
    if (!aend) { s = anchor + 2; continue; }
    int inner_len = (int)(aend - (tagclose + 1));

    /* dedup by id */
    int dup = 0;
    for (int j = 0; j < ncams; j++)
      if (strcmp(cams[j].id, idbuf) == 0) { dup = 1; break; }
    if (dup) { s = aend + 4; continue; }

    char *label = strip_html(tagclose + 1, inner_len);
    if (!label || !label[0]) { free(label); s = aend + 4; continue; }

    if (ncams >= capcams) {
      capcams = capcams ? capcams * 2 : 64;
      cam_rec *nc = realloc(cams, (size_t)capcams * sizeof *cams);
      if (!nc) { free(label); free(cams); free(combined); return -1; }
      cams = nc;
    }
    cam_rec *r = &cams[ncams++];
    snprintf(r->id, sizeof r->id, "%s", idbuf);
    int hl = href_len < (int)sizeof r->href - 1 ? href_len
                                                : (int)sizeof r->href - 1;
    memcpy(r->href, href, (size_t)hl); r->href[hl] = 0;
    r->label = label;
    r->has_coord = 0; r->lat = 0; r->lon = 0;
    s = aend + 4;
  }

  if (ncams == 0) { free(cams); free(combined); return 0; } /* seen.size==0 */

  /* Detail fetch (sequential; JS uses 8-way bounded workers — collapsed). */
  for (int i = 0; i < ncams; i++) {
    char durl[320];
    abs_url(cams[i].href, durl, sizeof durl);
    char *html = get_ua(http, durl, 8000, NULL);
    if (!html) continue;
    char la[32], lo[32];
    if (find_meta(html, "og:latitude", la, sizeof la) &&
        find_meta(html, "og:longitude", lo, sizeof lo)) {
      double dlat = atof(la), dlon = atof(lo);
      if (isfinite(dlat) && isfinite(dlon)) {
        cams[i].has_coord = 1; cams[i].lat = dlat; cams[i].lon = dlon;
      }
    }
    free(html);
  }

  /* Build features (jitter index = features.length, i.e. emitted-so-far). */
  cJSON **feats = malloc((size_t)ncams * sizeof *feats);
  if (!feats) { for (int i=0;i<ncams;i++) free(cams[i].label);
                 free(cams); free(combined); return -1; }
  int nf = 0;
  for (int i = 0; i < ncams; i++) {
    /* NO-FABRICATION: only emit cameras with REAL og:lat/lon scraped from the
     * detail page. The JS centroid-guess + jitter fallback invented
     * coordinates (prefecture-substring centroid, deterministic offset, Tokyo
     * default) — dropped. Cameras without a real coordinate are skipped. */
    if (!cams[i].has_coord) continue;
    double lat = cams[i].lat, lon = cams[i].lon;
    char durl[320];
    abs_url(cams[i].href, durl, sizeof durl);
    kv ex[1];
    ex[0].k = "url"; ex[0].is_num = 0; ex[0].is_null = 0;
      ex[0].sv = durl; ex[0].nv = 0;
    feats[nf++] = make_feature(lat, lon, cams[i].label,
                               "aggregator_worldcam_eu", "worldcam_eu",
                               ex, 1);
  }

  /* upgradeYouTubeStreamUrls(features, 4) — sequential port. */
  upgrade_youtube(http, feats, nf);

  /* Emit each through camera_upsert(channel="worldcam_eu"). */
  int count = 0;
  for (int i = 0; i < nf; i++) {
    if (camera_upsert(ctx->db, sink, feats[i], "worldcam_eu") >= 0) count++;
    cJSON_Delete(feats[i]);
  }
  free(feats);
  for (int i = 0; i < ncams; i++) free(cams[i].label);
  free(cams);
  free(combined);
  fprintf(stderr, "[cam-worldcam-eu] scraped %d cams, upserted %d\n",
          ncams, count);
  return 0;
}

static const source_def cam_worldcam_eu_def = {
  .id = "cam-worldcam_eu", .collector = "infrastructure",
  .name = "Camera discovery — WorldCam.eu",
  .name_ja = "カメラ探索 — WorldCam.eu",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_worldcam_eu_def)
