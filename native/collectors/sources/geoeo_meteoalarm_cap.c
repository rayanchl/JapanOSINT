/* MeteoAlarm — the pan-European severe weather warning aggregator (EUMETNET),
 * every member national met service's warnings normalised into CAP.
 *
 * Endpoint (keyless), the WORKING per-country Atom pattern:
 *   https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-<country>
 * The Italy feed is the one probed live (45 warnings, including Extreme heat);
 * the sibling country feeds use the identical schema and are polled
 * best-effort — a country feed that fails does NOT fail the run, only the
 * verified feed does.
 * Emits per <entry>: cap:event, cap:severity, cap:certainty, cap:urgency,
 *   cap:areaDesc, the EMMA_ID region code from cap:geocode, cap:onset,
 *   cap:expires, cap:sent, cap:scope, cap:message_type, cap:status,
 *   cap:polygon, cap:identifier, the CAP message link, the entry's Atom id,
 *   published/updated stamps and the entry title.
 * Licence: the feed states "Copyright 2026 MeteoAlarm.Org. Licensed under
 *   terms equivalent to CC BY 4.0, with additional requirements for
 *   redistributing outlined in our Terms and Conditions." Those extra
 *   conditions are: attribution to MeteoAlarm, NO ALTERATION of the warning
 *   text, and a link back to meteoalarm.org — the warning text is therefore
 *   carried verbatim and meteoalarm.org is carried as the link on every row.
 *
 * REJECTED PATHS (probed, from parse_notes): /feeds/meteoalarm-legacy-atom-europe
 * and /feeds/meteoalarm-legacy-rss-europe 404 or return a deprecation stub with
 * zero items; /api/v1/warnings/feeds-europe 404s. Only the per-country path
 * works.
 *
 * GEOMETRY: there is no point coordinate — <cap:geocode> valueName EMMA_ID
 * with a region code ("IT010") plus <cap:areaDesc> ("Umbria"). Some member
 * services publish a <cap:polygon> ("lat,lon lat,lon …", every Norway entry on
 * 2026-10-06); where one is present it is carried verbatim in properties and
 * as the row's GeoJSON geometry. has_geo stays 0: guessing a region centroid
 * is exactly the invented geometry R2 forbids.
 *
 * One <entry> exists per region per warning, so uid is identifier|EMMA_ID:
 * that keeps the per-region granularity while making repeated polls idempotent.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "geoeo_common.inc"
#include "lib/keyqual.h"
#include <strings.h>

#define MA_BASE "https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-"

/* The verified feed first; the rest are best-effort siblings. */
static const char *MA_COUNTRIES[] = {
  "italy", "france", "germany", "spain", "greece", "poland", "norway",
  "sweden", "austria", "portugal", "netherlands", "ireland", NULL
};

/* EMMA_ID lives in <cap:geocode><valueName>EMMA_ID</valueName><value>IT010</value>. */
static int entry_emma(const char *blk, char *out, size_t n) {
  out[0] = 0;
  const char *c = blk, *inner = NULL;
  int len = 0;
  while ((c = html_block(c, "geocode", &inner, &len)) != NULL) {
    if (len <= 0) continue;
    char *gb = geoeo_dupn(inner, (size_t)len);
    if (!gb) return 0;
    char vn[64], vv[96];
    if (geoeo_xml_text(gb, "valueName", vn, sizeof vn) &&
        strcasecmp(vn, "EMMA_ID") == 0 &&
        geoeo_xml_text(gb, "value", vv, sizeof vv)) {
      snprintf(out, n, "%s", vv);
      free(gb);
      return 1;
    }
    free(gb);
  }
  return 0;
}

/* One <entry>, held until every country feed has been read: the key census
 * must cover the whole set before any uid is decided (lib/keyqual.h). */
typedef struct {
  char *blk;                   /* the entry's inner XML, owned */
  const char *country;
  char key[384];               /* identifier|EMMA_ID (or title|areaDesc) */
  char atomid[480];            /* the entry's own Atom <id>, "" if absent */
} ma_entry;

typedef struct { ma_entry *v; int n, cap; } ma_list;

static void ma_list_free(ma_list *l) {
  for (int i = 0; i < l->n; i++) free(l->v[i].blk);
  free(l->v);
  l->v = NULL; l->n = l->cap = 0;
}

/* Fetch one country feed and append its entries. Returns the number of
 * entries read, or -1 when the feed could not be fetched. */
static int ma_collect(const source_ctx *ctx, const char *country, ma_list *l) {
  char url[256];
  snprintf(url, sizeof url, "%s%s", MA_BASE, country);
  char *body = feed_get_text(ctx->http, url, 30000);
  if (!body) return -1;

  int entries = 0;
  const char *cur = body, *inner = NULL;
  int ilen = 0;
  while ((cur = html_block(cur, "entry", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    char *blk = geoeo_dupn(inner, (size_t)ilen);
    if (!blk) break;
    entries++;
    char title[512], ident[256], area[256], emma[64];
    int has_title = geoeo_xml_text(blk, "title", title, sizeof title);
    geoeo_xml_text(blk, "identifier", ident, sizeof ident);
    geoeo_xml_text(blk, "areaDesc", area, sizeof area);
    entry_emma(blk, emma, sizeof emma);
    if (!has_title && !ident[0]) { free(blk); continue; }
    if (l->n == l->cap) {
      int nc = l->cap ? l->cap * 2 : 256;
      ma_entry *nv = realloc(l->v, (size_t)nc * sizeof *nv);
      if (!nv) { free(blk); break; }
      l->v = nv; l->cap = nc;
    }
    ma_entry *e = &l->v[l->n++];
    e->blk = blk;
    e->country = country;
    snprintf(e->key, sizeof e->key, "%s|%s",
             ident[0] ? ident : title, emma[0] ? emma : area);
    if (!geoeo_xml_text(blk, "id", e->atomid, sizeof e->atomid)) e->atomid[0] = 0;
  }
  free(body);
  return entries;
}

/* CAP <polygon> is "lat,lon lat,lon …" (WGS84, closed ring). Returns a GeoJSON
 * Polygon string the caller frees, or NULL if the text is not a valid ring.
 * Upstream geometry carried as published, not a derived centroid (R2). */
static char *ma_polygon_geojson(const char *txt) {
  if (!txt || !*txt) return NULL;
  cJSON *ring = cJSON_CreateArray();
  int pts = 0;
  const char *p = txt;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) break;
    char *end = NULL;
    double la = strtod(p, &end);
    if (end == p || *end != ',') { cJSON_Delete(ring); return NULL; }
    p = end + 1;
    double lo = strtod(p, &end);
    if (end == p || !geoeo_ll_ok(la, lo)) { cJSON_Delete(ring); return NULL; }
    p = end;
    cJSON *pt = cJSON_CreateArray();
    cJSON_AddItemToArray(pt, cJSON_CreateNumber(lo));
    cJSON_AddItemToArray(pt, cJSON_CreateNumber(la));
    cJSON_AddItemToArray(ring, pt);
    pts++;
  }
  if (pts < 4) { cJSON_Delete(ring); return NULL; }
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Polygon");
  cJSON *rings = cJSON_AddArrayToObject(g, "coordinates");
  cJSON_AddItemToArray(rings, ring);
  char *out = cJSON_PrintUnformatted(g);
  cJSON_Delete(g);
  return out;
}

/* href of the entry's <link type="application/cap+xml" …/> — the full CAP
 * message the Atom entry summarises. */
static int ma_cap_link(const char *blk, char *out, size_t n) {
  out[0] = 0;
  const char *p = blk;
  while ((p = strstr(p, "<link")) != NULL) {
    const char *gt = strchr(p, '>');
    if (!gt) break;
    size_t len = (size_t)(gt - p);
    char tag[1024];
    if (len >= sizeof tag) len = sizeof tag - 1;
    memcpy(tag, p, len); tag[len] = 0;
    char ty[96];
    if (html_attr(tag, "type", ty, sizeof ty) &&
        strcasecmp(ty, "application/cap+xml") == 0 &&
        html_attr(tag, "href", out, n)) {
      geoeo_unescape(out);
      return 1;
    }
    p = gt;
  }
  return 0;
}

static int ma_emit(intel_sink *sink, keyqual *kq, const ma_entry *e,
                   int *folded) {
  const char *blk = e->blk;
  char title[512], ident[256], event[128], sev[64], cert[64], urg[64];
  char area[256], onset[64], expires[64], emma[64];
  char sent[64], scope[64], mtype[64], status[64], published[64], updated[64];
  char caplink[512];
  int has_title = geoeo_xml_text(blk, "title", title, sizeof title);
  geoeo_xml_text(blk, "identifier", ident, sizeof ident);
  geoeo_xml_text(blk, "event", event, sizeof event);
  geoeo_xml_text(blk, "severity", sev, sizeof sev);
  geoeo_xml_text(blk, "certainty", cert, sizeof cert);
  geoeo_xml_text(blk, "urgency", urg, sizeof urg);
  geoeo_xml_text(blk, "areaDesc", area, sizeof area);
  geoeo_xml_text(blk, "onset", onset, sizeof onset);
  geoeo_xml_text(blk, "expires", expires, sizeof expires);
  geoeo_xml_text(blk, "sent", sent, sizeof sent);
  geoeo_xml_text(blk, "scope", scope, sizeof scope);
  geoeo_xml_text(blk, "message_type", mtype, sizeof mtype);
  geoeo_xml_text(blk, "status", status, sizeof status);
  geoeo_xml_text(blk, "published", published, sizeof published);
  geoeo_xml_text(blk, "updated", updated, sizeof updated);
  ma_cap_link(blk, caplink, sizeof caplink);
  entry_emma(blk, emma, sizeof emma);

  /* The polygon can run to thousands of characters: read it in place. */
  char *poly = NULL;
  {
    const char *pin = NULL; int plen = 0;
    if (html_block(blk, "polygon", &pin, &plen) && plen > 0) {
      poly = geoeo_dupn(pin, (size_t)plen);
      if (poly) geoeo_trim(poly);
    }
  }
  char *geom = ma_polygon_geojson(poly);

  /* One CAP alert area can be published as several Atom entries, one per
   * polygon: the Norway feed carries identifier+area pairs more than once,
   * identical in every CAP field and differing only in <cap:polygon> and in
   * the entry's own <id> (…&index_polygon=0 / =1). Every member of such a
   * group is qualified by that entry id; a unique pair keeps its plain key. */
  char rkbuf[1024];
  const char *rk = e->key;
  if (keyqual_count(kq, e->key) > 1) {
    rk = keyqual_uid(kq, e->key, e->atomid, blk, rkbuf, sizeof rkbuf);
    if (!keyqual_claim(kq, e->key, rk)) {
      (*folded)++; free(poly); free(geom); return 0;
    }
  }

  cJSON *props = cJSON_CreateObject();
  geoeo_add_str(props, "title", title);
  geoeo_add_str(props, "cap_identifier", ident);
  geoeo_add_str(props, "cap_event", event);
  geoeo_add_str(props, "cap_severity", sev);
  geoeo_add_str(props, "cap_certainty", cert);
  geoeo_add_str(props, "cap_urgency", urg);
  geoeo_add_str(props, "cap_areaDesc", area);
  geoeo_add_str(props, "cap_onset", onset);
  geoeo_add_str(props, "cap_expires", expires);
  geoeo_add_str(props, "cap_sent", sent);
  geoeo_add_str(props, "cap_scope", scope);
  geoeo_add_str(props, "cap_message_type", mtype);
  geoeo_add_str(props, "cap_status", status);
  geoeo_add_str(props, "cap_polygon", poly);
  geoeo_add_str(props, "cap_message_url", caplink);
  geoeo_add_str(props, "emma_id", emma);
  geoeo_add_str(props, "atom_id", e->atomid);
  geoeo_add_str(props, "atom_published", published);
  geoeo_add_str(props, "atom_updated", updated);
  cJSON_AddStringToObject(props, "country_feed", e->country);
  cJSON_AddStringToObject(props, "attribution",
                          "MeteoAlarm.org / EUMETNET member national met "
                          "service — warning text unaltered");
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[384];
  snprintf(summary, sizeof summary, "%s%s%s%s%s", sev[0] ? sev : "",
           (sev[0] && area[0]) ? " · " : "", area[0] ? area : "",
           onset[0] ? " · from " : "", onset[0] ? onset : "");

  intel_item it = {0};
  it.remote_key = rk;
  it.title = has_title ? title : ident;
  it.summary = summary[0] ? summary : NULL;
  it.link = "https://meteoalarm.org/";     /* licence: link back required */
  it.lang = "en";
  it.published_at = onset[0] ? onset : NULL;
  it.record_type = "weather-warning";
  it.has_geo = 0;                 /* no point coordinate; the polygon, if any, is the geometry */
  it.geometry_geojson = geom;
  it.properties_json = pj ? pj : "{}";
  it.tags_json = "[\"weather\",\"warning\",\"cap\",\"meteoalarm\",\"europe\"]";
  int ok = sink->emit(sink, &it) >= 0;
  free(pj);
  free(poly);
  free(geom);
  return ok;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  ma_list l = {0};
  int primary = ma_collect(ctx, MA_COUNTRIES[0], &l);
  if (primary < 0) {
    fprintf(stderr, "[meteoalarm-cap] %s feed fetch failed\n", MA_COUNTRIES[0]);
    return -1;
  }
  int failed = 0;
  for (int i = 1; MA_COUNTRIES[i]; i++)
    if (ma_collect(ctx, MA_COUNTRIES[i], &l) < 0) failed++;   /* best-effort siblings */

  /* Pass 1: every key over EVERY country's entries; pass 2: emit. It used to
   * be first-come-plain — the plain key went to whichever polygon's entry the
   * feed listed first, so a re-ordered feed stored one polygon's entry under
   * the other's uid. See lib/keyqual.h. */
  keyqual kq = {0};
  for (int i = 0; i < l.n; i++) keyqual_add(&kq, l.v[i].key, l.v[i].atomid);
  keyqual_seal(&kq);
  int n = 0, folded = 0;
  for (int i = 0; i < l.n; i++) n += ma_emit(sink, &kq, &l.v[i], &folded);
  keyqual_free(&kq);
  int total = l.n;
  ma_list_free(&l);

  fprintf(stderr, "[meteoalarm-cap] emitted %d of %d warning/region entries "
          "(%d byte-identical repeats folded; %d sibling feeds failed)\n",
          n, total, folded, failed);
  return 0;                                  /* calm weather → 0 is honest */
}

static const source_def geoeo_meteoalarm_def = {
  .id = "meteoalarm-cap", .collector = "environment",
  .name = "MeteoAlarm European Weather Warnings (CAP Atom)",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-italy",
  .description = "The pan-European severe weather warning aggregator — every EUMETNET member's national met service warnings normalised into CAP, per country feed.",
  .license = "MeteoAlarm.Org, terms equivalent to CC BY 4.0 with extra redistribution conditions: attribution, no alteration of warning text, link back to meteoalarm.org.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_meteoalarm_def)
