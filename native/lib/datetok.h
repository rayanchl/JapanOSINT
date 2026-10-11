/* lib/datetok.h -- {date…} tokens, one implementation for every engine.
 *
 * `{date}` is today (YYYY-MM-DD); `{date-1d}`, `{date+2w}`, `{date-3m}`,
 * `{date-1y}` shift it (months clamp: March 31 - 1m = Feb 28/29);
 * `{date-1d:%Y%m%d}` formats it with strftime; `{date:@s}` / `{date:@ms}` is
 * the epoch instant. The engine's first spelling, `{date:FORMAT:±N}` (a day
 * offset after the last ':'), renders identically here, and `{ago:N}` is
 * RFC 3339 UTC of now minus N seconds. Rendered in UTC+tz_min, then stepped back `shift` units
 * of `shift_unit` ('d' 'w' 'm' 'y') -- the date_back walk.
 *
 * Used by lib/hpengine.c (hp rows: url, post_body, headers, detail_url) and by
 * vsrc_url_dates() in collectors/sources/_verified_macros.inc (VJSON / VGEO
 * rows, UTC). Before it existed the VJSON fleet had only {{today-N}} and the
 * hp fleet nothing, so 44 rows carried a date window frozen at authoring. */
#ifndef JO_DATETOK_H
#define JO_DATETOK_H
#include <stddef.h>
#include <time.h>

/* `spec` = token body without braces ("date-1d:%Y%m%d"), `n` its length.
 * now_in = 0 means time(NULL). Returns 1 and fills `out`, 0 when malformed. */
int datetok_render(const char *spec, size_t n, time_t now_in, int tz_min,
                   int shift, char shift_unit, char *out, size_t cap);
/* As datetok_render, and also {ago:<seconds>} (RFC 3339 UTC of now - N).
 * `spec` is the token body without braces. */
int datetok_render_any(const char *spec, size_t n, time_t now_in, int tz_min,
                       int shift, char shift_unit, char *out, size_t cap);
/* Does `s` contain a {date…} token? */
int datetok_present(const char *s);
/* `tmpl` with every {date…} token rendered; malloc'd, or NULL when `tmpl`
 * holds none. A malformed token is left verbatim. */
char *datetok_expand(const char *tmpl, time_t now_in, int tz_min,
                     int shift, char shift_unit);
#endif
