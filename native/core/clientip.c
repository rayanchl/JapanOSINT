/* core/clientip.c — see clientip.h. */
#include "core/clientip.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int fam; unsigned char a[16]; int bits; } cidr;

#define CIP_MAX 64
static cidr g_net[CIP_MAX];
static int g_n;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

/* Parse one address (brackets and an IPv4-mapped form accepted). */
static int parse_addr(const char *s, int *fam, unsigned char a[16]) {
  char buf[64];
  size_t L = strlen(s);
  if (L >= sizeof buf) return 0;
  memcpy(buf, s, L + 1);
  char *p = buf;
  if (*p == '[') { p++; char *e = strchr(p, ']'); if (!e) return 0; *e = 0; }
  memset(a, 0, 16);
  if (inet_pton(AF_INET, p, a) == 1) { *fam = 4; return 1; }
  if (inet_pton(AF_INET6, p, a) == 1) {
    /* ::ffff:a.b.c.d is an IPv4 client seen through a dual-stack socket. */
    static const unsigned char mapped[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
    if (!memcmp(a, mapped, 12)) { memmove(a, a + 12, 4); memset(a + 4, 0, 12);
                                  *fam = 4; return 1; }
    *fam = 6; return 1;
  }
  return 0;
}

static int in_net(const cidr *n, int fam, const unsigned char *a) {
  if (n->fam != fam) return 0;
  int full = n->bits / 8, rem = n->bits % 8;
  if (memcmp(n->a, a, (size_t)full)) return 0;
  if (rem) { unsigned char m = (unsigned char)(0xff << (8 - rem));
             if ((n->a[full] & m) != (a[full] & m)) return 0; }
  return 1;
}

static int trusted(int fam, const unsigned char *a) {
  for (int i = 0; i < g_n; i++) if (in_net(&g_net[i], fam, a)) return 1;
  return 0;
}

static int load_locked(void) {
  g_n = 0;
  const char *env = getenv("JO_TRUSTED_PROXIES");
  if (!env || !*env) return 0;
  char *dup = strdup(env), *save = NULL;
  if (!dup) return 0;
  for (char *t = strtok_r(dup, ", \t", &save); t; t = strtok_r(NULL, ", \t", &save)) {
    if (g_n >= CIP_MAX) { fprintf(stderr, "[clientip] JO_TRUSTED_PROXIES: more "
                          "than %d entries — the rest are ignored\n", CIP_MAX); break; }
    cidr c = {0};
    char *slash = strchr(t, '/');
    int bits = -1;
    if (slash) {
      *slash = 0;
      char *e;
      long b = strtol(slash + 1, &e, 10);
      if (*e || slash[1] == 0 || b < 0) b = -2;
      bits = (int)b;
    }
    if (!parse_addr(t, &c.fam, c.a) || bits == -2 ||
        (bits >= 0 && bits > (c.fam == 4 ? 32 : 128))) {
      fprintf(stderr, "[clientip] JO_TRUSTED_PROXIES: '%s%s%s' is not an address "
              "or CIDR block — skipped\n", t, slash ? "/" : "", slash ? slash + 1 : "");
      continue;
    }
    c.bits = bits >= 0 ? bits : (c.fam == 4 ? 32 : 128);
    g_net[g_n++] = c;
  }
  free(dup);
  return g_n;
}

static void load_once(void) {
  pthread_mutex_lock(&g_mu);
  int n = load_locked();
  pthread_mutex_unlock(&g_mu);
  if (n) fprintf(stderr, "[clientip] X-Forwarded-For is believed from %d trusted "
                 "proxy entr%s (JO_TRUSTED_PROXIES)\n", n, n == 1 ? "y" : "ies");
}

int clientip_reload(void) {
  pthread_once(&g_once, load_once);
  pthread_mutex_lock(&g_mu);
  int n = load_locked();
  pthread_mutex_unlock(&g_mu);
  return n;
}

static void put(char *out, size_t n, int fam, const unsigned char *a,
                const char *fallback) {
  char tmp[INET6_ADDRSTRLEN];
  if (inet_ntop(fam == 4 ? AF_INET : AF_INET6, a, tmp, sizeof tmp))
    snprintf(out, n, "%s", tmp);
  else
    snprintf(out, n, "%s", fallback ? fallback : "");
}

int clientip_resolve(const char *peer, const char *xff, char *out, size_t n) {
  if (!out || !n) return 0;
  snprintf(out, n, "%s", peer ? peer : "");
  pthread_once(&g_once, load_once);
  pthread_mutex_lock(&g_mu);
  int fam; unsigned char a[16];
  int from_header = 0;
  if (g_n && peer && xff && *xff && parse_addr(peer, &fam, a) && trusted(fam, a)) {
    /* Right to left: each hop appended the address it received from. */
    char *dup = strdup(xff);
    if (dup) {
      int lfam = fam; unsigned char last[16]; memcpy(last, a, 16);
      char *end = dup + strlen(dup);
      for (;;) {
        char *comma = NULL;
        for (char *q = end; q > dup; q--) if (q[-1] == ',') { comma = q - 1; break; }
        char *tok = comma ? comma + 1 : dup;
        *end = 0;
        while (*tok == ' ' || *tok == '\t') tok++;
        char *te = tok + strlen(tok);
        while (te > tok && (te[-1] == ' ' || te[-1] == '\t')) *--te = 0;
        int tfam; unsigned char ta[16];
        if (!*tok || !parse_addr(tok, &tfam, ta)) break;  /* stop at the last trusted hop */
        lfam = tfam; memcpy(last, ta, 16);
        if (!trusted(tfam, ta)) { from_header = 1; break; }
        if (!comma) { from_header = 1; break; }            /* every hop trusted: leftmost */
        end = comma;
      }
      put(out, n, lfam, last, peer);
      free(dup);
    }
  }
  pthread_mutex_unlock(&g_mu);
  return from_header;
}
