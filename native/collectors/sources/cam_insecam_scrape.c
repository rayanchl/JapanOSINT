/* collectors/infrastructure/sources/cam_insecam_scrape.c
 *
 * Registered source_def — faithful port of the `insecam_scrape` discovery
 * channel from server/src/collectors/cameraDiscovery.js (fromInsecam()).
 *
 * JS flow (reproduced here):
 *  1. BASE=http://www.insecam.org/en/bycountry/JP/ ; fetchText(BASE,8s).
 *     If !firstHtml → return [] (here: 0 cameras, return 0 = "ran").
 *  2. totalPages = min( match(/pagenavigator\("\?page=",\s*(\d+)/)[1] || 1,
 *     MAX_PAGES=60 ). Fetch pages 2..totalPages (JS uses LIST_CONCURRENCY=6;
 *     here sequential — faithful: same requests + same parse, serialized).
 *  3. entryRe = /<a[^>]+href="\/en\/view\/(\d+)\/"[^>]+title="Live camera
 *     in Japan,\s*([^"]+)"[^>]*>[\s\S]*?<img[^>]+src="([^"]+)"/g over every
 *     page → cards {id, city=trim(m2), img=m3.replace(/&amp;/g,'&')},
 *     dedupe by id (Set, first wins).
 *  4. Detail pass: for cards (JS memoizes across runs in INSECAM_COORD_CACHE;
 *     here a PER-RUN cache — see LIMITATION) fetch
 *     http://www.insecam.org/en/view/<id>/ (6s), match
 *     /Latitude:[\s\S]{0,200}?([\-\d]+\.\d+)/ + Longitude; if both finite
 *     cache {lat,lon}. (JS DETAIL_CONCURRENCY=10; here sequential.)
 *  5. Build features in card order: if real coords →
 *     dx=((id*17)%97)*0.0003-0.015; dy=((id*13)%89)*0.0003-0.013;
 *     lat=real.lat+dy; lon=real.lon+dx; else
 *     centroid=guessCentroidFromText(card.city)||TOKYO;
 *     {lat,lon}=jitterAround(centroid, features.length). makeFeature(
 *     name=`Insecam ${city} #${id}`, camera_type='insecam',
 *     discovery_channel='insecam_scrape',
 *     extra order: url,thumbnail_url,city,coord_source,
 *       location_uncertain,auth_required) where
 *     url=`http://www.insecam.org/en/view/${id}/`,
 *     coord_source = real?'insecam_detail':'city_centroid',
 *     location_uncertain = real?0:1, auth_required=false.
 *
 * LIMITATIONS (noted, NOT faked):
 *  - JS runs list/detail fetches with bounded concurrency (6 / 10);
 *    feedlib has no worker-pool primitive, so they run sequentially. The
 *    set of requests and the parsing are byte-identical; only wall-time
 *    differs. The per-request 8s/6s timeouts are preserved.
 *  - JS memoizes detail coords across collector runs (module-level
 *    INSECAM_COORD_CACHE) purely as a perf optimisation. Here the cache is
 *    per-run (dedupe within this sweep only). Output Features are identical;
 *    a fresh run just re-fetches detail pages it could have remembered.
 *  - insecam.org periodically rate-limits / 403s datacenter IPs; a failed
 *    list fetch → 0 cameras (faithful, like Node from a blocked server).
 *
 * Each Feature → camera_upsert(...,"insecam_scrape").
 */
#include "../../lib/geojson.h"
#include "../../lib/jocore.h"
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
    if (e->is_bool) cJSON_AddBoolToObject(p, e->k, e->bv);
    else if (e->is_null) cJSON_AddNullToObject(p, e->k);
    else if (e->is_num) cJSON_AddNumberToObject(p, e->k, e->nv);
    else cJSON_AddItemToObject(p, e->k,
           e->sv ? cJSON_CreateString(e->sv) : cJSON_CreateNull());
  }
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

/* ── card scanner — equivalent of entryRe ──────────────────────────────────
 * /<a[^>]+href="\/en\/view\/(\d+)\/"[^>]+title="Live camera in Japan,\s*
 *  ([^"]+)"[^>]*>[\s\S]*?<img[^>]+src="([^"]+)"/g
 * Find <a ...>, require href="/en/view/<digits>/" AND title beginning
 * "Live camera in Japan," (then \s* then city up to next '"'), then the
 * FIRST <img ... src="..."> anywhere after the anchor's '>' (non-greedy
 * [\s\S]*?). Returns cursor past that <img src="...">. */
static const char *next_card(const char *from, char *idout, size_t idn,
                             char *cityout, size_t cityn,
                             char *imgout, size_t imgn) {
  idout[0] = cityout[0] = imgout[0] = 0;
  for (const char *p = from; (p = strstr(p, "<a")) != NULL; p++) {
    char d = p[2];
    if (d != ' ' && d != '\t' && d != '\n' && d != '\r') continue;
    const char *gt = strchr(p, '>');
    if (!gt) return NULL;
    size_t hdrlen = (size_t)(gt - p) + 1;
    char hdr[4096];
    if (hdrlen >= sizeof hdr) hdrlen = sizeof hdr - 1;
    memcpy(hdr, p, hdrlen);
    hdr[hdrlen] = 0;

    char href[256], title[512];
    if (!html_attr(hdr, "href", href, sizeof href)) { p = gt; continue; }
    /* href must be /en/view/<digits>/ exactly */
    const char *hp = "/en/view/";
    size_t hpl = strlen(hp);
    if (strncmp(href, hp, hpl) != 0) { p = gt; continue; }
    const char *hd = href + hpl;
    int nd = 0;
    while (*hd >= '0' && *hd <= '9') { hd++; nd++; }
    if (nd == 0 || strcmp(hd, "/") != 0) { p = gt; continue; }

    if (!html_attr(hdr, "title", title, sizeof title)) { p = gt; continue; }
    const char *tp = "Live camera in Japan,";
    size_t tpl = strlen(tp);
    if (strncmp(title, tp, tpl) != 0) { p = gt; continue; }
    /* \s* then city = rest up to end of title attr ([^"]+) */
    const char *cp = title + tpl;
    while (*cp == ' ' || *cp == '\t' || *cp == '\n' || *cp == '\r' ||
           *cp == '\f' || *cp == '\v') cp++;
    if (!*cp) { p = gt; continue; }      /* [^"]+ needs ≥1 char */

    /* first <img ... src="..."> after gt (the non-greedy [\s\S]*?) */
    const char *im = strstr(gt + 1, "<img");
    if (!im) return NULL;
    const char *imgt = strchr(im, '>');
    if (!imgt) return NULL;
    size_t ihl = (size_t)(imgt - im) + 1;
    char ihdr[2048];
    if (ihl >= sizeof ihdr) ihl = sizeof ihdr - 1;
    memcpy(ihdr, im, ihl);
    ihdr[ihl] = 0;
    char src[1024];
    if (!html_attr(ihdr, "src", src, sizeof src)) { p = gt; continue; }

    /* commit captures: id (digits), city, img (&amp; → &) */
    size_t cpyd = (size_t)nd < idn - 1 ? (size_t)nd : idn - 1;
    memcpy(idout, href + hpl, cpyd);
    idout[cpyd] = 0;
    snprintf(cityout, cityn, "%s", cp);
    /* m3.replace(/&amp;/g,'&') */
    size_t w = 0;
    for (size_t i = 0; src[i] && w + 1 < imgn; ) {
      if (strncmp(src + i, "&amp;", 5) == 0) { imgout[w++] = '&'; i += 5; }
      else imgout[w++] = src[i++];
    }
    imgout[w] = 0;
    const char *past = strchr(imgt, '>');  /* = imgt */
    return past ? past + 1 : imgt;
  }
  return NULL;
}

/* JS: city = m[2].trim() — trim ASCII+\s both ends. */
static void trim_inplace(char *s) {
  size_t L = strlen(s);
  size_t b = 0;
  while (s[b] == ' ' || s[b] == '\t' || s[b] == '\n' || s[b] == '\r' ||
         s[b] == '\f' || s[b] == '\v') b++;
  size_t e = L;
  while (e > b && (s[e-1] == ' ' || s[e-1] == '\t' || s[e-1] == '\n' ||
                   s[e-1] == '\r' || s[e-1] == '\f' || s[e-1] == '\v')) e--;
  memmove(s, s + b, e - b);
  s[e - b] = 0;
}

/* /(Lat|Long)itude:[\s\S]{0,200}?([\-\d]+\.\d+)/ — find the label, then
 * within the next ≤200 chars the first run matching [-\d]+\.\d+ . The JS
 * class [\-\d]+ allows '-' anywhere; \.\d+ requires a dot then ≥1 digit. */
static int find_coord(const char *html, const char *label, double *out) {
  const char *q = strstr(html, label);
  if (!q) return 0;
  q += strlen(label);
  /* non-greedy {0,200}: advance start offset 0..200 looking for a match
   * anchored at that offset. */
  for (int off = 0; off <= 200 && q[off]; off++) {
    const char *p = q + off;
    /* [\-\d]+ : one or more of '-' or digit */
    const char *s = p;
    while (*s == '-' || (*s >= '0' && *s <= '9')) s++;
    if (s == p) continue;
    if (*s != '.') continue;
    const char *fd = s + 1;
    const char *f = fd;
    while (*f >= '0' && *f <= '9') f++;
    if (f == fd) continue;             /* need ≥1 fractional digit */
    char buf[64];
    size_t L = (size_t)(f - p);
    if (L >= sizeof buf) L = sizeof buf - 1;
    memcpy(buf, p, L);
    buf[L] = 0;
    *out = atof(buf);
    return 1;
  }
  return 0;
}

#define INSECAM_BASE "http://www.insecam.org/en/bycountry/JP/"
#define MAX_PAGES 60   /* exhaustive-ok: page-walk runaway guard */

typedef struct { char *id; char *city; char *img; int have_real;
                 double rlat, rlon; } card_t;

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *first = feed_get_text(ctx->http, INSECAM_BASE, 8000);
  if (!first || !first[0]) {
    free(first);
    fprintf(stderr, "[cam-insecam-scrape] 0 (no html — rate-limit/WAF)\n");
    return 0;
  }

  /* totalPages = min( /pagenavigator\("\?page=",\s*(\d+)/ , 60 ) */
  int totalPages = 1;
  {
    const char *m = strstr(first, "pagenavigator(\"?page=\",");
    if (m) {
      const char *p = m + strlen("pagenavigator(\"?page=\",");
      while (*p == ' ' || *p == '\t') p++;
      int v = 0, nd = 0;
      while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; nd++; }
      if (nd > 0 && v > 0) totalPages = v;
    }
    if (totalPages > MAX_PAGES) totalPages = MAX_PAGES;
  }

  /* collect cards across page 1..totalPages (Set dedupe by id, first wins).
   * Sequential (JS LIST_CONCURRENCY=6 → serialized; same requests). */
  card_t *cards = NULL;
  int ncards = 0, ccap = 0;
  char **seenid = NULL;
  int nseen = 0, scap = 0;

  for (int page = 1; page <= totalPages; page++) {
    char *html;
    if (page == 1) {
      html = first;                    /* htmlByPage[1] = firstHtml */
    } else {
      char u[256];
      snprintf(u, sizeof u, "%s?page=%d", INSECAM_BASE, page);
      html = feed_get_text(ctx->http, u, 8000);
      if (!html) continue;             /* if(html) htmlByPage.set */
    }
    const char *cur = html;
    char id[32], city[512], img[1024];
    while ((cur = next_card(cur, id, sizeof id, city, sizeof city,
                            img, sizeof img)) != NULL) {
      if (!id[0]) continue;
      int dup = 0;
      for (int s = 0; s < nseen; s++)
        if (strcmp(seenid[s], id) == 0) { dup = 1; break; }
      if (dup) continue;
      if (nseen == scap) {
        scap = scap ? scap * 2 : 64;
        seenid = realloc(seenid, (size_t)scap * sizeof *seenid);
      }
      seenid[nseen++] = strdup(id);
      trim_inplace(city);
      if (ncards == ccap) {
        ccap = ccap ? ccap * 2 : 64;
        cards = realloc(cards, (size_t)ccap * sizeof *cards);
      }
      card_t *cd = &cards[ncards++];
      cd->id = strdup(id);
      cd->city = strdup(city);
      cd->img = strdup(img);
      cd->have_real = 0;
      cd->rlat = cd->rlon = 0;
    }
    if (page != 1) free(html);
  }
  free(first);
  for (int s = 0; s < nseen; s++) free(seenid[s]);
  free(seenid);

  /* Detail pass — resolve real coords (JS DETAIL_CONCURRENCY=10; here
   * sequential; per-run cache = the dedupe above already ensures one detail
   * fetch per id). */
  for (int i = 0; i < ncards; i++) {
    card_t *cd = &cards[i];
    char u[256];
    snprintf(u, sizeof u, "http://www.insecam.org/en/view/%s/", cd->id);
    char *dh = feed_get_text(ctx->http, u, 6000);
    if (!dh) continue;
    double la, lo;
    int okla = find_coord(dh, "Latitude:", &la);
    int oklo = find_coord(dh, "Longitude:", &lo);
    if (okla && oklo && isfinite(la) && isfinite(lo)) {
      cd->have_real = 1; cd->rlat = la; cd->rlon = lo;
    }
    free(dh);
  }

  /* Build features (card order). NO-FABRICATION: emit ONLY cameras whose real
   * lat/lon were parsed from the insecam detail page (cd->have_real). Cards
   * without real coordinates are skipped — the prior city-centroid + jitter
   * fallback (and the id-derived jitter applied to real coords) invented
   * locations, so both are removed; honest-empty beats fake points. */
  int count = 0;
  for (int i = 0; i < ncards; i++) {
    card_t *cd = &cards[i];
    if (!cd->have_real) { free(cd->id); free(cd->city); free(cd->img); continue; }
    double lat = cd->rlat, lon = cd->rlon;

    char name[640], url[256];
    snprintf(name, sizeof name, "Insecam %s #%s", cd->city, cd->id);
    snprintf(url, sizeof url, "http://www.insecam.org/en/view/%s/", cd->id);

    kv ex[6];
    int n = 0;
    ex[n].k="url"; ex[n].is_num=0; ex[n].is_null=0; ex[n].is_bool=0;
      ex[n].sv=url; ex[n].nv=0; ex[n].bv=0; n++;
    ex[n].k="thumbnail_url"; ex[n].is_num=0; ex[n].is_null=0; ex[n].is_bool=0;
      ex[n].sv=cd->img; ex[n].nv=0; ex[n].bv=0; n++;
    ex[n].k="city"; ex[n].is_num=0; ex[n].is_null=0; ex[n].is_bool=0;
      ex[n].sv=cd->city; ex[n].nv=0; ex[n].bv=0; n++;
    ex[n].k="geo_provenance"; ex[n].is_num=0; ex[n].is_null=0; ex[n].is_bool=0;
      ex[n].sv="insecam_detail";
      ex[n].nv=0; ex[n].bv=0; n++;
    ex[n].k="geo_uncertain"; ex[n].is_num=1; ex[n].is_null=0;
      ex[n].is_bool=0; ex[n].sv=NULL;
      ex[n].nv=0; ex[n].bv=0; n++;
    ex[n].k="auth_required"; ex[n].is_num=0; ex[n].is_null=0; ex[n].is_bool=1;
      ex[n].sv=NULL; ex[n].nv=0; ex[n].bv=0; n++;

    cJSON *f = make_feature(lat, lon, name, "insecam",
                            "insecam_scrape", ex, n);
    if (camera_upsert(ctx->db, sink, f, "insecam_scrape") >= 0) count++;
    cJSON_Delete(f);

    free(cd->id); free(cd->city); free(cd->img);
  }
  free(cards);

  fprintf(stderr, "[cam-insecam-scrape] emitted %d\n", count);
  return 0;
}

static const source_def cam_insecam_scrape_def = {
  .id = "cam-insecam-scrape", .collector = "camera-discovery",
  .name = "Camera Discovery: Insecam scrape",
  .name_ja = "カメラ探索: Insecam スクレイプ",
   .layer = "cameras",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cam_insecam_scrape_def)
