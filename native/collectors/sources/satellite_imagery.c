/* collectors/satellite/sources/satellite_imagery.c
 * Port of server/src/collectors/satelliteImagery.js — multi-provider
 * aggregate (each provider appends; not first-wins overall). Ported in JS
 * provider order:
 *   1 nict_himawari        GET himawari.asia latest.json → 1 feature
 *   2 nasa_gibs_modis      deterministic (Terra+Aqua), 2 features
 *   3 nasa_gibs_viirs      deterministic, 1 feature
 *   4 planetary_computer_landsat  POST STAC search
 *   5 rammb_slider_goes18  deterministic, 1 feature
 *   6 jaxa_alos2_seed      DROPPED — curated seed-only (rule 7)
 *   7 usgs_m2m_historical  token-gated; no USGS_M2M_TOKEN → null (0 rows)
 *   8 sentinel2_multi      first-wins: SentinelHub(creds; skipped w/o creds)
 *                          → Earth Search → Planetary Computer → CDSE OData
 *   9 sentinel1_multi      first-wins: CDSE OData → Planetary Computer
 *                          → Earth Search
 * generateSeed() fallback dropped (rule 7). _meta dropped. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* JAPAN_BBOX = [122,24,154,46] (W,S,E,N) */
#define BB_W 122
#define BB_S 24
#define BB_E 154
#define BB_N 46
#define BBOX_STR "122,24,154,46"

static const char *JSON_HDRS[] = { "Content-Type: application/json", NULL };

/* now ISO + today YYYY-MM-DD (UTC) */
static void now_iso(char *o, size_t n) {
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(o, n, "%Y-%m-%dT%H:%M:%S.000Z", &g);
}
static void today_ymd(char *o, size_t n) {
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(o, n, "%Y-%m-%d", &g);
}
/* ISO window: from = now-Ndays, to = now */
static void iso_window(int days, char *from, size_t fn, char *to, size_t tn) {
  time_t t = time(NULL); struct tm g;
  gmtime_r(&t, &g); strftime(to, tn, "%Y-%m-%dT%H:%M:%S.000Z", &g);
  time_t f = t - (time_t)days * 86400; gmtime_r(&f, &g);
  strftime(from, fn, "%Y-%m-%dT%H:%M:%S.000Z", &g);
}

static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static cJSON *mk_feat(double lon, double lat) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);
  return f;
}

/* s2CentroidFromGeom: Polygon→coords[0], MultiPolygon→coords[0][0], else
 * [139,35]; average the ring. */
static void centroid_of(const cJSON *geom, double *cx, double *cy) {
  *cx = 139; *cy = 35;
  if (!geom) return;
  const char *gt = sv(geom, "type");
  const cJSON *coords = cJSON_GetObjectItem(geom, "coordinates");
  const cJSON *ring = NULL;
  if (gt && !strcmp(gt, "Polygon")) ring = cJSON_GetArrayItem(coords, 0);
  else if (gt && !strcmp(gt, "MultiPolygon"))
    ring = cJSON_GetArrayItem(cJSON_GetArrayItem(coords, 0), 0);
  if (!cJSON_IsArray(ring) || cJSON_GetArraySize(ring) == 0) return;
  double sx = 0, sy = 0; int n = 0;
  const cJSON *pt;
  cJSON_ArrayForEach(pt, ring) {
    const cJSON *x = cJSON_GetArrayItem(pt, 0);
    const cJSON *y = cJSON_GetArrayItem(pt, 1);
    if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
      sx += cJSON_GetNumberValue(x); sy += cJSON_GetNumberValue(y); n++;
    }
  }
  if (n) { *cx = sx / n; *cy = sy / n; }
}

/* eo:cloud_cover ?? null */
static cJSON *cloud_cover(const cJSON *props) {
  const cJSON *v = props ? cJSON_GetObjectItem(props, "eo:cloud_cover") : NULL;
  if (v && cJSON_IsNumber(v)) return cJSON_CreateNumber(cJSON_GetNumberValue(v));
  return cJSON_CreateNull();
}
static cJSON *dup_or_null(const cJSON *o, const char *k) {
  const cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  if (!v || cJSON_IsNull(v)) return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}

/* ── 1. Himawari-9 ── */
static void prov_himawari(http_client *http, cJSON *out) {
  cJSON *latest = feed_get_json(http,
    "https://himawari.asia/img/D531106/latest.json", 8000);
  const char *date = latest ? sv(latest, "date") : NULL;
  /* No real fetched timestamp → no real scene; emit nothing (no fabrication). */
  if (!date || strlen(date) < 16) { if (latest) cJSON_Delete(latest); return; }
  char iso[40];
  if (date && strlen(date) >= 16) {
    /* new Date(date.replace(' ','T')+'Z').toISOString() */
    char tmp[40]; snprintf(tmp, sizeof tmp, "%s", date);
    char y[5],mo[3],d[3],h[3],mi[3],se[3];
    snprintf(y,5,"%.4s",date); snprintf(mo,3,"%.2s",date+5);
    snprintf(d,3,"%.2s",date+8); snprintf(h,3,"%.2s",date+11);
    snprintf(mi,3,"%.2s",date+14);
    snprintf(se,3,"%.2s",strlen(date)>=19?date+17:"00");
    snprintf(iso,sizeof iso,"%s-%s-%sT%s:%s:%s.000Z",y,mo,d,h,mi,se);
    (void)tmp;
  } else {
    now_iso(iso, sizeof iso);
  }
  /* scene_id = date || iso ; id digits of (date||iso) */
  char scenebuf[40];
  const char *scene = date ? date : iso;
  snprintf(scenebuf, sizeof scenebuf, "%s", scene);
  char idnum[64]; int io = 0;
  for (const char *p = scene; *p && io < 63; p++)
    if (*p >= '0' && *p <= '9') idnum[io++] = *p;
  idnum[io] = 0;
  char idbuf[96]; snprintf(idbuf, sizeof idbuf, "IMG_HIMAWARI_%s", idnum);

  char prev[256] = "", tile[256] = "";
  int hasDate = date && strlen(date) >= 16;
  if (hasDate) {
    snprintf(prev, sizeof prev,
      "https://himawari.asia/img/D531106/8d/%.4s/%.2s/%.2s/%.2s%.2s00_3_3.png",
      date, date+5, date+8, date+11, date+14);
    snprintf(tile, sizeof tile,
      "https://himawari.asia/img/D531106/8d/%.4s/%.2s/%.2s/%.2s%.2s00_{x}_{y}.png",
      date, date+5, date+8, date+11, date+14);
  }

  cJSON *f = mk_feat(138.0, 36.0);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "id", idbuf);
  cJSON_AddStringToObject(p, "platform", "Himawari-9");
  cJSON_AddStringToObject(p, "sensor", "AHI");
  cJSON_AddStringToObject(p, "scene_id", scenebuf);
  cJSON_AddStringToObject(p, "datetime", iso);
  cJSON_AddItemToObject(p, "cloud_cover", cJSON_CreateNull());
  cJSON_AddItemToObject(p, "preview_url",
    hasDate ? cJSON_CreateString(prev) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "tile_url",
    hasDate ? cJSON_CreateString(tile) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "archive_era", "real-time");
  cJSON_AddStringToObject(p, "source", "nict_himawari");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(out, f);
  if (latest) cJSON_Delete(latest);
}

/* ── 4. Landsat via Planetary Computer STAC ── */
static void prov_landsat(http_client *http, cJSON *out) {
  char from[40], to[40];
  iso_window(14, from, sizeof from, to, sizeof to);
  char body[512];
  snprintf(body, sizeof body,
    "{\"bbox\":[%d,%d,%d,%d],\"datetime\":\"%s/%s\","
    "\"collections\":[\"landsat-c2-l2\"],\"limit\":20,"
    "\"query\":{\"eo:cloud_cover\":{\"lt\":50}}}",
    BB_W, BB_S, BB_E, BB_N, from, to);
  cJSON *data = feed_post_json(http,
    "https://planetarycomputer.microsoft.com/api/stac/v1/search",
    body, JSON_HDRS, 10000);
  cJSON *feats = data ? cJSON_GetObjectItem(data, "features") : NULL;
  if (cJSON_IsArray(feats)) {
    int i = 0;
    cJSON *fe;
    cJSON_ArrayForEach(fe, feats) {
      cJSON *geom = cJSON_GetObjectItem(fe, "geometry");
      /* JS landsat centroid: ring = geom.coordinates[0]; avg, else [138,36] */
      double cx = 138, cy = 36;
      const cJSON *ring = geom ? cJSON_GetArrayItem(
        cJSON_GetObjectItem(geom, "coordinates"), 0) : NULL;
      if (cJSON_IsArray(ring) && cJSON_GetArraySize(ring) > 0) {
        double sx = 0, sy = 0; int n = 0; const cJSON *pt;
        cJSON_ArrayForEach(pt, ring) {
          const cJSON *x = cJSON_GetArrayItem(pt, 0);
          const cJSON *y = cJSON_GetArrayItem(pt, 1);
          if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
            sx += cJSON_GetNumberValue(x); sy += cJSON_GetNumberValue(y); n++;
          }
        }
        if (n) { cx = sx / n; cy = sy / n; }
      }
      const cJSON *props = cJSON_GetObjectItem(fe, "properties");
      const cJSON *assets = cJSON_GetObjectItem(fe, "assets");
      const char *thumb = NULL;
      if (assets) {
        const cJSON *rp = cJSON_GetObjectItem(assets, "rendered_preview");
        thumb = sv(rp, "href");
        if (!thumb) { const cJSON *tn = cJSON_GetObjectItem(assets, "thumbnail");
          thumb = sv(tn, "href"); }
      }
      const char *fid = sv(fe, "id");
      char idbuf[96];
      if (fid) snprintf(idbuf, sizeof idbuf, "IMG_LANDSAT_%s", fid);
      else snprintf(idbuf, sizeof idbuf, "IMG_LANDSAT_%d", i);

      cJSON *f = mk_feat(cx, cy);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "id", idbuf);
      const char *plat = sv(props, "platform");
      cJSON_AddStringToObject(p, "platform", plat ? plat : "Landsat-9");
      cJSON_AddStringToObject(p, "sensor", "OLI");
      cJSON_AddItemToObject(p, "scene_id",
        fid ? cJSON_CreateString(fid) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "datetime", dup_or_null(props, "datetime"));
      cJSON_AddItemToObject(p, "cloud_cover", cloud_cover(props));
      cJSON_AddItemToObject(p, "preview_url",
        thumb ? cJSON_CreateString(thumb) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "tile_url", cJSON_CreateNull());
      cJSON_AddItemToObject(p, "bbox_geom",
        geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "archive_era", "real-time");
      cJSON_AddStringToObject(p, "source", "planetary_computer");
      cJSON_AddStringToObject(p, "country", "JP");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(out, f);
      i++;
    }
  }
  if (data) cJSON_Delete(data);
}

/* s2MapStacFeature → push into `dst` (a temp array). */
static void s2_map_stac(cJSON *dst, const cJSON *fe, int i, const char *tag) {
  const cJSON *geom = cJSON_GetObjectItem(fe, "geometry");
  double cx, cy; centroid_of(geom, &cx, &cy);
  const cJSON *assets = cJSON_GetObjectItem(fe, "assets");
  const char *prev = NULL;
  if (assets) {
    const char *k[] = { "thumbnail","preview","visual","rendered_preview" };
    for (int z = 0; z < 4 && !prev; z++)
      prev = sv(cJSON_GetObjectItem(assets, k[z]), "href");
  }
  const cJSON *props = cJSON_GetObjectItem(fe, "properties");
  const char *fid = sv(fe, "id");
  char idbuf[96];
  if (fid) snprintf(idbuf, sizeof idbuf, "IMG_S2_%s", fid);
  else snprintf(idbuf, sizeof idbuf, "IMG_S2_%d", i);
  cJSON *f = mk_feat(cx, cy);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "id", idbuf);
  const char *plat = sv(props, "platform");
  cJSON_AddStringToObject(p, "platform", plat ? plat : "Sentinel-2");
  cJSON_AddStringToObject(p, "sensor", "MSI");
  cJSON_AddItemToObject(p, "scene_id",
    fid ? cJSON_CreateString(fid) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "datetime", dup_or_null(props, "datetime"));
  cJSON_AddItemToObject(p, "cloud_cover", cloud_cover(props));
  cJSON_AddItemToObject(p, "preview_url",
    prev ? cJSON_CreateString(prev) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "tile_url", cJSON_CreateNull());
  cJSON_AddItemToObject(p, "bbox_geom",
    geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "archive_era", "real-time");
  cJSON_AddStringToObject(p, "source", tag);
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(dst, f);
}

/* STAC POST search returning features[] → s2_map_stac each. Returns count. */
static int s2_stac_search(http_client *http, const char *url,
                          const char *body, const char *tag, cJSON *dst) {
  cJSON *data = feed_post_json(http, url, body, JSON_HDRS, 10000);
  cJSON *feats = data ? cJSON_GetObjectItem(data, "features") : NULL;
  int c = 0;
  if (cJSON_IsArray(feats) && cJSON_GetArraySize(feats) > 0) {
    int i = 0; cJSON *fe;
    cJSON_ArrayForEach(fe, feats) { s2_map_stac(dst, fe, i++, tag); c++; }
  }
  if (data) cJSON_Delete(data);
  return c;
}

/* ── 8. Sentinel-2 first-wins (SentinelHub creds path skipped: no creds) ── */
static void prov_s2(http_client *http, cJSON *out) {
  char from[40], to[40];
  iso_window(10, from, sizeof from, to, sizeof to);
  char body[512];
  cJSON *tmp = cJSON_CreateArray();

  /* Earth Search (AWS) */
  snprintf(body, sizeof body,
    "{\"bbox\":[%d,%d,%d,%d],\"datetime\":\"%s/%s\","
    "\"collections\":[\"sentinel-2-l2a\"],\"limit\":60,"
    "\"query\":{\"eo:cloud_cover\":{\"lt\":40}}}",
    BB_W, BB_S, BB_E, BB_N, from, to);
  if (s2_stac_search(http, "https://earth-search.aws.element84.com/v1/search",
        body, "earth_search_aws", tmp) > 0) goto done;

  /* Planetary Computer */
  if (s2_stac_search(http,
        "https://planetarycomputer.microsoft.com/api/stac/v1/search",
        body, "planetary_computer_s2", tmp) > 0) goto done;

  /* CDSE OData */
  {
    char filt[1024], url[1400];
    snprintf(filt, sizeof filt,
      "Collection/Name eq 'SENTINEL-2' and "
      "OData.CSC.Intersects(area=geography'SRID=4326;"
      "POLYGON((%d %d,%d %d,%d %d,%d %d,%d %d))') and "
      "ContentDate/Start ge %s and ContentDate/Start le %s and "
      "Attributes/OData.CSC.DoubleAttribute/any(att:att/Name eq 'cloudCover' "
      "and att/OData.CSC.DoubleAttribute/Value lt 40.0) and "
      "contains(Name,'MSIL2A')",
      BB_W,BB_S, BB_E,BB_S, BB_E,BB_N, BB_W,BB_N, BB_W,BB_S, from, to);
    /* URL-encode the filter (minimal: spaces→%20, '→%27, ()→%28%29, ,→%2C) */
    char enc[2048]; int eo = 0;
    for (const char *q = filt; *q && eo < 2000; q++) {
      unsigned char c = (unsigned char)*q;
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c=='-'||c=='_'||c=='.'||c=='~')
        enc[eo++] = (char)c;
      else { eo += snprintf(enc+eo, sizeof enc-eo, "%%%02X", c); }
    }
    enc[eo] = 0;
    snprintf(url, sizeof url,
      "https://catalogue.dataspace.copernicus.eu/odata/v1/Products?"
      "$filter=%s&$top=60&$orderby=ContentDate/Start desc", enc);
    const char *acc[] = { "Accept: application/json", NULL };
    cJSON *data = feed_get_json_h(http, url, acc, 10000);
    cJSON *items = data ? cJSON_GetObjectItem(data, "value") : NULL;
    if (cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0) {
      int i = 0; cJSON *it;
      cJSON_ArrayForEach(it, items) {
        const cJSON *geom = cJSON_GetObjectItem(it, "GeoFootprint");
        double cx, cy; centroid_of(geom, &cx, &cy);
        /* cc = Attributes.find(Name=='cloudCover').Value ?? null */
        cJSON *cc = cJSON_CreateNull();
        const cJSON *attrs = cJSON_GetObjectItem(it, "Attributes");
        if (cJSON_IsArray(attrs)) {
          const cJSON *a;
          cJSON_ArrayForEach(a, attrs) {
            const char *an = sv(a, "Name");
            if (an && !strcmp(an, "cloudCover")) {
              const cJSON *vv = cJSON_GetObjectItem(a, "Value");
              if (vv && cJSON_IsNumber(vv)) {
                cJSON_Delete(cc);
                cc = cJSON_CreateNumber(cJSON_GetNumberValue(vv));
              }
              break;
            }
          }
        }
        const char *iid = sv(it, "Id");
        const char *nm = sv(it, "Name");
        char idbuf[96];
        if (iid) snprintf(idbuf, sizeof idbuf, "IMG_S2_%s", iid);
        else snprintf(idbuf, sizeof idbuf, "IMG_S2_%d", i);
        char prev[256] = "";
        if (iid) snprintf(prev, sizeof prev,
          "https://catalogue.dataspace.copernicus.eu/odata/v1/Products(%s)/$value",
          iid);
        const cJSON *cd = cJSON_GetObjectItem(it, "ContentDate");
        cJSON *f = mk_feat(cx, cy);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "id", idbuf);
        cJSON_AddStringToObject(p, "platform", "Sentinel-2");
        cJSON_AddStringToObject(p, "sensor", "MSI");
        cJSON_AddItemToObject(p, "scene_id",
          nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "datetime", dup_or_null(cd, "Start"));
        cJSON_AddItemToObject(p, "cloud_cover", cc);
        cJSON_AddItemToObject(p, "preview_url",
          iid ? cJSON_CreateString(prev) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "tile_url", cJSON_CreateNull());
        cJSON_AddItemToObject(p, "bbox_geom",
          geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "archive_era", "real-time");
        cJSON_AddStringToObject(p, "source", "cdse_odata");
        cJSON_AddStringToObject(p, "country", "JP");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(tmp, f);
        i++;
      }
    }
    if (data) cJSON_Delete(data);
  }

done:;
  cJSON *fe;
  cJSON_ArrayForEach(fe, tmp)
    cJSON_AddItemToArray(out, cJSON_Duplicate(fe, 1));
  cJSON_Delete(tmp);
}

/* s1PlatformFromName */
static const char *s1_platform(const char *name) {
  if (!name) return "sentinel-1";
  char up[64]; int i = 0;
  for (; name[i] && i < 63; i++)
    up[i] = (name[i] >= 'a' && name[i] <= 'z') ? name[i]-32 : name[i];
  up[i] = 0;
  if (!strncmp(up,"S1A",3) || strstr(up,"SENTINEL-1A")) return "sentinel-1a";
  if (!strncmp(up,"S1B",3) || strstr(up,"SENTINEL-1B")) return "sentinel-1b";
  if (!strncmp(up,"S1C",3) || strstr(up,"SENTINEL-1C")) return "sentinel-1c";
  return "sentinel-1";
}

/* ── 9. Sentinel-1 first-wins: CDSE OData → Planetary Computer → Earth Search */
static void prov_s1(http_client *http, cJSON *out) {
  char from[40], to[40];
  iso_window(14, from, sizeof from, to, sizeof to);
  cJSON *tmp = cJSON_CreateArray();

  /* CDSE OData */
  {
    char filt[1024], url[1400];
    snprintf(filt, sizeof filt,
      "Collection/Name eq 'SENTINEL-1' and "
      "OData.CSC.Intersects(area=geography'SRID=4326;"
      "POLYGON((%d %d,%d %d,%d %d,%d %d,%d %d))') and "
      "ContentDate/Start ge %s and ContentDate/Start le %s and "
      "contains(Name,'GRD')",
      BB_W,BB_S, BB_E,BB_S, BB_E,BB_N, BB_W,BB_N, BB_W,BB_S, from, to);
    char enc[2048]; int eo = 0;
    for (const char *q = filt; *q && eo < 2000; q++) {
      unsigned char c = (unsigned char)*q;
      if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
          c=='-'||c=='_'||c=='.'||c=='~') enc[eo++] = (char)c;
      else eo += snprintf(enc+eo, sizeof enc-eo, "%%%02X", c);
    }
    enc[eo] = 0;
    snprintf(url, sizeof url,
      "https://catalogue.dataspace.copernicus.eu/odata/v1/Products?"
      "$filter=%s&$top=40&$orderby=ContentDate/Start desc", enc);
    const char *acc[] = { "Accept: application/json", NULL };
    cJSON *data = feed_get_json_h(http, url, acc, 10000);
    cJSON *items = data ? cJSON_GetObjectItem(data, "value") : NULL;
    if (cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0) {
      int i = 0; cJSON *it;
      cJSON_ArrayForEach(it, items) {
        const cJSON *geom = cJSON_GetObjectItem(it, "GeoFootprint");
        double cx, cy; centroid_of(geom, &cx, &cy);
        const char *iid = sv(it, "Id");
        const char *nm = sv(it, "Name");
        char idbuf[96], prev[256] = "";
        if (iid) { snprintf(idbuf,sizeof idbuf,"IMG_S1_%s",iid);
          snprintf(prev,sizeof prev,
            "https://catalogue.dataspace.copernicus.eu/odata/v1/Products(%s)/$value",iid);
        } else snprintf(idbuf,sizeof idbuf,"IMG_S1_%d",i);
        const cJSON *cd = cJSON_GetObjectItem(it, "ContentDate");
        cJSON *f = mk_feat(cx, cy);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "id", idbuf);
        cJSON_AddStringToObject(p, "platform", s1_platform(nm));
        cJSON_AddStringToObject(p, "sensor", "c-sar");
        cJSON_AddStringToObject(p, "product_type", "GRD");
        cJSON_AddStringToObject(p, "polarization", "VV");
        cJSON_AddItemToObject(p, "scene_id",
          nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "datetime", dup_or_null(cd, "Start"));
        cJSON_AddItemToObject(p, "preview_url",
          iid ? cJSON_CreateString(prev) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "tile_url", cJSON_CreateNull());
        cJSON_AddItemToObject(p, "bbox_geom",
          geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "archive_era", "real-time");
        cJSON_AddStringToObject(p, "source", "cdse_odata");
        cJSON_AddStringToObject(p, "country", "JP");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(tmp, f);
        i++;
      }
    }
    if (data) cJSON_Delete(data);
    if (cJSON_GetArraySize(tmp) > 0) goto done;
  }

  /* Planetary Computer */
  {
    char body[256];
    snprintf(body, sizeof body,
      "{\"bbox\":[%d,%d,%d,%d],\"datetime\":\"%s/%s\","
      "\"collections\":[\"sentinel-1-grd\"],\"limit\":40}",
      BB_W, BB_S, BB_E, BB_N, from, to);
    cJSON *data = feed_post_json(http,
      "https://planetarycomputer.microsoft.com/api/stac/v1/search",
      body, JSON_HDRS, 10000);
    cJSON *feats = data ? cJSON_GetObjectItem(data, "features") : NULL;
    if (cJSON_IsArray(feats) && cJSON_GetArraySize(feats) > 0) {
      int i = 0; cJSON *fe;
      cJSON_ArrayForEach(fe, feats) {
        const cJSON *geom = cJSON_GetObjectItem(fe, "geometry");
        double cx, cy; centroid_of(geom, &cx, &cy);
        const cJSON *props = cJSON_GetObjectItem(fe, "properties");
        const char *fid = sv(fe, "id");
        const char *plat = sv(props, "platform");
        char idbuf[96], tile[512] = "";
        if (fid) snprintf(idbuf,sizeof idbuf,"IMG_S1_%s",fid);
        else snprintf(idbuf,sizeof idbuf,"IMG_S1_%d",i);
        if (fid) {
          /* encodeURIComponent(f.id) — fid is alnum-ish; minimal encode */
          char fe_enc[256]; int z = 0;
          for (const char *q = fid; *q && z < 250; q++) {
            unsigned char c = (unsigned char)*q;
            if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
                c=='-'||c=='_'||c=='.'||c=='~'||c=='!'||c=='*'||
                c=='\''||c=='('||c==')') fe_enc[z++] = (char)c;
            else z += snprintf(fe_enc+z, sizeof fe_enc-z, "%%%02X", c);
          }
          fe_enc[z] = 0;
          snprintf(tile, sizeof tile,
            "https://planetarycomputer.microsoft.com/api/data/v1/item/tiles/"
            "WebMercatorQuad/{z}/{x}/{y}?collection=sentinel-1-grd&item=%s"
            "&assets=vv&rescale=-30,0", fe_enc);
        }
        const cJSON *assets = cJSON_GetObjectItem(fe, "assets");
        const char *prev = NULL;
        if (assets) {
          prev = sv(cJSON_GetObjectItem(assets, "thumbnail"), "href");
          if (!prev) prev = sv(cJSON_GetObjectItem(assets,
            "rendered_preview"), "href");
        }
        cJSON *f = mk_feat(cx, cy);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "id", idbuf);
        cJSON_AddStringToObject(p, "platform",
          s1_platform(plat ? plat : fid));
        cJSON_AddStringToObject(p, "sensor", "c-sar");
        cJSON_AddStringToObject(p, "product_type", "GRD");
        cJSON_AddStringToObject(p, "polarization", "VV");
        cJSON_AddItemToObject(p, "scene_id",
          fid ? cJSON_CreateString(fid) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "datetime", dup_or_null(props, "datetime"));
        cJSON_AddItemToObject(p, "preview_url",
          prev ? cJSON_CreateString(prev) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "tile_url",
          fid ? cJSON_CreateString(tile) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "bbox_geom",
          geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "archive_era", "real-time");
        cJSON_AddStringToObject(p, "source", "planetary_computer_s1");
        cJSON_AddStringToObject(p, "country", "JP");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(tmp, f);
        i++;
      }
    }
    if (data) cJSON_Delete(data);
    if (cJSON_GetArraySize(tmp) > 0) goto done;
  }

  /* Earth Search */
  {
    char body[256];
    snprintf(body, sizeof body,
      "{\"bbox\":[%d,%d,%d,%d],\"datetime\":\"%s/%s\","
      "\"collections\":[\"sentinel-1-grd\"],\"limit\":40}",
      BB_W, BB_S, BB_E, BB_N, from, to);
    cJSON *data = feed_post_json(http,
      "https://earth-search.aws.element84.com/v1/search",
      body, JSON_HDRS, 10000);
    cJSON *feats = data ? cJSON_GetObjectItem(data, "features") : NULL;
    if (cJSON_IsArray(feats) && cJSON_GetArraySize(feats) > 0) {
      int i = 0; cJSON *fe;
      cJSON_ArrayForEach(fe, feats) {
        const cJSON *geom = cJSON_GetObjectItem(fe, "geometry");
        double cx, cy; centroid_of(geom, &cx, &cy);
        const cJSON *props = cJSON_GetObjectItem(fe, "properties");
        const char *fid = sv(fe, "id");
        const char *plat = sv(props, "platform");
        const cJSON *assets = cJSON_GetObjectItem(fe, "assets");
        const char *prev = sv(cJSON_GetObjectItem(assets, "thumbnail"),
                              "href");
        const char *tile = sv(cJSON_GetObjectItem(assets, "vv"), "href");
        char idbuf[96];
        if (fid) snprintf(idbuf,sizeof idbuf,"IMG_S1_%s",fid);
        else snprintf(idbuf,sizeof idbuf,"IMG_S1_%d",i);
        cJSON *f = mk_feat(cx, cy);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "id", idbuf);
        cJSON_AddStringToObject(p, "platform",
          s1_platform(plat ? plat : fid));
        cJSON_AddStringToObject(p, "sensor", "c-sar");
        cJSON_AddStringToObject(p, "product_type", "GRD");
        cJSON_AddStringToObject(p, "polarization", "VV");
        cJSON_AddItemToObject(p, "scene_id",
          fid ? cJSON_CreateString(fid) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "datetime", dup_or_null(props, "datetime"));
        cJSON_AddItemToObject(p, "preview_url",
          prev ? cJSON_CreateString(prev) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "tile_url",
          tile ? cJSON_CreateString(tile) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "bbox_geom",
          geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "archive_era", "real-time");
        cJSON_AddStringToObject(p, "source", "earth_search_s1");
        cJSON_AddStringToObject(p, "country", "JP");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(tmp, f);
        i++;
      }
    }
    if (data) cJSON_Delete(data);
  }

done:;
  cJSON *fe;
  cJSON_ArrayForEach(fe, tmp)
    cJSON_AddItemToArray(out, cJSON_Duplicate(fe, 1));
  cJSON_Delete(tmp);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  char day[16]; today_ymd(day, sizeof day);

  prov_himawari(ctx->http, features);                              /* 1 */
  /* 2 nasa_gibs_modis / 3 nasa_gibs_viirs / 5 rammb_slider_goes18:
   *   DROPPED — these "deterministic" providers fabricated a scene record
   *   (hardcoded centroid + date-templated tile/preview URL) WITHOUT ever
   *   fetching a catalog entry. No real scene metadata = no honest record
   *   (no-fabrication rule). Only genuinely-fetched providers remain. */
  prov_landsat(ctx->http, features);                               /* 4 */
  /* 6 jaxa_alos2_seed: curated seed-only → dropped (rule 7)        */
  /* 7 usgs_m2m_historical: token-gated; no token → 0 rows         */
  prov_s2(ctx->http, features);                                    /* 8 */
  prov_s1(ctx->http, features);                                    /* 9 */
  (void)day;

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[satellite-imagery] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def satellite_imagery_def = {
  .id = "satellite-imagery", .collector = "satellite",
  .name = "Satellite Imagery (Multi-source)", .name_ja = "衛星画像 (複数ソース)",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(satellite_imagery_def)
