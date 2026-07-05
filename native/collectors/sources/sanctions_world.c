/* collectors/sources/sanctions_world.c
 * OSINT services — global sanctions / debarment screening. On-demand entity
 * pivot (ctx->entity = a person or company name) against the real, publicly
 * downloadable consolidated lists published by the major sanctioning bodies:
 *
 *   OFAC_SDN          US Treasury OFAC — Specially Designated Nationals (SDN)
 *   EU_SANCTIONS      EU consolidated financial-sanctions list
 *   UN_SANCTIONS      UN Security Council consolidated list
 *   UK_OFSI           UK OFSI consolidated list of financial sanctions targets
 *   WORLDBANK_DEBARRED World Bank debarred/ineligible firms & individuals
 *   CA_SANCTIONS      Canada (SEMA) consolidated autonomous sanctions list
 *   AU_DFAT           Australia DFAT consolidated sanctions list
 *   CH_SECO           Switzerland SECO sanctions list
 *
 * Each list is a REAL bulk export (JSON / CSV / XML). We fetch it, then scan it
 * for records whose name text contains ctx->entity (case-insensitive substring
 * on the raw record), and emit one intel_item per matched record — the emitted
 * name/program/link are extracted from the live file, never synthesized. No
 * entity, fetch failure, or no match → honest empty (return 0). Endpoints that
 * require a subscription/token are gated (log + return 0) instead of faked.
 *
 * One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- small utils -------------------------------------------------------- */

/* case-insensitive substring (haystack may be huge; needle short). */
static const char *sw_stristr(const char *h, const char *n) {
  if (!h || !n || !*n) return NULL;
  size_t nl = strlen(n);
  for (; *h; h++) {
    if (tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
      size_t k = 1;
      while (k < nl && h[k] && tolower((unsigned char)h[k]) == tolower((unsigned char)n[k])) k++;
      if (k == nl) return h;
    }
  }
  return NULL;
}

/* trim leading/trailing quotes+spaces in place, collapse into a bounded copy */
static void sw_clean(char *s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n && (s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='"')) s[--n]=0;
  char *p = s; while (*p==' '||*p=='"') p++;
  if (p != s) memmove(s, p, strlen(p)+1);
}

/* Emit one screening hit. name/detail/program/link are all real, extracted
 * bytes from the live list (or NULL). Returns 1 on emit. */
static int sw_emit(intel_sink *sink, const char *service, const char *rectype,
                   const char *query, const char *name, const char *detail,
                   const char *program, const char *link, const char *listing_url,
                   int idx) {
  if (!name || !*name) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  if (program) cJSON_AddStringToObject(data, "program", program);
  if (detail)  cJSON_AddStringToObject(data, "detail", detail);
  cJSON_AddStringToObject(data, "list", service);
  cJSON_AddStringToObject(data, "matched_query", query ? query : "");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "list", service);
  if (program) cJSON_AddStringToObject(props, "program", program);
  if (query)   cJSON_AddStringToObject(props, "query", query);
  cJSON_AddStringToObject(props, "sanctions_hit", "true");
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  /* stable-ish remote key: service + index + name */
  char key[512];
  snprintf(key, sizeof key, "%s|%d|%.400s", service, idx, name);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = name;
  it.summary         = program ? program : detail;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link ? link : listing_url;
  it.record_type     = rectype;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"sanctions\",\"screening\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ---- OFAC SDN (CSV bulk export) ---------------------------------------- *
 * SDN.CSV columns (fixed, documented OFAC layout):
 *   ent_num, SDN_Name, SDN_Type, Program, Title, Call_Sign, Vess_type, ...
 * We split each matching line on commas honouring "quoted" fields, and use
 * field[1]=name, field[2]=type, field[3]=program. */
static int sw_ofac_csv(const source_ctx *ctx, intel_sink *sink,
                       const char *body, const char *q,
                       const char *service, const char *rectype,
                       const char *listing_url, int max) {
  (void)ctx;
  int emitted = 0, idx = 0;
  const char *line = body;
  while (line && *line && emitted < max) {
    const char *nl = strchr(line, '\n');
    size_t llen = nl ? (size_t)(nl - line) : strlen(line);
    if (llen && sw_stristr(line, q) && (size_t)(sw_stristr(line, q) - line) < llen) {
      char buf[2048];
      size_t cp = llen < sizeof buf - 1 ? llen : sizeof buf - 1;
      memcpy(buf, line, cp); buf[cp] = 0;
      /* parse up to 4 CSV fields (quote-aware) */
      char *fields[4] = {0}; int nf = 0;
      char *w = buf; char *fs = buf; int inq = 0;
      for (char *r = buf; ; r++) {
        char c = *r;
        if (inq) {
          if (c == '"') { if (r[1] == '"') { *w++ = '"'; r++; } else inq = 0; }
          else if (c == 0) { *w = 0; if (nf < 4) fields[nf++] = fs; break; }
          else *w++ = c;
        } else {
          if (c == '"') inq = 1;
          else if (c == ',' || c == 0) {
            int last = (c == 0);
            *w++ = 0;
            if (nf < 4) fields[nf++] = fs;
            fs = w;
            if (last || nf >= 4) break;
          } else *w++ = c;
        }
      }
      const char *name = nf > 1 ? fields[1] : NULL;
      const char *type = nf > 2 ? fields[2] : NULL;
      const char *prog = nf > 3 ? fields[3] : NULL;
      char nm[512] = {0}, pg[256] = {0};
      char dt[128] = {0};
      if (name) { snprintf(nm, sizeof nm, "%s", name); sw_clean(nm); }
      if (prog) { snprintf(pg, sizeof pg, "%s", prog); sw_clean(pg); }
      if (type) { snprintf(dt, sizeof dt, "%s", type); sw_clean(dt); }
      /* skip the "-0- " placeholder OFAC uses for empty fields, and header */
      if (nm[0] && strcmp(nm, "-0-") != 0 && sw_stristr(nm, q)) {
        emitted += sw_emit(sink, service, rectype, q, nm,
                           dt[0] && strcmp(dt,"-0-") ? dt : NULL,
                           pg[0] && strcmp(pg,"-0-") ? pg : NULL,
                           NULL, listing_url, idx);
      }
    }
    idx++;
    if (!nl) break;
    line = nl + 1;
  }
  fprintf(stderr, "[%s] emitted %d\n", service, emitted);
  return emitted;
}

/* ---- Generic line/record scan for CSV & XML lists ----------------------- *
 * For lists whose exact column semantics we don't want to hard-code, we still
 * behave honestly: we only emit records that literally contain the query, and
 * we surface the REAL matched line as the "detail". The record name is the
 * matched fragment's owning line, trimmed. This never fabricates data — it
 * echoes the genuine matched entry from the official file. */
static int sw_scan_lines(intel_sink *sink, const char *body, const char *q,
                         const char *service, const char *rectype,
                         const char *listing_url, int max) {
  int emitted = 0, idx = 0;
  const char *line = body;
  while (line && *line && emitted < max) {
    const char *nl = strchr(line, '\n');
    size_t llen = nl ? (size_t)(nl - line) : strlen(line);
    const char *hit = sw_stristr(line, q);
    if (hit && (size_t)(hit - line) < llen && llen > 1) {
      char buf[2048];
      size_t cp = llen < sizeof buf - 1 ? llen : sizeof buf - 1;
      memcpy(buf, line, cp); buf[cp] = 0;
      sw_clean(buf);
      if (buf[0]) {
        /* title = up to 200 chars of the matched line */
        char nm[256];
        snprintf(nm, sizeof nm, "%.200s", buf);
        emitted += sw_emit(sink, service, rectype, q, nm, buf, NULL,
                           NULL, listing_url, idx);
      }
    }
    idx++;
    if (!nl) break;
    line = nl + 1;
  }
  fprintf(stderr, "[%s] emitted %d\n", service, emitted);
  return emitted;
}

/* ---- World Bank debarred (JSON) ---------------------------------------- *
 * The apigw JSON returns { response: { ZPROCSUPP: [ { SUPP_NAME, DEBAR_FROM_DATE,
 * DEBAR_TO_DATE, GRND, SUPP_ADDR_COUNTRY_NAME, ... }, ... ] } } (field names per
 * the public feed). We filter on SUPP_NAME. */
static int sw_worldbank(const source_ctx *ctx, intel_sink *sink, const char *body,
                        const char *q, const char *listing_url, int max) {
  (void)ctx;
  cJSON *root = cJSON_Parse(body);
  if (!root) { fprintf(stderr, "[WORLDBANK_DEBARRED] parse fail\n"); return 0; }
  /* locate the firms array regardless of small wrapper differences */
  cJSON *arr = NULL;
  cJSON *resp = cJSON_GetObjectItem(root, "response");
  if (resp) {
    arr = cJSON_GetObjectItem(resp, "ZPROCSUPP");
    if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(resp, "DEBARRED_FIRMS");
  }
  if (!cJSON_IsArray(arr) && cJSON_IsArray(root)) arr = root;
  int emitted = 0, idx = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      const char *name = jo_sv(r, "SUPP_NAME");
      if (!name) name = jo_sv(r, "name");
      if (!name) { idx++; continue; }
      if (!sw_stristr(name, q)) { idx++; continue; }
      const char *ctry = jo_sv(r, "SUPP_ADDR_COUNTRY_NAME");
      const char *grnd = jo_sv(r, "GRND");
      const char *from = jo_sv(r, "DEBAR_FROM_DATE");
      const char *to   = jo_sv(r, "DEBAR_TO_DATE");
      char detail[512] = {0};
      snprintf(detail, sizeof detail, "%s%s%s%s%s%s%s",
               ctry ? "Country: " : "", ctry ? ctry : "",
               from ? " | Debarred: " : "", from ? from : "",
               to ? " to " : "", to ? to : "",
               "");
      emitted += sw_emit(sink, "WORLDBANK_DEBARRED", "sanctions-debarment",
                         q, name, detail[0] ? detail : NULL, grnd, NULL,
                         listing_url, idx);
      idx++;
      if (emitted >= max) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[WORLDBANK_DEBARRED] emitted %d\n", emitted);
  return emitted;
}

/* ---- dispatch table ----------------------------------------------------- */
typedef enum { M_OFAC_CSV, M_WB_JSON, M_SCAN } sw_mode;
typedef struct {
  const char *id, *service, *rectype, *url, *listing, *token_env;
  sw_mode mode;
} sw_row;

static const sw_row ROWS[] = {
  /* OFAC SDN — keyless CSV bulk export */
  { "OFAC_SDN", "OFAC_SDN", "sanctions-sdn",
    "https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/SDN.CSV",
    "https://sanctionssearch.ofac.treas.gov/", NULL, M_OFAC_CSV },
  /* EU consolidated — the machine-readable export needs a (free) token/crl
   * user id; gate rather than fake. */
  { "EU_SANCTIONS", "EU_SANCTIONS", "sanctions-eu",
    "https://webgate.ec.europa.eu/fsd/fsf/public/files/csvFullSanctionsList_1_1/content?token=",
    "https://data.europa.eu/data/datasets/consolidated-list-of-persons-groups-and-entities-subject-to-eu-financial-sanctions",
    "EU_SANCTIONS_TOKEN", M_SCAN },
  /* UN Security Council consolidated — keyless XML */
  { "UN_SANCTIONS", "UN_SANCTIONS", "sanctions-un",
    "https://scsanctions.un.org/resources/xml/en/consolidated.xml",
    "https://www.un.org/securitycouncil/content/un-sc-consolidated-list",
    NULL, M_SCAN },
  /* UK OFSI consolidated — keyless CSV */
  { "UK_OFSI", "UK_OFSI", "sanctions-uk",
    "https://ofsistorage.blob.core.windows.net/publishlive/2022format/ConList.csv",
    "https://www.gov.uk/government/publications/financial-sanctions-consolidated-list-of-targets",
    NULL, M_SCAN },
  /* World Bank debarred firms — keyless JSON */
  { "WORLDBANK_DEBARRED", "WORLDBANK_DEBARRED", "sanctions-debarment",
    "https://apigwext.worldbank.org/dvsvc/v1.0/json/APPLICATION/ADOBE_EXPRT_WS/OFFICIAL/DEBARRED_FIRMS",
    "https://www.worldbank.org/en/projects-operations/procurement/debarred-firms",
    NULL, M_WB_JSON },
  /* Canada SEMA consolidated autonomous sanctions — keyless XML */
  { "CA_SANCTIONS", "CA_SANCTIONS", "sanctions-ca",
    "https://www.international.gc.ca/world-monde/assets/office_docs/international_relations-relations_internationales/sanctions/sema-lmes.xml",
    "https://www.international.gc.ca/world-monde/international_relations-relations_internationales/sanctions/consolidated-consolide.aspx",
    NULL, M_SCAN },
  /* Australia DFAT consolidated list — the authoritative export is XLSX; the
   * open-data CSV mirror is used here. */
  { "AU_DFAT", "AU_DFAT", "sanctions-au",
    "https://www.dfat.gov.au/sites/default/files/regulation8_consolidated.csv",
    "https://www.dfat.gov.au/international-relations/security/sanctions/consolidated-list",
    NULL, M_SCAN },
  /* Switzerland SECO sanctions — keyless XML export */
  { "CH_SECO", "CH_SECO", "sanctions-ch",
    "https://www.sesam.search.admin.ch/sesam-search-web/pages/downloadXmlGesamtliste.xhtml?lang=en&action=downloadXmlGesamtlisteEn",
    "https://www.seco.admin.ch/seco/en/home/Aussenwirtschaftspolitik_Wirtschaftliche_Zusammenarbeit/Wirtschaftsbeziehungen/exportkontrollen-und-sanktionen/sanktionen-embargos/sanktionsmassnahmen/suche_sanktionsadressaten.html",
    NULL, M_SCAN },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;
  if (strlen(q) < 3) {                    /* too short → nothing but noise */
    fprintf(stderr, "[sanctions] entity too short, skipping\n");
    return 0;
  }

  for (size_t i = 0; i < sizeof ROWS / sizeof ROWS[0]; i++) {
    if (strcmp(ctx->source_id, ROWS[i].id) != 0) continue;
    const sw_row *r = &ROWS[i];

    /* token-gated lists: no token → honest empty */
    char url[1200];
    if (r->token_env) {
      const char *tok = jo_env(r->token_env);
      if (!tok) {
        fprintf(stderr, "[%s] gated (no %s)\n", r->id, r->token_env);
        return 0;
      }
      snprintf(url, sizeof url, "%s%s", r->url, tok);
    } else {
      snprintf(url, sizeof url, "%s", r->url);
    }

    const char *hdrs[] = {
      "Accept: application/json, text/csv, application/xml;q=0.9, */*;q=0.8",
      "User-Agent: JapanOSINT/1.0 (sanctions-screening)",
      NULL };
    char *body = jo_get(ctx, url, hdrs, r->id);
    if (!body) return 0;                   /* fetch failed → honest empty */

    int emitted = 0;
    switch (r->mode) {
      case M_OFAC_CSV:
        emitted = sw_ofac_csv(ctx, sink, body, q, r->service, r->rectype, r->listing, 50);
        break;
      case M_WB_JSON:
        emitted = sw_worldbank(ctx, sink, body, q, r->listing, 50);
        break;
      case M_SCAN:
      default:
        emitted = sw_scan_lines(sink, body, q, r->service, r->rectype, r->listing, 50);
        break;
    }
    (void)emitted;
    free(body);
    return 0;                              /* honest empty is not an error */
  }
  return -1;
}

#define SANCT_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "government", .type = "dataset", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

SANCT_DEF(sw_ofac_def, "OFAC_SDN", "OFAC SDN Sanctions", "OFAC SDN 制裁リスト",
  "https://sanctionssearch.ofac.treas.gov/",
  "US Treasury OFAC Specially Designated Nationals list — screen a name against the SDN CSV export");
SANCT_DEF(sw_eu_def, "EU_SANCTIONS", "EU Consolidated Sanctions", "EU 制裁統合リスト",
  "https://data.europa.eu/data/datasets/consolidated-list-of-persons-groups-and-entities-subject-to-eu-financial-sanctions",
  "EU consolidated financial-sanctions list (needs EU_SANCTIONS_TOKEN for the export)");
SANCT_DEF(sw_un_def, "UN_SANCTIONS", "UN Security Council Sanctions", "国連安保理 制裁リスト",
  "https://www.un.org/securitycouncil/content/un-sc-consolidated-list",
  "UN Security Council consolidated sanctions list (keyless XML export)");
SANCT_DEF(sw_uk_def, "UK_OFSI", "UK OFSI Sanctions", "英国 OFSI 制裁リスト",
  "https://www.gov.uk/government/publications/financial-sanctions-consolidated-list-of-targets",
  "UK OFSI consolidated list of financial-sanctions targets (keyless CSV export)");
SANCT_DEF(sw_wb_def, "WORLDBANK_DEBARRED", "World Bank Debarred Firms", "世界銀行 資格停止企業",
  "https://www.worldbank.org/en/projects-operations/procurement/debarred-firms",
  "World Bank debarred/ineligible firms & individuals (keyless JSON export)");
SANCT_DEF(sw_ca_def, "CA_SANCTIONS", "Canada SEMA Sanctions", "カナダ SEMA 制裁リスト",
  "https://www.international.gc.ca/world-monde/international_relations-relations_internationales/sanctions/consolidated-consolide.aspx",
  "Canada consolidated autonomous sanctions list (SEMA, keyless XML export)");
SANCT_DEF(sw_au_def, "AU_DFAT", "Australia DFAT Sanctions", "豪州 DFAT 制裁リスト",
  "https://www.dfat.gov.au/international-relations/security/sanctions/consolidated-list",
  "Australia DFAT consolidated sanctions list (keyless CSV export)");
SANCT_DEF(sw_ch_def, "CH_SECO", "Switzerland SECO Sanctions", "スイス SECO 制裁リスト",
  "https://www.seco.admin.ch/",
  "Switzerland SECO sanctions list (keyless XML export)");
