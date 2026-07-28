/* core/camera_stills.h — roadmap 28: scheduled camera stills.
 *
 * ══ WHY THIS EXISTS ═══════════════════════════════════════════════════════
 *
 * The corpus knows about tens of thousands of public cameras (core/
 * camera_store.c) and has never looked through one of them. A camera row is a
 * pin on a map: a name, a coordinate, a feed URL that nobody dereferences.
 * Sampling that feed on a schedule turns each pin into a TIME SERIES — the
 * only artefact in this product that can answer "what did that intersection
 * look like at 03:40 on the night in question", and the only one that keeps
 * answering it after the camera is taken offline, re-aimed or firewalled. That
 * is the deliverable. Everything else here is the machinery that keeps it from
 * eating the disk.
 *
 * ══ THE HARD CONSTRAINT, STATED UP FRONT ══════════════════════════════════
 *
 * This host has NO ffmpeg and NO image decoder of any kind (no libjpeg, no
 * stb_image, no ImageMagick), and CONVENTIONS forbids adding a link
 * dependency. The design is drawn around that, not over it:
 *
 *   HTTP SNAPSHOT URLs   → FULLY BUILT, no dependency. A GET, a magic-byte
 *     sniff, a structural JPEG walk to confirm the frame is COMPLETE, a
 *     SHA-256, and the bytes go to the evidence blob store. This is the case
 *     that actually runs on this host today.
 *
 *   MJPEG STREAMS        → FULLY BUILT, no dependency. A multipart/x-mixed-
 *     replace body is a run of JPEGs separated by boundary lines; pulling ONE
 *     frame out of it needs no decoder, only a walk of the JPEG marker
 *     structure — SOI, segment lengths, SOS, the FF00 byte-stuffing rule,
 *     EOI. camera_stills_jpeg_frame() is that walk and it is the technical
 *     core of this file. See "MJPEG AND WHY http_request() CANNOT DO IT".
 *
 *   RTSP and HLS         → OPTIONAL EXTERNAL TOOL, and NOT THIS MODULE'S
 *     BUSINESS ANY MORE. Both require a demuxer and a decoder; there is no
 *     honest way to synthesize either in-process. They are handed to
 *     core/ffmpeg.h — the ONE ffmpeg seam in the product — which runs the tool
 *     ONLY when it is on PATH. When it is not, the camera is marked with an
 *     explicit stored reason ("rtsp_requires_ffmpeg") and backed off — NOT
 *     silently retried forever and NOT reported as a fetch failure, which
 *     would be a lie about why it did not work. NOTHING in this file pretends
 *     RTSP works here. It does not, because it cannot. See "WHAT IS AND IS NOT
 *     EXERCISED".
 *
 * ══ WHERE CAMERAS LIVE (read core/camera_store.h first) ═══════════════════
 *
 * There is no `cameras` table. A camera is an intel_items row with
 * source_id='camera-discovery', record_type='camera' and
 * uid = "camera-discovery|<camera_uid>"; the feed lives in the `properties`
 * JSON. So the per-camera opt-in column goes on intel_items, and
 * camera_stills.camera_id holds the FULL intel_items uid — not the bare
 * camera_uid — so it joins directly and can never collide with a camera_uid
 * that happens to look like something else. The read API accepts either
 * spelling and prefixes "camera-discovery|" when the caller passes a bare uid.
 *
 * FEED RESOLUTION, in priority order, from properties:
 *   snapshot_url, still_url, image_url, stream_url, thumbnail_url, url
 * `url` is LAST on purpose: for most discovery channels it is the human page
 * the camera is embedded in, not the image, and fetching a page and storing
 * the HTML as a "still" would be silent garbage. A resolved URL whose bytes
 * do not sniff as an image is rejected with reason "not_an_image" rather than
 * stored.
 *
 * ══ RETENTION IS MANDATORY, NOT OPTIONAL ══════════════════════════════════
 *
 * One frame per camera per interval, forever, is the fastest way to fill a
 * disk anywhere in this roadmap: it is the only pod whose output grows without
 * bound even when NOTHING IN THE WORLD CHANGES. A camera pointed at an empty
 * car park at 3am produces exactly as many bytes as one pointed at a riot. So
 * retention is not a knob bolted on afterwards, it is part of the write path:
 *
 *   - N-DAYS *AND* M-FRAMES-PER-CAMERA, whichever bites first.
 *   - PRUNED ON EVERY WRITE (camera_stills_prune_camera() runs immediately
 *     after each INSERT, in the same transaction), so the ceiling holds even
 *     if the sweep pod is never wired up or is disabled.
 *   - PRUNED AGAIN IN A SWEEP (camera_stills_sweep()) that walks EVERY camera
 *     with rows, not just the ones still opted in — otherwise a camera that is
 *     switched off, deleted or renamed keeps its frames forever and the
 *     on-write prune never fires again for it. That case is the whole reason
 *     the sweep exists.
 *
 * DEFAULTS (deliberately conservative; all env-overridable, see ENVIRONMENT):
 *   interval per camera      900 s   (4 frames/hour)
 *   retention window           3 days
 *   frames kept per camera    48     (= 12 h at the default interval, so the
 *                                     FRAME CAP is what normally bites, not
 *                                     the day window — the day window exists
 *                                     for cameras on a slow custom interval)
 *   per-frame byte cap          2 MiB (a 2 MiB "snapshot" is not a snapshot)
 *   cameras per tick            8
 *   pod interval              300 s
 *
 * WORST-CASE DISK FOR 100 OPTED-IN CAMERAS:
 *   rows   100 x 48 = 4,800 rows x ~250 B ≈ 1.2 MB of SQLite. Negligible.
 *   blobs  100 x 48 = 4,800 frames. At the 2 MiB per-frame cap and with every
 *          frame distinct that is 9.6 GiB of BLOBS — and that number is the
 *          reason the next two sentences exist.
 *          (a) Blobs are content-addressed in the evidence store, so a static
 *              scene stores ONE file no matter how many rows point at it. A
 *              night-time car park is a handful of distinct blobs, not 48.
 *          (b) The evidence store's own byte budget (JO_EVIDENCE_MAX_BYTES,
 *              default 2 GiB, reaped to 90% by evidence_gc) is a HARD global
 *              ceiling that stills share with all other evidence. On-disk
 *              bytes therefore cannot exceed that, whatever this module's
 *              row-level retention says.
 *   realistic  4,800 x ~120 KB (a typical municipal traffic-cam JPEG)
 *              ≈ 560 MB, before dedup.
 *   default    ZERO. Capture is OFF for every camera until an operator turns
 *              it on, per camera.
 *
 * The two ceilings are INDEPENDENT and both are needed: ours bounds ROWS and
 * how far back a camera's history goes; the evidence budget bounds BYTES
 * globally. Pruning a still row NEVER unlinks a blob — evidence.h is explicit
 * that blobs are the reaper's business alone, blobs are shared by content hash
 * (the frame may be referenced by another camera or pinned into a case), and
 * evidence rows are immutable. A pruned still row therefore frees SQLite
 * space immediately and disk space when the reaper next runs.
 *
 * ══ PER-CAMERA OPT-IN, DEFAULT OFF ════════════════════════════════════════
 *
 * Same stance as item 17's evidence capture, for the same reason: a feature
 * that starts consuming disk and hammering third-party cameras merely because
 * it was merged is a denial-of-service shipped as a feature — and unlike a
 * feed poll, this one puts sustained traffic on somebody else's camera. So:
 *   intel_items.capture_stills = 0 for every camera, forever, until an
 *   operator sets it to 1 for a specific camera.
 * There is no bulk enable and no "enable by channel" in this module by
 * design. Like sources.capture_evidence (item 17), flipping the flag is an
 * operator/SQL action rather than a tenant-facing endpoint, because a camera
 * row is platform corpus data with no tenant_id to authorize against:
 *   UPDATE intel_items SET capture_stills=1
 *    WHERE uid='camera-discovery|<camera_uid>';
 * A tenant-facing toggle needs a tenant→camera authorization model that does
 * not exist yet; inventing one here would be the wrong place for it.
 *
 * BYTES NEED A SECOND, SEPARATE OPT-IN — say this out loud, because it is the
 * most likely "why is blob_path NULL" support ticket. This module does not own
 * a blob store; it uses evidence.h's, and evidence_capture() is gated on
 * sources.capture_evidence for the source it is filed under:
 *   UPDATE sources SET capture_evidence=1 WHERE id='camera-stills';
 * Without it, stills are still captured, hashed, measured and compared — the
 * time series works — but blob_path is NULL and the raw endpoint 404s. That
 * is reported by camera_stills_capabilities() as bytes_preserved:false and
 * warned once to stderr, rather than left for someone to discover.
 *
 * ══ SCENE CHANGE, AND REFUSING TO LIE ABOUT IT ════════════════════════════
 *
 * scene_changed is what turns 48 pictures into an event log: it is the column
 * an analyst filters on to find the four frames out of a week that matter.
 * The comparator is a seam with two rungs, and the seam is the point:
 *
 *   1. pHash (media_phash_file / media_phash_distance, roadmap 27 — REUSED,
 *      not reimplemented). Hamming distance > CAM_STILLS_PHASH_THRESHOLD (6)
 *      → changed. This is the real answer: it survives JPEG re-encoding,
 *      brightness drift and a burnt-in clock overlay. IT NEEDS PIXELS, and
 *      pixels need a decoder this host does not have.
 *   2. SHA-256 equality. Byte-identical frames are DEFINITELY unchanged, so
 *      scene_changed=0 is written and is TRUE. This costs nothing and is the
 *      only certainty available without a decoder.
 *
 * If the hashes differ and no pHash is available, scene_changed is NULL —
 * "unknown" — NOT 0. This is deliberate and is the single most important
 * honesty decision in the file: two frames from the same static camera differ
 * in bytes constantly (JPEG quantisation noise, a clock overlay, a sensor
 * gain change), so writing 0 would report "nothing happened" for every frame
 * of a bank robbery. NULL says "nobody looked". On a host with a decoder the
 * column populates with no schema change and no backfill; on this host it is
 * mostly NULL and the API says why (`scene_change_available:false`).
 *
 * ══ MJPEG AND WHY http_request() CANNOT DO IT ═════════════════════════════
 *
 * DELIBERATE DEVIATION, flagged because it is the one place this file does not
 * use the house HTTP client. core/httpclient.c's do_once() ends:
 *
 *     if (rc != CURLE_OK) { free(b.buf); out->status = 0; out->body = NULL;
 *                           out->body_len = 0; return 1; }
 *
 * An MJPEG endpoint is an INFINITE response: it never completes, so curl
 * always ends in CURLE_OPERATION_TIMEDOUT, so the accumulated body — which
 * contains dozens of perfectly good frames — is FREED and the caller gets
 * NULL. There is no size hook, no streaming callback and no partial-body
 * escape in that API, and httpclient.c may not be edited from here. Passing a
 * Range header does not help: the servers that honour Range are the ones
 * serving a finite snapshot anyway.
 *
 * So MJPEG uses a PRIVATE libcurl easy handle (libcurl is already linked; this
 * adds no dependency) whose write callback ABORTS THE TRANSFER the instant one
 * complete JPEG is in the buffer. That is strictly better than a timeout even
 * where a timeout would work: a frame typically arrives in well under a
 * second, the connection is dropped immediately, and the camera is not made to
 * stream to us for the full timeout. It also gives the hard byte cap that
 * media.h could only ask for.
 *   ROUTING: a URL that looks like a stream (mjpg / mjpeg / action=stream /
 *   videostream / nphMotionJpeg / axis-cgi/mjpg / video.cgi) goes straight to
 *   the streaming grabber. Everything else tries http_request() first — so the
 *   ordinary snapshot case keeps the shared client's DNS/TLS/connection reuse,
 *   url_override_apply() rewrites and retry policy — and falls back to the
 *   streaming grabber ONCE on a hard failure, which is exactly the signature of
 *   an unannounced stream.
 *
 *   PROPER FIX for whoever owns httpclient.c, which would delete the private
 *   handle entirely: keep the partial body on CURLE_OPERATION_TIMEDOUT
 *   (`if (rc != CURLE_OK && (rc != CURLE_OPERATION_TIMEDOUT || !b.len))`), and
 *   add CURLOPT_MAXFILESIZE_LARGE — the same one-liner media.h asks for.
 *
 * ══ THE FFMPEG SEAM — NOW OWNED BY core/ffmpeg.h ══════════════════════════
 *
 * This module used to carry its own find_ffmpeg() and its own fork/execvp/
 * poll/SIGKILL/waitpid loop. It does not any more. core/ffmpeg.c is THE video
 * abstraction and the only place in the product that spawns a media tool;
 * duplicated process plumbing is the worst thing here to duplicate, because
 * the day one copy learns to reap its children or bound its output and the
 * other does not, the bug surfaces as a slow leak under load in whichever file
 * nobody was reading. The properties this path depends on are stated and
 * enforced THERE, once: argv array and never a shell, -nostdin, a hard SIGKILL
 * deadline with the child always reaped, bounded output, and an explicit
 * -protocol_whitelist whose network form deliberately excludes `file` so a
 * hostile HLS playlist cannot name a local path as a segment.
 *
 * WHAT REMAINS THIS MODULE'S POLICY, and is deliberately TIGHTER than
 * ffmpeg.h's: a camera feed URL must be rtsp/rtsps/http/https, printable ASCII
 * only, length-capped, and cannot start with '-' (which ffmpeg would read as an
 * OPTION rather than an input). ffmpeg.h additionally permits file, rtmp, tcp
 * and udp — correct for an uploaded video, wrong for a URL that came out of a
 * third-party camera registry, so url_exec_safe() stays and runs first.
 *
 * REASON STRINGS. "rtsp_requires_ffmpeg" / "hls_requires_ffmpeg" /
 * "rtsp_disabled_no_ffmpeg" / "hls_disabled_no_ffmpeg" / "unsafe_feed_url" are
 * unchanged — they are what a deployment already has in
 * camera_stills_state.last_error and they are the only ones that occur on a
 * host with no ffmpeg. What USED to be the single catch-all "ffmpeg_failed" is
 * now ffmpeg.h's specific token instead: ffmpeg_timeout, ffmpeg_exit_nonzero,
 * ffmpeg_output_too_large, ffmpeg_empty_output, ffmpeg_spawn_failed. That is
 * strictly more information in the same column, and the tool's own stderr (a
 * camera's 401, an "Invalid data found") is logged to stderr alongside it.
 *
 * ══ SCHEMA ════════════════════════════════════════════════════════════════
 *
 * camera_stills_migrate() performs EVERY statement below, in dependency order,
 * and is idempotent — wiring that ONE call is sufficient. The breakdown is
 * given so the safe parts can also be seeded into schema.sql for fresh
 * databases, and so the unsafe part is unmistakable.
 *
 * (A) SAFE for core/schema.sql — VERBATIM, as specified:
 *
 *   CREATE TABLE IF NOT EXISTS camera_stills (
 *     id TEXT PRIMARY KEY, camera_id TEXT NOT NULL,
 *     captured_at TEXT NOT NULL DEFAULT (datetime('now')),
 *     sha256 TEXT NOT NULL, blob_path TEXT, bytes INTEGER,
 *     phash INTEGER, ocr_text TEXT, width INTEGER, height INTEGER,
 *     scene_changed INTEGER);
 *   CREATE INDEX IF NOT EXISTS idx_camera_stills_cam ON camera_stills(camera_id, captured_at DESC);
 *
 *   -- Per-camera scheduler state: attempt counter, backoff deadline and the
 *   -- LAST REASON a capture did not happen. Separate from camera_stills
 *   -- because a failure produces no still row — if the counters lived on the
 *   -- stills table, a camera that has never once succeeded (the RTSP case on
 *   -- this host) would have nowhere to record why, and would be retried at
 *   -- full rate forever.
 *   CREATE TABLE IF NOT EXISTS camera_stills_state (
 *     camera_id TEXT PRIMARY KEY,
 *     fail_count INTEGER NOT NULL DEFAULT 0,
 *     last_attempt_at TEXT, next_attempt_at TEXT, last_ok_at TEXT,
 *     last_error TEXT, last_kind TEXT);
 *
 * (B) NEW COLUMN on an existing table — ensure_column(), NEVER an ALTER in
 *     schema.sql (db_open() hands that file to ONE sqlite3_exec() on every
 *     boot; the second boot dies with "duplicate column name", exec abandons
 *     the rest of the script, and the process never starts). Orchestrator: add
 *     to db.c's boot-migration block, or just call camera_stills_migrate():
 *
 *   ensure_column(db, "intel_items", "capture_stills", "INTEGER NOT NULL DEFAULT 0");
 *
 * (C) An index that REFERENCES that new column, and therefore MUST NOT go into
 *     schema.sql — schema.sql runs BEFORE the boot-migration block, so on an
 *     existing database the column does not exist yet and this CREATE would
 *     brick the boot. It is created inside camera_stills_migrate(), AFTER the
 *     ensure_column() above, and nowhere else:
 *
 *   CREATE INDEX IF NOT EXISTS idx_intel_items_capture_stills
 *     ON intel_items(uid) WHERE capture_stills=1;
 *
 *   COST: partial and tiny — it holds one entry per OPTED-IN camera, which is
 *   0 on every deployment until an operator acts, so building it is free.
 *   Unlike simhash.c's and media.c's pending indexes there is no one-off
 *   full-table-scan boot cost here, because the predicate matches nothing.
 *
 * ══ WIRING THE ORCHESTRATOR MUST DO ═══════════════════════════════════════
 *
 * 1. core/db.c, boot-migration block, beside the other ensure_column() calls:
 *        #include "camera_stills.h"
 *        camera_stills_migrate(db);
 *    This is the ONLY required wiring. Every entry point below also calls it
 *    lazily (once per process, mutex-guarded), so a missed call degrades to a
 *    one-time migration on first use rather than SQL errors.
 *
 * 2. Scheduled operation: NONE. camera_stills.c registers its own source_def
 *    ("camera-stills", collector "_maint", 300 s) via REGISTER_SOURCE, exactly
 *    like collectors/sources/entity_stats_pod.c. The Makefile globs the core
 *    .c files so the constructor links itself in. The underscore prefix is REQUIRED, not
 *    cosmetic: scheduler.c skips fetch_log_write() and anomaly_detect() for
 *    underscore collectors, and this pod emits zero intel_items (records=0
 *    forever would trip records_drop against its own baseline) and has a
 *    wall-clock duration that depends entirely on how many cameras an operator
 *    opted in (duration_outlier). It also inherits the scheduler's
 *    skip-if-running serialisation, so two ticks can never overlap.
 *
 * 3. core/httpd.c — two read routes:
 *
 *      GET /api/cameras/:id/stills            (plain auth, tenant-agnostic —
 *                                              same shape and same stance as
 *                                              the .../evidence and .../media
 *                                              routes; camera rows are corpus
 *                                              data with no tenant_id)
 *        char *body = camera_stills_list(g_db, id, limit, cursor);
 *        if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
 *        reply_json(c, 200, body); free(body); return;
 *
 *      GET /api/camera-stills/:id/raw         (OPERATOR-GATED, BYTES)
 *        unsigned char *buf = NULL; size_t len = 0; char sha[65]; int st = 200;
 *        char *e = camera_stills_raw(g_db, &usr, id, &buf, &len, sha, &st);
 *        if (e) { reply_json(c, st, e); free(e); return; }
 *        mg_printf(c, "HTTP/1.1 200 OK\r\n"
 *                     "Content-Type: application/octet-stream\r\n"
 *                     "Content-Disposition: attachment; filename=\"%s.bin\"\r\n"
 *                     "X-Content-Type-Options: nosniff\r\n"
 *                     "Content-Length: %lu\r\n"
 *                     "Cache-Control: no-store\r\n\r\n", sha, (unsigned long)len);
 *        c->is_resp = 0;                      -- we own the framing
 *        mg_send(c, buf, len);
 *        free(buf); return;
 *
 *      A still is a byte sequence an arbitrary internet camera handed us. It
 *      is NOT trusted image data: a "JPEG" that is also valid HTML, or a
 *      polyglot that a browser will happily execute, is a five-minute attack
 *      against any product that serves it inline. So it goes out as
 *      application/octet-stream, as an ATTACHMENT, with nosniff and no-store,
 *      never as image/jpeg and never embedded in a page. That is also why this
 *      one function breaks the house rule and returns bytes rather than JSON —
 *      routing them through a JSON string would corrupt them and invite
 *      exactly the HTML echo the octet-stream framing exists to prevent.
 *
 *      Optional operator route, answers "why is everything NULL" without SSH:
 *        GET /api/camera-stills/capabilities → camera_stills_capabilities(g_db)
 *
 * 4. No core/main.c change. No new thread. No new link dependency (libcurl,
 *    OpenSSL, cJSON, sqlite3 and pthread are all already linked).
 *
 * ══ ENVIRONMENT ═══════════════════════════════════════════════════════════
 *   CAM_STILLS_ENABLED=false     kill switch; anything else = on
 *   CAM_STILLS_INTERVAL_SEC      seconds between frames, per camera    (900)
 *   CAM_STILLS_RETAIN_DAYS       age ceiling                             (3)
 *   CAM_STILLS_MAX_PER_CAM       frame ceiling per camera               (48)
 *   CAM_STILLS_BATCH             cameras attempted per tick              (8)
 *   CAM_STILLS_MAX_BYTES         per-frame byte cap          (2097152 = 2 MiB)
 *   CAM_STILLS_TIMEOUT_MS        per-camera fetch timeout            (8000)
 *   CAM_STILLS_TICK_BUDGET_MS    wall-clock ceiling for one tick    (60000)
 *   CAM_STILLS_PHASH_THRESHOLD   Hamming distance called "changed"       (6)
 *   CAM_STILLS_MAX_BACKOFF_SEC   backoff ceiling for a failing camera (21600)
 *   CAM_STILLS_FFMPEG            path override; unset → probe PATH
 *   CAM_STILLS_NO_FFMPEG         set → never spawn ffmpeg; RTSP/HLS recorded
 *                                as unsupported
 *   (both are read by core/ffmpeg.c, which honours them as exact synonyms of
 *    its own JO_FFMPEG / JO_NO_FFMPEG so an existing deployment's settings keep
 *    working across this consolidation. JO_* wins where both are set. See
 *    core/ffmpeg.h for JO_FFPROBE, JO_FFMPEG_MAX_BYTES and JO_FFMPEG_TIMEOUT_MS,
 *    none of which this module sets — it passes its own CAM_STILLS_MAX_BYTES
 *    and CAM_STILLS_TIMEOUT_MS per call.)
 *   (bytes additionally obey evidence.h: JO_EVIDENCE_DIR, JO_EVIDENCE_MAX_BYTES,
 *    JO_EVIDENCE_MAX_BLOB_BYTES, JO_NO_EVIDENCE.)
 *
 * ══ WHAT IS AND IS NOT EXERCISED IN THIS ENVIRONMENT ══════════════════════
 *
 * EXERCISED (47 assertions, 0 failures, under ASan+UBSan — see the test note
 * at the top of camera_stills.c): the JPEG structural walk
 * (camera_stills_jpeg_frame) against hand-assembled fixtures — a baseline
 * JPEG, one with an APP1 segment containing an embedded thumbnail whose own
 * FFD9 must NOT be mistaken for the outer EOI, FF00 byte stuffing, FFD0..FFD7
 * restart markers, FF FF fill runs (this case caught a real truncation bug),
 * a multipart/x-mixed-replace body with CRLF boundary lines, adversarial
 * segment lengths, and EVERY truncation prefix of each. Also exercised against
 * a real SQLite database: both retention limbs, the sweep over an orphaned
 * camera, keyset paging with no duplicates or gaps, the JSON shapes including
 * scene_changed-as-null, URL classification, and every rung of
 * camera_stills_scene().
 *
 * NOT EXERCISED, and it would be dishonest to imply otherwise:
 *   - The pHash rung of scene detection. It calls media_phash_file(), which
 *     needs a decoder binary this host does not have, so it returns 0 and
 *     scene_changed is NULL for every frame pair that is not byte-identical.
 *     The comparator seam is written and syntax-checked; it has never once
 *     produced a `1`.
 *   - The ffmpeg RTSP/HLS path end to end. There is no ffmpeg here. That
 *     machinery now lives in core/ffmpeg.c and its PLUMBING has since been
 *     driven for real against a stub ffmpeg on PATH (argv construction, the
 *     SIGKILL deadline, child reaping, the byte cap, the absent-tool path — see
 *     the test note there). What has still never happened, in this file or that
 *     one, is a real ffmpeg decoding a real RTSP stream. What DOES run on this
 *     host is the absence path: no tool → reason "rtsp_requires_ffmpeg", long
 *     backoff, no retry storm, honest capabilities output.
 *   - Live cameras. No third-party camera was polled from this environment.
 *     The MJPEG grabber was driven against local multipart fixtures, not
 *     against a real streaming endpoint, so the write-callback abort has been
 *     exercised in shape but not against a live infinite stream.
 */
#ifndef JO_CAMERA_STILLS_H
#define JO_CAMERA_STILLS_H

#include "auth.h"
#include "db.h"
#include "httpclient.h"
#include <stddef.h>
#include <stdint.h>

/* ── feed classification ─────────────────────────────────────────────────── */

#define CAM_FEED_NONE     0   /* no usable URL in properties                 */
#define CAM_FEED_SNAPSHOT 1   /* plain http(s) GET → one image               */
#define CAM_FEED_MJPEG    2   /* http(s) multipart/x-mixed-replace           */
#define CAM_FEED_RTSP     3   /* rtsp:// or rtsps://  — ffmpeg only          */
#define CAM_FEED_HLS      4   /* .m3u8                — ffmpeg only          */
#define CAM_FEED_UNSUPPORTED 5

/* Classify a feed URL. Pure, no I/O. SNAPSHOT vs MJPEG is a HEURISTIC over the
 * URL shape (see the routing note above); the true distinction is only known
 * once bytes arrive, and the capture path handles being wrong in either
 * direction. Returns one of CAM_FEED_*. */
int camera_stills_classify(const char *url);

/* ── the decoder-free JPEG frame extractor ───────────────────────────────── */

/* Locate the first COMPLETE JPEG in `buf` and return 1 with *off and *flen set
 * to its exact extent; return 0 when there is no SOI or the frame is truncated.
 *
 * This is a walk of the JPEG MARKER STRUCTURE, not a scan for FFD8…FFD9, and
 * the difference matters: an EXIF APP1 segment routinely contains an entire
 * embedded thumbnail, complete with its own FFD9, and the naive scan returns a
 * frame that ends inside the metadata of the real one. So segments are skipped
 * by their DECLARED LENGTH, entropy-coded data after SOS is walked honouring
 * the FF00 stuffing rule and FFD0..FFD7 restart markers, and only the EOI that
 * terminates the outermost image is accepted.
 *
 * Multipart boundary lines, part headers and any leading garbage are skipped
 * implicitly by starting at the first SOI — which is strictly more reliable
 * than trusting a declared boundary token or a per-part Content-Length, both
 * of which cameras get wrong.
 *
 * Total function: no allocation, no I/O, no globals, re-entrant, and it cannot
 * read outside [buf, buf+len) for ANY input, including adversarial ones. */
int camera_stills_jpeg_frame(const void *buf, size_t len,
                             size_t *off, size_t *flen);

/* ── scene change ────────────────────────────────────────────────────────── */

#define CAM_SCENE_UNKNOWN (-1)   /* → SQL NULL: nobody could look           */
#define CAM_SCENE_SAME      0
#define CAM_SCENE_CHANGED   1

/* Compare a new frame with the camera's previous one. `prev_sha`/`sha` are
 * 64-hex strings (prev_sha may be NULL for the first frame ever). A pHash of 0
 * means "absent" — 0 is media.h's documented no-usable-hash sentinel, so it can
 * never be a real hash and needs no separate flag.
 *
 * Returns CAM_SCENE_CHANGED / CAM_SCENE_SAME when it can prove one, and
 * CAM_SCENE_UNKNOWN when it cannot — which on a host with no image decoder is
 * every pair whose bytes differ. Reporting SAME there would be a lie; see the
 * long note above. The first frame of a camera is CHANGED (there was nothing,
 * now there is something). */
int camera_stills_scene(const char *prev_sha, uint64_t prev_phash,
                        const char *sha, uint64_t phash);

/* ── pipeline ────────────────────────────────────────────────────────────── */

/* Every table, column and index this module needs, in dependency order (the
 * ensure_column BEFORE the partial index that names it). Idempotent,
 * mutex-guarded, runs its DDL at most once per process, and never fails
 * loudly — a DDL failure degrades the module to a no-op rather than taking
 * down whatever called it. */
void camera_stills_migrate(db_handle *db);

/* Capture ONE frame from ONE camera, now, ignoring its backoff deadline but
 * NOT its opt-in (a camera with capture_stills=0 returns 0 and is never
 * fetched). `camera_id` may be the full intel_items uid or a bare camera_uid.
 * `http` may be NULL, in which case a private client is created and freed.
 * Returns 1 when a still row was written, 0 otherwise; the reason for a 0 is
 * recorded in camera_stills_state.last_error and surfaced by the list route's
 * meta block. Never returns a partial write. */
int camera_stills_capture_one(db_handle *db, http_client *http,
                              const char *camera_id);

/* One capture batch: up to `limit` (<=0 → CAM_STILLS_BATCH) opted-in cameras
 * whose next_attempt_at is due, oldest-due first.
 *
 * Discipline, mirroring core/media.c: the due list is read and the statement
 * FINALIZED before the first fetch, so no read cursor is held across a network
 * call; each camera's writes are their own short transaction; `cancel` is
 * polled between cameras; and ONE DEAD CAMERA CANNOT STALL THE OTHERS —
 * every fetch carries its own bounded timeout, a failure is recorded and the
 * loop moves on, and the whole tick is additionally capped by
 * CAM_STILLS_TICK_BUDGET_MS so a tick can never run into the next one.
 *
 * A failing camera backs off exponentially (interval x 2^fail_count, capped at
 * CAM_STILLS_MAX_BACKOFF_SEC). It is never abandoned permanently: cameras come
 * back, and a permanently-abandoned camera is indistinguishable from a bug.
 * Returns frames captured. */
int camera_stills_capture_batch(db_handle *db, http_client *http, int limit,
                                volatile int *cancel);

/* Enforce retention for ONE camera: delete rows beyond CAM_STILLS_MAX_PER_CAM
 * (newest kept) and rows older than CAM_STILLS_RETAIN_DAYS. Called on every
 * write, so the ceiling holds even with no sweep. Returns rows deleted. Blobs
 * are NOT unlinked — see the retention note above. */
int camera_stills_prune_camera(db_handle *db, const char *camera_id);

/* Retention sweep over EVERY camera that has rows, including ones no longer
 * opted in or no longer present in intel_items — those are exactly the ones
 * the on-write prune can never reach again. `limit` (<=0 → all) caps cameras
 * examined per call; `cancel` stops it cleanly. Returns rows deleted. */
int camera_stills_sweep(db_handle *db, int limit, volatile int *cancel);

/* ── read paths ──────────────────────────────────────────────────────────── */

/* One camera's stills, newest first, keyset-paged:
 *   {"data":[{id,captured_at,sha256,bytes,phash,width,height,scene_changed,
 *             blob_present,ocr_text}],
 *    "page":{"limit":n,"count":n,"cursor":"<base64url>"|null},
 *    "meta":{"camera_id":…,"capture_stills":bool,"total":n,"changed":n,
 *            "unknown":n,"blobs_present":n,"oldest":…,"newest":…,"fail_count":n,
 *            "next_attempt_at":…,"last_error":…,"last_kind":…,
 *            "scene_change_available":bool}}
 * `phash` is a 16-CHAR HEX STRING, never a number: a 64-bit value does not
 * survive an IEEE double and a silently truncated fingerprint is worse than
 * none. `scene_changed` is true/false/null — null meaning UNKNOWN, which is a
 * different statement from false and must render differently.
 * Cursor is base64url of {"p":"<captured_at>","u":"<id>"} per CONVENTIONS; a
 * malformed cursor is ignored rather than rejected. limit defaults to 50,
 * clamped 1..200. NULL → caller replies 500. Caller frees. */
char *camera_stills_list(db_handle *db, const char *camera_id, int limit,
                         const char *cursor);

/* Operator-gated raw frame bytes. DELIBERATE DEVIATION from the house rule
 * that public functions return JSON, for the reasons in the wiring section.
 *   success → returns NULL, *status=200, *out_bytes = malloc'd frame (caller
 *             frees), *out_len = length, out_sha (may be NULL) = 64-hex + NUL.
 *   failure → returns a malloc'd {"error":…} envelope, *status set to
 *             401/403/404/410/500, *out_bytes untouched.
 * 410 means "row intact, blob reaped or never preserved" — an honest answer a
 * 404 would hide. The blob is re-hashed on the way out and refused if it does
 * not match, exactly as evidence_raw() does: serving bytes that are not the
 * bytes the row attests to is worse than serving nothing. Writes an
 * audit_events row on every successful reveal. */
char *camera_stills_raw(db_handle *db, const auth_user *u, const char *still_id,
                        unsigned char **out_bytes, size_t *out_len,
                        char *out_sha, int *status);

/* What this host can actually do, for an ops route:
 *   {"data":{"enabled":bool,"cameras_opted_in":n,"stills":n,"cameras_with_stills":n,
 *            "bytes_preserved":bool,"blobs_present":n,
 *            "snapshot":{"available":true,"builtin":true},
 *            "mjpeg":{"available":true,"builtin":true},
 *            "rtsp":{"available":bool,"tool":"…"|null},
 *            "hls":{"available":bool,"tool":"…"|null},
 *            "scene_change":{"available":bool,"method":"phash"|"sha256_only"},
 *            "retention":{"days":n,"max_per_camera":n,"interval_sec":n,
 *                         "max_bytes":n}}}
 * Always non-NULL unless db is NULL. Caller frees. */
char *camera_stills_capabilities(db_handle *db);

#endif /* JO_CAMERA_STILLS_H */
