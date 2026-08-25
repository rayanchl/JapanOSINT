/* collectors/sources/_timefmt.inc — the ONE epoch/clock → text formatter for
 * the collector tree.
 *
 * WHY THIS FILE EXISTS. Formatting a time was rewritten, by hand, in about
 * fifteen named helpers (`iso_now`, `iso_of`, `iso_ms`, `epoch_iso`, `soc_iso`,
 * `iso_from_epoch`, `av_epoch_iso`, `tp_today`, …) plus several dozen inline
 * copies, and they had drifted apart on the one thing that matters:
 *
 *     time_t t = (time_t)rec->valuedouble;   // an UPSTREAM number
 *     struct tm g; gmtime_r(&t, &g);         // return IGNORED
 *     strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &g);   // return IGNORED
 *     cJSON_AddStringToObject(p, "published_at", iso);       // stack garbage
 *
 * Both calls can fail and both failures are silent:
 *
 *   - `gmtime_r()` RETURNS NULL when the value cannot be represented (its year
 *     would overflow an int). The `struct tm` is then INDETERMINATE, and the
 *     caller formats whatever was on the stack into a date.
 *   - `strftime()` RETURNS 0 when the result does not fit, and in that case it
 *     leaves the buffer contents UNSPECIFIED — it does NOT write a NUL. A
 *     caller that ignores the 0 reads an unterminated or stale buffer.
 *
 * A timestamp assembled from stack contents is invented data — house rule 1,
 * the same class of defect as a fixture row. It is not theoretical: the epoch
 * is very often a number the UPSTREAM chose (`taken_at`, `created_utc`,
 * `issueDate`, `/Date(…)/`, `first_connected`), so a malformed or hostile feed
 * is all it takes. atlas_jp, diet_records, edinet_filings, houjin_bangou and
 * reddit_jp_subs were each fixed on their own, one incident at a time; this is
 * that fix, once, for everyone else.
 *
 * CONTRACT. Every function here writes an empty string into `out` and returns
 * NULL (or 0) when the time cannot be rendered. There is no path that returns
 * a buffer it did not fully write. Callers therefore degrade honestly — a JSON
 * null, a skipped field, an explicit error — instead of publishing garbage.
 *
 * Included, never compiled standalone; `-iquote collectors/sources` is in
 * CFLAGS so any collector at any depth can `#include "_timefmt.inc"`. All
 * symbols are `static inline`, so each translation unit gets its own copy and
 * an unused one costs nothing (no -Wunused-function for static inline). */
#ifndef JO_TIMEFMT_INC
#define JO_TIMEFMT_INC

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------ broken-down */

/* time_t → UTC struct tm. 1 on success, 0 when the value is unrepresentable
 * (in which case *out is left ZEROED, never indeterminate). */
static inline int jo_tm_utc(time_t t, struct tm *out) {
  if (!out) return 0;
  memset(out, 0, sizeof *out);
#if defined(_WIN32)
  return gmtime_s(out, &t) == 0;
#else
  return gmtime_r(&t, out) != NULL;
#endif
}

/* Same, in the process's local zone. Only correct for sources whose upstream
 * publishes local wall-clock times; prefer jo_tm_utc everywhere else. */
static inline int jo_tm_local(time_t t, struct tm *out) {
  if (!out) return 0;
  memset(out, 0, sizeof *out);
#if defined(_WIN32)
  return localtime_s(out, &t) == 0;
#else
  return localtime_r(&t, out) != NULL;
#endif
}

/* ---------------------------------------------------------------- strftime */

/* time_t + strftime format → `out`. Returns `out`, or NULL with out[0]=0 when
 * either the conversion or the formatting failed. */
static inline const char *jo_time_fmt(time_t t, const char *fmt,
                                      char *out, size_t n) {
  if (!out || n == 0) return NULL;
  out[0] = 0;
  struct tm tmv;
  if (!jo_tm_utc(t, &tmv)) return NULL;
  if (strftime(out, n, fmt, &tmv) == 0) { out[0] = 0; return NULL; }
  return out;
}

/* Local-zone counterpart of jo_time_fmt. */
static inline const char *jo_time_fmt_local(time_t t, const char *fmt,
                                            char *out, size_t n) {
  if (!out || n == 0) return NULL;
  out[0] = 0;
  struct tm tmv;
  if (!jo_tm_local(t, &tmv)) return NULL;
  if (strftime(out, n, fmt, &tmv) == 0) { out[0] = 0; return NULL; }
  return out;
}

/* "now", formatted. */
static inline const char *jo_now_fmt(const char *fmt, char *out, size_t n) {
  return jo_time_fmt(time(NULL), fmt, out, n);
}

/* `back_secs` before now, formatted — the `from=`/`since=` half of every
 * windowed API query. */
static inline const char *jo_ago_fmt(long back_secs, const char *fmt,
                                     char *out, size_t n) {
  return jo_time_fmt(time(NULL) - (time_t)back_secs, fmt, out, n);
}

/* ---------------------------------------------------------------- ISO 8601 */

/* Epoch SECONDS → "YYYY-MM-DDTHH:MM:SSZ". NULL when secs <= 0 (the JS-era
 * sentinel for "no timestamp", and what every hand-written copy tested) or
 * when the value cannot be rendered. */
static inline const char *jo_epoch_iso(double secs, char *out, size_t n) {
  if (!out || n == 0) return NULL;
  out[0] = 0;
  if (!(secs > 0)) return NULL;
  return jo_time_fmt((time_t)secs, "%Y-%m-%dT%H:%M:%SZ", out, n);
}

/* Epoch SECONDS → "YYYY-MM-DDTHH:MM:SS.000Z" — the millisecond-suffixed shape
 * core/intel.c stores and the iOS client parses. Needs n >= 25. */
static inline const char *jo_epoch_iso_ms(double secs, char *out, size_t n) {
  if (!out || n == 0) return NULL;
  out[0] = 0;
  if (!(secs > 0)) return NULL;
  return jo_time_fmt((time_t)secs, "%Y-%m-%dT%H:%M:%S.000Z", out, n);
}

/* Epoch MILLISECONDS → "YYYY-MM-DDTHH:MM:SSZ". Half the upstreams in the tree
 * publish ms (`/Date(1371427200000)/`, `issueDate`, USGS `time`), and dividing
 * by 1000 at each call site was itself a copied line. */
static inline const char *jo_ms_iso(double ms, char *out, size_t n) {
  if (!out || n == 0) return NULL;
  out[0] = 0;
  if (!(ms > 0)) return NULL;
  return jo_time_fmt((time_t)(ms / 1000.0), "%Y-%m-%dT%H:%M:%SZ", out, n);
}

/* Epoch seconds WITH a real fractional part → "…THH:MM:SS.mmmZ". Used where
 * the upstream's own precision is sub-second (Reddit `created_utc`); the
 * carry at .9996 is handled so ".1000Z" can never be emitted. Needs n >= 25. */
static inline const char *jo_epoch_iso_frac(double secs, char *out, size_t n) {
  if (!out || n == 0) return NULL;
  out[0] = 0;
  if (!(secs > 0)) return NULL;
  time_t t = (time_t)(long long)secs;
  int msi = (int)(((secs - (double)(long long)secs) * 1000.0) + 0.5);
  if (msi >= 1000) { msi -= 1000; t += 1; }
  if (msi < 0) msi = 0;
  char base[32];
  if (!jo_time_fmt(t, "%Y-%m-%dT%H:%M:%S", base, sizeof base)) return NULL;
  if ((size_t)snprintf(out, n, "%s.%03dZ", base, msi) >= n) { out[0] = 0; return NULL; }
  return out;
}

/* "now" as ISO-8601 UTC with a .000 millisecond field. Distinct from
 * jocore.h's jo_iso_now(), which reads gettimeofday() and renders the REAL
 * microseconds; use this one where the JS original emitted a literal ".000Z"
 * and the stored value is compared for equality across a run. */
static inline const char *jo_now_iso_ms(char *out, size_t n) {
  return jo_now_fmt("%Y-%m-%dT%H:%M:%S.000Z", out, n);
}

/* "now" as ISO-8601 UTC to the second. */
static inline const char *jo_now_iso(char *out, size_t n) {
  return jo_now_fmt("%Y-%m-%dT%H:%M:%SZ", out, n);
}

#endif /* JO_TIMEFMT_INC */
