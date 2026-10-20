/* core/camera_feed.c — camera record properties → capture targets. The
 * contract and the reasons are in camera_feed.h; this file is the mechanics.
 *
 * THE ONE IDEA. Every string in the record is visited once as a LEAF with its
 * full dotted path ("views.3.image_url", "attributes.ImageURL"). Two shapes
 * reach this file and the leaf walk makes them one:
 *
 *   nested JSON   {"views":[{"image_url":…}]}          → views.0.image_url
 *   hp-flattened  {"views.0.image_url":…}              → views.0.image_url
 *
 * because a member name is split on its own dots exactly like a nesting level.
 * The GROUP of a leaf is its path up to and including the LAST all-digit
 * segment ("views.3"), or "" when it is not inside an array. A group is one
 * candidate target; within it the best-ranked key wins.
 */
#include "camera_feed.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define CF_MAX_LEAVES 4096          /* a real record carries a few dozen      */
#define CF_MAX_DEPTH  12

/* ── ranking ─────────────────────────────────────────────────────────────── *
 * Folded names (lowercase, `_` `-` `@` removed). ORDER IS THE RANK. Tiers:
 * still images, then streams, then thumbnails, then pages-that-might-be-images
 * with `url` dead last — the order camera_stills.c always used for its six
 * keys, extended with the spellings found by grepping the camera collectors
 * and the hp camera tables (hp3b29a_webcams.c, hp3b30_jpcam.c,
 * hp3b31_survcam.c, trn_*_cameras.c). */
static const char *const RANKED[] = {
  /* still images */
  "snapshoturl", "stillurl", "imageurl", "cameraimageurl", "camimageurl",
  "imgurl", "imagelink", "imagedisplay", "latestimageurl", "latestimage",
  "currentimageurl", "currentimage", "liveimageurl", "liveimage",
  "webcamimage", "camimage", "camimg", "jpegurl", "jpgurl", "photourl",
  "pictureurl", "imageview", "snapshot", "still", "image", "picture", "photo",
  /* streams */
  "streamurl", "mjpegurl", "hlsurl", "m3u8url", "rtspurl", "videourl",
  "liveurl", "httpsurl", "stream", "video",
  /* thumbnails */
  "thumbnailurl", "thumburl", "previewurl", "thumbnail", "thumb", "preview",
  /* pages that are sometimes the image itself */
  "webcamurl", "cameraurl", "cctvurl", "url",
  NULL
};

static int rank_of(const char *folded) {
  for (int i = 0; RANKED[i]; i++)
    if (strcmp(RANKED[i], folded) == 0) return i;
  return -1;
}

/* The tail of the list: keys that are usually the camera's PAGE. A record that
 * has per-view targets does not also get one of these as its own target. */
static int is_page_rank(int r) {
  return r >= 0 && r >= rank_of("webcamurl");
}

/* Fields that name an element (for the target suffix) and label it. */
static const char *const ID_KEYS[] = {
  "presetid", "viewid", "cameraid", "camid", "imageid", "cctvid", "deviceid",
  "id", "code", NULL };
static const char *const LABEL_KEYS[] = {
  "presentationname", "name", "title", "description", "direction",
  "location", NULL };

/* ── per-source overrides ────────────────────────────────────────────────── *
 * Paths, case-insensitive, matched against the leaf's full dotted path. `#`
 * matches a run of digits and makes each distinct run its own target (the
 * index is the suffix id). Each entry was found by reading the row that
 * produces the record — the reason is next to it. */
typedef struct {
  const char *source;   /* exact source id, or a prefix ending in '*'         */
  const char *paths;    /* comma-separated, in preference order               */
} cf_override;

static const cf_override OVERRIDES[] = {
  /* hp3b29a_webcams.c: ArcGIS geojson with the camera slots flattened INTO the
   * key name (`properties.cameras_0_latestImage`, `…_1_…`); no generic rule
   * can see an index that is part of a word. One target per slot. */
  { "JO29A_CAM_US_AK_DOT_RWIS", "properties.cameras_#_latestImage" },
  /* hp3b29a_webcams.c: Socrata `url` column type — {"url": …} flattened to
   * `image_view.url`, whose last segment alone would rank as a page. */
  { "JO29A_CAM_US_LA_BATON_ROUGE", "image_view.url,image_view" },
  /* hp3b30_jpcam.c: a CSV whose header is FULLWIDTH ＵＲＬ — case folding is
   * ASCII-only, so the generic `url` rule never matches it. */
  { "JO30_CAM_TOKUSHIMA_LIVECAM_INVENTORY", "ＵＲＬ" },
  { NULL, NULL }
};

/* ── small helpers ───────────────────────────────────────────────────────── */

static void fold(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < cap; p++) {
    if (*p == '_' || *p == '-' || *p == '@' || *p == ' ') continue;
    out[o++] = (char)tolower(*p);
  }
  out[o] = '\0';
}

static int all_digits(const char *s, size_t n) {
  if (n == 0) return 0;
  for (size_t i = 0; i < n; i++) if (!isdigit((unsigned char)s[i])) return 0;
  return 1;
}

/* A target URL must be something camera_stills can dial: http(s) or rtsp(s).
 * Everything else — a filename, a relative path, a `javascript:` link — is not
 * a feed, whatever its key is called. */
static int feed_scheme_ok(const char *u) {
  if (!u || !*u) return 0;
  return strncasecmp(u, "http://", 7) == 0 || strncasecmp(u, "https://", 8) == 0 ||
         strncasecmp(u, "rtsp://", 7) == 0 || strncasecmp(u, "rtsps://", 8) == 0;
}

/* ── the leaf walk ───────────────────────────────────────────────────────── */

typedef struct {
  char        path[256];
  char        group[200];
  char        field[96];        /* folded last segment                       */
  const char *sval;             /* string leaf, or NULL                       */
  double      nval;
  int         is_num;
} cf_leaf;

typedef struct { cf_leaf *v; int n, cap; } cf_leaves;

/* group = prefix of `path` through its last all-digit segment, or "". */
static void group_of(const char *path, char *out, size_t cap) {
  out[0] = '\0';
  const char *seg = path;
  const char *best_end = NULL;
  for (const char *p = path; ; p++) {
    if (*p == '.' || *p == '\0') {
      if (all_digits(seg, (size_t)(p - seg))) best_end = p;
      if (*p == '\0') break;
      seg = p + 1;
    }
  }
  if (best_end) {
    size_t n = (size_t)(best_end - path);
    if (n >= cap) n = cap - 1;
    memcpy(out, path, n);
    out[n] = '\0';
  }
}

static void leaf_add(cf_leaves *L, const char *path, const cJSON *v) {
  if (L->n >= CF_MAX_LEAVES) return;
  if (L->n == L->cap) {
    int nc = L->cap ? L->cap * 2 : 64;
    cf_leaf *q = realloc(L->v, (size_t)nc * sizeof *q);
    if (!q) return;
    L->v = q; L->cap = nc;
  }
  cf_leaf *f = &L->v[L->n];
  memset(f, 0, sizeof *f);
  snprintf(f->path, sizeof f->path, "%s", path);
  group_of(f->path, f->group, sizeof f->group);
  const char *last = strrchr(f->path, '.');
  fold(last ? last + 1 : f->path, f->field, sizeof f->field);
  if (cJSON_IsString(v)) f->sval = v->valuestring;
  else if (cJSON_IsNumber(v)) { f->nval = v->valuedouble; f->is_num = 1; }
  L->n++;
}

static void walk(cf_leaves *L, const cJSON *node, const char *path, int depth) {
  if (!node || depth > CF_MAX_DEPTH) return;
  char sub[256];
  int idx = 0;
  const cJSON *ch;
  cJSON_ArrayForEach(ch, node) {
    if (cJSON_IsArray(node))
      snprintf(sub, sizeof sub, "%s%s%d", path, *path ? "." : "", idx++);
    else if (ch->string)
      snprintf(sub, sizeof sub, "%s%s%s", path, *path ? "." : "", ch->string);
    else continue;
    if (cJSON_IsObject(ch) || cJSON_IsArray(ch)) walk(L, ch, sub, depth + 1);
    else if (cJSON_IsString(ch) || cJSON_IsNumber(ch)) leaf_add(L, sub, ch);
  }
}

/* ── candidate collection ────────────────────────────────────────────────── */

typedef struct {
  char        group[200];
  char        arrname[112];     /* the group minus its index: "views"        */
  char        idx[24];          /* the index segment: "3"                    */
  const char *url;
  char        key[128];
  int         rank;
  int         order;            /* first-seen order, for stable output       */
} cf_cand;

typedef struct { cf_cand *v; int n, cap; } cf_cands;

static cf_cand *cand_get(cf_cands *C, const char *group, int create) {
  for (int i = 0; i < C->n; i++)
    if (strcmp(C->v[i].group, group) == 0) return &C->v[i];
  if (!create) return NULL;
  if (C->n == C->cap) {
    int nc = C->cap ? C->cap * 2 : 16;
    cf_cand *q = realloc(C->v, (size_t)nc * sizeof *q);
    if (!q) return NULL;
    C->v = q; C->cap = nc;
  }
  cf_cand *c = &C->v[C->n];
  memset(c, 0, sizeof *c);
  snprintf(c->group, sizeof c->group, "%s", group);
  c->rank = 1 << 30;
  c->order = C->n;
  const char *dot = strrchr(group, '.');
  if (dot) {
    size_t n = (size_t)(dot - group);
    if (n >= sizeof c->arrname) n = sizeof c->arrname - 1;
    memcpy(c->arrname, group, n);
    c->arrname[n] = '\0';
    snprintf(c->idx, sizeof c->idx, "%s", dot + 1);
  } else if (*group) {             /* a top-level array: "0" */
    snprintf(c->arrname, sizeof c->arrname, "items");
    snprintf(c->idx, sizeof c->idx, "%s", group);
  }
  C->n++;
  return c;
}

static void cand_offer(cf_cands *C, const char *group, const char *url,
                       const char *key, int rank) {
  cf_cand *c = cand_get(C, group, 1);
  if (!c || rank >= c->rank) return;
  c->url = url;
  c->rank = rank;
  snprintf(c->key, sizeof c->key, "%s", key);
}

/* Override path match: case-insensitive, `#` = a digit run. On a match that
 * consumed a `#`, *grp gets "<pattern before #, trailing ._ stripped>.<digits>"
 * so each slot is its own group. Returns 1 on a full match. */
static int ov_match(const char *pat, size_t plen, const char *s,
                    char *grp, size_t gcap) {
  size_t i = 0;
  const char *sp = s;
  grp[0] = '\0';
  while (i < plen) {
    if (pat[i] == '#') {
      const char *d0 = sp;
      while (isdigit((unsigned char)*sp)) sp++;
      if (sp == d0) return 0;
      size_t pre = i;
      while (pre > 0 && (pat[pre - 1] == '_' || pat[pre - 1] == '.')) pre--;
      const char *nm = pat;
      for (size_t k = 0; k < pre; k++) if (pat[k] == '.') nm = pat + k + 1;
      snprintf(grp, gcap, "%.*s.%.*s", (int)(pre - (size_t)(nm - pat)), nm,
               (int)(sp - d0), d0);
      i++;
      continue;
    }
    if (!*sp || tolower((unsigned char)pat[i]) != tolower((unsigned char)*sp))
      return 0;
    i++; sp++;
  }
  return *sp == '\0';
}

static const cf_override *override_for(const char *sid) {
  if (!sid || !*sid) return NULL;
  for (int i = 0; OVERRIDES[i].source; i++) {
    const char *o = OVERRIDES[i].source;
    size_t n = strlen(o);
    if (n && o[n - 1] == '*') { if (strncmp(sid, o, n - 1) == 0) return &OVERRIDES[i]; }
    else if (strcmp(sid, o) == 0) return &OVERRIDES[i];
  }
  return NULL;
}

/* ── suffix id / label for an array element ──────────────────────────────── */

static void sanitize(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < cap; p++)
    out[o++] = (isalnum(*p) || *p == '.' || *p == '_' || *p == '-') ? (char)*p : '_';
  out[o] = '\0';
}

static void element_meta(const cf_leaves *L, const char *group,
                         char *id, size_t idcap, char *label, size_t lcap) {
  id[0] = label[0] = '\0';
  int best_id = 1 << 30, best_lb = 1 << 30;
  size_t gl = strlen(group);
  for (int i = 0; i < L->n; i++) {
    const cf_leaf *f = &L->v[i];
    if (strcmp(f->group, group) != 0) continue;
    /* only DIRECT fields of the element — `views.3.id`, not `views.3.x.id` */
    if (strchr(f->path + gl + (f->path[gl] == '.' ? 1 : 0), '.')) continue;
    for (int k = 0; ID_KEYS[k]; k++) {
      if (k >= best_id || strcmp(ID_KEYS[k], f->field) != 0) continue;
      char raw[96];
      if (f->sval && f->sval[0]) snprintf(raw, sizeof raw, "%s", f->sval);
      else if (f->is_num) snprintf(raw, sizeof raw, "%.0f", f->nval);
      else continue;
      sanitize(raw, id, idcap);
      best_id = k;
    }
    for (int k = 0; LABEL_KEYS[k]; k++) {
      if (k >= best_lb || strcmp(LABEL_KEYS[k], f->field) != 0) continue;
      if (!f->sval || !f->sval[0]) continue;
      snprintf(label, lcap, "%s", f->sval);
      best_lb = k;
    }
  }
}

/* ── public ──────────────────────────────────────────────────────────────── */

void camera_feed_free(camera_feed_target *t, int n) {
  if (!t) return;
  for (int i = 0; i < n; i++) { free(t[i].url); t[i].url = NULL; }
}

int camera_feed_target_id(const char *uid, const char *suffix,
                          char *out, size_t cap) {
  if (!out || !cap) return 0;
  out[0] = '\0';
  if (!uid || !*uid) return 0;
  int n = (suffix && *suffix)
            ? snprintf(out, cap, "%s" CAM_FEED_VIEW_SEP "%s", uid, suffix)
            : snprintf(out, cap, "%s", uid);
  if (n < 0 || (size_t)n >= cap) { out[0] = '\0'; return 0; }
  return 1;
}

char *camera_feed_split(const char *tid, const char **suffix) {
  if (suffix) *suffix = NULL;
  if (!tid) return NULL;
  const char *hit = NULL;
  size_t sl = strlen(CAM_FEED_VIEW_SEP);
  for (const char *p = strstr(tid, CAM_FEED_VIEW_SEP); p;
       p = strstr(p + 1, CAM_FEED_VIEW_SEP))
    hit = p;
  if (!hit) return strdup(tid);
  size_t n = (size_t)(hit - tid);
  char *uid = malloc(n + 1);
  if (!uid) return NULL;
  memcpy(uid, tid, n);
  uid[n] = '\0';
  if (suffix) *suffix = hit + sl;
  return uid;
}

int camera_feed_resolve(const char *props_json, const char *source_id,
                        camera_feed_target *out, int cap) {
  if (!props_json || !*props_json) return 0;
  cJSON *root = cJSON_Parse(props_json);
  if (!root) return 0;
  if (!cJSON_IsObject(root)) { cJSON_Delete(root); return 0; }

  /* hpengine stamps its own source_id into every record it writes, so a
   * caller that only has the properties still gets the override. */
  if (!source_id || !*source_id) {
    const cJSON *s = cJSON_GetObjectItemCaseSensitive(root, "source_id");
    if (cJSON_IsString(s)) source_id = s->valuestring;
  }

  cf_leaves L = { 0 };
  walk(&L, root, "", 0);

  cf_cands C = { 0 };
  const cf_override *ov = override_for(source_id);
  if (ov) {
    /* Each override path, in order, ranks below every generic key so an
     * override always wins; the first path listed wins among overrides. */
    const char *p = ov->paths;
    int pr = -1000;
    while (p && *p) {
      const char *e = strchr(p, ',');
      size_t pl = e ? (size_t)(e - p) : strlen(p);
      for (int i = 0; i < L.n; i++) {
        char g[200];
        if (!feed_scheme_ok(L.v[i].sval)) continue;
        if (!ov_match(p, pl, L.v[i].path, g, sizeof g)) continue;
        cand_offer(&C, g, L.v[i].sval, L.v[i].path, pr);
      }
      pr++;
      p = e ? e + 1 : NULL;
    }
  }
  if (C.n == 0) {
    for (int i = 0; i < L.n; i++) {
      const cf_leaf *f = &L.v[i];
      if (!feed_scheme_ok(f->sval)) continue;
      int r = rank_of(f->field);
      if (r < 0) continue;
      cand_offer(&C, f->group, f->sval, f->path, r);
    }
  }

  /* A record with per-view targets keeps its OWN target only when that is an
   * image/stream key — a top-level `url` there is the station's page. */
  int have_views = 0;
  for (int i = 0; i < C.n; i++) if (C.v[i].group[0] && C.v[i].url) have_views = 1;
  for (int i = 0; i < C.n; i++)
    if (!C.v[i].group[0] && have_views && is_page_rank(C.v[i].rank))
      C.v[i].url = NULL;

  /* The record's own target first, then the views in the order they appear. */
  int *ord = calloc((unsigned)C.n + 1u, sizeof *ord);
  int no = 0;
  if (ord) {
    for (int i = 0; i < C.n; i++) if (!C.v[i].group[0]) ord[no++] = i;
    for (int i = 0; i < C.n; i++) if (C.v[i].group[0])  ord[no++] = i;
  }

  int total = 0;
  for (int oi = 0; oi < no; oi++) {
    cf_cand *c = &C.v[ord[oi]];
    if (!c->url) continue;
    /* Same URL twice (a station that repeats its default view at the top
     * level): the first occurrence is the target. */
    int dup = 0;
    for (int oj = 0; oj < oi && !dup; oj++) {
      const cf_cand *d = &C.v[ord[oj]];
      if (d->url && strcmp(d->url, c->url) == 0) dup = 1;
    }
    if (dup) continue;

    if (out && total < cap) {
      camera_feed_target *t = &out[total];
      memset(t, 0, sizeof *t);
      t->url = strdup(c->url);
      if (!t->url) continue;              /* OOM: lose the slot, not the run */
      t->rank = c->rank;
      snprintf(t->key, sizeof t->key, "%s", c->key);
      if (c->group[0]) {
        char id[64], label[128];
        element_meta(&L, c->group, id, sizeof id, label, sizeof label);
        char an[112];
        sanitize(c->arrname, an, sizeof an);
        snprintf(t->suffix, sizeof t->suffix, "%.60s/%.48s", an,
                 id[0] ? id : c->idx);
        snprintf(t->label, sizeof t->label, "%s", label);
        /* Two elements naming the same id: keep both, told apart by index. */
        for (int k = 0; k < total; k++)
          if (strcmp(out[k].suffix, t->suffix) == 0) {
            snprintf(t->suffix, sizeof t->suffix, "%.60s/%.40s~%.6s", an,
                     id[0] ? id : c->idx, c->idx);
            break;
          }
      }
    }
    total++;
  }
  free(ord);

  free(C.v);
  free(L.v);
  cJSON_Delete(root);
  return total;
}
