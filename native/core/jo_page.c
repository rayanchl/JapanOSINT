/* core/jo_page.c — see jo_page.h. */
#include "jo_page.h"

cJSON *jo_page_fields(cJSON *o, long long limit, long long offset,
                      long long count, long long total, int has_more,
                      const char *count_key) {
  if (!o) return NULL;
  if (limit >= 0) cJSON_AddNumberToObject(o, "limit", (double)limit);
  else            cJSON_AddNullToObject(o, "limit");
  cJSON_AddNumberToObject(o, "offset", (double)offset);
  cJSON_AddNumberToObject(o, count_key ? count_key : "count", (double)count);
  if (total >= 0) cJSON_AddNumberToObject(o, "total", (double)total);
  else            cJSON_AddNullToObject(o, "total");
  cJSON_AddBoolToObject(o, "has_more", has_more ? 1 : 0);
  return o;
}

cJSON *jo_page_new(long long limit, long long offset, long long count,
                   long long total, int has_more) {
  return jo_page_fields(cJSON_CreateObject(), limit, offset, count, total,
                        has_more, NULL);
}
