/* core/visualsearchapi.h — GET /api/cameras/visual-search: find cameras by
 * what their frames SHOW ("flooded road", "雪の高速道路").
 *
 *   ?q=      the sentence (required)
 *   ?bbox=   w,s,e,n — cameras whose record lies inside (optional)
 *   ?since=  ISO date/time — stills captured at or after it (optional)
 *   ?k=      cameras to return, 1..50 (default 10)
 *
 * q is embedded by the vision sidecar's POST /embed/text (the SAME space the
 * frames were embedded in — core/image_vec.h), the KNN runs over image_vec
 * with kind/ts filtered inside it, and the hits are grouped BY CAMERA: one
 * result per capture target, its nearest frame, and that frame's newest still
 * (several stills can share one content-addressed vector). A camera's frames
 * are its stills AND its weekly reference frame (core/camera_refframe.h), so a
 * camera nobody opted into stills still answers; such a hit has `still` null
 * and `frame.kind` "refframe" with the frame's evidence id (its bytes are at
 * GET /api/evidence/:id/raw, gated like every raw route):
 *
 *   {"data":[{camera_id, item_uid, view, name, lat, lon, distance, cosine,
 *             still:{id, captured_at, sha256}|null,
 *             frame:{kind:"still"|"refframe", ref_id, captured_at, sha256,
 *                    evidence_id|null}, frames_matched}],
 *    "meta":{query, k, since, bbox, model, dim,
 *            cameras:{indexed:N, eligible:M, excluded_by_policy:X,
 *                     note:"N of M cameras indexed"},
 *            candidates:{examined, limit, exhausted},
 *            coverage:<vecindex coverage>}}
 *
 * `candidates.exhausted` is false when the KNN's candidate limit was reached
 * before k cameras were found — the answer is then the best of what was
 * looked at, and says so, rather than reading as "no more cameras match".
 *
 * Every camera is re-checked against camera_record_indexable() at query time,
 * so a channel removed from JO_CAMERA_INDEX_CHANNELS stops answering at once,
 * not on the next rebuild.
 *
 * 503 with {"error":"visual_search_unavailable","reason":…} when the sidecar
 * is not configured or does not answer, the index has not been built, was
 * refused, or the sidecar now serves a different model or width than the
 * index holds. 400 for a missing q or a malformed bbox/since/k.
 *
 * Embeds the query: run it OFF the event loop (httpd.c sem_spawn). Camera rows
 * are platform corpus data with no tenant_id; `tenant` is unused. */
#ifndef JO_VISUALSEARCHAPI_H
#define JO_VISUALSEARCHAPI_H

#include "db.h"
#include <stddef.h>

char *visualsearchapi_route(db_handle *db, const char *tenant, const char *id,
                            const char *qs, int is_operator, int *status);

/* POST /api/cameras/visual-search/by-image — cameras whose frames look like
 * THIS frame (image→image), with the same grouping, filters and meta as the
 * text search. The JSON body names exactly one query frame:
 *
 *   {"still_id":"<camera_stills.id>"}   a stored still: its vector is LOADED
 *       from the index by the frame's sha256 (vecindex_load), nothing is sent
 *       to the sidecar. Its camera must pass camera_record_indexable() now
 *       (409 still_not_indexable) and the frame must already be embedded (409
 *       still_not_embedded). The still's own camera is left out of `data`
 *       (it matches itself at distance 0) and named in meta.excluded_camera.
 *   {"upload_id":"<uuid>"}             a committed client upload of THIS
 *       tenant (another tenant's id is 404, uploadapi.h's error model), role
 *       analyst+ like the upload routes (403). Its bytes are read from the
 *       evidence store, proven by sha256, and embedded with the sidecar's
 *       POST /embed/image for this query only — an upload is never written to
 *       the index, so it can never answer anyone else's search.
 *
 * plus optional "k", "bbox" (string "w,s,e,n" or [w,s,e,n]) and "since",
 * which the URL's query string may also carry (the body wins). meta.query is
 * {by:"still"|"upload", still_id|upload_id, camera_id?, sha256}.
 *
 * httpd.c calls visualsearchapi_by_image_args() ON the loop (parse + role
 * check only, no I/O) and hands its url-encoded result to
 * visualsearchapi_by_image_route() as `id` through sem_spawn: an upload query
 * reads a blob and calls the sidecar. */
#define VS_BYIMAGE_ARGS_MAX 600     /* = sizeof sem_arg.id in httpd.c */

char *visualsearchapi_by_image_args(const char *body, size_t len, const char *role,
                                    char *out, size_t cap, int *status);
char *visualsearchapi_by_image_route(db_handle *db, const char *tenant,
                                     const char *id, const char *qs,
                                     int is_operator, int *status);

#endif
