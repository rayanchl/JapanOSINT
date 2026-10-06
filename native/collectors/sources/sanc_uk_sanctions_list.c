/* UK Sanctions List (FCDO) — the statutory designations under the Sanctions and
 * Anti-Money Laundering Act 2018.
 *
 * Endpoint : https://sanctionslist.fcdo.gov.uk/docs/UK-Sanctions-List.xml
 * Format   : XML, record delimiter <Designation>; names are split across Name1..Name6 with a
 *            NameType of "Primary Name" or "Alias".
 * Verified : HTTP 200, 21.7 MB, DateGenerated 31/07/2026, 6,315 <Designation> blocks. First:
 *            AFG0001 "HAJI KHAIRULLAH HAJI SATTAR MONEY EXCHANGE" (alias "Haji Alim Hawala"),
 *            UN ref TAe.010, designated 29/06/2012, regime The Afghanistan (Sanctions)
 *            (EU Exit) Regulations 2020, Asset freeze.
 * Keyless  : yes.
 * Emits    : primary name (Name1..Name6 joined in order), every alias with its alias strength,
 *            the regime, whether the target is an Individual/Entity/Ship, the designation
 *            source (UN/UK), the sanctions imposed and the individual indicator flags that are
 *            true, date designated and last updated, UK statement of reasons, other
 *            information, addresses as published, dates of birth, nationalities, positions,
 *            passport numbers and national identifiers, plus the UniqueID / OFSIGroupID /
 *            UN reference.
 * Geometry : NONE (R2). Addresses are published text; nothing is geocoded.
 * Licence  : Open Government Licence v3.0.
 *
 * Notes    : this is a DIFFERENT list from the OFSI ConList.csv already collected in
 *            sanctions_world.c (ofsistorage.blob.core.windows.net). The FCDO list is the legal
 *            designation register and includes travel bans and arms-embargo targets that the
 *            OFSI asset-freeze list does not carry. The CSV twin at .../UK-Sanctions-List.csv
 *            has a one-line "Report Date:" preamble BEFORE the header row, which breaks a
 *            naive header parse — the XML avoids that entirely.
 *
 * NAME MATCHING: the list is emitted, not matched. Matching is the platform's job.
 */
#include "sanc_common.inc"
#include <limits.h>

#define UK_SANC_URL "https://sanctionslist.fcdo.gov.uk/docs/UK-Sanctions-List.xml"

/* No per-record sub-array bounds. There were five (aliases 200, measures 64,
 * DOBs/nationalities/positions 64, passport/national ids 32, addresses 32),
 * described as guards against a pathological <Designation> allocating without
 * limit — but every array is built from a block of a document that is already
 * in memory, so the document bounds it, and a bound that bit would have
 * dropped names and identifiers silently (no notice existed for it). Live
 * maxima on 2026-10-02: 144 names, 19 addresses, 14 passports. */

/* Join Name1..Name6 in order, as the FCDO splits a single name across them. */
static void uk_join_name(const char *b, const char *e, char *out, size_t n) {
  size_t j = 0;
  out[0] = 0;
  for (int i = 1; i <= 6; i++) {
    char tag[8];
    snprintf(tag, sizeof tag, "Name%d", i);
    char *v = sanc_xml_text(b, e, tag);
    if (!v) continue;
    size_t need = strlen(v) + (j ? 1 : 0);
    if (j + need < n - 1) {
      if (j) out[j++] = ' ';
      strcpy(out + j, v);
      j += strlen(v);
    }
    free(v);
  }
}

/* Every <tag> text under a container, as a JSON array. */
static cJSON *uk_texts(const char *b, const char *e, const char *container,
                       const char *tag) {
  cJSON *out = cJSON_CreateArray();
  const char *cur = b;
  sanc_el c;
  if (!sanc_xml_next(&cur, e, container, &c)) return out;
  const char *ic = c.body;
  sanc_el t;
  while (sanc_xml_next(&ic, c.body_end, tag, &t)) {
    char *v = sanc_decode(t.body, (size_t)(t.body_end - t.body));
    if (v && v[0]) cJSON_AddItemToArray(out, cJSON_CreateString(v));
    free(v);
  }
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  size_t len = 0;
  char *xml = sanc_http_get(ctx, UK_SANC_URL, "Accept: application/xml, text/xml, */*",
                            NULL, NULL, 180000, "uk-sanctions-list", &len, NULL);
  if (!xml) return -1;
  const char *end = xml + len;

  char geniso[16] = {0};
  {
    char *g = sanc_xml_text(xml, end, "DateGenerated");
    if (g) { sanc_ddmmyyyy(g, geniso, sizeof geniso); free(g); }
  }

  /* Unbounded unless the operator sets JO_SANC_MAX_ROWS. The shared default
   * of 5,000 BIT on this list: 5,000 of 6,370 designations emitted on
   * 2026-10-02, 1,370 dropped every run (disclosed, but dropped), for a parse
   * that takes six seconds. An explicit setting still applies and is still
   * disclosed below. */
  int max_rows = sanc_env_int("JO_SANC_MAX_ROWS", INT_MAX);
  int n = 0;
  const char *cur = xml;
  sanc_el d;
  while (n < max_rows && sanc_xml_next(&cur, end, "Designation", &d)) {
    const char *b = d.body, *e = d.body_end;

    /* --- names: first Primary Name is the title, the rest are aliases --- */
    char primary[512];
    primary[0] = 0;
    cJSON *aliases = cJSON_CreateArray();
    {
      const char *nc = b;
      sanc_el names;
      if (sanc_xml_next(&nc, e, "Names", &names)) {
        const char *c2 = names.body;
        sanc_el nm;
        while (sanc_xml_next(&c2, names.body_end, "Name", &nm)) {
          char joined[512];
          uk_join_name(nm.body, nm.body_end, joined, sizeof joined);
          if (!joined[0]) continue;
          char *ntype = sanc_xml_text(nm.body, nm.body_end, "NameType");
          int is_primary = ntype && strstr(ntype, "Primary") != NULL;
          if (is_primary && !primary[0]) {
            snprintf(primary, sizeof primary, "%s", joined);
          } else {
            char *strength = sanc_xml_text(nm.body, nm.body_end, "AliasStrength");
            char line[600];
            snprintf(line, sizeof line, "%s%s%s%s", joined,
                     strength ? " (" : "", strength ? strength : "",
                     strength ? ")" : "");
            cJSON_AddItemToArray(aliases, cJSON_CreateString(line));
            free(strength);
          }
          free(ntype);
        }
      }
    }
    if (!primary[0]) {
      /* No Primary Name marked — fall back to the first name we did parse.
       * This picks a TITLE, it does not choose which names survive: every
       * parsed name is already in `aliases` and `aliases` is stored whole. */
      const cJSON *a0 = cJSON_GetArrayItem(aliases, 0);  /* exhaustive-ok: title fallback; picks a TITLE only, does not choose which names survive — every parsed name is already in `aliases`, which is stored whole below */
      if (cJSON_IsString(a0)) snprintf(primary, sizeof primary, "%s", a0->valuestring);
    }
    if (!primary[0]) { cJSON_Delete(aliases); continue; }   /* no name -> no row */

    char *uid = sanc_xml_text(b, e, "UniqueID");
    char *ofsi = sanc_xml_text(b, e, "OFSIGroupID");
    char *unref = sanc_xml_text(b, e, "UNReferenceNumber");
    char *regime = sanc_xml_text(b, e, "RegimeName");
    char *kind = sanc_xml_text(b, e, "IndividualEntityShip");
    char *dsource = sanc_xml_text(b, e, "DesignationSource");
    char *imposed = sanc_xml_text(b, e, "SanctionsImposed");
    char *reasons = sanc_xml_text(b, e, "UKStatementofReasons");
    char *other = sanc_xml_text(b, e, "OtherInformation");
    char *designated = sanc_xml_text(b, e, "DateDesignated");
    char *updated = sanc_xml_text(b, e, "LastUpdated");

    char diso[16] = {0}, uiso[16] = {0};
    if (designated) sanc_ddmmyyyy(designated, diso, sizeof diso);
    if (updated) sanc_ddmmyyyy(updated, uiso, sizeof uiso);

    /* which measure flags the FCDO set to true */
    cJSON *measures = cJSON_CreateArray();
    {
      const char *ic = b;
      sanc_el ind;
      if (sanc_xml_next(&ic, e, "SanctionsImposedIndicators", &ind)) {
        /* Walk the child elements generically — the FCDO adds new indicator
         * flags over time and a hardcoded list would silently drop them. Only
         * the ones the file says are "true" are recorded. */
        const char *p = ind.body;
        while (p < ind.body_end) {
          const char *lt = (const char *)memchr(p, '<', (size_t)(ind.body_end - p));
          if (!lt) break;
          const char *gt = (const char *)memchr(lt, '>', (size_t)(ind.body_end - lt));
          if (!gt) break;
          if (lt + 1 < gt && lt[1] == '/') { p = gt + 1; continue; }
          char tagname[64];
          size_t tl = 0;
          const char *q = lt + 1;
          while (q < gt && *q != ' ' && *q != '/' && tl + 1 < sizeof tagname)
            tagname[tl++] = *q++;
          tagname[tl] = 0;
          if (gt[-1] == '/' || !tl) { p = gt + 1; continue; }   /* empty element */
          const char *val = gt + 1;
          const char *close = (const char *)memchr(val, '<',
                                                   (size_t)(ind.body_end - val));
          if (!close) break;
          if ((size_t)(close - val) == 4 && strncmp(val, "true", 4) == 0)
            cJSON_AddItemToArray(measures, cJSON_CreateString(tagname));
          p = close;      /* the close tag is skipped on the next iteration */
        }
      }
    }

    cJSON *dobs = uk_texts(b, e, "DOBs", "DOB");
    cJSON *nats = uk_texts(b, e, "Nationalities", "Nationality");
    cJSON *positions = uk_texts(b, e, "Positions", "Position");
    cJSON *passports = uk_texts(b, e, "PassportDetails", "PassportNumber");
    cJSON *natids = uk_texts(b, e, "NationalIdentifierDetails",
                             "NationalIdentifierNumber");

    /* addresses as published (R2: text only) */
    cJSON *addresses = cJSON_CreateArray();
    {
      const char *ac = b;
      sanc_el al;
      if (sanc_xml_next(&ac, e, "Addresses", &al)) {
        const char *c2 = al.body;
        sanc_el ad;
        while (sanc_xml_next(&c2, al.body_end, "Address", &ad)) {
          char line[600];
          size_t j = 0;
          line[0] = 0;
          for (int i = 1; i <= 6; i++) {
            char tag[24];
            snprintf(tag, sizeof tag, "AddressLine%d", i);
            char *v = sanc_xml_text(ad.body, ad.body_end, tag);
            if (!v) continue;
            size_t need = strlen(v) + (j ? 2 : 0);
            if (j + need < sizeof line - 1) {
              if (j) { strcpy(line + j, ", "); j += 2; }
              strcpy(line + j, v);
              j += strlen(v);
            }
            free(v);
          }
          char *ac2 = sanc_xml_text(ad.body, ad.body_end, "AddressCountry");
          if (ac2 && j + strlen(ac2) + 2 < sizeof line) {
            if (j) { strcpy(line + j, ", "); j += 2; }
            strcpy(line + j, ac2);
          }
          free(ac2);
          if (line[0]) cJSON_AddItemToArray(addresses, cJSON_CreateString(line));
        }
      }
    }

    char mlist[320];
    sanc_join(measures, ", ", 20, mlist, sizeof mlist);
    char summary[512];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             kind ? kind : "Designation",
             regime ? " · " : "", regime ? regime : "",
             imposed ? " · " : "", imposed ? imposed : "");

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "name", primary);
    sanc_add(body, "unique_id", uid);
    sanc_add(body, "ofsi_group_id", ofsi);
    sanc_add(body, "un_reference", unref);
    sanc_add(body, "regime", regime);
    sanc_add(body, "designation_type", kind);
    sanc_add(body, "designation_source", dsource);
    sanc_add(body, "sanctions_imposed", imposed);
    if (mlist[0]) sanc_add(body, "measures", mlist);
    sanc_add(body, "date_designated", diso[0] ? diso : designated);
    sanc_add(body, "last_updated", uiso[0] ? uiso : updated);
    cJSON_AddItemToObject(body, "aliases", cJSON_Duplicate(aliases, 1));
    cJSON_AddItemToObject(body, "addresses", cJSON_Duplicate(addresses, 1));
    cJSON_AddItemToObject(body, "dates_of_birth", cJSON_Duplicate(dobs, 1));
    cJSON_AddItemToObject(body, "nationalities", cJSON_Duplicate(nats, 1));
    cJSON_AddItemToObject(body, "positions", cJSON_Duplicate(positions, 1));
    cJSON_AddItemToObject(body, "passport_numbers", cJSON_Duplicate(passports, 1));
    cJSON_AddItemToObject(body, "national_identifiers", cJSON_Duplicate(natids, 1));
    sanc_add(body, "uk_statement_of_reasons", reasons);
    sanc_add(body, "other_information", other);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "list", "UK Sanctions List (FCDO)");
    sanc_add(props, "unique_id", uid);
    sanc_add(props, "ofsi_group_id", ofsi);
    sanc_add(props, "un_reference", unref);
    sanc_add(props, "regime", regime);
    sanc_add(props, "designation_type", kind);
    sanc_add(props, "designation_source", dsource);
    sanc_add(props, "sanctions_imposed", imposed);
    sanc_add(props, "date_designated", diso[0] ? diso : NULL);
    sanc_add(props, "last_updated", uiso[0] ? uiso : NULL);
    sanc_add(props, "list_generated", geniso[0] ? geniso : NULL);
    cJSON_AddItemToObject(props, "measures", measures);       /* transferred */
    cJSON_AddItemToObject(props, "aliases", aliases);         /* transferred */
    cJSON_AddItemToObject(props, "addresses", addresses);     /* transferred */
    cJSON_AddItemToObject(props, "dates_of_birth", dobs);     /* transferred */
    cJSON_AddItemToObject(props, "nationalities", nats);      /* transferred */
    cJSON_AddItemToObject(props, "positions", positions);     /* transferred */
    cJSON_AddItemToObject(props, "passport_numbers", passports);
    cJSON_AddItemToObject(props, "national_identifiers", natids);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = uid ? uid : primary;
    it.title = primary;
    it.summary = summary;
    it.body = bj;
    it.lang = "en";
    it.published_at = diso[0] ? diso : NULL;
    it.record_type = "uk-sanctions-designation";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sanctions\",\"uk\",\"fcdo\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(bj); free(pj);
    free(uid); free(ofsi); free(unref); free(regime); free(kind); free(dsource);
    free(imposed); free(reasons); free(other); free(designated); free(updated);
  }
  /* JO_SANC_MAX_ROWS (shared default 5,000; unbounded here unless set) is a
   * bound across the sanctions collectors, and on this list it BIT: the FCDO consolidated list is past
   * 5,000 designations, so a run emitted exactly 5,000 and stopped — the round
   * number in the log being the only hint that anything was missing, which is
   * the "a log nobody reads is not a disclosure" case house rule 2 names.
   *
   * The bound stays (it is the shared guard against a half-parsed 40 MB XML),
   * but the shortfall is now counted from the document itself and reported as
   * data. `available` is the number of <Designation> elements the FCDO
   * actually published — scanned, not estimated. */
  long available = n;
  if (n >= max_rows) {
    sanc_el extra;
    while (sanc_xml_next(&cur, end, "Designation", &extra)) available++;
    if (available > n)
      jo_trunc_notice(sink, "uk-sanctions-list", UK_SANC_URL, n, available,
                      "the shared JO_SANC_MAX_ROWS row bound stopped the parse "
                      "before the end of the FCDO designation list",
                      "raise $JO_SANC_MAX_ROWS above the list's designation "
                      "count");
  }

  free(xml);
  fprintf(stderr, "[uk-sanctions-list] emitted %d of %ld designation(s) "
                  "(generated %s)%s\n", n, available,
          geniso[0] ? geniso : "?",
          (available > n) ? " (TRUNCATED — notice emitted)" : "");
  return 0;
}

static const source_def sanc_uk_sanctions_list_def = {
  .id = "uk-sanctions-list", .collector = "sanctions",
  .name = "UK Sanctions List (FCDO)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = UK_SANC_URL,
  .description = "The statutory UK Sanctions List published by the FCDO — the legal designations under the Sanctions and Anti-Money Laundering Act, including travel bans and arms-embargo targets the OFSI asset-freeze list does not carry.",
  .license = "Open Government Licence v3.0.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_uk_sanctions_list_def)
