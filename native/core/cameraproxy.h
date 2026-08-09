/* core/cameraproxy.h — GET /api/data/cameras/proxy?camera_uid=…
 *
 * Port of the Node route of the same name (server/src/routes/data.js, removed
 * in 4db45c8). WHY IT EXISTS: the cameras discovered by shodan_api /
 * manual_ip_seed / insecam_scrape are plain-HTTP endpoints on arbitrary IPs.
 * An iOS client cannot load those directly — App Transport Security would need
 * a per-host exception for every camera ever discovered — so the image is
 * fetched server-side and handed back over the app's own HTTPS origin. Without
 * this route `CameraFeedResolver.proxiedImage` resolves to a URL that 501s and
 * those cameras render as a broken feed.
 *
 * SSRF: the only reachable targets are URLs already stored against a camera
 * record this server itself discovered — the caller supplies a camera_uid, not
 * a URL. Plus http/https only, one bounded fetch, a size cap, and an
 * image/* content-type requirement.
 *
 * NOT PORTED from the Node version: the puppeteer snapshot fallback for
 * Referer-gated / MJPEG-only cameras (it needs a headless browser this server
 * does not have). Those cameras fail with 502 instead of falling back. */
#ifndef JO_CAMERAPROXY_H
#define JO_CAMERAPROXY_H

#include "db.h"
#include <stddef.h>

/* Fetch one image for `camera_uid`.
 *   success → returns NULL, *status = 200, *out_bytes = malloc'd image bytes
 *             (caller frees), *out_len = length, ct = content type.
 *   failure → returns a malloc'd {"error":…} envelope and sets *status to
 *             400/404/502; *out_bytes is untouched.
 * Results are cached in-process for CAM_PROXY_TTL_SEC so a refreshing client
 * doesn't hammer the upstream camera. Thread-safe. */
char *camera_proxy_fetch(db_handle *db, const char *camera_uid,
                         unsigned char **out_bytes, size_t *out_len,
                         char *ct, size_t ct_cap, int *status);

#endif
