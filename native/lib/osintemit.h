/* lib/osintemit.h — the one OSINT emit envelope.
 *
 * The `soc_*` collectors (11 files, aggregators → wikimedia) each carried a
 * byte-identical copy of the same three functions: the emit envelope, the
 * type-preserving property copier and the %-encoder. Not "similar" — the same
 * 60 lines, eleven times.
 *
 * A census of the whole collector tree found 147 emit wrappers across 112
 * files. Most are NOT interchangeable: they differ in which intel_item fields
 * they set (uid vs remote_key, body carried or omitted, has_geo/lat/lon), and
 * changing any of those changes what the source persists. Only the provably
 * identical family lives here. Anything else stays in its collector until
 * someone proves the row it emits is the same row.
 */
#ifndef JO_OSINTEMIT_H
#define JO_OSINTEMIT_H

#include "../source.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Emit one intel row from a built `props` object, which this function TAKES
 * OWNERSHIP of (it is printed, then deleted — the caller must not reuse it).
 *
 * `body` is deliberately the same printed JSON as `properties_json`: these
 * sources have no prose body, and the FTS mirror in core/intel.c indexes
 * `body`, so pointing it at the properties is what makes the row findable.
 * Returns 1 if the sink accepted the row, 0 otherwise — so callers can
 * accumulate a count directly. */
static inline int soc_emit(intel_sink *sink, cJSON *props,
                           const char *record_type, const char *tags,
                           const char *remote_key, const char *title,
                           const char *summary, const char *link,
                           const char *published, const char *lang) {
  if (!props) return 0;
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);
  intel_item it = {0};
  it.remote_key      = remote_key;
  it.title           = title;
  it.summary         = summary;
  it.link            = link;
  it.published_at    = published;
  it.record_type     = record_type;
  it.body            = pj;
  it.properties_json = pj;
  it.tags_json       = tags;
  it.lang            = lang;
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Copy src[sk] → dst[dk] preserving its JSON type (string/number/bool/array).
 * Empty strings and objects are dropped: an empty string is upstream saying
 * "no value", and the object case is always a nested blob nobody reads. */
static inline void jcopy(cJSON *dst, const char *dk,
                         const cJSON *src, const char *sk) {
  const cJSON *v = cJSON_GetObjectItem(src, sk);
  if (!v) return;
  if (cJSON_IsString(v)) {
    if (v->valuestring && v->valuestring[0])
      cJSON_AddStringToObject(dst, dk, v->valuestring);
  } else if (cJSON_IsNumber(v)) {
    cJSON_AddNumberToObject(dst, dk, v->valuedouble);
  } else if (cJSON_IsBool(v)) {
    cJSON_AddBoolToObject(dst, dk, cJSON_IsTrue(v));
  } else if (cJSON_IsArray(v)) {
    cJSON_AddItemToObject(dst, dk, cJSON_Duplicate(v, 1));
  }
}

/* RFC-3986 %-encode, malloc'd (caller frees). Distinct from jo_urlencode by
 * exactly one behaviour: a NULL input yields NULL rather than "", and these
 * call sites branch on that to skip the request entirely. */
static inline char *soc_urlenc(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s);
  char *out = (char *)malloc(n * 3 + 1);
  if (!out) return NULL;
  static const char hx[] = "0123456789ABCDEF";
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      out[j++] = (char)c;
    else { out[j++] = '%'; out[j++] = hx[c >> 4]; out[j++] = hx[c & 15]; }
  }
  out[j] = 0;
  return out;
}

/* ── schema-drift notices, as DATA ───────────────────────────────────────
 *
 * House rule 2: anything a run left unused is reported as a record, not as a
 * log line. The truncation notice (`collector-truncation-notice`) covers the
 * case where we STOPPED early. This covers the case where the upstream's
 * SHAPE stopped matching the row's declaration — a declared array_path that
 * resolves to nothing, title/id keys that match none of the records, a
 * densest-array guess made between several candidates, a page parameter the
 * server ignores. Each of those is a run that "succeeded" while the row was
 * quietly reading the wrong thing; known-issue #54 counted 86 rows in the
 * last of them alone.
 *
 * One record per (source, condition, UTC day): the remote_key is
 * `shape:<condition>:<YYYY-MM-DD>`, so a scheduled row that hits the same
 * condition on every tick updates one disclosure a day instead of piling up
 * duplicates, and yesterday's row stays as history. The sink prefixes the
 * source id itself (core/intel.c derives uid as "<source_id>|<remote_key>").
 *
 * `props` is TAKEN (printed and deleted); the caller puts the NUMBERS in it
 * and in `title`, which doubles as the body so the FTS mirror can find it.
 * Returns 1 if the sink accepted the row, 0 if it refused or notices are
 * silenced. Never called when the condition did not occur — the caller only
 * reaches this with a measured shortfall in hand.
 *
 * JO_SHAPE_NOTICES=0 silences them. It exists for the emit auditors
 * (tools/audit_batch_emit.py, tools/audit_registry_emit.py): they read the
 * run line's `records=` and the row count in intel_items, and one notice would
 * lift a row that stores NOTHING to "emitted 1, stored 1" — hiding exactly
 * the defect the notice describes. Unset (the default) means on: it is data. */
static inline int jo_shape_notices_enabled(void) {
  const char *e = getenv("JO_SHAPE_NOTICES");
  return !(e && e[0] == '0' && e[1] == '\0');
}

static inline int jo_shape_notice(intel_sink *sink, const char *source_id,
                                  const char *condition, const char *title,
                                  cJSON *props, const char *tags) {
  if (!sink || !condition || !jo_shape_notices_enabled()) {
    cJSON_Delete(props);
    return 0;
  }
  char day[16];
  time_t now = time(NULL);
  struct tm tmv;
  if (gmtime_r(&now, &tmv)) strftime(day, sizeof day, "%Y-%m-%d", &tmv);
  else snprintf(day, sizeof day, "unknown-day");
  if (!props) props = cJSON_CreateObject();
  if (props) {
    cJSON_AddStringToObject(props, "source_id", source_id ? source_id : "");
    cJSON_AddStringToObject(props, "condition", condition);
    cJSON_AddStringToObject(props, "run_day", day);
  }
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
  cJSON_Delete(props);
  char key[160];
  snprintf(key, sizeof key, "shape:%.64s:%s", condition, day);
  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.body            = title;
  it.lang            = "en";
  it.record_type     = "collector-shape-notice";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = tags ? tags : "[\"shape-notice\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

#endif /* JO_OSINTEMIT_H */
