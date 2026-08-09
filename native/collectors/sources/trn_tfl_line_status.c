/* Transport for London — line status for Tube, DLR, Overground, Elizabeth line.
 * Endpoint:
 *   https://api.tfl.gov.uk/Line/Mode/tube,dlr,overground,elizabeth-line/Status
 * Emits: one intel row per line — line id, display name, mode, the numeric
 * statusSeverity and its description, the free-text disruption reason when the
 * line is degraded, and the line's own created/modified timestamps. Keyless.
 * Licence: TfL Open Data — attribution "Powered by TfL Open Data".
 *
 * Parse notes: the response is a top-level ARRAY of lines; the status itself is
 * one level down in lineStatuses[]. There are NO coordinates anywhere in this
 * feed, so every row is emitted with has_geo = 0 rather than pinned to London
 * (R2) — a line is a corridor, not a point, and TfL does not publish one here.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define TFL_LINE_URL \
  "https://api.tfl.gov.uk/Line/Mode/tube,dlr,overground,elizabeth-line/Status"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, TFL_LINE_URL, 20000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[tfl-line-status] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *ln;
  cJSON_ArrayForEach(ln, doc) {
    const char *id   = jo_sv(ln, "id");
    const char *name = jo_sv(ln, "name");
    const char *mode = jo_sv(ln, "modeName");
    if (!id && !name) continue;

    /* worst (highest-impact = lowest severity code) status on the line */
    const cJSON *statuses = cJSON_GetObjectItem(ln, "lineStatuses");
    double worst = 0; int have_worst = 0;
    const char *worst_desc = NULL, *reason = NULL;
    const cJSON *st;
    cJSON_ArrayForEach(st, statuses) {
      double sev;
      if (!trn_num(st, "statusSeverity", &sev)) continue;
      if (!have_worst || sev < worst) {
        worst = sev; have_worst = 1;
        worst_desc = jo_sv(st, "statusSeverityDescription");
        reason = jo_sv(st, "reason");
      }
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Transport for London");
    trn_put_str(pr, "line_id", id);
    trn_put_str(pr, "line_name", name);
    trn_put_str(pr, "mode", mode);
    if (have_worst) cJSON_AddNumberToObject(pr, "status_severity", worst);
    trn_put_str(pr, "status_severity_description", worst_desc);
    trn_put_str(pr, "reason", reason);
    trn_put_str(pr, "created", jo_sv(ln, "created"));
    trn_put_str(pr, "modified", jo_sv(ln, "modified"));
    {
      const cJSON *dis = cJSON_GetObjectItem(ln, "disruptions");
      if (cJSON_IsArray(dis) && cJSON_GetArraySize(dis) > 0)
        cJSON_AddNumberToObject(pr, "disruption_count",
                                cJSON_GetArraySize(dis));
    }
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256];
    snprintf(title, sizeof title, "%s — %s", name ? name : id,
             worst_desc ? worst_desc : "status unknown");

    char link[256];
    snprintf(link, sizeof link, "https://tfl.gov.uk/tube-dlr-overground/status");

    intel_item it = {0};
    it.remote_key      = id ? id : name;
    it.title           = title;
    it.summary         = reason;
    it.body            = reason;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = jo_sv(ln, "modified");
    it.record_type     = "rail-line-status";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"london\",\"disruption\"]";
    /* no coordinates upstream → no geo (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[tfl-line-status] emitted %d\n", n);
  return 0;
}

static const source_def trn_tfl_line_status_def = {
  .id = "tfl-line-status", .collector = "transport",
  .name = "TfL line status — Tube, DLR, Overground, Elizabeth line",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = TFL_LINE_URL,
  .description = "Live service status per London rail line with severity code and the free-text disruption reason — the fastest read on whether London's rail network is functioning.",
  .license = "TfL Open Data — attribution 'Powered by TfL Open Data'.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_tfl_line_status_def)
