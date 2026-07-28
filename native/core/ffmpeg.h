/* core/ffmpeg.h — THE video/audio abstraction. One ffmpeg seam, and only one.
 *
 * ══ WHY THIS EXISTS ═══════════════════════════════════════════════════════
 *
 * Two files in this codebase had grown their own way of talking to an external
 * media tool, and they disagreed on every decision that matters:
 *
 *   core/camera_stills.c  fork + execvp of `ffmpeg` with an argv array, a poll
 *     loop, a SIGKILL deadline and a byte cap — correct, but private, and it
 *     had no protocol allowlist, no ffprobe, no version reporting and no way
 *     for any other module to reuse a line of it.
 *   core/media.c          popen() of ImageMagick / tesseract through /bin/sh,
 *     with a character whitelist standing in for quoting. Safe for the two
 *     kinds of path it actually passes (an evidence content hash and an
 *     mkstemp() name) and NOT safe for anything a third party names.
 *
 * Duplicated process plumbing is the worst thing to duplicate: the day one
 * copy learns to reap its children and the other does not, the bug shows up as
 * a slow leak of zombies under load, in the file nobody was looking at. So
 * ffmpeg becomes FIRST-CLASS here — the single place in the product that spawns
 * a media tool, decodes a frame, or asks what a file is. Every video path calls
 * this module. There is no second one.
 *
 * WHAT THIS BUYS BEYOND DE-DUPLICATION, in rough order of value:
 *
 *   1. GPS OUT OF VIDEO. This is the OSINT deliverable and the main reason
 *      video metadata is worth extracting at all. Every iPhone video carries
 *      `com.apple.quicktime.location.ISO6709` in its moov atom — a hard
 *      coordinate, ground truth rather than inference, exactly like the EXIF
 *      GPS that core/media.c is built around. Android writes the same fact into
 *      the `location` tag. ffprobe surfaces both as format tags;
 *      ffmpeg_parse_iso6709() normalises them to decimal lat/lon, and
 *      ffmpeg_info() hands them back next to creation_time, so a video shared
 *      into a case can place a pin on the map with the same authority a photo
 *      does. Nothing else in the corpus can do this.
 *   2. Video uploads become inspectable. core/uploadapi.h reassembles client
 *      files into the evidence blob store and records a sha256 and a byte
 *      count; for an .mp4 that is everything it knows. ffmpeg_info() over the
 *      committed blob turns that into duration, geometry, codecs, camera
 *      make/model, capture time and a coordinate.
 *   3. Frames become evidence. ffmpeg_extract_frame() / _series() emit JPEG
 *      bytes that go straight into core/evidence.h's content-addressed store —
 *      NOT a new store. Extracted frames are content-addressed and byte-budgeted
 *      exactly like every other captured byte in the product, dedupe against
 *      each other for a static scene, and are reaped by the same reaper. There
 *      is one blob store in this codebase and this module does not add a second.
 *   4. RTSP and HLS cameras. core/camera_stills.c's opt-in scheduled capture
 *      already needed this and now gets it from here.
 *
 * ══ THE ENVIRONMENT THIS WAS WRITTEN IN — STATED UP FRONT ═════════════════
 *
 * THERE IS NO FFMPEG ON THIS HOST AND THERE CANNOT BE (no passwordless sudo,
 * and the .deb dependency closure does not resolve). Nothing in this file has
 * ever run against a real ffmpeg. What HAS been exercised, and how, is spelled
 * out in "WHAT IS AND IS NOT EXERCISED" at the bottom; read it before treating
 * any of this as verified. The design is drawn around the absence rather than
 * over it: every entry point returns a DISTINCT, reportable reason when the
 * tool is missing (FFMPEG_ERR_NOT_INSTALLED, "ffmpeg_not_installed"), never a
 * generic failure, so a caller can render "ffmpeg not installed" instead of
 * "capture failed" — which is the difference between an operator installing a
 * package and an operator debugging a camera that was never broken.
 *
 * ══ SECURITY: THE FIVE RULES THIS MODULE EXISTS TO ENFORCE ════════════════
 *
 * The input to every function here is a URL from a third-party camera registry
 * or the name of a file some analyst uploaded. It is hostile by default.
 *
 * 1. NEVER A SHELL. fork() + execvp() with an argv ARRAY, always. No popen(),
 *    no system(), no /bin/sh anywhere on any path in this file. A URL
 *    containing `;`, backticks or $( ) is one opaque argv element with nothing
 *    to interpret it. Hand-quoting hostile input for a shell is a
 *    command-injection bug with a countdown on it, and the whitelist-the-path
 *    trick core/media.c uses is only available because media.c interpolates
 *    nothing but hex hashes and mkstemp() names.
 *    Between fork() and execvp() only async-signal-safe calls are made
 *    (open/dup2/close/execvp/_exit) — mandatory in a process with this many
 *    threads, where a malloc lock held by another thread at the moment of fork
 *    is a deadlocked child.
 *    `-nostdin` is passed to every invocation as well. Without it a stalled
 *    ffmpeg reads the PARENT's stdin, and a server that has quietly eaten the
 *    terminal it was launched from is a genuinely confusing failure.
 *
 * 2. HARD TIMEOUT WITH SIGKILL, AND THE CHILD IS ALWAYS REAPED. A camera that
 *    completes a TCP handshake and then never sends a frame is the NORMAL
 *    failure for this workload, not the exceptional one, and ffmpeg's own
 *    per-protocol timeout options are private to each demuxer (passing
 *    -rtsp_transport to an HLS input is an immediate "Option not found"), so
 *    they cannot be relied on generically. The deadline here is external and
 *    unconditional: poll() to the deadline, kill(SIGKILL), waitpid() on every
 *    path including every error path. SIGTERM is not tried first — a demuxer
 *    blocked in a socket read will not handle it, and the delay buys nothing.
 *
 * 3. BOUNDED OUTPUT. Bytes read from the child are capped and the child is
 *    killed the moment it exceeds the cap. An unbounded read from a subprocess
 *    fed by a hostile URL is a remote memory-exhaustion primitive: `ffmpeg -i
 *    <url> -f image2 -` against a source that keeps producing frames writes
 *    until something dies. Default FFMPEG_MAX_BYTES (8 MiB), per-call
 *    overridable via ffmpeg_extract_frame_capped().
 *
 * 4. PROTOCOL ALLOWLIST, ENFORCED TWICE. ffmpeg speaks dozens of protocols and
 *    several of them read the local filesystem: `concat:/etc/passwd|…`,
 *    `subfile:`, `crypto:` and friends are file-disclosure primitives dressed
 *    as URLs. So (a) ffmpeg_input_allowed() REFUSES any input whose scheme is
 *    not in {file, http, https, rtsp, rtsps, rtmp, rtmps, hls, tcp, udp}
 *    before a process is ever spawned, and (b) `-protocol_whitelist` is passed
 *    explicitly on the ffmpeg command line so a REDIRECT or a playlist cannot
 *    escalate into one we did not intend.
 *
 *    THE SUBTLE PART, and the reason (b) is not redundant with (a): an HLS
 *    playlist is a list of URLs supplied by the remote server, and ffmpeg will
 *    follow them. A whitelist containing `file` therefore lets a remote .m3u8
 *    name local paths as its segments and stream this host's filesystem back
 *    into the frame we then store as evidence. So the network whitelists in
 *    this file DO NOT CONTAIN `file`, and the local-file whitelist does not
 *    contain any network protocol. They are disjoint by construction; see
 *    whitelist_for() in ffmpeg.c.
 *
 * 5. NO OPTION INJECTION. An input beginning with '-' would be parsed by ffmpeg
 *    as an OPTION rather than a filename, which is how "give me a frame of this
 *    file" becomes "run with these flags". Refused. Control characters and
 *    embedded newlines are refused too, and both URLs and paths are
 *    length-capped.
 *
 * ══ DEGRADING HONESTLY ════════════════════════════════════════════════════
 *
 * Every function returns one of the FFMPEG_ERR_* codes below and fills the
 * caller's `err` buffer, which carries the tool's OWN stderr when there was
 * one — a 401 from a camera, an "Invalid data found when processing input",
 * the actual reason. ffmpeg_strerror() maps a code to a stable snake_case
 * token suitable for storing in a column or rendering in an API:
 *
 *   ffmpeg_not_installed    the tool is not on PATH. NOT a failure of the
 *                           source, and a caller must be able to say so.
 *   ffmpeg_disabled         an operator set JO_NO_FFMPEG. Also not a failure.
 *   ffmpeg_bad_input        empty / over-long / control characters / leading '-'
 *   ffmpeg_scheme_refused   scheme not in the allowlist — see rule 4
 *   ffmpeg_spawn_failed     fork/pipe failed, or exec did
 *   ffmpeg_timeout          deadline hit; child SIGKILLed and reaped
 *   ffmpeg_output_too_large cap exceeded; child SIGKILLed and reaped
 *   ffmpeg_exit_nonzero     the tool ran and refused; `err` has its stderr
 *   ffmpeg_empty_output     exit 0 and nothing on stdout
 *   ffmpeg_parse_failed     ffprobe emitted something that is not usable JSON
 *   ffmpeg_no_duration      a series was asked for over an unbounded live input
 *   ffmpeg_oom
 *
 * The three that are NOT errors of the media — not_installed, disabled and
 * scheme_refused — are deliberately distinct codes rather than one "failed",
 * because they are the three a human can actually act on.
 *
 * ══ SCHEMA ════════════════════════════════════════════════════════════════
 *
 * NONE. This module owns no tables, no columns and no indexes. It has no
 * db_handle in any signature and touches no database. Frames belong in
 * core/evidence.h's blob store and metadata belongs on the caller's own row;
 * a "video" table would be a second place for facts that already have a home.
 * There is nothing for the orchestrator to add to core/schema.sql and nothing
 * for ensure_column().
 *
 * ══ WIRING THE ORCHESTRATOR MUST DO ═══════════════════════════════════════
 *
 * The Makefile globs the core .c files, so linking is automatic. Required
 * wiring: NONE.
 * The module is lazily probed on first use (pthread_once) and every caller
 * degrades cleanly when ffmpeg is absent. What is OPTIONAL and worth doing:
 *
 * 1. core/httpd.c — one ops route, so "why is every video field NULL" is
 *    answerable without an SSH session. Same shape and same stance as the
 *    existing /api/media/capabilities route:
 *
 *      GET /api/ffmpeg/capabilities        (operator/admin)
 *        char *body = ffmpeg_capabilities();
 *        if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
 *        reply_json(c, 200, body); free(body); return;
 *
 * 2. core/camera_stills.c — ALREADY DONE as part of this change. Its private
 *    find_ffmpeg()/run_argv()/ffmpeg_grab() are deleted and it calls
 *    ffmpeg_extract_frame_capped() and ffmpeg_path(). No behaviour change: the
 *    same opt-in gating, the same retention, the same scene_changed semantics
 *    (NULL = "nobody could look", never a lie), and its MJPEG and snapshot
 *    paths are untouched and still need no ffmpeg at all.
 *
 * 3. core/media.c — NOT DONE, deliberately: this task was scoped not to edit
 *    it. What it needs is exposed here and is a drop-in for its two seams,
 *    whenever whoever owns that file wants it:
 *
 *      (a) ffmpeg_run() replaces run_capture()+build_cmd(), deleting the last
 *          popen() in core/. Same contract — bounded stdout, non-zero exit is a
 *          failure — with a real timeout, a reaped child and no /bin/sh, which
 *          in turn makes media.c's path_ok() whitelist unnecessary rather than
 *          load-bearing. media.c currently has NO timeout at all: a wedged
 *          tesseract blocks a scheduler thread forever.
 *      (b) ffmpeg_gray32() replaces the ImageMagick pHash seam outright. It
 *          emits exactly the 1024 bytes of 32x32 8-bit grayscale that
 *          media_phash_gray32() documents as its input, so on a host with
 *          ffmpeg the perceptual-hash column populates with NO ImageMagick, no
 *          schema change and no backfill — and, because media.c computes the
 *          DCT itself, the hashes stay comparable with any already stored.
 *          That also lights up camera_stills' scene-change pHash rung, which
 *          is the difference between scene_changed being mostly NULL and being
 *          the column an analyst actually filters on.
 *      Both are additive; neither changes media.c's behaviour where the
 *      existing tools are present.
 *
 * 4. No core/main.c change, no new thread, no new link dependency (this module
 *    uses cJSON and libc only — no libcurl, no OpenSSL, no sqlite3).
 *
 * ══ ENVIRONMENT ═══════════════════════════════════════════════════════════
 *   JO_FFMPEG              absolute path override; unset → probe PATH
 *   JO_FFPROBE             absolute path override; unset → probe PATH, then
 *                          fall back to `ffprobe` beside the resolved ffmpeg
 *   JO_NO_FFMPEG           set → never spawn anything; every call returns
 *                          FFMPEG_ERR_DISABLED
 *   JO_FFMPEG_MAX_BYTES    default output cap in bytes    (8388608 = 8 MiB)
 *   JO_FFMPEG_TIMEOUT_MS   fallback when a caller passes timeout_ms <= 0 (15000)
 *
 *   CAM_STILLS_FFMPEG / CAM_STILLS_NO_FFMPEG are ALSO honoured, as exact
 *   synonyms for the two above. They are documented in camera_stills.h and may
 *   already be set in a deployment; silently ignoring them during a
 *   consolidation would turn "RTSP is switched off" into "RTSP is on" without
 *   anyone touching the configuration. JO_* wins where both are set.
 *
 * ══ WHAT IS AND IS NOT EXERCISED IN THIS ENVIRONMENT ══════════════════════
 *
 * EXERCISED, against a STUB `ffmpeg`/`ffprobe` — a pair of shell scripts placed
 * first on PATH that log their argv and emit scripted output — plus direct unit
 * tests of the pure functions. That is a real test of everything in this file
 * EXCEPT ffmpeg's own behaviour: the tool probe and its cache, argv
 * construction for every input class, the scheme allowlist (including the
 * assertion that a refused input spawns NO process at all), the SIGKILL
 * deadline, the output cap, child reaping, non-zero exit with stderr capture,
 * ffprobe JSON parsing and normalisation, the ISO-6709 grammar, and the
 * absent-tool path. 216 assertions across 20 groups, 0 failures, under
 * ASan+UBSan with leak detection. The exact case list is in the test note at
 * the top of core/ffmpeg.c.
 *
 * NOT EXERCISED, and it would be dishonest to imply otherwise:
 *   - Every claim about what REAL ffmpeg does with the argv this file builds.
 *     The flags are right as far as the documentation goes; no invocation of
 *     any of them has been observed to produce a frame. If `-frames:v 1 -f
 *     image2 -vcodec mjpeg -` is wrong for some input class, this file will not
 *     have told you.
 *   - Any actual decode. No JPEG has been produced from any video by this code.
 *     The bytes the harness moves are bytes a shell script echoed.
 *   - Any live network source. No RTSP camera, no HLS playlist, no real
 *     redirect, and therefore no evidence that the -protocol_whitelist values
 *     are sufficient for real streams (too NARROW is the plausible failure —
 *     "Protocol not on whitelist" — and it fails closed, which is the right
 *     direction, but it is untested).
 *   - Real ffprobe output. The parser was driven against hand-written JSON
 *     shaped like ffprobe's, including the tag spellings iOS and Android
 *     actually write, but a genuine ffprobe document has never been seen here.
 *   - ffmpeg_gray32() end to end, for the same reason media.c's pHash seam is
 *     unexercised: nothing on this host can turn an image into pixels.
 */
#ifndef JO_FFMPEG_H
#define JO_FFMPEG_H

#include <stddef.h>

/* ── result codes ────────────────────────────────────────────────────────── */

#define FFMPEG_OK                 0
#define FFMPEG_ERR_NOT_INSTALLED (-1)
#define FFMPEG_ERR_DISABLED      (-2)
#define FFMPEG_ERR_BAD_INPUT     (-3)
#define FFMPEG_ERR_SCHEME        (-4)
#define FFMPEG_ERR_SPAWN         (-5)
#define FFMPEG_ERR_TIMEOUT       (-6)
#define FFMPEG_ERR_TOO_LARGE     (-7)
#define FFMPEG_ERR_EXIT          (-8)
#define FFMPEG_ERR_EMPTY         (-9)
#define FFMPEG_ERR_PARSE        (-10)
#define FFMPEG_ERR_NO_DURATION  (-11)
#define FFMPEG_ERR_OOM          (-12)

/* Stable snake_case token for a code — safe to store in a column or return in
 * an API body, and stable across releases. Returns a static string; never
 * NULL, including for an unknown code ("ffmpeg_failed"). */
const char *ffmpeg_strerror(int rc);

/* Default output cap and default timeout. Both env-overridable; see ENVIRONMENT. */
#define FFMPEG_MAX_BYTES  (8u * 1024u * 1024u)
#define FFMPEG_TIMEOUT_MS 15000

/* Bytes of the child's stderr retained for the `err` buffer. The FIRST 2 KiB,
 * not the last: ffmpeg reports the fatal problem and then unwinds, so the head
 * of the stream is the informative part. */
#define FFMPEG_ERRBUF_MAX 2048

/* ── availability ────────────────────────────────────────────────────────── */

/* 1 when an executable ffmpeg was found AND `ffmpeg -version` ran. Probes once
 * per process behind a pthread_once and caches the answer — a PATH walk plus a
 * fork per frame on a host with no ffmpeg is a real cost when a pod is
 * iterating cameras. Thread-safe. */
int ffmpeg_available(void);

/* First line of `ffmpeg -version`, e.g. "ffmpeg version 6.1.1 Copyright …".
 * NULL when ffmpeg is absent or disabled. Static storage, do not free. */
const char *ffmpeg_version(void);

/* Absolute path of the resolved ffmpeg binary, or NULL. Static storage, do not
 * free. Exposed because an ops/capabilities route wants to show WHICH ffmpeg
 * answered — "available: true" is not much help when two are installed. */
const char *ffmpeg_path(void);

/* The same three for ffprobe. ffprobe is probed INDEPENDENTLY: a build can ship
 * one without the other, and reporting "no video metadata" because the frame
 * extractor is missing would be wrong. */
int ffprobe_available(void);
const char *ffprobe_version(void);
const char *ffprobe_path(void);

/* ── input validation (pure; no I/O, no spawn) ───────────────────────────── */

/* FFMPEG_OK when `input` may be handed to the tool, else FFMPEG_ERR_SCHEME or
 * FFMPEG_ERR_BAD_INPUT. Enforces security rules 4 and 5 above: allowlisted
 * scheme or a plain local path, no leading '-', no control bytes, no embedded
 * newline, length-capped, no "/../" traversal in a local path.
 *
 * Called internally by every entry point BEFORE anything is spawned — a refused
 * input never creates a process. Exposed so a caller can pre-screen a URL and
 * report why without paying for a fork. Pure and re-entrant. */
int ffmpeg_input_allowed(const char *input);

/* ── frames ──────────────────────────────────────────────────────────────── */

/* One frame as JPEG bytes.
 *
 *   input      a local path OR a URL (file/http/https/rtsp/rtsps/rtmp/rtmps/
 *              hls/tcp/udp). Validated per ffmpeg_input_allowed() first.
 *   at_sec     < 0 means "the first decodable frame", which is the only
 *              meaningful request for a live source. >= 0 seeks; the seek is
 *              placed BEFORE -i (input seeking), which is the fast, keyframe-
 *              accurate form — it is approximate by design, and for a still
 *              from a 40-minute recording that is the right trade.
 *   max_width  0 = native. >0 scales to that width preserving aspect
 *              (`-vf scale=W:-2`; -2 keeps the height even, which several
 *              encoders require and which -1 does not guarantee).
 *   timeout_ms <=0 → JO_FFMPEG_TIMEOUT_MS. Hard: SIGKILL at the deadline.
 *   out/out_len  on FFMPEG_OK, malloc'd JPEG bytes the CALLER FREES.
 *   err/errcap   may be NULL. On failure receives a human-readable reason,
 *                including the tool's own stderr when there was any.
 *
 * Returns FFMPEG_OK or a negative FFMPEG_ERR_*. *out is untouched on failure.
 *
 * A child that is KILLED at the deadline but has already written a COMPLETE
 * JPEG (SOI…EOI) is a SUCCESS, not a timeout: `-frames:v 1` flushes the whole
 * image before it exits and a demuxer that then hangs on the next packet is the
 * camera's problem, not evidence that the frame is bad. A killed child whose
 * output is truncated is FFMPEG_ERR_TIMEOUT and the partial bytes are
 * discarded — half an image stored as though it were whole is worse than
 * nothing, because everything downstream (hashes, scene comparison, the map)
 * treats it as real. */
int ffmpeg_extract_frame(const char *input, double at_sec, int max_width,
                         int timeout_ms, unsigned char **out, size_t *out_len,
                         char *err, size_t errcap);

/* As above with an explicit output cap (0 → FFMPEG_MAX_BYTES). Exists because
 * a caller with its own per-frame byte ceiling — core/camera_stills.c has
 * CAM_STILLS_MAX_BYTES — must be able to enforce ITS number here rather than
 * discover the overrun two layers up after the bytes are already in RAM. */
int ffmpeg_extract_frame_capped(const char *input, double at_sec, int max_width,
                                int timeout_ms, size_t max_bytes,
                                unsigned char **out, size_t *out_len,
                                char *err, size_t errcap);

/* Called once per extracted frame. `jpeg`/`len` are borrowed for the duration
 * of the call ONLY — copy or write them, do not retain the pointer. Return 0 to
 * continue, non-zero to stop the series cleanly (already-delivered frames
 * count; it is not an error). */
typedef int (*ffmpeg_frame_cb)(int index, double at_sec,
                               const unsigned char *jpeg, size_t len,
                               void *ctx);

/* `count` frames evenly spaced across the media — the timelapse / scene-change
 * path. Frame i is taken at duration*(i+0.5)/count, i.e. at the MIDPOINT of its
 * slice: sampling at 0 lands on a fade-in or a black leader on a large fraction
 * of real recordings, and sampling at `duration` lands past the last frame.
 *
 * Requires a KNOWN DURATION and therefore an ffprobe: a live stream has no
 * "evenly spaced" and asking for it is a caller bug, answered with
 * FFMPEG_ERR_NO_DURATION rather than a guess. Use ffmpeg_extract_frame() with
 * at_sec < 0 for live sources.
 *
 * ONE EXEC PER FRAME, deliberately. A single `-vf fps=…` pass would be cheaper
 * and produces a concatenated MJPEG stream that then has to be split by
 * scanning for frame boundaries; per-frame execs cost more forks and buy exact
 * timestamps, an independent bound on every frame (one unseekable position
 * cannot take the whole series down), and the ability to stop the instant the
 * callback says so. At the batch sizes anything in this product asks for
 * (single digits per item) the fork cost is irrelevant.
 *
 * `timeout_ms` is the budget for the WHOLE call including the probe, not per
 * frame — one number the caller can reason about. It is divided across the
 * frames remaining, with a 1s floor per frame, and the series stops early with
 * whatever it has if the budget runs out.
 *
 * Returns the number of frames DELIVERED to the callback (>= 0), or a negative
 * FFMPEG_ERR_*. A partial series is a success with a smaller number. */
int ffmpeg_extract_series(const char *input, int count, int max_width,
                          int timeout_ms, ffmpeg_frame_cb cb, void *ctx,
                          char *err, size_t errcap);

/* ── probe / metadata ────────────────────────────────────────────────────── */

/* `ffprobe -show_format -show_streams -print_format json`, parsed.
 *
 * The document is PARSED here rather than passed through: a caller that stores
 * or re-serves this must not be handed whatever a subprocess wrote to stdout,
 * and a truncated or non-JSON document is a failure (FFMPEG_ERR_PARSE) rather
 * than something to discover downstream. What comes back is ffprobe's own
 * object with ONE addition — a top-level "jo" block carrying the normalised
 * facts, so no caller has to re-implement the ISO-6709 grammar or the
 * "which of eleven tag spellings holds the coordinate" question:
 *
 *   {"format":{…},"streams":[…],
 *    "jo":{"duration_sec":n|null,"width":n|null,"height":n|null,"fps":n|null,
 *          "video_codec":…|null,"audio_codec":…|null,"format_name":…|null,
 *          "bit_rate":n|null,"creation_time":…|null,"make":…|null,
 *          "model":…|null,"has_gps":bool,"lat":n|null,"lon":n|null,
 *          "altitude_m":n|null}}
 *
 * Returns malloc'd JSON the CALLER FREES, or NULL on failure with `err` set. */
char *ffmpeg_probe(const char *input, int timeout_ms, char *err, size_t errcap);

/* The normalised subset of the above as a struct, for callers that want the
 * facts and not a document. Fixed-size, no allocation, safe on the stack;
 * fully zeroed on entry including on every failure path, so it is safe to read
 * either way. Strings are truncated to fit and always NUL-terminated.
 *
 * `have_gps` is 1 ONLY when a coordinate was found, parsed, is in range, and is
 * not exactly (0,0) — the same stance core/media.c takes for EXIF GPS, and for
 * the same reason: (0,0) is what a device writes when it has no fix, and a
 * confident wrong pin is worse than no pin. */
typedef struct {
  double duration_sec;          /* 0 when unknown (live / no container info)  */
  int    width, height;         /* first video stream; 0 when none            */
  double fps;                   /* r_frame_rate evaluated; 0 when unknown     */
  long long bit_rate;           /* container bit rate; 0 when unknown         */
  char   format_name[64];
  char   video_codec[32];
  char   audio_codec[32];
  char   creation_time[40];     /* ISO-8601 as the container wrote it         */
  char   make[64], model[64];   /* com.apple.quicktime.make/model, or make/model */
  int    have_gps;
  double lat, lon;
  int    have_alt;
  double altitude_m;
} ffmpeg_media_info;

/* Probe and normalise in one call. Returns FFMPEG_OK or a negative
 * FFMPEG_ERR_*; `out` is zeroed on entry regardless. */
int ffmpeg_info(const char *input, int timeout_ms, ffmpeg_media_info *out,
                char *err, size_t errcap);

/* ISO 6709 → decimal degrees. Pure, no I/O, re-entrant — and the single most
 * test-worthy function in this file, because it is the one that decides where a
 * video says it was shot.
 *
 * Accepts every form the wild actually produces:
 *   "+35.6586+139.7454/"                 iPhone, degrees
 *   "+35.6586+139.7454+012.345/"         with altitude in metres
 *   "+3539.5160+13944.7240/"             degrees + decimal minutes
 *   "+353935.16+1394442.40/"             degrees + minutes + decimal seconds
 *   "+35.6586+139.7454CRSWGS_84/"        with a CRS suffix
 *   "35.6586,139.7454"                   the comma form some muxers emit
 * The DDMM / DDMMSS forms are distinguished from plain degrees by the COUNT OF
 * DIGITS BEFORE THE DECIMAL POINT (2/4/6 for latitude, 3/5/7 for longitude) —
 * that is how the standard encodes it, and guessing by magnitude instead gets
 * "+1394442.40" wrong in a way that puts the pin in the sea.
 *
 * `alt` may be NULL. Returns 1 only when both coordinates parsed, are in range
 * and are not exactly (0,0); 0 otherwise, with the outputs untouched. */
int ffmpeg_parse_iso6709(const char *s, double *lat, double *lon, double *alt);

/* ── generic seam (see WIRING note 3) ────────────────────────────────────── */

/* Run any tool with an argv ARRAY, bounded and deadlined, capturing stdout.
 * `argv[0]` is the program; the array is NUL-terminated as execvp() requires.
 * This is the one process-spawning primitive in the codebase and the intended
 * replacement for core/media.c's popen(): no shell, a real timeout, a reaped
 * child, a byte cap, and the child's stderr in `err` instead of /dev/null.
 *
 * `max_bytes` 0 → FFMPEG_MAX_BYTES. On FFMPEG_OK, *out is malloc'd (caller
 * frees) and NUL-terminated one past *out_len for the convenience of text
 * callers — the terminator is NOT counted in *out_len and binary output is
 * unaffected. Does NOT consult ffmpeg_available(): the tool being run is the
 * caller's business. Honours JO_NO_FFMPEG, which is the process-wide "spawn
 * nothing" switch. */
int ffmpeg_run(const char *const argv[], int timeout_ms, size_t max_bytes,
               unsigned char **out, size_t *out_len, char *err, size_t errcap);

/* Exactly the 1024 bytes of 32x32 8-bit grayscale that media_phash_gray32()
 * documents as its input, decoded from any image or video ffmpeg can read (for
 * a video: the first frame). Writes `out` only on FFMPEG_OK, and returns
 * FFMPEG_ERR_EMPTY when the tool produced the wrong number of bytes — a short
 * read here would be hashed as though it were an image and produce a confident
 * fingerprint of nothing.
 *
 * The scaler is pinned (`-sws_flags bilinear`, `scale=32:32` with no aspect
 * preservation, `-pix_fmt gray`) because the hash must be a property of THIS
 * CODEBASE and not of whichever ffmpeg build is installed: two deployments with
 * different default scalers must produce the same 64 bits for the same file, or
 * every stored hash silently stops being comparable. Same reasoning, and the
 * same hash definition, as core/media.h's pHash seam. */
int ffmpeg_gray32(const char *input, int timeout_ms, unsigned char out[1024],
                  char *err, size_t errcap);

/* ── ops ─────────────────────────────────────────────────────────────────── */

/* What this host can actually do:
 *   {"data":{"ffmpeg":{"available":bool,"path":…|null,"version":…|null},
 *            "ffprobe":{"available":bool,"path":…|null,"version":…|null},
 *            "disabled":bool,
 *            "protocols":["file","http",…],
 *            "limits":{"max_bytes":n,"timeout_ms":n}}}
 * Always non-NULL except on OOM. Caller frees. Answers "why is every video
 * field NULL" without an SSH session. */
char *ffmpeg_capabilities(void);

#endif /* JO_FFMPEG_H */
