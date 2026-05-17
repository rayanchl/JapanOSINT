/* core/linegeom.h — read-time line stitch + smooth (faithful port of
 * server/src/utils/transportStore.js stitchAndSmoothLines + chaikinSmooth +
 * simplifyPolyline). Closes the documented "raw fragments" deviation in the
 * transport read path: degree-2 endpoint stitch → Chaikin×4 → RDP(ε1e-6).
 * `mode=="bus"` passes through untouched (MLIT N07 already smooth). */
#ifndef JO_LINEGEOM_H
#define JO_LINEGEOM_H
#include "../third_party/cJSON.h"

/* Takes a cJSON array of LineString Feature objects; returns a NEW cJSON
 * array (caller cJSON_Delete) of stitched+smoothed lines followed by any
 * non-LineString passthrough features. `in` is read-only (not freed). */
cJSON *linegeom_stitch_smooth(cJSON *in, const char *mode);

#endif
