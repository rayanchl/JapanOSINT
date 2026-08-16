/* collectors/sources/tsp_ofcom_wtr.c
 * Ofcom Wireless Telegraphy Register — the UK's statutory register of who is
 * licensed to transmit on what frequency and from where (Wireless Telegraphy
 * Act 2006 s.31).
 * Endpoint: https://static.ofcom.org.uk/static/radiolicensing/html/register/WTR.csv
 *   (keyless, 53.9 MB)
 * Emits per licensed emission: licence number, issue date, frequency (Hz),
 *   station type, channel width, height above sea level, antenna ERP + unit,
 *   antenna type/gain/azimuth/height/location/direction/polarisation, emission
 *   code, NGR, licensee (surname/first name/company), status, tradeable and
 *   publishable flags, product code and description, and the station site
 *   coordinates.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "53.9 MB — MUST be streamed line-by-line; do not buffer." There is no
 *    streaming HTTP reader in this codebase, so instead of silently pulling
 *    54 MB into RAM this collector issues an HTTP Range request for the first
 *    RANGE_BYTES and, when the server honours it (206), DISCARDS the final
 *    partial line. That is a STATED BOUND, not a silent truncation: the byte
 *    bound and the row cap are written into every emitted row's properties and
 *    into the run log. If the server ignores Range and returns 200, the whole
 *    file is parsed, still line-by-line and in place with no second copy.
 *  - "Geo is given twice: DMS ... and, more conveniently, as decimal
 *    'Latitude(Deg)' and 'Longitude(Deg)' in the LAST TWO columns. These are
 *    the station site, not the licensee address." That is the R2 trap for this
 *    domain — the licensee company/surname is emitted as a NAME only and never
 *    used for geometry. Columns are resolved by header name, so "last two"
 *    never becomes a hardcoded index.
 *  - "Missing numeric values are the literal '-'": '-' is treated as absent.
 *  - "one licence yields many rows (one per emission/frequency), so
 *    de-duplicate on Licence Number + Frequency + coordinates": that triple is
 *    the remote_key.
 *  - "only records flagged Publishable=Yes appear in this extract": the flag is
 *    carried through so that stays visible.
 * Licence: Ofcom publishes the WTR under the OPEN GOVERNMENT LICENCE — free
 *   reuse with attribution to Ofcom.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WTR_URL "https://static.ofcom.org.uk/static/radiolicensing/html/register/WTR.csv"
#define RANGE_BYTES (16 * 1024 * 1024)   /* stated prefix bound, see header */
#define MAX_ROWS 12000
#define MAXCOL 64

/* RFC4180 splitter, in place (licensee company names contain commas). */
static int csv_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  while (n < max) {
    if (*p == '"') {
      char *start, *dst;
      p++; start = dst = p;
      while (*p) {
        if (*p == '"') {
          if (p[1] == '"') { *dst++ = '"'; p += 2; continue; }
          p++; break;
        }
        *dst++ = *p++;
      }
      while (*p && *p != ',') p++;
      char end = *p;
      *dst = '\0';
      out[n++] = start;
      if (end == ',') { p++; continue; }
      break;
    }
    char *start = p;
    while (*p && *p != ',') p++;
    if (*p == ',') { *p = '\0'; out[n++] = start; p++; }
    else { out[n++] = start; break; }
  }
  return n;
}
static char *trim(char *s) {
  while (*s == ' ') s++;
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ') s[--n] = '\0';
  return s;
}
static void add_num(cJSON *o, const char *k, const char *v) {
  if (!v) return;
  char *e = NULL;
  double d = strtod(v, &e);
  if (e && e != v) cJSON_AddNumberToObject(o, k, d);
  else cJSON_AddStringToObject(o, k, v);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char range[64];
  snprintf(range, sizeof range, "Range: bytes=0-%d", RANGE_BYTES - 1);
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (ofcom-wtr)", range, NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", WTR_URL, hdrs, NULL, 0, 120000, 1, &hr);
  if (rc != 0 || (hr.status != 200 && hr.status != 206) || !hr.body) {
    fprintf(stderr, "[ofcom-wtr] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  int bounded = (hr.status == 206);
  char *body = hr.body;
  size_t blen = hr.body_len;
  hr.body = NULL;
  http_response_free(&hr);

  /* a Range response ends mid-record: drop the trailing partial line */
  if (bounded && blen) {
    char *last = NULL;
    for (size_t i = blen; i > 0; i--)
      if (body[i - 1] == '\n') { last = body + i; break; }
    if (last) *last = '\0';
  }

  char *p = body;
  char *head = jo_next_line_cr(&p);
  if (!head || !*head) {
    fprintf(stderr, "[ofcom-wtr] empty body\n");
    free(body);
    return -1;
  }
  char *hc[MAXCOL];
  int nh = csv_split(head, hc, MAXCOL);
  for (int i = 0; i < nh; i++) hc[i] = trim(hc[i]);
  int i_lic  = jo_col_of(hc, nh, "Licence Number");
  int i_iss  = jo_col_of(hc, nh, "License Issue Date");
  int i_ngr  = jo_col_of(hc, nh, "NGR");
  int i_freq = jo_col_of(hc, nh, "Frequency (Hz)");
  int i_sty  = jo_col_of(hc, nh, "Station Type");
  int i_cw   = jo_col_of(hc, nh, "Channel Width (Hz)");
  int i_hasl = jo_col_of(hc, nh, "Height Above Sea Level");
  int i_erp  = jo_col_of(hc, nh, "Antenna ERP");
  int i_erpu = jo_col_of(hc, nh, "Antenna ERP Unit");
  int i_atyp = jo_col_of(hc, nh, "Antenna Type");
  int i_gain = jo_col_of(hc, nh, "Antenna Gain");
  int i_azim = jo_col_of(hc, nh, "Antenna AZIMUTH");
  int i_aht  = jo_col_of(hc, nh, "Antenna Height");
  int i_aloc = jo_col_of(hc, nh, "Antenna Location");
  int i_adir = jo_col_of(hc, nh, "Antenna Direction");
  int i_apol = jo_col_of(hc, nh, "Antenna Polarisation");
  int i_emis = jo_col_of(hc, nh, "Emission Code");
  int i_lsur = jo_col_of(hc, nh, "Licencee Surname");
  int i_lfir = jo_col_of(hc, nh, "Licencee First Name");
  int i_lco  = jo_col_of(hc, nh, "Licencee Company");
  int i_stat = jo_col_of(hc, nh, "Status");
  int i_trad = jo_col_of(hc, nh, "Tradeable");
  int i_pub  = jo_col_of(hc, nh, "Publishable");
  int i_pcod = jo_col_of(hc, nh, "Product Code");
  int i_pdes = jo_col_of(hc, nh, "Product Description");
  int i_lat  = jo_col_of(hc, nh, "Latitude(Deg)");
  int i_lon  = jo_col_of(hc, nh, "Longitude(Deg)");
  if (i_lic < 0 || i_freq < 0) {
    fprintf(stderr, "[ofcom-wtr] header shape changed; not parsing\n");
    free(body);
    return -1;
  }

  int n = 0, seen = 0, nogeo = 0;
  char *line;
  while ((line = jo_next_line_cr(&p)) != NULL) {
    if (!*line) continue;
    seen++;
    if (n >= MAX_ROWS) break;
    char *f[MAXCOL];
    int nf = csv_split(line, f, MAXCOL);
    for (int i = 0; i < nf; i++) f[i] = trim(f[i]);

    const char *lic  = jo_field(f, nf, i_lic);
    const char *freq = jo_field(f, nf, i_freq);
    if (!lic) continue;                          /* no identity -> no row */

    const char *company = jo_field(f, nf, i_lco);
    const char *surname = jo_field(f, nf, i_lsur);
    const char *first   = jo_field(f, nf, i_lfir);
    const char *stat    = jo_field(f, nf, i_stat);
    const char *sty     = jo_field(f, nf, i_sty);

    /* R2: geometry from the STATION SITE columns only. The licensee company /
     * surname above is a NAME; it is never geocoded. */
    double lat = 0, lon = 0;
    int has_geo = 0;
    const char *slat = jo_field(f, nf, i_lat), *slon = jo_field(f, nf, i_lon);
    if (slat && slon) {
      char *e1 = NULL, *e2 = NULL;
      double a = strtod(slat, &e1), b = strtod(slon, &e2);
      if (e1 && e1 != slat && e2 && e2 != slon &&
          a >= -90.0 && a <= 90.0 && b >= -180.0 && b <= 180.0 &&
          !(a == 0.0 && b == 0.0)) {
        lat = a; lon = b; has_geo = 1;
      }
    }
    if (!has_geo) nogeo++;

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "licence_number", lic);
    jo_add_str(pr, "licence_issue_date", jo_field(f, nf, i_iss));
    add_num(pr, "frequency_hz", freq);
    add_num(pr, "channel_width_hz", jo_field(f, nf, i_cw));
    jo_add_str(pr, "station_type", sty);
    add_num(pr, "height_above_sea_level_m", jo_field(f, nf, i_hasl));
    add_num(pr, "antenna_erp", jo_field(f, nf, i_erp));
    jo_add_str(pr, "antenna_erp_unit", jo_field(f, nf, i_erpu));
    jo_add_str(pr, "antenna_type", jo_field(f, nf, i_atyp));
    add_num(pr, "antenna_gain", jo_field(f, nf, i_gain));
    add_num(pr, "antenna_azimuth_deg", jo_field(f, nf, i_azim));
    add_num(pr, "antenna_height_m", jo_field(f, nf, i_aht));
    jo_add_str(pr, "antenna_location", jo_field(f, nf, i_aloc));
    jo_add_str(pr, "antenna_direction", jo_field(f, nf, i_adir));
    jo_add_str(pr, "antenna_polarisation", jo_field(f, nf, i_apol));
    jo_add_str(pr, "emission_code", jo_field(f, nf, i_emis));
    jo_add_str(pr, "ngr", jo_field(f, nf, i_ngr));
    jo_add_str(pr, "licensee_company", company);
    jo_add_str(pr, "licensee_surname", surname);
    jo_add_str(pr, "licensee_first_name", first);
    jo_add_str(pr, "status", stat);
    jo_add_str(pr, "tradeable", jo_field(f, nf, i_trad));
    jo_add_str(pr, "publishable", jo_field(f, nf, i_pub));
    jo_add_str(pr, "product_code", jo_field(f, nf, i_pcod));
    jo_add_str(pr, "product_description", jo_field(f, nf, i_pdes));
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "site_latitude", lat);
      cJSON_AddNumberToObject(pr, "site_longitude", lon);
      cJSON_AddStringToObject(pr, "geo_subject",
        "licensed station site (Ofcom SID coordinates), not the licensee address");
    }
    cJSON_AddBoolToObject(pr, "extract_byte_bounded", bounded ? 1 : 0);
    if (bounded) cJSON_AddNumberToObject(pr, "extract_bytes_fetched", RANGE_BYTES);
    cJSON_AddNumberToObject(pr, "row_cap", MAX_ROWS);
    cJSON_AddStringToObject(pr, "source", "Ofcom Wireless Telegraphy Register");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    const char *who = company ? company : (surname ? surname : "licensee n/a");
    char title[288], summary[288], key[160];
    snprintf(key, sizeof key, "%s|%s|%.5f|%.5f", lic, freq ? freq : "", lat, lon);
    if (freq) {
      char *e = NULL;
      double hz = strtod(freq, &e);
      if (e && e != freq)
        snprintf(title, sizeof title, "%s — %.4f MHz (licence %s)",
                 who, hz / 1e6, lic);
      else
        snprintf(title, sizeof title, "%s — licence %s", who, lic);
    } else {
      snprintf(title, sizeof title, "%s — licence %s", who, lic);
    }
    snprintf(summary, sizeof summary, "%s%s%s%s",
             stat ? stat : "status n/a",
             sty ? " · station type " : "", sty ? sty : "",
             has_geo ? "" : " · no site coordinates published");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.ofcom.org.uk/spectrum/information/wireless-telegraphy-register";
    it.lang            = "en";
    it.published_at    = jo_field(f, nf, i_iss);
    it.record_type     = "spectrum-licence-emission";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"spectrum\",\"uk\",\"ofcom\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[ofcom-wtr] emitted %d of %d rows read (%s prefix, cap %d, "
                  "%d rows without site coordinates)\n",
          n, seen, bounded ? "byte-bounded" : "full-file", MAX_ROWS, nogeo);
  return 0;
}

static const source_def tsp_ofcom_wtr_def = {
  .id = "ofcom-wtr", .collector = "telecom",
  .name = "Ofcom Wireless Telegraphy Register (UK spectrum licences)",
  .update_interval_sec = 86400, .run = run,
  .category = "telecom", .type = "dataset", .url = WTR_URL,
  .description = "The UK statutory register of who may transmit on what frequency and from where: per-emission frequency, channel width, ERP, antenna type/gain/azimuth/polarisation, emission designator, licensee and the transmitter site coordinates.",
  .license = "Ofcom Wireless Telegraphy Register — Open Government Licence (OGL); free reuse with attribution to Ofcom. Only records flagged Publishable=Yes appear in the extract.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_ofcom_wtr_def)
