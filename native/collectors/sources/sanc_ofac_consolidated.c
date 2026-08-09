/* OFAC Consolidated Sanctions List (the NON-SDN lists).
 *
 * Endpoint : https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/
 *            CONSOLIDATED.XML   (302-redirects to a presigned S3 object; libcurl follows it)
 * Format   : namespaced XML, record delimiter <sdnEntry>, header <publshInformation> with
 *            Publish_Date and Record_Count.
 * Verified : HTTP 200, ~1 MB, Record_Count 481, Publish_Date 07/27/2026; first entry
 *            uid 9640 "ABU TEIR, Mohammed", Individual, programme NS-PLC, a.k.a. ABU TAIR.
 * Keyless  : yes.
 * Emits    : per designation — display name, entity type (Individual/Entity/Vessel), every
 *            sanctions programme, all a.k.a. names with their a.k.a. type, addresses as
 *            published (city / state / country strings), dates and places of birth,
 *            nationalities, identity documents, remarks, and the OFAC uid.
 * Geometry : NONE (R2). Addresses are emitted as the text OFAC published; nothing is geocoded
 *            and no designated party is pinned to a country.
 * Licence  : US Government work, public domain. OFAC asks that any match be re-checked against
 *            the authoritative publication before it is acted on — carried in properties.caveat.
 *
 * Notes    : this is a DIFFERENT list from the SDN file already wired up (SDN.CSV in
 *            sanctions_world.c). It holds NS-PLC, FSE-IR/SY, SSI (sectoral), CMIC, PLC and
 *            NS-MBS. A name absent from the SDN list can still be here, so screening on SDN
 *            alone under-reports. A CSV twin exists at .../CONS_PRIM.CSV.
 *
 * NAME MATCHING: this collector emits the list, it does not match against it. OFAC stores
 * people surname-first ("PUTIN, Vladimir Vladimirovich") and naive substring screening
 * matches "PUTIN" inside "COMPUTING" — matching is left to the platform, deliberately.
 */
#include "sanc_common.inc"

#define OFAC_CONS_URL \
  "https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/CONSOLIDATED.XML"

/* Collect the text of every <field> inside every <item> of one <list> block. */
static cJSON *ofac_list_texts(const char *b, const char *e, const char *list_tag,
                              const char *item_tag, const char *field, int max) {
  cJSON *out = cJSON_CreateArray();
  const char *cur = b;
  sanc_el lb;
  if (!sanc_xml_next(&cur, e, list_tag, &lb)) return out;
  const char *ic = lb.body;
  sanc_el ib;
  while (cJSON_GetArraySize(out) < max &&
         sanc_xml_next(&ic, lb.body_end, item_tag, &ib)) {
    char *v = sanc_xml_text(ib.body, ib.body_end, field);
    if (v) {
      cJSON_AddItemToArray(out, cJSON_CreateString(v));
      free(v);
    }
  }
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  size_t len = 0;
  char *xml = sanc_http_get(ctx, OFAC_CONS_URL, "Accept: application/xml, text/xml, */*",
                            NULL, NULL, 120000, "ofac-consolidated", &len, NULL);
  if (!xml) return -1;
  const char *end = xml + len;

  /* publication header — the same date for every row in this publication */
  char pubiso[16] = {0};
  {
    char *pd = sanc_xml_text(xml, end, "Publish_Date");
    if (pd) { sanc_mmddyyyy(pd, pubiso, sizeof pubiso); free(pd); }
  }

  int max_rows = sanc_env_int("JO_SANC_MAX_ROWS", 5000);
  int n = 0;
  const char *cur = xml;
  sanc_el entry;
  while (n < max_rows && sanc_xml_next(&cur, end, "sdnEntry", &entry)) {
    const char *b = entry.body, *e = entry.body_end;

    /* The entry's OWN scalar fields all precede its first child list; scanning
     * the whole block would pick up an <aka>'s firstName for an Entity record
     * that has none of its own. */
    const char *own_end = e;
    static const char *const LISTS[] = { "<programList", "<akaList", "<addressList",
                                         "<dateOfBirthList", "<placeOfBirthList",
                                         "<nationalityList", "<citizenshipList",
                                         "<idList", "<vesselInfo", NULL };
    for (int i = 0; LISTS[i]; i++) {
      const char *h = sanc_xml_open(b, e, LISTS[i] + 1);
      if (h && h < own_end) own_end = h;
    }

    char *uid = sanc_xml_text(b, own_end, "uid");
    char *first = sanc_xml_text(b, own_end, "firstName");
    char *last = sanc_xml_text(b, own_end, "lastName");
    char *type = sanc_xml_text(b, own_end, "sdnType");
    char *title_f = sanc_xml_text(b, own_end, "title");
    char *remarks = sanc_xml_text(b, own_end, "remarks");

    if (!uid || (!last && !first)) {
      free(uid); free(first); free(last); free(type); free(title_f); free(remarks);
      continue;                                    /* no fetched name -> no row */
    }

    char display[512];
    if (last && first) snprintf(display, sizeof display, "%s, %s", last, first);
    else snprintf(display, sizeof display, "%s", last ? last : first);

    /* <program> holds its value directly, so it is read inline rather than via
     * ofac_list_texts() (which extracts a CHILD element of each item). */
    cJSON *programs = cJSON_CreateArray();
    {
      const char *pc = b;
      sanc_el pl;
      if (sanc_xml_next(&pc, e, "programList", &pl)) {
        const char *c2 = pl.body;
        sanc_el pe;
        while (cJSON_GetArraySize(programs) < 24 &&
               sanc_xml_next(&c2, pl.body_end, "program", &pe)) {
          char *v = sanc_decode(pe.body, (size_t)(pe.body_end - pe.body));
          if (v && v[0]) cJSON_AddItemToArray(programs, cJSON_CreateString(v));
          free(v);
        }
      }
    }

    /* a.k.a. names, each with the a.k.a. type OFAC assigned */
    cJSON *akas = cJSON_CreateArray();
    {
      const char *ac = b;
      sanc_el al;
      if (sanc_xml_next(&ac, e, "akaList", &al)) {
        const char *c2 = al.body;
        sanc_el ae;
        while (cJSON_GetArraySize(akas) < 24 &&
               sanc_xml_next(&c2, al.body_end, "aka", &ae)) {
          char *af = sanc_xml_text(ae.body, ae.body_end, "firstName");
          char *alast = sanc_xml_text(ae.body, ae.body_end, "lastName");
          char *at = sanc_xml_text(ae.body, ae.body_end, "type");
          if (af || alast) {
            char line[512];
            snprintf(line, sizeof line, "%s%s%s%s%s%s",
                     alast ? alast : "", (alast && af) ? ", " : "", af ? af : "",
                     at ? " (" : "", at ? at : "", at ? ")" : "");
            cJSON_AddItemToArray(akas, cJSON_CreateString(line));
          }
          free(af); free(alast); free(at);
        }
      }
    }

    /* addresses exactly as published — text only, never geocoded (R2) */
    cJSON *addresses = cJSON_CreateArray();
    {
      const char *ac = b;
      sanc_el al;
      if (sanc_xml_next(&ac, e, "addressList", &al)) {
        const char *c2 = al.body;
        sanc_el ae;
        while (cJSON_GetArraySize(addresses) < 12 &&
               sanc_xml_next(&c2, al.body_end, "address", &ae)) {
          const char *fields[5] = { "address1", "city", "stateOrProvince",
                                    "postalCode", "country" };
          char line[512];
          size_t j = 0;
          line[0] = 0;
          for (int i = 0; i < 5; i++) {
            char *v = sanc_xml_text(ae.body, ae.body_end, fields[i]);
            if (!v) continue;
            size_t need = strlen(v) + (j ? 2 : 0);
            if (j + need < sizeof line - 1) {
              if (j) { strcpy(line + j, ", "); j += 2; }
              strcpy(line + j, v);
              j += strlen(v);
            }
            free(v);
          }
          if (line[0]) cJSON_AddItemToArray(addresses, cJSON_CreateString(line));
        }
      }
    }

    cJSON *dobs = ofac_list_texts(b, e, "dateOfBirthList", "dateOfBirthItem",
                                  "dateOfBirth", 8);
    cJSON *pobs = ofac_list_texts(b, e, "placeOfBirthList", "placeOfBirthItem",
                                  "placeOfBirth", 8);
    cJSON *nats = ofac_list_texts(b, e, "nationalityList", "nationality", "country", 8);
    cJSON *cits = ofac_list_texts(b, e, "citizenshipList", "citizenship", "country", 8);
    cJSON *idnums = ofac_list_texts(b, e, "idList", "id", "idNumber", 12);
    cJSON *idtypes = ofac_list_texts(b, e, "idList", "id", "idType", 12);

    char proglist[256];
    sanc_join(programs, ", ", 24, proglist, sizeof proglist);
    char summary[512];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             type ? type : "Designation",
             proglist[0] ? " · " : "", proglist,
             cJSON_GetArraySize(akas) ? " · aka " : "",
             cJSON_GetArraySize(akas)
               ? cJSON_GetStringValue(cJSON_GetArrayItem(akas, 0)) : "");

    char link[160];
    snprintf(link, sizeof link,
             "https://sanctionssearch.ofac.treas.gov/Details.aspx?id=%s", uid);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "name", display);
    sanc_add(body, "sdn_type", type);
    sanc_add(body, "uid", uid);
    sanc_add(body, "title", title_f);
    cJSON_AddItemToObject(body, "programs", cJSON_Duplicate(programs, 1));
    cJSON_AddItemToObject(body, "aka", cJSON_Duplicate(akas, 1));
    cJSON_AddItemToObject(body, "addresses", cJSON_Duplicate(addresses, 1));
    cJSON_AddItemToObject(body, "dates_of_birth", cJSON_Duplicate(dobs, 1));
    cJSON_AddItemToObject(body, "places_of_birth", cJSON_Duplicate(pobs, 1));
    cJSON_AddItemToObject(body, "nationalities", cJSON_Duplicate(nats, 1));
    cJSON_AddItemToObject(body, "citizenships", cJSON_Duplicate(cits, 1));
    cJSON_AddItemToObject(body, "id_numbers", cJSON_Duplicate(idnums, 1));
    cJSON_AddItemToObject(body, "id_types", cJSON_Duplicate(idtypes, 1));
    sanc_add(body, "remarks", remarks);
    sanc_add(body, "list", "OFAC Consolidated (non-SDN)");
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "list", "OFAC Consolidated (non-SDN)");
    sanc_add(props, "uid", uid);
    sanc_add(props, "sdn_type", type);
    if (proglist[0]) sanc_add(props, "programs", proglist);
    cJSON_AddItemToObject(props, "aka", akas);              /* transferred */
    cJSON_AddItemToObject(props, "addresses", addresses);   /* transferred */
    cJSON_AddItemToObject(props, "dates_of_birth", dobs);   /* transferred */
    cJSON_AddItemToObject(props, "places_of_birth", pobs);  /* transferred */
    cJSON_AddItemToObject(props, "nationalities", nats);    /* transferred */
    cJSON_AddItemToObject(props, "citizenships", cits);     /* transferred */
    cJSON_AddItemToObject(props, "id_numbers", idnums);     /* transferred */
    cJSON_AddItemToObject(props, "id_types", idtypes);      /* transferred */
    sanc_add(props, "publish_date", pubiso[0] ? pubiso : NULL);
    sanc_add(props, "caveat",
             "OFAC asks that any apparent match be re-checked against the authoritative "
             "publication before it is acted on.");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);
    cJSON_Delete(programs);

    intel_item it = {0};
    it.remote_key = uid;
    it.title = display;
    it.summary = summary;
    it.body = bj;
    it.link = link;
    it.lang = "en";
    it.published_at = pubiso[0] ? pubiso : NULL;
    it.record_type = "ofac-consolidated-entry";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sanctions\",\"ofac\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(bj); free(pj);
    free(uid); free(first); free(last); free(type); free(title_f); free(remarks);
  }
  free(xml);
  fprintf(stderr, "[ofac-consolidated] emitted %d (publish %s)\n", n,
          pubiso[0] ? pubiso : "?");
  return 0;
}

static const source_def sanc_ofac_consolidated_def = {
  .id = "ofac-consolidated-nonsdn", .collector = "sanctions",
  .name = "OFAC Consolidated Sanctions List (non-SDN)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = OFAC_CONS_URL,
  .description = "The OFAC lists that are NOT the SDN list — NS-PLC, FSE-IR/SY, SSI (sectoral), CMIC, PLC, NS-MBS. A name absent from SDN can still be here, so screening on SDN alone under-reports.",
  .license = "US Government work, public domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_ofac_consolidated_def)
