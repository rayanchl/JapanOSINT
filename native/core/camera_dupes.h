/* core/camera_dupes.h — GET /api/admin/cameras/duplicates: PROPOSED duplicate
 * camera feeds, with the evidence for each proposal as data. Nothing is
 * merged, hidden or rewritten: an operator reads the proposals and decides.
 *
 * WHY. The camera registry is assembled from a dozen discovery channels and
 * hp tables, and the same physical camera turns up under several of them — an
 * operator's own feed and an aggregator re-hosting it, two registries of one
 * road authority. Each copy is captured, embedded and searched separately,
 * and a search answers "3 cameras" where there is one. Nothing could see it.
 *
 * WHAT IS COMPARED. Each capture target's LATEST frame: its newest still or
 * its reference frame (core/camera_refframe.h), whichever is newer. Only
 * cameras that pass camera_record_indexable() (camera_privacy.h) take part;
 * a target's views of one record are never paired with each other (a station
 * looking two ways is two views, not two feeds). Three signals, strongest
 * first, each a named constant below:
 *
 *   same_sha256  the latest frames are byte-identical: a re-host. Not
 *                narrowed by location (a re-host often carries different or
 *                no coordinates). A frame shared by more than
 *                CAM_DUP_SHA_GROUP_MAX cameras is a placeholder ("camera
 *                offline" image), not a re-host: its pairs are not proposed,
 *                and it is listed in meta.shared_frames_skipped with its
 *                camera count rather than dropped silently.
 *   phash        both frames carry a pHash (camera_stills.phash — only where
 *                the decoder tool exists) and their Hamming distance is
 *                <= CAM_DUP_PHASH_MAX;
 *   cosine       no pHash match, both frames have a vector in the image
 *                index and their cosine is >= CAM_DUP_COSINE_MIN (a measured
 *                pHash distance above the bound is still shown as evidence).
 *   phash and cosine pairs are CANDIDATES only when both cameras have
 *   coordinates within CAM_DUP_RADIUS_M of each other: comparing every pair
 *   of 100k cameras would be quadratic and would mostly measure "two roads
 *   in snow look alike".
 *
 *   {"data":[{signal, cameras:[{camera_id, item_uid, view, name, lat, lon,
 *                              frame:{kind, ref_id, sha256, captured_at,
 *                                     phash}} ×2],
 *             evidence:{same_sha256, phash_distance|null, cosine|null,
 *                       distance_m|null}}],
 *    "page":{limit, offset, count, total, has_more},
 *    "meta":{thresholds:{…}, cameras_examined, cameras_with_vector,
 *            cameras_with_phash, cameras_without_location,
 *            shared_frames_skipped:[{sha256, cameras}], note}}
 *
 * Ordered by signal (same_sha256, phash, cosine), then strength. ?limit=
 * (default 50, max 500) and ?offset=; `total` is every proposal under the same
 * thresholds. Operator-gated (operatorgate.h) and run OFF the loop (sem_spawn):
 * it reads every camera's latest frame and loads vectors. */
#ifndef JO_CAMERA_DUPES_H
#define JO_CAMERA_DUPES_H

#include "db.h"

#define CAM_DUP_PHASH_MAX     6       /* Hamming bits of 64                  */
#define CAM_DUP_COSINE_MIN    0.97    /* whole-frame embedding cosine        */
#define CAM_DUP_RADIUS_M      1000.0  /* candidate pairs, metres             */
#define CAM_DUP_SHA_GROUP_MAX 8       /* more sharing one frame = placeholder */

char *camera_dupes_route(db_handle *db, const char *tenant, const char *id,
                         const char *qs, int is_operator, int *status);

#endif
