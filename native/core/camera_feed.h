/* core/camera_feed.h — where, in a camera record's properties, the pixels are.
 *
 * WHY THIS EXISTS. camera_stills.c used to read six top-level keys
 * (snapshot_url, still_url, image_url, stream_url, thumbnail_url, url) and
 * nothing else. That matched the camera-discovery rows camera_store.c writes
 * and almost nothing else that carries a camera in this corpus:
 *
 *   - the hpengine camera tables (hp3b29a_webcams.c, hp3b30_jpcam.c,
 *     hp3b31_survcam.c — ~80k records) keep the UPSTREAM's field names, and
 *     hpengine FLATTENS them: `attributes.ImageURL`, `properties.ImageURL`,
 *     `links.imageDisplay`, `cameraimageurl`, `Thumbnail_Url`, `views.0.url`;
 *   - trn_digitraffic_weathercam_stations.c keeps one station per row with a
 *     `views[]` array, one image_url per PRESET — a station is several cameras
 *     pointing different ways, and the top level has no image at all;
 *   - trn_511_*_cameras.c the same shape with `views[].url`.
 *
 * So an operator could opt any of those in and get "no_feed_url" forever.
 *
 * WHAT IT RETURNS. One TARGET per capturable image stream in the record:
 *
 *   - at most one for the record itself (suffix ""), the best-ranked key among
 *     the record's non-array fields — the camera's own image;
 *   - one per element of every array of objects whose element carries an image
 *     key (suffix "views/<id>"), so each view gets its own capture timeline.
 *     <id> is the element's own id field when it has one (preset_id, id, …)
 *     and its index otherwise.
 *
 * Ranking, per group (case and `_`/`-` folded, so IMAGE_URL, ImageUrl and
 * image_url are one key): still-image keys, then stream keys, then thumbnail
 * keys, then `url` LAST — for most discovery channels `url` is the page the
 * camera is embedded in, not the image. When a record has per-view targets its
 * top-level `url` is not taken as a target of its own: it is that page.
 *
 * A small per-source override table handles shapes no generic rule should
 * guess at (an image key whose name says nothing, an index buried inside a key
 * name like `cameras_0_latestImage`). An override that yields a target wins;
 * one that yields nothing falls through to the generic rules rather than
 * hiding the record.
 *
 * Nothing here is fetched and nothing is invented: a target is a URL that is
 * literally in the record, and a record with none resolves to zero targets
 * (camera_stills then records "no_feed_url" against it, as before). */
#ifndef JO_CAMERA_FEED_H
#define JO_CAMERA_FEED_H

#include <stddef.h>

/* Targets beyond this are counted, not filled (the return value says how many
 * exist) so a caller can disclose the remainder rather than drop it silently. */
#define CAM_FEED_MAX_TARGETS 64

/* Separator between an intel_items uid and a view suffix in a capture target
 * id: "digitraffic-weathercam-stations|C01502#view=views/C0150201". Chosen to
 * be vanishingly unlikely inside a real uid (a uid can contain `#` — URL
 * fragments do), and searched for from the RIGHT. */
#define CAM_FEED_VIEW_SEP "#view="

typedef struct {
  char  *url;           /* malloc'd; the feed URL exactly as in the record     */
  char   suffix[112];   /* "" for the record's own feed, else "views/<id>"    */
  char   key[128];      /* the property it came from, e.g. "views.3.image_url" */
  char   label[128];    /* a human name for the view when the element has one */
  int    rank;          /* lower is better; see camera_feed.c                  */
} camera_feed_target;

/* Resolve `properties_json` (an intel_items.properties blob) into capture
 * targets. `source_id` selects a per-source override when one exists; NULL is
 * fine. Fills up to `cap` entries of `out` (which may be NULL when cap is 0)
 * and returns how many targets the record HAS — which can exceed `cap`.
 * Returns 0 for a record with no feed and on unparseable JSON. Duplicated URLs
 * collapse to the first (best-ranked) occurrence. */
int camera_feed_resolve(const char *properties_json, const char *source_id,
                        camera_feed_target *out, int cap);

/* Free the url of each of the first `n` targets. */
void camera_feed_free(camera_feed_target *t, int n);

/* Compose "<uid>" or "<uid>#view=<suffix>" into out. Returns 0 if it did not
 * fit (out is then empty). */
int camera_feed_target_id(const char *uid, const char *suffix,
                          char *out, size_t cap);

/* Split a target id back into its uid (malloc'd, caller frees) and, when there
 * is one, a pointer to the suffix INSIDE `target_id`. *suffix is set to NULL
 * for a plain uid. Returns NULL on OOM / NULL input. */
char *camera_feed_split(const char *target_id, const char **suffix);

#endif
