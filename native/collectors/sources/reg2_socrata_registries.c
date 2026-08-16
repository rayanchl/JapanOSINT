/* collectors/sources/reg2_socrata_registries.c
 *
 * Eleven Socrata (SoQL) open-data business / procurement registers, all
 * keyless, all fetched live. One generic sweep engine drives them because the
 * payload shape is identical: a bare JSON array of flat row objects.
 *
 * Endpoints (exactly as probed):
 *   us-co-business-entities        https://data.colorado.gov/resource/4ykn-tg5h.json?$limit=200&$order=entityformdate%20DESC
 *   us-tx-franchise-taxpayers      https://data.texas.gov/resource/9cir-efmm.json?$limit=200
 *   us-ct-business-registry        https://data.ct.gov/resource/n7gp-d28j.json?$limit=200&$order=create_dt%20DESC
 *   us-nyc-business-licenses       https://data.cityofnewyork.us/resource/w7w3-xahh.json?$limit=200
 *   us-nyc-procurement-notices     https://data.cityofnewyork.us/resource/qyyg-4tf5.json?$limit=200&$order=start_date%20DESC
 *   us-chicago-contracts           https://data.cityofchicago.org/resource/rsxa-ify5.json?$limit=200&$order=approval_date%20DESC
 *   us-chicago-business-licenses   https://data.cityofchicago.org/resource/r5kz-chrr.json?$limit=200
 *   us-la-business-registry        https://data.lacity.org/resource/6rrh-rzua.json?$limit=200
 *   us-sf-business-locations       https://data.sfgov.org/resource/g8m3-pdis.json?$limit=200
 *   us-seattle-business-licenses   https://data.seattle.gov/resource/wnbq-64tb.json?$limit=200
 *   co-secop-contracts             https://www.datos.gov.co/resource/jbjy-vk9h.json?$limit=200&$order=fecha_de_firma%20DESC
 *
 * Emitted per row: title (legal/trade name, contract or notice title), a
 * remote_key taken from the dataset's own record id, published_at from the
 * dataset's own date column, and EVERY scalar field the row actually carried
 * copied verbatim into properties_json (Socrata's internal ":"-prefixed and
 * ":@computed_region_*" columns are dropped). Nothing is synthesized.
 *
 * Geo (R2): only for the four datasets that publish coordinates, and only from
 * the row's own values — NYC/Chicago string latitude/longitude columns, LA's
 * nested location_1 {latitude,longitude}, SF's GeoJSON `location` Point
 * ([lon,lat]). Rows without usable coordinates are emitted with no geo. No
 * address is ever geocoded.
 *
 * Keyless (no app token; Socrata throttles anonymous callers but serves them).
 * Licence: each publisher's open-data / public-record terms, stated per source
 * in .license below.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

typedef struct {
  const char *service;      /* source id, stamped into properties          */
  const char *url;
  const char *rtype;        /* record_type                                  */
  const char *title_f;      /* primary title column                         */
  const char *title_alt_f;  /* fallback title column (may be NULL)          */
  const char *key_f;        /* natural record id column (may be NULL)       */
  const char *date_f;       /* date column (may be NULL)                    */
  int   date_compact;       /* 1 => value is bare YYYYMMDD                  */
  int   geo_kind;           /* 0 none | 1 flat cols | 2 nested obj | 3 Point*/
  const char *geo_a;        /* kind 1: lat column; kind 2/3: object name    */
  const char *geo_b;        /* kind 1: lon column                           */
  const char *sum1, *sum2, *sum3;   /* columns joined into the summary line */
} soc_src;

/* Field as text: strings verbatim, numbers rendered into `buf`. NULL when the
 * column is absent or empty — we never invent a placeholder. */
static const char *soc_field(const cJSON *row, const char *k,
                             char *buf, size_t n) {
  if (!k) return NULL;
  const cJSON *v = cJSON_GetObjectItem(row, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) return v->valuestring;
  if (cJSON_IsNumber(v)) { snprintf(buf, n, "%.10g", v->valuedouble); return buf; }
  return NULL;
}

/* Numeric field, accepting Socrata's habit of shipping numbers as strings. */
static int soc_num(const cJSON *o, const char *k, double *out) {
  if (!o || !k) return 0;
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *end = NULL;
    double d = strtod(v->valuestring, &end);
    if (end && end != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

static int soc_run(const soc_src *s, const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, s->url, 30000);
  if (!doc) { fprintf(stderr, "[%s] fetch/parse failed\n", s->service); return -1; }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[%s] unexpected payload (not an array)\n", s->service);
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  const cJSON *row;
  cJSON_ArrayForEach(row, doc) {
    if (!cJSON_IsObject(row)) continue;

    char tbuf[64];
    const char *raw = soc_field(row, s->title_f, tbuf, sizeof tbuf);
    if (!raw) raw = soc_field(row, s->title_alt_f, tbuf, sizeof tbuf);
    if (!raw) continue;                       /* no real title -> no row (R1) */
    char title[400];
    snprintf(title, sizeof title, "%s", raw);

    char kbuf[64];
    const char *key = soc_field(row, s->key_f, kbuf, sizeof kbuf);
    char hashed[21];
    if (!key) {
      const char *parts[3] = { s->service, title, NULL };
      feed_hash_key(hashed, parts, 2);
      key = hashed;
    }

    /* published_at strictly from the dataset's own date column. */
    char dbuf[64], iso[64];
    const char *pub = NULL;
    const char *dv = soc_field(row, s->date_f, dbuf, sizeof dbuf);
    if (dv) {
      if (s->date_compact && strlen(dv) == 8) {
        snprintf(iso, sizeof iso, "%.4s-%.2s-%.2s", dv, dv + 4, dv + 6);
      } else {
        snprintf(iso, sizeof iso, "%s", dv);
        char *sp = strchr(iso, ' ');
        if (sp) *sp = 'T';
      }
      pub = iso;
    }

    /* geo ONLY from coordinates the row itself carried (R2) */
    double la = 0, lo = 0;
    int hasgeo = 0;
    if (s->geo_kind == 1) {
      hasgeo = soc_num(row, s->geo_a, &la) && soc_num(row, s->geo_b, &lo);
    } else if (s->geo_kind == 2) {
      const cJSON *o = cJSON_GetObjectItem(row, s->geo_a);
      if (cJSON_IsObject(o))
        hasgeo = soc_num(o, "latitude", &la) && soc_num(o, "longitude", &lo);
    } else if (s->geo_kind == 3) {
      const cJSON *o = cJSON_GetObjectItem(row, s->geo_a);
      const cJSON *c = o ? cJSON_GetObjectItem(o, "coordinates") : NULL;
      if (cJSON_IsArray(c) && cJSON_GetArraySize(c) >= 2) {
        const cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
        if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
          lo = x->valuedouble; la = y->valuedouble; hasgeo = 1;
        }
      }
    }
    if (hasgeo && (la == 0.0 || lo == 0.0)) hasgeo = 0;   /* 0/0 placeholder */
    if (hasgeo && (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0))
      hasgeo = 0;

    /* properties: every scalar column this row actually carried */
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", s->service);
    cJSON_AddStringToObject(props, "dataset_url", s->url);
    for (const cJSON *f = row->child; f; f = f->next) {
      if (!f->string || f->string[0] == ':') continue;   /* Socrata internals */
      if (cJSON_IsString(f) && f->valuestring && f->valuestring[0])
        cJSON_AddStringToObject(props, f->string, f->valuestring);
      else if (cJSON_IsNumber(f))
        cJSON_AddNumberToObject(props, f->string, f->valuedouble);
      else if (cJSON_IsBool(f))
        cJSON_AddBoolToObject(props, f->string, cJSON_IsTrue(f) ? 1 : 0);
    }
    if (hasgeo) {
      cJSON_AddNumberToObject(props, "lat", la);
      cJSON_AddNumberToObject(props, "lon", lo);
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    /* summary from up to three real columns */
    char sbuf1[96], sbuf2[96], sbuf3[96], summary[320];
    const char *a = soc_field(row, s->sum1, sbuf1, sizeof sbuf1);
    const char *b = soc_field(row, s->sum2, sbuf2, sizeof sbuf2);
    const char *c = soc_field(row, s->sum3, sbuf3, sizeof sbuf3);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             a ? a : "", (a && b) ? " \xc2\xb7 " : "", b ? b : "",
             ((a || b) && c) ? " \xc2\xb7 " : "", c ? c : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = s->url;
    it.published_at    = pub;
    it.record_type     = s->rtype;
    it.has_geo         = hasgeo;
    it.lat             = la;
    it.lon             = lo;
    it.properties_json = pj;
    it.tags_json       = "[\"registry\",\"open-data\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", s->service, n);
  return 0;                                   /* fetched fine (R3) */
}

#define SOC_SOURCE(sym, ID, NAME, URL, COLL, CAT, LIC, DESC, IVAL, ...)      \
  static const soc_src sym##_cfg = { .service = ID, .url = URL, __VA_ARGS__ };\
  static int sym##_run(const source_ctx *ctx, intel_sink *sink) {             \
    return soc_run(&sym##_cfg, ctx, sink);                                    \
  }                                                                           \
  static const source_def sym##_def = {                                       \
    .id = ID, .collector = COLL, .name = NAME,                                \
    .update_interval_sec = IVAL, .run = sym##_run,                            \
    .category = CAT, .type = "dataset", .url = URL,                           \
    .description = DESC, .license = LIC, .layer = NULL, .free_tier = 1,       \
  };                                                                          \
  REGISTER_SOURCE(sym##_def)

/* --- Colorado Secretary of State business entities ---------------------- */
SOC_SOURCE(co_biz, "us-co-business-entities",
  "Colorado Secretary of State business entities",
  "https://data.colorado.gov/resource/4ykn-tg5h.json?$limit=200&$order=entityformdate%20DESC",
  "economy", "economy", "Colorado open data, public record.",
  "Colorado SoS corporate register — entity name, id, status, type, principal "
  "and mailing address, registered agent name and address.",
  86400,
  .rtype = "us-company", .title_f = "entityname", .key_f = "entityid",
  .date_f = "entityformdate",
  .sum1 = "entitystatus", .sum2 = "entitytype", .sum3 = "jurisdictonofformation")

/* --- Texas franchise-tax registered entities ---------------------------- */
SOC_SOURCE(tx_fran, "us-tx-franchise-taxpayers",
  "Texas active franchise-tax registered entities",
  "https://data.texas.gov/resource/9cir-efmm.json?$limit=200",
  "economy", "economy", "Texas open data, public record.",
  "Texas Comptroller register of entities holding a franchise-tax account, "
  "carrying the Secretary of State file number and right-to-transact-business "
  "status (A=active, N=forfeited).",
  86400,
  .rtype = "us-company", .title_f = "taxpayer_name", .key_f = "taxpayer_number",
  .date_f = "sos_charter_date",
  .sum1 = "taxpayer_city", .sum2 = "right_to_transact_business_code",
  .sum3 = "taxpayer_organizational_type")

/* --- Connecticut business registry -------------------------------------- */
SOC_SOURCE(ct_biz, "us-ct-business-registry",
  "Connecticut business registry",
  "https://data.ct.gov/resource/n7gp-d28j.json?$limit=200&$order=create_dt%20DESC",
  "economy", "economy", "Connecticut open data, public record.",
  "Connecticut SoS business register including ownership-designation flags "
  "(woman/veteran/minority owned) and registration status; ordered newest-first "
  "so it reads as a new-registrations feed.",
  86400,
  .rtype = "us-company", .title_f = "name", .key_f = "accountnumber",
  .date_f = "create_dt",
  .sum1 = "status", .sum2 = "mailing_address", .sum3 = "date_registration")

/* --- NYC DCWP legally operating businesses ------------------------------ */
SOC_SOURCE(nyc_lic, "us-nyc-business-licenses",
  "NYC DCWP legally operating businesses",
  "https://data.cityofnewyork.us/resource/w7w3-xahh.json?$limit=200",
  "economy", "economy", "NYC Open Data, public record.",
  "Every business licensed to operate in New York City — legal name, DBA, "
  "licence category and status, with per-premises coordinates where the row "
  "carries them.",
  86400,
  .rtype = "us-business-licence", .title_f = "business_name",
  .title_alt_f = "dba_trade_name", .key_f = "license_nbr",
  .geo_kind = 1, .geo_a = "latitude", .geo_b = "longitude",
  .sum1 = "business_category", .sum2 = "license_status", .sum3 = "address_street_name")

/* --- NYC City Record procurement notices -------------------------------- */
SOC_SOURCE(nyc_proc, "us-nyc-procurement-notices",
  "NYC City Record procurement notices and awards",
  "https://data.cityofnewyork.us/resource/qyyg-4tf5.json?$limit=200&$order=start_date%20DESC",
  "government", "government", "NYC Open Data, public record.",
  "NYC City Record procurement section — solicitations and awards with agency, "
  "PIN, award amount, winning vendor and responsible contact officer "
  "(type_of_notice_description=\"Award\" isolates awards).",
  21600,
  .rtype = "procurement-notice", .title_f = "short_title",
  .title_alt_f = "agency_name", .key_f = "request_id", .date_f = "start_date",
  .sum1 = "agency_name", .sum2 = "type_of_notice_description", .sum3 = "vendor_name")

/* --- City of Chicago contracts ------------------------------------------ */
SOC_SOURCE(chi_con, "us-chicago-contracts",
  "City of Chicago contracts",
  "https://data.cityofchicago.org/resource/rsxa-ify5.json?$limit=200&$order=approval_date%20DESC",
  "government", "government", "City of Chicago open data, public record.",
  "Every City of Chicago purchase order and contract with vendor name, stable "
  "vendor id, mailing address, department and award amount.",
  43200,
  .rtype = "procurement-contract", .title_f = "purchase_order_description",
  .title_alt_f = "vendor_name", .key_f = "purchase_order_contract_number",
  .date_f = "approval_date",
  .sum1 = "vendor_name", .sum2 = "department", .sum3 = "award_amount")

/* --- City of Chicago business licences ---------------------------------- */
SOC_SOURCE(chi_lic, "us-chicago-business-licenses",
  "City of Chicago business licenses",
  "https://data.cityofchicago.org/resource/r5kz-chrr.json?$limit=200",
  "economy", "economy", "City of Chicago open data, public record.",
  "Chicago business licences with legal entity name, DBA, premises address, "
  "licence class and business activity — a usable corporate-alias source.",
  86400,
  .rtype = "us-business-licence", .title_f = "legal_name",
  .title_alt_f = "doing_business_as_name", .key_f = "license_number",
  .date_f = "license_start_date",
  .geo_kind = 1, .geo_a = "latitude", .geo_b = "longitude",
  .sum1 = "doing_business_as_name", .sum2 = "license_description", .sum3 = "address")

/* --- Los Angeles active business tax registry --------------------------- */
SOC_SOURCE(la_biz, "us-la-business-registry",
  "Los Angeles active business tax registry",
  "https://data.lacity.org/resource/6rrh-rzua.json?$limit=200",
  "economy", "economy", "City of Los Angeles open data, public record.",
  "Every business holding an active LA city tax registration certificate with "
  "NAICS activity, premises address and — where the row carries location_1 — "
  "real coordinates.",
  86400,
  .rtype = "us-business-licence", .title_f = "business_name",
  .key_f = "location_account", .date_f = "location_start_date",
  .geo_kind = 2, .geo_a = "location_1",
  .sum1 = "primary_naics_description", .sum2 = "street_address", .sum3 = "council_district")

/* --- San Francisco registered business locations ------------------------ */
SOC_SOURCE(sf_biz, "us-sf-business-locations",
  "San Francisco registered business locations",
  "https://data.sfgov.org/resource/g8m3-pdis.json?$limit=200",
  "economy", "economy", "SF Open Data, public record.",
  "SF Treasurer register of every business location, linking the registered "
  "ownership name to the trading (DBA) name, address and GeoJSON point.",
  86400,
  .rtype = "us-business-licence", .title_f = "dba_name",
  .title_alt_f = "ownership_name", .key_f = "uniqueid", .date_f = "dba_start_date",
  .geo_kind = 3, .geo_a = "location",
  .sum1 = "ownership_name", .sum2 = "full_business_address", .sum3 = "city")

/* --- Seattle business licence tax certificates -------------------------- */
SOC_SOURCE(sea_lic, "us-seattle-business-licenses",
  "Seattle active business licence tax certificates",
  "https://data.seattle.gov/resource/wnbq-64tb.json?$limit=200",
  "economy", "economy", "Seattle open data, public record.",
  "Seattle business licence tax certificates — legal name, trade name, "
  "ownership type and NAICS activity. No coordinates in this dataset, so rows "
  "are emitted without geo.",
  86400,
  .rtype = "us-business-licence", .title_f = "business_legal_name",
  .title_alt_f = "trade_name", .key_f = "city_account_number",
  .date_f = "license_start_date", .date_compact = 1,
  .sum1 = "trade_name", .sum2 = "naics_description", .sum3 = "ownership_type")

/* --- Colombia SECOP II electronic contracts ----------------------------- */
SOC_SOURCE(co_secop, "co-secop-contracts",
  "Colombia SECOP II electronic contracts",
  "https://www.datos.gov.co/resource/jbjy-vk9h.json?$limit=200&$order=fecha_de_firma%20DESC",
  "government", "government",
  "Colombia datos abiertos, public record (Ley 1712 transparency law).",
  "Every contract executed through Colombia's SECOP II platform with buying "
  "entity NIT, contractor, value, modality and status.",
  21600,
  .rtype = "procurement-contract", .title_f = "descripcion_del_proceso",
  .title_alt_f = "nombre_entidad", .key_f = "id_contrato",
  .date_f = "fecha_de_firma",
  .sum1 = "nombre_entidad", .sum2 = "estado_contrato", .sum3 = "ciudad")
