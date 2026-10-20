/* core/storyapi.h — GET /api/stories/:id (see storyapi.c). */
#ifndef JO_STORYAPI_H
#define JO_STORYAPI_H
#include "db.h"

/* The semsearch_route_fn shape (semsearchapi.h): `id` is the story id,
 * `qs` carries limit/offset. 200 / 400 / 404 / 500 in *status. */
char *storyapi_get(db_handle *db, const char *tenant, const char *id,
                   const char *qs, int is_operator, int *status);

#endif
