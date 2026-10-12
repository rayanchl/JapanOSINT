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
 * (several stills can share one content-addressed vector):
 *
 *   {"data":[{camera_id, item_uid, view, name, lat, lon, distance, cosine,
 *             still:{id, captured_at, sha256}, frames_matched}],
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

char *visualsearchapi_route(db_handle *db, const char *tenant, const char *id,
                            const char *qs, int is_operator, int *status);

#endif
