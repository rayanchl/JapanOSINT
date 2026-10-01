/* core/ffmpeg.c — the one ffmpeg seam. See core/ffmpeg.h for WHY this module
 * exists, the five security rules it enforces, the wiring and an explicit
 * statement of what is and is not exercised. This file is the mechanics.
 *
 * ── HOW THIS WAS TESTED ───────────────────────────────────────────────────
 *
 * THERE IS NO FFMPEG ON THIS HOST, so nothing here has ever moved a real video
 * frame. Saying "it compiles" would not be a test of a module whose entire job
 * is process plumbing, so the plumbing was driven for real against a STUB: two
 * shell scripts named `ffmpeg` and `ffprobe`, placed first on PATH, which log
 * their argv to a file and then emit whatever the fixture asks for — a JPEG, a
 * truncated JPEG, four megabytes of noise, an ffprobe document, garbage, a
 * non-zero exit with text on stderr, or nothing at all while sleeping past the
 * deadline. That exercises everything in this file except ffmpeg's own
 * behaviour: which argv gets built for which input, that a refused input
 * spawns NO process at all, that the deadline kills and REAPS, that the byte
 * cap kills and reports, that stderr reaches the caller, that ffprobe JSON is
 * parsed and normalised, and that the whole module degrades to one distinct
 * reportable reason when the tool is absent.
 *
 * 216 assertions across 20 groups, 0 failures, built -fsanitize=address,
 * undefined with leak detection on, so an out-of-bounds read or a buffer leaked
 * on an error path is a hard failure rather than a silent one. Each group runs
 * in its OWN forked child: the tool probe is a pthread_once, so a group that
 * needs "nothing on PATH" cannot share a process with one that needs a stub on
 * it, and forking before the parent has touched the once-flag is what makes
 * each group's environment independent.
 *
 *   probe/cache   no ffmpeg on PATH → available()==0, version()==NULL, and
 *                 every entry point returns FFMPEG_ERR_NOT_INSTALLED rather
 *                 than a generic failure. Stub present → available()==1 and
 *                 version() is the stub's first -version line. The probe runs
 *                 ONCE: the argv log shows exactly one -version invocation
 *                 across many calls. JO_NO_FFMPEG (and the CAM_STILLS_NO_FFMPEG
 *                 synonym) → FFMPEG_ERR_DISABLED with the stub still on PATH,
 *                 and the log proves nothing was spawned.
 *   argv          -nostdin, -frames:v 1, -f image2, -vcodec mjpeg and the
 *                 input are present and in order; -protocol_whitelist carries
 *                 the RIGHT list per scheme and NEVER contains `file` for a
 *                 network input (the HLS-playlist file-disclosure case, which
 *                 is the whole reason the lists are disjoint); -rtsp_transport
 *                 tcp appears for rtsp and NOT for http; -ss appears only when
 *                 at_sec >= 0 and carries the right value; -vf scale=W:-2
 *                 appears only when max_width > 0. ffprobe gets NO -nostdin —
 *                 it does not accept the option and would exit 1 on it.
 *   refusal       concat:, data:, subfile,,start,0,end,9,,:, crypto:, pipe:,
 *                 ftp://, gopher://, an input starting with '-', an embedded
 *                 newline, a 5,000-byte URL, "" and NULL → refused with
 *                 FFMPEG_ERR_SCHEME or FFMPEG_ERR_BAD_INPUT, and in every case
 *                 the stub log is byte-identical afterwards, i.e. NO PROCESS
 *                 WAS CREATED. That assertion is the point of the group.
 *   timeout       stub sleeps 30s → returns FFMPEG_ERR_TIMEOUT in under 1.5x
 *                 the deadline, and waitpid(-1) afterwards reports ECHILD, so
 *                 the child was reaped and there is no zombie. Stub that writes
 *                 a COMPLETE JPEG and then sleeps forever → FFMPEG_OK with the
 *                 frame, because -frames:v 1 flushes before it hangs. Stub that
 *                 writes a TRUNCATED JPEG and hangs → FFMPEG_ERR_TIMEOUT and
 *                 the partial bytes are discarded, not returned.
 *   cap           stub emits 4 MiB against a 64 KiB cap → FFMPEG_ERR_TOO_LARGE,
 *                 promptly, child reaped.
 *   exits         exit 1 with "Server returned 401 Unauthorized" on stderr →
 *                 FFMPEG_ERR_EXIT and that text is in the caller's err buffer.
 *                 exit 0 with no output → FFMPEG_ERR_EMPTY.
 *   probe         a hand-written ffprobe document with the tag spellings iOS
 *                 and Android actually write → duration, geometry, fps from
 *                 "30000/1001", codecs, make/model, creation_time and GPS all
 *                 normalised into the "jo" block. Truncated JSON and a plain
 *                 "not json" line → FFMPEG_ERR_PARSE, never a half-populated
 *                 struct.
 *   iso6709       every documented form plus the failure cases — see the block
 *                 comment above ffmpeg_parse_iso6709().
 *   series        duration 10s, count 4 → callback at 1.25/3.75/6.25/8.75 and
 *                 the -ss values in the log match; a callback returning
 *                 non-zero stops early and the count returned is what was
 *                 delivered; a live input (no duration) → FFMPEG_ERR_NO_DURATION
 *                 and no frame exec at all.
 *   gray32        exactly 1024 bytes → accepted; 500 bytes → refused rather
 *                 than hashed as though it were an image, with the expected
 *                 count in the reason. The scaler pinning (-sws_flags,
 *                 scale=32:32, -pix_fmt gray) is asserted in the argv.
 *   ffmpeg_run    argv passed through verbatim, output NUL-terminated one past
 *                 the length, child reaped, and an unexecutable argv[0] →
 *                 FFMPEG_ERR_SPAWN rather than a hang or a false success.
 *
 * NOT TESTED, and no reading of the above should suggest otherwise: whether
 * real ffmpeg accepts these flags, whether they produce a decodable frame, and
 * whether the protocol whitelists are wide enough for real streams. Those need
 * an ffmpeg, and there is not one here.
 */
/* pipe2() is a GNU extension; glibc only declares it under _GNU_SOURCE. The
 * call itself is #ifdef'd to Linux and falls back to pipe()+FD_CLOEXEC, so
 * this only affects which declaration is visible, not which code runs. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "ffmpeg.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Resolved tool paths. 512 is media.c's MEDIA_TOOLPATH_MAX and find_on_path()
 * below uses a scratch buffer of exactly this size, so the copy back out cannot
 * truncate — which is what keeps -Wformat-truncation (a -O2-only diagnostic,
 * invisible to -fsyntax-only) quiet about a real hazard rather than a fake one. */
#define FF_PATH_MAX 512

/* ffprobe documents for a many-stream file run to a few tens of KB; 1 MiB is
 * two orders of headroom and still a bound. */
#define FF_PROBE_MAX_BYTES (1u << 20)

/* Frames one ffmpeg_extract_series() call will ever exec for. A caller asking
 * for 100,000 evenly spaced frames is asking for 100,000 forks; this is an
 * absurdity bound, not a policy. */
#define FF_MAX_SERIES 512

/* ══════════════════════════════════════════════════════════════════════════
 * Small helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static long long ff_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static const char *env_nonempty(const char *k) {
  const char *v = getenv(k);
  return (v && *v) ? v : NULL;
}

/* JO_NO_FFMPEG is the modern spelling; CAM_STILLS_NO_FFMPEG is honoured as an
 * exact synonym because it is documented in camera_stills.h and may already be
 * set in a deployment. Silently ignoring it during this consolidation would
 * turn "RTSP capture is switched off" into "RTSP capture is on" without anyone
 * touching the configuration — the worst class of refactor bug, because it
 * only shows up as traffic on somebody else's camera. */
static int ff_disabled(void) {
  return (getenv("JO_NO_FFMPEG") || getenv("CAM_STILLS_NO_FFMPEG")) ? 1 : 0;
}

static size_t cfg_max_bytes(void) {
  const char *v = env_nonempty("JO_FFMPEG_MAX_BYTES");
  if (!v) return (size_t)FFMPEG_MAX_BYTES;
  long long n = strtoll(v, NULL, 10);
  if (n < 4096) n = 4096;
  if (n > 512LL * 1024 * 1024) n = 512LL * 1024 * 1024;
  return (size_t)n;
}

static int cfg_timeout_ms(void) {
  const char *v = env_nonempty("JO_FFMPEG_TIMEOUT_MS");
  if (!v) return FFMPEG_TIMEOUT_MS;
  long n = strtol(v, NULL, 10);
  if (n < 500) n = 500;
  if (n > 600000) n = 600000;
  return (int)n;
}

const char *ffmpeg_strerror(int rc) {
  switch (rc) {
    case FFMPEG_OK:                return "ok";
    case FFMPEG_ERR_NOT_INSTALLED: return "ffmpeg_not_installed";
    case FFMPEG_ERR_DISABLED:      return "ffmpeg_disabled";
    case FFMPEG_ERR_BAD_INPUT:     return "ffmpeg_bad_input";
    case FFMPEG_ERR_SCHEME:        return "ffmpeg_scheme_refused";
    case FFMPEG_ERR_SPAWN:         return "ffmpeg_spawn_failed";
    case FFMPEG_ERR_TIMEOUT:       return "ffmpeg_timeout";
    case FFMPEG_ERR_TOO_LARGE:     return "ffmpeg_output_too_large";
    case FFMPEG_ERR_EXIT:          return "ffmpeg_exit_nonzero";
    case FFMPEG_ERR_EMPTY:         return "ffmpeg_empty_output";
    case FFMPEG_ERR_PARSE:         return "ffmpeg_parse_failed";
    case FFMPEG_ERR_NO_DURATION:   return "ffmpeg_no_duration";
    case FFMPEG_ERR_OOM:           return "ffmpeg_oom";
    default:                       return "ffmpeg_failed";
  }
}

/* "<token>: <detail>" with the detail flattened to one line and stripped of
 * control bytes. The detail is usually the child's stderr, which is remote
 * text: it ends up in a log, a column and an API body, so a raw 0x1B from a
 * crafted stream has no business surviving. */
static void ff_seterr(char *err, size_t cap, int rc, const char *detail) {
  if (!err || cap == 0) return;
  const char *tok = ffmpeg_strerror(rc);
  size_t o = 0;
  for (const char *p = tok; *p && o + 1 < cap; p++) err[o++] = *p;
  if (detail && *detail && o + 3 < cap) {
    err[o++] = ':';
    err[o++] = ' ';
    int sp = 1;                          /* collapse leading/rep. whitespace */
    for (const unsigned char *p = (const unsigned char *)detail;
         *p && o + 1 < cap; p++) {
      unsigned char c = *p;
      if (c < 0x20 || c == 0x7F) c = ' ';
      if (c == ' ') { if (sp) continue; sp = 1; } else sp = 0;
      err[o++] = (char)c;
    }
    while (o > 0 && err[o - 1] == ' ') o--;
    if (o >= 2 && err[o - 1] == ' ' && err[o - 2] == ':') o -= 2;
  }
  err[o] = '\0';
}

/* ffmpeg wrote a WHOLE image: SOI at the front, EOI at the very back. Enough
 * for output we produced ourselves — this is a single JPEG on a pipe, not the
 * arbitrary multipart body camera_stills_jpeg_frame() has to walk, so the full
 * marker walk would be borrowed complexity and a dependency pointing the wrong
 * way (the generic media module must not include the camera module). */
static int jpeg_complete(const unsigned char *b, size_t n) {
  return b && n >= 4 && b[0] == 0xFF && b[1] == 0xD8 &&
         b[n - 2] == 0xFF && b[n - 1] == 0xD9;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Input validation — security rules 4 and 5. Pure: no I/O, no spawn, and it
 * runs BEFORE any process exists, so a refused input costs a fork of zero.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Every protocol this product will ever hand ffmpeg. rtsps and rtmps are here
 * and are not in the brief's list: camera_stills.c already classifies rtsps://
 * as an RTSP feed, and dropping it during a consolidation would silently
 * disable every TLS camera in the corpus. */
static const char *const FF_SCHEMES[] = {
  "file", "http", "https", "rtsp", "rtsps", "rtmp", "rtmps",
  "hls", "tcp", "udp", NULL
};

/* Extract the scheme, if the input has one. Returns 1 with `out` set to the
 * lowercased scheme, 0 when the input is a plain local path, -1 when there is
 * something colon-shaped in front of the first '/' that is NOT a well-formed
 * scheme — `subfile,,start,0,end,9,,:` is the shape that matters, and it is
 * refused rather than guessed at. */
static int scheme_of(const char *in, char *out, size_t cap) {
  size_t i = 0;
  for (; in[i] && in[i] != '/'; i++) {
    if (in[i] == ':') break;
  }
  if (in[i] != ':') return 0;                 /* no colon before a '/' → path */
  if (i == 0 || i + 1 >= cap) return -1;
  if (!isalpha((unsigned char)in[0])) return -1;
  for (size_t k = 0; k < i; k++) {
    unsigned char c = (unsigned char)in[k];
    if (!isalnum(c) && c != '+' && c != '-' && c != '.') return -1;
    out[k] = (char)tolower(c);
  }
  out[i] = '\0';
  return 1;
}

static int scheme_allowed(const char *s) {
  for (int i = 0; FF_SCHEMES[i]; i++)
    if (strcmp(FF_SCHEMES[i], s) == 0) return 1;
  return 0;
}

int ffmpeg_input_allowed(const char *input) {
  if (!input || !*input) return FFMPEG_ERR_BAD_INPUT;
  /* A leading '-' would be parsed by ffmpeg as an OPTION, not a filename, which
   * is how "decode this file" becomes "run with these flags". */
  if (input[0] == '-') return FFMPEG_ERR_BAD_INPUT;
  size_t n = strlen(input);

  char sc[32];
  int k = scheme_of(input, sc, sizeof sc);
  if (k < 0) return FFMPEG_ERR_SCHEME;
  if (k == 1) {
    if (!scheme_allowed(sc)) return FFMPEG_ERR_SCHEME;
    if (n > 2048) return FFMPEG_ERR_BAD_INPUT;
    /* Same stance camera_stills.c has always taken for a feed URL: printable
     * ASCII only. A URL with a space, a control byte or a newline in it is not
     * a URL, it is an attempt at something. */
    for (const unsigned char *p = (const unsigned char *)input; *p; p++)
      if (*p < 0x21 || *p > 0x7E) return FFMPEG_ERR_BAD_INPUT;
    return FFMPEG_OK;
  }

  /* Local path. Spaces are legal in a filename so they are allowed; control
   * bytes and newlines are not, and ".." components are refused even though
   * every path this module is handed today is generated (an evidence blob path
   * or an mkstemp() name) — the day one is not, this is the check that was
   * already there. */
  if (n > 4096) return FFMPEG_ERR_BAD_INPUT;
  for (const unsigned char *p = (const unsigned char *)input; *p; p++)
    if (*p < 0x20 || *p == 0x7F) return FFMPEG_ERR_BAD_INPUT;
  if (strcmp(input, "..") == 0 || strncmp(input, "../", 3) == 0 ||
      strstr(input, "/../") || (n >= 3 && strcmp(input + n - 3, "/..") == 0))
    return FFMPEG_ERR_BAD_INPUT;
  return FFMPEG_OK;
}

/* The -protocol_whitelist value for an input, and the single most important
 * four lines in this file.
 *
 * THE NETWORK LISTS DO NOT CONTAIN `file`, AND THAT IS DELIBERATE. An HLS
 * playlist is a list of URLs the REMOTE SERVER supplies and ffmpeg will follow
 * them; with `file` on the whitelist a hostile .m3u8 can name /etc/shadow as a
 * segment and this host will dutifully decode its own filesystem into the frame
 * we then store as evidence. The same argument applies to any redirect-capable
 * demuxer. Conversely the local list contains no network protocol, so a local
 * playlist cannot be turned into an outbound request. The two sets are disjoint
 * by construction, which is a property you can check by reading them.
 *
 * The option is placed BEFORE -i, so it binds to the INPUT context only; the
 * "-" output goes through the pipe protocol and is unaffected. */
static const char *whitelist_for(const char *scheme) {
  if (!scheme || !*scheme || strcmp(scheme, "file") == 0) return "file";
  if (strcmp(scheme, "http") == 0 || strcmp(scheme, "https") == 0 ||
      strcmp(scheme, "hls") == 0)
    return "http,https,tcp,tls,crypto";
  if (strcmp(scheme, "rtsp") == 0 || strcmp(scheme, "rtsps") == 0)
    return "rtsp,rtsps,rtp,udp,tcp,tls,crypto";
  if (strcmp(scheme, "rtmp") == 0 || strcmp(scheme, "rtmps") == 0)
    return "rtmp,rtmps,tcp,tls";
  if (strcmp(scheme, "tcp") == 0) return "tcp";
  if (strcmp(scheme, "udp") == 0) return "udp";
  return "file";
}

/* Convenience: validate and classify in one step. */
static int classify(const char *input, char *sc, size_t cap, const char **wl) {
  int rc = ffmpeg_input_allowed(input);
  if (rc != FFMPEG_OK) return rc;
  sc[0] = '\0';
  if (scheme_of(input, sc, cap) != 1) sc[0] = '\0';
  *wl = whitelist_for(sc);
  return FFMPEG_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * The one process-spawning primitive.
 *
 * fork + execvp with an argv ARRAY. No shell exists on this path, so a URL
 * containing `;`, backticks or $( ) is one opaque argv element with nothing to
 * interpret it. Between fork() and execvp() only async-signal-safe calls are
 * made — mandatory in a process with this many threads, where a malloc lock
 * held by another thread at the instant of fork is a deadlocked child.
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
  unsigned char *buf;        /* stdout; owned by the caller on EVERY path    */
  size_t len;
  int exit_code;             /* WEXITSTATUS, or -1 when signalled            */
  int signalled;
  int killed;                /* WE killed it (deadline or cap)               */
  int capped;                /* killed specifically for exceeding max_bytes  */
  char err[FFMPEG_ERRBUF_MAX];   /* first bytes of the child's stderr        */
} ff_result;

/* pipe() with O_CLOEXEC on both ends.
 *
 * WHY THIS IS NOT PEDANTRY. The plain pipe() this replaces leaked its write
 * ends into every process forked by any other thread in the window between the
 * pipe and this function's own fork — in particular media.c's popen(), which
 * runs on a different worker. That child holds the write end open, so this
 * function never sees EOF, waits out its deadline and SIGKILLs a perfectly
 * healthy ffmpeg. Lose that race once inside probe_tools() and the tool is
 * recorded as absent (see ensure_probed for the second half of the fix).
 *
 * The dup2()s in the child intentionally clear O_CLOEXEC on the descriptor
 * they produce, so the child's own stdout/stderr still cross the exec.
 *
 * pipe2 is Linux; the fcntl pair is the portable fallback. It is not fully
 * race-free (another thread can fork between the pipe and the fcntl) but the
 * window is two syscalls instead of the whole spawn, and macOS has no
 * atomic equivalent. */
static int ff_pipe_cloexec(int fd[2]) {
#if defined(__linux__) && defined(O_CLOEXEC)
  if (pipe2(fd, O_CLOEXEC) == 0) return 0;
  if (errno != ENOSYS) return -1;      /* a real failure, not "no pipe2 here" */
#endif
  if (pipe(fd) != 0) return -1;
  for (int i = 0; i < 2; i++) {
    int fl = fcntl(fd[i], F_GETFD);
    if (fl < 0 || fcntl(fd[i], F_SETFD, fl | FD_CLOEXEC) < 0) {
      close(fd[0]); close(fd[1]);
      return -1;
    }
  }
  return 0;
}

static int ff_spawn(const char *const argv[], int timeout_ms, size_t max_bytes,
                    ff_result *r) {
  memset(r, 0, sizeof *r);
  if (!argv || !argv[0]) return FFMPEG_ERR_BAD_INPUT;
  if (max_bytes == 0) max_bytes = cfg_max_bytes();
  if (timeout_ms <= 0) timeout_ms = cfg_timeout_ms();

  int op[2], ep[2];
  if (ff_pipe_cloexec(op) != 0) return FFMPEG_ERR_SPAWN;
  if (ff_pipe_cloexec(ep) != 0) { close(op[0]); close(op[1]); return FFMPEG_ERR_SPAWN; }

  pid_t pid = fork();
  if (pid < 0) {
    close(op[0]); close(op[1]); close(ep[0]); close(ep[1]);
    return FFMPEG_ERR_SPAWN;
  }
  if (pid == 0) {
    close(op[0]);
    close(ep[0]);
    /* stdin from /dev/null. -nostdin is also passed to ffmpeg, but ffprobe does
     * not accept that option, and this redirect covers BOTH tools and anything
     * else ffmpeg_run() is ever pointed at. A child that inherits the server's
     * stdin and blocks reading it is a genuinely baffling hang. */
    int dn = open("/dev/null", O_RDONLY);
    if (dn >= 0) { dup2(dn, 0); if (dn > 2) close(dn); }
    dup2(op[1], 1);
    dup2(ep[1], 2);
    if (op[1] > 2) close(op[1]);
    if (ep[1] > 2) close(ep[1]);
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  }
  close(op[1]);
  close(ep[1]);

  int fds[2] = { op[0], ep[0] };
  int live = 2;
  unsigned char *buf = NULL;
  size_t len = 0, cap = 0, elen = 0;
  long long deadline = ff_now_ms() + timeout_ms;
  int killed = 0, capped = 0, oom = 0;
  unsigned char chunk[65536];

  while (live > 0) {
    long long left = deadline - ff_now_ms();
    if (left <= 0) { kill(pid, SIGKILL); killed = 1; break; }

    struct pollfd pf[2];
    int map[2], np = 0;
    for (int i = 0; i < 2; i++) {
      if (fds[i] < 0) continue;
      pf[np].fd = fds[i]; pf[np].events = POLLIN; pf[np].revents = 0;
      map[np] = i; np++;
    }
    int pr = poll(pf, (nfds_t)np, (int)(left > 500 ? 500 : left));
    if (pr < 0) { if (errno == EINTR) continue; break; }
    if (pr == 0) continue;

    for (int k = 0; k < np; k++) {
      if (pf[k].revents == 0) continue;
      int i = map[k];
      ssize_t got = read(fds[i], chunk, sizeof chunk);
      if (got < 0) {
        if (errno == EINTR || errno == EAGAIN) continue;
        close(fds[i]); fds[i] = -1; live--; continue;
      }
      if (got == 0) { close(fds[i]); fds[i] = -1; live--; continue; }

      if (i == 1) {                          /* stderr: keep the FIRST 2 KiB */
        size_t room = sizeof r->err - 1 - elen;
        if (room) {
          size_t take = (size_t)got < room ? (size_t)got : room;
          memcpy(r->err + elen, chunk, take);
          elen += take;
          r->err[elen] = '\0';
        }
        continue;
      }

      if (len + (size_t)got > max_bytes) {    /* rule 3: bounded output */
        capped = 1; kill(pid, SIGKILL); killed = 1;
        break;
      }
      if (len + (size_t)got + 1 > cap) {
        size_t nc = cap ? cap * 2 : 65536;
        while (nc < len + (size_t)got + 1) nc *= 2;
        if (nc > max_bytes + 1) nc = max_bytes + 1;
        unsigned char *q = realloc(buf, nc);
        if (!q) { oom = 1; kill(pid, SIGKILL); killed = 1; break; }
        buf = q; cap = nc;
      }
      memcpy(buf + len, chunk, (size_t)got);
      len += (size_t)got;
      buf[len] = '\0';                       /* convenience for text callers */
    }
    if (capped || oom) break;
  }
  for (int i = 0; i < 2; i++) if (fds[i] >= 0) close(fds[i]);

  /* Reap. Unconditionally, on every path: a leaked zombie per failed capture
   * is a slow process-table leak in a server that runs for months. While the
   * child may still be alive we poll with WNOHANG so the deadline stays
   * enforceable; once it has been SIGKILLed the wait is blocking, because
   * SIGKILL is not refusable and there is nothing left to wait out. */
  int st = 0, reaped = 0;
  for (;;) {
    pid_t w = waitpid(pid, &st, killed ? 0 : WNOHANG);
    if (w == pid) { reaped = 1; break; }
    if (w < 0) { if (errno == EINTR) continue; break; }
    if (ff_now_ms() >= deadline) { kill(pid, SIGKILL); killed = 1; continue; }
    struct timespec ts = { 0, 2 * 1000 * 1000 };
    nanosleep(&ts, NULL);
  }

  r->buf = buf;
  r->len = len;
  r->killed = killed;
  r->capped = capped;
  if (reaped && WIFEXITED(st)) {
    r->exit_code = WEXITSTATUS(st);
  } else {
    r->signalled = 1;
    r->exit_code = -1;
  }

  if (oom)     return FFMPEG_ERR_OOM;
  if (capped)  return FFMPEG_ERR_TOO_LARGE;
  if (killed)  return FFMPEG_ERR_TIMEOUT;
  if (!reaped) return FFMPEG_ERR_SPAWN;
  if (r->signalled) return FFMPEG_ERR_EXIT;
  /* 127 with nothing on stdout is execvp()'s failure convention. The tool path
   * was access(X_OK)-checked before we got here, so this is a genuine "could
   * not run it" and not the tool's own exit code. */
  if (r->exit_code == 127 && len == 0) return FFMPEG_ERR_SPAWN;
  if (r->exit_code != 0) return FFMPEG_ERR_EXIT;
  if (len == 0) return FFMPEG_ERR_EMPTY;
  return FFMPEG_OK;
}

int ffmpeg_run(const char *const argv[], int timeout_ms, size_t max_bytes,
               unsigned char **out, size_t *out_len, char *err, size_t errcap) {
  if (err && errcap) err[0] = '\0';
  if (!argv || !argv[0] || !out || !out_len) {
    ff_seterr(err, errcap, FFMPEG_ERR_BAD_INPUT, NULL);
    return FFMPEG_ERR_BAD_INPUT;
  }
  if (ff_disabled()) {
    ff_seterr(err, errcap, FFMPEG_ERR_DISABLED, NULL);
    return FFMPEG_ERR_DISABLED;
  }
  ff_result r;
  int rc = ff_spawn(argv, timeout_ms, max_bytes, &r);
  if (rc != FFMPEG_OK) {
    free(r.buf);
    ff_seterr(err, errcap, rc, r.err);
    return rc;
  }
  *out = r.buf;
  *out_len = r.len;
  return FFMPEG_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Tool discovery. Probed ONCE per process behind a pthread_once: a PATH walk
 * plus a fork per frame on a host with no ffmpeg is a real cost when a pod is
 * iterating cameras, and the answer cannot change under us in any way worth
 * tracking.
 * ══════════════════════════════════════════════════════════════════════════ */

static char g_ff_path[FF_PATH_MAX], g_fp_path[FF_PATH_MAX];
static char g_ff_ver[256], g_fp_ver[256];
static int  g_ff_ok, g_fp_ok;

/* `out` must be FF_PATH_MAX bytes; the scratch buffer is sized to match so the
 * copy back out cannot truncate. A PATH element too long to hold the candidate
 * is SKIPPED rather than silently truncated into a different path that
 * access() might then happily accept. */
static int find_on_path(const char *name, char *out) {
  const char *path = getenv("PATH");
  if (!path || !*path) return 0;
  size_t nlen = strlen(name);
  for (const char *p = path; *p; ) {
    const char *e = strchr(p, ':');
    size_t n = e ? (size_t)(e - p) : strlen(p);
    if (n > 0 && n + nlen + 2 <= FF_PATH_MAX) {
      char cand[FF_PATH_MAX];
      snprintf(cand, sizeof cand, "%.*s/%s", (int)n, p, name);
      if (access(cand, X_OK) == 0) {
        memcpy(out, cand, strlen(cand) + 1);
        return 1;
      }
    }
    if (!e) break;
    p = e + 1;
  }
  return 0;
}

/* First line of `<tool> -version`, which is also the liveness check: a binary
 * that is on PATH but cannot execute (wrong arch, missing shared library) must
 * report "not installed" rather than fail once per frame forever. */
static int tool_version(const char *tool, int is_ffmpeg, char *ver, size_t cap) {
  const char *av[6];
  int n = 0;
  av[n++] = tool;
  if (is_ffmpeg) av[n++] = "-nostdin";     /* ffprobe rejects this option */
  av[n++] = "-hide_banner";
  av[n++] = "-version";
  av[n] = NULL;

  ff_result r;
  int rc = ff_spawn(av, 5000, 65536, &r);
  if (rc != FFMPEG_OK) { free(r.buf); return 0; }
  size_t o = 0;
  for (size_t i = 0; i < r.len && o + 1 < cap; i++) {
    unsigned char c = r.buf[i];
    if (c == '\n' || c == '\r') break;
    ver[o++] = (c < 0x20 || c == 0x7F) ? ' ' : (char)c;
  }
  ver[o] = '\0';
  free(r.buf);
  return o > 0;
}

static void probe_tools(void) {
  if (ff_disabled()) return;
  /* Re-entrant: a retry must not inherit the previous attempt's answers. */
  g_ff_ok = g_fp_ok = 0;
  g_ff_path[0] = g_fp_path[0] = '\0';

  const char *ov = env_nonempty("JO_FFMPEG");
  if (!ov) ov = env_nonempty("CAM_STILLS_FFMPEG");
  if (ov) {
    if (strlen(ov) + 1 <= FF_PATH_MAX && access(ov, X_OK) == 0)
      memcpy(g_ff_path, ov, strlen(ov) + 1);
  } else {
    find_on_path("ffmpeg", g_ff_path);
  }
  if (g_ff_path[0] && tool_version(g_ff_path, 1, g_ff_ver, sizeof g_ff_ver))
    g_ff_ok = 1;
  else
    g_ff_path[0] = '\0';

  const char *po = env_nonempty("JO_FFPROBE");
  if (po) {
    if (strlen(po) + 1 <= FF_PATH_MAX && access(po, X_OK) == 0)
      memcpy(g_fp_path, po, strlen(po) + 1);
  } else if (!find_on_path("ffprobe", g_fp_path) && g_ff_path[0]) {
    /* Not on PATH but ffmpeg was: try the sibling. A tarball build installed
     * outside PATH is the common case, and the two binaries ship together. */
    char sib[FF_PATH_MAX];
    snprintf(sib, sizeof sib, "%s", g_ff_path);
    char *slash = strrchr(sib, '/');
    char *base = slash ? slash + 1 : sib;
    if (strcmp(base, "ffmpeg") == 0 &&
        (size_t)(base - sib) + strlen("ffprobe") + 1 <= FF_PATH_MAX) {
      memcpy(base, "ffprobe", strlen("ffprobe") + 1);
      if (access(sib, X_OK) == 0) memcpy(g_fp_path, sib, strlen(sib) + 1);
    }
  }
  if (g_fp_path[0] && tool_version(g_fp_path, 0, g_fp_ver, sizeof g_fp_ver))
    g_fp_ok = 1;
  else
    g_fp_path[0] = '\0';
}

/* A FAILED probe is retried; a successful one is still probed exactly once.
 *
 * This was a pthread_once, which made "ffmpeg is not installed" permanent for
 * the process lifetime. The probe forks ffmpeg -version, so it can fail for
 * reasons that have nothing to do with whether ffmpeg exists — a transient
 * fork failure, or (before ff_pipe_cloexec above) another thread's popen()
 * holding the read pipe open until the 5 s deadline killed the child. Losing
 * that race once meant every camera reported rtsp_requires_ffmpeg forever,
 * and only a restart could clear it. Now a negative answer expires.
 *
 * The retry is rate-limited because a genuinely absent ffmpeg is the common
 * case and a PATH walk plus two forks per frame is a real cost when a pod is
 * iterating cameras — which is exactly why the once existed. */
#define FF_PROBE_RETRY_MS 60000

static pthread_mutex_t g_probe_mu = PTHREAD_MUTEX_INITIALIZER;
static int             g_probed;          /* a probe has completed at least once */
static long long       g_probed_at;

/* Probe if due, then hand back a SNAPSHOT of the two flags taken while the
 * lock is still held.
 *
 * The accessors below used to call ensure_probed() and then read g_ff_ok /
 * g_fp_ok after it had unlocked. That was safe when this was a pthread_once —
 * once() gives every later caller a happens-before edge to the single write,
 * and there was never a second write. It stopped being safe the moment a
 * negative probe became retryable: probe_tools() now rewrites both flags
 * (line 622) on any thread, at any 60s boundary, so an unlocked read of them
 * is a plain data race against a concurrent retry. Two pods reach here —
 * core/media.c:921 and core/camera_stills.c:1636 — on separate threads.
 *
 * The g_ff_path / g_ff_ver BUFFERS are still handed out unlocked, and that is
 * deliberate rather than an oversight: probe_tools() only rewrites them while
 * the retry condition holds, which requires BOTH flags to be 0 — and with
 * both flags 0 every accessor below returns NULL instead of the buffer. So no
 * caller can hold a pointer into a buffer that is about to be rewritten. */
static void probe_snapshot(int *ff_ok, int *fp_ok) {
  pthread_mutex_lock(&g_probe_mu);
  if (!g_probed ||
      (!g_ff_ok && !g_fp_ok && ff_now_ms() - g_probed_at >= FF_PROBE_RETRY_MS)) {
    probe_tools();
    g_probed = 1;
    g_probed_at = ff_now_ms();
  }
  if (ff_ok) *ff_ok = g_ff_ok;
  if (fp_ok) *fp_ok = g_fp_ok;
  pthread_mutex_unlock(&g_probe_mu);
}

int ffmpeg_available(void)  { int a; probe_snapshot(&a, NULL); return a; }
int ffprobe_available(void) { int b; probe_snapshot(NULL, &b); return b; }
const char *ffmpeg_version(void)  { int a; probe_snapshot(&a, NULL); return a ? g_ff_ver : NULL; }
const char *ffprobe_version(void) { int b; probe_snapshot(NULL, &b); return b ? g_fp_ver : NULL; }
const char *ffmpeg_path(void)  { int a; probe_snapshot(&a, NULL); return a ? g_ff_path : NULL; }
const char *ffprobe_path(void) { int b; probe_snapshot(NULL, &b); return b ? g_fp_path : NULL; }

/* The gate every entry point opens with. Distinguishing DISABLED from
 * NOT_INSTALLED is not pedantry: one is a configuration an operator chose and
 * the other is a package they can install, and reporting either as "capture
 * failed" sends them to debug a camera that was never broken. */
static int gate(int need_probe, char *err, size_t errcap) {
  if (ff_disabled()) {
    ff_seterr(err, errcap, FFMPEG_ERR_DISABLED, NULL);
    return FFMPEG_ERR_DISABLED;
  }
  int ff_ok, fp_ok;
  probe_snapshot(&ff_ok, &fp_ok);
  if (need_probe ? !fp_ok : !ff_ok) {
    ff_seterr(err, errcap, FFMPEG_ERR_NOT_INSTALLED,
              need_probe ? "ffprobe" : "ffmpeg");
    return FFMPEG_ERR_NOT_INSTALLED;
  }
  return FFMPEG_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Frames
 * ══════════════════════════════════════════════════════════════════════════ */

int ffmpeg_extract_frame_capped(const char *input, double at_sec, int max_width,
                                int timeout_ms, size_t max_bytes,
                                unsigned char **out, size_t *out_len,
                                char *err, size_t errcap) {
  if (err && errcap) err[0] = '\0';
  if (!out || !out_len) {
    ff_seterr(err, errcap, FFMPEG_ERR_BAD_INPUT, NULL);
    return FFMPEG_ERR_BAD_INPUT;
  }
  char sc[32];
  const char *wl = NULL;
  int rc = classify(input, sc, sizeof sc, &wl);
  if (rc != FFMPEG_OK) { ff_seterr(err, errcap, rc, input); return rc; }
  rc = gate(0, err, errcap);
  if (rc != FFMPEG_OK) return rc;

  /* %.3f of a double needs at most sign + 309 integer digits + '.' + 3, so a
   * 400-byte buffer is provably wide enough and -Wformat-truncation (a -O2-only
   * diagnostic) has nothing to say about it. Clamping first keeps the value
   * sane as well as the buffer. */
  char sbuf[400];
  char vbuf[48];
  if (at_sec < 0 || !(at_sec == at_sec)) at_sec = -1.0;   /* NaN → "first" */
  if (at_sec > 1e6) at_sec = 1e6;
  snprintf(sbuf, sizeof sbuf, "%.3f", at_sec);
  if (max_width > 0) {
    if (max_width > 16384) max_width = 16384;
    /* -2 rather than -1: it keeps the derived height EVEN, which several
     * encoders require and which -1 does not guarantee. */
    snprintf(vbuf, sizeof vbuf, "scale=%d:-2", max_width);
  }

  const char *av[28];
  int n = 0;
  av[n++] = g_ff_path;
  av[n++] = "-nostdin";
  av[n++] = "-hide_banner";
  av[n++] = "-loglevel";  av[n++] = "error";
  av[n++] = "-protocol_whitelist"; av[n++] = wl;
  /* -rtsp_transport is a PRIVATE option of the RTSP demuxer; passing it for an
   * HLS or file input makes ffmpeg exit immediately with "Option not found".
   * TCP because UDP interleaving through NAT is how a camera connects and then
   * never delivers a frame. */
  if (strcmp(sc, "rtsp") == 0 || strcmp(sc, "rtsps") == 0) {
    av[n++] = "-rtsp_transport"; av[n++] = "tcp";
  }
  /* -ss BEFORE -i is input seeking: fast and keyframe-accurate rather than
   * exact. For a still out of a 40-minute recording that is the right trade;
   * output seeking would decode every frame up to the mark. */
  if (at_sec >= 0) { av[n++] = "-ss"; av[n++] = sbuf; }
  av[n++] = "-i"; av[n++] = input;
  av[n++] = "-frames:v"; av[n++] = "1";
  if (max_width > 0) { av[n++] = "-vf"; av[n++] = vbuf; }
  av[n++] = "-an";                  /* the image2 muxer takes no audio       */
  av[n++] = "-sn";                  /* …nor subtitles                        */
  av[n++] = "-f"; av[n++] = "image2";
  av[n++] = "-vcodec"; av[n++] = "mjpeg";
  av[n++] = "-";
  av[n] = NULL;

  ff_result r;
  rc = ff_spawn(av, timeout_ms, max_bytes, &r);

  /* A child we KILLED that had already written a complete JPEG is a SUCCESS:
   * -frames:v 1 flushes the whole image before it exits, and a demuxer that
   * then hangs on the next packet is the camera's problem, not evidence that
   * the frame is bad. A killed child with TRUNCATED output is not rescued —
   * half an image stored as though it were whole is worse than nothing,
   * because every consumer downstream (the hash, the scene comparison, the
   * map) treats it as real. */
  if (rc == FFMPEG_ERR_TIMEOUT && jpeg_complete(r.buf, r.len)) rc = FFMPEG_OK;

  if (rc != FFMPEG_OK) {
    free(r.buf);
    ff_seterr(err, errcap, rc, r.err);
    return rc;
  }
  *out = r.buf;
  *out_len = r.len;
  return FFMPEG_OK;
}

int ffmpeg_extract_frame(const char *input, double at_sec, int max_width,
                         int timeout_ms, unsigned char **out, size_t *out_len,
                         char *err, size_t errcap) {
  return ffmpeg_extract_frame_capped(input, at_sec, max_width, timeout_ms, 0,
                                     out, out_len, err, errcap);
}

int ffmpeg_extract_series(const char *input, int count, int max_width,
                          int timeout_ms, ffmpeg_frame_cb cb, void *ctx,
                          char *err, size_t errcap) {
  if (err && errcap) err[0] = '\0';
  if (!cb || count <= 0) {
    ff_seterr(err, errcap, FFMPEG_ERR_BAD_INPUT, NULL);
    return FFMPEG_ERR_BAD_INPUT;
  }
  if (count > FF_MAX_SERIES) count = FF_MAX_SERIES;
  char sc[32];
  const char *wl = NULL;
  int rc = classify(input, sc, sizeof sc, &wl);
  if (rc != FFMPEG_OK) { ff_seterr(err, errcap, rc, input); return rc; }
  if (timeout_ms <= 0) timeout_ms = cfg_timeout_ms();

  long long deadline = ff_now_ms() + timeout_ms;

  /* The probe is charged to the same budget — a quarter of it, capped at 10s.
   * "Evenly spaced" is meaningless without a duration, so this is not optional
   * work that can be skipped when ffprobe is missing. */
  int pbudget = timeout_ms / 4;
  if (pbudget > 10000) pbudget = 10000;
  if (pbudget < 1000) pbudget = 1000;

  ffmpeg_media_info mi;
  rc = ffmpeg_info(input, pbudget, &mi, err, errcap);
  if (rc != FFMPEG_OK) return rc;
  if (!(mi.duration_sec > 0.0)) {
    ff_seterr(err, errcap, FFMPEG_ERR_NO_DURATION, input);
    return FFMPEG_ERR_NO_DURATION;
  }

  int delivered = 0, last_rc = FFMPEG_OK;
  for (int i = 0; i < count; i++) {
    long long left = deadline - ff_now_ms();
    if (left <= 0) break;
    int remaining = count - i;
    int per = (int)(left / remaining);
    if (per < 1000) per = 1000;           /* a 200ms slice cannot decode      */

    /* MIDPOINT of slice i, not its start: sampling at 0 lands on a fade-in or
     * a black leader on a large fraction of real recordings, and sampling at
     * `duration` lands past the last frame. */
    double at = mi.duration_sec * ((double)i + 0.5) / (double)count;

    unsigned char *buf = NULL;
    size_t len = 0;
    int frc = ffmpeg_extract_frame_capped(input, at, max_width, per, 0,
                                          &buf, &len, err, errcap);
    if (frc != FFMPEG_OK) {
      /* One unseekable position must not take the whole series down. The reason
       * is remembered so a series that delivered NOTHING can report why rather
       * than looking like an empty video. */
      last_rc = frc;
      continue;
    }
    int stop = cb(i, at, buf, len, ctx);
    free(buf);
    delivered++;
    if (stop) break;
  }

  if (delivered == 0 && last_rc != FFMPEG_OK) return last_rc;
  if (err && errcap) err[0] = '\0';
  return delivered;
}

/* ══════════════════════════════════════════════════════════════════════════
 * ISO 6709 — where a video says it was shot.
 *
 * The forms differ ONLY in how many digits sit in front of the decimal point,
 * which is how the standard encodes the units and is why this cannot be done
 * by magnitude: "+1394442.40" is 139°44'42.40"E, and any parser that reads it
 * as a number of degrees puts the pin somewhere off the coast of nowhere.
 *   latitude    2 int digits = DD.DDDD, 4 = DDMM.MMMM, 6 = DDMMSS.SS
 *   longitude   3 int digits = DDD.DDDD, 5 = DDDMM.MMMM, 7 = DDDMMSS.SS
 * 1..3 digits are also accepted as plain degrees: the standard requires zero
 * padding, plenty of writers do not, and "+9.5" is unambiguous. The counts that
 * ARE ambiguous (5 for a latitude, 4 or 6 for a longitude) are refused rather
 * than guessed — a coordinate that might be wrong is worse than none.
 * ══════════════════════════════════════════════════════════════════════════ */

static int iso_deg(const char *f, int is_lon, double *out) {
  int neg = (f[0] == '-');
  const char *d = f + 1;
  const char *dot = strchr(d, '.');
  size_t ip = dot ? (size_t)(dot - d) : strlen(d);
  if (ip == 0) return 0;
  double val = strtod(d, NULL);
  double deg, dd, mm, ss;

  if (!is_lon) {
    if (ip <= 3) deg = val;
    else if (ip == 4) {
      dd = floor(val / 100.0); mm = val - dd * 100.0;
      if (mm >= 60.0) return 0;
      deg = dd + mm / 60.0;
    } else if (ip == 6) {
      dd = floor(val / 10000.0);
      mm = floor((val - dd * 10000.0) / 100.0);
      ss = val - dd * 10000.0 - mm * 100.0;
      if (mm >= 60.0 || ss >= 60.0) return 0;
      deg = dd + mm / 60.0 + ss / 3600.0;
    } else return 0;
  } else {
    if (ip <= 3) deg = val;
    else if (ip == 5) {
      dd = floor(val / 100.0); mm = val - dd * 100.0;
      if (mm >= 60.0) return 0;
      deg = dd + mm / 60.0;
    } else if (ip == 7) {
      dd = floor(val / 10000.0);
      mm = floor((val - dd * 10000.0) / 100.0);
      ss = val - dd * 10000.0 - mm * 100.0;
      if (mm >= 60.0 || ss >= 60.0) return 0;
      deg = dd + mm / 60.0 + ss / 3600.0;
    } else return 0;
  }
  *out = neg ? -deg : deg;
  return 1;
}

int ffmpeg_parse_iso6709(const char *s, double *lat, double *lon, double *alt) {
  if (!s || !lat || !lon) return 0;
  while (*s == ' ' || *s == '\t') s++;

  double la = 0.0, lo = 0.0, al = 0.0;
  int have_alt = 0;

  if (*s == '+' || *s == '-') {
    char f[3][48];
    int nf = 0;
    const char *p = s;
    while (nf < 3 && (*p == '+' || *p == '-')) {
      size_t n = 0;
      f[nf][n++] = *p++;
      while (*p && n + 1 < sizeof f[0] &&
             ((*p >= '0' && *p <= '9') || *p == '.'))
        f[nf][n++] = *p++;
      f[nf][n] = '\0';
      if (n < 2) return 0;                     /* a sign with no digits */
      nf++;
    }
    if (nf < 2) return 0;
    if (!iso_deg(f[0], 0, &la)) return 0;
    if (!iso_deg(f[1], 1, &lo)) return 0;
    if (nf >= 3) { al = strtod(f[2], NULL); have_alt = 1; }
  } else {
    /* "35.6586,139.7454" — not ISO 6709 at all, but several muxers write it
     * into the same tag and refusing it would drop real coordinates. */
    char *e1 = NULL, *e2 = NULL;
    double a = strtod(s, &e1);
    if (!e1 || e1 == s) return 0;
    while (*e1 == ' ') e1++;
    if (*e1 != ',') return 0;
    const char *second = e1 + 1;
    double b = strtod(second, &e2);
    if (!e2 || e2 == second) return 0;
    la = a; lo = b;
  }

  /* Same stance media.c takes for EXIF GPS: out of range is a corrupt tag, and
   * exactly (0,0) is what a device writes when it has NO fix. A confident pin
   * in the Gulf of Guinea is worse than an empty map. */
  if (!(la >= -90.0 && la <= 90.0)) return 0;
  if (!(lo >= -180.0 && lo <= 180.0)) return 0;
  if (la == 0.0 && lo == 0.0) return 0;

  *lat = la;
  *lon = lo;
  if (alt) *alt = have_alt ? al : 0.0;
  return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 * ffprobe
 * ══════════════════════════════════════════════════════════════════════════ */

/* ffprobe emits numbers as JSON STRINGS in some fields (duration, bit_rate) and
 * as numbers in others (width, height), and the mix varies by version. Both are
 * accepted everywhere rather than tracked per field. */
static double jnum(const cJSON *o, const char *k, double dflt) {
  const cJSON *v = o ? cJSON_GetObjectItemCaseSensitive(o, k) : NULL;
  if (!v) return dflt;
  if (cJSON_IsNumber(v)) return v->valuedouble;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e = NULL;
    double d = strtod(v->valuestring, &e);
    if (e && e != v->valuestring) return d;
  }
  return dflt;
}

static const char *jstr(const cJSON *o, const char *k) {
  const cJSON *v = o ? cJSON_GetObjectItemCaseSensitive(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

/* Tag keys are CASE-INSENSITIVE here on purpose: containers written by
 * different tools disagree on "Make" vs "make" vs "MAKE", and a case-sensitive
 * lookup silently loses the camera model on half the corpus. */
static const char *tag_get(const cJSON *tags, const char *name) {
  if (!tags || !cJSON_IsObject(tags)) return NULL;
  for (const cJSON *it = tags->child; it; it = it->next) {
    if (it->string && strcasecmp(it->string, name) == 0 &&
        cJSON_IsString(it) && it->valuestring && it->valuestring[0])
      return it->valuestring;
  }
  return NULL;
}

static const char *tag_first(const cJSON *tags, const char *const *names) {
  for (int i = 0; names[i]; i++) {
    const char *v = tag_get(tags, names[i]);
    if (v) return v;
  }
  return NULL;
}

static void copy_str(char *dst, size_t cap, const char *src) {
  if (!src) { dst[0] = '\0'; return; }
  size_t o = 0;
  for (const unsigned char *p = (const unsigned char *)src; *p && o + 1 < cap; p++)
    dst[o++] = (*p < 0x20 || *p == 0x7F) ? ' ' : (char)*p;
  dst[o] = '\0';
}

/* "30000/1001" → 29.97. Also accepts a bare "25". A denominator of 0 is what
 * ffprobe writes for a stream with no meaningful rate; it must not divide. */
static double parse_rate(const char *r) {
  if (!r || !*r) return 0.0;
  char *e = NULL;
  double num = strtod(r, &e);
  if (!e || e == r) return 0.0;
  if (*e != '/') return num;
  double den = strtod(e + 1, NULL);
  if (!(den > 0.0)) return 0.0;
  return num / den;
}

static void info_from_root(const cJSON *root, ffmpeg_media_info *out) {
  const cJSON *fmt = cJSON_GetObjectItemCaseSensitive(root, "format");
  const cJSON *streams = cJSON_GetObjectItemCaseSensitive(root, "streams");
  const cJSON *ftags = fmt ? cJSON_GetObjectItemCaseSensitive(fmt, "tags") : NULL;

  if (fmt) {
    out->duration_sec = jnum(fmt, "duration", 0.0);
    out->bit_rate = (long long)jnum(fmt, "bit_rate", 0.0);
    copy_str(out->format_name, sizeof out->format_name, jstr(fmt, "format_name"));
  }

  const cJSON *vtags = NULL;
  if (streams && cJSON_IsArray(streams)) {
    for (const cJSON *s = streams->child; s; s = s->next) {
      const char *ct = jstr(s, "codec_type");
      if (!ct) continue;
      if (strcmp(ct, "video") == 0 && out->width == 0) {
        out->width = (int)jnum(s, "width", 0.0);
        out->height = (int)jnum(s, "height", 0.0);
        out->fps = parse_rate(jstr(s, "avg_frame_rate"));
        if (!(out->fps > 0.0)) out->fps = parse_rate(jstr(s, "r_frame_rate"));
        copy_str(out->video_codec, sizeof out->video_codec, jstr(s, "codec_name"));
        vtags = cJSON_GetObjectItemCaseSensitive(s, "tags");
        if (!(out->duration_sec > 0.0)) out->duration_sec = jnum(s, "duration", 0.0);
      } else if (strcmp(ct, "audio") == 0 && out->audio_codec[0] == '\0') {
        copy_str(out->audio_codec, sizeof out->audio_codec, jstr(s, "codec_name"));
      }
    }
  }

  /* Tag spellings, in priority order, as the devices in this corpus actually
   * write them. Apple's namespaced keys first because when both are present the
   * namespaced one is the one the OS wrote and the bare one is whatever an
   * editor left behind. Stream tags are consulted after format tags: a
   * QuickTime file carries creation_time on the track as well as the movie, and
   * a remux frequently keeps only one of the two. */
  static const char *const K_TIME[] = {
    "creation_time", "com.apple.quicktime.creationdate", "date", "date_eng", NULL };
  static const char *const K_LOC[] = {
    "com.apple.quicktime.location.ISO6709", "location", "location-eng",
    "com.apple.quicktime.location.accuracy.horizontal", NULL };
  static const char *const K_MAKE[] = {
    "com.apple.quicktime.make", "make", "manufacturer", NULL };
  static const char *const K_MODEL[] = {
    "com.apple.quicktime.model", "model", NULL };

  const char *t = tag_first(ftags, K_TIME);
  if (!t) t = tag_first(vtags, K_TIME);
  copy_str(out->creation_time, sizeof out->creation_time, t);

  const char *mk = tag_first(ftags, K_MAKE);
  if (!mk) mk = tag_first(vtags, K_MAKE);
  copy_str(out->make, sizeof out->make, mk);

  const char *md = tag_first(ftags, K_MODEL);
  if (!md) md = tag_first(vtags, K_MODEL);
  copy_str(out->model, sizeof out->model, md);

  const char *loc = NULL;
  for (int i = 0; K_LOC[i] && !loc; i++) {
    /* The accuracy key is in the list only so the loop order documents that it
     * is NOT a coordinate; it never yields one. */
    if (strstr(K_LOC[i], "accuracy")) break;
    loc = tag_get(ftags, K_LOC[i]);
    if (!loc) loc = tag_get(vtags, K_LOC[i]);
  }
  if (loc) {
    double la = 0, lo = 0, al = 0;
    if (ffmpeg_parse_iso6709(loc, &la, &lo, &al)) {
      out->have_gps = 1;
      out->lat = la;
      out->lon = lo;
      if (al != 0.0) { out->have_alt = 1; out->altitude_m = al; }
    }
  }
}

static cJSON *probe_root(const char *input, int timeout_ms, char *err,
                         size_t errcap, int *rc_out) {
  if (err && errcap) err[0] = '\0';
  char sc[32];
  const char *wl = NULL;
  int rc = classify(input, sc, sizeof sc, &wl);
  if (rc != FFMPEG_OK) { ff_seterr(err, errcap, rc, input); *rc_out = rc; return NULL; }
  rc = gate(1, err, errcap);
  if (rc != FFMPEG_OK) { *rc_out = rc; return NULL; }

  const char *av[16];
  int n = 0;
  av[n++] = g_fp_path;
  /* NO -nostdin: ffprobe does not define that option and exits 1 on it. The
   * child's stdin is /dev/null anyway (see ff_spawn), which is strictly
   * stronger than the flag. */
  av[n++] = "-hide_banner";
  av[n++] = "-loglevel"; av[n++] = "error";
  av[n++] = "-protocol_whitelist"; av[n++] = wl;
  av[n++] = "-print_format"; av[n++] = "json";
  av[n++] = "-show_format";
  av[n++] = "-show_streams";
  av[n++] = "-i"; av[n++] = input;
  av[n] = NULL;

  ff_result r;
  rc = ff_spawn(av, timeout_ms, FF_PROBE_MAX_BYTES, &r);
  if (rc != FFMPEG_OK) {
    free(r.buf);
    ff_seterr(err, errcap, rc, r.err);
    *rc_out = rc;
    return NULL;
  }

  cJSON *root = cJSON_ParseWithLength((const char *)r.buf, r.len);
  free(r.buf);
  if (!root || !cJSON_IsObject(root) ||
      (!cJSON_GetObjectItemCaseSensitive(root, "format") &&
       !cJSON_GetObjectItemCaseSensitive(root, "streams"))) {
    if (root) cJSON_Delete(root);
    ff_seterr(err, errcap, FFMPEG_ERR_PARSE, "ffprobe output was not a usable "
                                             "format/streams document");
    *rc_out = FFMPEG_ERR_PARSE;
    return NULL;
  }
  *rc_out = FFMPEG_OK;
  return root;
}

int ffmpeg_info(const char *input, int timeout_ms, ffmpeg_media_info *out,
                char *err, size_t errcap) {
  if (!out) {
    ff_seterr(err, errcap, FFMPEG_ERR_BAD_INPUT, NULL);
    return FFMPEG_ERR_BAD_INPUT;
  }
  memset(out, 0, sizeof *out);
  int rc = FFMPEG_OK;
  cJSON *root = probe_root(input, timeout_ms, err, errcap, &rc);
  if (!root) return rc;
  info_from_root(root, out);
  cJSON_Delete(root);
  return FFMPEG_OK;
}

/* The normalised block appended to the raw probe document, so no caller has to
 * re-implement the ISO-6709 grammar or the eleven-tag-spellings question. */
static cJSON *jo_block(const ffmpeg_media_info *mi) {
  cJSON *jo = cJSON_CreateObject();
  if (!jo) return NULL;
  if (mi->duration_sec > 0) cJSON_AddNumberToObject(jo, "duration_sec", mi->duration_sec);
  else                      cJSON_AddNullToObject(jo, "duration_sec");
  if (mi->width)  cJSON_AddNumberToObject(jo, "width", mi->width);
  else            cJSON_AddNullToObject(jo, "width");
  if (mi->height) cJSON_AddNumberToObject(jo, "height", mi->height);
  else            cJSON_AddNullToObject(jo, "height");
  if (mi->fps > 0) cJSON_AddNumberToObject(jo, "fps", mi->fps);
  else             cJSON_AddNullToObject(jo, "fps");
  if (mi->bit_rate) cJSON_AddNumberToObject(jo, "bit_rate", (double)mi->bit_rate);
  else              cJSON_AddNullToObject(jo, "bit_rate");
  if (mi->video_codec[0]) cJSON_AddStringToObject(jo, "video_codec", mi->video_codec);
  else                    cJSON_AddNullToObject(jo, "video_codec");
  if (mi->audio_codec[0]) cJSON_AddStringToObject(jo, "audio_codec", mi->audio_codec);
  else                    cJSON_AddNullToObject(jo, "audio_codec");
  if (mi->format_name[0]) cJSON_AddStringToObject(jo, "format_name", mi->format_name);
  else                    cJSON_AddNullToObject(jo, "format_name");
  if (mi->creation_time[0]) cJSON_AddStringToObject(jo, "creation_time", mi->creation_time);
  else                      cJSON_AddNullToObject(jo, "creation_time");
  if (mi->make[0])  cJSON_AddStringToObject(jo, "make", mi->make);
  else              cJSON_AddNullToObject(jo, "make");
  if (mi->model[0]) cJSON_AddStringToObject(jo, "model", mi->model);
  else              cJSON_AddNullToObject(jo, "model");
  cJSON_AddBoolToObject(jo, "has_gps", mi->have_gps);
  if (mi->have_gps) {
    cJSON_AddNumberToObject(jo, "lat", mi->lat);
    cJSON_AddNumberToObject(jo, "lon", mi->lon);
  } else {
    cJSON_AddNullToObject(jo, "lat");
    cJSON_AddNullToObject(jo, "lon");
  }
  if (mi->have_alt) cJSON_AddNumberToObject(jo, "altitude_m", mi->altitude_m);
  else              cJSON_AddNullToObject(jo, "altitude_m");
  return jo;
}

char *ffmpeg_probe(const char *input, int timeout_ms, char *err, size_t errcap) {
  int rc = FFMPEG_OK;
  cJSON *root = probe_root(input, timeout_ms, err, errcap, &rc);
  if (!root) return NULL;

  ffmpeg_media_info mi;
  memset(&mi, 0, sizeof mi);
  info_from_root(root, &mi);
  cJSON *jo = jo_block(&mi);
  if (jo) {
    cJSON_DeleteItemFromObjectCaseSensitive(root, "jo");   /* never two */
    cJSON_AddItemToObject(root, "jo", jo);
  }
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!out) ff_seterr(err, errcap, FFMPEG_ERR_OOM, NULL);
  return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 32x32 grayscale — the pixels core/media.h's pHash needs and this host has
 * never had. See the pinning note in the header: the scaler and pixel format
 * are fixed so the hash is a property of THIS codebase and stays comparable
 * across deployments with different ffmpeg builds.
 * ══════════════════════════════════════════════════════════════════════════ */

int ffmpeg_gray32(const char *input, int timeout_ms, unsigned char out[1024],
                  char *err, size_t errcap) {
  if (err && errcap) err[0] = '\0';
  if (!out) {
    ff_seterr(err, errcap, FFMPEG_ERR_BAD_INPUT, NULL);
    return FFMPEG_ERR_BAD_INPUT;
  }
  char sc[32];
  const char *wl = NULL;
  int rc = classify(input, sc, sizeof sc, &wl);
  if (rc != FFMPEG_OK) { ff_seterr(err, errcap, rc, input); return rc; }
  rc = gate(0, err, errcap);
  if (rc != FFMPEG_OK) return rc;

  const char *av[24];
  int n = 0;
  av[n++] = g_ff_path;
  av[n++] = "-nostdin";
  av[n++] = "-hide_banner";
  av[n++] = "-loglevel"; av[n++] = "error";
  av[n++] = "-protocol_whitelist"; av[n++] = wl;
  av[n++] = "-i"; av[n++] = input;
  av[n++] = "-frames:v"; av[n++] = "1";
  av[n++] = "-sws_flags"; av[n++] = "bilinear";
  av[n++] = "-vf"; av[n++] = "scale=32:32";
  av[n++] = "-pix_fmt"; av[n++] = "gray";
  av[n++] = "-an"; av[n++] = "-sn";
  av[n++] = "-f"; av[n++] = "rawvideo";
  av[n++] = "-";
  av[n] = NULL;

  ff_result r;
  /* 4 KiB rather than 1 KiB: a multi-frame or wrongly-scaled result must be
   * DETECTED and refused, not silently truncated to the first 1024 bytes and
   * hashed as though it were the image. */
  rc = ff_spawn(av, timeout_ms, 4096, &r);
  if (rc != FFMPEG_OK) {
    free(r.buf);
    ff_seterr(err, errcap, rc, r.err);
    return rc;
  }
  if (r.len != 1024) {
    char d[96];
    snprintf(d, sizeof d, "expected 1024 grayscale bytes, got %lu",
             (unsigned long)r.len);
    free(r.buf);
    ff_seterr(err, errcap, FFMPEG_ERR_EMPTY, d);
    return FFMPEG_ERR_EMPTY;
  }
  memcpy(out, r.buf, 1024);
  free(r.buf);
  return FFMPEG_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Ops
 * ══════════════════════════════════════════════════════════════════════════ */

static void add_tool(cJSON *d, const char *key, int ok, const char *path,
                     const char *ver) {
  cJSON *o = cJSON_CreateObject();
  if (!o) return;
  cJSON_AddBoolToObject(o, "available", ok);
  if (ok && path) cJSON_AddStringToObject(o, "path", path);
  else            cJSON_AddNullToObject(o, "path");
  if (ok && ver)  cJSON_AddStringToObject(o, "version", ver);
  else            cJSON_AddNullToObject(o, "version");
  cJSON_AddItemToObject(d, key, o);
}

char *ffmpeg_capabilities(void) {
  int dis = ff_disabled();
  if (!dis) probe_snapshot(NULL, NULL);   /* probe if due; flags read below */

  cJSON *d = cJSON_CreateObject();
  if (!d) return NULL;
  add_tool(d, "ffmpeg", ffmpeg_available(), ffmpeg_path(), ffmpeg_version());
  add_tool(d, "ffprobe", ffprobe_available(), ffprobe_path(), ffprobe_version());
  cJSON_AddBoolToObject(d, "disabled", dis);

  cJSON *pr = cJSON_CreateArray();
  if (pr) {
    for (int i = 0; FF_SCHEMES[i]; i++)
      cJSON_AddItemToArray(pr, cJSON_CreateString(FF_SCHEMES[i]));
    cJSON_AddItemToObject(d, "protocols", pr);
  }
  cJSON *lim = cJSON_CreateObject();
  if (lim) {
    cJSON_AddNumberToObject(lim, "max_bytes", (double)cfg_max_bytes());
    cJSON_AddNumberToObject(lim, "timeout_ms", cfg_timeout_ms());
    cJSON_AddItemToObject(d, "limits", lim);
  }

  cJSON *env = cJSON_CreateObject();
  if (!env) { cJSON_Delete(d); return NULL; }
  cJSON_AddItemToObject(env, "data", d);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return j;
}
