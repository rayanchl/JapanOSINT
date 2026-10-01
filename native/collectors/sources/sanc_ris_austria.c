/* Austria RIS — Rechtsinformationssystem, judicial decisions API (OGH and lower courts).
 *
 * Endpoint : https://data.bka.gv.at/ris/api/v2.6/Judikatur
 *              ?Applikation=Justiz&Suchworte=<term>&DokumenteProSeite=Fifty&Seitennummer=1
 * Format   : JSON.
 * Verified : HTTP 200; probe term "Betrug" (fraud) -> 396 hits, and "Geldwäscherei" (money
 *            laundering) -> 18 hits, e.g. JJR_20140826_OGH0002_0110NS00041_14I0000_001,
 *            Organ OGH, published 2014-09-30, Geschäftszahl 11Ns41/14i.
 * Keyless  : yes.
 * Emits    : document id, deciding court/organ, document type (Rechtssatz / Text), the case
 *            number(s), the decision and publication dates, the ECLI where present, and the
 *            RIS document URL.
 * Geometry : NONE (R2).
 * Licence  : RIS open data — free reuse under the Austrian federal open-data terms
 *            (CC BY 4.0 equivalent). Attribution: Rechtsinformationssystem des Bundes (RIS).
 *
 * PARSE TRAPS handled here:
 *   * the payload is deeply nested — OgdSearchResult.OgdDocumentResults.OgdDocumentReference[]
 *     .Data.Metadaten.{Technisch,Allgemein,Judikatur} — and RIS moves fields between those
 *     three groups depending on document type, so fields are resolved with a depth-limited
 *     search instead of one hard-coded path;
 *   * OgdDocumentReference is an ARRAY for multi-hit queries but a bare OBJECT when a single
 *     document matches — both shapes are handled;
 *   * Geschaeftszahl is sometimes a string, sometimes {"item": "…"} and sometimes
 *     {"item": [ … dozens … ]};
 *   * the hit count lives at .Hits["#text"] as a STRING, not a number.
 *
 * The API needs a search term. On an OSINT pivot ctx->entity is used verbatim. On a scheduled
 * run the collector walks a small fixed set of financial-crime terms relevant to this platform
 * — those are QUERIES, not data: every emitted field still comes from the response.
 */
#include "sanc_common.inc"

static int ris_emit(intel_sink *sink, const cJSON *ref, const char *term) {
  const cJSON *meta = cJSON_GetObjectItem(
      cJSON_GetObjectItem(ref, "Data"), "Metadaten");
  if (!meta) return 0;

  const char *id = sanc_deep_str(meta, "ID", 4);
  if (!id) return 0;
  const char *organ = sanc_deep_str(meta, "Organ", 4);
  const char *gericht = sanc_deep_str(meta, "Gericht", 6);
  const char *doctype = sanc_deep_str(meta, "Dokumenttyp", 4);
  const char *url = sanc_deep_str(meta, "DokumentUrl", 4);
  const char *published = sanc_deep_str(meta, "Veroeffentlicht", 4);
  const char *decided = sanc_deep_str(meta, "Entscheidungsdatum", 5);
  const char *ecli = sanc_deep_str(meta, "EuropeanCaseLawIdentifier", 5);
  const char *gz = sanc_deep_str(meta, "Geschaeftszahl", 5);
  const char *satznr = sanc_deep_str(meta, "Rechtssatznummern", 6);
  const char *gebiet = sanc_deep_str(meta, "Rechtsgebiete", 6);

  const char *court = gericht ? gericht : organ;
  char title[400];
  if (gz) snprintf(title, sizeof title, "%s %s", court ? court : "RIS", gz);
  else snprintf(title, sizeof title, "%s %s", court ? court : "RIS", id);

  char summary[400];
  snprintf(summary, sizeof summary, "%s%s%s%s%s",
           doctype ? doctype : "Judikatur",
           gebiet ? " · " : "", gebiet ? gebiet : "",
           decided ? " · " : "", decided ? decided : "");

  cJSON *body = cJSON_CreateObject();
  sanc_add(body, "id", id);
  sanc_add(body, "court", court);
  sanc_add(body, "document_type", doctype);
  sanc_add(body, "case_numbers", gz);
  sanc_add(body, "legal_field", gebiet);
  sanc_add(body, "rechtssatz_number", satznr);
  sanc_add(body, "decision_date", decided);
  sanc_add(body, "published", published);
  sanc_add(body, "ecli", ecli);
  sanc_add(body, "document_url", url);
  char *bj = cJSON_PrintUnformatted(body);
  cJSON_Delete(body);

  cJSON *props = cJSON_CreateObject();
  sanc_add(props, "ris_id", id);
  sanc_add(props, "query", term);
  sanc_add(props, "court", court);
  sanc_add(props, "organ", organ);
  sanc_add(props, "document_type", doctype);
  sanc_add(props, "case_numbers", gz);
  sanc_add(props, "legal_field", gebiet);
  sanc_add(props, "ecli", ecli);
  sanc_add(props, "decision_date", decided);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key = id;
  it.title = title;
  it.summary = summary;
  it.body = bj;
  it.link = url;
  it.lang = "de";
  it.published_at = published ? published : decided;
  it.record_type = "ris-austria-decision";
  it.properties_json = pj ? pj : "{}";
  it.tags_json = "[\"court\",\"judgment\",\"austria\"]";
  int ok = sink->emit(sink, &it) >= 0;
  free(bj);
  free(pj);
  return ok;
}

static int ris_query(const source_ctx *ctx, intel_sink *sink, const char *term,
                     int *fetched) {
  char *enc = jo_urlencode(term);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://data.bka.gv.at/ris/api/v2.6/Judikatur"
           "?Applikation=Justiz&Suchworte=%s&DokumenteProSeite=Fifty&Seitennummer=1",
           enc);
  free(enc);

  cJSON *doc = sanc_http_json(ctx, url, NULL, 45000, "ris-austria");
  if (!doc) return 0;
  *fetched = 1;

  cJSON *res = cJSON_GetObjectItem(
      cJSON_GetObjectItem(doc, "OgdSearchResult"), "OgdDocumentResults");
  cJSON *refs = cJSON_GetObjectItem(res, "OgdDocumentReference");
  int n = 0;
  if (cJSON_IsArray(refs)) {
    const cJSON *ref;
    cJSON_ArrayForEach(ref, refs) n += ris_emit(sink, ref, term);
  } else if (cJSON_IsObject(refs)) {          /* single hit -> bare object */
    n += ris_emit(sink, refs, term);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int fetched = 0, n = 0;
  if (ctx->entity && *ctx->entity) {
    n = ris_query(ctx, sink, ctx->entity, &fetched);
  } else {
    static const char *const TERMS[] = { "Geldw\xc3\xa4scherei", "Korruption",
                                         "Sanktionen", "Betrug", NULL };
    for (int i = 0; TERMS[i]; i++) n += ris_query(ctx, sink, TERMS[i], &fetched);
  }
  fprintf(stderr, "[ris-austria] emitted %d\n", n);
  /* R3: only a total failure to fetch is an error. A term that matched nothing
   * is a clean, meaningful answer. */
  return fetched ? 0 : -1;
}

static const source_def sanc_ris_austria_def = {
  .id = "ris-austria-judikatur", .collector = "sanctions",
  .name = "Austria RIS — judicial decisions (OGH and lower courts)",
  .update_interval_sec = 43200, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.bka.gv.at/ris/api/v2.6/Judikatur",
  .description = "Austria's official legal information system, full-text searchable over Supreme Court and appellate decisions, returning case numbers, ECLIs and document URLs as JSON.",
  .license = "RIS open data — free reuse under Austrian federal open-data terms (CC BY 4.0 equivalent). Attribution: Rechtsinformationssystem des Bundes.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_ris_austria_def)
