/* core/camera_refframe.h — ONE reference frame per indexable camera record,
 * so the camera image index (core/image_vec.h) covers cameras nobody opted
 * into stills capture.
 *
 * WHY. image_vec embeds frames already in the evidence store: camera_stills
 * rows, which exist only for cameras an operator opted in one by one
 * (capture_stills=1). Every other public-infrastructure camera — most of
 * them — had no frame to embed, so "flooded road" could only ever find the
 * handful being captured. A reference frame is the smallest thing that fixes
 * that: one image per camera RECORD, refreshed weekly, never a timeline.
 *
 * WHAT IS FETCHED, AND WHAT IS NOT.
 *   - Only records that pass camera_record_indexable() (camera_privacy.h) AND
 *     resolve to a feed (camera_feed_resolve() > 0). Candidates are camera
 *     rows (record_type 'camera' / 'camera-discovery') and rows opted into
 *     stills (capture_stills=1); the check runs on every row, so narrowing
 *     JO_CAMERA_INDEX_CHANNELS stops the next fetch.
 *   - The record's best-ranked target (camera_feed.h's first): one view, not
 *     every view.
 *   - Through camera_stills_fetch_frame(): the SAME fetch path stills use —
 *     hostgate's camera destination policy (LAN/metadata/link-local refused
 *     unless JO_CAMERA_ALLOW_LAN=1), its per-host politeness, the size cap and
 *     the one-complete-frame trim. Whole frames; no face or person feature.
 *   - Preserved as an evidence blob filed under the camera's uid and the
 *     pseudo-source "camera-refframe" (created with capture_evidence=1 on
 *     first boot, like uploadapi's "client-upload"; an operator who turns it
 *     off gets "not_preserved" recorded, never a vector of nothing).
 *
 * THE TABLE (camera_refframe_migrate; one row per record):
 *
 *   CREATE TABLE camera_refframes (
 *     item_uid TEXT PRIMARY KEY,   -- the intel_items row
 *     camera_uid TEXT NOT NULL,    -- its capture target id (camera_feed.h)
 *     url TEXT NOT NULL,           -- the feed the frame came from
 *     sha256 TEXT, blob_path TEXT, evidence_id TEXT, bytes INTEGER,
 *     content_type TEXT,
 *     fetched_at TEXT,             -- last success, camera_stills' format
 *     attempted_at TEXT NOT NULL,  -- last attempt, success or not
 *     last_error TEXT, fail_count INTEGER NOT NULL DEFAULT 0);
 *
 * A successful frame is what the _image_embed pod walks as kind "refframe":
 * ref_id = item_uid, camera_uid = the target id, captured_at = fetched_at. A
 * refresh that brings new bytes moves the ref to the new sha and the old
 * vector is collected once nothing points at it.
 *
 * THE POD, `_camera_refframe` (maintenance lane, hourly tick):
 *   - walks up to JO_REFFRAME_MAX_WALK (2000) candidate rows per tick from a
 *     rowid cursor kept in image_vec_meta ("refframe_cursor"); a short page
 *     ends the lap and the next tick starts over;
 *   - fetches at most JO_REFFRAME_PER_TICK (20) frames per tick and at most
 *     JO_REFFRAME_HOST_PER_TICK (2) per upstream host — the rest stay due;
 *   - a record is due when never attempted or last attempted more than
 *     JO_REFFRAME_MAX_AGE_SEC (604800 = weekly) ago, success or failure: a
 *     dead camera is retried weekly, not hourly;
 *   - writes its counts to image_vec_meta "refframe_last_tick", which the
 *     image index's coverage (and so /api/status's vectorIndexes) reports.
 *
 * INERT without JO_VISION_EMBED_URL: a frame nothing will embed is not worth
 * a request to somebody's camera. */
#ifndef JO_CAMERA_REFFRAME_H
#define JO_CAMERA_REFFRAME_H

#include "db.h"
#include "httpclient.h"
#include "../third_party/cJSON.h"

#define CAM_REFFRAME_SID      "_camera_refframe"
#define CAM_REFFRAME_EV_SID   "camera-refframe"   /* evidence pseudo-source  */

/* Table + the evidence pseudo-source. Idempotent. */
void camera_refframe_migrate(db_handle *db);

/* One tick (see above). Returns frames stored, 0 when inert or nothing was
 * due, -1 on a DB failure. `cancel` may be NULL. */
int camera_refframe_tick(db_handle *db, http_client *http, volatile int *cancel);

/* {"cameras_with_frame":N,"failing":F,"last_tick":{…}|null,
 *  "cursor":rowid} — merged into the image index's coverage. */
cJSON *camera_refframe_coverage(db_handle *db);

#endif
