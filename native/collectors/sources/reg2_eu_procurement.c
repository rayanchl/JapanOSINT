/* collectors/sources/reg2_eu_procurement.c
 *
 * Four European public-procurement / public-spending feeds. All keyless, all
 * scheduled sweeps, all emitting only fields the upstream actually returned.
 * None of them publish coordinates (France's lieuexecution_code is a
 * department code, not a point), so no row ever claims geo — R2.
 *
 * The three EU_SOURCE feeds are walked page by page (eu_run -> pw_walk).
 *
 *  nl-tenderned-notices
 *    GET https://www.tenderned.nl/papi/tenderned-rs-tns/v2/publicaties?page=0&size=100
 *    (page is 0-based; size above 100 is refused with HTTP 400)
 *    Envelope {"content":[...],"totalElements":N}. Emits publicatieId,
 *    publicatieDatum, aanbestedingNaam, opdrachtgeverNaam (buying authority),
 *    typeOpdracht, opdrachtBeschrijving, europees and typePublicatie.omschrijving
 *    (prior-information / contract-notice / award).
 *    Licence: procurement notices published by PIANOo/TenderNed; no stated
 *    restriction on the public API.
 *
 *  fr-decp-marches
 *    GET https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/
 *        decp-v3-marches-valides/records?limit=100&offset=0&order_by=datenotification%20desc
 *    Envelope {"total_count":N,"results":[...]}. Emits objet, montant,
 *    acheteur_nom / acheteur_id, titulaire_id_* (supplier SIRETs — pivots into
 *    SIRENE), datenotification, codecpv, lieuexecution_nom.
 *    Licence: Licence Ouverte / Open Licence (Ministere de l'Economie).
 *
 *  pl-bzp-notices
 *    GET https://ezamowienia.gov.pl/mo-board/api/v1/Board/Search?
 *        SortingColumnName=PublicationDate&SortingDirection=DESC&PageNumber=1&PageSize=10
 *    (the server answers 10 rows whatever PageSize asks for — measured at 10,
 *    20, 25, 50 and 100 — so 10 is declared, which is what lets the page walk
 *    judge a page full) Bare JSON array (no envelope). Emits noticeNumber, bzpNumber, noticeType
 *    (ContractNotice / ContractAwardNotice / ContractPerformingNotice),
 *    publicationDate, orderObject, cpvCode, organizationName, organizationCity,
 *    orderType, isTenderAmountBelowEU.
 *    Licence: official UZP e-Zamowienia platform; notices are statutorily public.
 *
 *  gr-diavgeia-decisions
 *    GET https://diavgeia.gov.gr/opendata/search.json?type=Γ.3.4&size=500&page=0
 *    (type Γ.3.4 is the one Diavgeia decision type labelled ΣΥΜΒΑΣΗ; see the
 *    note above gr_run for why it is not `q=type:"ΣΥΜΒΑΣΗ"` any more.)
 *    Envelope {"decisions":[...],"info":{"total":N}}. Emits ada /
 *    protocolNumber, subject, issueDate (epoch MILLISECONDS → ISO),
 *    organizationId, decisionTypeId, and from extraFieldValues: contractType,
 *    numberOfPeople, financedProject, person[] (contractor name + AFM),
 *    contractAmount, duration, relatedDecisions — plus every field verbatim.
 *    Licence: statutory transparency programme (Law 3861/2010); the opendata
 *    endpoint is the official machine interface.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include "lib/pagewalk.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"
#include "_timefmt.inc"

typedef struct {
  const char *service, *url;
  const char *envelope;      /* NULL => the payload IS the array */
  const char *rtype, *lang;
  const char *title_f, *title_alt_f, *key_f, *date_f;
  const char *sum1, *sum2, *sum3;
} eu_src;

static const char *eu_field(const cJSON *row, const char *k,
                            char *buf, size_t n);

/* The record key. `key_f` may COMPOSE several fields with `+` (the house
 * id_keys convention): every named field is read and the values joined with
 * '|', a missing one as the empty string. NULL when none of them is present. */
static const char *eu_key(const cJSON *row, const char *spec,
                          char *out, size_t n) {
  if (!spec) return NULL;
  if (!strchr(spec, '+')) return eu_field(row, spec, out, n);
  size_t used = 0;
  int any = 0;
  out[0] = 0;
  const char *p = spec;
  while (*p) {
    const char *e = strchr(p, '+');
    size_t kl = e ? (size_t)(e - p) : strlen(p);
    char k[64], vb[64];
    snprintf(k, sizeof k, "%.*s", (int)kl, p);
    const char *v = eu_field(row, k, vb, sizeof vb);
    if (v) any = 1;
    int w = snprintf(out + used, n - used, "%s%s", used ? "|" : "", v ? v : "");
    if (w < 0 || (size_t)w >= n - used) break;
    used += (size_t)w;
    if (!e) break;
    p = e + 1;
  }
  return any ? out : NULL;
}

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

/* One walk's state: the feed, and whether its FIRST page had the expected
 * array (a first page without one is a shape change, reported as -1 — R3). */
typedef struct { const eu_src *s; int pages; int first_bad; } eu_walk;

/* pw_emit_fn: emit every row of one page; `seen` is what the page HELD. */
static int eu_emit_page(const source_ctx *ctx, intel_sink *sink, const char *id,
                        cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)id;
  eu_walk *w = (eu_walk *)ud;
  const eu_src *s = w->s;
  const cJSON *arr = s->envelope ? cJSON_GetObjectItem(doc, s->envelope) : doc;
  *seen = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
  if (!cJSON_IsArray(arr)) {
    if (w->pages == 0) w->first_bad = 1;
    w->pages++;
    return 0;
  }
  w->pages++;

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

    char kb[320];
    const char *key = eu_key(row, s->key_f, kb, sizeof kb);
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
  return n;
}

/* Every feed here pages, and each one used to be read ONE page deep: TenderNed
 * page=0 of 2,925 (146,213 notices, measured 2026-10-02), BZP PageNumber=1,
 * DECP the first 50 records. pw_walk advances the page/offset parameter each
 * URL already carries while pages come back full, and publishes a truncation
 * notice when its ceiling (JO_PAGE_MAX) stops it short. */
static int eu_run(const eu_src *s, const source_ctx *ctx, intel_sink *sink) {
  eu_walk w = { s, 0, 0 };
  int n = pw_walk(ctx, sink, s->service, s->url, pw_fetch_json, eu_emit_page, &w);
  if (n < 0) { fprintf(stderr, "[%s] fetch/parse failed\n", s->service); return -1; }
  if (w.first_bad) {
    fprintf(stderr, "[%s] unexpected payload shape\n", s->service);
    return -1;
  }
  fprintf(stderr, "[%s] emitted %d across %d page(s)\n", s->service, n, w.pages);
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
  "https://www.tenderned.nl/papi/tenderned-rs-tns/v2/publicaties?page=0&size=100",
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
  "https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/decp-v3-marches-valides/records?limit=100&offset=0&order_by=datenotification%20desc,id",
  "government", "government",
  "Licence Ouverte / Open Licence (data.economie.gouv.fr, Ministere de l'Economie).",
  "Validated French public contracts with buyer SIRET, supplier SIRET, amount, "
  "CPV code and place of execution — pivots into the SIRENE company registry. "
  "Keyless.",
  86400,
  .envelope = "results", .rtype = "procurement-contract", .lang = "fr",
  .title_f = "objet", .title_alt_f = "acheteur_nom",
  /* `id` is the BUYER's contract number, not a record key: across 2,000
   * records (2026-10-02) it took 1,895 values — reused by other buyers, and
   * repeated per supplier and lot of one contract. buyer+id+supplier+amount
   * took 2,000. */
  .key_f = "acheteur_id+id+titulaire_id_1+montant", .date_f = "datenotification",
  .sum1 = "acheteur_nom", .sum2 = "montant", .sum3 = "lieuexecution_nom")

EU_SOURCE(pl_bzp, "pl-bzp-notices",
  "Poland Biuletyn Zamowien Publicznych notices",
  "https://ezamowienia.gov.pl/mo-board/api/v1/Board/Search?SortingColumnName=PublicationDate&SortingDirection=DESC&PageNumber=1&PageSize=10",
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

/* WHAT THIS ROW ASKS FOR, AND WHY IT USED TO GET SOMETHING ELSE.
 * It asked the simple search for `q=type:"ΣΥΜΒΑΣΗ"`. /opendata/search ignores
 * `q` altogether (bb7b3fc measured it for four sibling rows), so every run read
 * the 50 newest acts of the whole Greek state — measured 2026-10-07: total
 * 2,963,247, the first 50 typed Β.1.3 / Β.2.2 / Α.2 / 2.4.7.1, not one
 * ΣΥΜΒΑΣΗ — and stored them as "contracts". The fields it extracted
 * (extraFieldValues.org / expenseAmount / sponsor) are the ones expenditure
 * decisions carry, so even the payload it read was misdescribed; and it read
 * `decisionTypeUid`, which the records do not have (they carry
 * `decisionTypeId`), so the type of what it stored was never recorded either.
 *
 * The decision-type filter the API honours is `type=<uid>`
 * (https://diavgeia.gov.gr/luminapi/opendata/types.json). The one type whose
 * label IS "ΣΥΜΒΑΣΗ" is Γ.3.4, under 2.4.3 (organisational and administrative
 * acts): the contracts a public body signs with people — fixed-term and
 * open-ended private-law employment contracts and contracts for services
 * (σύμβαση έργου). type=Γ.3.4 → total 124,873, every record Γ.3.4;
 * type=ZZ.9 → 0. No other row in the tree reads Γ.3.4 (the procurement award
 * type Δ.1 is eur-diavgeia-anathesi, expenditure Β.2.1 is eur-diavgeia-dapani).
 *
 * What a Γ.3.4 record carries in extraFieldValues (500 records, 2026-10-07):
 * contractType (all 500), documentType (500), financedProject (411),
 * numberOfPeople (390), contractAmount {amount,currency} (129), person[]
 * {name, afm, afmType} (122), duration "dd/mm/yyyy-dd/mm/yyyy" (118),
 * relatedDecisions[] {relatedDecisionsADA}. No buyer name or AFM: the issuing
 * body is organizationId. The detail endpoint (/opendata/decisions/<ada>.json)
 * returns the same object as the search hit, field for field, so there is no
 * second hop to make.
 *
 * Paging: page is 0-based, size is clamped to 500 server-side, and the search
 * spans the last six months of issueDate only (a wider from_issue_date is
 * narrowed back by the server). pw_walk advances `page` while pages come back
 * full and its JO_PAGE_MAX ceiling is disclosed as a truncation notice; the
 * upstream's own total sits at info.total, which pw_walk does not read, so the
 * fetcher below lifts it to the top level where it does. */
#define GR_TYPE_UID   "\xce\x93.3.4"                      /* Γ.3.4 ΣΥΜΒΑΣΗ */
#define GR_URL "https://diavgeia.gov.gr/opendata/search.json" \
               "?type=%CE%93.3.4&size=500&page=0"

/* pw_fetch_fn: the JSON page, with the upstream's info.total copied to the
 * top-level `total` pw_walk reads. The number is the server's own. */
static cJSON *gr_fetch(const source_ctx *ctx, const char *url, void *ud) {
  (void)ud;
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  const cJSON *info = doc ? cJSON_GetObjectItem(doc, "info") : NULL;
  const cJSON *t = info ? cJSON_GetObjectItem(info, "total") : NULL;
  if (cJSON_IsNumber(t) && !cJSON_GetObjectItem(doc, "total"))
    cJSON_AddNumberToObject(doc, "total", t->valuedouble);
  return doc;
}

/* The ADAs already emitted this walk. The search is newest-first, so a
 * decision published while the walk is under way pushes every later record
 * one place down and the record at a page boundary is served twice. The
 * second copy is the same decision (same ADA, the uid) and is skipped rather
 * than emitted onto its own uid. Open addressing over FNV-1a; the table grows
 * at half full. */
typedef struct { char **slot; size_t cap, n; } gr_seen;

static unsigned long gr_hash(const char *s) {
  unsigned long h = 1469598103934665603UL;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    h ^= *p;
    h *= 1099511628211UL;
  }
  return h;
}

/* 1 if `k` was new (and is now recorded), 0 if already seen, -1 on OOM. */
static int gr_seen_add(gr_seen *s, const char *k) {
  if (s->n * 2 >= s->cap) {
    size_t nc = s->cap ? s->cap * 2 : 1024;
    char **ns = calloc(nc, sizeof *ns);
    if (!ns) return -1;
    for (size_t i = 0; i < s->cap; i++) {
      if (!s->slot[i]) continue;
      size_t j = gr_hash(s->slot[i]) & (nc - 1);
      while (ns[j]) j = (j + 1) & (nc - 1);
      ns[j] = s->slot[i];
    }
    free(s->slot);
    s->slot = ns;
    s->cap = nc;
  }
  size_t j = gr_hash(k) & (s->cap - 1);
  while (s->slot[j]) {
    if (!strcmp(s->slot[j], k)) return 0;
    j = (j + 1) & (s->cap - 1);
  }
  s->slot[j] = strdup(k);
  if (!s->slot[j]) return -1;
  s->n++;
  return 1;
}

static void gr_seen_free(gr_seen *s) {
  for (size_t i = 0; i < s->cap; i++) free(s->slot[i]);
  free(s->slot);
}

typedef struct {
  gr_seen seen;
  int pages, first_bad;
  int reserved;     /* re-served copies of an ADA already emitted (skipped) */
  int off_type;     /* records of another decision type (not attributed)    */
  char off_example[64];
} gr_walk;

/* Copy an epoch-ms field as ISO-8601, when the upstream's value renders. */
static void gr_put_ms(cJSON *props, const char *name, const cJSON *d, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(d, k);
  char iso[32];
  if (cJSON_IsNumber(v) && v->valuedouble > 0 && jo_ms_iso(v->valuedouble, iso, sizeof iso))
    cJSON_AddStringToObject(props, name, iso);
}

/* pw_emit_fn: one page of Γ.3.4 decisions. `seen` is what the page HELD. */
static int gr_emit_page(const source_ctx *ctx, intel_sink *sink, const char *id,
                        cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)id;
  gr_walk *w = (gr_walk *)ud;
  const cJSON *arr = cJSON_GetObjectItem(doc, "decisions");
  *seen = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
  if (!cJSON_IsArray(arr)) {
    if (w->pages == 0) w->first_bad = 1;
    w->pages++;
    return 0;
  }
  w->pages++;

  int n = 0, reserved_before = w->reserved;
  const cJSON *d;
  cJSON_ArrayForEach(d, arr) {
    if (!cJSON_IsObject(d)) continue;
    const char *subject = jo_sv(d, "subject");
    const char *ada     = jo_sv(d, "ada");
    const char *proto   = jo_sv(d, "protocolNumber");
    const char *dtype   = jo_sv(d, "decisionTypeId");
    if (!subject && !ada) continue;

    /* Rule 4d, per record: a record of another type is not a ΣΥΜΒΑΣΗ, so it
     * is not stored under this row's label. Counted and disclosed after the
     * walk — if the server ever stops honouring type= again, the run says so
     * instead of filling the row with the national firehose. */
    if (!dtype || strcmp(dtype, GR_TYPE_UID) != 0) {
      if (!w->off_type)
        snprintf(w->off_example, sizeof w->off_example, "%s", dtype ? dtype : "(none)");
      w->off_type++;
      continue;
    }
    if (ada) {
      int fresh = gr_seen_add(&w->seen, ada);
      if (fresh == 0) { w->reserved++; continue; }
    }

    /* issueDate is epoch milliseconds. */
    char iso[32];
    const char *pub = NULL;
    const cJSON *idt = cJSON_GetObjectItem(d, "issueDate");
    if (cJSON_IsNumber(idt) && idt->valuedouble > 0) {
      /* An issueDate the upstream chose that no calendar can render leaves
       * pub NULL, so issue_date is simply absent rather than invented. */
      pub = jo_ms_iso(idt->valuedouble, iso, sizeof iso);
    } else if (jo_sv(d, "issueDate")) {
      snprintf(iso, sizeof iso, "%s", jo_sv(d, "issueDate"));
      pub = iso;
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "gr-diavgeia-decisions");
    cJSON_AddStringToObject(props, "source", "diavgeia.gov.gr");
    if (ada)     cJSON_AddStringToObject(props, "ada", ada);
    if (proto)   cJSON_AddStringToObject(props, "protocol_number", proto);
    if (subject) cJSON_AddStringToObject(props, "subject", subject);
    if (pub)     cJSON_AddStringToObject(props, "issue_date", pub);
    cJSON_AddStringToObject(props, "decision_type_id", dtype);
    cJSON_AddStringToObject(props, "decision_type_label", "ΣΥΜΒΑΣΗ");
    if (jo_sv(d, "organizationId"))
      cJSON_AddStringToObject(props, "organization_id", jo_sv(d, "organizationId"));
    if (jo_sv(d, "documentUrl"))
      cJSON_AddStringToObject(props, "document_url", jo_sv(d, "documentUrl"));
    gr_put_ms(props, "publish_timestamp", d, "publishTimestamp");
    gr_put_ms(props, "submission_timestamp", d, "submissionTimestamp");

    /* The contract itself — what a Γ.3.4 record carries, under readable
     * names. Absent fields stay absent (R1). */
    const char *ctype = NULL, *duration = NULL;
    double amount = 0; const char *currency = NULL; int has_amount = 0;
    char people[400] = "";
    const cJSON *efv = cJSON_GetObjectItem(d, "extraFieldValues");
    if (cJSON_IsObject(efv)) {
      ctype = jo_sv(efv, "contractType");
      if (ctype) cJSON_AddStringToObject(props, "contract_type", ctype);
      const cJSON *np = cJSON_GetObjectItem(efv, "numberOfPeople");
      if (cJSON_IsNumber(np))
        cJSON_AddNumberToObject(props, "number_of_people", np->valuedouble);
      const cJSON *fp = cJSON_GetObjectItem(efv, "financedProject");
      if (cJSON_IsBool(fp))
        cJSON_AddBoolToObject(props, "financed_project", cJSON_IsTrue(fp));
      duration = jo_sv(efv, "duration");
      if (duration) cJSON_AddStringToObject(props, "duration", duration);
      if (jo_sv(efv, "documentType"))
        cJSON_AddStringToObject(props, "document_type", jo_sv(efv, "documentType"));
      const cJSON *amt = cJSON_GetObjectItem(efv, "contractAmount");
      if (cJSON_IsObject(amt)) {
        const cJSON *v = cJSON_GetObjectItem(amt, "amount");
        if (cJSON_IsNumber(v)) {
          amount = v->valuedouble; has_amount = 1;
          cJSON_AddNumberToObject(props, "contract_amount", amount);
        }
        currency = jo_sv(amt, "currency");
        if (currency) cJSON_AddStringToObject(props, "contract_currency", currency);
      }
      /* person[]: the contractor(s) — name and AFM (tax number). Every
       * entry is kept; the summary lists names until it runs out of room. */
      const cJSON *pl = cJSON_GetObjectItem(efv, "person");
      if (cJSON_IsArray(pl)) {
        cJSON *list = cJSON_CreateArray();
        size_t used = 0;
        const cJSON *e;
        cJSON_ArrayForEach(e, pl) {
          const char *nm = jo_sv(e, "name"), *afm = jo_sv(e, "afm");
          if (!nm && !afm) continue;
          cJSON *o = cJSON_CreateObject();
          if (nm)  cJSON_AddStringToObject(o, "name", nm);
          if (afm) cJSON_AddStringToObject(o, "afm", afm);
          if (jo_sv(e, "afmType")) cJSON_AddStringToObject(o, "afm_type", jo_sv(e, "afmType"));
          cJSON_AddItemToArray(list, o);
          if (nm && used < sizeof people - 1) {
            int wr = snprintf(people + used, sizeof people - used, "%s%s",
                              used ? ", " : "", nm);
            if (wr > 0) used += (size_t)wr < sizeof people - used ? (size_t)wr
                                                                  : sizeof people - used - 1;
          }
        }
        cJSON_AddItemToObject(props, "contractors", list);
      }
      const cJSON *rel = cJSON_GetObjectItem(efv, "relatedDecisions");
      if (cJSON_IsArray(rel)) {
        cJSON *list = cJSON_CreateArray();
        const cJSON *e;
        cJSON_ArrayForEach(e, rel) {
          const char *ra = jo_sv(e, "relatedDecisionsADA");
          if (ra) cJSON_AddItemToArray(list, cJSON_CreateString(ra));
        }
        cJSON_AddItemToObject(props, "related_decision_adas", list);
      }
    }
    /* ...and every field the record carried, verbatim (house rule 2): signer,
     * unit and thematic-category ids, version and status, attachments,
     * checksum, the whole extraFieldValues object. The names above are a view.
     * JSON nulls are skipped. */
    for (const cJSON *f = d->child; f; f = f->next) {
      if (!f->string || cJSON_IsNull(f) || cJSON_GetObjectItem(props, f->string)) continue;
      cJSON_AddItemToObject(props, f->string, cJSON_Duplicate(f, 1));
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[500];
    snprintf(title, sizeof title, "%s", subject ? subject : ada);

    char amt_s[64] = "";
    if (has_amount)
      snprintf(amt_s, sizeof amt_s, "%.2f %s", amount, currency ? currency : "");
    char summary[640];
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             ctype ? ctype : "",
             (ctype && amt_s[0]) ? " · " : "", amt_s,
             ((ctype || amt_s[0]) && people[0]) ? " · " : "", people,
             ((ctype || amt_s[0] || people[0]) && duration) ? " · " : "",
             duration ? duration : "");

    char link[256];
    if (ada) snprintf(link, sizeof link, "https://diavgeia.gov.gr/decision/view/%s", ada);
    else     snprintf(link, sizeof link, "%s", GR_URL);

    intel_item it = {0};
    it.remote_key      = ada ? ada : (proto ? proto : title);
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = link;
    it.lang            = "el";
    it.published_at    = pub;
    it.record_type     = "gr-contract-decision";
    it.properties_json = pj;
    it.tags_json       = "[\"contracts\",\"greece\",\"transparency\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  /* A re-served copy is a record this walk already used, not one it
   * discarded: counting it as used keeps pw_walk from reporting it as
   * "dropped" (its records_dropped means records it could not turn into
   * rows). The run line's records=/stored= count emit() calls and are not
   * affected. Off-type records stay dropped — they were. */
  return n + (w->reserved - reserved_before);
}

static int gr_run(const source_ctx *ctx, intel_sink *sink) {
  gr_walk w;
  memset(&w, 0, sizeof w);
  int n = pw_walk(ctx, sink, "gr-diavgeia-decisions", GR_URL, gr_fetch,
                  gr_emit_page, &w);
  gr_seen_free(&w.seen);
  if (n < 0) { fprintf(stderr, "[gr-diavgeia-decisions] fetch/parse failed\n"); return -1; }
  if (w.first_bad) {
    fprintf(stderr, "[gr-diavgeia-decisions] unexpected payload shape\n");
    return -1;
  }
  if (w.off_type > 0) {
    char reason[320];
    snprintf(reason, sizeof reason,
             "the upstream answered type=Γ.3.4 with %d record(s) of another "
             "decision type (e.g. %s); they are not ΣΥΜΒΑΣΗ decisions and were "
             "not stored under this row", w.off_type, w.off_example);
    cJSON *extra = cJSON_CreateObject();
    if (extra) {
      cJSON_AddStringToObject(extra, "url", GR_URL);
      cJSON_AddNumberToObject(extra, "records_off_type", w.off_type);
    }
    jo_truncation_notice_ex(sink, "gr-diavgeia-decisions", "off-type",
                            n - w.reserved, -1,
                            reason, "check whether /opendata/search still honours "
                            "type= (rule 4d)", extra);
  }
  fprintf(stderr, "[gr-diavgeia-decisions] emitted %d Γ.3.4 decisions across %d "
          "page(s) (%d re-served ADAs skipped, %d off-type)\n",
          n - w.reserved, w.pages, w.reserved, w.off_type);
  return 0;
}

static const source_def reg2_gr_diavgeia_def = {
  .id = "gr-diavgeia-decisions", .collector = "government",
  .name = "Greece Diavgeia contract decisions (ΣΥΜΒΑΣΗ, type Γ.3.4)",
  .update_interval_sec = 10800, .run = gr_run,
  .category = "government", .type = "api",
  .url = "https://diavgeia.gov.gr/opendata/search.json?type=%CE%93.3.4",
  .description = "Every ΣΥΜΒΑΣΗ (decision type Γ.3.4) a Greek public body "
                 "published to the statutory Diavgeia transparency programme "
                 "in the last six months: fixed-term and open-ended "
                 "private-law employment contracts and contracts for services, "
                 "with contract type, number of people, contractor name and "
                 "AFM, amount and duration where the record states them, the "
                 "issuing organisation id and the related decisions. Keyless.",
  .license = "Statutory transparency programme (Law 3861/2010); the opendata "
             "endpoint is the official machine interface.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_gr_diavgeia_def)
