/* OpenDataSoft public catalogue - GeoNames cities with population > 1000.
 * Endpoint: https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/
 *           geonames-all-cities-with-a-population-1000/records?limit=100&offset=0&order_by=geoname_id
 * Row tier (results[]), 170,363 places. Emits, per record, every scalar field
 * the API returned (name, ascii_name, country_code, cou_name_en, population,
 * timezone, feature_code, admin codes, elevation, dem, modification_date).
 * R2: has_geo/lat/lon come only from the upstream coordinates{lon,lat} object
 * - no centroid or fallback point is ever substituted. Keyless.
 * Licence: GeoNames source data CC-BY 4.0; the ODS public hosting requires no
 * auth. */
#include "od_shared.inc"

#define SID "ods-public-geonames"
static const char *URL =
  "https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/"
  "geonames-all-cities-with-a-population-1000/records?limit=100&offset=0&order_by=geoname_id";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */
/* PAGED 2026-10-12. This made ONE request:
 * 171,176 places (total_count, measured 2026-10-12) and TWENTY were stored.
 * The ceiling here is the UPSTREAM's, not ours: ODS refuses offset+limit past
 * 10,000 ("Invalid value for sum of offset + limit API parameter"), so 100
 * pages of 100 is every record this API will serve and the remaining 161,176
 * are a disclosed shortfall. The complete set is one request to
 * .../exports/json, measured at 111 MB for all 171,176 rows -- that needs a
 * streaming parse this collector does not have, so it is named in the notice
 * as the remedy rather than silently attempted.
 */
/* ORDER_BY IS NOT COSMETIC ON AN OFFSET WALK (measured 2026-10-12). ODS
 * returns records in no guaranteed order, and this dataset updates live, so
 * paging by `offset` without a stable sort re-serves some records on a later
 * page and SKIPS others entirely. The skips are invisible; the re-serves show
 * up as a UID-COLLISION on the run line, which is how this was found: Velib
 * reported 1,519 emitted / 1,508 stored, and a `where=stationcode="13022"`
 * query proved the dataset holds exactly ONE record for the code that had
 * supposedly appeared twice. A collision that is really a skip is the worst
 * shape in this file -- the visible half is harmless and the invisible half is
 * lost data. `order_by` on the row's own id makes the walk a stable sequence.
 */
#define ODS_PAGE_MAX 100  /* exhaustive-ok: page ceiling handed to od_walk_rows_max(); a stop at it files a collector-truncation-notice */
static const char *const TITLE_KEYS[] = { "name", "ascii_name", NULL };
static const char *const SUM_KEYS[]   = { "cou_name_en", "country_code", NULL };

/* THE COMPLETE SET, over the records API's own 10,000 ceiling (2026-10-20).
 *
 * The paged walk above is kept as the comment it is: it reached every record
 * that API will serve and disclosed the other 161,176. This now reads the
 * whole dataset instead, and the route matters:
 *
 *   /exports/json   a BARE ROOT ARRAY of all 171,176 records. Buffering 111 MB
 *                   and then handing it to cJSON needs the text AND the parsed
 *                   tree at once — about a gigabyte for one collector run.
 *   /exports/jsonl  the same records as NDJSON, one complete object per line.
 *                   Verified 2026-10-20 (`exports` honours `limit`, so the
 *                   route and the record shape cost five rows to check, not
 *                   111 MB). Parsed a line at a time, peak memory is the body
 *                   plus ONE record.
 *
 * lib/jsonstream.c would stream the array in bounded memory and is the right
 * tool for a row whose rows it can shape — but its emit has a fixed shape with
 * no id key, no date key and no geometry, and this row needs all three
 * (`geoname_id`, `modification_date`, and R2 coordinates). Streaming it would
 * buy complete coverage at the price of 171,176 mis-keyed, undated, unplaced
 * rows, so the body is buffered under a per-call ceiling instead
 * (http_request_max, core/httpclient.h) and every field mapping below is the
 * one the paged version measured. A keyed streaming emit would make the
 * ceiling unnecessary; that is the follow-up, not this commit.
 *
 * The ceiling is this row's alone and is restored when the fetch returns, so
 * the 64 MB default that guards the other 48,000 sources is untouched. */
#define GEONAMES_JSONL \
  "https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/" \
  "geonames-all-cities-with-a-population-1000/exports/jsonl"
/* 111 MB measured; 192 MB leaves room for the dataset to grow without the
 * fetch failing closed, and a body past it is still refused rather than
 * truncated.
 *
 * This is the FALLBACK, not the policy. The ceiling an operator sets for this
 * source in the Sources settings tab arrives on `ctx->body_ceiling`
 * (core/source_settings.c, key http_body_ceiling_bytes) and wins, because how
 * much memory one collector run may hold is a decision about the machine it
 * runs on and belongs to whoever owns that machine. A deployment that sets
 * nothing gets this number, which is the one that was measured. */
#define GEONAMES_MAX_BODY (192u * 1024u * 1024u)

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "gazetteer-place";
  sp.tags_json = "[\"opendata\",\"gazetteer\",\"geonames\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "geoname_id";
  sp.date_key = "modification_date";

  size_t ceiling = ctx->body_ceiling ? ctx->body_ceiling : GEONAMES_MAX_BODY;
  char *body = feed_get_text_max(ctx->http, GEONAMES_JSONL, 600000, ceiling);
  /* THE EXPORT IS PREFERRED, NOT REQUIRED, and that is deliberate.
   *
   * Measured 2026-10-20 from the authoring container: three attempts, three
   * "SSL connect error", 0 bytes — the relay this session fetches through
   * drops a transfer this large (/root/.ccr/README.md, ws_closed_mid_exchange
   * against public.opendatasoft.com). A five-row `exports/jsonl?limit=5` ask
   * succeeds, so the ROUTE and the record shape are verified and only the full
   * 111 MB transfer is not.
   *
   * Making the export mandatory would therefore have traded 10,000 real
   * records for ZERO wherever the big fetch cannot complete: the run above
   * returned rc=-1, opened a status_bad anomaly and backed the row off for a
   * week. So a failed export falls back to the paged walk — the same 100 pages
   * main measured — and the run says which path it took. Where the export IS
   * reachable this row stores all 171,176; where it is not it stores the
   * 10,000 the records API will serve and discloses the rest, which is exactly
   * what it did before. Neither outcome is silent. */
  if (!body) {
    fprintf(stderr, "[" SID "] exports/jsonl unreachable; falling back to the "
                    "paged records API (10,000 ceiling)\n");
    int paged = od_walk_rows_max(ctx, sink, SID, URL, "results", &sp,
                                 ODS_PAGE_MAX);
    if (paged >= 0)
      jo_trunc_notice(sink, SID, GEONAMES_JSONL, paged, 171176,
                      "the complete export could not be fetched this run, so "
                      "this is the paged records API, which the upstream "
                      "refuses past offset+limit 10,000",
                      "re-run from a network that can carry the ~111 MB "
                      "exports/jsonl file, which holds every record");
    return od_rc(SID, paged);
  }

  /* One reusable single-element array, so each line goes through the SAME
   * od_emit_rows() field mapping as the paged walk rather than a second copy
   * of it. The object is detached before the array is freed — od_emit_rows
   * does not take ownership. */
  cJSON *one = cJSON_CreateArray();
  if (!one) { free(body); return od_rc(SID, -1); }

  int n = 0;
  long lines = 0, malformed = 0;
  for (char *p = body; p && *p;) {
    char *nl = strchr(p, '\n');
    if (nl) *nl = 0;
    char *line = p;
    p = nl ? nl + 1 : NULL;
    while (*line == ' ' || *line == '\r' || *line == '\t') line++;
    if (!*line) continue;
    lines++;
    cJSON *rec = cJSON_Parse(line);
    if (!cJSON_IsObject(rec)) {        /* a LOST RECORD, counted not hidden */
      malformed++;
      cJSON_Delete(rec);
      continue;
    }
    cJSON_AddItemToArray(one, rec);
    n += od_emit_rows(sink, one, &sp);
    cJSON_DetachItemViaPointer(one, rec);
    cJSON_Delete(rec);
  }
  cJSON_Delete(one);
  free(body);

  fprintf(stderr, "[" SID "] %ld jsonl line(s), %ld malformed, %d emitted\n",
          lines, malformed, n);
  if (malformed)
    jo_trunc_notice(sink, SID, GEONAMES_JSONL, n, lines,
                    "one or more NDJSON lines did not parse as a JSON object, "
                    "so those records were lost rather than stored",
                    "re-read the export; if it persists, the upstream is "
                    "emitting malformed lines and should be reported");
  return od_rc(SID, n);
}

static const source_def od_ods_geonames_def = {
  .id = SID, .collector = "government",
  .name = "OpenDataSoft public catalogue - GeoNames cities >1000",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/geonames-all-cities-with-a-population-1000/records?limit=100&offset=0&order_by=geoname_id",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "Keyless global gazetteer rows (place name, country, population, timezone, feature code) with upstream coordinates",
  .license = "GeoNames data CC-BY 4.0 (hosted on public.opendatasoft.com)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_geonames_def)
