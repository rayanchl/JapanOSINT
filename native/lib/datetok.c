/* lib/datetok.c -- see datetok.h. */
#include "datetok.h"
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Civil-date arithmetic by day number (H. Hinnant's days_from_civil), not
 * timegm(): it is not in C99, and every host must render the same date. */
static long dt_days_from_civil(long y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
static void dt_civil_from_days(long z, long *y, int *m, int *d) {
  z += 719468;
  long era = (z >= 0 ? z : z - 146096) / 146097;
  long doe = z - era * 146097;
  long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  long mp = (5 * doy + 2) / 153;
  *d = (int)(doy - (153 * mp + 2) / 5 + 1);
  *m = (int)(mp < 10 ? mp + 3 : mp - 9);
  *y = yoe + era * 400 + (*m <= 2);
}
static int dt_month_days(long y, int m) {
  static const int D[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return D[m - 1];
}

/* Move day number `days` by `amount` units ('d' 'w' 'm' 'y'). Calendar months
 * clamp to the last day: March 31 minus one month is February 28/29. */
static long dt_shift_days(long days, long amount, char unit) {
  if (unit == 'm' || unit == 'y') {
    long y; int m, d;
    dt_civil_from_days(days, &y, &m, &d);
    long mm = (long)(m - 1) + (unit == 'y' ? amount * 12 : amount);
    y += mm >= 0 ? mm / 12 : (mm - 11) / 12;
    m = (int)(((mm % 12) + 12) % 12) + 1;
    if (d > dt_month_days(y, m)) d = dt_month_days(y, m);
    return dt_days_from_civil(y, m, d);
  }
  return days + (unit == 'w' ? amount * 7 : amount);
}

/* `spec` is the token body without braces: "date", "date-1d", "date+2w:%Y%m%d",
 * "date-3m", "date:@ms". Returns 1 and fills `out`, or 0 when spec is not a
 * well-formed date token (it is then left verbatim, like any unknown token). */
int datetok_render(const char *spec, size_t n, time_t now_in, int tz_min,
                   int shift, char shift_unit, char *out, size_t cap) {
  if (n < 4 || strncmp(spec, "date", 4)) return 0;
  size_t i = 4;
  long amount = 0; char unit = 'd';
  if (i < n && (spec[i] == '+' || spec[i] == '-')) {
    int sign = spec[i++] == '-' ? -1 : 1;
    if (i >= n || !isdigit((unsigned char)spec[i])) return 0;
    while (i < n && isdigit((unsigned char)spec[i])) amount = amount * 10 + (spec[i++] - '0');
    if (i < n && strchr("dwmy", spec[i])) unit = spec[i++];
    amount *= sign;
  }
  char fmt[96] = "%Y-%m-%d";
  long sfx_days = 0;
  if (i < n) {
    if (spec[i] != ':' || n - i - 1 == 0 || n - i - 1 >= sizeof fmt) return 0;
    memcpy(fmt, spec + i + 1, n - i - 1);
    fmt[n - i - 1] = 0;
    /* The second spelling of a day offset, {date:FORMAT:+N} / {date:FORMAT:N},
     * which the engine shipped first (hp_date_token on main): the text after
     * the LAST ':' is a day offset when it is a signed or unsigned integer of
     * 1-5 digits, so a format holding ':' itself (%H:%M) still parses. Both
     * spellings render through this one function. */
    char *colon = strrchr(fmt, ':');
    if (colon) {
      const char *d = colon + 1;
      if (*d == '+' || *d == '-') d++;
      int digits = 0;
      while (isdigit((unsigned char)d[digits])) digits++;
      if (digits > 0 && digits <= 5 && d[digits] == 0) {
        sfx_days = strtol(colon + 1, NULL, 10);
        *colon = 0;
      }
    }
    if (!fmt[0]) return 0;
  }
  time_t now = now_in ? now_in : time(NULL);
  long long t = (long long)now + (long long)tz_min * 60;
  long days = (long)(t >= 0 ? t / 86400 : (t - 86399) / 86400);
  long secs = (long)(t - (long long)days * 86400);
  long y; int m, d;
  days = dt_shift_days(days, amount, unit);
  days += sfx_days;
  if (shift)
    days = dt_shift_days(days, -(long)shift, shift_unit ? shift_unit : 'd');
  dt_civil_from_days(days, &y, &m, &d);
  if (!strcmp(fmt, "@s") || !strcmp(fmt, "@ms")) {
    long long ep = (long long)days * 86400 + secs - (long long)tz_min * 60;
    snprintf(out, cap, "%lld", fmt[2] ? ep * 1000 : ep);
    return 1;
  }
  struct tm tm;
  memset(&tm, 0, sizeof tm);
  tm.tm_year = (int)(y - 1900); tm.tm_mon = m - 1; tm.tm_mday = d;
  tm.tm_hour = (int)(secs / 3600); tm.tm_min = (int)(secs / 60 % 60);
  tm.tm_sec = (int)(secs % 60);
  tm.tm_wday = (int)(((days % 7) + 11) % 7);       /* 1970-01-01 was a Thursday */
  tm.tm_yday = (int)(days - dt_days_from_civil(y, 1, 1));
  if (!strftime(out, cap, fmt, &tm)) return 0;
  return 1;
}

/* {ago:<seconds>}: RFC 3339 UTC of (now - seconds), clamped at 0 — the "since
 * this instant" cursor of an append-only feed (the Go module index). Always
 * UTC: the trailing Z says so, whatever the row's tz_minutes. */
static int datetok_ago(const char *spec, size_t n, time_t now_in, char *out, size_t cap) {
  if (n < 5 || strncmp(spec, "ago:", 4)) return 0;
  char num[24];
  size_t k = n - 4 < sizeof num - 1 ? n - 4 : sizeof num - 1;
  memcpy(num, spec + 4, k); num[k] = 0;
  long secs = strtol(num, NULL, 10);   /* as main: "{ago:x}" reads as 0 = now */
  if (secs < 0) secs = 0;
  time_t t = (now_in ? now_in : time(NULL)) - (time_t)secs;
  struct tm g;
  if (!gmtime_r(&t, &g)) return 0;
  return strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &g) > 0;
}

int datetok_render_any(const char *spec, size_t n, time_t now_in, int tz_min,
                       int shift, char shift_unit, char *out, size_t cap) {
  if (n >= 4 && !strncmp(spec, "ago:", 4)) return datetok_ago(spec, n, now_in, out, cap);
  return datetok_render(spec, n, now_in, tz_min, shift, shift_unit, out, cap);
}

int datetok_present(const char *s) {
  for (const char *p = s ? strstr(s, "{date") : NULL; p; p = strstr(p + 1, "{date"))
    if (strchr("}+-:", p[5])) return 1;
  return 0;
}


char *datetok_expand(const char *tmpl, time_t now_in, int tz_min,
                     int shift, char shift_unit) {
  if (!tmpl || (!datetok_present(tmpl) && !strstr(tmpl, "{ago:"))) return NULL;
  size_t cap = strlen(tmpl) + 256, len = 0;
  char *out = malloc(cap);
  if (!out) return NULL;
  for (const char *p = tmpl; *p; ) {
    char dbuf[128];
    const char *close = (*p == '{' && ((!strncmp(p + 1, "date", 4) && strchr("}+-:", p[5])) ||
                                       !strncmp(p + 1, "ago:", 4)))
                        ? strchr(p, '}') : NULL;
    if (close && close - p < 112 &&
        datetok_render_any(p + 1, (size_t)(close - p - 1), now_in, tz_min, shift,
                           shift_unit, dbuf, sizeof dbuf)) {
      size_t dl = strlen(dbuf);
      if (len + dl + 1 > cap) {
        char *n = realloc(out, cap = (len + dl + 1) * 2);
        if (!n) { free(out); return NULL; }
        out = n;
      }
      memcpy(out + len, dbuf, dl); len += dl;
      p = close + 1;
      continue;
    }
    if (len + 2 > cap) {
      char *n = realloc(out, cap *= 2);
      if (!n) { free(out); return NULL; }
      out = n;
    }
    out[len++] = *p++;
  }
  out[len] = 0;
  return out;
}
