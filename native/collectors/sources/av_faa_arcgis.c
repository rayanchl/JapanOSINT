/* collectors/sources/av_faa_arcgis.c
 * FAA Aeronautical Information Services / UAS Data Delivery System — the
 * fourteen public ArcGIS Online FeatureServer layers, all keyless, all served
 * from https://services6.arcgis.com/ssFJjBXIUyZDrSYZ/ArcGIS/rest/services/.
 *
 *   faa-uas-facility-map        FAA_UAS_FacilityMap_Data_V5   (~370k Polygons)
 *   faa-special-use-airspace    Special_Use_Airspace          (~1.5k Polygons)
 *   faa-prohibited-areas        Prohibited_Areas              (13 Polygons)
 *   faa-national-defense-tfr    National_Defense_Airspace_TFR_Areas (8 Polygons)
 *   faa-military-training-routes MTRSegment                   (~3.7k LineStrings)
 *   faa-obstacles-dof           Digital_Obstacle_File         (~641k Points)
 *   faa-recreational-flyer-sites Recreational_Flyer_Fixed_Sites (~2.2k Polygons)
 *   faa-fria                    FAA_Recognized_Identification_Areas (~2.7k Polygons)
 *   faa-class-airspace          Class_Airspace                (~6.1k Polygons)
 *   faa-navaids                 NAVAIDSystem                  (~1.4k Points)
 *   faa-airports-heliports      ADHP                          (~36k Points)
 *   faa-runway-lines            RunwayLine                    (~42k LineStrings)
 *   faa-stadium-sites           Stadiums                      (203 Points)
 *   faa-part-time-nsufr         Part_Time_National_Security_UAS_Flight_Restrictions (5 Polygons)
 *
 * Emitted fields are the layer's own attributes verbatim (see each decorator
 * below for the exact list) plus a `title` and a `uid` built ONLY from values
 * the server returned.
 *
 * GEOMETRY (R2): every row's geometry is the GeoJSON `geometry` the server
 * returned — Polygon, MultiPolygon, LineString or Point. Nothing is ever
 * substituted: no airport point stands in for a Class-airspace polygon, no
 * corridor polygon is fabricated from an MTR's WIDTHLEFT/WIDTHRIGHT (those are
 * half-widths in NM, not a shape), no FRIA sponsor's postal address is
 * geocoded (the polygon is authoritative and the two genuinely differ).
 *
 * TRAP #1, quoted from the work list: "ArcGIS FeatureServer layers need
 * f=geojson and outSR=4326 or you get Web Mercator metres, which as lat/lon
 * land nowhere near the asset." Both parameters are pinned on every request in
 * av_arcgis_run().
 * TRAP #2: the server sets properties.exceededTransferLimit=true whenever a
 * page was truncated — even on a 2-row probe of a 13-row layer. We always
 * page on resultOffset/resultRecordCount rather than assuming one call is the
 * whole layer.
 *
 * PER-RUN CAPS (explicit, never silent): the two very large layers cannot be
 * pulled whole on a schedule — 370k and 641k features would be ~185 and ~321
 * requests per run. faa-uas-facility-map and faa-obstacles-dof therefore stop
 * at AV_CAP_HUGE features per run and LOG that the cap was reached; every
 * other layer is capped above its published feature count and so completes.
 *
 * R3: a page that fetches fine but is empty returns 0. Only a failure of the
 * FIRST page is reported as -1.
 *
 * Licence: US Government work (FAA Aeronautical Information Services / FAA UAS
 * Data Delivery System), public domain, keyless.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define FAA_ARC_LICENSE \
  "US Government work (FAA Aeronautical Information Services / FAA UAS Data " \
  "Delivery System) — public domain, keyless."

#define AV_CAP_HUGE   20000    /* documented per-run bound, logged when hit */
#define AV_CAP_LARGE  60000    /* above the published count → completes      */
#define AV_CAP_SMALL   8000

/* Add/replace a string property (ArcGIS layers sometimes already carry the
 * key — FRIA publishes its own `title`). */
static void arc_set(cJSON *p, const char *k, const char *v) {
  if (!v || !*v) return;
  if (cJSON_GetObjectItem(p, k)) cJSON_DeleteItemFromObject(p, k);
  cJSON_AddStringToObject(p, k, v);
}

/* Trimmed copy of a string attribute; NULL when absent/blank. FAA text fields
 * are heavily space-padded ("RIG               ", "DAUPHIN ISLAND  "). */
static const char *arc_s(const cJSON *p, const char *k, char *buf, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(p, k);
  if (!v || !cJSON_IsString(v) || !v->valuestring) return NULL;
  return av_trim(v->valuestring, buf, n);
}

/* Numeric attribute → double; 0 when absent. */
static double arc_n(const cJSON *p, const char *k) {
  double d = 0;
  return av_dbl(p, k, &d) ? d : 0;
}

/* Epoch MILLISECONDS attribute → an extra ISO property. Several FAA layers use
 * ms (FRIA startDate/endDate, ADHP LASTMOD_DATE, RunwayLine EFFECTIVE_DATE). */
static void arc_ms_iso(cJSON *p, const char *k, const char *outk) {
  double ms = 0;
  if (!av_dbl(p, k, &ms) || ms <= 0) return;
  char iso[32];
  const char *s = av_epoch_iso(ms / 1000.0, iso, sizeof iso);
  if (s) arc_set(p, outk, s);
}

static void arc_tags(cJSON *p, const char *t1, const char *t2) {
  if (cJSON_GetObjectItem(p, "tags")) cJSON_DeleteItemFromObject(p, "tags");
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
  if (t1) cJSON_AddItemToArray(tags, cJSON_CreateString(t1));
  if (t2) cJSON_AddItemToArray(tags, cJSON_CreateString(t2));
  cJSON_AddItemToObject(p, "tags", tags);
}

static void arc_common(cJSON *p, const char *uid_key, const char *record_type,
                       const char *tag) {
  char b[128];
  const char *u = arc_s(p, uid_key, b, sizeof b);
  if (u) arc_set(p, "uid", u);
  arc_set(p, "record_type", record_type);
  arc_set(p, "source", "FAA Aeronautical Information Services");
  arc_tags(p, tag, NULL);
}

/* ------------------------------------------------ vertical-limit helper --- *
 * UPPER_VAL/LOWER_VAL are STRINGS on some layers and numbers on others, with
 * separate UOM and datum codes (MSL/AGL/SFC). Never coerced blindly: the text
 * is assembled only from parts the server actually sent. */
static void arc_limits(char *out, size_t n, const cJSON *p) {
  char lb[32], lu[16], lc[16], ub[32], uu[16], uc[16];
  const char *lv = arc_s(p, "LOWER_VAL", lb, sizeof lb);
  const char *lU = arc_s(p, "LOWER_UOM", lu, sizeof lu);
  const char *lC = arc_s(p, "LOWER_CODE", lc, sizeof lc);
  const char *uv = arc_s(p, "UPPER_VAL", ub, sizeof ub);
  const char *uU = arc_s(p, "UPPER_UOM", uu, sizeof uu);
  const char *uC = arc_s(p, "UPPER_CODE", uc, sizeof uc);
  char lnum[32] = {0}, unum[32] = {0};
  if (!lv) { double d = arc_n(p, "LOWER_VAL"); if (d) { snprintf(lnum, sizeof lnum, "%.0f", d); lv = lnum; } }
  if (!uv) { double d = arc_n(p, "UPPER_VAL"); if (d) { snprintf(unum, sizeof unum, "%.0f", d); uv = unum; } }
  if (!lv && !uv) { out[0] = 0; return; }
  snprintf(out, n, "%s%s%s%s%s%s%s%s%s%s",
           lv ? lv : "", (lv && lU) ? " " : "", lU ? lU : "",
           (lv && lC) ? " " : "", lC ? lC : "",
           (lv && uv) ? " – " : "",
           uv ? uv : "", (uv && uU) ? " " : "", uU ? uU : "",
           uC ? uC : "");
}

/* ============================================================ decorators == */

static void dec_uasfm(cJSON *p) {
  char an[128], ai[16], un[16];
  const char *name = arc_s(p, "APT1_NAME", an, sizeof an);
  const char *icao = arc_s(p, "APT1_ICAO", ai, sizeof ai);
  const char *unit = arc_s(p, "UNIT", un, sizeof un);
  double ceil_ft = arc_n(p, "CEILING");
  char t[256];
  snprintf(t, sizeof t, "UAS ceiling %.0f %s%s%s%s%s", ceil_ft,
           unit ? unit : "ft", (icao || name) ? " — " : "",
           icao ? icao : "", (icao && name) ? " " : "", name ? name : "");
  arc_set(p, "title", t);
  arc_common(p, "GLOBALID", "uas-facility-map-cell", "drone");
}

static void dec_sua(cJSON *p) {
  char nb[128], tb[16], cb[96], sb[16];
  const char *name = arc_s(p, "NAME", nb, sizeof nb);
  const char *type = arc_s(p, "TYPE_CODE", tb, sizeof tb);
  const char *city = arc_s(p, "CITY", cb, sizeof cb);
  const char *st   = arc_s(p, "STATE", sb, sizeof sb);
  char lim[96]; arc_limits(lim, sizeof lim, p);
  char t[320];
  snprintf(t, sizeof t, "%s%s%s%s%s%s%s%s%s",
           name ? name : "Special use airspace",
           type ? " (" : "", type ? type : "", type ? ")" : "",
           lim[0] ? " · " : "", lim,
           city ? " · " : "", city ? city : "", st ? st : "");
  arc_set(p, "title", t);
  arc_common(p, "GLOBAL_ID", "special-use-airspace", "airspace");
}

static void dec_prohibited(cJSON *p) {
  char nb[128], cb[128], sb[16];
  const char *name = arc_s(p, "NAME", nb, sizeof nb);
  const char *city = arc_s(p, "CITY", cb, sizeof cb);
  const char *st   = arc_s(p, "STATE", sb, sizeof sb);
  char lim[96]; arc_limits(lim, sizeof lim, p);
  char t[320];
  snprintf(t, sizeof t, "Prohibited area %s%s%s%s%s",
           name ? name : "", city ? " — " : "", city ? city : "",
           st ? ", " : "", st ? st : "");
  arc_set(p, "title", t);
  if (lim[0]) arc_set(p, "vertical_limits", lim);
  arc_common(p, "GLOBAL_ID", "prohibited-area", "airspace");
}

static void dec_ndtfr(cJSON *p) {
  char nb[160], cb[96], sb[16], wb[96];
  const char *name = arc_s(p, "NAME", nb, sizeof nb);
  const char *city = arc_s(p, "CITY", cb, sizeof cb);
  const char *st   = arc_s(p, "STATE", sb, sizeof sb);
  const char *wk   = arc_s(p, "WKHR_RMK", wb, sizeof wb);
  char t[320];
  snprintf(t, sizeof t, "%s%s%s%s%s",
           name ? name : "National defense airspace TFR",
           city ? " — " : "", city ? city : "", st ? ", " : "", st ? st : "");
  arc_set(p, "title", t);
  if (wk) arc_set(p, "activation", wk);
  arc_common(p, "GLOBAL_ID", "national-defense-tfr", "airspace-restriction");
}

static void dec_mtr(cJSON *p) {
  char ib[32], nb[96], rb[16];
  const char *ident = arc_s(p, "IDENT", ib, sizeof ib);
  const char *name  = arc_s(p, "NAME", nb, sizeof nb);
  const char *rt    = arc_s(p, "ROUTETYPE", rb, sizeof rb);
  char lim[96]; arc_limits(lim, sizeof lim, p);
  char t[288];
  /* LOWER_CODE HEI == AGL, ALT == MSL — the codes are emitted verbatim in the
   * attributes; the title just carries the assembled band. */
  snprintf(t, sizeof t, "Military training route %s%s%s%s%s%s%s",
           ident ? ident : "", rt ? " (type " : "", rt ? rt : "",
           rt ? ")" : "", lim[0] ? " · " : "", lim, name ? name : "");
  arc_set(p, "title", t);
  arc_common(p, "GLOBAL_ID", "military-training-route", "military");
}

static void dec_dof(cJSON *p) {
  char ob[32], tb[32], cb[96], sb[16], vb[8];
  const char *oas  = arc_s(p, "OAS_Number", ob, sizeof ob);
  const char *type = arc_s(p, "Type_Code", tb, sizeof tb);
  const char *city = arc_s(p, "City", cb, sizeof cb);
  const char *st   = arc_s(p, "State", sb, sizeof sb);
  const char *ver  = arc_s(p, "Verified", vb, sizeof vb);
  double agl = arc_n(p, "AGL");
  char t[288];
  snprintf(t, sizeof t, "%s%s%.0f ft AGL%s%s%s%s", type ? type : "Obstruction",
           " · ", agl, city ? " · " : "", city ? city : "",
           st ? ", " : "", st ? st : "");
  arc_set(p, "title", t);
  /* `Verified` O/U is the survey-confidence flag — kept, not interpreted. */
  if (ver) arc_set(p, "survey_verified", ver);
  if (oas) arc_set(p, "uid", oas);
  arc_set(p, "record_type", "vertical-obstruction");
  arc_set(p, "source", "FAA Digital Obstacle File");
  arc_tags(p, "obstacle", NULL);
}

static void dec_recsite(cJSON *p) {
  char nb[160], cb[96], sb[16], ub[16];
  const char *name = arc_s(p, "SITE_NAME", nb, sizeof nb);
  const char *city = arc_s(p, "CITY", cb, sizeof cb);
  const char *st   = arc_s(p, "STATE", sb, sizeof sb);
  const char *unit = arc_s(p, "UNIT", ub, sizeof ub);
  double ceil_ft = arc_n(p, "CEILING");
  char t[288];
  snprintf(t, sizeof t, "%s%s%s%s%s · ceiling %.0f %s",
           name ? name : "Recreational flyer fixed site",
           city ? " — " : "", city ? city : "", st ? ", " : "", st ? st : "",
           ceil_ft, unit ? unit : "ft");
  arc_set(p, "title", t);
  /* The published site EXTENT is the geometry, not a point. */
  arc_set(p, "geo_precision", "site-boundary");
  arc_common(p, "SITE_ID", "recreational-flyer-site", "drone");
}

static void dec_fria(cJSON *p) {
  char tb[192], ob[192], cb[96], sb[16], rb[64];
  const char *title = arc_s(p, "title", tb, sizeof tb);
  const char *org   = arc_s(p, "orgName", ob, sizeof ob);
  const char *city  = arc_s(p, "city", cb, sizeof cb);
  const char *st    = arc_s(p, "state", sb, sizeof sb);
  const char *ref   = arc_s(p, "refNumber", rb, sizeof rb);
  char t[352];
  snprintf(t, sizeof t, "FRIA: %s%s%s%s%s%s%s",
           title ? title : (ref ? ref : "Recognized Identification Area"),
           org ? " · " : "", org ? org : "",
           city ? " — " : "", city ? city : "", st ? ", " : "", st ? st : "");
  arc_set(p, "title", t);
  arc_ms_iso(p, "startDate", "valid_from");     /* epoch MILLISECONDS */
  arc_ms_iso(p, "endDate", "valid_until");
  /* address1/city are the SPONSOR's postal address; the polygon is the
   * authoritative location and the two can differ, so nothing is geocoded. */
  arc_set(p, "geo_precision", "approved-boundary");
  arc_common(p, "refNumber", "fria", "drone");
}

static void dec_class(cJSON *p) {
  char nb[160], ib[16], lb[32], cb[96], sb[16];
  const char *name = arc_s(p, "NAME", nb, sizeof nb);
  const char *icao = arc_s(p, "ICAO_ID", ib, sizeof ib);
  const char *lt   = arc_s(p, "LOCAL_TYPE", lb, sizeof lb);
  const char *city = arc_s(p, "CITY", cb, sizeof cb);
  const char *st   = arc_s(p, "STATE", sb, sizeof sb);
  char lim[96]; arc_limits(lim, sizeof lim, p);
  char t[352];
  snprintf(t, sizeof t, "%s%s%s%s%s%s%s%s%s%s",
           name ? name : "Controlled airspace",
           lt ? " (" : "", lt ? lt : "", lt ? ")" : "",
           icao ? " · " : "", icao ? icao : "",
           lim[0] ? " · " : "", lim,
           city ? " · " : "", city ? city : "");
  arc_set(p, "title", t);
  if (st) arc_set(p, "state_code", st);
  arc_common(p, "GLOBAL_ID", "controlled-airspace", "airspace");
}

static void dec_navaid(cJSON *p) {
  char ib[16], nb[160], cb[32], sb[16], ct[96], stt[16];
  const char *ident = arc_s(p, "IDENT", ib, sizeof ib);
  const char *name  = arc_s(p, "NAME_TXT", nb, sizeof nb);
  const char *cls   = arc_s(p, "CLASS_TXT", cb, sizeof cb);
  const char *stat  = arc_s(p, "STATUS", sb, sizeof sb);
  const char *city  = arc_s(p, "CITY", ct, sizeof ct);
  const char *st    = arc_s(p, "STATE", stt, sizeof stt);
  char t[352];
  snprintf(t, sizeof t, "%s%s%s%s%s%s%s%s%s%s", ident ? ident : "NAVAID",
           name ? " " : "", name ? name : "",
           cls ? " (" : "", cls ? cls : "", cls ? ")" : "",
           city ? " — " : "", city ? city : "", st ? ", " : "", st ? st : "");
  arc_set(p, "title", t);
  /* STATUS distinguishes IFR/VFR/decommissioned — surfaced, never assumed. */
  if (stat) arc_set(p, "operational_status", stat);
  arc_common(p, "GLOBAL_ID", "navaid", "navaid");
}

static void dec_adhp(cJSON *p) {
  char ib[16], nb[160], cb[16], tb[16], sc[96];
  const char *ident = arc_s(p, "IDENT_TXT", ib, sizeof ib);
  const char *name  = arc_s(p, "NAME_TXT", nb, sizeof nb);
  const char *icao  = arc_s(p, "ICAO_TXT", cb, sizeof cb);
  const char *type  = arc_s(p, "TYPE_CODE", tb, sizeof tb);
  const char *city  = arc_s(p, "SERVICINGCITY_TXT", sc, sizeof sc);
  char t[352];
  snprintf(t, sizeof t, "%s%s%s%s%s%s%s%s", ident ? ident : "",
           icao ? " / " : "", icao ? icao : "",
           name ? " — " : "", name ? name : "",
           type ? " (" : "", type ? type : "", type ? ")" : "");
  arc_set(p, "title", t);
  if (city) arc_set(p, "servicing_city", city);
  arc_ms_iso(p, "LASTMOD_DATE", "last_modified");   /* epoch MILLISECONDS */
  arc_ms_iso(p, "EFFECTIVE_DATE", "effective");
  arc_common(p, "GFID", "landing-facility", "airport");
}

static void dec_runway(cJSON *p) {
  char kb[96], db[32];
  const char *key = arc_s(p, "CLIENTKEY_ID", kb, sizeof kb);
  const char *src = arc_s(p, "DATASOURCE_TXT", db, sizeof db);
  /* CLIENTKEY_ID embeds the airport key and the runway designator, e.g.
   * "50009.*A_AK_05/23" = airport key "50009.*A_AK", runway "05/23". Parse it
   * rather than inventing a name. */
  char apt[96] = {0}, rwy[32] = {0};
  if (key) {
    const char *us = strrchr(key, '_');
    if (us && us[1]) {
      size_t alen = (size_t)(us - key);
      if (alen >= sizeof apt) alen = sizeof apt - 1;
      memcpy(apt, key, alen); apt[alen] = 0;
      snprintf(rwy, sizeof rwy, "%.31s", us + 1);
    }
  }
  char t[224];
  snprintf(t, sizeof t, "Runway %s%s%s", rwy[0] ? rwy : (key ? key : "?"),
           apt[0] ? " · airport key " : "", apt[0] ? apt : "");
  arc_set(p, "title", t);
  if (rwy[0]) arc_set(p, "runway_designator", rwy);
  if (apt[0]) arc_set(p, "airport_key", apt);
  if (src) arc_set(p, "data_source", src);
  arc_ms_iso(p, "EFFECTIVE_DATE", "effective");     /* epoch MILLISECONDS */
  arc_ms_iso(p, "LASTMOD_DATE", "last_modified");
  arc_common(p, "GFID", "runway-centreline", "airport");
}

static void dec_stadium(cJSON *p) {
  char nb[160], cb[96], sb[16], st[32];
  const char *name = arc_s(p, "NAME", nb, sizeof nb);
  const char *city = arc_s(p, "CITY", cb, sizeof cb);
  const char *state= arc_s(p, "STATE", sb, sizeof sb);
  const char *stat = arc_s(p, "STATUS_CODE", st, sizeof st);
  char t[288];
  snprintf(t, sizeof t, "%s%s%s%s%s%s%s", name ? name : "Stadium",
           city ? " — " : "", city ? city : "", state ? ", " : "",
           state ? state : "", stat ? " · " : "", stat ? stat : "");
  arc_set(p, "title", t);
  /* STATUS_CODE 'Open' vs a planned venue with a non-null OPENING_ON — the
   * flag is surfaced so an unbuilt stadium is never presented as active. */
  if (stat) arc_set(p, "venue_status", stat);
  arc_common(p, "GLOBAL_ID", "stadium-flight-restriction", "airspace-restriction");
}

static void dec_nsufr(cJSON *p) {
  char fb[160], bb[128], rb[96], sb[16], cb[96], al[32];
  const char *fac  = arc_s(p, "Facility", fb, sizeof fb);
  const char *base = arc_s(p, "Base", bb, sizeof bb);
  const char *rsn  = arc_s(p, "Reason", rb, sizeof rb);
  const char *st   = arc_s(p, "State", sb, sizeof sb);
  const char *cty  = arc_s(p, "County", cb, sizeof cb);
  const char *alt  = arc_s(p, "ALERTYPE", al, sizeof al);
  char t[352];
  snprintf(t, sizeof t, "Part-time NSUFR: %s%s%s%s%s%s%s",
           fac ? fac : (base ? base : "national security site"),
           rsn ? " — " : "", rsn ? rsn : "",
           cty ? " · " : "", cty ? cty : "", st ? ", " : "", st ? st : "");
  arc_set(p, "title", t);
  /* The activation SCHEDULE is not in this layer: ALERTYPE/ALERTTIME describe
   * the alerting regime, not a currently-in-force restriction. */
  if (alt) arc_set(p, "alert_type", alt);
  arc_ms_iso(p, "ALERTTIME", "alert_time");
  arc_ms_iso(p, "ACTIVETIME", "active_time");
  arc_ms_iso(p, "ENDTIME", "end_time");
  arc_common(p, "FAA_ID", "part-time-nsufr", "drone");
}

/* ================================================================= runs == */
#define AV_ARC_RUN(fn, svc, cap, dec, tag)                                    \
  static int fn(const source_ctx *ctx, intel_sink *sink) {                    \
    int n = av_arcgis_run(ctx, sink, svc, cap, dec, tag);                     \
    return n < 0 ? -1 : 0;                                                    \
  }

AV_ARC_RUN(run_uasfm,      "FAA_UAS_FacilityMap_Data_V5", AV_CAP_HUGE,  dec_uasfm,      "faa-uas-facility-map")
AV_ARC_RUN(run_sua,        "Special_Use_Airspace",        AV_CAP_SMALL, dec_sua,        "faa-special-use-airspace")
AV_ARC_RUN(run_prohibited, "Prohibited_Areas",            AV_CAP_SMALL, dec_prohibited, "faa-prohibited-areas")
AV_ARC_RUN(run_ndtfr,      "National_Defense_Airspace_TFR_Areas", AV_CAP_SMALL, dec_ndtfr, "faa-national-defense-tfr")
AV_ARC_RUN(run_mtr,        "MTRSegment",                  AV_CAP_SMALL, dec_mtr,        "faa-military-training-routes")
AV_ARC_RUN(run_dof,        "Digital_Obstacle_File",       AV_CAP_HUGE,  dec_dof,        "faa-obstacles-dof")
AV_ARC_RUN(run_recsite,    "Recreational_Flyer_Fixed_Sites", AV_CAP_SMALL, dec_recsite, "faa-recreational-flyer-sites")
AV_ARC_RUN(run_fria,       "FAA_Recognized_Identification_Areas", AV_CAP_SMALL, dec_fria, "faa-fria")
AV_ARC_RUN(run_class,      "Class_Airspace",              AV_CAP_SMALL, dec_class,      "faa-class-airspace")
AV_ARC_RUN(run_navaid,     "NAVAIDSystem",                AV_CAP_SMALL, dec_navaid,     "faa-navaids")
AV_ARC_RUN(run_adhp,       "ADHP",                        AV_CAP_LARGE, dec_adhp,       "faa-airports-heliports")
AV_ARC_RUN(run_runway,     "RunwayLine",                  AV_CAP_LARGE, dec_runway,     "faa-runway-lines")
AV_ARC_RUN(run_stadium,    "Stadiums",                    AV_CAP_SMALL, dec_stadium,    "faa-stadium-sites")
AV_ARC_RUN(run_nsufr,      "Part_Time_National_Security_UAS_Flight_Restrictions", AV_CAP_SMALL, dec_nsufr, "faa-part-time-nsufr")

#define AV_ARC_DEF(sym, ID, NM, IVL, FN, DESC)                                \
  static const source_def sym = {                                             \
    .id = ID, .collector = "transport", .name = NM,                           \
    .update_interval_sec = IVL, .run = FN,                                    \
    .category = "transport", .type = "dataset",                               \
    .url = AV_ARC_BASE, .description = DESC,                                  \
    .license = FAA_ARC_LICENSE, .layer = NULL, .free_tier = 1 };              \
  REGISTER_SOURCE(sym)

AV_ARC_DEF(av_faa_uasfm_def, "faa-uas-facility-map",
  "FAA UAS Facility Map (LAANC drone altitude grid)", 604800, run_uasfm,
  "The authoritative grid of maximum drone altitudes the FAA will auto-authorise near every controlled airport in the US — one polygon per cell with its ceiling in feet and the airport(s) it belongs to. Paged; a documented per-run cap applies to this 370k-feature layer and is logged when reached.")

AV_ARC_DEF(av_faa_sua_def, "faa-special-use-airspace",
  "FAA Special Use Airspace", 604800, run_sua,
  "All US special use airspace as polygons with vertical limits: restricted areas, warning areas, alert areas, MOAs and national security areas — the layer that explains why military traffic clusters where it does.")

AV_ARC_DEF(av_faa_prohibited_def, "faa-prohibited-areas",
  "FAA Prohibited Areas", 604800, run_prohibited,
  "The airspace volumes over which flight is flatly prohibited in the US — the White House, Camp David, Mount Vernon and similar. Tiny layer, high analytic weight.")

AV_ARC_DEF(av_faa_ndtfr_def, "faa-national-defense-tfr",
  "FAA National Defense Airspace TFR Areas", 3600, run_ndtfr,
  "Polygon geometry for the standing national-defense temporary flight restrictions — the ones enforceable by military interception. Supplies the geometry the plain TFR list endpoint lacks.")

AV_ARC_DEF(av_faa_mtr_def, "faa-military-training-routes",
  "FAA Military Training Route Segments (IR/VR)", 604800, run_mtr,
  "Every published US military training route segment as a centreline with corridor half-widths and floor/ceiling — directly useful when correlating a military ADS-B contact with a published route.")

AV_ARC_DEF(av_faa_dof_def, "faa-obstacles-dof",
  "FAA Digital Obstacle File", 604800, run_dof,
  "The FAA obstruction database — towers, rigs, stacks, cranes and buildings with verified position, AGL/AMSL height, lighting and marking status. Paged; a documented per-run cap applies to this 641k-feature layer and is logged when reached.")

AV_ARC_DEF(av_faa_recsite_def, "faa-recreational-flyer-sites",
  "FAA Recreational Drone Flying Fixed Sites", 604800, run_recsite,
  "FAA-approved fixed sites where recreational drone flight is permitted inside controlled airspace, as published site boundaries with their altitude ceiling.")

AV_ARC_DEF(av_faa_fria_def, "faa-fria",
  "FAA Recognized Identification Areas (Remote-ID exempt)", 604800, run_fria,
  "The only places a drone may legally fly WITHOUT broadcasting Remote ID, each with sponsoring organisation and validity window — doubling as a directory of organised UAS activity sites.")

AV_ARC_DEF(av_faa_class_def, "faa-class-airspace",
  "FAA Class B/C/D/E Controlled Airspace", 604800, run_class,
  "Every US controlled-airspace class boundary as a polygon, letting any observed aircraft position be classified by the airspace it is inside.")

AV_ARC_DEF(av_faa_navaid_def, "faa-navaids",
  "FAA NAVAID Systems (VOR/DME/TACAN/NDB)", 604800, run_navaid,
  "US radio navigation aids with identifier, class, channel, operational status and free-text remarks including outage and unusable-sector notes. Carries FAA status, unlike the global OurAirports navaid file.")

AV_ARC_DEF(av_faa_adhp_def, "faa-airports-heliports",
  "FAA Airports, Heliports and Landing Facilities (ADHP)", 604800, run_adhp,
  "The full FAA landing-facility inventory — public and private airports, heliports, seaplane bases and ultralight strips with type, elevation, servicing city and private-use flag.")

AV_ARC_DEF(av_faa_runway_def, "faa-runway-lines",
  "FAA Runway Centrelines", 604800, run_runway,
  "Runway centrelines as real threshold-to-threshold geometry for US runways — what you need to measure orientation, length and approach corridors rather than just a runway name.")

AV_ARC_DEF(av_faa_stadium_def, "faa-stadium-sites",
  "FAA Stadiums Subject to Standing Flight Restrictions", 604800, run_stadium,
  "The FAA stadium list that drives the standing 3 NM / 3000 ft sporting-event TFR — both an airspace-restriction layer and a geocoded venue index.")

AV_ARC_DEF(av_faa_nsufr_def, "faa-part-time-nsufr",
  "FAA Part-Time National Security UAS Flight Restrictions", 86400, run_nsufr,
  "Airspace over specific national-security facilities where drone flight is banned only during declared periods — a direct indicator of sensitive sites with episodic UAS restrictions.")
