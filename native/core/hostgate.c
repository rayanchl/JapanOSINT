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

#define HG_SLOTS   1024          /* open-addressed; hosts are never evicted   */
#define HG_HOSTLEN 128

typedef struct {
  char      host[HG_HOSTLEN];    /* empty => free slot                        */
  int       in_flight;
  long long last_start_ms;       /* monotonic ms of the most recent grant     */
} hg_slot;

static hg_slot         g_slots[HG_SLOTS];
static pthread_mutex_t g_mu  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv  = PTHREAD_COND_INITIALIZER;
static long            g_waits, g_timeouts, g_inflight;

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
static void cfg_once(void) { pthread_once(&g_cfg_once, cfg_init); }

static long long mono_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
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

/* Slot for `host`, claiming a free one if needed. Caller holds g_mu.
 * NULL only when the table is full, which needs >1024 distinct hosts in one
 * process; callers treat that as "ungated" rather than blocking forever. */
static hg_slot *slot_for(const char *host) {
  unsigned h = hg_hash(host) % HG_SLOTS;
  for (unsigned i = 0; i < HG_SLOTS; i++) {
    hg_slot *s = &g_slots[(h + i) % HG_SLOTS];
    if (s->host[0] == 0) {                 /* free -> claim                   */
      snprintf(s->host, sizeof s->host, "%s", host);
      s->in_flight = 0; s->last_start_ms = 0;
      return s;
    }
    if (strcmp(s->host, host) == 0) return s;
  }
  return NULL;
}

int hostgate_acquire(const char *url, int max_wait_ms) {
  char host[HG_HOSTLEN];
  if (!url_host(url, host, sizeof host)) return 0;
  cfg_once();
  if (g_max_conc == 0 && g_min_gap_ms == 0) return 0;   /* gate disabled      */

  const long long deadline = mono_ms() + (max_wait_ms > 0 ? max_wait_ms : 0);
  int waited = 0;

  pthread_mutex_lock(&g_mu);
  hg_slot *s = slot_for(host);
  if (!s) { pthread_mutex_unlock(&g_mu); return 0; }

  for (;;) {
    long long now = mono_ms();
    int conc_ok = (g_max_conc == 0) || (s->in_flight < g_max_conc);
    long long ready_at = s->last_start_ms + g_min_gap_ms;
    int gap_ok  = (g_min_gap_ms == 0) || (now >= ready_at);

    if (conc_ok && gap_ok) {
      s->in_flight++;
      s->last_start_ms = now;
      g_inflight++;
      if (waited) g_waits++;
      pthread_mutex_unlock(&g_mu);
      return 1;
    }
    if (now >= deadline) {                 /* fail open — see header          */
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
    /* `s` stays valid: slots are never evicted or moved once claimed. */
  }
}

void hostgate_release(const char *url) {
  char host[HG_HOSTLEN];
  if (!url_host(url, host, sizeof host)) return;
  pthread_mutex_lock(&g_mu);
  hg_slot *s = slot_for(host);
  if (s && s->in_flight > 0) { s->in_flight--; g_inflight--; }
  pthread_mutex_unlock(&g_mu);
  pthread_cond_broadcast(&g_cv);
}

void hostgate_counters(long *out_waits, long *out_timeouts, long *out_inflight) {
  pthread_mutex_lock(&g_mu);
  if (out_waits)    *out_waits    = g_waits;
  if (out_timeouts) *out_timeouts = g_timeouts;
  if (out_inflight) *out_inflight = g_inflight;
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

int hostgate_url_check_strict(const char *url) {
  int rc = url_check(url, 1);
  if (rc != HG_URL_OK) return rc;

  /* Caller-supplied URL: resolve it and judge EVERY answer, so a name that
   * points at 127.0.0.1 (or at a metadata address) is rejected at save time
   * rather than at fetch time. One DNS lookup on a config-write path is free;
   * doing this on the collector hot path would not be. */
  char host[HG_HOSTLEN];
  if (!hostgate_url_host(url, host, sizeof host)) return HG_URL_BAD_HOST;
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
    bad = hostgate_addr_check(txt, 1);
  }
  freeaddrinfo(res);
  return bad;
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
