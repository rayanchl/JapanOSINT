#include "hostgate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Capacity. This was 1,024 open-addressed slots with no eviction, and the
 * collectors name ~6,500 distinct hosts: once 1,024 had been seen, slot_for()
 * returned NULL and acquire() returned 0 — UNGATED and uncounted — so every
 * host first contacted after the table filled was fetched with no politeness
 * at all, and nothing on the heartbeat line said so. Each lookup of a new host
 * on the full table also walked all 1,024 slots under the global mutex.
 *
 * Now: 16,384 slots (2.5x the registry's host count) in STABLE storage, found
 * through a chained hash index, so a lookup costs one short chain, never a
 * table walk. A slot is reclaimed only when it is provably idle — nothing in
 * flight, nobody waiting on it, and its gap and any server-requested penalty
 * over by HG_IDLE_TTL_MS — at which point a fresh slot for that host behaves
 * identically, so eviction loses no politeness. Reclaim is a clock sweep. A
 * table genuinely full of live hosts still fails open, but COUNTED
 * (hostgate_counters' over_budget, hostgate_stats' table_full). */
#define HG_SLOTS    16384
#define HG_BUCKETS  32768          /* power of two; load factor <= 0.5         */
#define HG_HOSTLEN  128
/* A slot idle this long past its ready time may be reclaimed. Generous on
 * purpose: reclaiming is only ever needed past 16,384 live hosts. A variable,
 * not a constant, only so the unit test can shrink it. */
static long long g_idle_ttl_ms = 60000;
/* After a sweep finds NOTHING reclaimable, new hosts fail open (counted) for
 * this long without sweeping again, so a table full of live hosts costs one
 * walk per second rather than one walk per request. */
#define HG_FULL_RESCAN_MS 1000
/* Ceiling on a server-requested back-off (hostgate_penalize). A Retry-After of
 * an hour must not park every worker that touches that host for an hour; a
 * minute stops a burst from walking into the same 429 wall, and the fetch that
 * received the long Retry-After surfaces the status to its own caller. */
#define HG_PENALTY_MAX_MS 60000

typedef struct {
  char      host[HG_HOSTLEN];    /* empty => never claimed                    */
  int       in_flight;
  int       waiters;             /* threads parked in acquire() on THIS slot  */
  long long last_start_ms;       /* monotonic ms of the most recent grant     */
  long long penalty_until_ms;    /* the server asked us to stay away until    */
  int       gap_ms;              /* min gap for THIS host (override or global)*/
  int       next;                /* hash chain: slot index + 1, 0 = end       */
} hg_slot;

static hg_slot         g_slots[HG_SLOTS];
static int             g_bucket[HG_BUCKETS];   /* slot index + 1, 0 = empty   */
static int             g_used;                 /* slots ever handed out       */
static int             g_hand;                 /* clock-sweep position        */
static long long       g_full_until_ms;        /* see HG_FULL_RESCAN_MS       */
static pthread_mutex_t g_mu  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv  = PTHREAD_COND_INITIALIZER;
static long            g_waits, g_timeouts, g_inflight;
static long            g_table_full, g_evictions, g_penalties, g_hosts;

static int g_max_conc, g_min_gap_ms;

/* pthread_once, not a `if (initialised) return;` guard: acquire() is called
 * from every collector worker at once, and an unsynchronised read of the
 * init flag is a data race (ThreadSanitizer caught exactly that here). Same
 * pattern http_client_global_init uses for curl_global_init. */
static pthread_once_t g_cfg_once = PTHREAD_ONCE_INIT;

static void cfg_init(void) {
  const char *mc = getenv("JO_HOST_MAX_CONC");
  const char *mg = getenv("JO_HOST_MIN_GAP_MS");
  g_max_conc   = mc ? atoi(mc) : 2;
  g_min_gap_ms = mg ? atoi(mg) : 150;
  if (g_max_conc   < 0) g_max_conc   = 0;
  if (g_min_gap_ms < 0) g_min_gap_ms = 0;
}
/* ── per-host minimum-gap overrides ───────────────────────────────────────
 * Some upstreams rate-limit per client IP far above the global 150 ms gap.
 * reddit.com is the measured case (sources/reddit_world_geo.c): roughly one
 * request per 30 s per IP, and the multireddit groups all come due together,
 * so 15 of 17 groups were 429'd on every pass even when run one at a time.
 * The floor has to live HERE, at the point of the real fetch, because no
 * per-source interval can pace a family that fires as a block.
 *
 * A host in this table matches the hostname or any subdomain of it.
 * JO_HOST_MIN_GAP_OVERRIDES="host=ms,host=ms" adds or replaces entries at
 * boot. An override is a GAP between starts, never a delay of the first
 * request. */
#define HG_MAX_OVERRIDES 32
static struct { char host[HG_HOSTLEN]; int gap_ms; } g_over[HG_MAX_OVERRIDES] = {
  { "reddit.com", 30000 },
  /* 615 gnews-* rows share news.google.com; under a 4-worker sweep they
   * fire together and come back empty, alone every one emits (2026-08-25
   * sweep: 7 marked dead, 5/5 sampled fine serially). 1.5 s keeps a full
   * pass of the family at ~15 min. */
  { "news.google.com", 1500 },
  /* NCBI E-utilities allow 3 requests/second per IP without an API key. The
   * esearch → esummary hop rows (collectors/sources/_ncbi_esummary.inc) pass
   * run one at a time and fail when a sweep runs several together (measured
   * 2026-09-15); 400 ms keeps the whole family under the limit. */
  { "eutils.ncbi.nlm.nih.gov", 400 },
  /* crates.io's own crawler policy is one request per second per client, and
   * it enforces it: measured 2026-10-04, a reverse_dependencies walk under a
   * 4-worker batch sweep was 429'd part-way (the engine stamped the
   * truncation notice naming the 429 and pointing here) and a later single
   * run of the same row was 429'd on its FIRST request while the window was
   * still hot. The batch-33 crates.io rows walk hundreds of pages, so the
   * floor has to be at the fetch, not in a per-source interval. */
  { "crates.io", 1100 },
};
/* The number of LIVE entries above. This was `1` while the table held two, so
 * gap_for_host() — which scans only the first g_nover entries — never saw the
 * news.google.com gap its comment describes: the 615 gnews-* rows kept firing
 * together (found 2026-09-15). Keep it equal to the initialiser count. */
static int g_nover = 4;

static void override_set(const char *host, int gap_ms) {
  for (int i = 0; i < g_nover; i++)
    if (strcmp(g_over[i].host, host) == 0) { g_over[i].gap_ms = gap_ms; return; }
  if (g_nover < HG_MAX_OVERRIDES) {
    snprintf(g_over[g_nover].host, HG_HOSTLEN, "%s", host);
    g_over[g_nover].gap_ms = gap_ms;
    g_nover++;
  }
}

static void overrides_init(void) {
  const char *e = getenv("JO_HOST_MIN_GAP_OVERRIDES");
  if (!e || !*e) return;
  char buf[2048];
  snprintf(buf, sizeof buf, "%s", e);
  char *save = NULL;
  for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
    char *eq = strchr(tok, '=');
    if (!eq) continue;
    *eq = 0;
    while (*tok == ' ') tok++;
    int ms = atoi(eq + 1);
    if (*tok && ms >= 0) override_set(tok, ms);
  }
}

/* Gap for `host`: the longest matching override (exact or parent domain),
 * else the global gap. "notreddit.com" does not match "reddit.com". */
static int gap_for_host(const char *host) {
  int best = g_min_gap_ms; size_t bestlen = 0;
  size_t hl = strlen(host);
  for (int i = 0; i < g_nover; i++) {
    size_t ol = strlen(g_over[i].host);
    if (ol > hl || ol <= bestlen) continue;
    if (strcmp(host + (hl - ol), g_over[i].host) != 0) continue;
    if (ol < hl && host[hl - ol - 1] != '.') continue;
    best = g_over[i].gap_ms; bestlen = ol;
  }
  return best;
}

/* An override host is NOT allowed to fail open on its gap: proceeding into a
 * known per-IP rate limit is a guaranteed 429, which is worse than waiting.
 * The wait is still bounded — a queue deeper than this fails open, and the
 * source retries on its own interval, now de-clustered by the wait. */
#define HG_OVERRIDE_MAX_WAIT_MS 300000

static void cfg_init_all(void) { cfg_init(); overrides_init(); }
static void cfg_once(void) { pthread_once(&g_cfg_once, cfg_init_all); }

/* Always 0 in the product; the unit test advances it to age slots without
 * sleeping. */
static long long g_clock_skew_ms;

static long long mono_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000LL + g_clock_skew_ms;
}

/* Lowercased hostname of `url` into out[]. Returns 0 if there is nothing to
 * gate: unparseable, over-long, or loopback (see header). Mirrors the host
 * parse in httpclient.c's log_host so the gate and the per-client host stats
 * agree on what "the host" is. */
int hostgate_url_host(const char *url, char *out, size_t cap) {
  if (!url || cap == 0) return 0;
  const char *p = strstr(url, "://");
  p = p ? p + 3 : url;
  const char *at = NULL;                   /* strip user:pass@ if present     */
  for (const char *q = p; *q && *q != '/' && *q != '?' && *q != '#'; q++)
    if (*q == '@') at = q;
  if (at) p = at + 1;
  size_t n = 0;
  /* A bracketed IPv6 literal keeps its colons: stop at the ']' and unwrap it,
   * otherwise "[::1]" truncates to "[" and every IPv6 destination looks like a
   * different host than the one we connect to. */
  if (*p == '[') {
    const char *close = strchr(p, ']');
    if (!close) return 0;
    p++;
    n = (size_t)(close - p);
  } else {
    while (p[n] && p[n] != '/' && p[n] != ':' && p[n] != '?' && p[n] != '#') n++;
  }
  if (n == 0 || n >= cap) return 0;
  for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
  out[n] = 0;
  return 1;
}

static int url_host(const char *url, char *out, size_t cap) {
  if (!hostgate_url_host(url, out, cap)) return 0;
  if (!strcmp(out, "localhost") || !strcmp(out, "127.0.0.1") ||
      !strcmp(out, "::1")) return 0;
  return 1;
}

static unsigned hg_hash(const char *s) {
  unsigned h = 2166136261u;                /* FNV-1a                          */
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
  return h;
}

/* The earliest instant this slot may start another request. */
static long long slot_ready_at(const hg_slot *s) {
  long long r = s->last_start_ms + s->gap_ms;
  return s->penalty_until_ms > r ? s->penalty_until_ms : r;
}

/* Existing slot for `host`, or NULL. Never claims. Caller holds g_mu. */
static hg_slot *slot_lookup(const char *host) {
  for (int i = g_bucket[hg_hash(host) & (HG_BUCKETS - 1)]; i; i = g_slots[i - 1].next)
    if (strcmp(g_slots[i - 1].host, host) == 0) return &g_slots[i - 1];
  return NULL;
}

/* Reclaimable: nothing in flight, nobody parked on it (acquire() holds `s`
 * across its cond waits — reclaiming under a waiter would hand that thread
 * another host's slot), and idle well past its gap and any penalty. */
static int slot_evictable(const hg_slot *s, long long now) {
  return s->in_flight == 0 && s->waiters == 0 &&
         now >= slot_ready_at(s) + g_idle_ttl_ms;
}

static void slot_unlink(int idx) {
  int *pp = &g_bucket[hg_hash(g_slots[idx].host) & (HG_BUCKETS - 1)];
  while (*pp && *pp != idx + 1) pp = &g_slots[*pp - 1].next;
  if (*pp) *pp = g_slots[idx].next;
  g_slots[idx].next = 0;
}

/* One clock sweep for a reclaimable slot; its index, or -1. Caller holds g_mu. */
static int slot_reclaim(long long now) {
  if (now < g_full_until_ms) return -1;
  for (int k = 0; k < HG_SLOTS; k++) {
    int idx = g_hand;
    g_hand = (g_hand + 1) % HG_SLOTS;
    if (slot_evictable(&g_slots[idx], now)) {
      slot_unlink(idx);
      g_evictions++;
      g_hosts--;
      return idx;
    }
  }
  g_full_until_ms = now + HG_FULL_RESCAN_MS;
  return -1;
}

/* Slot for `host`, claiming (or reclaiming) one if needed. Caller holds g_mu.
 * NULL only when all HG_SLOTS hold hosts that are live right now; the caller
 * fails open and COUNTS it rather than blocking forever. */
static hg_slot *slot_for(const char *host) {
  hg_slot *s = slot_lookup(host);
  if (s) return s;
  int idx = g_used < HG_SLOTS ? g_used++ : slot_reclaim(mono_ms());
  if (idx < 0) return NULL;
  s = &g_slots[idx];
  memset(s, 0, sizeof *s);
  snprintf(s->host, sizeof s->host, "%s", host);
  s->gap_ms = gap_for_host(host);
  unsigned b = hg_hash(host) & (HG_BUCKETS - 1);
  s->next = g_bucket[b];
  g_bucket[b] = idx + 1;
  g_hosts++;
  return s;
}

int hostgate_acquire(const char *url, int max_wait_ms) {
  char host[HG_HOSTLEN];
  if (!url_host(url, host, sizeof host)) return 0;
  cfg_once();
  if (g_max_conc == 0 && g_min_gap_ms == 0) return 0;   /* gate disabled      */

  long long deadline = mono_ms() + (max_wait_ms > 0 ? max_wait_ms : 0);
  int waited = 0;

  pthread_mutex_lock(&g_mu);
  hg_slot *s = slot_for(host);
  if (!s) {                                /* fail open — but COUNTED         */
    if (g_table_full++ % 1000 == 0)
      fprintf(stderr, "[hostgate] table full (%d live hosts): %s fetched "
                      "ungated (%ld so far)\n", HG_SLOTS, host, g_table_full);
    pthread_mutex_unlock(&g_mu);
    return 0;
  }
  const int gap_ms = s->gap_ms;
  if (gap_ms > g_min_gap_ms) {             /* override host: wait for the gap */
    long long floor = mono_ms() + HG_OVERRIDE_MAX_WAIT_MS;
    if (floor > deadline) deadline = floor;
  }
  /* A server-requested back-off is waited out for the same reason an override
   * gap is: going in early is a guaranteed 429. hostgate_penalize() caps it at
   * HG_PENALTY_MAX_MS, so this wait is bounded too. */
  if (s->penalty_until_ms > deadline) deadline = s->penalty_until_ms;

  s->waiters++;                            /* pins `s`: see slot_evictable()  */
  for (;;) {
    long long now = mono_ms();
    int conc_ok = (g_max_conc == 0) || (s->in_flight < g_max_conc);
    long long ready_at = slot_ready_at(s);
    int gap_ok  = (now >= ready_at);

    if (conc_ok && gap_ok) {
      s->waiters--;
      s->in_flight++;
      s->last_start_ms = now;
      g_inflight++;
      if (waited) g_waits++;
      pthread_mutex_unlock(&g_mu);
      return 1;
    }
    if (now >= deadline) {                 /* fail open — see header          */
      s->waiters--;
      g_timeouts++;
      pthread_mutex_unlock(&g_mu);
      return 0;
    }
    waited = 1;

    /* Wake either when a slot frees (broadcast) or when the gap elapses,
     * whichever is sooner — a pure broadcast wait would sleep past the gap
     * when nothing else is in flight on this host. */
    long long wake = deadline;
    if (!gap_ok && ready_at < wake) wake = ready_at;
    long long delta = wake - now;
    if (delta < 1) delta = 1;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += (time_t)(delta / 1000);
    ts.tv_nsec += (long)(delta % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_cond_timedwait(&g_cv, &g_mu, &ts);
    /* `s` stays valid: slots never move, and our waiters count keeps
     * slot_reclaim() from handing it to another host while we sleep. */
  }
}

void hostgate_release(const char *url) {
  char host[HG_HOSTLEN];
  if (!url_host(url, host, sizeof host)) return;
  pthread_mutex_lock(&g_mu);
  /* Lookup, never claim: a release for a host we hold no slot for (acquire
   * failed open) must not consume a slot — that used to be slot_for(). */
  hg_slot *s = slot_lookup(host);
  if (s && s->in_flight > 0) { s->in_flight--; g_inflight--; }
  pthread_mutex_unlock(&g_mu);
  pthread_cond_broadcast(&g_cv);
}

void hostgate_penalize(const char *url, long ms) {
  char host[HG_HOSTLEN];
  if (ms <= 0 || !url_host(url, host, sizeof host)) return;
  cfg_once();
  if (ms > HG_PENALTY_MAX_MS) ms = HG_PENALTY_MAX_MS;
  pthread_mutex_lock(&g_mu);
  hg_slot *s = slot_for(host);
  if (s) {
    long long until = mono_ms() + ms;
    if (until > s->penalty_until_ms) s->penalty_until_ms = until;
    g_penalties++;
  }
  pthread_mutex_unlock(&g_mu);
}

/* `over_budget` on the scheduler heartbeat is every fail-open, whatever the
 * cause: a wait that ran out of budget AND a host the table had no room for.
 * The second used to be invisible, which is how ~5,000 hosts went ungated
 * without the heartbeat moving. hostgate_stats() splits them. */
void hostgate_counters(long *out_waits, long *out_timeouts, long *out_inflight) {
  pthread_mutex_lock(&g_mu);
  if (out_waits)    *out_waits    = g_waits;
  if (out_timeouts) *out_timeouts = g_timeouts + g_table_full;
  if (out_inflight) *out_inflight = g_inflight;
  pthread_mutex_unlock(&g_mu);
}

void hostgate_stats(hostgate_stats_t *out) {
  if (!out) return;
  pthread_mutex_lock(&g_mu);
  out->waits      = g_waits;
  out->timeouts   = g_timeouts;
  out->table_full = g_table_full;
  out->inflight   = g_inflight;
  out->hosts      = g_hosts;
  out->capacity   = HG_SLOTS;
  out->evictions  = g_evictions;
  out->penalties  = g_penalties;
  pthread_mutex_unlock(&g_mu);
}

/* ── SSRF destination check (see header) ──────────────────────────────────── */

/* Hostnames that name a link-local metadata service without looking like one.
 * The 169.254.169.254 literal is covered by the address rules below; these are
 * the CNAME-style aliases the cloud providers also answer on. */
static int metadata_hostname(const char *h) {
  return !strcmp(h, "metadata.google.internal") ||
         !strcmp(h, "metadata.goog")            ||
         !strcmp(h, "metadata")                 ||
         !strcmp(h, "instance-data");
}

/* Classify one IPv4 address (host byte order). Returns 0 allowed, 1 blocked
 * at the floor, 2 blocked only under `strict`. Split this way so the two
 * strengths cannot disagree about which range is which. */
static int v4_class(unsigned a) {
  unsigned b1 = (a >> 24) & 0xFF, b2 = (a >> 16) & 0xFF;
  if (b1 == 0)                       return 1;   /* 0.0.0.0/8   "this host"  */
  if (b1 == 169 && b2 == 254)        return 1;   /* 169.254/16  link-local   */
  if (b1 >= 224)                     return 1;   /* multicast + reserved     */
  if (a == 0xFFFFFFFFu)              return 1;   /* broadcast                */
  if (b1 == 127)                     return 2;   /* loopback                 */
  if (b1 == 10)                      return 2;   /* RFC1918                  */
  if (b1 == 172 && b2 >= 16 && b2 <= 31) return 2;
  if (b1 == 192 && b2 == 168)        return 2;
  if (b1 == 100 && b2 >= 64 && b2 <= 127) return 2;  /* CGNAT 100.64/10      */
  if (b1 == 192 && b2 == 0 && ((a >> 8) & 0xFF) == 0) return 2;  /* 192.0.0/24 */
  if (b1 == 198 && (b2 == 18 || b2 == 19)) return 2; /* benchmark 198.18/15  */
  return 0;
}

static int v6_class(const unsigned char *a) {
  static const unsigned char v4mapped[12] =
    {0,0,0,0,0,0,0,0,0,0,0xFF,0xFF};
  if (memcmp(a, v4mapped, 12) == 0) {           /* ::ffff:1.2.3.4           */
    unsigned v = ((unsigned)a[12] << 24) | ((unsigned)a[13] << 16) |
                 ((unsigned)a[14] << 8)  |  (unsigned)a[15];
    return v4_class(v);
  }
  int all_zero = 1;
  for (int i = 0; i < 16; i++) if (a[i]) { all_zero = 0; break; }
  if (all_zero)                       return 1;  /* ::  unspecified          */
  if (a[0] == 0xFF)                   return 1;  /* ff00::/8 multicast       */
  if (a[0] == 0xFE && (a[1] & 0xC0) == 0x80) return 1;  /* fe80::/10 link-loc */
  {
    static const unsigned char loop[16] =
      {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    if (memcmp(a, loop, 16) == 0)     return 2;  /* ::1                      */
  }
  if ((a[0] & 0xFE) == 0xFC)          return 2;  /* fc00::/7 unique-local    */
  return 0;
}

int hostgate_addr_check(const char *ip_text, int strict) {
  if (!ip_text || !*ip_text) return HG_URL_OK;
  struct in_addr  v4;
  struct in6_addr v6;
  int cls;
  if (inet_pton(AF_INET, ip_text, &v4) == 1)
    cls = v4_class(ntohl(v4.s_addr));
  else if (inet_pton(AF_INET6, ip_text, &v6) == 1)
    cls = v6_class((const unsigned char *)&v6);
  else
    return HG_URL_OK;                    /* not an address literal: fail open */
  if (cls == 1) return HG_URL_PRIVATE;
  if (cls == 2 && strict) return HG_URL_PRIVATE;
  return HG_URL_OK;
}

/* JO_HTTP_BLOCK_PRIVATE=1 raises the floor to the strict ruleset for every
 * outbound fetch. Off by default because llama-server lives on 127.0.0.1 and
 * LAN cameras are a shipped feature — turning it on is a deployment decision,
 * not something this file may make on the operator's behalf. */
static int floor_is_strict(void) {
  const char *e = getenv("JO_HTTP_BLOCK_PRIVATE");
  return (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
}

/* Scheme + literal-host check. No DNS: this runs on every outbound request and
 * the resolved-address half is done by httpclient's per-connection callback,
 * which sees redirects too. */
static int url_check(const char *url, int strict) {
  if (!url) return HG_URL_BAD_HOST;
  if (strncasecmp(url, "http://", 7) != 0 && strncasecmp(url, "https://", 8) != 0)
    return HG_URL_BAD_SCHEME;
  char host[HG_HOSTLEN];
  if (!hostgate_url_host(url, host, sizeof host)) return HG_URL_BAD_HOST;
  if (metadata_hostname(host)) return HG_URL_PRIVATE;
  if (strict && (!strcmp(host, "localhost") ||
                 (strlen(host) > 10 && !strcmp(host + strlen(host) - 10, ".localhost"))))
    return HG_URL_PRIVATE;
  return hostgate_addr_check(host, strict);
}

int hostgate_url_check(const char *url) {
  return url_check(url, floor_is_strict());
}

/* The floor, applied to an already-resolved address. httpclient.c's
 * per-connection callback used to hardcode `strict = 0`, which quietly made
 * JO_HTTP_BLOCK_PRIVATE=1 a no-op for every HOSTNAME target: url_check() can
 * only judge literal addresses, so the resolved-address callback is the ONLY
 * place a name pointing at 10.0.0.5 is ever seen. Keeping the strength choice
 * here — rather than at the call site — is what stops the two halves of the
 * same policy from disagreeing again. */
int hostgate_addr_check_floor(const char *ip_text) {
  return hostgate_addr_check(ip_text, floor_is_strict());
}

/* The strict RULESET without the DNS half. For a URL that is a match KEY
 * rather than a destination — url_override's `old_url` — resolvability is not
 * a safety property, and demanding it refuses exactly the overrides that
 * matter most: a repair usually exists BECAUSE the old host stopped
 * resolving. The literal-address and metadata-hostname rules still apply, so
 * an internal address cannot be smuggled in on that side either. */
int hostgate_url_check_strict_nodns(const char *url) {
  return url_check(url, 1);
}

int hostgate_host_check(const char *host, int strict) {
  if (!host || !*host) return HG_URL_BAD_HOST;
  if (metadata_hostname(host)) return HG_URL_PRIVATE;
  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res)
    return HG_URL_BAD_HOST;              /* unresolvable: refuse to store it  */
  int bad = HG_URL_OK;
  for (struct addrinfo *ai = res; ai && bad == HG_URL_OK; ai = ai->ai_next) {
    char txt[64] = {0};
    if (ai->ai_family == AF_INET)
      inet_ntop(AF_INET, &((struct sockaddr_in *)ai->ai_addr)->sin_addr,
                txt, sizeof txt);
    else if (ai->ai_family == AF_INET6)
      inet_ntop(AF_INET6, &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr,
                txt, sizeof txt);
    else continue;
    bad = hostgate_addr_check(txt, strict);
  }
  freeaddrinfo(res);
  return bad;
}

int hostgate_url_check_strict(const char *url) {
  int rc = url_check(url, 1);
  if (rc != HG_URL_OK) return rc;

  /* Caller-supplied URL: resolve it and judge EVERY answer, so a name that
   * points at 127.0.0.1 (or at a metadata address) is rejected at save time
   * rather than at fetch time. One DNS lookup on a config-write path is free;
   * doing this on the collector hot path would not be. */
  char host[HG_HOSTLEN];
  if (!hostgate_url_host(url, host, sizeof host)) return HG_URL_BAD_HOST;
  return hostgate_host_check(host, 1);
}

/* ── camera / media destination policy (see header) ───────────────────────── */

int hostgate_camera_lan_allowed(void) {
  const char *e = getenv("JO_CAMERA_ALLOW_LAN");
  return (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
}

/* Strict unless the operator opted in to LAN cameras — and strict regardless
 * when JO_HTTP_BLOCK_PRIVATE raised the floor for everything. */
static int camera_strict(void) {
  return floor_is_strict() || !hostgate_camera_lan_allowed();
}

int hostgate_camera_addr_check(const char *ip_text) {
  return hostgate_addr_check(ip_text, camera_strict());
}

int hostgate_camera_url_check(const char *url, int resolve) {
  if (!url) return HG_URL_BAD_HOST;
  const char *sep = strstr(url, "://");
  if (!sep || sep == url) return HG_URL_BAD_SCHEME;
  for (const char *p = url; p < sep; p++)
    if (!isalnum((unsigned char)*p) && *p != '+' && *p != '-' && *p != '.')
      return HG_URL_BAD_SCHEME;
  char host[HG_HOSTLEN];
  if (!hostgate_url_host(url, host, sizeof host)) return HG_URL_BAD_HOST;
  if (metadata_hostname(host)) return HG_URL_PRIVATE;
  int strict = camera_strict();
  size_t hl = strlen(host);
  if (strict && (!strcmp(host, "localhost") ||
                 (hl > 10 && !strcmp(host + hl - 10, ".localhost"))))
    return HG_URL_PRIVATE;
  int rc = hostgate_addr_check(host, strict);
  if (rc != HG_URL_OK || !resolve) return rc;
  return hostgate_host_check(host, strict);
}

const char *hostgate_url_reason(int rc) {
  switch (rc) {
    case HG_URL_OK:         return "ok";
    case HG_URL_BAD_SCHEME: return "url must be http(s)";
    case HG_URL_BAD_HOST:   return "url host is missing or does not resolve";
    case HG_URL_PRIVATE:    return "url resolves to a private, loopback or "
                                   "link-local address";
    default:                return "url rejected";
  }
}

int hostgate_same_host(const char *a, const char *b) {
  char ha[HG_HOSTLEN], hb[HG_HOSTLEN];
  if (!hostgate_url_host(a, ha, sizeof ha)) return 0;
  if (!hostgate_url_host(b, hb, sizeof hb)) return 0;
  return strcmp(ha, hb) == 0;
}
