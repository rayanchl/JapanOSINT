/* core/image_vec.h — the camera IMAGE index: whole frames embedded by the
 * vision sidecar (scripts/vision_embed_server.py) so a still can be found by a
 * sentence ("snow on the road", "冠水した道路").
 *
 * WHAT IS INDEXED, AND WHAT IS NOT.
 *   - Frames already in the evidence blob store: camera_stills rows (kind
 *     "still") and media_assets image rows of a camera record (kind "media").
 *     Nothing is fetched for this index; a frame whose bytes were never
 *     preserved (sources.capture_evidence off) has nothing to embed and is
 *     counted as `no_bytes`, never guessed at.
 *   - ONLY cameras that pass camera_record_indexable() (core/camera_privacy.h):
 *     the public-infrastructure allowlist. Checked when a frame is embedded
 *     AND again when a search answers, so narrowing JO_CAMERA_INDEX_CHANNELS
 *     takes effect on the next query, not on the next rebuild.
 *   - Whole frames only. No face, person or identity feature, ever (see
 *     camera_privacy.h).
 *
 * ITS OWN VECTOR SPACE. The sidecar's model is not the text embedder's, so
 * image vectors live in image_vec and NEVER in intel_vec / entity_vec / any
 * text index; the sidecar is JO_VISION_EMBED_URL, not JO_EMBED_URL. Model and
 * dimension are recorded with the first write (lib/vecindex.h) and a sidecar
 * that changed either is refused, not mixed.
 *
 * KEYED BY CONTENT. The vec0 key is the frame's SHA-256: a static scene that
 * produced 48 byte-identical stills is embedded ONCE. Who points at a vector is
 * image_vec_ref, one row per (kind, ref_id):
 *
 *   CREATE TABLE image_vec_ref (
 *     kind TEXT NOT NULL,          -- still | media | refframe
 *     ref_id TEXT NOT NULL,        -- camera_stills.id / media_assets.id / …
 *     sha256 TEXT NOT NULL,        -- the vector's key
 *     camera_uid TEXT,             -- capture target id (camera_stills.camera_id)
 *     item_uid TEXT,               -- the intel_items row it belongs to
 *     captured_at TEXT,            -- camera_stills.captured_at format
 *     added_at TEXT NOT NULL,
 *     PRIMARY KEY(kind, ref_id));
 *
 * A ref exists only for a sha that has a vector (both are written in one
 * transaction), and a vector with no ref left is deleted: stills retention
 * (camera_stills_prune_camera) calls image_vec_unref_stills() on every prune,
 * and the pod's gc pass mops up whatever was deleted by any other route.
 *
 * The vec0 metadata, filterable inside the KNN: `ts` (epoch seconds of the
 * NEWEST ref's captured_at — a `since` filter can then never drop a frame that
 * still has a recent ref) and `kind` ("still" whenever any still points at the
 * sha, else the first ref's kind).
 *
 * INERT without JO_VISION_EMBED_URL: no table is created and nothing is sent. */
#ifndef JO_IMAGE_VEC_H
#define JO_IMAGE_VEC_H

#include "db.h"
#include "../lib/vecindex.h"

struct http_client;

#define IMAGE_VEC_KIND_STILL    "still"
#define IMAGE_VEC_KIND_MEDIA    "media"
#define IMAGE_VEC_KIND_REFFRAME "refframe"

/* EMBEDDING DRIFT. When a still's vector lands, camera_stills gets
 *   embed_drift      1 − cos(frame, the target's previous embedded frame)
 *   drift_from_mean  1 − cos(frame, mean of its last IMAGE_VEC_DRIFT_WINDOW)
 * NULL for a target's first embedded frame. The mean is the slow baseline
 * (a camera re-aimed, a lens fogging, snow settling); the previous frame is
 * the fast one. Both are separate from scene_changed, which is pHash/bytes
 * and keeps its own meaning. A frame embedded out of order (its bytes landed
 * late) is compared with what was embedded before it; later frames' values
 * are not rewritten. */
#define IMAGE_VEC_DRIFT_WINDOW 10

extern const vecindex_spec image_vec_spec;

/* JO_VISION_EMBED_URL, or NULL when unset/empty. */
const char *image_vec_base_url(void);

/* image_vec_ref + the vecindex meta/done/fail tables. Idempotent; the vec0
 * table itself is created by the first write, at the sidecar's dimension. */
void image_vec_migrate(db_handle *db);

/* 1 when the index exists, holds a dimension and is not refused. */
int image_vec_ready(db_handle *db);

/* May frames of this intel_items row be indexed and searched?
 * camera_record_indexable() on its properties and source id, plus — when
 * `need_feed` — proof that the row is a camera at all (camera_feed_resolve
 * finds a feed). 0 for an unknown uid. */
int image_vec_item_indexable(db_handle *db, const char *item_uid, int need_feed);

/* One pod tick: walk up to `max_walk` (<=0 → JO_IMAGE_EMBED_MAX_WALK, 512)
 * stills and as many media assets without a ref, embed the new frames, add
 * refs for frames already embedded, gc orphans. Returns frames embedded, 0
 * when inert, -1 on a sidecar/db failure or a refused index (recorded in
 * image_vec_meta). */
int image_vec_tick(db_handle *db, struct http_client *http, int max_walk);

/* Drop the still refs of `camera_id` whose camera_stills row is gone, and the
 * vector of any sha left with no ref. Runs inside the CALLER's transaction
 * (camera_stills_prune_camera's) — it opens none. A no-op when the index was
 * never created. Returns refs removed. */
int image_vec_unref_stills(db_handle *db, const char *camera_id);

/* Refs whose referent row is gone (any kind), then vectors with no ref, at
 * most `limit` vectors per call. Opens its own transaction. Returns vectors
 * removed. */
int image_vec_gc(db_handle *db, int limit);

/* Embed one text with the sidecar's POST /embed/text. 0 with *out (malloc'd),
 * *dim and model filled; -1 with `err` holding why (no sidecar, transport,
 * HTTP status, a malformed answer). `http` may be NULL. */
int image_vec_embed_text(struct http_client *http, const char *text,
                         float **out, int *dim, char *model, size_t mcap,
                         char *err, size_t ecap);

/* Embed ONE image with the sidecar's POST /embed/image — the query side of an
 * image→image search (POST /api/cameras/visual-search/by-image). Same contract
 * as image_vec_embed_text; a body the sidecar will not embed comes back as -1
 * with its per-image error ("not_an_image", "image_too_large") in `err`. */
int image_vec_embed_image(struct http_client *http, const unsigned char *bytes,
                          size_t len, float **out, int *dim, char *model,
                          size_t mcap, char *err, size_t ecap);

/* The bytes of a frame from the evidence blob store, proven to hash to `sha`
 * (the walk's own read). `rel` is the blob path when the caller has it, else
 * NULL and any evidence row of that content serves. NULL when there are none
 * or they are over the sidecar's per-image cap (*too_large set). */
unsigned char *image_vec_read_frame(db_handle *db, const char *sha,
                                    const char *rel, size_t *len, int *too_large);

#endif
