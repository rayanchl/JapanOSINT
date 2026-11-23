/* lib/joenv.c — see joenv.h. */
#include "joenv.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Names already complained about. Tiny (a process has at most a few hundred
 * knobs), linear scan under a mutex, grown by realloc. */
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static char **g_warned;
static size_t g_nwarned, g_cwarned;

/* 1 the first time `name` is seen, 0 afterwards. */
static int first_time(const char *name) {
  int fresh = 1;
  pthread_mutex_lock(&g_mu);
  for (size_t i = 0; i < g_nwarned; i++)
    if (strcmp(g_warned[i], name) == 0) { fresh = 0; break; }
  if (fresh) {
    if (g_nwarned == g_cwarned) {
      size_t nc = g_cwarned ? g_cwarned * 2 : 64;
      char **nv = realloc(g_warned, nc * sizeof *nv);
      if (nv) { g_warned = nv; g_cwarned = nc; }
    }
    if (g_nwarned < g_cwarned) {
      char *d = strdup(name);
      if (d) g_warned[g_nwarned++] = d;
    }
  }
  pthread_mutex_unlock(&g_mu);
  return fresh;
}

static void warn(const char *name, const char *val, const char *what) {
  if (first_time(name))
    fprintf(stderr, "[env] %s=\"%.64s\" %s\n", name, val, what);
}

static const char *raw(const char *name) {
  const char *v = getenv(name);
  return (v && *v) ? v : NULL;
}

static int only_space(const char *p) {
  while (*p && isspace((unsigned char)*p)) p++;
  return *p == 0;
}

static long long ll_impl(const char *name, long long def, long long min,
                         long long max, int strict) {
  const char *v = raw(name);
  if (!v) return def;
  char *end = NULL;
  errno = 0;
  long long x = strtoll(v, &end, 10);
  if (end == v || !only_space(end)) {
    warn(name, v, "is not an integer; using the default");
    return def;
  }
  if (errno == ERANGE || x < min || x > max) {
    long long c = (errno == ERANGE) ? (x < 0 ? min : max) : (x < min ? min : max);
    if (strict) {
      warn(name, v, "is out of range; using the default");
      return def;
    }
    warn(name, v, "is out of range; clamped");
    return c;
  }
  return x;
}

long long jo_env_ll(const char *n, long long d, long long lo, long long hi) {
  return ll_impl(n, d, lo, hi, 0);
}
long long jo_env_ll_strict(const char *n, long long d, long long lo, long long hi) {
  return ll_impl(n, d, lo, hi, 1);
}
long jo_env_long(const char *n, long d, long lo, long hi) {
  return (long)ll_impl(n, d, lo, hi, 0);
}
long jo_env_long_strict(const char *n, long d, long lo, long hi) {
  return (long)ll_impl(n, d, lo, hi, 1);
}
int jo_env_int(const char *n, int d, int lo, int hi) {
  return (int)ll_impl(n, d, lo, hi, 0);
}
int jo_env_int_strict(const char *n, int d, int lo, int hi) {
  return (int)ll_impl(n, d, lo, hi, 1);
}

static double dbl_impl(const char *name, double def, double min, double max,
                       int strict) {
  const char *v = raw(name);
  if (!v) return def;
  char *end = NULL;
  double x = strtod(v, &end);
  if (end == v || !only_space(end) || !isfinite(x)) {
    warn(name, v, "is not a number; using the default");
    return def;
  }
  if (x < min || x > max) {
    if (strict) {
      warn(name, v, "is out of range; using the default");
      return def;
    }
    warn(name, v, "is out of range; clamped");
    return x < min ? min : max;
  }
  return x;
}
double jo_env_double(const char *n, double d, double lo, double hi) {
  return dbl_impl(n, d, lo, hi, 0);
}
double jo_env_double_strict(const char *n, double d, double lo, double hi) {
  return dbl_impl(n, d, lo, hi, 1);
}

int jo_env_bool(const char *name, int def) {
  const char *v = raw(name);
  if (!v) return def;
  static const char *const yes[] = {"1", "true", "yes", "on", "y", "t", NULL};
  static const char *const no[]  = {"0", "false", "no", "off", "n", "f", NULL};
  char buf[16];
  size_t n = strlen(v);
  while (n && isspace((unsigned char)v[n - 1])) n--;
  while (n && isspace((unsigned char)*v)) { v++; n--; }
  if (n && n < sizeof buf) {
    memcpy(buf, v, n);
    buf[n] = 0;
    for (int i = 0; yes[i]; i++) if (!strcasecmp(buf, yes[i])) return 1;
    for (int i = 0; no[i]; i++)  if (!strcasecmp(buf, no[i]))  return 0;
  }
  warn(name, v, "is not a boolean (1/0/true/false/yes/no/on/off); using the default");
  return def;
}

const char *jo_env_str(const char *name, const char *def) {
  const char *v = raw(name);
  return v ? v : def;
}
