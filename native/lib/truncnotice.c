/* lib/truncnotice.c — see truncnotice.h. */
#include "truncnotice.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

void trunc_notice(intel_sink *sink, const char *source_id, const char *endpoint,
                  const char *query, long used, long available,
                  const char *reason, const char *remedy) {
  if (!sink || !sink->emit || !source_id) return;

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "source_id", source_id);
  if (endpoint) cJSON_AddStringToObject(p, "endpoint", endpoint);
  if (query)    cJSON_AddStringToObject(p, "query", query);
  cJSON_AddNumberToObject(p, "records_used", (double)used);
  if (available >= 0) {
    cJSON_AddNumberToObject(p, "records_available", (double)available);
    cJSON_AddBoolToObject(p, "more_pending", available > used);
  } else {
    /* Saying "unknown" is the honest answer. Substituting `used` here would
     * make an unread remainder look like no remainder at all. */
    cJSON_AddStringToObject(p, "records_available",
                            "unknown — upstream declared no total");
    cJSON_AddBoolToObject(p, "more_pending", 1);
  }
  if (reason) cJSON_AddStringToObject(p, "reason", reason);
  if (remedy) cJSON_AddStringToObject(p, "remedy", remedy);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char key[320], title[256];
  snprintf(key, sizeof key, "%.150s|truncation:%.120s", source_id,
           query ? query : "");
  if (available >= 0)
    snprintf(title, sizeof title, "%s used %ld of %ld available records",
             source_id, used, available);
  else
    snprintf(title, sizeof title,
             "%s used %ld records and stopped before the end", source_id, used);

  intel_item note = {0};
  note.remote_key      = key;
  note.title           = title;
  note.lang            = "en";
  note.record_type     = "collector-truncation-notice";
  note.properties_json = pj ? pj : "{}";
  note.tags_json       = "[\"osint-search\",\"truncation-notice\"]";
  sink->emit(sink, &note);
  free(pj);
}
