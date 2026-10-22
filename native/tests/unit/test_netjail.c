/* tests/unit/test_netjail.c — ffmpeg's network box (core/netjail.c).
 *
 * WHAT IS BEING PROTECTED. ffmpeg dials its own sockets, so before the box a
 * live playlist's later segments, a name ffmpeg re-resolved and an RTSP
 * redirect all reached destinations nothing had judged. Pinned:
 *   1. plans: http(s)/hls → proxy, rtsp(s) → relay, rtmp/tcp/udp refused; an
 *      rtsp URL is rewritten to the relay with its credentials, path and query
 *      intact, and a refused rtsp host is refused before any process exists;
 *   2. the proxy, driven in-process: a GET or CONNECT to the metadata service
 *      or the LAN is a 403 and counted; an allowed GET reaches the origin in
 *      origin form with hop-by-hop headers dropped and Connection: close;
 *      CONNECT tunnels bytes both ways;
 *   3. the relay sends every connection to the pinned address;
 *   4. a real child (curl) through the box: via the proxy it reaches the
 *      fixture and is refused the metadata service; and in NAMESPACE mode a
 *      direct connection that ignores the proxy has no route at all — the
 *      property that confines a redirect or a re-resolved name;
 *   5. real ffmpeg (when installed) decodes a frame of an HLS stream through
 *      the box, so the proxy is not merely refusing things.
 * Fixture servers are on 127.0.0.1, which the camera policy refuses; the
 * test-only seam admits loopback and still refuses metadata. Steps needing a
 * namespace, curl or ffmpeg say SKIP when the host lacks them. */
#include "../../core/netjail.c"
#include "../../core/ffmpeg.h"

#include <assert.h>
#include <signal.h>
#include <sys/stat.h>

/* ── a fixture HTTP origin ─────────────────────────────────────────────── */

typedef struct { const char *path; const unsigned char *body; size_t len; } fx_file;

static int fx_lfd = -1, fx_port;
static pthread_t fx_th;
static pthread_mutex_t fx_mu = PTHREAD_MUTEX_INITIALIZER;
static char fx_last[4096];
static int fx_hits;
static fx_file fx_files[8];
static int fx_nfiles;
static int fx_stop;                 /* __atomic: main writes, fixture reads */

static void *fx_main(void *p) {
  (void)p;
  while (!__atomic_load_n(&fx_stop, __ATOMIC_ACQUIRE)) {
    struct pollfd pf = { fx_lfd, POLLIN, 0 };
    if (poll(&pf, 1, 200) <= 0) continue;
    int c = accept(fx_lfd, NULL, NULL);
    if (c < 0) continue;
    char head[4096];
    size_t n = 0;
    while (n < sizeof head - 1) {
      ssize_t g = recv(c, head + n, sizeof head - 1 - n, 0);
      if (g <= 0) break;
      n += (size_t)g;
      head[n] = '\0';
      if (strstr(head, "\r\n\r\n")) break;
    }
    head[n] = '\0';
    pthread_mutex_lock(&fx_mu);
    snprintf(fx_last, sizeof fx_last, "%s", head);
    fx_hits++;
    pthread_mutex_unlock(&fx_mu);
    if (!strncmp(head, "ECHO", 4)) {           /* raw byte echo for tunnels */
      send(c, head, n, MSG_NOSIGNAL);
      close(c);
      continue;
    }
    char path[512] = "";
    sscanf(head, "%*s %511s", path);
    fx_file hit = { NULL, NULL, 0 };
    const fx_file *f = NULL;
    pthread_mutex_lock(&fx_mu);
    for (int i = 0; i < fx_nfiles; i++)
      if (!strcmp(fx_files[i].path, path)) { hit = fx_files[i]; f = &hit; }
    pthread_mutex_unlock(&fx_mu);
    char h[256];
    if (f) {
      int hl = snprintf(h, sizeof h, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n"
                        "Connection: close\r\n\r\n", f->len);
      send(c, h, (size_t)hl, MSG_NOSIGNAL);
      send(c, f->body, f->len, MSG_NOSIGNAL);
    } else {
      static const char nf[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                               "Connection: close\r\n\r\n";
      send(c, nf, sizeof nf - 1, MSG_NOSIGNAL);
    }
    close(c);
  }
  return NULL;
}

static void fx_start(void) {
  fx_lfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  socklen_t sl = sizeof sa;
  assert(bind(fx_lfd, (struct sockaddr *)&sa, sizeof sa) == 0);
  assert(listen(fx_lfd, 16) == 0);
  assert(getsockname(fx_lfd, (struct sockaddr *)&sa, &sl) == 0);
  fx_port = ntohs(sa.sin_port);
  pthread_create(&fx_th, NULL, fx_main, NULL);
}

static int fx_hit_count(void) {
  pthread_mutex_lock(&fx_mu);
  int n = fx_hits;
  pthread_mutex_unlock(&fx_mu);
  return n;
}

static void fx_add(const char *path, const void *body, size_t len) {
  pthread_mutex_lock(&fx_mu);
  fx_files[fx_nfiles].path = path;
  fx_files[fx_nfiles].body = body;
  fx_files[fx_nfiles].len = len;
  fx_nfiles++;
  pthread_mutex_unlock(&fx_mu);
}

/* ── a client talking to a box's port in-process ────────────────────────── */

static int box_port(netjail *j) { return j->port; }

static int dial_local(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_port = htons((unsigned short)port);
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(connect(fd, (struct sockaddr *)&sa, sizeof sa) == 0);
  return fd;
}

/* Send `req`, read until EOF (bounded), into out. */
static size_t exchange(int port, const char *req, char *out, size_t cap) {
  int fd = dial_local(port);
  send(fd, req, strlen(req), MSG_NOSIGNAL);
  size_t n = 0;
  for (;;) {
    struct pollfd pf = { fd, POLLIN, 0 };
    if (poll(&pf, 1, 5000) <= 0) break;
    ssize_t g = recv(fd, out + n, cap - 1 - n, 0);
    if (g <= 0) break;
    n += (size_t)g;
    if (n >= cap - 1) break;
  }
  out[n] = '\0';
  close(fd);
  return n;
}

/* ── a real child in a box ──────────────────────────────────────────────── */

static int run_boxed(netjail *j, const char *const argv[], char *out, size_t cap) {
  int p[2];
  assert(pipe(p) == 0);
  pid_t pid = fork();
  assert(pid >= 0);
  if (pid == 0) {
    close(p[0]);
    if (netjail_child_enter(j) != 0) _exit(126);
    dup2(p[1], 1);
    int dn = open("/dev/null", O_WRONLY);
    if (dn >= 0) dup2(dn, 2);
    execve(argv[0], (char *const *)argv, netjail_envp(j));
    _exit(127);
  }
  close(p[1]);
  assert(netjail_parent_start(j) == 0);
  size_t n = 0;
  ssize_t g;
  while (n < cap - 1 && (g = read(p[0], out + n, cap - 1 - n)) > 0) n += (size_t)g;
  out[n] = '\0';
  close(p[0]);
  int st = 0;
  waitpid(pid, &st, 0);
  return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

int main(void) {
  unsetenv("JO_HTTP_BLOCK_PRIVATE");
  signal(SIGPIPE, SIG_IGN);
  printf("  sandbox mode on this host: %s\n", netjail_mode());

  /* 1. plans and URL surgery */
  char why[384];
  assert(netjail_plan("http://x/a", "http", why, sizeof why) == NJ_PROXY);
  assert(netjail_plan("https://x/a", "https", why, sizeof why) == NJ_PROXY);
  assert(netjail_plan("rtsp://x/a", "rtsp", why, sizeof why) == NJ_RELAY);
  assert(netjail_plan("rtsps://x/a", "rtsps", why, sizeof why) == NJ_RELAY);
  assert(netjail_plan("rtmp://x/a", "rtmp", why, sizeof why) < 0);
  assert(netjail_plan("udp://x:1", "udp", why, sizeof why) < 0);
  int blocked = -1;
  netjail *j = netjail_new("rtsp://user:p%40ss@93.184.216.34:8554/live/ch1?x=1",
                           NJ_RELAY, 5000, &blocked, why, sizeof why);
  assert(j && blocked == 0);
  char want[256];
  snprintf(want, sizeof want, "rtsp://user:p%%40ss@127.0.0.1:%d/live/ch1?x=1", j->port);
  if (strcmp(netjail_input(j), want)) { fprintf(stderr, "got %s want %s\n", netjail_input(j), want); assert(0); }
  assert(j->tgt.ss_family == AF_INET &&
         ntohs(((struct sockaddr_in *)&j->tgt)->sin_port) == 8554);
  netjail_free(j);
  j = netjail_new("rtsp://[2606:2800:220:1:248:1893:25c8:1946]/s", NJ_RELAY, 5000,
                  &blocked, why, sizeof why);
  assert(j && j->tgt.ss_family == AF_INET6 &&
         ntohs(((struct sockaddr_in6 *)&j->tgt)->sin6_port) == 554);
  snprintf(want, sizeof want, "rtsp://127.0.0.1:%d/s", j->port);
  assert(!strcmp(netjail_input(j), want));
  netjail_free(j);
  assert(!netjail_new("rtsp://169.254.169.254/x", NJ_RELAY, 5000, &blocked, why, sizeof why) && blocked == 1);
  assert(!netjail_new("rtsp://10.1.2.3/x", NJ_RELAY, 5000, &blocked, why, sizeof why) && blocked == 1);
  assert(!netjail_new("rtsp://localhost/x", NJ_RELAY, 5000, &blocked, why, sizeof why) && blocked == 1);
  /* the environment drops every inherited proxy and names the box's */
  setenv("HTTPS_PROXY", "http://corp-proxy:8080", 1);
  setenv("no_proxy", "*", 1);
  j = netjail_new("http://x/", NJ_PROXY, 5000, &blocked, why, sizeof why);
  assert(j);
  int saw_box = 0;
  for (char *const *e = netjail_envp(j); *e; e++) {
    assert(strncmp(*e, "HTTPS_PROXY=", 12) && strncmp(*e, "no_proxy=", 9));
    char b[64];
    snprintf(b, sizeof b, "http_proxy=http://127.0.0.1:%d", j->port);
    if (!strcmp(*e, b)) saw_box = 1;
  }
  assert(saw_box);
  netjail_free(j);
  unsetenv("HTTPS_PROXY");
  unsetenv("no_proxy");
  printf("  plans, rtsp rewrite (v4, v6, credentials kept), refusals, env: ok\n");

  /* 2. the proxy, in-process (no namespace needed: the parent side is the
   *    same code in both modes) */
  static const char okbody[] = "ok";
  fx_add("/ok", okbody, 2);
  fx_start();
  netjail_test_force_no_ns(1);
  hostgate_camera_test_allow_lan(1);
  char resp[8192], req[1024];

  j = netjail_new("http://x/", NJ_PROXY, 8000, &blocked, why, sizeof why);
  assert(j && netjail_parent_start(j) == 0);
  exchange(box_port(j), "GET http://169.254.169.254/latest/meta-data/ HTTP/1.1\r\n"
           "Host: 169.254.169.254\r\n\r\n", resp, sizeof resp);
  assert(!strncmp(resp, "HTTP/1.1 403", 12));
  exchange(box_port(j), "CONNECT 169.254.169.254:443 HTTP/1.1\r\n\r\n", resp, sizeof resp);
  assert(!strncmp(resp, "HTTP/1.1 403", 12));
  exchange(box_port(j), "POST http://93.184.216.34/ HTTP/1.1\r\n\r\n", resp, sizeof resp);
  assert(!strncmp(resp, "HTTP/1.1 405", 12));
  assert(netjail_refused(j) == 2);

  snprintf(req, sizeof req, "GET http://127.0.0.1:%d/ok HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n"
           "Proxy-Connection: keep-alive\r\nConnection: keep-alive\r\n"
           "User-Agent: Lavf\r\n\r\n", fx_port, fx_port);
  exchange(box_port(j), req, resp, sizeof resp);
  assert(!strncmp(resp, "HTTP/1.1 200", 12) && strstr(resp, "\r\n\r\nok"));
  pthread_mutex_lock(&fx_mu);
  assert(!strncmp(fx_last, "GET /ok HTTP/1.1\r\n", 18));
  assert(strstr(fx_last, "User-Agent: Lavf\r\n"));
  assert(strstr(fx_last, "Connection: close\r\n"));
  assert(!strstr(fx_last, "keep-alive") && !strstr(fx_last, "Proxy-"));
  pthread_mutex_unlock(&fx_mu);

  snprintf(req, sizeof req, "CONNECT 127.0.0.1:%d HTTP/1.1\r\n\r\nECHO tunnel\r\n\r\n", fx_port);
  exchange(box_port(j), req, resp, sizeof resp);
  assert(!strncmp(resp, "HTTP/1.1 200 Connection established", 35));
  assert(strstr(resp, "ECHO tunnel"));
  netjail_free(j);
  printf("  proxy: metadata GET/CONNECT 403 and counted, POST 405, allowed GET in "
         "origin form with hop-by-hop headers dropped, CONNECT tunnels: ok\n");

  /* 3. the relay goes to the pinned address whatever the client asks */
  snprintf(req, sizeof req, "rtsp://127.0.0.1:%d/stream", fx_port);
  j = netjail_new(req, NJ_RELAY, 8000, &blocked, why, sizeof why);
  assert(j && netjail_parent_start(j) == 0);
  exchange(box_port(j), "ECHO relay\r\n\r\n", resp, sizeof resp);
  assert(strstr(resp, "ECHO relay"));
  netjail_free(j);
  printf("  relay: bytes reach the pinned target: ok\n");

  /* 4. a real child through the box, in each mode this host supports */
  int have_curl = access("/usr/bin/curl", X_OK) == 0;
  for (int pass = 0; pass < 2; pass++) {
    netjail_test_force_no_ns(pass == 0);
    int ns = strcmp(netjail_mode(), "namespace") == 0;
    if (pass == 1 && !ns) { printf("  SKIP namespace pass: no network namespaces here\n"); break; }
    if (!have_curl) { printf("  SKIP child pass: no /usr/bin/curl\n"); break; }
    char url[128], out[256];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/ok", fx_port);
    j = netjail_new("http://x/", NJ_PROXY, 10000, &blocked, why, sizeof why);
    assert(j);
    const char *via[] = { "/usr/bin/curl", "-s", "--max-time", "5", "-o", "/dev/null",
                          "-w", "%{http_code}", url, NULL };
    run_boxed(j, via, out, sizeof out);
    assert(!strcmp(out, "200"));
    netjail_free(j);

    j = netjail_new("http://x/", NJ_PROXY, 10000, &blocked, why, sizeof why);
    const char *meta[] = { "/usr/bin/curl", "-s", "--max-time", "5", "-o", "/dev/null",
                           "-w", "%{http_code}", "http://169.254.169.254/latest/", NULL };
    run_boxed(j, meta, out, sizeof out);
    assert(!strcmp(out, "403"));
    assert(netjail_refused(j) == 1);
    netjail_free(j);

    if (ns) {
      /* Ignore the proxy and dial the fixture directly: in the namespace
       * there is no route to it — which is what confines an RTSP redirect, a
       * re-resolved name or a tcp:// segment, none of which go through the
       * proxy. */
      j = netjail_new("http://x/", NJ_PROXY, 10000, &blocked, why, sizeof why);
      const char *direct[] = { "/usr/bin/curl", "-s", "--noproxy", "*", "--max-time", "3",
                               "-o", "/dev/null", "-w", "%{http_code}", url, NULL };
      int rc = run_boxed(j, direct, out, sizeof out);
      assert(rc != 0 && strcmp(out, "200") != 0);
      netjail_free(j);
    }
    printf("  child (%s): through the proxy 200, metadata 403%s: ok\n",
           ns ? "namespace" : "proxy-only",
           ns ? ", a direct connection has no route" : "");
  }
  netjail_test_force_no_ns(0);

  /* 5. real ffmpeg decodes an HLS frame through the box */
  if (!ffmpeg_available()) {
    printf("  SKIP ffmpeg pass: no ffmpeg installed\n");
  } else {
    char dir[] = "/tmp/jo-nj-XXXXXX";
    assert(mkdtemp(dir));
    char cmd[512];
    snprintf(cmd, sizeof cmd, "ffmpeg -nostdin -loglevel error -f lavfi -i "
             "color=c=red:s=64x48:d=1:r=5 -f mpegts %s/seg0.ts", dir);
    if (system(cmd) != 0) {
      printf("  SKIP ffmpeg pass: could not generate a fixture segment\n");
    } else {
      char path[600];
      snprintf(path, sizeof path, "%s/seg0.ts", dir);
      FILE *f = fopen(path, "rb");
      static unsigned char seg[1 << 20];
      size_t sl = fread(seg, 1, sizeof seg, f);
      fclose(f);
      static const char pl[] = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:1\n"
                               "#EXTINF:1.0,\nseg0.ts\n#EXT-X-ENDLIST\n";
      fx_add("/live.m3u8", pl, sizeof pl - 1);
      fx_add("/seg0.ts", seg, sl);
      for (int pass = 0; pass < 2; pass++) {
        netjail_test_force_no_ns(pass == 0);
        if (pass == 1 && strcmp(netjail_mode(), "namespace")) break;
        int before = fx_hit_count();
        char url[128], err[512] = "";
        snprintf(url, sizeof url, "http://127.0.0.1:%d/live.m3u8", fx_port);
        unsigned char *jpg = NULL;
        size_t jl = 0;
        int rc = ffmpeg_extract_frame(url, -1.0, 0, 20000, &jpg, &jl, err, sizeof err);
        if (rc != FFMPEG_OK) fprintf(stderr, "ffmpeg rc=%d err=%s\n", rc, err);
        assert(rc == FFMPEG_OK && jl > 4 && jpg[0] == 0xFF && jpg[1] == 0xD8);
        free(jpg);
        /* hls_check's read, then ffmpeg's playlist and segment via the box */
        assert(fx_hit_count() - before >= 3);
        printf("  ffmpeg (%s): an HLS frame decoded through the box: ok\n",
               pass == 0 ? "proxy-only" : "namespace");
      }
      netjail_test_force_no_ns(0);
    }
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) { /* scratch only */ }
  }

  hostgate_camera_test_allow_lan(0);
  __atomic_store_n(&fx_stop, 1, __ATOMIC_RELEASE);
  pthread_join(fx_th, NULL);
  close(fx_lfd);
  printf("test_netjail: OK\n");
  return 0;
}
