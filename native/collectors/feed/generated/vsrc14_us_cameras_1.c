/* Verified-live us_cameras sources (35), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* Every row in this file fetched perfectly and stored NOTHING, all 35 of them,
 * for one reason: Caltrans wraps each element of its "data" array in a
 * single-key envelope — the array is [{"cctv":{…}}, …], [{"cms":{…}}, …] or
 * [{"lcs":{…}}, …] — and jsonlist's envelope list knows attributes/properties/
 * resource/metas, not these. The OUTER object therefore carries no scalar at
 * all: no title, no id, not even a first-scalar fallback, so every record was
 * dropped as shape noise and the run reported success with 0 rows. Live-
 * verified 2026-09-07: cctvStatusD01 holds 145 records, cmsStatusD01 28,
 * lcsStatusD01 423, and the registry sweep stored none of them.
 *
 * The fix hoists the fields the emitter needs OUT of the envelope and onto the
 * outer record, all of them values the upstream already sent, and leaves the
 * envelope in place so properties_json still carries the complete record:
 *   id           <- <wrap>.index          (145/145, 28/28 and 423/423 distinct
 *                                          on the three files measured)
 *   locationName <- location.locationName, else the lane-closure form
 *                   location.begin.beginLocationName / beginFreeFormDescription
 *   latitude /   <- location.latitude/longitude, else the closure's
 *   longitude       location.begin.beginLatitude/beginLongitude
 *   date         <- recordTimestamp.recordDate (+ recordTime when present)
 * Nothing is invented and nothing is removed. */
typedef struct {
  const char *path, *wrap, *record_type, *lang, *tags_json;
} cal_wrap_opts;

static const char *cal_str(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItemCaseSensitive(o, k) : NULL;
  return (cJSON_IsString(v) && v->valuestring && *v->valuestring)
           ? v->valuestring : NULL;
}
static cJSON *cal_obj(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItemCaseSensitive(o, k) : NULL;
  return cJSON_IsObject(v) ? v : NULL;
}

static int cal_emit_page(const source_ctx *c, intel_sink *s, const char *id,
                         cJSON *doc, void *ud, int *seen) {
  (void)c;
  cal_wrap_opts *o = (cal_wrap_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *in = cal_obj(rec, o->wrap);
      if (!in) continue;
      cJSON *loc = cal_obj(in, "location");
      cJSON *beg = cal_obj(loc, "begin");

      const char *nm = cal_str(loc, "locationName");
      if (!nm) nm = cal_str(beg, "beginLocationName");
      if (!nm) nm = cal_str(beg, "beginFreeFormDescription");

      /* id. `index` is unique in the cctv and lcs files but NOT in cms:
       * measured 2026-09-14, cmsStatusD12 gives all 67 of its signs index "1"
       * (the sweep: 67 emitted, 1 stored), D03 has 108 signs on 107 indexes and
       * D11 80 on 77, while locationName is distinct on all three. For cms the
       * id is index|locationName, both upstream values; cctv and lcs keep the
       * bare index so the uids they already stored do not change. */
      const char *ix = cal_str(in, "index");
      if (ix && !cJSON_GetObjectItemCaseSensitive(rec, "id")) {
        if (nm && o->wrap && strcmp(o->wrap, "cms") == 0) {
          char idb[384];
          snprintf(idb, sizeof idb, "%s|%s", ix, nm);
          cJSON_AddStringToObject(rec, "id", idb);
        } else {
          cJSON_AddStringToObject(rec, "id", ix);
        }
      }
      if (nm && !cJSON_GetObjectItemCaseSensitive(rec, "locationName"))
        cJSON_AddStringToObject(rec, "locationName", nm);

      const char *la = cal_str(loc, "latitude");
      const char *lo = cal_str(loc, "longitude");
      if (!la) la = cal_str(beg, "beginLatitude");
      if (!lo) lo = cal_str(beg, "beginLongitude");
      if (la && lo && !cJSON_GetObjectItemCaseSensitive(rec, "latitude")) {
        cJSON_AddStringToObject(rec, "latitude", la);
        cJSON_AddStringToObject(rec, "longitude", lo);
      }

      cJSON *ts = cal_obj(in, "recordTimestamp");
      const char *rd = cal_str(ts, "recordDate");
      const char *rt = cal_str(ts, "recordTime");
      if (rd && !cJSON_GetObjectItemCaseSensitive(rec, "date")) {
        char when[48];
        snprintf(when, sizeof when, "%s%s%s", rd, rt ? "T" : "", rt ? rt : "");
        cJSON_AddStringToObject(rec, "date", when);
      }
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

/* Same argument list as VJSON with one extra trailing field: the single-key
 * envelope each array element is wrapped in. */
#define VCAL(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, PATH, LANG, TAGS, IVAL, DESC, WRAP) \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    cal_wrap_opts o = { PATH, WRAP, CAT, LANG, TAGS };                        \
    int n = pw_walk(c, s, ID, URL, pw_fetch_json, cal_emit_page, &o);         \
    if (n < 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }     \
    return 0; }                                                               \
  static const source_def SYM = {                                            \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,             \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = CAT, .type = "api", .url = URL,                               \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

VCAL(us_caltrans_cctv_d01, "us-caltrans-cctv-d01", "Caltrans CCTV Cameras — District 01 (Eureka / North Coast)", "カリフォルニア州道路カメラ 第01管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d1/cctv/cctvStatusD01.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d01\"]", 1800,
  "Every Caltrans traffic camera in District 01 (Eureka / North Coast): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d02, "us-caltrans-cctv-d02", "Caltrans CCTV Cameras — District 02 (Redding / Northeast)", "カリフォルニア州道路カメラ 第02管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d2/cctv/cctvStatusD02.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d02\"]", 1800,
  "Every Caltrans traffic camera in District 02 (Redding / Northeast): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d03, "us-caltrans-cctv-d03", "Caltrans CCTV Cameras — District 03 (Marysville / Sacramento Valley)", "カリフォルニア州道路カメラ 第03管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d3/cctv/cctvStatusD03.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d03\"]", 1800,
  "Every Caltrans traffic camera in District 03 (Marysville / Sacramento Valley): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d04, "us-caltrans-cctv-d04", "Caltrans CCTV Cameras — District 04 (Bay Area / Oakland)", "カリフォルニア州道路カメラ 第04管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d4/cctv/cctvStatusD04.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d04\"]", 1800,
  "Every Caltrans traffic camera in District 04 (Bay Area / Oakland): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d05, "us-caltrans-cctv-d05", "Caltrans CCTV Cameras — District 05 (San Luis Obispo / Central Coast)", "カリフォルニア州道路カメラ 第05管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d5/cctv/cctvStatusD05.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d05\"]", 1800,
  "Every Caltrans traffic camera in District 05 (San Luis Obispo / Central Coast): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d06, "us-caltrans-cctv-d06", "Caltrans CCTV Cameras — District 06 (Fresno / South Valley)", "カリフォルニア州道路カメラ 第06管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d6/cctv/cctvStatusD06.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d06\"]", 1800,
  "Every Caltrans traffic camera in District 06 (Fresno / South Valley): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d07, "us-caltrans-cctv-d07", "Caltrans CCTV Cameras — District 07 (Los Angeles / Ventura)", "カリフォルニア州道路カメラ 第07管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d7/cctv/cctvStatusD07.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d07\"]", 1800,
  "Every Caltrans traffic camera in District 07 (Los Angeles / Ventura): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d08, "us-caltrans-cctv-d08", "Caltrans CCTV Cameras — District 08 (San Bernardino / Riverside)", "カリフォルニア州道路カメラ 第08管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d8/cctv/cctvStatusD08.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d08\"]", 1800,
  "Every Caltrans traffic camera in District 08 (San Bernardino / Riverside): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d09, "us-caltrans-cctv-d09", "Caltrans CCTV Cameras — District 09 (Bishop / Eastern Sierra)", "カリフォルニア州道路カメラ 第09管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d9/cctv/cctvStatusD09.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d09\"]", 1800,
  "Every Caltrans traffic camera in District 09 (Bishop / Eastern Sierra): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d10, "us-caltrans-cctv-d10", "Caltrans CCTV Cameras — District 10 (Stockton / Central Valley)", "カリフォルニア州道路カメラ 第10管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d10/cctv/cctvStatusD10.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d10\"]", 1800,
  "Every Caltrans traffic camera in District 10 (Stockton / Central Valley): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d11, "us-caltrans-cctv-d11", "Caltrans CCTV Cameras — District 11 (San Diego / Imperial)", "カリフォルニア州道路カメラ 第11管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d11/cctv/cctvStatusD11.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d11\"]", 1800,
  "Every Caltrans traffic camera in District 11 (San Diego / Imperial): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cctv_d12, "us-caltrans-cctv-d12", "Caltrans CCTV Cameras — District 12 (Orange County)", "カリフォルニア州道路カメラ 第12管区",
  "us_cameras", "cameras",
  "https://cwwp2.dot.ca.gov/data/d12/cctv/cctvStatusD12.json",
  "data",
  "en", "[\"usa\",\"california\",\"camera\",\"cctv\",\"roadway\",\"d12\"]", 1800,
  "Every Caltrans traffic camera in District 12 (Orange County): location name, route and postmile, latitude/longitude, in-service state and the current still-image and stream URLs.",  "cctv");

VCAL(us_caltrans_cms_d01, "us-caltrans-cms-d01", "Caltrans Changeable Message Signs — District 01 (Eureka / North Coast)", "カリフォルニア州可変情報板 第01管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d1/cms/cmsStatusD01.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d01\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 01 (Eureka / North Coast), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d02, "us-caltrans-cms-d02", "Caltrans Changeable Message Signs — District 02 (Redding / Northeast)", "カリフォルニア州可変情報板 第02管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d2/cms/cmsStatusD02.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d02\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 02 (Redding / Northeast), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d03, "us-caltrans-cms-d03", "Caltrans Changeable Message Signs — District 03 (Marysville / Sacramento Valley)", "カリフォルニア州可変情報板 第03管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d3/cms/cmsStatusD03.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d03\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 03 (Marysville / Sacramento Valley), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d04, "us-caltrans-cms-d04", "Caltrans Changeable Message Signs — District 04 (Bay Area / Oakland)", "カリフォルニア州可変情報板 第04管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d4/cms/cmsStatusD04.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d04\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 04 (Bay Area / Oakland), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d05, "us-caltrans-cms-d05", "Caltrans Changeable Message Signs — District 05 (San Luis Obispo / Central Coast)", "カリフォルニア州可変情報板 第05管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d5/cms/cmsStatusD05.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d05\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 05 (San Luis Obispo / Central Coast), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d06, "us-caltrans-cms-d06", "Caltrans Changeable Message Signs — District 06 (Fresno / South Valley)", "カリフォルニア州可変情報板 第06管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d6/cms/cmsStatusD06.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d06\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 06 (Fresno / South Valley), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d07, "us-caltrans-cms-d07", "Caltrans Changeable Message Signs — District 07 (Los Angeles / Ventura)", "カリフォルニア州可変情報板 第07管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d7/cms/cmsStatusD07.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d07\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 07 (Los Angeles / Ventura), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d08, "us-caltrans-cms-d08", "Caltrans Changeable Message Signs — District 08 (San Bernardino / Riverside)", "カリフォルニア州可変情報板 第08管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d8/cms/cmsStatusD08.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d08\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 08 (San Bernardino / Riverside), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d09, "us-caltrans-cms-d09", "Caltrans Changeable Message Signs — District 09 (Bishop / Eastern Sierra)", "カリフォルニア州可変情報板 第09管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d9/cms/cmsStatusD09.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d09\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 09 (Bishop / Eastern Sierra), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d10, "us-caltrans-cms-d10", "Caltrans Changeable Message Signs — District 10 (Stockton / Central Valley)", "カリフォルニア州可変情報板 第10管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d10/cms/cmsStatusD10.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d10\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 10 (Stockton / Central Valley), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d11, "us-caltrans-cms-d11", "Caltrans Changeable Message Signs — District 11 (San Diego / Imperial)", "カリフォルニア州可変情報板 第11管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d11/cms/cmsStatusD11.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d11\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 11 (San Diego / Imperial), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_cms_d12, "us-caltrans-cms-d12", "Caltrans Changeable Message Signs — District 12 (Orange County)", "カリフォルニア州可変情報板 第12管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d12/cms/cmsStatusD12.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"signage\",\"d12\"]", 1800,
  "Live text displayed on every changeable message sign in Caltrans District 12 (Orange County), with sign location and coordinates — a running feed of incidents and closures.",  "cms");

VCAL(us_caltrans_lcs_d01, "us-caltrans-lcs-d01", "Caltrans Lane Closures — District 01 (Eureka / North Coast)", "カリフォルニア州車線規制 第01管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d1/lcs/lcsStatusD01.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d01\"]", 1800,
  "Active and planned lane closures in Caltrans District 01 (Eureka / North Coast): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d02, "us-caltrans-lcs-d02", "Caltrans Lane Closures — District 02 (Redding / Northeast)", "カリフォルニア州車線規制 第02管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d2/lcs/lcsStatusD02.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d02\"]", 1800,
  "Active and planned lane closures in Caltrans District 02 (Redding / Northeast): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d03, "us-caltrans-lcs-d03", "Caltrans Lane Closures — District 03 (Marysville / Sacramento Valley)", "カリフォルニア州車線規制 第03管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d3/lcs/lcsStatusD03.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d03\"]", 1800,
  "Active and planned lane closures in Caltrans District 03 (Marysville / Sacramento Valley): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d04, "us-caltrans-lcs-d04", "Caltrans Lane Closures — District 04 (Bay Area / Oakland)", "カリフォルニア州車線規制 第04管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d4/lcs/lcsStatusD04.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d04\"]", 1800,
  "Active and planned lane closures in Caltrans District 04 (Bay Area / Oakland): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d05, "us-caltrans-lcs-d05", "Caltrans Lane Closures — District 05 (San Luis Obispo / Central Coast)", "カリフォルニア州車線規制 第05管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d5/lcs/lcsStatusD05.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d05\"]", 1800,
  "Active and planned lane closures in Caltrans District 05 (San Luis Obispo / Central Coast): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d06, "us-caltrans-lcs-d06", "Caltrans Lane Closures — District 06 (Fresno / South Valley)", "カリフォルニア州車線規制 第06管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d6/lcs/lcsStatusD06.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d06\"]", 1800,
  "Active and planned lane closures in Caltrans District 06 (Fresno / South Valley): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d08, "us-caltrans-lcs-d08", "Caltrans Lane Closures — District 08 (San Bernardino / Riverside)", "カリフォルニア州車線規制 第08管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d8/lcs/lcsStatusD08.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d08\"]", 1800,
  "Active and planned lane closures in Caltrans District 08 (San Bernardino / Riverside): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d09, "us-caltrans-lcs-d09", "Caltrans Lane Closures — District 09 (Bishop / Eastern Sierra)", "カリフォルニア州車線規制 第09管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d9/lcs/lcsStatusD09.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d09\"]", 1800,
  "Active and planned lane closures in Caltrans District 09 (Bishop / Eastern Sierra): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d10, "us-caltrans-lcs-d10", "Caltrans Lane Closures — District 10 (Stockton / Central Valley)", "カリフォルニア州車線規制 第10管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d10/lcs/lcsStatusD10.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d10\"]", 1800,
  "Active and planned lane closures in Caltrans District 10 (Stockton / Central Valley): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d11, "us-caltrans-lcs-d11", "Caltrans Lane Closures — District 11 (San Diego / Imperial)", "カリフォルニア州車線規制 第11管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d11/lcs/lcsStatusD11.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d11\"]", 1800,
  "Active and planned lane closures in Caltrans District 11 (San Diego / Imperial): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");

VCAL(us_caltrans_lcs_d12, "us-caltrans-lcs-d12", "Caltrans Lane Closures — District 12 (Orange County)", "カリフォルニア州車線規制 第12管区",
  "us_cameras", "transport",
  "https://cwwp2.dot.ca.gov/data/d12/lcs/lcsStatusD12.json",
  "data",
  "en", "[\"usa\",\"california\",\"roadway\",\"closure\",\"d12\"]", 1800,
  "Active and planned lane closures in Caltrans District 12 (Orange County): route, postmile range, work type, schedule and the closure's coordinates.",  "lcs");
