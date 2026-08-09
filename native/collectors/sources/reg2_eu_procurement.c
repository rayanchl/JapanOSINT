/* collectors/sources/reg2_eu_procurement.c
 *
 * Four European public-procurement / public-spending feeds. All keyless, all
 * scheduled sweeps, all emitting only fields the upstream actually returned.
 * None of them publish coordinates (France's lieuexecution_code is a
 * department code, not a point), so no row ever claims geo — R2.
 *
 *  nl-tenderned-notices
 *    GET https://www.tenderned.nl/papi/tenderned-rs-tns/v2/publicaties?page=0&size=50
 *    Envelope {"content":[...],"totalElements":N}. Emits publicatieId,
 *    publicatieDatum, aanbestedingNaam, opdrachtgeverNaam (buying authority),
 *    typeOpdracht, opdrachtBeschrijving, europees and typePublicatie.omschrijving
 *    (prior-information / contract-notice / award).
 *    Licence: procurement notices published by PIANOo/TenderNed; no stated
 *    restriction on the public API.
 *
 *  fr-decp-marches
 *    GET https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/
 *        decp-v3-marches-valides/records?limit=50&order_by=datenotification%20desc
 *    Envelope {"total_count":N,"results":[...]}. Emits objet, montant,
 *    acheteur_nom / acheteur_id, titulaire_id_* (supplier SIRETs — pivots into
 *    SIRENE), datenotification, codecpv, lieuexecution_nom.
 *    Licence: Licence Ouverte / Open Licence (Ministere de l'Economie).
 *
 *  pl-bzp-notices
 *    GET https://ezamowienia.gov.pl/mo-board/api/v1/Board/Search?
 *        SortingColumnName=PublicationDate&SortingDirection=DESC&PageNumber=1&PageSize=50
 *    Bare JSON array (no envelope). Emits noticeNumber, bzpNumber, noticeType
 *    (ContractNotice / ContractAwardNotice / ContractPerformingNotice),
 *    publicationDate, orderObject, cpvCode, organizationName, organizationCity,
 *    orderType, isTenderAmountBelowEU.
 *    Licence: official UZP e-Zamowienia platform; notices are statutorily public.
 *
 *  gr-diavgeia-decisions
 *    GET https://diavgeia.gov.gr/opendata/search.json?q=type:%22ΣΥΜΒΑΣΗ%22&size=50
 *    Envelope {"decisions":[...]}. Emits ada / protocolNumber, subject,
 *    issueDate (epoch MILLISECONDS → ISO), organizationId, and from
 *    extraFieldValues: org.name / org.afm (buyer VAT number),
 *    sponsor[].sponsorAFMName (counterparty name + AFM) and expenseAmount.
 *    Licence: statutory transparency programme (Law 3861/2010); the opendata
 *    endpoint is the official machine interface.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

typedef struct {
  const char *service, *url;
  const char *envelope;      /* NULL => the payload IS the array */
  const char *rtype, *lang;
  const char *title_f, *title_alt_f, *key_f, *date_f;
  const char *sum1, *sum2, *sum3;
} eu_src;

static const char *eu_field(const cJSON *row, const char *k,
                            char *buf, size_t n) {
  if (!k) return NULL;
  const cJSON *v = cJSON_GetObjectItem(row, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) return v->valuestring;
  if (cJSON_IsNumber(v)) { snprintf(buf, n, "%.10g", v->valuedouble); return buf; }
  return NULL;
}

/* Copy scalars, flattening one level of nested objects as parent_child. */
static void eu_copy(cJSON *props, const cJSON *row, const char *prefix) {
  for (const cJSON *f = row->child; f; f = f->next) {
    if (!f->string || !f->string[0]) continue;
    char key[192];
    if (prefix) snprintf(key, sizeof key, "%s_%s", prefix, f->string);
    else        snprintf(key, sizeof key, "%s", f->string);
    if (cJSON_IsString(f) && f->valuestring && f->valuestring[0])
      cJSON_AddStringToObject(props, key, f->valuestring);
    else if (cJSON_IsNumber(f))
      cJSON_AddNumberToObject(props, key, f->valuedouble);
    else if (cJSON_IsBool(f))
      cJSON_AddBoolToObject(props, key, cJSON_IsTrue(f) ? 1 : 0);
    else if (cJSON_IsObject(f) && !prefix)
      eu_copy(props, f, f->string);
  }
}

static int eu_run(const eu_src *s, const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, s->url, 30000);
  if (!doc) { fprintf(stderr, "[%s] fetch/parse failed\n", s->service); return -1; }

  const cJSON *arr = s->envelope ? cJSON_GetObjectItem(doc, s->envelope) : doc;
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[%s] unexpected payload shape\n", s->service);
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, arr) {
    if (!cJSON_IsObject(row)) continue;
    char tb[64];
    const char *raw = eu_field(row, s->title_f, tb, sizeof tb);
    if (!raw) raw = eu_field(row, s->title_alt_f, tb, sizeof tb);
    if (!raw) continue;                       /* no real title -> no row (R1) */
    char title[500];
    snprintf(title, sizeof title, "%s", raw);

    char kb[64];
    const char *key = eu_field(row, s->key_f, kb, sizeof kb);
    char hashed[21];
    if (!key) {
      const char *parts[2] = { s->service, title };
      feed_hash_key(hashed, parts, 2);
      key = hashed;
    }

    char db[64], iso[64];
    const char *pub = eu_field(row, s->date_f, db, sizeof db);
    if (pub) {
      snprintf(iso, sizeof iso, "%s", pub);
      char *sp = strchr(iso, ' ');
      if (sp) *sp = 'T';
      pub = iso;
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", s->service);
    cJSON_AddStringToObject(props, "dataset_url", s->url);
    eu_copy(props, row, NULL);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char s1[128], s2[128], s3[128], summary[420];
    const char *a = eu_field(row, s->sum1, s1, sizeof s1);
    const char *b = eu_field(row, s->sum2, s2, sizeof s2);
    const char *c = eu_field(row, s->sum3, s3, sizeof s3);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             a ? a : "", (a && b) ? " · " : "", b ? b : "",
             ((a || b) && c) ? " · " : "", c ? c : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = s->url;
    it.lang            = s->lang;
    it.published_at    = pub;
    it.record_type     = s->rtype;
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"europe\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", s->service, n);
  return 0;
}

#define EU_SOURCE(sym, ID, NAME, URL, COLL, CAT, LIC, DESC, IVAL, ...)        \
  static const eu_src sym##_cfg = { .service = ID, .url = URL, __VA_ARGS__ };  \
  static int sym##_run(const source_ctx *ctx, intel_sink *sink) {              \
    return eu_run(&sym##_cfg, ctx, sink);                                      \
  }                                                                            \
  static const source_def sym##_def = {                                        \
    .id = ID, .collector = COLL, .name = NAME,                                 \
    .update_interval_sec = IVAL, .run = sym##_run,                             \
    .category = CAT, .type = "api", .url = URL,                                \
    .description = DESC, .license = LIC, .layer = NULL, .free_tier = 1,        \
  };                                                                           \
  REGISTER_SOURCE(sym##_def)

EU_SOURCE(nl_tn, "nl-tenderned-notices",
  "TenderNed Netherlands procurement notices",
  "https://www.tenderned.nl/papi/tenderned-rs-tns/v2/publicaties?page=0&size=50",
  "government", "government",
  "Public procurement notices published by PIANOo/TenderNed; no stated "
  "restriction on the public API.",
  "Dutch national tender portal — every published Dutch public procurement "
  "notice with buying authority, contract type and full description. Keyless.",
  10800,
  .envelope = "content", .rtype = "procurement-notice", .lang = "nl",
  .title_f = "aanbestedingNaam", .title_alt_f = "opdrachtBeschrijving",
  .key_f = "publicatieId", .date_f = "publicatieDatum",
  .sum1 = "opdrachtgeverNaam", .sum2 = "typeOpdracht", .sum3 = "opdrachtBeschrijving")

EU_SOURCE(fr_decp, "fr-decp-marches",
  "France DECP — declared public contracts",
  "https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/decp-v3-marches-valides/records?limit=50&order_by=datenotification%20desc",
  "government", "government",
  "Licence Ouverte / Open Licence (data.economie.gouv.fr, Ministere de l'Economie).",
  "Validated French public contracts with buyer SIRET, supplier SIRET, amount, "
  "CPV code and place of execution — pivots into the SIRENE company registry. "
  "Keyless.",
  86400,
  .envelope = "results", .rtype = "procurement-contract", .lang = "fr",
  .title_f = "objet", .title_alt_f = "acheteur_nom",
  .key_f = "id", .date_f = "datenotification",
  .sum1 = "acheteur_nom", .sum2 = "montant", .sum3 = "lieuexecution_nom")

EU_SOURCE(pl_bzp, "pl-bzp-notices",
  "Poland Biuletyn Zamowien Publicznych notices",
  "https://ezamowienia.gov.pl/mo-board/api/v1/Board/Search?SortingColumnName=PublicationDate&SortingDirection=DESC&PageNumber=1&PageSize=50",
  "government", "government",
  "Official UZP e-Zamowienia platform; notices are statutorily public.",
  "Poland's official public-procurement bulletin — every below- and "
  "above-threshold notice with contracting authority, city, CPV code and notice "
  "type. Keyless.",
  10800,
  .envelope = NULL, .rtype = "procurement-notice", .lang = "pl",
  .title_f = "orderObject", .title_alt_f = "organizationName",
  .key_f = "noticeNumber", .date_f = "publicationDate",
  .sum1 = "organizationName", .sum2 = "organizationCity", .sum3 = "noticeType")

/* ------------------------------------------------------ Greece — Diavgeia */

static int gr_run(const source_ctx *ctx, intel_sink *sink) {
  const char *url =
    "https://diavgeia.gov.gr/opendata/search.json"
    "?q=type:%22%CE%A3%CE%A5%CE%9C%CE%92%CE%91%CE%A3%CE%97%22&size=50";
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[gr-diavgeia-decisions] fetch/parse failed\n"); return -1; }

  const cJSON *arr = cJSON_GetObjectItem(doc, "decisions");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[gr-diavgeia-decisions] unexpected payload shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  const cJSON *d;
  cJSON_ArrayForEach(d, arr) {
    if (!cJSON_IsObject(d)) continue;
    const char *subject = jo_sv(d, "subject");
    const char *ada     = jo_sv(d, "ada");
    const char *proto   = jo_sv(d, "protocolNumber");
    if (!subject && !ada) continue;

    /* issueDate is epoch milliseconds. */
    char iso[32];
    const char *pub = NULL;
    const cJSON *idt = cJSON_GetObjectItem(d, "issueDate");
    if (cJSON_IsNumber(idt) && idt->valuedouble > 0) {
      time_t secs = (time_t)(idt->valuedouble / 1000.0);
      struct tm g;
      gmtime_r(&secs, &g);
      strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &g);
      pub = iso;
    } else if (jo_sv(d, "issueDate")) {
      snprintf(iso, sizeof iso, "%s", jo_sv(d, "issueDate"));
      pub = iso;
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "gr-diavgeia-decisions");
    cJSON_AddStringToObject(props, "source", "diavgeia.gov.gr");
    if (ada)   cJSON_AddStringToObject(props, "ada", ada);
    if (proto) cJSON_AddStringToObject(props, "protocol_number", proto);
    if (subject) cJSON_AddStringToObject(props, "subject", subject);
    if (pub)   cJSON_AddStringToObject(props, "issue_date", pub);
    if (jo_sv(d, "organizationId"))
      cJSON_AddStringToObject(props, "organization_id", jo_sv(d, "organizationId"));
    if (jo_sv(d, "decisionTypeUid"))
      cJSON_AddStringToObject(props, "decision_type_uid", jo_sv(d, "decisionTypeUid"));
    if (jo_sv(d, "documentUrl"))
      cJSON_AddStringToObject(props, "document_url", jo_sv(d, "documentUrl"));

    const char *buyer = NULL;
    const cJSON *efv = cJSON_GetObjectItem(d, "extraFieldValues");
    if (efv) {
      const cJSON *org = cJSON_GetObjectItem(efv, "org");
      if (org) {
        buyer = jo_sv(org, "name");
        if (buyer) cJSON_AddStringToObject(props, "buyer_name", buyer);
        if (jo_sv(org, "afm"))
          cJSON_AddStringToObject(props, "buyer_afm", jo_sv(org, "afm"));
      }
      const cJSON *amt = cJSON_GetObjectItem(efv, "expenseAmount");
      if (amt) {
        const cJSON *v = cJSON_GetObjectItem(amt, "amount");
        if (cJSON_IsNumber(v))
          cJSON_AddNumberToObject(props, "expense_amount", v->valuedouble);
        if (jo_sv(amt, "currency"))
          cJSON_AddStringToObject(props, "expense_currency", jo_sv(amt, "currency"));
      }
      const cJSON *sp = cJSON_GetObjectItem(efv, "sponsor");
      if (cJSON_IsArray(sp)) {
        cJSON *list = cJSON_CreateArray();
        const cJSON *e;
        cJSON_ArrayForEach(e, sp) {
          const cJSON *sa = cJSON_GetObjectItem(e, "sponsorAFMName");
          if (!sa) continue;
          const char *nm = jo_sv(sa, "name");
          if (!nm) continue;
          cJSON *o = cJSON_CreateObject();
          cJSON_AddStringToObject(o, "name", nm);
          if (jo_sv(sa, "afm")) cJSON_AddStringToObject(o, "afm", jo_sv(sa, "afm"));
          cJSON_AddItemToArray(list, o);
        }
        cJSON_AddItemToObject(props, "counterparties", list);
      }
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[500];
    snprintf(title, sizeof title, "%s", subject ? subject : ada);

    char summary[420];
    snprintf(summary, sizeof summary, "%s%s%s",
             buyer ? buyer : "", (buyer && ada) ? " · " : "", ada ? ada : "");

    char link[256];
    if (ada) snprintf(link, sizeof link, "https://diavgeia.gov.gr/decision/view/%s", ada);
    else     snprintf(link, sizeof link, "%s", url);

    intel_item it = {0};
    it.remote_key      = ada ? ada : (proto ? proto : title);
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = link;
    it.lang            = "el";
    it.published_at    = pub;
    it.record_type     = "gr-transparency-decision";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"greece\",\"transparency\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[gr-diavgeia-decisions] emitted %d\n", n);
  return 0;
}

static const source_def reg2_gr_diavgeia_def = {
  .id = "gr-diavgeia-decisions", .collector = "government",
  .name = "Greece Diavgeia transparency decisions (contracts/awards)",
  .update_interval_sec = 10800, .run = gr_run,
  .category = "government", .type = "api",
  .url = "https://diavgeia.gov.gr/opendata/search.json",
  .description = "Greek public contracting stream from the statutory Diavgeia "
                 "transparency programme — buyer VAT number (AFM), supplier AFM "
                 "and amount for every published contract decision. Keyless.",
  .license = "Statutory transparency programme (Law 3861/2010); the opendata "
             "endpoint is the official machine interface.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_gr_diavgeia_def)
