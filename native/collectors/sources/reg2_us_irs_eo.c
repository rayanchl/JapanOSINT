/* collectors/sources/reg2_us_irs_eo.c
 *
 * IRS Exempt Organizations Business Master File — the authoritative US register
 * of tax-exempt nonprofits.
 *   GET https://www.irs.gov/pub/irs-soi/eo1.csv     (eo1 = Northeast region;
 *       eo2/eo3/eo4 cover the rest, eo_pr.csv etc. for territories)
 *
 * The full region file is tens of megabytes and is refreshed monthly, so this
 * collector asks for a byte RANGE (HTTP 206) instead of buffering the whole
 * file, truncates the received bytes at the last complete line, and parses only
 * that. Servers that ignore Range and answer 200 are handled by truncating the
 * body to the same cap before parsing. Nothing outside the fetched bytes is
 * ever emitted, and no row is synthesized.
 *
 * Emits per CSV row, verbatim: EIN, NAME, ICO, STREET, CITY, STATE, ZIP,
 * SUBSECTION (the 501(c) paragraph), RULING (YYYYMM), ASSET_AMT, INCOME_AMT,
 * REVENUE_AMT, NTEE_CD and every other non-empty column present in the header.
 * The file carries no coordinates, so no row claims geo and the street address
 * is never geocoded (R2).
 *
 * Keyless. Licence: US Government work, public domain.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/csv.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define IRS_URL   "https://www.irs.gov/pub/irs-soi/eo1.csv"
#define IRS_BYTES 2000000      /* leading slice requested per run (~15k rows) */
#define IRS_MAX   5000         /* rows emitted per run                        */

static int irs_run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Range: bytes=0-1999999", "Accept: text/csv", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", IRS_URL, hdrs, NULL, 0, 60000, 1, &hr);
  if (rc != 0 || !hr.body || (hr.status != 200 && hr.status != 206)) {
    fprintf(stderr, "[us-irs-exempt-orgs] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *text = hr.body;
  size_t len = hr.body_len;
  hr.body = NULL;
  http_response_free(&hr);

  /* Cap and cut at the last complete line so no partial record is parsed. */
  if (len > IRS_BYTES) { len = IRS_BYTES; text[len] = 0; }
  size_t cut = len;
  while (cut > 0 && text[cut - 1] != '\n') cut--;
  if (cut > 0) text[cut] = 0;

  cJSON *rows = csv_parse(text, 1);
  free(text);
  if (!rows) { fprintf(stderr, "[us-irs-exempt-orgs] csv parse failed\n"); return -1; }

  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, rows) {
    if (n >= IRS_MAX) break;
    if (!cJSON_IsObject(row)) continue;
    const char *name = jo_sv(row, "NAME");
    const char *ein  = jo_sv(row, "EIN");
    if (!name || !ein) continue;              /* no real record -> no row (R1) */

    const char *city  = jo_sv(row, "CITY");
    const char *state = jo_sv(row, "STATE");
    const char *sub   = jo_sv(row, "SUBSECTION");
    const char *ntee  = jo_sv(row, "NTEE_CD");

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "us-irs-exempt-orgs");
    cJSON_AddStringToObject(props, "source", "irs.gov/pub/irs-soi/eo1.csv");
    cJSON_AddStringToObject(props, "region_file", "eo1 (Northeast)");
    for (const cJSON *f = row->child; f; f = f->next) {
      if (!f->string || !f->string[0]) continue;
      if (!cJSON_IsString(f) || !f->valuestring || !f->valuestring[0]) continue;
      cJSON_AddStringToObject(props, f->string, f->valuestring);
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[420];
    snprintf(summary, sizeof summary, "EIN %s%s%s%s%s%s%s%s%s%s",
             ein,
             city ? " · " : "", city ? city : "",
             (city && state) ? ", " : "", state ? state : "",
             sub ? " · 501(c)(" : "", sub ? sub : "", sub ? ")" : "",
             ntee ? " · NTEE " : "", ntee ? ntee : "");

    intel_item it = {0};
    it.remote_key      = ein;
    it.title           = name;
    it.summary         = summary;
    it.body            = pj;
    it.link            = IRS_URL;
    it.lang            = "en";
    it.record_type     = "us-exempt-org";
    it.properties_json = pj;
    it.tags_json       = "[\"registry\",\"nonprofit\",\"usa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(rows);
  fprintf(stderr, "[us-irs-exempt-orgs] emitted %d\n", n);
  return 0;
}

static const source_def reg2_us_irs_eo_def = {
  .id = "us-irs-exempt-orgs", .collector = "government",
  .name = "IRS Exempt Organizations Business Master File",
  .update_interval_sec = 604800, .run = irs_run,
  .category = "government", .type = "dataset",
  .url = IRS_URL,
  .description = "The authoritative US register of tax-exempt nonprofits — EIN, "
                 "legal name, address, 501(c) subsection, ruling date and "
                 "financials. Keyless; refreshed monthly by the IRS.",
  .license = "US Government work, public domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_us_irs_eo_def)
