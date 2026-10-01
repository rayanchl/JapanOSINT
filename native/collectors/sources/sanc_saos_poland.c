/* SAOS — Polish court judgments API.
 *
 * Endpoint : https://www.saos.org.pl/api/search/judgments
 *              ?pageSize=100&sortingField=JUDGMENT_DATE&sortingDirection=DESC
 * Format   : HAL-style JSON, {"links":[…], "items":[…], "queryTemplate":{…}, "info":{…}}.
 * Verified : HTTP 200; paged feed over ~400k judgments, items carrying courtType, courtCases
 *            [{caseNumber}], judgmentType, judges[{name,specialRoles}], judgmentDate, division
 *            (with the court name) and full textContent.
 * Keyless  : yes; the API is documented as open.
 * Emits    : case number, court type and the deciding court/division as returned, judgment
 *            type, judgment date, the named judges with their special roles (presiding judge,
 *            reporter), keywords, and a truncated head of the judgment text.
 * Geometry : NONE (R2).
 * Licence  : SAOS (ICM, University of Warsaw) republishes public Polish court judgments;
 *            open API, no key.
 *
 * THE TRAP THIS URL FIXES: SAOS defaults to sortingField=DATABASE_ID ASC, which returns 2009
 * judgments first — a collector on the bare /api/search/judgments path answers 200 and quietly
 * emits decade-old rows as if they were fresh. sortingField=JUDGMENT_DATE&sortingDirection=DESC
 * is mandatory here; do not "simplify" this URL.
 *
 * textContent runs to hundreds of KB per item and is truncated (on a UTF-8 character boundary)
 * before it is emitted.
 */
#include "sanc_common.inc"

#define SAOS_URL "https://www.saos.org.pl/api/search/judgments" \
                 "?pageSize=100&sortingField=JUDGMENT_DATE&sortingDirection=DESC"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = sanc_http_json(ctx, SAOS_URL, NULL, 60000, "saos-poland");
  if (!doc) return -1;

  cJSON *items = cJSON_GetObjectItem(doc, "items");
  int n = 0;
  const cJSON *j;
  cJSON_ArrayForEach(j, items) {
    const cJSON *idn = cJSON_GetObjectItem(j, "id");
    char jid[32] = {0};
    if (cJSON_IsNumber(idn)) snprintf(jid, sizeof jid, "%d", idn->valueint);
    if (!jid[0]) continue;

    const char *ctype = jo_sv(j, "courtType");
    const char *jtype = jo_sv(j, "judgmentType");
    const char *jdate = jo_sv(j, "judgmentDate");
    const char *text = jo_sv(j, "textContent");

    /* case numbers */
    char caseno[192];
    caseno[0] = 0;
    {
      size_t k = 0;
      const cJSON *cc;
      cJSON_ArrayForEach(cc, cJSON_GetObjectItem(j, "courtCases")) {
        const char *cn = jo_sv(cc, "caseNumber");
        if (!cn) continue;
        size_t need = strlen(cn) + (k ? 2 : 0);
        if (k + need >= sizeof caseno - 1) break;
        if (k) { strcpy(caseno + k, ", "); k += 2; }
        strcpy(caseno + k, cn);
        k += strlen(cn);
      }
    }

    /* deciding division + court */
    const cJSON *div = cJSON_GetObjectItem(j, "division");
    const char *divname = jo_sv(div, "name");
    const char *courtname = jo_sv(cJSON_GetObjectItem(div, "court"), "name");

    /* judges, with the roles SAOS states */
    cJSON *judges = cJSON_CreateArray();
    const cJSON *jd;
    cJSON_ArrayForEach(jd, cJSON_GetObjectItem(j, "judges")) {
      const char *jn = jo_sv(jd, "name");
      if (!jn) continue;
      char roles[128];
      sanc_join(cJSON_GetObjectItem(jd, "specialRoles"), ",", 4, roles, sizeof roles);
      char line[320];
      snprintf(line, sizeof line, "%s%s%s%s", jn, roles[0] ? " (" : "", roles,
               roles[0] ? ")" : "");
      cJSON_AddItemToArray(judges, cJSON_CreateString(line));
      if (cJSON_GetArraySize(judges) >= 12) break;
    }
    char keywords[256];
    sanc_join(cJSON_GetObjectItem(j, "keywords"), ", ", 10, keywords, sizeof keywords);

    char title[400];
    snprintf(title, sizeof title, "%s%s%s",
             caseno[0] ? caseno : jid,
             courtname ? " — " : "", courtname ? courtname : "");

    char excerpt[1024];
    sanc_trunc(excerpt, sizeof excerpt, text ? text : "");
    sanc_collapse_ws(excerpt);

    char summary[400];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             jtype ? jtype : "Judgment", ctype ? " · " : "", ctype ? ctype : "",
             jdate ? " · " : "", jdate ? jdate : "");

    char link[128];
    snprintf(link, sizeof link, "https://www.saos.org.pl/judgments/%s", jid);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "case_number", caseno[0] ? caseno : NULL);
    sanc_add(body, "court", courtname);
    sanc_add(body, "division", divname);
    sanc_add(body, "court_type", ctype);
    sanc_add(body, "judgment_type", jtype);
    sanc_add(body, "judgment_date", jdate);
    if (keywords[0]) sanc_add(body, "keywords", keywords);
    cJSON_AddItemToObject(body, "judges", cJSON_Duplicate(judges, 1));
    sanc_add(body, "text_excerpt", excerpt[0] ? excerpt : NULL);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "judgment_id", jid);
    sanc_add(props, "case_number", caseno[0] ? caseno : NULL);
    sanc_add(props, "court", courtname);
    sanc_add(props, "division", divname);
    sanc_add(props, "court_type", ctype);
    sanc_add(props, "judgment_type", jtype);
    sanc_add(props, "judgment_date", jdate);
    if (keywords[0]) sanc_add(props, "keywords", keywords);
    cJSON_AddItemToObject(props, "judges", judges);   /* transferred */
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = jid;
    it.title = title;
    it.summary = summary;
    it.body = bj;
    it.link = link;
    it.lang = "pl";
    it.published_at = jdate;
    it.record_type = "saos-judgment";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"court\",\"judgment\",\"poland\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[saos-poland] emitted %d\n", n);
  return 0;
}

static const source_def sanc_saos_poland_def = {
  .id = "saos-poland-judgments", .collector = "sanctions",
  .name = "SAOS — Polish court judgments",
  .update_interval_sec = 43200, .run = run,
  .category = "government", .type = "api",
  .url = SAOS_URL,
  .description = "Polish common, administrative, supreme and constitutional court judgments with named judges, case numbers and text — a rare open bulk API for a national judiciary.",
  .license = "SAOS (ICM, University of Warsaw) — open API republishing public court judgments.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_saos_poland_def)
