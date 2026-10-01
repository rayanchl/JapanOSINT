/* lib/arcgis_dir.h — emit the services an ArcGIS REST services DIRECTORY
 * publishes.
 *
 * A directory root (`…/rest/services?f=json`) answers
 * {folders:["Hydro","Hazards",…], services:[{name,type},…]}. The folders are
 * bare strings and most roots carry no services of their own, so a row that
 * points a JSON-array emitter at "folders" fetches a live directory and emits
 * nothing, forever: a list of strings is not a list of records. The records a
 * directory exists to publish are its services — at the root and one hop down
 * in each folder's own listing, where `name` is already "Folder/Service".
 * (ArcGIS Server folders do not nest.)
 *
 * Each service keeps every field the upstream sent, plus two composed only from
 * upstream values: `id` = name/type (one service is routinely published as both
 * a MapServer and a FeatureServer under one name) and `service_url` =
 * base/name/type, which is how ArcGIS itself addresses a service. A folder whose
 * listing fails is disclosed as a collector-truncation-notice, not skipped.
 *
 * Returns 0 on success, -1 when the root could not be fetched or when every
 * folder failed and nothing was emitted. */
#ifndef JO_ARCGIS_DIR_H
#define JO_ARCGIS_DIR_H

#include "../source.h"

/* `base` is the directory root WITHOUT a query string, e.g.
 * "https://coast.noaa.gov/arcgis/rest/services". */
int arcgis_dir_walk(const source_ctx *c, intel_sink *s, const char *source_id,
                    const char *base, const char *record_type,
                    const char *lang, const char *tags_json);

#endif /* JO_ARCGIS_DIR_H */
