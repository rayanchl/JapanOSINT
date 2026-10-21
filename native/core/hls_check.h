/* core/hls_check.h — judge every URL an HLS playlist would make ffmpeg fetch.
 *
 * WHY. ffmpeg dials its own sockets. core/ffmpeg.c checks the input URL under
 * the camera destination policy (core/hostgate.h) before spawning, but a
 * playlist is a list of FURTHER URLs the remote server supplies — segments,
 * keys, init sections, variant playlists — and ffmpeg follows them with only
 * -protocol_whitelist (a scheme list, never a host) in the way. A public
 * playlist naming http://127.0.0.1:8082/… or http://169.254.169.254/… made this
 * server request it. So before ffmpeg sees an http(s) input, it is read here:
 *
 *   - the first bytes decide whether it is a playlist at all (#EXTM3U), the
 *     same content sniff ffmpeg's own demuxer uses, so a .ts-named or
 *     extension-less playlist is caught too; anything else costs one packet;
 *   - a playlist is read whole (bounded), every URI in it — plain lines and
 *     URI="…" attributes alike — is resolved against the playlist's URL and
 *     judged by hostgate_camera_url_check(…, resolve = 1);
 *   - a variant (master) playlist's media playlists are read and judged the
 *     same way, to a fixed depth and count.
 *
 * It FAILS CLOSED: a playlist that cannot be read, is too large, nests too
 * deep or names too many playlists is refused, never waved through.
 *
 * WHAT IT CANNOT COVER, stated so nobody assumes it does: a LIVE playlist adds
 * segments after it was read here, and ffmpeg resolves names again itself, so
 * an answer can differ from the one judged here (DNS rebinding). Closing those
 * needs ffmpeg's network in a sandbox. The window is the few seconds one frame
 * takes to read.
 */
#ifndef JO_HLS_CHECK_H
#define JO_HLS_CHECK_H

#include <stddef.h>

#define HLS_OK          0    /* not a playlist, or every URI it names passed */
#define HLS_BLOCKED   (-1)   /* a URI it names is refused by the camera policy */
#define HLS_UNREADABLE (-2)  /* could not be read whole, so it was not judged  */

/* Fetch `url` (http, https or hls+http[s]) and judge it as above. `why` (may
 * be NULL) receives a one-line reason when the answer is not HLS_OK. */
int hls_check_input(const char *url, char *why, size_t why_cap);

/* The pure half, for tests: judge the playlist TEXT `text` found at
 * `base_url`. Variant playlists it names are fetched only when `follow` is 1. */
int hls_check_text(const char *base_url, const char *text, int follow,
                   char *why, size_t why_cap);

/* Resolve `ref` against `base` (RFC 3986 subset that playlists use: absolute,
 * scheme-relative "//host/p", absolute-path "/p", and relative "p"). Returns
 * 0 and writes `out`, or -1 when it does not fit. Exposed for tests. */
int hls_resolve(const char *base, const char *ref, char *out, size_t cap);

#endif /* JO_HLS_CHECK_H */
