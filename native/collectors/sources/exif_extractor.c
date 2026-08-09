/* collectors/osint/sources/exif_extractor.c — EXIF_EXTRACTOR, ported from
 * OSINTsaas image_analysis.c onto the JapanOSINT source ABI. Pivots on an
 * image URL, fetches the bytes, and parses JPEG EXIF (TIFF IFD0 + GPS IFD)
 * for camera make/model, timestamp, and — the OSINT payload — GPS coordinates,
 * which geolocate the photo. Pure C, no external library. Honest-empty when
 * the image has no EXIF/GPS. */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* endian-aware readers over the TIFF block */
/* Per-thread: the TIFF byte order of the image THIS thread is parsing.
 * As a plain global, two concurrent EXIF parses of images with opposite
 * endianness (the "II"/"MM" magic at r16/r32) would read each other's flag
 * and decode every value byte-swapped — including the GPS IFD, i.e. a
 * fabricated coordinate, which is the one class of defect this codebase has
 * already paid to remove once. The scheduler pool and the parallel OSINT
 * dispatch both make that reachable. */
static __thread int g_le;
static uint16_t r16(const uint8_t *p) { return g_le ? (uint16_t)(p[0] | p[1] << 8) : (uint16_t)(p[1] | p[0] << 8); }
static uint32_t r32(const uint8_t *p) {
  return g_le ? ((uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24)
              : ((uint32_t)p[3] | p[2] << 8 | p[1] << 16 | (uint32_t)p[0] << 24);
}
static double rat(const uint8_t *t, size_t tlen, uint32_t off) {
  if (off + 8 > tlen) return 0;
  uint32_t num = r32(t + off), den = r32(t + off + 4);
  return den ? (double)num / (double)den : 0;
}

/* Read an ASCII tag value into out (best-effort, bounded). */
static void read_ascii(const uint8_t *t, size_t tlen, const uint8_t *entry, char *out, size_t cap) {
  uint32_t cnt = r32(entry + 4);
  uint32_t off = (cnt <= 4) ? (uint32_t)(entry + 8 - t) : r32(entry + 8);
  out[0] = 0;
  if (off >= tlen || cnt == 0) return;
  size_t n = cnt - 1; if (n >= cap) n = cap - 1; if (off + n > tlen) n = tlen - off;
  memcpy(out, t + off, n); out[n] = 0;
}

/* GPS coordinate: 3 rationals (deg/min/sec) at the tag's value offset. */
static double gps_coord(const uint8_t *t, size_t tlen, const uint8_t *entry, char ref) {
  uint32_t off = r32(entry + 8);
  if (off + 24 > tlen) return 0;
  double d = rat(t, tlen, off), m = rat(t, tlen, off + 8), s = rat(t, tlen, off + 16);
  double v = d + m / 60.0 + s / 3600.0;
  if (ref == 'S' || ref == 'W') v = -v;
  return v;
}

static int parse_exif(const uint8_t *buf, size_t len, cJSON *out,
                      int *has_geo, double *lat, double *lon) {
  if (len < 4 || buf[0] != 0xFF || buf[1] != 0xD8) return 0;       /* not JPEG */
  size_t pos = 2; const uint8_t *t = NULL; size_t tlen = 0;
  while (pos + 4 < len) {
    if (buf[pos] != 0xFF) break;
    uint8_t marker = buf[pos + 1];
    uint16_t seg = (uint16_t)(buf[pos + 2] << 8 | buf[pos + 3]);
    if (marker == 0xE1 && pos + 4 + 6 <= len && memcmp(buf + pos + 4, "Exif\0\0", 6) == 0) {
      t = buf + pos + 10; tlen = (size_t)seg - 2 - 6;
      if (t + tlen > buf + len) tlen = (size_t)(buf + len - t);
      break;
    }
    if (marker == 0xDA) break;                                     /* start of scan */
    pos += 2 + seg;
  }
  if (!t || tlen < 8) return 0;
  g_le = (t[0] == 'I' && t[1] == 'I');
  uint32_t ifd0 = r32(t + 4);
  if (ifd0 + 2 > tlen) return 0;
  uint16_t n = r16(t + ifd0);
  uint32_t gps_off = 0;
  char make[128] = {0}, model[128] = {0}, dt[64] = {0};
  for (uint16_t i = 0; i < n; i++) {
    const uint8_t *e = t + ifd0 + 2 + (size_t)i * 12;
    if (e + 12 > t + tlen) break;
    uint16_t tag = r16(e);
    if (tag == 0x010F) read_ascii(t, tlen, e, make, sizeof make);
    else if (tag == 0x0110) read_ascii(t, tlen, e, model, sizeof model);
    else if (tag == 0x0132) read_ascii(t, tlen, e, dt, sizeof dt);
    else if (tag == 0x8825) gps_off = r32(e + 8);
  }
  if (*make) cJSON_AddStringToObject(out, "make", make);
  if (*model) cJSON_AddStringToObject(out, "model", model);
  if (*dt) cJSON_AddStringToObject(out, "datetime", dt);

  if (gps_off && gps_off + 2 <= tlen) {
    uint16_t gn = r16(t + gps_off);
    char latref = 'N', lonref = 'E'; const uint8_t *late = NULL, *lone = NULL;
    for (uint16_t i = 0; i < gn; i++) {
      const uint8_t *e = t + gps_off + 2 + (size_t)i * 12;
      if (e + 12 > t + tlen) break;
      uint16_t tag = r16(e);
      if (tag == 0x0001) latref = (char)t[(r32(e + 4) <= 4) ? (size_t)(e + 8 - t) : r32(e + 8)];
      else if (tag == 0x0002) late = e;
      else if (tag == 0x0003) lonref = (char)t[(r32(e + 4) <= 4) ? (size_t)(e + 8 - t) : r32(e + 8)];
      else if (tag == 0x0004) lone = e;
    }
    if (late && lone) {
      double la = gps_coord(t, tlen, late, latref), lo = gps_coord(t, tlen, lone, lonref);
      if (la != 0 || lo != 0) {
        cJSON_AddNumberToObject(out, "latitude", la);
        cJSON_AddNumberToObject(out, "longitude", lo);
        *has_geo = 1; *lat = la; *lon = lo;
      }
    }
  }
  return 1;
}

static int exif_run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = ctx->entity;
  if (!url || strncmp(url, "http", 4) != 0) {
    fprintf(stderr, "[EXIF_EXTRACTOR] entity is not an image URL: %s\n",
            url ? url : "(none)");
    return 0;
  }
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 15000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body || hr.body_len < 4) {
    fprintf(stderr, "[EXIF_EXTRACTOR] fetch failed status=%ld len=%zu %s\n",
            hr.status, hr.body_len, url);
    http_response_free(&hr);
    return 0;
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "source", "image-exif");
  cJSON_AddStringToObject(data, "image_url", url);
  int has_geo = 0; double lat = 0, lon = 0;
  int ok = parse_exif((const uint8_t *)hr.body, hr.body_len, data, &has_geo, &lat, &lon);
  http_response_free(&hr);
  /* nothing useful (no make/model/datetime/gps) → honest empty */
  if (!ok || cJSON_GetArraySize(data) <= 2) {
    fprintf(stderr, "[EXIF_EXTRACTOR] no EXIF payload (jpeg=%d fields=%d) %s\n",
            ok, cJSON_GetArraySize(data) - 2, url);
    cJSON_Delete(data);
    return 0;
  }

  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EXIF_EXTRACTOR");
  cJSON_AddStringToObject(props, "entity", url);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[256], title[160];
  snprintf(rk, sizeof rk, "exif:%s", url);
  snprintf(title, sizeof title, "EXIF%s — image", has_geo ? " (geotagged)" : "");
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = "exif metadata";
  it.link = url; it.has_geo = has_geo; it.lat = lat; it.lon = lon;
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"EXIF_EXTRACTOR\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(data); cJSON_Delete(props);
  fprintf(stderr, "[EXIF_EXTRACTOR] emitted 1 (geo=%d) %s\n", has_geo, url);
  /* audit-09: this used to `return 1` on SUCCESS. core/scheduler.c treats any
   * non-zero rc as status="error" and hands it to anomaly_detect, so every
   * successful extraction quarantined the source. run() returns 0 or -1. */
  return rc >= 0 ? 0 : -1;
}

static const source_def exif_def = {
  .id = "EXIF_EXTRACTOR", .collector = "osint", .name = "EXIF Extractor", .run = exif_run,
  .category = "investigation", .type = "api", .url = "internal://exif",
  .description = "Image URL → EXIF camera make/model/timestamp + GPS geolocation (free).",
  .free_tier = 1,
};
REGISTER_SOURCE(exif_def)
