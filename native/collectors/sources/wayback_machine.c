/* collectors/osint/sources/wayback_machine.c
 * OSINT service — WAYBACK_MACHINE. Canonical SERVICE = WAYBACK_MACHINE
 * (dispatcher row {SERVICE_WAYBACK_MACHINE, handle_wayback_machine,
 * "WAYBACK_MACHINE", true}; ARCHIVE_SEARCH aliases the same handler —
 * WAYBACK_MACHINE is the first/dedup name). On-demand (interval 0);
 * ctx->entity = a URL/domain. Internet Archive (archive.org/wayback/available,
 * web.archive.org CDX) — no key. Skips email entities.
 *
 * PER-RECORD EMIT: fetches the CDX snapshot index (web.archive.org/cdx) and
 * emits ONE intel_item per real snapshot, keyed by timestamp+original url.
 * Each row's body carries the real snapshot fields (timestamp/date/mimetype/
 * status/size) and link = the playback wayback URL. Availability is probed for
 * the per-row link fallback. No snapshots / fetch failure → emits nothing
 * (honest empty — no fabricated additional-archive descriptors). */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static cJSON *get_json(http_client *http, const char *url) {
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return j;
}

/* Emit one intel row for a single CDX row. Returns 1 if emitted. */
static int emit_snapshot(intel_sink *sink, const cJSON *row) {
  if (!(row && cJSON_IsArray(row) && cJSON_GetArraySize(row) >= 7)) return 0;
  const cJSON *ts   = cJSON_GetArrayItem(row, 1);
  const cJSON *orig = cJSON_GetArrayItem(row, 2);
  const cJSON *mt   = cJSON_GetArrayItem(row, 3);
  const cJSON *scd  = cJSON_GetArrayItem(row, 4);
  const cJSON *ln   = cJSON_GetArrayItem(row, 6);
  if (!(ts && ts->valuestring && orig && orig->valuestring)) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "timestamp", ts->valuestring);
  char date[32] = {0};
  if (strlen(ts->valuestring) >= 14) {
    snprintf(date, sizeof date, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s",
             ts->valuestring, ts->valuestring + 4, ts->valuestring + 6,
             ts->valuestring + 8, ts->valuestring + 10, ts->valuestring + 12);
    cJSON_AddStringToObject(data, "date", date);
  }
  cJSON_AddStringToObject(data, "original_url", orig->valuestring);
  if (mt  && mt->valuestring)  cJSON_AddStringToObject(data, "mimetype", mt->valuestring);
  if (scd && scd->valuestring) cJSON_AddStringToObject(data, "status_code", scd->valuestring);
  if (ln  && ln->valuestring)  cJSON_AddStringToObject(data, "size_bytes", ln->valuestring);

  char playback[2200];
  snprintf(playback, sizeof playback, "https://web.archive.org/web/%s/%s",
           ts->valuestring, orig->valuestring);
  cJSON_AddStringToObject(data, "url", playback);
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WAYBACK_MACHINE");
  cJSON_AddStringToObject(props, "timestamp", ts->valuestring);
  if (mt && mt->valuestring) cJSON_AddStringToObject(props, "mimetype", mt->valuestring);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[2300];
  snprintf(rk, sizeof rk, "snapshot:%s:%s", ts->valuestring, orig->valuestring);
  char title[2300];
  snprintf(title, sizeof title, "%s — %s", date[0] ? date : ts->valuestring,
           orig->valuestring);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = orig->valuestring;
  it.link            = playback;
  it.published_at    = date[0] ? date : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WAYBACK_MACHINE\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;
  if (strchr(entity, '@')) return 0;   /* skip emails (honest empty) */

  char enc[1024], url[1200];
  jo_uri_encode_buf(entity, enc, sizeof enc);
  snprintf(url, sizeof url,
           "https://web.archive.org/cdx/search/cdx?url=%s&output=json&limit=50",
           enc);
  cJSON *json = get_json(ctx->http, url);
  if (!json) return 0;

  int emitted = 0;
  if (cJSON_IsArray(json)) {
    int n = cJSON_GetArraySize(json);
    /* row 0 is the CDX header — start at 1. */
    for (int i = 1; i < n; i++)
      emitted += emit_snapshot(sink, cJSON_GetArrayItem(json, i));
  }
  cJSON_Delete(json);
  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static const source_def wayback_machine_def = {
  .id = "WAYBACK_MACHINE", .collector = "osint",
  .name = "Wayback Machine", .name_ja = "ウェイバックマシン",
  .update_interval_sec = 0, .run = run,
  .category = "news", .type = "api",
  .url = "internal://osint/wayback-machine",
  .description = "Fetch archived snapshots of a URL (Wayback Machine).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(wayback_machine_def)
