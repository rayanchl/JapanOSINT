/* collectors/sources/cam_centroids.inc — the ONE Japanese place-name →
 * area-centroid table shared by the cam_* webcam aggregator collectors.
 * Included, never compiled standalone (the Makefile globs *.c, not *.inc —
 * same trick as av_common.inc / geoeo_common.inc).  -iquote collectors/sources
 * is already in CFLAGS, so `#include "cam_centroids.inc"` works from any depth.
 *
 * Every includer must already have pulled in "lib/jocore.h" (for jo_lc()),
 * <stdlib.h> and <string.h>.  Everything here is `static inline`, so a file
 * that uses only one helper still compiles warning-free under -Wall -Wextra
 * (an unused `static inline` is not diagnosed, an unused plain `static` is).
 *
 * ── WHY this file exists ──────────────────────────────────────────────────
 * Ten collectors (cam_camscape, cam_camstreamer, cam_geocam, cam_scs_com_ua,
 * cam_skylinewebcams, cam_tabi_cam, cam_webcamendirect_list, cam_webcamera24,
 * cam_webcamtaxi, cam_worldcams) each carried a PRIVATE copy of this 60-row
 * table and its substring lookup.  The 60 (key, lat, lon) triples were
 * byte-identical in all ten — but the *precision label* derived from them had
 * drifted into four different implementations, and one of them was wrong.
 *
 * The table's 60 rows are 47 prefecture centroids (indices 0..46) followed by
 * 13 city/landmark anchors (indices 47..59).  The emitted `geo_precision` /
 * `location_precision` property tells a consumer how coarse the point is, so
 * "prefecture" on a city anchor is a false claim about our own accuracy.
 * Nine copies derived it correctly (an index test, an explicit 13-name list,
 * or an explicit `prec` column).  cam_camscape.c derived it from the key's
 * spelling instead:
 *
 *     is_city = (strlen(k) > 5 && strcmp(k + strlen(k) - 5, "_city") == 0)
 *
 * which is true for exactly two of the thirteen city anchors — `nara_city` and
 * `nagasaki_city`, the two that carry the suffix only to disambiguate them
 * from a same-named prefecture.  The other ELEVEN city anchors (sapporo,
 * yokohama, nagoya, kobe, sendai, nikko, fuji, hakone, asakusa, shibuya,
 * shinjuku) were emitted as "prefecture" — advertising a camera's location as
 * coarser than the point we actually placed it at.  That is the incident this
 * file closes: the precision now lives in the table, next to the coordinate it
 * describes, so it cannot be re-derived wrongly.
 *
 * Nothing here invents a location.  A miss returns 0 / -1 and the caller
 * SKIPS the camera — no default point, no TOKYO fallback, no jitter (the JS
 * originals did all three; the C ports deliberately do not).  A hit is the
 * named area's real centroid, emitted EXACTLY, and every caller flags it
 * `geo_uncertain` / `location_approximate` because an area centroid is not the
 * camera's GPS.
 *
 * ── the "_city" suffix strip ──────────────────────────────────────────────
 * cam_centroid_find() strips a trailing "_city" from the KEY before the
 * strstr().  That is load-bearing: it is what makes the text "Nara" match the
 * `nara_city` row.  Table order is equally load-bearing — the scan is
 * first-match-wins, so `nara` (index 28, the prefecture) is tested before
 * `nara_city` (index 52) and wins, and likewise `nagasaki` before
 * `nagasaki_city`.  Do not sort this table.
 *
 * ── scan_cap ──────────────────────────────────────────────────────────────
 * The ten originals also disagreed on how much of the text they lowercased
 * before scanning: four built an unbounded heap copy, six used a 1024-byte
 * jo_lower_buf() and so silently ignored any place name past byte 1023.
 * `scan_cap` preserves each caller's own behaviour exactly (0 = unbounded,
 * CAM_CENTROID_SCAN_1K = the six-file legacy cap) rather than quietly moving
 * cameras in or out of the result set as a side effect of unification.
 * Reconciling that difference is a separate, deliberate change: uncapping is
 * not automatically better, because an <a> with no closing tag makes `inner`
 * swallow the rest of the page, and an unbounded scan would then match a
 * place name from unrelated markup.
 */
#ifndef JO_CAM_CENTROIDS_INC
#define JO_CAM_CENTROIDS_INC

/* Legacy scan bound of the six collectors that lowercased through a
 * jo_lower_buf(text, low, 1024). */
#define CAM_CENTROID_SCAN_1K ((size_t)1024)

typedef struct { const char *key; double lat, lon; const char *prec; }
        cam_centroid;

/* 47 prefecture centroids (0..46) then 13 city/landmark anchors (47..59).
 * ORDER IS SIGNIFICANT — see the header comment. */
static const cam_centroid CAM_CENTROIDS[] = {
  {"hokkaido",43.2203,142.8635,"prefecture"},{"aomori",40.7644,140.7400,"prefecture"},
  {"iwate",39.7036,141.1527,"prefecture"},{"miyagi",38.2688,140.8719,"prefecture"},
  {"akita",39.7186,140.1024,"prefecture"},{"yamagata",38.2404,140.3636,"prefecture"},
  {"fukushima",37.7503,140.4677,"prefecture"},{"ibaraki",36.3418,140.4468,"prefecture"},
  {"tochigi",36.5657,139.8836,"prefecture"},{"gunma",36.3906,139.0604,"prefecture"},
  {"saitama",35.8572,139.6489,"prefecture"},{"chiba",35.6050,140.1234,"prefecture"},
  {"tokyo",35.6762,139.6503,"prefecture"},{"kanagawa",35.4478,139.6425,"prefecture"},
  {"niigata",37.9161,139.0364,"prefecture"},{"toyama",36.6953,137.2113,"prefecture"},
  {"ishikawa",36.5946,136.6256,"prefecture"},{"fukui",36.0652,136.2216,"prefecture"},
  {"yamanashi",35.6639,138.5684,"prefecture"},{"nagano",36.6513,138.1810,"prefecture"},
  {"gifu",35.3911,136.7222,"prefecture"},{"shizuoka",34.9769,138.3831,"prefecture"},
  {"aichi",35.1802,136.9066,"prefecture"},{"mie",34.7303,136.5086,"prefecture"},
  {"shiga",35.0045,135.8686,"prefecture"},{"kyoto",35.0116,135.7681,"prefecture"},
  {"osaka",34.6937,135.5023,"prefecture"},{"hyogo",34.6913,135.1830,"prefecture"},
  {"nara",34.6851,135.8050,"prefecture"},{"wakayama",34.2261,135.1675,"prefecture"},
  {"tottori",35.5036,134.2383,"prefecture"},{"shimane",35.4723,133.0505,"prefecture"},
  {"okayama",34.6618,133.9344,"prefecture"},{"hiroshima",34.3966,132.4596,"prefecture"},
  {"yamaguchi",34.1859,131.4706,"prefecture"},{"tokushima",34.0658,134.5593,"prefecture"},
  {"kagawa",34.3401,134.0434,"prefecture"},{"ehime",33.8416,132.7657,"prefecture"},
  {"kochi",33.5597,133.5311,"prefecture"},{"fukuoka",33.5902,130.4017,"prefecture"},
  {"saga",33.2494,130.2988,"prefecture"},{"nagasaki",32.7448,129.8737,"prefecture"},
  {"kumamoto",32.7898,130.7417,"prefecture"},{"oita",33.2382,131.6126,"prefecture"},
  {"miyazaki",31.9111,131.4239,"prefecture"},{"kagoshima",31.5602,130.5581,"prefecture"},
  {"okinawa",26.3344,127.8056,"prefecture"},{"sapporo",43.0642,141.3469,"city"},
  {"yokohama",35.4437,139.6380,"city"},{"nagoya",35.1815,136.9066,"city"},
  {"kobe",34.6901,135.1955,"city"},{"sendai",38.2682,140.8694,"city"},
  {"nara_city",34.6851,135.8050,"city"},{"nikko",36.7581,139.6117,"city"},
  {"nagasaki_city",32.7448,129.8737,"city"},{"fuji",35.3606,138.7274,"city"},
  {"hakone",35.2323,139.1069,"city"},{"asakusa",35.7148,139.7967,"city"},
  {"shibuya",35.6580,139.7016,"city"},{"shinjuku",35.6938,139.7034,"city"},
};
#define CAM_CENTROID_COUNT (sizeof CAM_CENTROIDS / sizeof *CAM_CENTROIDS)

/* Honest precision of row i: "prefecture" for 0..46, "city" for 47..59. */
static inline const char *cam_centroid_prec(size_t i) {
  return (i < CAM_CENTROID_COUNT) ? CAM_CENTROIDS[i].prec : NULL;
}

/* EXACT key lookup (the JS `PREFECTURE_CENTROIDS[slug]` object index): no
 * lowercasing, no "_city" strip, no substring match.  Returns the row index
 * or -1.  A slug of "nara" therefore hits the PREFECTURE row, which is what
 * a `/en/japan/nara/...` path actually means. */
static inline int cam_centroid_idx_exact(const char *key) {
  if (!key || !*key) return -1;
  for (size_t i = 0; i < CAM_CENTROID_COUNT; i++)
    if (strcmp(CAM_CENTROIDS[i].key, key) == 0) return (int)i;
  return -1;
}

/* guessCentroidFromText: lowercase `text`, then for each row in table order
 * strip a trailing "_city" from the key and test substring containment.
 * FIRST MATCH WINS.  Returns the row index or -1.
 * scan_cap: 0 scans all of `text`; N mirrors a jo_lower_buf(text, low, N),
 * i.e. only the first N-1 bytes are considered. */
static inline int cam_centroid_idx_find(const char *text, size_t scan_cap) {
  if (!text || !*text) return -1;
  size_t tl = strlen(text);
  if (scan_cap && tl > scan_cap - 1) tl = scan_cap - 1;
  char *low = (char *)malloc(tl + 1);
  if (!low) return -1;                      /* OOM: no match, never a guess */
  for (size_t i = 0; i < tl; i++) low[i] = jo_lc(text[i]);
  low[tl] = 0;
  for (size_t i = 0; i < CAM_CENTROID_COUNT; i++) {
    const char *k = CAM_CENTROIDS[i].key;
    char kb[32];
    size_t kl = strlen(k);
    if (kl > 5 && strcmp(k + kl - 5, "_city") == 0) kl -= 5;
    if (kl >= sizeof kb) kl = sizeof kb - 1;
    memcpy(kb, k, kl); kb[kl] = 0;
    if (strstr(low, kb)) { free(low); return (int)i; }
  }
  free(low);
  return -1;
}

/* Value-returning wrappers: 1 on hit (filling *lat and *lon with the EXACT
 * centroid and *prec with the table's own label), 0 on miss.  On a miss the
 * outputs are left untouched — the caller must skip the record. */
static inline int cam_centroid_exact(const char *key, double *lat, double *lon,
                                     const char **prec) {
  int i = cam_centroid_idx_exact(key);
  if (i < 0) return 0;
  *lat = CAM_CENTROIDS[i].lat;
  *lon = CAM_CENTROIDS[i].lon;
  if (prec) *prec = CAM_CENTROIDS[i].prec;
  return 1;
}

static inline int cam_centroid_find(const char *text, size_t scan_cap,
                                    double *lat, double *lon,
                                    const char **prec) {
  int i = cam_centroid_idx_find(text, scan_cap);
  if (i < 0) return 0;
  *lat = CAM_CENTROIDS[i].lat;
  *lon = CAM_CENTROIDS[i].lon;
  if (prec) *prec = CAM_CENTROIDS[i].prec;
  return 1;
}

#endif /* JO_CAM_CENTROIDS_INC */
