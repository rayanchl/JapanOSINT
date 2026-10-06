#include "ratelimit.h"
#include <string.h>
#include <time.h>
#include <pthread.h>

#define RL_SLOTS  512            /* open-addressed; recycled, never grown */
#define RL_KEYLEN 64             /* an IPv6 text form is 45 bytes worst case */

typedef struct {
  char      key[RL_KEYLEN];      /* empty => free slot                      */
  int       cls;
  long long win_start_ms;        /* monotonic ms the current window opened  */
  long long touched_ms;          /* for LRU recycling when the table fills  */
  long long win_ms;              /* this slot's window, to tell expired ones */
  int       count;
} rl_slot;

/* See ratelimit.h: a live counter of these classes is never recycled. */
static int fail_closed(int cls) { return cls == RL_BREAKGLASS || cls == RL_JWKS; }

static rl_slot         g_slots[RL_SLOTS];
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static long            g_denials;

/* CLOCK_MONOTONIC, not time(): a wall-clock step (NTP, container resume) must
 * not hand out a free window or freeze one shut. Same clock hostgate.c uses. */
static long long mono_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
}

static unsigned rl_hash(const char *s, int cls) {
  unsigned h = 2166136261u;                    /* FNV-1a                    */
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
  h ^= (unsigned)(cls + 1); h *= 16777619u;
  return h;
}

static void slot_claim(rl_slot *s, const char *key, int cls, long long now) {
  size_t n = strlen(key);
  if (n >= RL_KEYLEN) n = RL_KEYLEN - 1;
  memcpy(s->key, key, n); s->key[n] = 0;
  s->cls = cls; s->win_start_ms = now; s->touched_ms = now; s->win_ms = 0;
  s->count = 0;
}

/* Slot for (key, cls). Claims a free slot, else recycles — in order — an
 * expired slot, then the least recently touched slot of a fail-open class.
 * Returns NULL only when every slot holds a live fail-closed counter; the
 * caller then fails OPEN or CLOSED by the requesting class. Caller holds g_mu. */
static rl_slot *slot_for(const char *key, int cls, long long now) {
  unsigned h = rl_hash(key, cls) % RL_SLOTS;
  rl_slot *expired = NULL, *lru = NULL;
  for (unsigned i = 0; i < RL_SLOTS; i++) {
    rl_slot *s = &g_slots[(h + i) % RL_SLOTS];
    if (s->key[0] == 0) { slot_claim(s, key, cls, now); return s; }
    if (s->cls == cls && strcmp(s->key, key) == 0) return s;
    if (s->win_ms > 0 && now - s->win_start_ms >= s->win_ms) {
      if (!expired || s->touched_ms < expired->touched_ms) expired = s;
    } else if (!fail_closed(s->cls)) {
      if (!lru || s->touched_ms < lru->touched_ms) lru = s;
    }
  }
  rl_slot *v = expired ? expired : lru;
  if (v) slot_claim(v, key, cls, now);
  return v;
}

int ratelimit_allow(rl_class cls, const char *key, int limit, int window_sec,
                    int *retry_after_sec) {
  if (retry_after_sec) *retry_after_sec = 0;
  if (limit <= 0 || window_sec <= 0) return 1;          /* disabled          */
  if (cls < 0 || cls >= RL_CLASS_MAX) return 1;
  /* A caller we cannot identify shares one bucket rather than bypassing the
   * limiter: "unknown peer" is exactly the shape an abusive client wants. */
  const char *k = (key && *key) ? key : "-";

  const long long now = mono_ms();
  const long long win = (long long)window_sec * 1000LL;
  int allowed;

  pthread_mutex_lock(&g_mu);
  rl_slot *s = slot_for(k, (int)cls, now);
  if (!s) {
    /* Table pinned by live fail-closed counters. A fail-closed class is
     * refused (a pre-auth path waits out a window rather than getting a free
     * one); anything else is let through as the header documents. */
    allowed = !fail_closed((int)cls);
    if (!allowed) {
      g_denials++;
      if (retry_after_sec) *retry_after_sec = window_sec;
    }
    pthread_mutex_unlock(&g_mu);
    return allowed;
  }
  if (now - s->win_start_ms >= win) { s->win_start_ms = now; s->count = 0; }
  s->touched_ms = now;
  s->win_ms = win;
  if (s->count < limit) { s->count++; allowed = 1; }
  else {
    allowed = 0;
    g_denials++;
    if (retry_after_sec) {
      long long left = win - (now - s->win_start_ms);
      if (left < 0) left = 0;
      *retry_after_sec = (int)((left + 999) / 1000);
    }
  }
  pthread_mutex_unlock(&g_mu);
  return allowed;
}

long ratelimit_denials(void) {
  long n;
  pthread_mutex_lock(&g_mu);
  n = g_denials;
  pthread_mutex_unlock(&g_mu);
  return n;
}
