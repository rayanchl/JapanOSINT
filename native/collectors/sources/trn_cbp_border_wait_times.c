/* US Customs and Border Protection — live land border crossing wait times.
 * Endpoint: https://bwt.cbp.gov/api/bwtnew
 * Emits: one intel row per crossing — port_number, port_name, crossing_name,
 * which border it is on, port_status, published hours, the feed's own date/time
 * stamp, and the queue delay in minutes plus lanes open for the commercial,
 * passenger and pedestrian standard lanes. Keyless.
 * Licence: US Customs and Border Protection — US Government work, public domain.
 *
 * Parse notes: top-level JSON array. Delay values are STRINGS and include empty
 * strings and "N/A", so each is coerced and simply omitted when it is not a
 * number. There are NO coordinates in this feed — rows are emitted with
 * has_geo = 0 rather than positioned from the port name (R2). The sibling
 * collector bts-border-crossing-volumes carries the real port coordinates and
 * joins on port_number.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

/* One lane group's standard lane: delay minutes + lanes open, both strings. */
static void put_lane(cJSON *pr, const cJSON *row, const char *group,
                     const char *lane, const char *prefix) {
  const cJSON *g = cJSON_GetObjectItem(row, group);
  const cJSON *l = cJSON_GetObjectItem(g, lane);
  if (!l) return;
  char key[96];
  double d;
  if (trn_num(l, "delay_minutes", &d)) {
    snprintf(key, sizeof key, "%s_delay_minutes", prefix);
    cJSON_AddNumberToObject(pr, key, d);
  }
  if (trn_num(l, "lanes_open", &d)) {
    snprintf(key, sizeof key, "%s_lanes_open", prefix);
    cJSON_AddNumberToObject(pr, key, d);
  }
  const char *st = jo_sv(l, "operational_status");
  if (st) {
    snprintf(key, sizeof key, "%s_operational_status", prefix);
    cJSON_AddStringToObject(pr, key, st);
  }
  const char *ut = jo_sv(l, "update_time");
  if (ut) {
    snprintf(key, sizeof key, "%s_update_time", prefix);
    cJSON_AddStringToObject(pr, key, ut);
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://bwt.cbp.gov/api/bwtnew", 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[cbp-border-wait-times] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *p;
  cJSON_ArrayForEach(p, doc) {
    const char *port = jo_sv(p, "port_number");
    const char *pname = jo_sv(p, "port_name");
    const char *cname = jo_sv(p, "crossing_name");
    if (!port && !pname) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "US Customs and Border Protection");
    trn_put_str(pr, "port_number", port);
    trn_put_str(pr, "port_name", pname);
    trn_put_str(pr, "crossing_name", cname);
    trn_put_str(pr, "border", jo_sv(p, "border"));
    trn_put_str(pr, "port_status", jo_sv(p, "port_status"));
    trn_put_str(pr, "hours", jo_sv(p, "hours"));
    trn_put_str(pr, "date", jo_sv(p, "date"));
    trn_put_str(pr, "time", jo_sv(p, "time"));
    trn_put_str(pr, "construction_notice", jo_sv(p, "construction_notice"));
    put_lane(pr, p, "commercial_vehicle_lanes", "standard_lanes", "commercial");
    put_lane(pr, p, "commercial_vehicle_lanes", "FAST_lanes", "commercial_fast");
    put_lane(pr, p, "passenger_vehicle_lanes", "standard_lanes", "passenger");
    put_lane(pr, p, "passenger_vehicle_lanes", "NEXUS_SENTRI_lanes",
             "passenger_trusted");
    put_lane(pr, p, "passenger_vehicle_lanes", "ready_lanes", "passenger_ready");
    put_lane(pr, p, "pedestrian_lanes", "standard_lanes", "pedestrian");
    char *pj = cJSON_PrintUnformatted(pr);

    char title[320], summary[160];
    if (pname && cname) snprintf(title, sizeof title, "%s — %s", pname, cname);
    else snprintf(title, sizeof title, "%s", pname ? pname : port);

    const cJSON *pv = cJSON_GetObjectItem(
        cJSON_GetObjectItem(p, "passenger_vehicle_lanes"), "standard_lanes");
    double delay;
    const char *status = jo_sv(p, "port_status");
    if (pv && trn_num(pv, "delay_minutes", &delay))
      snprintf(summary, sizeof summary, "%s · passenger delay %.0f min",
               status ? status : "", delay);
    else
      snprintf(summary, sizeof summary, "%s", status ? status : "");

    intel_item it = {0};
    it.remote_key      = port ? port : pname;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.link            = "https://bwt.cbp.gov/";
    it.record_type     = "border-crossing-wait";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"border\",\"us\",\"wait-time\"]";
    /* no coordinates in this feed (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[cbp-border-wait-times] emitted %d\n", n);
  return 0;
}

static const source_def trn_cbp_border_wait_times_def = {
  .id = "cbp-border-wait-times", .collector = "transport",
  .name = "US CBP border wait times — all land ports of entry",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://bwt.cbp.gov/api/bwtnew",
  .description = "Live queue delay and lanes-open counts for every US land border crossing on the Canadian and Mexican borders, split by commercial, passenger and pedestrian traffic.",
  .license = "US Customs and Border Protection — US Government work, public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_cbp_border_wait_times_def)
