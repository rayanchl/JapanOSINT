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
#include "od_shared.inc"

#define SID "socrata-nj-pension-members"
/* `$offset=0` seeds lib/pagewalk.c's offset walk — pw_walk only ever advances a
 * parameter the URL already carries, and never invents one. `$order=:id` makes
 * the sequence total. Measured 2026-09-19: 401,000 members upstream, of which
 * the single-page collector kept 100. */
static const char *URL =
  "https://data.nj.gov/resource/44xg-bswk.json"
  "?$limit=100&$offset=0&$order=:id";

/* pw_emit_fn: one page, reporting what it CONTAINED in `seen` — pw_walk drives
 * both continuation and disclosure off that, never off the emitted count. */
static int emit_page(const source_ctx *ctx, intel_sink *sink, const char *id,
                     cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)id; (void)ud;
  *seen = 0;
  if (!cJSON_IsArray(doc)) return 0;
  *seen = cJSON_GetArraySize(doc);

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

    /* Identity (rule 4b): without uid OR remote_key core/intel.c refuses the
     * item, so this source parsed 100 members and stored 0 on every run. The
     * dataset has no id column; name + enrollment date + location code is the
     * upstream's own distinguishing tuple and is stable across snapshots.
     * as_of_date alone would collapse the whole quarterly file onto one uid. */
    char rk[320];
    snprintf(rk, sizeof rk, "%s|%s|%s|%s",
             last ? last : "-", first ? first : "-",
             od_s(r, "enrollment_date") ? od_s(r, "enrollment_date") : "-",
             od_s(r, "location_code") ? od_s(r, "location_code") : "-");

    intel_item it = {0};
    it.remote_key = rk;
    it.title = title;
    it.summary = emp;
    it.published_at = od_s(r, "as_of_date");
    it.link = URL;
    it.record_type = "public-pension-member";
    it.tags_json = "[\"opendata\",\"person\",\"us\"]";
    n += od_emit(sink, &it, props);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_wrc(SID, pw_walk(ctx, sink, SID, URL, pw_fetch_json,
                             emit_page, NULL));
}

static const source_def od_socrata_nj_pension_def = {
  .id = SID, .collector = "government",
  .name = "New Jersey YourMoney active pension members (Socrata)",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.nj.gov/resource/44xg-bswk.json?$limit=100&$order=:id",
  .description = "NJ Treasury transparency register of active public-employee pension members: name, employer, retirement system and enrollment date",
  .license = "New Jersey YourMoney transparency portal, public record (contains named public employees)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_nj_pension_def)
