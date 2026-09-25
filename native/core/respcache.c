/* core/respcache.c — see respcache.h for why this exists. */
#include "respcache.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Eight entries is not a guess: the routes that qualify (a body identical for
 * every caller, expensive to build) are /api/status in its two operator
 * variants and /api/intel/sources, and a handful of slots leaves room for the
 * next two without making this a memory sink — the entries are megabytes each,
 * not kilobytes. Full table evicts the OLDEST, which for a TTL cache is also
 * the least useful. */
#define RC_SLOTS 8
#define RC_KEY_MAX 64

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static struct {
  char   key[RC_KEY_MAX];
  char  *body;
  size_t len;
  long long at_ms;            /* monotonic, so a clock change cannot make an
                               * entry look fresh forever (or expire early) */
} g_e[RC_SLOTS];
static long long g_hits, g_misses;

static long long now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

char *respcache_get(const char *key, int ttl_sec, long *age_ms) {
  if (age_ms) *age_ms = 0;
  if (!key || !*key || ttl_sec <= 0) return NULL;
  char *copy = NULL;
  pthread_mutex_lock(&g_mu);
  for (int i = 0; i < RC_SLOTS; i++) {
    if (!g_e[i].body || strcmp(g_e[i].key, key)) continue;
    long long age = now_ms() - g_e[i].at_ms;
    if (age > (long long)ttl_sec * 1000) break;      /* stale: treat as a miss */
    copy = malloc(g_e[i].len + 1);
    if (copy) {
      memcpy(copy, g_e[i].body, g_e[i].len + 1);
      if (age_ms) *age_ms = (long)age;
    }
    break;
  }
  if (copy) g_hits++; else g_misses++;
  pthread_mutex_unlock(&g_mu);
  return copy;
}

void respcache_put(const char *key, const char *body) {
  if (!key || !*key || !body || !*body) return;      /* never cache a failure */
  size_t len = strlen(body);
  char *copy = malloc(len + 1);
  if (!copy) return;
  memcpy(copy, body, len + 1);

  pthread_mutex_lock(&g_mu);
  int slot = -1, oldest = 0;
  for (int i = 0; i < RC_SLOTS; i++) {
    if (g_e[i].body && !strcmp(g_e[i].key, key)) { slot = i; break; }
    if (!g_e[i].body) { if (slot < 0) slot = i; }
    if (g_e[i].at_ms < g_e[oldest].at_ms) oldest = i;
  }
  if (slot < 0) slot = oldest;
  free(g_e[slot].body);
  snprintf(g_e[slot].key, sizeof g_e[slot].key, "%s", key);
  g_e[slot].body  = copy;
  g_e[slot].len   = len;
  g_e[slot].at_ms = now_ms();
  pthread_mutex_unlock(&g_mu);
}

void respcache_drop(const char *key) {
  pthread_mutex_lock(&g_mu);
  for (int i = 0; i < RC_SLOTS; i++) {
    if (!g_e[i].body) continue;
    if (key && strcmp(g_e[i].key, key)) continue;
    free(g_e[i].body);
    g_e[i].body = NULL;
    g_e[i].len = 0;
    g_e[i].key[0] = 0;
  }
  pthread_mutex_unlock(&g_mu);
}

void respcache_stats(long long *hits, long long *misses, size_t *bytes) {
  pthread_mutex_lock(&g_mu);
  if (hits) *hits = g_hits;
  if (misses) *misses = g_misses;
  if (bytes) {
    size_t n = 0;
    for (int i = 0; i < RC_SLOTS; i++) n += g_e[i].len;
    *bytes = n;
  }
  pthread_mutex_unlock(&g_mu);
}
