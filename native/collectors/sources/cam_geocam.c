/* collectors/infrastructure/sources/cam_geocam.c
 *
 * Registered source_def — faithful port of the `geocam` discovery channel
 * from server/src/collectors/cameraDiscovery.js (fromGeocam()).
 *
 * JS: fetch base + ?page=2 + ?page=3 (plain fetchText, browser UA), join,
 * scan `/<a href="(/en/online/[a-z0-9-]+/?)">(inner)</a>/gi` (dedupe by
 * href, cap 80), label = html_strip(inner). The listing exposes no real
 * per-camera coordinates, so location comes only from a place-name match in
 * the label: guessCentroidFromText(label). The JS fabrication (TOKYO_CENTROID
 * default + jitterAround) is intentionally NOT ported — we emit the matched
 * centroid EXACTLY, flagged location_approximate (area, not GPS), and SKIP
 * cameras with no recognized place. makeFeature(camera_type='aggregator_geocam',
 * discovery_channel='geocam', extra: url). LLM geocodeFeatures is owned by the
 * runner / camera_store, not this source.
 *
 * Each Feature is emitted via camera_upsert(...,"geocam") so the
 * discovery_channels[] union + existing-non-null-wins merge + seen_count++
 * fire identically to cameraRunner.js.
 *
 * geocam.ru is a plain server-rendered listing (no Cloudflare JS challenge),
 * so feed_get_text reproduces fromGeocam's plain fetchText path faithfully.
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

/* ── makeFeature parity (cameraDiscovery.js:48-68) ─────────────────────────*/
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


/* absUrl(href, base): new URL(href, base).href. The geocam hrefs are always
 * site-absolute paths ("/en/online/..."), so this resolves to
 * scheme://host + href. Derive scheme://host from base. */
static void abs_url(const char *href, const char *base, char *out, size_t n) {
  if (href && (strncmp(href, "http://", 7) == 0 ||
                strncmp(href, "https://", 8) == 0)) {
    snprintf(out, n, "%s", href);
    return;
  }
  /* scheme://host = up to the 3rd '/' of base */
  const char *p = base;
  int slashes = 0;
  while (*p) {
    if (*p == '/') { slashes++; if (slashes == 3) break; }
    p++;
  }
  size_t hostlen = (size_t)(p - base);
  if (href && href[0] == '/') {
    snprintf(out, n, "%.*s%s", (int)hostlen, base, href);
  } else {
    snprintf(out, n, "%.*s/%s", (int)hostlen, base, href ? href : "");
  }
}

/* ── anchor scanner ────────────────────────────────────────────────────────
 * Faithful equivalent of /<a[^>]+href="([^"]*)"[^>]*>([\s\S]*?)<\/a>/gi :
 * find next `<a `, capture full opening tag up to '>', pull href="..." from
 * it via html_attr, then capture INNER up to the first `</a>` (non-greedy,
 * matches the JS `[\s\S]*?`). Returns pointer past `</a>` (feed back as
 * `from`), or NULL. href[]/inner copied out (truncated to buffer). The JS
 * `[^>]+` between `<a` and href means there must be at least one attr char;
 * any real anchor satisfies this — we just require a space after `a`. */
static const char *next_anchor(const char *from, char *href, size_t hn,
                               char **inner_out) {
  *inner_out = NULL;
  href[0] = 0;
  for (const char *p = from; (p = strstr(p, "<a")) != NULL; p++) {
    char d = p[2];
    if (d != ' ' && d != '\t' && d != '\n' && d != '\r') continue; /* <abbr> */
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

/* JS path gate: /en/online/[a-z0-9-]+/?  (no query, no extra segments) */
static int href_path_ok(const char *href) {
  /* must equal /en/online/<[a-z0-9-]+>/?  (no query, no extra segments) */
  const char *pfx = "/en/online/";
  size_t pl = strlen(pfx);
  if (strncmp(href, pfx, pl) != 0) return 0;
  const char *p = href + pl;
  int n = 0;
  while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                *p == '-')) { p++; n++; }
  if (n == 0) return 0;
  if (*p == '/') p++;
  return *p == 0;
}

static const char *GEOCAM_BASE = "https://www.geocam.ru/en/in/japan/";

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* pageUrls = [base, base?page=2, base?page=3] */
  const char *urls[3];
  char u2[256], u3[256];
  snprintf(u2, sizeof u2, "%s?page=2", GEOCAM_BASE);
  snprintf(u3, sizeof u3, "%s?page=3", GEOCAM_BASE);
  urls[0] = GEOCAM_BASE; urls[1] = u2; urls[2] = u3;

  /* fetch all (Promise.all → join('\n') of the truthy ones). If every fetch
   * fails the JS `if(!html) return []` path → 0 cameras; return 0 (ran). */
  char *htmls[3] = {0};
  size_t total = 1;
  int any = 0;
  for (int i = 0; i < 3; i++) {
    htmls[i] = feed_get_text(ctx->http, urls[i], 8000);
    if (htmls[i] && htmls[i][0]) { total += strlen(htmls[i]) + 1; any = 1; }
  }
  if (!any) {
    for (int i = 0; i < 3; i++) free(htmls[i]);
    fprintf(stderr, "[cam-geocam] 0 (no html — likely WAF/empty)\n");
    return 0;
  }
  char *html = malloc(total);
  if (!html) {
    for (int i = 0; i < 3; i++) free(htmls[i]);
    return -1;
  }
  html[0] = 0;
  for (int i = 0; i < 3; i++) {
    if (htmls[i] && htmls[i][0]) {
      if (html[0]) strcat(html, "\n");
      strcat(html, htmls[i]);
    }
    free(htmls[i]);
  }

  /* seenHref Set + features cap 80 */
  char **seen = NULL;
  int nseen = 0, seencap = 0;
  int count = 0;
  const char *cur = html;
  char href[512];
  char *inner;
  while (count < 80 &&
         (cur = next_anchor(cur, href, sizeof href, &inner)) != NULL) {
    if (!inner) continue;
    if (!href_path_ok(href)) { free(inner); continue; }

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

    /* The geocam listing exposes no real per-camera coordinates; the only
     * location signal is the place-name in the label. If none matches a
     * known centroid we have NO honest location — skip rather than plant a
     * default point. When it matches, emit the centroid EXACTLY (no jitter)
     * and flag it as an approximate area centroid, not the camera's GPS. */
    double lat, lon;
    const char *precision = NULL;
    if (!cam_centroid_find(label ? label : "", CAM_CENTROID_SCAN_1K, &lat, &lon,
                           &precision)) {
      free(label);
      continue;
    }

    char fullurl[640];
    abs_url(href, GEOCAM_BASE, fullurl, sizeof fullurl);

    const char *nm = (label && label[0]) ? label : "Geocam feed";
    kv ex[3] = {0};
    ex[0].k = "url"; ex[0].sv = fullurl;
    ex[1].k = "geo_precision"; ex[1].sv = precision;
    ex[2].k = "geo_uncertain"; ex[2].is_bool = 1; ex[2].bv = 1;
    cJSON *f = make_feature(lat, lon, nm, "aggregator_geocam", "geocam",
                            ex, 3);
    if (camera_upsert(ctx->db, sink, f, "geocam") >= 0) count++;
    cJSON_Delete(f);
    free(label);
  }

  for (int s = 0; s < nseen; s++) free(seen[s]);
  free(seen);
  free(html);
  fprintf(stderr, "[cam-geocam] emitted %d\n", count);
  return 0;
}

static const source_def cam_geocam_def = {
  .id = "cam-geocam", .collector = "camera-discovery",
  .name = "Camera Discovery: Geocam",
  .name_ja = "カメラ探索: Geocam",
   .layer = "cameras",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cam_geocam_def)
