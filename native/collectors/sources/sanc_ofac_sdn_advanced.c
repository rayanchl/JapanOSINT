/* OFAC SDN Advanced XML — full structured designation detail (incl. crypto addresses).
 *
 * Endpoint : https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/
 *            SDN_ADVANCED.XML   (302 -> presigned S3 object; libcurl follows the redirect)
 * Format   : namespaced XML. <ReferenceValueSets> at the head define the code tables;
 *            <DistinctParties> then holds one <DistinctParty FixedRef="…"> per designation.
 * Verified : HTTP 200, 125,744,456 bytes, DateOfIssue 2026-07-30, 19,181 <DistinctParty>
 *            blocks, 75 FeatureType codes including "Digital Currency Address - XBT" (344),
 *            "- ETH" (345), "- USDT" (887), "- TRX" (992) …
 *
 *            THIS IS THE FILE A PREVIOUS AUDIT RECORDED AS 404. The dead path was the old
 *            lowercase www.treasury.gov/…/sdn_advanced.xml; the uppercase SDN_ADVANCED.XML on
 *            sanctionslistservice.ofac.treas.gov resolves today. Do not "restore" the old URL.
 *
 * Keyless  : yes.
 * Emits    : per designated party — the primary name (assembled from its <NamePartValue>s),
 *            the party type resolved through the PartySubType/PartyType code tables, every
 *            typed <Feature> with its human-readable FeatureType name and value, and the
 *            digital-currency (crypto) addresses called out separately.
 * Geometry : NONE (R2). The file carries a <Locations> section; it is deliberately NOT used to
 *            place a sanctioned party on a map.
 * Licence  : US Government work, public domain. OFAC asks that the list be re-checked against
 *            the authoritative publication before action — carried in properties.caveat.
 *
 * SIZE STRATEGY (why this collector is not a plain GET):
 *   the export is ~125 MB and its sections are laid out ReferenceValueSets (~94 KB) …
 *   Locations … IDRegDocuments … DistinctParties (~26 MB-108 MB) … SanctionsEntries (tail).
 *   Parties are ordered by FixedRef ASCENDING, so the NEWEST designations are at the END of
 *   the DistinctParties section. Reading a prefix would therefore return the oldest entries
 *   forever and never surface a new designation. Two bounded ranged requests are made instead:
 *     1. bytes=0-262143       -> the code tables (FeatureType, PartySubType, PartyType);
 *     2. bytes=-<N>           -> the LAST N bytes (JO_SANC_SDN_TAIL_MB, default 48 MB), which
 *                                covers the trailing sections plus the most recent parties.
 *   Both requests send Accept-Encoding: identity, because a byte range over a gzip stream is a
 *   range over the COMPRESSED bytes. HTTP 206 on both was verified against the live endpoint.
 *   A tail window that contains no <DistinctParty> at all means the layout moved and the
 *   window needs enlarging — that is a real failure and returns -1, loudly.
 *
 * NAME MATCHING: the list is emitted, not matched. OFAC stores people surname-first and naive
 * substring screening matches "PUTIN" inside "COMPUTING"; matching is left to the platform.
 */
#include "sanc_common.inc"

#define SDN_ADV_URL \
  "https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/SDN_ADVANCED.XML"

#define SDN_MAX_CODES 256

typedef struct { char id[12]; char name[96]; } sdn_code;

/* Read <Tag ID="n" …>Text</Tag> pairs into a small code table. */
static int sdn_load_codes(const char *b, const char *e, const char *tag,
                          sdn_code *out, int max) {
  int n = 0;
  const char *cur = b;
  sanc_el el;
  while (n < max && sanc_xml_next(&cur, e, tag, &el)) {
    char *id = sanc_xml_attr(&el, "ID");
    if (!id) continue;
    char *txt = sanc_decode(el.body, (size_t)(el.body_end - el.body));
    if (txt && txt[0]) {
      snprintf(out[n].id, sizeof out[n].id, "%s", id);
      snprintf(out[n].name, sizeof out[n].name, "%s", txt);
      n++;
    }
    free(txt);
    free(id);
  }
  return n;
}

static const char *sdn_code_lookup(const sdn_code *t, int n, const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < n; i++) if (strcmp(t[i].id, id) == 0) return t[i].name;
  return NULL;
}

/* Attribute value of a named attribute on the open tag of the element that
 * starts at `open` (used for PartySubTypeID on <Profile>). */
static char *sdn_open_attr(const char *open, const char *end, const char *attr) {
  const char *gt = (const char *)memchr(open, '>', (size_t)(end - open));
  if (!gt) return NULL;
  sanc_el el;
  el.tag_start = open;
  el.tag_end = gt;
  el.body = el.body_end = gt + 1;
  return sanc_xml_attr(&el, attr);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* ---- 1. the code tables, from the head of the file --------------------- */
  size_t hlen = 0;
  long hstatus = 0;
  char *head = sanc_http_get(ctx, SDN_ADV_URL, "Accept: application/xml, text/xml, */*",
                             "Range: bytes=0-262143", NULL, 60000,
                             "ofac-sdn-advanced", &hlen, &hstatus);
  if (!head) return -1;

  sdn_code *ftypes = (sdn_code *)calloc(SDN_MAX_CODES, sizeof(sdn_code));
  sdn_code *subtypes = (sdn_code *)calloc(SDN_MAX_CODES, sizeof(sdn_code));
  sdn_code *ptypes = (sdn_code *)calloc(SDN_MAX_CODES, sizeof(sdn_code));
  if (!ftypes || !subtypes || !ptypes) {
    free(head); free(ftypes); free(subtypes); free(ptypes);
    return -1;
  }
  const char *hend = head + hlen;
  int nft = sdn_load_codes(head, hend, "FeatureType", ftypes, SDN_MAX_CODES);
  int npt = sdn_load_codes(head, hend, "PartyType", ptypes, SDN_MAX_CODES);

  /* PartySubType's own text is often "Unknown"; the meaningful label comes from
   * resolving its PartyTypeID through the PartyType table. */
  int nst = 0;
  {
    const char *cur = head;
    sanc_el el;
    while (nst < SDN_MAX_CODES && sanc_xml_next(&cur, hend, "PartySubType", &el)) {
      char *id = sanc_xml_attr(&el, "ID");
      char *ptid = sanc_xml_attr(&el, "PartyTypeID");
      char *own = sanc_decode(el.body, (size_t)(el.body_end - el.body));
      if (id) {
        const char *label = sdn_code_lookup(ptypes, npt, ptid);
        if (!label || !label[0]) label = (own && own[0]) ? own : NULL;
        if (label) {
          snprintf(subtypes[nst].id, sizeof subtypes[nst].id, "%s", id);
          snprintf(subtypes[nst].name, sizeof subtypes[nst].name, "%s", label);
          nst++;
        }
      }
      free(id); free(ptid); free(own);
    }
  }

  char issue[16] = {0};
  {
    char *y = sanc_xml_text(head, hend, "Year");
    char *m = sanc_xml_text(head, hend, "Month");
    char *d = sanc_xml_text(head, hend, "Day");
    if (y && m && d) snprintf(issue, sizeof issue, "%04d-%02d-%02d",
                              atoi(y), atoi(m), atoi(d));
    free(y); free(m); free(d);
  }
  free(head);

  /* ---- 2. the tail window, where the newest DistinctParty blocks live ---- */
  int tail_mb = sanc_env_int("JO_SANC_SDN_TAIL_MB", 48);
  char range[64];
  snprintf(range, sizeof range, "Range: bytes=-%lld", (long long)tail_mb * 1048576LL);

  size_t tlen = 0;
  long tstatus = 0;
  char *tail = sanc_http_get(ctx, SDN_ADV_URL, "Accept: application/xml, text/xml, */*",
                             range, NULL, 300000, "ofac-sdn-advanced", &tlen, &tstatus);
  if (!tail) { free(ftypes); free(subtypes); free(ptypes); return -1; }
  const char *tend = tail + tlen;

  int max_rows = sanc_env_int("JO_SANC_MAX_ROWS", 5000);
  int n = 0, parties = 0;
  const char *cur = tail;
  sanc_el party;
  while (n < max_rows && sanc_xml_next(&cur, tend, "DistinctParty", &party)) {
    parties++;
    const char *b = party.body, *e = party.body_end;
    char *fixedref = sanc_xml_attr(&party, "FixedRef");
    if (!fixedref) continue;

    /* party type via <Profile PartySubTypeID="…"> */
    const char *ptype = NULL;
    char *subid = NULL;
    {
      const char *po = sanc_xml_open(b, e, "Profile");
      if (po) subid = sdn_open_attr(po, e, "PartySubTypeID");
      ptype = sdn_code_lookup(subtypes, nst, subid);
    }

    /* primary name: the Alias marked Primary="true"; its DocumentedName's
     * NamePartValue elements concatenated in file order. */
    char name[512];
    name[0] = 0;
    {
      const char *ac = b;
      sanc_el alias;
      const char *fallback_b = NULL, *fallback_e = NULL;
      while (sanc_xml_next(&ac, e, "Alias", &alias)) {
        char *prim = sanc_xml_attr(&alias, "Primary");
        int is_primary = prim && strcmp(prim, "true") == 0;
        free(prim);
        if (!fallback_b) { fallback_b = alias.body; fallback_e = alias.body_end; }
        if (!is_primary) continue;
        fallback_b = alias.body;
        fallback_e = alias.body_end;
        break;
      }
      if (fallback_b) {
        const char *nc = fallback_b;
        sanc_el np;
        size_t j = 0;
        while (sanc_xml_next(&nc, fallback_e, "NamePartValue", &np)) {
          char *v = sanc_decode(np.body, (size_t)(np.body_end - np.body));
          if (v && v[0]) {
            size_t need = strlen(v) + (j ? 1 : 0);
            if (j + need < sizeof name - 1) {
              if (j) name[j++] = ' ';
              strcpy(name + j, v);
              j += strlen(v);
            }
          }
          free(v);
        }
      }
    }
    if (!name[0]) { free(fixedref); free(subid); continue; }  /* no name -> no row */

    /* typed features: FeatureTypeID -> human name, value from <VersionDetail> */
    cJSON *features = cJSON_CreateArray();
    cJSON *crypto = cJSON_CreateArray();
    {
      const char *fc = b;
      sanc_el feat;
      while (cJSON_GetArraySize(features) < 40 &&
             sanc_xml_next(&fc, e, "Feature", &feat)) {
        char *ftid = sanc_xml_attr(&feat, "FeatureTypeID");
        const char *fname = sdn_code_lookup(ftypes, nft, ftid);
        char *val = sanc_xml_text(feat.body, feat.body_end, "VersionDetail");
        if (val && val[0]) {
          char line[512];
          snprintf(line, sizeof line, "%s: %s", fname ? fname : "Feature", val);
          cJSON_AddItemToArray(features, cJSON_CreateString(line));
          if (fname && strstr(fname, "Digital Currency Address") &&
              cJSON_GetArraySize(crypto) < 40) {
            cJSON *c = cJSON_CreateObject();
            const char *dash = strstr(fname, " - ");
            sanc_add(c, "asset", dash ? dash + 3 : fname);
            sanc_add(c, "address", val);
            cJSON_AddItemToArray(crypto, c);
          }
        }
        free(val);
        free(ftid);
      }
    }

    char flist[400];
    sanc_join(features, " · ", 6, flist, sizeof flist);
    char summary[600];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             ptype ? ptype : "SDN party",
             cJSON_GetArraySize(crypto)
               ? " · digital currency addresses listed" : "",
             flist[0] ? " · " : "", flist, "");

    char link[160];
    snprintf(link, sizeof link,
             "https://sanctionssearch.ofac.treas.gov/Details.aspx?id=%s", fixedref);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "name", name);
    sanc_add(body, "party_type", ptype);
    sanc_add(body, "fixed_ref", fixedref);
    sanc_add(body, "list", "OFAC SDN (advanced export)");
    sanc_add(body, "date_of_issue", issue[0] ? issue : NULL);
    cJSON_AddItemToObject(body, "features", cJSON_Duplicate(features, 1));
    cJSON_AddItemToObject(body, "digital_currency_addresses", cJSON_Duplicate(crypto, 1));
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "list", "OFAC SDN (advanced export)");
    sanc_add(props, "fixed_ref", fixedref);
    sanc_add(props, "party_type", ptype);
    sanc_add(props, "date_of_issue", issue[0] ? issue : NULL);
    cJSON_AddItemToObject(props, "features", features);                  /* moved */
    cJSON_AddItemToObject(props, "digital_currency_addresses", crypto);  /* moved */
    sanc_add(props, "coverage",
             "Bounded tail window of the 125 MB export — the most recent designations "
             "in the DistinctParties section, not the whole file.");
    sanc_add(props, "caveat",
             "OFAC asks that any apparent match be re-checked against the authoritative "
             "publication before it is acted on.");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = fixedref;
    it.title = name;
    it.summary = summary;
    it.body = bj;
    it.link = link;
    it.lang = "en";
    it.published_at = issue[0] ? issue : NULL;
    it.record_type = "ofac-sdn-advanced";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sanctions\",\"ofac\",\"sdn\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(bj); free(pj); free(fixedref); free(subid);
  }
  free(tail);
  free(ftypes); free(subtypes); free(ptypes);

  fprintf(stderr,
          "[ofac-sdn-advanced] codes: %d feature/%d subtype; tail %d MB (http %ld); "
          "%d parties seen, emitted %d\n", nft, nst, tail_mb, tstatus, parties, n);
  if (parties == 0) {
    /* Not an honest empty: the SDN list is never empty. The layout moved and
     * the tail window no longer reaches DistinctParties. */
    fprintf(stderr, "[ofac-sdn-advanced] tail window contained no <DistinctParty> — "
                    "raise JO_SANC_SDN_TAIL_MB\n");
    return -1;
  }
  return 0;
}

static const source_def sanc_ofac_sdn_advanced_def = {
  .id = "ofac-sdn-advanced", .collector = "sanctions",
  .name = "OFAC SDN Advanced XML (full designation detail)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = SDN_ADV_URL,
  .description = "OFAC's advanced (structured) SDN export. Unlike SDN.CSV it carries typed features: passport and national-ID documents, dates of birth, vessel call signs and digital-currency (crypto) addresses per designated party.",
  .license = "US Government work, public domain. OFAC asks that the list be re-checked against the authoritative publication.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_ofac_sdn_advanced_def)
