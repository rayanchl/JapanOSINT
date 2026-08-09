/* US HHS OIG LEIE — List of Excluded Individuals and Entities.
 *
 * Endpoint : https://oig.hhs.gov/exclusions/downloadables/UPDATED.csv
 * Format   : RFC-4180 CSV, quoted fields, header on the first line. Dates are YYYYMMDD with
 *            00000000 as the null marker.
 * Verified : HTTP 200, header
 *            LASTNAME,FIRSTNAME,MIDNAME,BUSNAME,GENERAL,SPECIALTY,UPIN,NPI,DOB,ADDRESS,CITY,
 *            STATE,ZIP,EXCLTYPE,EXCLDATE,REINDATE,WAIVERDATE,WVRSTATE
 *            and records from the first byte, e.g. "1 BEST CARE, INC", OTHER BUSINESS, HOME
 *            HEALTH AGENCY, SAINT PAUL MN 55114, EXCLTYPE 1128b5, EXCLDATE 20230518.
 * Keyless  : yes.
 * Emits    : the excluded party (business name for entities, surname/first/middle for
 *            individuals — whichever the row populates), provider category and specialty, NPI
 *            and UPIN, the statutory exclusion basis (1128a1 = conviction of a programme-
 *            related crime, 1128b4 = licence revocation, …), exclusion, reinstatement and
 *            waiver dates, and the address as published.
 * Geometry : NONE (R2). City/state/ZIP are emitted as text; nothing is geocoded.
 * Licence  : US Government work, public domain. OIG asks users to verify a match against the
 *            online searchable database before acting — carried in properties.caveat.
 *
 * Notes    : the HTML landing page oig.hhs.gov/exclusions/exclusions_list.asp redirects to a
 *            Drupal page and carries no data — this downloadable is the record-bearing path.
 *            The full file is ~80k rows; the download is bounded with a Range request
 *            (JO_SANC_OIG_MB, default 4 MB) and, when the server honours it (HTTP 206), the
 *            trailing PARTIAL line is discarded rather than parsed into a half-row.
 */
#include "sanc_common.inc"
#include "../../lib/csv.h"

#define OIG_URL "https://oig.hhs.gov/exclusions/downloadables/UPDATED.csv"

/* Non-empty CSV cell, or NULL (csv_parse fills missing cells with ""). */
static const char *oig_cell(const cJSON *row, const char *key) {
  const cJSON *v = cJSON_GetObjectItem(row, key);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int mb = sanc_env_int("JO_SANC_OIG_MB", 4);
  char range[64];
  snprintf(range, sizeof range, "Range: bytes=0-%lld", (long long)mb * 1048576LL - 1LL);

  size_t len = 0;
  long status = 0;
  char *text = sanc_http_get(ctx, OIG_URL, "Accept: text/csv, */*", range, NULL,
                             120000, "hhs-oig-exclusions", &len, &status);
  if (!text) return -1;

  /* A ranged response ends mid-record; cut back to the last complete line so no
   * half-parsed row is ever emitted. */
  if (status == 206) {
    char *last = NULL;
    for (size_t i = len; i > 0; i--)
      if (text[i - 1] == '\n') { last = text + i; break; }
    if (last) *last = 0;
  }

  cJSON *rows = csv_parse(text, 1);
  free(text);
  if (!rows) return -1;

  int max_rows = sanc_env_int("JO_SANC_MAX_ROWS", 5000);
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    if (n >= max_rows) break;
    const char *bus = oig_cell(r, "BUSNAME");
    const char *last = oig_cell(r, "LASTNAME");
    const char *first = oig_cell(r, "FIRSTNAME");
    const char *mid = oig_cell(r, "MIDNAME");
    if (!bus && !last) continue;                   /* no fetched name -> no row */

    char display[400];
    if (bus) snprintf(display, sizeof display, "%s", bus);
    else snprintf(display, sizeof display, "%s%s%s%s%s", last,
                  first ? ", " : "", first ? first : "",
                  mid ? " " : "", mid ? mid : "");

    const char *general = oig_cell(r, "GENERAL");
    const char *specialty = oig_cell(r, "SPECIALTY");
    const char *npi = oig_cell(r, "NPI");
    const char *upin = oig_cell(r, "UPIN");
    const char *dob = oig_cell(r, "DOB");
    const char *addr = oig_cell(r, "ADDRESS");
    const char *city = oig_cell(r, "CITY");
    const char *state = oig_cell(r, "STATE");
    const char *zip = oig_cell(r, "ZIP");
    const char *excltype = oig_cell(r, "EXCLTYPE");
    const char *excldate = oig_cell(r, "EXCLDATE");
    const char *reindate = oig_cell(r, "REINDATE");
    const char *waiverdate = oig_cell(r, "WAIVERDATE");

    /* NPI/UPIN use all-zero as their null marker */
    if (npi && strspn(npi, "0") == strlen(npi)) npi = NULL;
    if (upin && strspn(upin, "0") == strlen(upin)) upin = NULL;

    char excl_iso[16] = {0}, rein_iso[16] = {0}, waiv_iso[16] = {0}, dob_iso[16] = {0};
    if (excldate) sanc_yyyymmdd(excldate, excl_iso, sizeof excl_iso);
    if (reindate) sanc_yyyymmdd(reindate, rein_iso, sizeof rein_iso);
    if (waiverdate) sanc_yyyymmdd(waiverdate, waiv_iso, sizeof waiv_iso);
    if (dob) sanc_yyyymmdd(dob, dob_iso, sizeof dob_iso);

    char where[256];
    where[0] = 0;
    {
      size_t j = 0;
      const char *parts[4];
      parts[0] = addr; parts[1] = city; parts[2] = state; parts[3] = zip;
      for (int i = 0; i < 4; i++) {
        if (!parts[i]) continue;
        size_t need = strlen(parts[i]) + (j ? 2 : 0);
        if (j + need >= sizeof where - 1) break;
        if (j) { strcpy(where + j, ", "); j += 2; }
        strcpy(where + j, parts[i]);
        j += strlen(parts[i]);
      }
    }

    char summary[400];
    snprintf(summary, sizeof summary, "Excluded%s%s%s%s%s%s",
             excl_iso[0] ? " " : "", excl_iso,
             excltype ? " under " : "", excltype ? excltype : "",
             general ? " · " : "", general ? general : "");

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "name", display);
    sanc_add(body, "entity_kind", bus ? "business" : "individual");
    sanc_add(body, "provider_category", general);
    sanc_add(body, "specialty", specialty);
    sanc_add(body, "npi", npi);
    sanc_add(body, "upin", upin);
    sanc_add(body, "date_of_birth", dob_iso[0] ? dob_iso : NULL);
    sanc_add(body, "address", where[0] ? where : NULL);
    sanc_add(body, "exclusion_authority", excltype);
    sanc_add(body, "exclusion_date", excl_iso[0] ? excl_iso : NULL);
    sanc_add(body, "reinstatement_date", rein_iso[0] ? rein_iso : NULL);
    sanc_add(body, "waiver_date", waiv_iso[0] ? waiv_iso : NULL);
    sanc_add(body, "list", "HHS OIG LEIE");
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "list", "HHS OIG LEIE");
    sanc_add(props, "entity_kind", bus ? "business" : "individual");
    sanc_add(props, "provider_category", general);
    sanc_add(props, "specialty", specialty);
    sanc_add(props, "npi", npi);
    sanc_add(props, "upin", upin);
    sanc_add(props, "city", city);
    sanc_add(props, "state", state);
    sanc_add(props, "zip", zip);
    sanc_add(props, "exclusion_authority", excltype);
    sanc_add(props, "exclusion_date", excl_iso[0] ? excl_iso : NULL);
    sanc_add(props, "reinstatement_date", rein_iso[0] ? rein_iso : NULL);
    sanc_add(props, "waiver_date", waiv_iso[0] ? waiv_iso : NULL);
    sanc_add(props, "caveat",
             "HHS OIG asks that an apparent match be verified against the online "
             "searchable exclusions database before it is acted on.");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char key[512];
    snprintf(key, sizeof key, "%s|%s|%s", display, excltype ? excltype : "",
             excl_iso[0] ? excl_iso : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = display;
    it.summary = summary;
    it.body = bj;
    it.lang = "en";
    it.published_at = excl_iso[0] ? excl_iso : NULL;
    it.record_type = "hhs-oig-exclusion";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"debarment\",\"exclusion\",\"healthcare\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  cJSON_Delete(rows);
  fprintf(stderr, "[hhs-oig-exclusions] emitted %d (http %ld, %d MB window)\n",
          n, status, mb);
  return 0;
}

static const source_def sanc_hhs_oig_exclusions_def = {
  .id = "hhs-oig-exclusions", .collector = "sanctions",
  .name = "US HHS OIG LEIE — excluded individuals and entities",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = OIG_URL,
  .description = "Every person and company barred from US federal healthcare programmes, with the statutory exclusion basis, provider category, NPI and address.",
  .license = "US Government work, public domain. OIG asks users to verify a match against the online searchable database before acting.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_hhs_oig_exclusions_def)
