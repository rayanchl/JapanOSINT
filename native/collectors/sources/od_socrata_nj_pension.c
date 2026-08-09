/* New Jersey YourMoney active pension members (Socrata row tier).
 * Endpoint: https://data.nj.gov/resource/44xg-bswk.json?$limit=100
 * Bare JSON array, refreshed quarterly (as_of_date). Emits, per member, the
 * fields the API returned: as_of_date, member_last_name, member_first_name,
 * enrollment_date, employer_name, location_code, retirement_system.
 * No coordinates are published, so has_geo is never set (R2). Keyless.
 *
 * ATTRIBUTION / SENSITIVITY: this register is published deliberately by the
 * New Jersey Treasury on its YourMoney transparency portal and is a public
 * record. It nevertheless contains NAMED INDIVIDUALS - public employees only
 * - so it is a name-to-public-employer pivot and should be treated as such.
 * Licence: New Jersey YourMoney transparency portal, public record. */
#include "od_shared.c"

#define SID "socrata-nj-pension-members"
static const char *URL = "https://data.nj.gov/resource/44xg-bswk.json?$limit=100";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    const char *last = od_s(r, "member_last_name");
    const char *first = od_s(r, "member_first_name");
    const char *emp = od_s(r, "employer_name");
    if (!last && !first) continue;             /* no name -> no row */

    char title[320];
    if (last && first) snprintf(title, sizeof title, "%s, %s", last, first);
    else               snprintf(title, sizeof title, "%s", last ? last : first);

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, r);

    intel_item it = {0};
    it.title = title;
    it.summary = emp;
    it.published_at = od_s(r, "as_of_date");
    it.link = URL;
    it.record_type = "public-pension-member";
    it.tags_json = "[\"opendata\",\"person\",\"us\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_socrata_nj_pension_def = {
  .id = SID, .collector = "government",
  .name = "New Jersey YourMoney active pension members (Socrata)",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.nj.gov/resource/44xg-bswk.json?$limit=100",
  .description = "NJ Treasury transparency register of active public-employee pension members: name, employer, retirement system and enrollment date",
  .license = "New Jersey YourMoney transparency portal, public record (contains named public employees)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_nj_pension_def)
