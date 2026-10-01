/* collectors/sources/reg2_us_irs_eo.c
 *
 * IRS Exempt Organizations Business Master File — the authoritative US register
 * of tax-exempt nonprofits.
 *   GET https://www.irs.gov/pub/irs-soi/eo1.csv     (eo1 = Northeast region;
 *       eo2/eo3/eo4 cover the rest, eo_pr.csv etc. for territories)
 *
 * This used to ask for a byte RANGE and parse the first 2 MB. Measured
 * 2026-08-24: **irs.gov ignores Range entirely** — `curl -r 0-999` answers
 * HTTP 200 with all 48,629,769 bytes. So the collector was already paying for
 * the whole 48.6 MB file every run, throwing 46 MB of it away unparsed, and
 * then capping the survivors at 5,000 rows. eo1.csv holds 278,014 records:
 * 1.8% reached the sink and nothing in the output said so.
 *
 * The body is now parsed in full. It is parsed in LINE-ALIGNED CHUNKS rather
 * than in one csv_parse call, because a 48 MB CSV inflated into one cJSON tree
 * is roughly half a gigabyte of small allocations; the chunk size is a memory
 * guard on the parse, not a bound on records — every chunk's rows are emitted
 * and the walk continues to end-of-file. Nothing outside the fetched bytes is
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
/* exhaustive-ok: parse-buffer size, not a record bound — every chunk is emitted and the walk runs to EOF */
#define IRS_CHUNK 4000000      /* bytes of CSV handed to csv_parse at a time */

/* Emit every row of one parsed chunk. Returns rows emitted. */
static int irs_emit_rows(intel_sink *sink, const cJSON *rows) {
  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, rows) {
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
  return n;
}

static int irs_run(const source_ctx *ctx, intel_sink *sink) {
  /* No Range header: irs.gov ignores it (measured — see the file header), so
   * asking for one only made the collector believe it had a slice when it had
   * the whole file. Ask for what we intend to read. */
  const char *hdrs[] = { "Accept: text/csv", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", IRS_URL, hdrs, NULL, 0, 180000, 1, &hr);
  if (rc != 0 || !hr.body || (hr.status != 200 && hr.status != 206)) {
    fprintf(stderr, "[us-irs-exempt-orgs] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *text = hr.body;
  size_t len = hr.body_len;
  long status = hr.status;
  hr.body = NULL;
  http_response_free(&hr);

  /* The header line is prepended to every chunk so each chunk parses into
   * header-keyed objects exactly as one whole-file parse would. */
  const char *nl = memchr(text, '\n', len);
  if (!nl) {
    fprintf(stderr, "[us-irs-exempt-orgs] no header line in %zu bytes\n", len);
    free(text);
    return -1;
  }
  size_t hdr_len = (size_t)(nl - text) + 1;

  /* A trailing partial line means the transfer stopped short of end-of-file.
   * Cut it (never parse half a record) but remember that it happened — a body
   * that ends mid-row is a real shortfall and gets disclosed below. */
  size_t body_end = len;
  while (body_end > hdr_len && text[body_end - 1] != '\n') body_end--;
  int short_body = (body_end != len);

  int n = 0, chunks = 0;
  size_t pos = hdr_len;
  while (pos < body_end) {
    size_t end = pos + IRS_CHUNK;
    if (end >= body_end) end = body_end;
    else {
      size_t back = end;
      while (back > pos && text[back - 1] != '\n') back--;
      if (back > pos) end = back;      /* line-align; a pathological single
                                        * line longer than IRS_CHUNK is taken
                                        * whole rather than split */
      else { const char *e = memchr(text + pos, '\n', body_end - pos);
             end = e ? (size_t)(e - text) + 1 : body_end; }
    }

    size_t clen = end - pos;
    char *chunk = (char *)malloc(hdr_len + clen + 1);
    if (!chunk) break;                 /* out of memory — disclosed below */
    memcpy(chunk, text, hdr_len);
    memcpy(chunk + hdr_len, text + pos, clen);
    chunk[hdr_len + clen] = 0;

    cJSON *rows = csv_parse(chunk, 1);
    free(chunk);
    if (!rows) { fprintf(stderr, "[us-irs-exempt-orgs] csv parse failed at byte %zu\n", pos); break; }
    n += irs_emit_rows(sink, rows);
    cJSON_Delete(rows);
    chunks++;
    pos = end;
  }
  int stopped_early = (pos < body_end);
  free(text);

  if (short_body || stopped_early)
    jo_trunc_notice(sink, "us-irs-exempt-orgs", IRS_URL, n, -1,
                    short_body
                      ? "the response body ended mid-record, so the transfer "
                        "did not reach the end of the file"
                      : "the chunked parse stopped before end-of-file "
                        "(allocation or CSV parse failure)",
                    "re-run the collector; the BMF is a static monthly file, "
                    "so a complete run supersedes a short one");

  fprintf(stderr, "[us-irs-exempt-orgs] emitted %d rows from %zu bytes "
                  "(status %ld, %d chunk(s))%s\n",
          n, len, status, chunks,
          (short_body || stopped_early) ? " (TRUNCATED — notice emitted)" : "");
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
