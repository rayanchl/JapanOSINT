/* core/jo_page.h — the list page block, ONE builder.
 *
 * `{limit, offset, count, total, has_more}` was hand-built in eight files in
 * three placements (a "page" object, the response root, under "meta"), and a
 * failed COUNT was spelled two ways: `total: null` in most, `total: 0` in
 * sourceapi.c — a measured "nothing matched" where nothing was measured
 * (house rule 1; found 2026-11-07). One builder now writes the fields, so a
 * count that could not be taken reads `null` everywhere.
 *
 * It only WRITES; where the block sits is still each route's choice (pass the
 * object to fill). has_more is the caller's verdict, not re-derived here,
 * because the routes decide it differently on purpose (breach_store.c: a
 * short page is the last whatever the count says). */
#ifndef JO_PAGE_H
#define JO_PAGE_H
#include "../third_party/cJSON.h"

/* Adds limit, offset, <count_key>, total, has_more to `o` and returns `o`.
 *   limit < 0       -> "limit": null  (maintenanceapi.c: every row)
 *   total < 0       -> "total": null  (the count failed or was not taken)
 *   count_key NULL  -> "count"        (entityapi.c's merges says "returned") */
cJSON *jo_page_fields(cJSON *o, long long limit, long long offset,
                      long long count, long long total, int has_more,
                      const char *count_key);

/* A new {"limit","offset","count","total","has_more"} object. */
cJSON *jo_page_new(long long limit, long long offset, long long count,
                   long long total, int has_more);

#endif
