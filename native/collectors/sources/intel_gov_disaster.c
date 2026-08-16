/* collectors/sources/intel_gov_disaster.c — real-time government, disaster and
 * geo-hazard open feeds. Official advisories (travel/security, law enforcement),
 * multilateral humanitarian updates, and live geophysical alert streams (global
 * earthquakes, disaster events, volcanic activity). Real RSS/Atom via
 * rss_collect; each becomes an FTS-indexed intel row the alert hooks can raise
 * on. These extend the JP-domestic disaster collectors (jma, saigai, bosai) to
 * worldwide coverage. Scheduled on tighter intervals for the live hazard feeds. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/rss_atom.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COLL "government"

#define RSS_I(SYM, ID, NAME, NAMEJA, CAT, URL, LANG, TAGS, DESC, IVAL)       \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

#define RSS(SYM, ID, NAME, NAMEJA, CAT, URL, LANG, TAGS, DESC) \
  RSS_I(SYM, ID, NAME, NAMEJA, CAT, URL, LANG, TAGS, DESC, 3600)

/* ------------------------------------------------------------------------
 * audit-09: the geo-hazard streams below were RSS/Atom, and rss_collect()
 * carries no coordinates — so every earthquake, cyclone and flood landed in
 * intel_items with lat/lon NULL and could never draw a map pin. All four are
 * now read from the publishers' GeoJSON, which is both geo-bearing and far
 * richer (magnitude, depth, alert level, felt reports, tsunami flag …).
 * ---------------------------------------------------------------------- */

static int js_num(cJSON *o, const char *k, double *out) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsNumber(v)) return 0;
  *out = v->valuedouble; return 1;
}
/* epoch-milliseconds → "YYYY-MM-DDTHH:MM:SSZ" */
static void ms_iso(double ms, char *out, size_t n) {
  time_t t = (time_t)(ms / 1000.0);
  struct tm tm; gmtime_r(&t, &tm);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}
/* Point geometry → lat/lon (+ depth from the third ordinate when present). */
static int geom_point(cJSON *g, double *lat, double *lon, double *depth) {
  if (!g) return 0;
  const char *ty = jo_sv(g, "type");
  cJSON *c = cJSON_GetObjectItem(g, "coordinates");
  if (!ty || !c || strcmp(ty, "Point") != 0) return 0;
  cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1),
        *z = cJSON_GetArrayItem(c, 2);
  if (!x || !y || !cJSON_IsNumber(x) || !cJSON_IsNumber(y)) return 0;
  *lon = x->valuedouble; *lat = y->valuedouble;
  if (z && cJSON_IsNumber(z) && depth) *depth = z->valuedouble;
  return 1;
}

/* --- USGS earthquake GeoJSON summary feeds ------------------------------ */
static int usgs_run(const source_ctx *ctx, intel_sink *sink, const char *url) {
  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc) { fprintf(stderr, "[%s] fetch failed %s\n", ctx->source_id, url);
              return -1; }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0; cJSON *f;
  cJSON_ArrayForEach(f, feats ? feats : NULL) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!p) continue;
    double lat = 0, lon = 0, depth = 0;
    int geo = geom_point(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &depth);
    double ms = 0; char iso[32] = {0};
    if (js_num(p, "time", &ms)) ms_iso(ms, iso, sizeof iso);

    char body[512]; double mag = 0; int hasmag = js_num(p, "mag", &mag);
    const char *place = jo_sv(p, "place");
    snprintf(body, sizeof body, "M %.1f%s%s — depth %.1f km%s%s",
             hasmag ? mag : 0.0,
             place ? " — " : "", place ? place : "",
             depth,
             iso[0] ? " at " : "", iso[0] ? iso : "");

    /* the upstream property bag verbatim + the depth the geometry carries */
    cJSON *props = cJSON_Duplicate(p, 1);
    cJSON_AddNumberToObject(props, "depth_km", depth);
    cJSON_AddStringToObject(props, "record_type", "earthquake");
    char *pj = cJSON_PrintUnformatted(props);
    char *gj = cJSON_PrintUnformatted(cJSON_GetObjectItem(f, "geometry"));

    intel_item it = {0};
    const char *id = jo_sv(f, "id");
    it.remote_key = id;
    it.title = jo_sv(p, "title");
    it.body = body; it.summary = body;
    it.link = jo_sv(p, "url");
    it.lang = "en";
    it.published_at = iso[0] ? iso : NULL;
    it.has_geo = geo; it.lat = lat; it.lon = lon;
    it.geometry_geojson = gj;
    it.record_type = "earthquake";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"earthquake\",\"usgs\",\"global\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); free(gj); cJSON_Delete(props);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", ctx->source_id, n);
  return 0;
}

#define USGS(SYM, ID, NAME, NAMEJA, URL, DESC, IVAL)                          \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    return usgs_run(c, s, URL); }                                             \
  static const source_def SYM = {                                            \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,             \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = "environment", .type = "api", .url = URL,                     \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

USGS(gd_usgs_sig, "usgs-quake-significant", "USGS Significant Earthquakes (7d)",
  "USGS 主要地震",
  "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/significant_week.geojson",
  "USGS significant earthquakes worldwide, past 7 days (GeoJSON: epicentre, "
  "magnitude, depth, alert level, felt reports, tsunami flag)", 900);

USGS(gd_usgs_25, "usgs-quake-m25-day", "USGS M2.5+ Earthquakes (24h)",
  "USGS M2.5+ 地震",
  "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/2.5_day.geojson",
  "USGS magnitude 2.5+ earthquakes worldwide, past 24 hours (GeoJSON)", 600);

USGS(gd_usgs_45, "usgs-quake-m45-week", "USGS M4.5+ Earthquakes (7d)",
  "USGS M4.5+ 地震",
  "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_week.geojson",
  "USGS magnitude 4.5+ earthquakes worldwide, past 7 days (GeoJSON)", 1800);

/* --- GDACS global disaster alerts (GeoJSON event list) ------------------ */
#define GDACS_URL "https://www.gdacs.org/gdacsapi/api/events/geteventlist/MAP"

/* Mean of every finite [x,y] position anywhere under `node`. For a ring or a
 * cyclone track that is a representative point derived from the real geometry
 * — no coordinate is invented. */
static void vertex_mean(cJSON *node, double *sx, double *sy, int *cnt) {
  if (!cJSON_IsArray(node)) return;
  cJSON *a = cJSON_GetArrayItem(node, 0), *b = cJSON_GetArrayItem(node, 1);
  if (a && b && cJSON_IsNumber(a) && cJSON_IsNumber(b)) {
    *sx += a->valuedouble; *sy += b->valuedouble; (*cnt)++; return;
  }
  cJSON *ch; cJSON_ArrayForEach(ch, node) vertex_mean(ch, sx, sy, cnt);
}
static int geom_repr_point(cJSON *g, double *lat, double *lon) {
  double dep = 0;
  if (geom_point(g, lat, lon, &dep)) return 1;
  cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
  if (!c) return 0;
  double sx = 0, sy = 0; int cnt = 0;
  vertex_mean(c, &sx, &sy, &cnt);
  if (!cnt) return 0;
  *lon = sx / cnt; *lat = sy / cnt;
  return 1;
}

/* audit-09: geteventlist/MAP returns ONE FEATURE PER GEOMETRY LAYER — a
 * cyclone ships its centre point, its track line and several impact polygons.
 * Tonight that was 305 features for 38 distinct events. The old loop emitted
 * all 305 against 38 remote_keys, so each event's row was overwritten by
 * whichever layer happened to come last, and only the 15 events whose last
 * layer was a Point kept a lat/lon — the other 23 had no map pin at all
 * (has_geo came from geom_point(), which only understands "Point"). It also
 * reported records=305 to the scheduler for 38 stored rows.
 *
 * Now: one row per (eventtype,eventid,episodeid). Coordinates come from the
 * event's own Point layer when it has one, else from the mean of its polygon
 * vertices; the stored geometry prefers the impact polygon (that is the shape
 * worth drawing) and falls back to whatever layer exists. */
typedef struct { char key[96]; cJSON *pt; cJSON *poly; cJSON *any; cJSON *props;
                 int layers; } gd_event;

static int gdacs_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, GDACS_URL, 30000);
  if (!doc) { fprintf(stderr, "[gdacs] fetch failed\n"); return -1; }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  int cap = 256, nev = 0;
  gd_event *ev = calloc((size_t)cap, sizeof *ev);
  if (!ev) { cJSON_Delete(doc); return -1; }

  cJSON *f;
  cJSON_ArrayForEach(f, feats ? feats : NULL) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!p) continue;
    double eid = 0, epi = 0;
    js_num(p, "eventid", &eid); js_num(p, "episodeid", &epi);
    const char *etype = jo_sv(p, "eventtype");
    char key[96];
    snprintf(key, sizeof key, "%s-%.0f-%.0f", etype ? etype : "EV", eid, epi);

    int idx = -1;
    for (int i = 0; i < nev; i++)
      if (strcmp(ev[i].key, key) == 0) { idx = i; break; }
    if (idx < 0) {
      if (nev == cap) {
        int ncap = cap * 2;
        gd_event *tmp = realloc(ev, (size_t)ncap * sizeof *ev);
        if (!tmp) break;
        memset(tmp + cap, 0, (size_t)(ncap - cap) * sizeof *ev);
        ev = tmp; cap = ncap;
      }
      idx = nev++;
      snprintf(ev[idx].key, sizeof ev[idx].key, "%s", key);
      ev[idx].props = p;                 /* first layer's property bag */
    }
    ev[idx].layers++;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    const char *gt = g ? jo_sv(g, "type") : NULL;
    if (!gt) continue;
    if (!ev[idx].any) ev[idx].any = g;
    if (!ev[idx].pt && strcmp(gt, "Point") == 0) ev[idx].pt = g;
    if (!ev[idx].poly && (strcmp(gt, "Polygon") == 0 ||
                          strcmp(gt, "MultiPolygon") == 0)) ev[idx].poly = g;
  }

  int n = 0;
  for (int i = 0; i < nev; i++) {
    cJSON *p = ev[i].props;
    double lat = 0, lon = 0;
    int geo = geom_repr_point(ev[i].pt, &lat, &lon);
    if (!geo) geo = geom_repr_point(ev[i].poly, &lat, &lon);
    if (!geo) geo = geom_repr_point(ev[i].any, &lat, &lon);

    const char *title = jo_sv(p, "name");
    if (!title) title = jo_sv(p, "eventname");
    if (!title) title = jo_sv(p, "description");
    char *body = html_strip(jo_sv(p, "htmldescription"));

    /* url is an object of report/details/geometry links */
    const char *link = NULL;
    cJSON *u = cJSON_GetObjectItem(p, "url");
    if (u && cJSON_IsObject(u)) {
      link = jo_sv(u, "report");
      if (!link) link = jo_sv(u, "details");
    }

    cJSON *props = cJSON_Duplicate(p, 1);
    /* `polygonlabel`/`Class` describe ONE geometry layer, not the event, so
     * they would mislabel the merged row. */
    cJSON_DeleteItemFromObject(props, "polygonlabel");
    cJSON_DeleteItemFromObject(props, "Class");
    cJSON_AddStringToObject(props, "record_type", "disaster-alert");
    cJSON_AddNumberToObject(props, "geometry_layers", ev[i].layers);
    cJSON_AddBoolToObject(props, "has_impact_polygon", ev[i].poly != NULL);
    cJSON_AddStringToObject(props, "coord_source",
                            ev[i].pt ? "gdacs-point-layer"
                                     : "mean-of-polygon-vertices");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON *gsel = ev[i].poly ? ev[i].poly : (ev[i].pt ? ev[i].pt : ev[i].any);
    char *gj = gsel ? cJSON_PrintUnformatted(gsel) : NULL;

    intel_item it = {0};
    it.remote_key = ev[i].key;
    it.title = title;
    it.body = (body && *body) ? body : NULL;
    it.summary = (body && *body) ? body : NULL;
    it.link = link;
    it.lang = "en";
    it.published_at = jo_sv(p, "fromdate");
    it.has_geo = geo; it.lat = lat; it.lon = lon;
    it.geometry_geojson = gj;
    it.record_type = "disaster-alert";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"disaster\",\"alert\",\"global\",\"osint\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); free(gj); free(body); cJSON_Delete(props);
  }
  free(ev);
  cJSON_Delete(doc);
  fprintf(stderr, "[gdacs] emitted %d events\n", n);
  return 0;
}

static const source_def gd_gdacs = {
  .id = "gdacs", .collector = COLL, .name = "GDACS Global Disaster Alerts",
  .name_ja = "GDACS 災害アラート", .update_interval_sec = 900, .run = gdacs_run,
  .category = "environment", .type = "api", .url = GDACS_URL,
  .description = "Global Disaster Alert and Coordination System — earthquakes, "
                 "cyclones, floods, volcanoes, with event coordinates",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(gd_gdacs)

RSS_I(gd_volcano, "gvp-volcano-activity", "Smithsonian Weekly Volcanic Activity",
  "スミソニアン 火山活動週報", "environment",
  "https://volcano.si.edu/news/WeeklyVolcanoRSS.xml", "en",
  "[\"volcano\",\"hazard\",\"global\"]",
  "Smithsonian Global Volcanism Program — weekly volcanic activity report",
  21600);

/* --- US NWS active alerts -----------------------------------------------
 * audit-09: alerts.weather.gov no longer resolves at all (NXDOMAIN), so this
 * source returned rc=-1 on every run and sat quarantined. The replacement is
 * the official api.weather.gov GeoJSON, which also carries the alert polygon
 * — the CAP RSS never did. Properties are handed to geojson_emit_features so
 * polygons get an on-geometry representative point. */
#define NWS_URL "https://api.weather.gov/alerts/active"

static int nws_run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: japanosint-collector (osint feed reader)",
                         "Accept: application/geo+json", NULL };
  cJSON *doc = feed_get_json_h(ctx->http, NWS_URL, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[nws-alerts-us] fetch failed\n"); return -1; }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  cJSON *f;
  cJSON_ArrayForEach(f, feats ? feats : NULL) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!p) continue;
    /* audit-09 (2nd pass): most active alerts are zone-referenced and carry
     * "geometry": null. lib/geojson.c only checks that the key EXISTS before
     * cJSON_PrintUnformatted(), so a JSON null was being stored as the literal
     * four-character string "null" in intel_items.geometry — 243 of 256 rows
     * last run. Drop the key so the column stays NULL. */
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && cJSON_IsNull(g)) cJSON_DeleteItemFromObject(f, "geometry");
    const char *ev   = jo_sv(p, "event");
    const char *area = jo_sv(p, "areaDesc");
    const char *head = jo_sv(p, "headline");
    char title[512];
    if (head) snprintf(title, sizeof title, "%s", head);
    else snprintf(title, sizeof title, "%s%s%s", ev ? ev : "NWS alert",
                  area ? " — " : "", area ? area : "");
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddStringToObject(p, "record_type", "weather-alert");
    const char *sent = jo_sv(p, "sent");
    if (sent) cJSON_AddStringToObject(p, "published_at", sent);
    const char *web = jo_sv(p, "@id");
    if (web) cJSON_AddStringToObject(p, "link", web);
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("weather"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("alert"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("usa"));
    cJSON_AddItemToObject(p, "tags", tags);
  }
  int n = geojson_emit_doc(sink, ctx->source_id, doc);
  cJSON_Delete(doc);
  fprintf(stderr, "[nws-alerts-us] emitted %d\n", n);
  return 0;
}

static const source_def gd_nws = {
  .id = "nws-alerts-us", .collector = COLL, .name = "US NWS Weather Alerts",
  .name_ja = "米国 気象警報", .update_interval_sec = 900, .run = nws_run,
  .category = "environment", .type = "api", .url = NWS_URL,
  .description = "US National Weather Service active alerts (all hazards, "
                 "nationwide) with alert-area geometry",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(gd_nws)

RSS_I(gd_reliefweb, "reliefweb", "ReliefWeb Updates (UN OCHA)",
  "ReliefWeb 更新", "environment",
  "https://reliefweb.int/updates/rss.xml", "en",
  "[\"humanitarian\",\"disaster\",\"un\"]",
  "UN OCHA ReliefWeb — humanitarian situation reports and disaster updates",
  3600);

/* --- Government / security / law-enforcement advisories --- */
RSS(gd_state_travel, "us-state-travel", "US State Dept Travel Advisories",
  "米国務省 渡航情報", "government",
  "https://travel.state.gov/_res/rss/TAsTWs.xml", "en",
  "[\"advisory\",\"travel\",\"us-gov\"]",
  "US Department of State travel advisories and warnings by country");

RSS(gd_fcdo, "uk-fcdo-travel", "UK FCDO Travel Advice",
  "英国外務省 渡航情報", "government",
  "https://www.gov.uk/foreign-travel-advice.atom", "en",
  "[\"advisory\",\"travel\",\"uk-gov\"]",
  "UK Foreign, Commonwealth & Development Office foreign travel advice updates");

RSS(gd_fbi, "fbi-press", "FBI National Press Releases",
  "FBI 報道発表", "government",
  "https://www.fbi.gov/feeds/national-press-releases/rss.xml", "en",
  "[\"law-enforcement\",\"fbi\",\"us-gov\"]",
  "US FBI national press releases — investigations, charges and advisories");

RSS(gd_europol, "europol-news", "Europol Newsroom",
  "ユーロポール", "government",
  "https://www.europol.europa.eu/rss.xml", "en",
  "[\"law-enforcement\",\"europol\",\"eu\"]",
  "Europol newsroom — EU cross-border law-enforcement operations");

/* audit-09: /system/files/126/ofac.xml is 403 (the static file was pulled);
 * OFAC's site feed at /rss.xml is the live recent-actions stream. */
RSS(gd_ofac, "ofac-recent-actions", "US Treasury OFAC Recent Actions",
  "OFAC 制裁措置", "government",
  "https://ofac.treasury.gov/rss.xml", "en",
  "[\"sanctions\",\"ofac\",\"us-gov\"]",
  "US Treasury OFAC recent sanctions actions and SDN list updates");

RSS(gd_unnews, "un-news", "UN News",
  "国連ニュース", "government",
  "https://news.un.org/feed/subscribe/en/news/all/rss.xml", "en",
  "[\"un\",\"news\",\"global\"]",
  "United Nations News — official global UN system reporting");

RSS(gd_nato, "nato-news", "NATO News",
  "NATO ニュース", "government",
  "https://www.nato.int/cps/en/natohq/news.rss", "en",
  "[\"nato\",\"defense\",\"news\"]",
  "NATO official news releases and statements");

/* --- WHO Disease Outbreak News ------------------------------------------
 * audit-09: /feeds/entity/csr/don/en/rss.xml 404s — WHO dropped the RSS when
 * the site moved. The DON items are served by the site's own OData endpoint,
 * which is what who.int/emergencies/disease-outbreak-news itself reads. */
#define WHO_DON_URL "https://www.who.int/api/news/diseaseoutbreaknews"

static int who_don_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WHO_DON_URL, 25000);
  if (!doc) { fprintf(stderr, "[who-outbreak-news] fetch failed\n"); return -1; }
  cJSON *arr = cJSON_GetObjectItem(doc, "value");
  int n = 0; cJSON *e;
  cJSON_ArrayForEach(e, arr ? arr : NULL) {
    const char *id = jo_sv(e, "Id");
    const char *title = jo_sv(e, "Title");
    if (!title) title = jo_sv(e, "OverrideTitle");
    if (!id || !title) continue;
    const char *url_name = jo_sv(e, "UrlName");
    char link[512] = {0};
    if (url_name)
      snprintf(link, sizeof link,
               "https://www.who.int/emergencies/disease-outbreak-news/item/%s",
               url_name);
    char *body = html_strip(jo_sv(e, "Overview"));
    char *summ = html_strip(jo_sv(e, "Summary"));
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "who_id", id);
    if (url_name) cJSON_AddStringToObject(props, "url_name", url_name);
    const char *pd = jo_sv(e, "PublicationDate");
    if (pd) cJSON_AddStringToObject(props, "publication_date", pd);
    const char *lm = jo_sv(e, "LastModified");
    if (lm) cJSON_AddStringToObject(props, "last_modified", lm);
    char *ass = html_strip(jo_sv(e, "Assessment"));
    if (ass && *ass) cJSON_AddStringToObject(props, "who_risk_assessment", ass);
    char *adv = html_strip(jo_sv(e, "Advice"));
    if (adv && *adv) cJSON_AddStringToObject(props, "who_advice", adv);
    char *resp = html_strip(jo_sv(e, "Response"));
    if (resp && *resp) cJSON_AddStringToObject(props, "who_response", resp);
    char *pj = cJSON_PrintUnformatted(props);

    intel_item it = {0};
    it.remote_key = id;
    it.title = title;
    it.body = (body && *body) ? body : NULL;
    it.summary = (summ && *summ) ? summ : ((body && *body) ? body : NULL);
    it.link = link[0] ? link : NULL;
    it.lang = "en";
    it.published_at = pd;
    it.record_type = "who-outbreak-news";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"health\",\"outbreak\",\"who\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); free(body); free(summ); free(ass); free(adv); free(resp);
    cJSON_Delete(props);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[who-outbreak-news] emitted %d\n", n);
  return 0;
}

static const source_def gd_who_don = {
  .id = "who-outbreak-news", .collector = COLL,
  .name = "WHO Disease Outbreak News", .name_ja = "WHO 疾病アウトブレイク",
  .update_interval_sec = 3600, .run = who_don_run,
  .category = "health", .type = "api", .url = WHO_DON_URL,
  .description = "World Health Organization Disease Outbreak News — global "
                 "health emergencies, with WHO risk assessment and advice",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(gd_who_don)

/* audit-09: media/403372 is the retired 2019-nCoV feed — it answers 200 with
 * ZERO items, so the source was silently empty. media/132608 is the actual
 * "CDC Online Newsroom" feed. */
RSS(gd_cdc, "cdc-newsroom", "CDC Newsroom",
  "CDC ニュース", "health",
  "https://tools.cdc.gov/api/v2/resources/media/132608.rss", "en",
  "[\"health\",\"cdc\",\"usa\"]",
  "US CDC newsroom — outbreak, advisory and public-health releases");

RSS(gd_ecdc, "ecdc-threats", "ECDC Communicable Disease Threats",
  "ECDC 感染症脅威", "health",
  "https://www.ecdc.europa.eu/en/taxonomy/term/2942/feed", "en",
  "[\"health\",\"ecdc\",\"eu\"]",
  "European CDC communicable disease threat reports and risk assessments");

RSS(gd_firms, "nasa-firms-blog", "NASA Earth Observatory",
  "NASA 地球観測", "environment",
  "https://earthobservatory.nasa.gov/feeds/earth-observatory.rss", "en",
  "[\"satellite\",\"earth-observation\",\"nasa\"]",
  "NASA Earth Observatory — imagery-driven natural-event and environmental reporting");
