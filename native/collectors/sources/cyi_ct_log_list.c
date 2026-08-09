/* collectors/sources/cyi_ct_log_list.c
 * Certificate Transparency log list trusted by Chrome.
 * Endpoint: https://www.gstatic.com/ct/log_list/v3/log_list.json  (keyless)
 * Emits, per CT log, only values parsed out of that document: operator name,
 * log description, log_id, submission url, mmd, state key (usable/qualified/
 * retired/readonly/pending) + its timestamp, and the temporal_interval shard
 * bounds. Also carries the list version/timestamp onto every row.
 * No coordinates anywhere upstream -> has_geo stays 0 (R2).
 * Licence: published by Google for public consumption, no stated restriction;
 * attribute "Chrome CT log list".
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://www.gstatic.com/ct/log_list/v3/log_list.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 25000);
  if (!doc) { fprintf(stderr, "[ct-log-list] fetch/parse failed\n"); return -1; }

  const char *version = jo_sv(doc, "version");
  const char *listts  = jo_sv(doc, "log_list_timestamp");

  int n = 0;
  const cJSON *ops = cJSON_GetObjectItem(doc, "operators");
  const cJSON *op;
  cJSON_ArrayForEach(op, ops) {
    const char *opname = jo_sv(op, "name");
    const cJSON *logs = cJSON_GetObjectItem(op, "logs");
    const cJSON *lg;
    cJSON_ArrayForEach(lg, logs) {
      const char *desc  = jo_sv(lg, "description");
      const char *logid = jo_sv(lg, "log_id");
      const char *url   = jo_sv(lg, "url");
      if (!logid || !url) continue;            /* no fetched identity -> no row */

      /* "state is an object with exactly one key (usable/qualified/retired/
       * readonly/pending) whose value carries a timestamp" — read the key
       * name, never assume which one is present. */
      const char *state = NULL, *state_at = NULL;
      const cJSON *st = cJSON_GetObjectItem(lg, "state");
      if (cJSON_IsObject(st) && st->child) {
        state = st->child->string;
        state_at = jo_sv(st->child, "timestamp");
      }
      const char *t_start = NULL, *t_end = NULL;
      const cJSON *ti = cJSON_GetObjectItem(lg, "temporal_interval");
      if (cJSON_IsObject(ti)) {
        t_start = jo_sv(ti, "start_inclusive");
        t_end   = jo_sv(ti, "end_exclusive");
      }
      const cJSON *mmd = cJSON_GetObjectItem(lg, "mmd");

      cJSON *p = cJSON_CreateObject();
      if (opname) cJSON_AddStringToObject(p, "operator", opname);
      if (desc)   cJSON_AddStringToObject(p, "description", desc);
      cJSON_AddStringToObject(p, "log_id", logid);
      cJSON_AddStringToObject(p, "log_url", url);
      if (state)    cJSON_AddStringToObject(p, "state", state);
      if (state_at) cJSON_AddStringToObject(p, "state_since", state_at);
      if (t_start)  cJSON_AddStringToObject(p, "temporal_start", t_start);
      if (t_end)    cJSON_AddStringToObject(p, "temporal_end", t_end);
      if (cJSON_IsNumber(mmd)) cJSON_AddNumberToObject(p, "mmd_sec", mmd->valuedouble);
      if (version) cJSON_AddStringToObject(p, "list_version", version);
      if (listts)  cJSON_AddStringToObject(p, "list_timestamp", listts);
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char summary[512];
      snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
               opname ? opname : "", opname ? " · " : "",
               state ? state : "state n/a",
               t_start ? " · shard " : "", t_start ? t_start : "",
               t_end ? "..." : "", t_end ? t_end : "");

      intel_item it = {0};
      it.remote_key      = logid;
      it.title           = desc ? desc : url;
      it.summary         = summary;
      it.link            = url;
      it.lang            = "en";
      it.published_at    = state_at;
      it.record_type     = "ct-log";
      it.properties_json = pj;
      it.tags_json       = "[\"cyber\",\"certificate-transparency\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
    }
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[ct-log-list] emitted %d\n", n);
  return 0;                                    /* fetched fine (R3) */
}

static const source_def cyi_ct_log_list_def = {
  .id = "ct-log-list", .collector = "cyber",
  .name = "Certificate Transparency log list (Google v3)",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Authoritative inventory of every CT log trusted by Chrome: operator, log id, submission URL, usable/retired state and temporal shard.",
  .license = "Google Chrome CT log list — public, attribute Chrome CT log list.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_ct_log_list_def)
