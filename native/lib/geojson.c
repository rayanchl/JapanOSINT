#include "geojson.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* collectorMirror.NATIVE_ID_KEYS, exact order. */
static const char *NATIVE_ID_KEYS[] = {
  "camera_uid","station_uid","line_uid","cluster_uid","footprint_id",
  "post_uid","event_id","earthquake_id","buoy_id","station_id",
  "spring_id","stop_id","uid","id","uuid", NULL };

static const char *prop_str(cJSON *props, const char *k) {
  cJSON *v = cJSON_GetObjectItem(props, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
  return NULL;
}

/* pickText(props, keys...) — first non-empty trimmed STRING. */
static const char *pick_text(cJSON *props, const char *const *keys) {
  for (int i = 0; keys[i]; i++) {
    cJSON *v = cJSON_GetObjectItem(props, keys[i]);
    if (v && cJSON_IsString(v)) {
      const char *s = v->valuestring;
      while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
      if (*s) return v->valuestring; /* Node returns trimmed; trim on use */
    }
  }
  return NULL;
}

/* JS String(v): numbers without quotes, strings raw. For uid we only hit
 * NATIVE keys/id which are strings or numbers. */
static void id_to_str(cJSON *v, char *out, size_t n) {
  if (cJSON_IsString(v)) snprintf(out, n, "%s", v->valuestring);
  else if (cJSON_IsNumber(v)) {
    if (v->valuedouble == (double)(long long)v->valuedouble)
      snprintf(out, n, "%lld", (long long)v->valuedouble);
    else snprintf(out, n, "%g", v->valuedouble);
  } else out[0] = 0;
}

static void sha1_hex16(const char *s, char *out17) {
  unsigned char d[20]; SHA1((const unsigned char *)s, strlen(s), d);
  for (int i = 0; i < 8; i++) sprintf(out17 + i*2, "%02x", d[i]);
  out17[16] = 0;
}

/* featureUid: NATIVE_ID_KEYS → feature.id → sourceId|h:sha1(JSON.stringify
 * {g:geometry,p:props})[:16]. cJSON_PrintUnformatted preserves source key
 * order (like JS), so the fingerprint matches for the hash-fallback minority
 * (most features carry a natural id; per-source parity tests flag any drift). */
static void feature_uid(cJSON *feat, const char *sid, char *out, size_t n) {
  cJSON *props = cJSON_GetObjectItem(feat, "properties");
  if (props) {
    for (int i = 0; NATIVE_ID_KEYS[i]; i++) {
      cJSON *v = cJSON_GetObjectItem(props, NATIVE_ID_KEYS[i]);
      if (v && !cJSON_IsNull(v)) {
        char idv[256]; id_to_str(v, idv, sizeof idv);
        if (idv[0]) { snprintf(out, n, "%s|%s", sid, idv); return; }
      }
    }
  }
  cJSON *fid = cJSON_GetObjectItem(feat, "id");
  if (fid && !cJSON_IsNull(fid)) {
    char idv[256]; id_to_str(fid, idv, sizeof idv);
    if (idv[0]) { snprintf(out, n, "%s|%s", sid, idv); return; }
  }
  cJSON *g = cJSON_GetObjectItem(feat, "geometry");
  cJSON *fp = cJSON_CreateObject();
  cJSON_AddItemToObject(fp, "g", g ? cJSON_Duplicate(g, 1) : cJSON_CreateNull());
  cJSON_AddItemToObject(fp, "p", props ? cJSON_Duplicate(props, 1) : cJSON_CreateObject());
  char *s = cJSON_PrintUnformatted(fp);
  char h[17]; sha1_hex16(s ? s : "", h);
  free(s); cJSON_Delete(fp);
  snprintf(out, n, "%s|h:%s", sid, h);
}

/* geometryCentroid → sets *lat,*lon; returns 1 if usable. */
static void visit(cJSON *node, double *mnx, double *mxx, double *mny,
                   double *mxy, int *cnt) {
  if (!cJSON_IsArray(node)) return;
  cJSON *a = cJSON_GetArrayItem(node, 0);  /* exhaustive-ok: [x,y] tuple */
  cJSON *b = cJSON_GetArrayItem(node, 1);
  if (a && b && cJSON_IsNumber(a) && cJSON_IsNumber(b) &&
      isfinite(a->valuedouble) && isfinite(b->valuedouble)) {
    if (a->valuedouble < *mnx) *mnx = a->valuedouble;
    if (a->valuedouble > *mxx) *mxx = a->valuedouble;
    if (b->valuedouble < *mny) *mny = b->valuedouble;
    if (b->valuedouble > *mxy) *mxy = b->valuedouble;
    (*cnt)++; return;
  }
  cJSON *ch; cJSON_ArrayForEach(ch, node) visit(ch, mnx, mxx, mny, mxy, cnt);
}
static int centroid(cJSON *geom, double *lat, double *lon) {
  if (!geom) return 0;
  cJSON *t = cJSON_GetObjectItem(geom, "type");
  cJSON *c = cJSON_GetObjectItem(geom, "coordinates");
  if (!t || !cJSON_IsString(t) || !c) return 0;
  if (strcmp(t->valuestring, "Point") == 0) {
    cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
    if (x && y && cJSON_IsNumber(x) && cJSON_IsNumber(y) &&
        isfinite(x->valuedouble) && isfinite(y->valuedouble)) {
      *lon = x->valuedouble; *lat = y->valuedouble; return 1;
    }
    return 0;
  }
  double mnx=1e308,mxx=-1e308,mny=1e308,mxy=-1e308; int cnt=0;
  visit(c, &mnx,&mxx,&mny,&mxy,&cnt);
  if (cnt == 0) return 0;
  *lon = (mnx+mxx)/2; *lat = (mny+mxy)/2; return 1;
}

static const char *T_TITLE[]  = {"title","name","name_ja","label",NULL};
static const char *T_SUMM[]   = {"summary","description","desc",NULL};
static const char *T_LINK[]   = {"link","url","href",NULL};
static const char *T_AUTH[]   = {"author","operator",NULL};
static const char *T_LANG[]   = {"language","lang",NULL};
static const char *T_PUB[]    = {"published_at","observed_at","time","timestamp",NULL};

int geojson_emit_features(intel_sink *sink, const char *sid, cJSON *features) {
  if (!cJSON_IsArray(features)) return 0;
  int n = 0; cJSON *feat;
  cJSON_ArrayForEach(feat, features) {
    if (!cJSON_IsObject(feat)) continue;
    cJSON *props = cJSON_GetObjectItem(feat, "properties");
    cJSON *geom  = cJSON_GetObjectItem(feat, "geometry");
    char uid[600]; feature_uid(feat, sid, uid, sizeof uid);
    double lat=0, lon=0; int geo = centroid(geom, &lat, &lon);

    const char *rt = props ? prop_str(props, "record_type") : NULL;
    if (!rt && props) rt = prop_str(props, "kind");
    if (!rt) rt = sid;
    const char *sub = props ? prop_str(props, "sub_source_id") : NULL;
    if (!sub && props) sub = prop_str(props, "channel");

    char *gj = geom ? cJSON_PrintUnformatted(geom) : NULL;
    char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
    cJSON *tagsv = props ? cJSON_GetObjectItem(props, "tags") : NULL;
    char *tj = (tagsv && cJSON_IsArray(tagsv)) ? cJSON_PrintUnformatted(tagsv) : NULL;

    intel_item it = {0};
    it.uid = uid;
    it.title       = props ? pick_text(props, T_TITLE) : NULL;
    it.summary     = props ? pick_text(props, T_SUMM)  : NULL;
    it.link        = props ? pick_text(props, T_LINK)  : NULL;
    it.author      = props ? pick_text(props, T_AUTH)  : NULL;
    it.lang        = props ? pick_text(props, T_LANG)  : NULL;
    it.published_at= props ? pick_text(props, T_PUB)   : NULL;
    it.record_type = rt;
    it.sub_source_id = sub;
    it.has_geo = geo; it.lat = lat; it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = tj ? tj : "[]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(gj); free(pj); free(tj);
  }
  return n;
}

int geojson_emit_doc(intel_sink *sink, const char *sid, cJSON *doc) {
  if (!doc) return 0;
  if (cJSON_IsArray(doc)) return geojson_emit_features(sink, sid, doc);
  cJSON *f = cJSON_GetObjectItem(doc, "features");
  return f ? geojson_emit_features(sink, sid, f) : 0;
}
