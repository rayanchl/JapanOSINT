/* core/netjail.c — see netjail.h for WHY. This file is the mechanics: a
 * per-child forward proxy / TCP relay that judges and pins every destination,
 * and (Linux) the network namespace that leaves the child no other route.
 *
 * THREADING. One server thread per box accepts on the listening socket and
 * starts one handler thread per connection (bounded, joinable). Every wait in
 * every thread polls the box's stop pipe too, so netjail_free() can end them
 * all promptly when the child has exited, and nothing outlives the box.
 *
 * THE CHILD SIDE runs between fork() and execve() in a process with many
 * threads, so it makes system calls only, on buffers netjail_new() filled in
 * before the fork: no malloc, no stdio, no locks. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE          /* unshare() and CLONE_NEW* on glibc */
#endif
#include "netjail.h"
#include "hostgate.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <net/if.h>
#include <sched.h>
#include <sys/ioctl.h>
#endif

extern char **environ;

/* macOS has none of these three. CLOEXEC is then set with fcntl right after
 * the socket exists (nj_cloexec), and SIGPIPE is suppressed per socket with
 * SO_NOSIGPIPE instead of per send. */
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0
#endif

static int nj_cloexec(int fd) {
  if (fd >= 0) {
    fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
  }
  return fd;
}

/* Inside a fresh namespace the loopback is ours alone, so the port can be a
 * constant — nothing else can be listening on it. */
#define NJ_NS_PORT      3128
#define NJ_HEAD_MAX     16384   /* a request head larger than this is refused */
#define NJ_MAX_CONN     256     /* connections one child may open in a run    */
#define NJ_MAX_LIVE     32      /* ...and at the same time                    */
#define NJ_HOST_MAX     256

struct netjail {
  int plan;
  int ns;                       /* this run is namespace-confined             */
  int port;
  int lfd;                      /* listening socket, parent side              */
  int sp[2];                    /* socketpair: [0] parent, [1] child          */
  int stop[2];                  /* stop pipe                                  */
  long long deadline_ms;
  char input[NJ_INPUT_MAX];
  char **envp;

  struct sockaddr_storage tgt;  /* RELAY: the pinned, judged address         */
  socklen_t tgt_len;
  char tgt_name[NJ_HOST_MAX];

  /* Prepared for the child (async-signal-safe use only). */
  char uid_map[64], gid_map[64];
  struct sockaddr_in bind_addr;
#ifdef __linux__
  struct ifreq ifr;
#endif
  union { struct cmsghdr h; char buf[CMSG_SPACE(sizeof(int))]; } cmsg;

  pthread_mutex_t mu;
  pthread_t srv;
  int srv_started;
  pthread_t th[NJ_MAX_CONN];
  int nth;
  int live;
  int refused;
};

static long long nj_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void nj_set(char *why, size_t cap, const char *msg) {
  if (why && cap) snprintf(why, cap, "%s", msg);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Mode probe
 * ══════════════════════════════════════════════════════════════════════════ */

static int g_force_no_ns;
static pthread_once_t g_probe_once = PTHREAD_ONCE_INIT;
static int g_ns_ok;

void netjail_test_force_no_ns(int on) { g_force_no_ns = on; }

#ifdef __linux__
static int nj_write_file(const char *path, const char *s) {
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) return -1;
  size_t n = strlen(s);
  ssize_t w = write(fd, s, n);
  close(fd);
  return w == (ssize_t)n ? 0 : -1;
}

/* Enter a network namespace of our own, with loopback up. System calls only;
 * `uid_map`/`gid_map`/`ifr` were prepared before fork. */
static int nj_enter_ns(const char *uid_map, const char *gid_map,
                       struct ifreq *ifr) {
  if (unshare(CLONE_NEWNET) != 0) {
    /* Not root: a user namespace grants the capability inside it. */
    if (unshare(CLONE_NEWUSER | CLONE_NEWNET) != 0) return -1;
    if (nj_write_file("/proc/self/setgroups", "deny") != 0) return -1;
    if (nj_write_file("/proc/self/uid_map", uid_map) != 0) return -1;
    if (nj_write_file("/proc/self/gid_map", gid_map) != 0) return -1;
  }
  int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (s < 0) return -1;
  int rc = ioctl(s, SIOCGIFFLAGS, ifr);
  if (rc == 0) {
    ifr->ifr_flags |= IFF_UP | IFF_RUNNING;
    rc = ioctl(s, SIOCSIFFLAGS, ifr);
  }
  close(s);
  return rc == 0 ? 0 : -1;
}

static void nj_prepare_ids(char *um, size_t umc, char *gm, size_t gmc) {
  snprintf(um, umc, "%u %u 1\n", (unsigned)geteuid(), (unsigned)geteuid());
  snprintf(gm, gmc, "%u %u 1\n", (unsigned)getegid(), (unsigned)getegid());
}

/* Fork a child that does exactly what a real run will do up to the listen,
 * once per process. The answer cannot change while the process lives. */
static void nj_probe(void) {
  char um[64], gm[64];
  struct ifreq ifr;
  memset(&ifr, 0, sizeof ifr);
  snprintf(ifr.ifr_name, sizeof ifr.ifr_name, "lo");
  nj_prepare_ids(um, sizeof um, gm, sizeof gm);
  pid_t pid = fork();
  if (pid < 0) return;
  if (pid == 0) _exit(nj_enter_ns(um, gm, &ifr) == 0 ? 0 : 1);
  int st = 0;
  while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
  g_ns_ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
  if (!g_ns_ok)
    fprintf(stderr, "[netjail] network namespaces unavailable: ffmpeg runs "
                    "proxy-only (http(s) and RTSP judged and pinned; an RTSP "
                    "redirect is not confined)\n");
}
#else
static void nj_probe(void) { g_ns_ok = 0; }
#endif

static int nj_ns_available(void) {
  if (g_force_no_ns) return 0;
  pthread_once(&g_probe_once, nj_probe);
  return g_ns_ok;
}

const char *netjail_mode(void) {
  return nj_ns_available() ? "namespace" : "proxy-only";
}

/* ══════════════════════════════════════════════════════════════════════════
 * Plans and URL surgery
 * ══════════════════════════════════════════════════════════════════════════ */

int netjail_plan(const char *input, const char *scheme, char *why, size_t cap) {
  if (!input || !scheme) { nj_set(why, cap, "no input"); return -1; }
  if (!strcmp(scheme, "http") || !strcmp(scheme, "https") ||
      !strcmp(scheme, "hls"))
    return NJ_PROXY;
  if (!strcmp(scheme, "rtsp") || !strcmp(scheme, "rtsps"))
    return NJ_RELAY;
  if (why && cap)
    snprintf(why, cap, "no sandboxed network path for %s:// inputs", scheme);
  return -1;
}

/* Split "scheme://[userinfo@]host[:port]rest". Offsets into `url`. */
typedef struct {
  size_t auth_start, host_start, host_len, rest_start;
  int port;                    /* 0 when absent */
} nj_url;

static int nj_parse_url(const char *url, nj_url *u) {
  memset(u, 0, sizeof *u);
  const char *sep = strstr(url, "://");
  if (!sep) return -1;
  size_t a = (size_t)(sep - url) + 3;
  size_t e = a;
  while (url[e] && url[e] != '/' && url[e] != '?' && url[e] != '#') e++;
  u->auth_start = a;
  u->rest_start = e;
  size_t h = a;
  for (size_t i = a; i < e; i++) if (url[i] == '@') h = i + 1;
  u->host_start = h;
  size_t hp;                   /* index just past the host */
  if (url[h] == '[') {
    size_t k = h + 1;
    while (k < e && url[k] != ']') k++;
    if (k >= e) return -1;
    u->host_start = h + 1;
    u->host_len = k - h - 1;
    hp = k + 1;
  } else {
    size_t k = h;
    while (k < e && url[k] != ':') k++;
    u->host_len = k - h;
    hp = k;
  }
  if (u->host_len == 0 || u->host_len >= NJ_HOST_MAX) return -1;
  if (hp < e) {
    if (url[hp] != ':') return -1;
    long p = 0;
    size_t k = hp + 1;
    if (k == e) return -1;
    for (; k < e; k++) {
      if (!isdigit((unsigned char)url[k])) return -1;
      p = p * 10 + (url[k] - '0');
      if (p > 65535) return -1;
    }
    if (p < 1) return -1;
    u->port = (int)p;
  }
  return 0;
}

/* Resolve `host`, judge EVERY answer under the camera policy, and return them
 * (caller frees with freeaddrinfo). -2 refused, -1 unresolvable. */
static int nj_resolve_judged(const char *host, int port, struct addrinfo **out,
                             char *why, size_t cap) {
  char url[NJ_HOST_MAX + 32];
  /* The name-level rules (localhost, metadata names, literals) first. */
  if (strchr(host, ':')) snprintf(url, sizeof url, "http://[%s]:%d/", host, port);
  else                   snprintf(url, sizeof url, "http://%s:%d/", host, port);
  int hg = hostgate_camera_url_check(url, 0);
  if (hg != HG_URL_OK) {
    if (why && cap) snprintf(why, cap, "%s: %s", host, hostgate_url_reason(hg));
    return -2;
  }
  char ps[8];
  snprintf(ps, sizeof ps, "%d", port);
  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICSERV;
  if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) {
    if (why && cap) snprintf(why, cap, "%s: does not resolve", host);
    return -1;
  }
  for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
    char ip[INET6_ADDRSTRLEN];
    if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ip, sizeof ip, NULL, 0,
                    NI_NUMERICHOST) != 0 ||
        hostgate_camera_addr_check(ip) != HG_URL_OK) {
      if (why && cap) snprintf(why, cap, "%s resolves to %s, which the camera "
                               "policy refuses", host, ip);
      freeaddrinfo(res);
      return -2;
    }
  }
  *out = res;
  return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Socket helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static void nj_nonblock(int fd) {
  int fl = fcntl(fd, F_GETFL);
  if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

/* Wait for `ev` on `fd`, or the stop pipe, or the box's deadline.
 * 1 ready, 0 stopped / timed out. */
static int nj_wait(netjail *j, int fd, short ev) {
  for (;;) {
    long long left = j->deadline_ms - nj_now_ms();
    if (left <= 0) return 0;
    struct pollfd pf[2] = { { fd, ev, 0 }, { j->stop[0], POLLIN, 0 } };
    int r = poll(pf, 2, (int)(left > 1000 ? 1000 : left));
    if (r < 0) { if (errno == EINTR) continue; return 0; }
    if (pf[1].revents) return 0;
    if (pf[0].revents) return 1;
  }
}

static int nj_write_all(netjail *j, int fd, const char *b, size_t n) {
  while (n) {
    ssize_t w = send(fd, b, n, MSG_NOSIGNAL);
    if (w > 0) { b += w; n -= (size_t)w; continue; }
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      if (!nj_wait(j, fd, POLLOUT)) return -1;
      continue;
    }
    return -1;
  }
  return 0;
}

static int nj_connect_addr(netjail *j, const struct sockaddr *sa,
                           socklen_t len) {
  int fd = nj_cloexec(socket(sa->sa_family, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (fd < 0) return -1;
  nj_nonblock(fd);
  if (connect(fd, sa, len) == 0) return fd;
  if (errno != EINPROGRESS) { close(fd); return -1; }
  if (!nj_wait(j, fd, POLLOUT)) { close(fd); return -1; }
  int err = 0;
  socklen_t el = sizeof err;
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) != 0 || err) {
    close(fd);
    return -1;
  }
  return fd;
}

/* Copy bytes both ways until both sides have finished or the box stops. */
static void nj_pump(netjail *j, int a, int b) {
  char buf[16384];
  int a_open = 1, b_open = 1;
  while (a_open || b_open) {
    long long left = j->deadline_ms - nj_now_ms();
    if (left <= 0) return;
    struct pollfd pf[3];
    int n = 0, ia = -1, ib = -1;
    if (a_open) { pf[n].fd = a; pf[n].events = POLLIN; pf[n].revents = 0; ia = n++; }
    if (b_open) { pf[n].fd = b; pf[n].events = POLLIN; pf[n].revents = 0; ib = n++; }
    pf[n].fd = j->stop[0]; pf[n].events = POLLIN; pf[n].revents = 0; int is = n++;
    int r = poll(pf, (nfds_t)n, (int)(left > 1000 ? 1000 : left));
    if (r < 0) { if (errno == EINTR) continue; return; }
    if (pf[is].revents) return;
    for (int side = 0; side < 2; side++) {
      int idx = side == 0 ? ia : ib;
      if (idx < 0 || !pf[idx].revents) continue;
      int from = side == 0 ? a : b, to = side == 0 ? b : a;
      ssize_t got = recv(from, buf, sizeof buf, 0);
      if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        continue;
      if (got <= 0) {
        shutdown(to, SHUT_WR);
        if (side == 0) a_open = 0; else b_open = 0;
        if (got < 0) return;
        continue;
      }
      if (nj_write_all(j, to, buf, (size_t)got) != 0) return;
    }
  }
}

static void nj_reply(netjail *j, int fd, const char *status) {
  char b[160];
  int n = snprintf(b, sizeof b, "HTTP/1.1 %s\r\nContent-Length: 0\r\n"
                   "Connection: close\r\n\r\n", status);
  if (n > 0) nj_write_all(j, fd, b, (size_t)n);
}

static void nj_count_refused(netjail *j) {
  pthread_mutex_lock(&j->mu);
  j->refused++;
  pthread_mutex_unlock(&j->mu);
}

/* Resolve, judge, connect to a judged address. -2 refused, -1 failed. */
static int nj_dial(netjail *j, const char *host, int port) {
  struct addrinfo *res = NULL;
  char why[384] = "";
  int rc = nj_resolve_judged(host, port, &res, why, sizeof why);
  if (rc != 0) {
    fprintf(stderr, "[netjail] refused %s:%d — %s\n", host, port, why);
    return rc;
  }
  int fd = -1;
  for (struct addrinfo *ai = res; ai && fd < 0; ai = ai->ai_next)
    fd = nj_connect_addr(j, ai->ai_addr, ai->ai_addrlen);
  freeaddrinfo(res);
  return fd < 0 ? -1 : fd;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Connection handlers
 * ══════════════════════════════════════════════════════════════════════════ */

/* "host:port" or "[v6]:port" → host, port. */
static int nj_split_hostport(const char *s, size_t n, char *host, size_t hc,
                             int *port, int dflt) {
  size_t hs = 0, he, ps;
  if (n && s[0] == '[') {
    size_t k = 1;
    while (k < n && s[k] != ']') k++;
    if (k >= n) return -1;
    hs = 1; he = k; ps = k + 1;
  } else {
    size_t k = 0;
    while (k < n && s[k] != ':') k++;
    he = k; ps = k;
  }
  if (he <= hs || he - hs >= hc) return -1;
  memcpy(host, s + hs, he - hs);
  host[he - hs] = '\0';
  if (ps >= n) { if (dflt <= 0) return -1; *port = dflt; return 0; }
  if (s[ps] != ':' || ps + 1 >= n) return -1;
  long p = 0;
  for (size_t k = ps + 1; k < n; k++) {
    if (!isdigit((unsigned char)s[k])) return -1;
    p = p * 10 + (s[k] - '0');
    if (p > 65535) return -1;
  }
  if (p < 1) return -1;
  *port = (int)p;
  return 0;
}

static int nj_hdr_drop(const char *line, size_t n) {
  static const char *const drop[] = { "connection:", "proxy-connection:",
                                      "proxy-authorization:", "keep-alive:",
                                      NULL };
  for (int i = 0; drop[i]; i++) {
    size_t dl = strlen(drop[i]);
    if (n >= dl && strncasecmp(line, drop[i], dl) == 0) return 1;
  }
  return 0;
}

/* Append n bytes, or mark the buffer overflowed (*o = cap) and append
 * nothing more. The caller checks *o < cap before using the buffer. */
static void nj_put(char *b, size_t cap, size_t *o, const char *src, size_t n) {
  if (*o >= cap || n >= cap - *o) { *o = cap; return; }
  memcpy(b + *o, src, n);
  *o += n;
}

static void nj_handle_proxy(netjail *j, int cfd) {
  char *head = malloc(NJ_HEAD_MAX + 1);
  if (!head) return;
  size_t len = 0, hend = 0;
  while (!hend) {
    if (len >= NJ_HEAD_MAX) { nj_reply(j, cfd, "431 Request Header Fields Too Large"); goto out; }
    if (!nj_wait(j, cfd, POLLIN)) goto out;
    ssize_t got = recv(cfd, head + len, NJ_HEAD_MAX - len, 0);
    if (got < 0 && (errno == EAGAIN || errno == EINTR)) continue;
    if (got <= 0) goto out;
    len += (size_t)got;
    head[len] = '\0';
    char *e = strstr(head, "\r\n\r\n");
    if (e) hend = (size_t)(e - head) + 4;
  }

  char *eol = strstr(head, "\r\n");
  char *sp1 = memchr(head, ' ', (size_t)(eol - head));
  char *sp2 = sp1 ? memchr(sp1 + 1, ' ', (size_t)(eol - sp1 - 1)) : NULL;
  if (!sp1 || !sp2) { nj_reply(j, cfd, "400 Bad Request"); goto out; }
  size_t ml = (size_t)(sp1 - head);
  const char *tgt = sp1 + 1;
  size_t tl = (size_t)(sp2 - tgt);

  char host[NJ_HOST_MAX];
  int port = 0;

  if (ml == 7 && !strncmp(head, "CONNECT", 7)) {
    if (nj_split_hostport(tgt, tl, host, sizeof host, &port, 0) != 0) {
      nj_reply(j, cfd, "400 Bad Request"); goto out;
    }
    int ufd = nj_dial(j, host, port);
    if (ufd < 0) {
      if (ufd == -2) nj_count_refused(j);
      nj_reply(j, cfd, ufd == -2 ? "403 Forbidden" : "502 Bad Gateway");
      goto out;
    }
    static const char ok[] = "HTTP/1.1 200 Connection established\r\n\r\n";
    if (nj_write_all(j, cfd, ok, sizeof ok - 1) == 0 &&
        (len == hend || nj_write_all(j, ufd, head + hend, len - hend) == 0))
      nj_pump(j, cfd, ufd);
    close(ufd);
    goto out;
  }

  /* Absolute-form GET/HEAD — the only shapes ffmpeg sends a proxy. */
  if (!((ml == 3 && !strncmp(head, "GET", 3)) ||
        (ml == 4 && !strncmp(head, "HEAD", 4))) ||
      tl < 8 || strncasecmp(tgt, "http://", 7) != 0) {
    nj_reply(j, cfd, "405 Method Not Allowed"); goto out;
  }
  const char *auth = tgt + 7;
  const char *pe = auth;
  while (pe < sp2 && *pe != '/' && *pe != '?') pe++;
  const char *at = NULL;
  for (const char *q = auth; q < pe; q++) if (*q == '@') at = q;
  const char *hp = at ? at + 1 : auth;
  if (nj_split_hostport(hp, (size_t)(pe - hp), host, sizeof host, &port, 80) != 0) {
    nj_reply(j, cfd, "400 Bad Request"); goto out;
  }
  int ufd = nj_dial(j, host, port);
  if (ufd < 0) {
    if (ufd == -2) nj_count_refused(j);
    nj_reply(j, cfd, ufd == -2 ? "403 Forbidden" : "502 Bad Gateway");
    goto out;
  }

  /* Origin-form request line, the client's headers minus hop-by-hop ones, and
   * Connection: close — one request per upstream connection, so a second
   * request on a kept-alive proxy connection can never ride a tunnel to a host
   * it did not name. */
  size_t cap = hend + 512;
  char *out = malloc(cap);
  if (!out) { close(ufd); goto out; }
  size_t o = 0;
  const char *path = pe < sp2 ? pe : "/";
  size_t pl = pe < sp2 ? (size_t)(sp2 - pe) : 1;
  int has_host = 0;
  nj_put(out, cap, &o, head, ml);
  nj_put(out, cap, &o, " ", 1);
  if (*path == '?') nj_put(out, cap, &o, "/", 1);
  nj_put(out, cap, &o, path, pl);
  nj_put(out, cap, &o, sp2, (size_t)(eol - sp2));      /* " HTTP/1.x" */
  nj_put(out, cap, &o, "\r\n", 2);
  for (char *l = eol + 2; l < head + hend - 2;) {
    char *le = strstr(l, "\r\n");
    if (!le) break;
    size_t ll = (size_t)(le - l);
    if (ll >= 5 && !strncasecmp(l, "host:", 5)) has_host = 1;
    if (!nj_hdr_drop(l, ll)) {
      nj_put(out, cap, &o, l, ll);
      nj_put(out, cap, &o, "\r\n", 2);
    }
    l = le + 2;
  }
  if (!has_host) {
    nj_put(out, cap, &o, "Host: ", 6);
    nj_put(out, cap, &o, hp, (size_t)(pe - hp));
    nj_put(out, cap, &o, "\r\n", 2);
  }
  static const char cc[] = "Connection: close\r\n\r\n";
  nj_put(out, cap, &o, cc, sizeof cc - 1);
  if (o >= cap) {                     /* cannot happen: cap > the head + 512 */
    nj_reply(j, cfd, "431 Request Header Fields Too Large");
    free(out);
    close(ufd);
    goto out;
  }
  if (nj_write_all(j, ufd, out, o) == 0 &&
      (len == hend || nj_write_all(j, ufd, head + hend, len - hend) == 0))
    nj_pump(j, cfd, ufd);
  free(out);
  close(ufd);
out:
  free(head);
}

static void nj_handle_relay(netjail *j, int cfd) {
  int ufd = nj_connect_addr(j, (const struct sockaddr *)&j->tgt, j->tgt_len);
  if (ufd < 0) {
    fprintf(stderr, "[netjail] relay to %s failed\n", j->tgt_name);
    return;
  }
  nj_pump(j, cfd, ufd);
  close(ufd);
}

typedef struct { netjail *j; int fd; } nj_arg;

static void *nj_conn_main(void *p) {
  nj_arg *a = p;
  netjail *j = a->j;
  nj_nonblock(a->fd);
  if (j->plan == NJ_PROXY) nj_handle_proxy(j, a->fd);
  else                     nj_handle_relay(j, a->fd);
  close(a->fd);
  free(a);
  pthread_mutex_lock(&j->mu);
  j->live--;
  pthread_mutex_unlock(&j->mu);
  return NULL;
}

static void *nj_server_main(void *p) {
  netjail *j = p;
  for (;;) {
    if (!nj_wait(j, j->lfd, POLLIN)) break;
    int cfd = accept(j->lfd, NULL, NULL);
    if (cfd < 0) { if (errno == EINTR || errno == EAGAIN) continue; break; }
    nj_cloexec(cfd);
    pthread_mutex_lock(&j->mu);
    int over = j->nth >= NJ_MAX_CONN || j->live >= NJ_MAX_LIVE;
    if (over) j->refused++;
    pthread_mutex_unlock(&j->mu);
    if (over) {
      fprintf(stderr, "[netjail] connection limit reached; refusing\n");
      close(cfd);
      continue;
    }
    nj_arg *a = malloc(sizeof *a);
    if (!a) { close(cfd); continue; }
    a->j = j; a->fd = cfd;
    pthread_mutex_lock(&j->mu);
    j->live++;
    int rc = pthread_create(&j->th[j->nth], NULL, nj_conn_main, a);
    if (rc == 0) j->nth++;
    else j->live--;
    pthread_mutex_unlock(&j->mu);
    if (rc != 0) { close(cfd); free(a); }
  }
  for (int i = 0; i < j->nth; i++) pthread_join(j->th[i], NULL);
  return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ══════════════════════════════════════════════════════════════════════════ */

static int nj_is_proxy_var(const char *e) {
  static const char *const v[] = { "http_proxy=", "HTTP_PROXY=", "https_proxy=",
                                   "HTTPS_PROXY=", "all_proxy=", "ALL_PROXY=",
                                   "no_proxy=", "NO_PROXY=", NULL };
  for (int i = 0; v[i]; i++)
    if (!strncmp(e, v[i], strlen(v[i]))) return 1;
  return 0;
}

static int nj_build_env(netjail *j) {
  size_t n = 0;
  for (char **e = environ; e && *e; e++) n++;
  j->envp = calloc(n + 2, sizeof *j->envp);
  if (!j->envp) return -1;
  size_t k = 0;
  for (char **e = environ; e && *e; e++) {
    if (nj_is_proxy_var(*e)) continue;
    if (!(j->envp[k] = strdup(*e))) return -1;
    k++;
  }
  if (j->plan == NJ_PROXY) {
    char b[64];
    snprintf(b, sizeof b, "http_proxy=http://127.0.0.1:%d", j->port);
    if (!(j->envp[k] = strdup(b))) return -1;
    k++;
  }
  j->envp[k] = NULL;
  return 0;
}

netjail *netjail_new(const char *input, int plan, int timeout_ms,
                     int *blocked, char *why, size_t cap) {
  if (blocked) *blocked = 0;
  if (!input || (plan != NJ_PROXY && plan != NJ_RELAY)) {
    nj_set(why, cap, "no sandbox plan");
    return NULL;
  }
  netjail *j = calloc(1, sizeof *j);
  if (!j) { nj_set(why, cap, "out of memory"); return NULL; }
  j->plan = plan;
  j->lfd = j->sp[0] = j->sp[1] = j->stop[0] = j->stop[1] = -1;
  pthread_mutex_init(&j->mu, NULL);
  if (timeout_ms <= 0) timeout_ms = 30000;
  j->deadline_ms = nj_now_ms() + timeout_ms + 2000;
  j->ns = nj_ns_available();

  if (pipe(j->stop) != 0) goto fail_sys;
  fcntl(j->stop[0], F_SETFD, FD_CLOEXEC);
  fcntl(j->stop[1], F_SETFD, FD_CLOEXEC);

  if (j->ns) {
    j->port = NJ_NS_PORT;
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, j->sp) != 0)
      goto fail_sys;
    nj_cloexec(j->sp[0]);
    nj_cloexec(j->sp[1]);
#ifdef __linux__
    nj_prepare_ids(j->uid_map, sizeof j->uid_map, j->gid_map, sizeof j->gid_map);
    snprintf(j->ifr.ifr_name, sizeof j->ifr.ifr_name, "lo");
#endif
    j->bind_addr.sin_family = AF_INET;
    j->bind_addr.sin_port = htons(NJ_NS_PORT);
    j->bind_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  } else {
    /* Proxy-only: our own loopback, an ephemeral port. */
    j->lfd = nj_cloexec(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (j->lfd < 0) goto fail_sys;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t sl = sizeof sa;
    if (bind(j->lfd, (struct sockaddr *)&sa, sizeof sa) != 0 ||
        listen(j->lfd, 64) != 0 ||
        getsockname(j->lfd, (struct sockaddr *)&sa, &sl) != 0)
      goto fail_sys;
    j->port = ntohs(sa.sin_port);
  }

  if (plan == NJ_RELAY) {
    nj_url u;
    if (nj_parse_url(input, &u) != 0) {
      if (blocked) *blocked = 1;
      nj_set(why, cap, "unparseable rtsp url");
      goto fail;
    }
    memcpy(j->tgt_name, input + u.host_start, u.host_len);
    j->tgt_name[u.host_len] = '\0';
    int port = u.port ? u.port : (!strncasecmp(input, "rtsps", 5) ? 322 : 554);
    struct addrinfo *res = NULL;
    if (nj_resolve_judged(j->tgt_name, port, &res, why, cap) != 0) {
      if (blocked) *blocked = 1;
      goto fail;
    }
    memcpy(&j->tgt, res->ai_addr, res->ai_addrlen);
    j->tgt_len = (socklen_t)res->ai_addrlen;
    freeaddrinfo(res);
    /* Same URL, same credentials and path; only the authority's host:port
     * changes, to the relay. */
    size_t ul = u.host_start - u.auth_start;
    if (ul && input[u.host_start - 1] == '[') ul--;      /* drop the bracket */
    int n = snprintf(j->input, sizeof j->input, "%.*s%.*s127.0.0.1:%d%s",
                     (int)u.auth_start, input, (int)ul, input + u.auth_start,
                     j->port, input + u.rest_start);
    if (n < 0 || (size_t)n >= sizeof j->input) { nj_set(why, cap, "url too long"); goto fail; }
  } else {
    if (strlen(input) >= sizeof j->input) { nj_set(why, cap, "url too long"); goto fail; }
    snprintf(j->input, sizeof j->input, "%s", input);
  }
  if (nj_build_env(j) != 0) goto fail_sys;
  return j;

fail_sys:
  nj_set(why, cap, "could not set up the network sandbox");
fail:
  netjail_free(j);
  return NULL;
}

const char *netjail_input(const netjail *j) { return j ? j->input : NULL; }
char *const *netjail_envp(const netjail *j) { return j ? j->envp : NULL; }
int netjail_refused(netjail *j) {
  if (!j) return 0;
  pthread_mutex_lock(&j->mu);
  int n = j->refused;
  pthread_mutex_unlock(&j->mu);
  return n;
}

int netjail_child_enter(netjail *j) {
  if (!j || !j->ns) return 0;
#ifdef __linux__
  if (nj_enter_ns(j->uid_map, j->gid_map, &j->ifr) != 0) return -1;
  int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (s < 0) return -1;
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  if (bind(s, (struct sockaddr *)&j->bind_addr, sizeof j->bind_addr) != 0 ||
      listen(s, 64) != 0)
    return -1;
  char byte = 'L';
  struct iovec iov = { &byte, 1 };
  struct msghdr m;
  memset(&m, 0, sizeof m);
  m.msg_iov = &iov;
  m.msg_iovlen = 1;
  m.msg_control = j->cmsg.buf;
  m.msg_controllen = sizeof j->cmsg.buf;
  struct cmsghdr *c = CMSG_FIRSTHDR(&m);
  c->cmsg_level = SOL_SOCKET;
  c->cmsg_type = SCM_RIGHTS;
  c->cmsg_len = CMSG_LEN(sizeof(int));
  memcpy(CMSG_DATA(c), &s, sizeof s);
  if (sendmsg(j->sp[1], &m, MSG_NOSIGNAL) != 1) return -1;
  close(s);                    /* the parent holds it now */
  close(j->sp[1]);
  return 0;
#else
  return -1;
#endif
}

int netjail_parent_start(netjail *j) {
  if (!j) return -1;
  if (j->ns) {
    close(j->sp[1]);
    j->sp[1] = -1;
    struct pollfd pf = { j->sp[0], POLLIN, 0 };
    if (poll(&pf, 1, 10000) <= 0) return -1;
    char byte;
    struct iovec iov = { &byte, 1 };
    union { struct cmsghdr h; char buf[CMSG_SPACE(sizeof(int))]; } cb;
    struct msghdr m;
    memset(&m, 0, sizeof m);
    m.msg_iov = &iov;
    m.msg_iovlen = 1;
    m.msg_control = cb.buf;
    m.msg_controllen = sizeof cb.buf;
    if (recvmsg(j->sp[0], &m, MSG_CMSG_CLOEXEC) != 1) return -1;
    struct cmsghdr *c = CMSG_FIRSTHDR(&m);
    if (!c || c->cmsg_type != SCM_RIGHTS) return -1;
    memcpy(&j->lfd, CMSG_DATA(c), sizeof j->lfd);
    nj_cloexec(j->lfd);
  }
  if (pthread_create(&j->srv, NULL, nj_server_main, j) != 0) return -1;
  j->srv_started = 1;
  return 0;
}

void netjail_free(netjail *j) {
  if (!j) return;
  if (j->stop[1] >= 0) { char b = 'x'; ssize_t w = write(j->stop[1], &b, 1); (void)w; }
  if (j->srv_started) pthread_join(j->srv, NULL);
  if (j->lfd >= 0) close(j->lfd);
  for (int i = 0; i < 2; i++) {
    if (j->sp[i] >= 0) close(j->sp[i]);
    if (j->stop[i] >= 0) close(j->stop[i]);
  }
  if (j->envp) {
    for (char **e = j->envp; *e; e++) free(*e);
    free(j->envp);
  }
  pthread_mutex_destroy(&j->mu);
  free(j);
}
