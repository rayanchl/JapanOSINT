/* core/netjail.h — ffmpeg's network, in a box.
 *
 * WHY. ffmpeg dials its own sockets, so every check this process makes on a
 * camera URL is a check on what ffmpeg was TOLD to fetch, not on what it does
 * fetch. core/ffmpeg.c judges the input under the camera policy and
 * core/hls_check.c judges every URI a playlist names, and three things still
 * reached a destination nobody judged:
 *
 *   1. a LIVE playlist adds segments after hls_check read it;
 *   2. ffmpeg resolves names again itself, so a name can answer a public
 *      address to our check and 127.0.0.1 to ffmpeg (DNS rebinding);
 *   3. an RTSP server's redirect sends ffmpeg to a host of the server's
 *      choosing, and RTSP has no proxy to route it through.
 *
 * WHAT THIS DOES. Every network run of ffmpeg/ffprobe gets one of two plans:
 *
 *   PROXY (http, https, hls) — ffmpeg is started with http_proxy pointing at a
 *     forward proxy this process runs for that one child. Every request ffmpeg
 *     makes, the first playlist and the 400th live segment alike, arrives as a
 *     CONNECT or an absolute GET; the proxy resolves the name ONCE, judges
 *     EVERY answer under the camera policy, and connects to the address it
 *     judged. A refused destination is a 403 and ffmpeg fails.
 *   RELAY (rtsp, rtsps) — the name is resolved and judged here, the address is
 *     pinned, and ffmpeg is handed the same URL with its host replaced by the
 *     relay's loopback address. Every connection to the relay goes to the
 *     pinned address and nowhere else; media is interleaved on that one TCP
 *     connection (-rtsp_transport tcp), so nothing else needs to open.
 *
 * And on Linux the child runs in a NETWORK NAMESPACE of its own (a user
 * namespace too when the server is not root): it has a loopback interface and
 * nothing else, and the only socket listening on it is the proxy/relay, whose
 * listening descriptor is handed back to this process. A redirect, a
 * re-resolved name, a tcp:// segment — anything that is not the proxy or relay
 * — has no route to take. That is the part that closes (3) and makes (1) and
 * (2) structural rather than a promise the proxy env var keeps.
 *
 * WHERE NAMESPACES ARE NOT AVAILABLE (macOS, a container whose seccomp profile
 * refuses unshare, a kernel with unprivileged user namespaces off) the proxy
 * and the relay still run, on this process's own loopback, so every http(s)
 * request and every RTSP connection is still judged and pinned — but an RTSP
 * redirect or a non-http segment a live playlist adds later is NOT confined.
 * In that mode the proxy also listens on THIS host's loopback for the length
 * of the run, so another local process could use it — it would get exactly
 * what ffmpeg gets, the camera policy on every destination, nothing more.
 * netjail_mode() says which, and media capabilities report it, so a deployment
 * can see which guarantee it has instead of assuming the strong one.
 *
 * Nothing here is an operator knob: there is no way to turn the box off.
 */
#ifndef JO_NETJAIL_H
#define JO_NETJAIL_H

#include <stddef.h>
#include <sys/socket.h>

#define NJ_NONE   0
#define NJ_PROXY  1
#define NJ_RELAY  2

/* The input ffmpeg is handed can be longer than the original by the rewritten
 * authority; ffmpeg.c refuses network inputs over 2048 bytes. */
#define NJ_INPUT_MAX 2400

typedef struct netjail netjail;

/* Decide the plan for a network input whose scheme (lowercased) is `scheme`.
 * Returns NJ_PROXY / NJ_RELAY, or -1 with a one-line reason in `why` when the
 * input cannot be run in a box (a scheme with no sandboxed path, an
 * unparseable or refused RTSP host). */
int netjail_plan(const char *input, const char *scheme, char *why, size_t cap);

/* Parent, before fork. Resolves and pins a RELAY target, builds the child's
 * environment and the input string ffmpeg is to be handed. NULL with `why` set
 * on failure; `*blocked` (may be NULL) is 1 when the failure is the
 * destination (refused or unresolvable), 0 when it is the sandbox itself. */
netjail *netjail_new(const char *input, int plan, int timeout_ms,
                     int *blocked, char *why, size_t cap);
const char *netjail_input(const netjail *j);       /* what to pass to -i      */
char *const *netjail_envp(const netjail *j);       /* the child's environment */

/* Child, between fork and exec. Async-signal-safe: system calls only, on data
 * netjail_new prepared. 0 on success; the caller _exit()s on anything else. */
int netjail_child_enter(netjail *j);

/* Parent, right after fork. Takes the listening socket the child created (in
 * namespace mode) and starts the proxy/relay thread. 0 on success. */
int netjail_parent_start(netjail *j);

/* Stops the thread, closes every connection, frees. NULL-safe. */
void netjail_free(netjail *j);

/* "namespace" when children are confined to their own network namespace,
 * "proxy-only" when they are not (see above). Probed once per process. */
const char *netjail_mode(void);

/* Connections a box refused, for a test or a log line. */
int netjail_refused(netjail *j);

/* UNIT TESTS ONLY: 1 forces proxy-only mode (no namespace), 0 restores the
 * probe. Nothing in the product calls it. */
void netjail_test_force_no_ns(int on);

#endif /* JO_NETJAIL_H */
