/* tests/unit/test_hls_check.c — every URL an HLS playlist names is judged by
 * the camera destination policy before ffmpeg can fetch it (core/hls_check.c).
 *
 * WHAT IS BEING PROTECTED. core/ffmpeg.c checked only the INPUT url; the
 * segments, keys and variant playlists a playlist names were fetched by ffmpeg
 * itself with a scheme whitelist and no host check, so a public playlist
 * naming http://169.254.169.254/… made this server request the metadata
 * service. Pinned:
 *   1. relative, absolute-path, scheme-relative and absolute URIs resolve as a
 *      player resolves them;
 *   2. a segment, a key URI or a non-http URI that the policy refuses fails the
 *      whole playlist; public ones pass;
 *   3. against a local server: a master playlist whose MEDIA playlist names
 *      the metadata service is refused (the variant is read and judged), a
 *      clean chain passes, a non-playlist body passes after one read, and an
 *      unreadable playlist is refused (fail closed).
 * The fixture server is on 127.0.0.1, which the camera policy refuses; the
 * test-only seam admits loopback for the playlist fetch and still refuses the
 * metadata range. */
#include "../../core/hls_check.c"

#include <assert.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void resolve_is(const char *base, const char *ref, const char *want) {
  char out[512];
  assert(hls_resolve(base, ref, out, sizeof out) == 0);
  if (strcmp(out, want)) { fprintf(stderr, "resolve %s + %s = %s (want %s)\n",
                                   base, ref, out, want); assert(0); }
}

static int check(const char *base, const char *text) {
  char why[512] = "";
  return hls_check_text(base, text, 0, why, sizeof why);
}

static pid_t g_web;
static void web_stop(void) {
  if (g_web > 0) { kill(g_web, SIGKILL); waitpid(g_web, NULL, 0); g_web = 0; }
}
static int web_start(const char *dir, int port) {
  fflush(stdout);
  g_web = fork();
  if (g_web < 0) return -1;
  if (g_web == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    if (!freopen("/dev/null", "w", stdout)) { /* harmless */ }
    if (!freopen("/dev/null", "w", stderr)) { /* harmless */ }
    execlp("python3", "python3", "-m", "http.server", ps, "--bind", "127.0.0.1",
           "--directory", dir, (char *)NULL);
    _exit(127);
  }
  return 0;
}
static void put(const char *dir, const char *name, const char *body) {
  char path[600];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "w");
  assert(f);
  fputs(body, f);
  fclose(f);
}

int main(void) {
  unsetenv("JO_HTTP_BLOCK_PRIVATE");

  /* 1. resolution */
  resolve_is("http://93.184.216.34/live/cam1/index.m3u8?t=1", "seg-1.ts",
             "http://93.184.216.34/live/cam1/seg-1.ts");
  resolve_is("http://93.184.216.34/live/index.m3u8", "/abs/seg.ts",
             "http://93.184.216.34/abs/seg.ts");
  resolve_is("https://93.184.216.34:8443/a/b.m3u8", "//8.8.8.8/x.ts",
             "https://8.8.8.8/x.ts");
  resolve_is("http://93.184.216.34", "seg.ts", "http://93.184.216.34/seg.ts");
  resolve_is("http://93.184.216.34/a.m3u8", "http://8.8.4.4/b.ts",
             "http://8.8.4.4/b.ts");
  printf("  relative, absolute-path, scheme-relative and absolute URIs resolve: ok\n");

  /* 2. judging the text */
  const char *base = "http://93.184.216.34/live/index.m3u8";
  assert(check(base, "#EXTM3U\n#EXTINF:2,\nseg1.ts\n#EXTINF:2,\nhttp://8.8.8.8/seg2.ts\n") == HLS_OK);
  assert(check(base, "#EXTM3U\n#EXTINF:2,\nhttp://169.254.169.254/latest/x.ts\n") == HLS_BLOCKED);
  assert(check(base, "#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"http://127.0.0.1:8082/k\"\nseg.ts\n") == HLS_BLOCKED);
  assert(check(base, "#EXTM3U\n#EXT-X-MAP:URI=\"http://10.0.0.5/init.mp4\"\nseg.ts\n") == HLS_BLOCKED);
  assert(check(base, "#EXTM3U\nfile:///etc/passwd\n") == HLS_BLOCKED);
  assert(check(base, "#EXTM3U\n#EXTINF:2,\nhttp://localhost/seg.ts\n") == HLS_BLOCKED);
  assert(check(base, "#EXTM3U\r\n#EXTINF:2,\r\n//192.168.1.4/seg.ts\r\n") == HLS_BLOCKED);
  printf("  a refused segment, key, map, scheme or name fails the playlist; public ones pass: ok\n");

  /* 3. the fetch path, against a local server */
  char dir[] = "/tmp/jo-hls-XXXXXX";
  assert(mkdtemp(dir));
  int port = 18741;
  char url[256];
  put(dir, "clean.m3u8", "#EXTM3U\n#EXTINF:2,\nhttp://93.184.216.34/seg1.ts\n");
  put(dir, "evil-media.m3u8", "#EXTM3U\n#EXTINF:2,\nhttp://169.254.169.254/latest/meta-data/\n");
  put(dir, "master.m3u8", "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\nclean.m3u8\n"
                          "#EXT-X-STREAM-INF:BANDWIDTH=2\nevil-media\n");
  put(dir, "evil-media", "#EXTM3U\n#EXTINF:2,\nhttp://169.254.169.254/x.ts\n");
  put(dir, "good-master.m3u8", "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\nclean.m3u8\n");
  put(dir, "frame.jpg", "\xFF\xD8\xFF\xE0 not a playlist, a jpeg");
  if (web_start(dir, port) != 0) { printf("  SKIP fetch path: no python3\n"); return 0; }
  hostgate_camera_test_allow_lan(1);
  int up = 0;
  for (int i = 0; i < 100 && !up; i++) {
    char why[256];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/clean.m3u8", port);
    up = hls_check_input(url, why, sizeof why) == HLS_OK;
    if (!up) usleep(100 * 1000);
  }
  if (!up) { printf("  SKIP fetch path: fixture server did not answer\n"); web_stop(); return 0; }
  char why[512];
  snprintf(url, sizeof url, "http://127.0.0.1:%d/master.m3u8", port);
  assert(hls_check_input(url, why, sizeof why) == HLS_BLOCKED);
  assert(strstr(why, "169.254.169.254"));
  snprintf(url, sizeof url, "http://127.0.0.1:%d/good-master.m3u8", port);
  assert(hls_check_input(url, why, sizeof why) == HLS_OK);
  snprintf(url, sizeof url, "http://127.0.0.1:%d/evil-media.m3u8", port);
  assert(hls_check_input(url, why, sizeof why) == HLS_BLOCKED);
  snprintf(url, sizeof url, "http://127.0.0.1:%d/frame.jpg", port);
  assert(hls_check_input(url, why, sizeof why) == HLS_OK);
  /* A variant that 404s cannot be judged: refused. */
  put(dir, "missing-master.m3u8", "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\nnope.m3u8\n");
  snprintf(url, sizeof url, "http://127.0.0.1:%d/missing-master.m3u8", port);
  assert(hls_check_input(url, why, sizeof why) == HLS_UNREADABLE);
  hostgate_camera_test_allow_lan(0);
  /* Without the seam the loopback fixture itself is refused before any read. */
  snprintf(url, sizeof url, "http://127.0.0.1:%d/clean.m3u8", port);
  assert(hls_check_input(url, why, sizeof why) != HLS_OK);
  web_stop();
  printf("  master -> media naming the metadata service refused, clean chain passes, "
         "a jpeg passes, an unreadable variant is refused: ok\n");
  printf("test_hls_check: OK\n");
  return 0;
}
