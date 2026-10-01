/* EU Sanctions Map — restrictive-measure regimes.
 *
 * Endpoint : https://www.sanctionsmap.eu/api/v1/regime
 * Format   : JSON, {"data":[ … ]} — 55 regimes on the verified probe.
 * Verified : HTTP 200, 396 KB; regime id 1 "Restrictive measures imposed with respect to the
 *            Taliban", adopted_by UN, country Afghanistan, with legal acts and measures.
 * Keyless  : yes.
 * Emits    : per regime — its official specification (title), the target country as the map
 *            gives it, who adopted it (EU/UN), the programme codes, the measure types in force
 *            (asset freeze, arms export, travel ban, sectoral …) with the publisher's own
 *            description, the legal acts with their CELEX/Official Journal links, and the last
 *            amendment date.
 * Geometry : NONE (R2). A regime names a country; it is NOT a point, and pinning it to a
 *            capital would be an invented coordinate.
 * Licence  : published by the EU Council/EEAS for public information; attribution expected.
 *            The map itself states it is NOT a legally binding source — the Official Journal
 *            act (linked per row in properties.legal_acts) is the citable instrument.
 *
 * Notes    : /api/v1/regime is the record-bearing path. The commonly cited
 *            /api/v1/data?members&carrier=regime endpoint answers 200 with only UI
 *            translations and the competent-authority directory — no regime records at all.
 *            That is the HTTP-200-but-empty trap for this source; do not "simplify" back to it.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = sanc_http_json(ctx, "https://www.sanctionsmap.eu/api/v1/regime",
                              NULL, 40000, "eu-sanctions-map");
  if (!doc) return -1;

  cJSON *arr = cJSON_GetObjectItem(doc, "data");
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    const char *spec = jo_sv(r, "specification");
    if (!spec) continue;                         /* no fetched title -> no row */
    const char *acronym = jo_sv(r, "acronym");
    const cJSON *idn = cJSON_GetObjectItem(r, "id");
    char rid[32] = {0};
    if (cJSON_IsNumber(idn)) snprintf(rid, sizeof rid, "%d", idn->valueint);

    const cJSON *cd = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "country"), "data");
    const char *country = jo_sv(cd, "title");
    const char *cc = jo_sv(cd, "code");
    const cJSON *ad = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "adopted_by"), "data");
    const char *adopted = jo_sv(ad, "title");

    char programme[128];
    sanc_join(cJSON_GetObjectItem(r, "programme"), ", ", 8, programme, sizeof programme);

    /* measure types actually in force for this regime */
    cJSON *measures = cJSON_CreateArray();
    const cJSON *m;
    cJSON_ArrayForEach(m, cJSON_GetObjectItem(cJSON_GetObjectItem(r, "measures"), "data")) {
      const cJSON *td = cJSON_GetObjectItem(cJSON_GetObjectItem(m, "type"), "data");
      const char *mt = jo_sv(td, "title");
      if (mt) cJSON_AddItemToArray(measures, cJSON_CreateString(mt));
      if (cJSON_GetArraySize(measures) >= 16) break;
    }
    /* legal acts with their EUR-Lex links */
    cJSON *acts = cJSON_CreateArray();
    const char *first_act_url = NULL;
    const cJSON *la;
    cJSON_ArrayForEach(la, cJSON_GetObjectItem(cJSON_GetObjectItem(r, "legal_acts"), "data")) {
      const char *num = jo_sv(la, "number");
      const char *url = jo_sv(la, "url");
      const char *ttl = jo_sv(la, "title");
      if (!num && !ttl) continue;
      if (!first_act_url && url) first_act_url = url;
      cJSON *o = cJSON_CreateObject();
      sanc_add(o, "number", num);
      sanc_add(o, "title", ttl);
      sanc_add(o, "url", url);
      cJSON_AddItemToArray(acts, o);
      if (cJSON_GetArraySize(acts) >= 12) break;
    }

    /* last amendment: unix seconds in the payload -> ISO date */
    char amended[16] = {0};
    const cJSON *am = cJSON_GetObjectItem(r, "amendment");
    if (cJSON_IsNumber(am) && am->valuedouble > 0) {
      int y, mo, d;
      sanc_civil_from_days((long)(am->valuedouble / 86400.0), &y, &mo, &d);
      snprintf(amended, sizeof amended, "%04d-%02d-%02d", y, mo, d);
    }

    char mlist[320];
    sanc_join(measures, ", ", 16, mlist, sizeof mlist);
    char summary[512];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             adopted ? adopted : "EU", country ? " · " : "", country ? country : "",
             mlist[0] ? " · " : "", mlist);

    const char *notes = jo_sv(r, "notes");
    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "regime", spec);
    sanc_add(body, "acronym", acronym);
    sanc_add(body, "country", country);
    sanc_add(body, "adopted_by", adopted);
    if (programme[0]) sanc_add(body, "programme", programme);
    if (mlist[0]) sanc_add(body, "measures", mlist);
    sanc_add(body, "last_amendment", amended[0] ? amended : NULL);
    sanc_add(body, "notes", notes);
    cJSON_AddItemToObject(body, "legal_acts", cJSON_Duplicate(acts, 1));
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "regime_id", rid[0] ? rid : NULL);
    sanc_add(props, "country", country);
    sanc_add(props, "country_code", cc);
    sanc_add(props, "adopted_by", adopted);
    if (programme[0]) sanc_add(props, "programme", programme);
    cJSON_AddItemToObject(props, "measures", measures);   /* transferred */
    cJSON_AddItemToObject(props, "legal_acts", acts);     /* transferred */
    sanc_add(props, "last_amendment", amended[0] ? amended : NULL);
    sanc_add(props, "caveat",
             "The EU Sanctions Map is published for information only and is not legally "
             "binding; cite the Official Journal act linked in legal_acts.");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = rid[0] ? rid : spec;
    it.title = spec;
    it.summary = summary;
    it.body = bj;
    it.link = first_act_url;          /* the EUR-Lex act, when the map gave one */
    it.lang = "en";
    it.published_at = amended[0] ? amended : NULL;
    it.record_type = "eu-sanctions-regime";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sanctions\",\"eu\",\"regime\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[eu-sanctions-map] emitted %d\n", n);
  return 0;
}

static const source_def sanc_eu_sanctions_map_def = {
  .id = "eu-sanctions-map-regimes", .collector = "sanctions",
  .name = "EU Sanctions Map — restrictive-measure regimes",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "api",
  .url = "https://www.sanctionsmap.eu/api/v1/regime",
  .description = "Regime-level view of EU sanctions: which country or theme, adopted by whom, under which legal acts, and which measure types (asset freeze, travel ban, arms embargo, sectoral). Complements the entity-level EU consolidated list.",
  .license = "Published by the EU Council/EEAS for public information; attribution expected. Not a legally binding source — cite the Official Journal act.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_eu_sanctions_map_def)
