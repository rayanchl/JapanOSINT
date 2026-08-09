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

#endif /* JO_OSINTEMIT_H */
