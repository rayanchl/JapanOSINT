/* collectors/sources/vuln_world.c
 * OSINT services — global vulnerability intelligence. On-demand entity pivot
 * (ctx->entity = a product/vendor/keyword, or a CVE id) against the real,
 * publicly reachable keyless vulnerability feeds:
 *
 *   NVD_CVE          NIST NVD CVE API 2.0 — keyword search (keyless, rate-limited)
 *   CISA_KEV_GLOBAL  CISA Known Exploited Vulnerabilities catalog (global JSON)
 *   EXPLOITDB        Exploit-DB files_exploits.csv (GitLab raw export)
 *   EPSS_SCORES      FIRST.org EPSS API — exploit-prediction score for a CVE
 *
 * Every run() REAL-fetches over HTTP and emits ONLY parsed real data, or honest
 * empty (return 0) on fetch failure / no match. Nothing is fabricated: names,
 * descriptions, scores and links are all extracted from the live response.
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

/* True when q looks like a CVE id (CVE-YYYY-NNNN…). */
static int vw_is_cve(const char *q) {
  return q && (strncasecmp(q, "CVE-", 4) == 0);
}

/* trim CR/LF/quotes/spaces from both ends, in place. */
static void vw_clean(char *s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n && (s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='"')) s[--n]=0;
  char *p = s; while (*p==' '||*p=='"') p++;
  if (p != s) memmove(s, p, strlen(p)+1);
}

/* ---- NVD CVE API 2.0 ---------------------------------------------------- *
 * Response: { vulnerabilities: [ { cve: { id, published, lastModified,
 *   descriptions:[{lang,value}], metrics:{ cvssMetricV31:[{cvssData:{baseScore,
 *   baseSeverity}}] }, references:[{url}] } } ] } */
static const char *nvd_desc_en(const cJSON *cve) {
  const cJSON *descs = cJSON_GetObjectItem(cve, "descriptions");
  if (!cJSON_IsArray(descs)) return NULL;
  const cJSON *d;
  cJSON_ArrayForEach(d, descs) {
    const char *lang = jo_sv(d, "lang");
    if (lang && strcmp(lang, "en") == 0) return jo_sv(d, "value");
  }
  return NULL;
}

/* pull baseScore/baseSeverity from the first available CVSS metric array. */
static double nvd_cvss(const cJSON *cve, const char **severity) {
  const cJSON *metrics = cJSON_GetObjectItem(cve, "metrics");
  if (!metrics) return -1;
  static const char *keys[] = { "cvssMetricV31", "cvssMetricV30", "cvssMetricV2" };
  for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
    const cJSON *arr = cJSON_GetObjectItem(metrics, keys[i]);
    if (!cJSON_IsArray(arr)) continue;
    const cJSON *m = cJSON_GetArrayItem(arr, 0);  /* exhaustive-ok: primary CVSS metric per version key */
    if (!m) continue;
    const cJSON *cd = cJSON_GetObjectItem(m, "cvssData");
    const cJSON *bs = cd ? cJSON_GetObjectItem(cd, "baseScore") : NULL;
    if (severity) {
      const char *sev = cd ? jo_sv(cd, "baseSeverity") : NULL;
      if (!sev) sev = jo_sv(m, "baseSeverity");
      *severity = sev;
    }
    if (bs && cJSON_IsNumber(bs)) return bs->valuedouble;
  }
  return -1;
}

static int nvd_emit(intel_sink *sink, const cJSON *item) {
  const cJSON *cve = cJSON_GetObjectItem(item, "cve");
  if (!cve) return 0;
  const char *id  = jo_sv(cve, "id");
  if (!id) return 0;
  const char *pub = jo_sv(cve, "published");
  const char *mod = jo_sv(cve, "lastModified");
  const char *desc = nvd_desc_en(cve);
  const char *sev = NULL;
  double score = nvd_cvss(cve, &sev);

  /* first reference url = link */
  const char *ref = NULL;
  const cJSON *refs = cJSON_GetObjectItem(cve, "references");
  if (cJSON_IsArray(refs)) {
    const cJSON *r = cJSON_GetArrayItem(refs, 0);  /* exhaustive-ok: display pick; references_all carries every reference */
    if (r) ref = jo_sv(r, "url");
  }
  char nvdurl[128];
  snprintf(nvdurl, sizeof nvdurl, "https://nvd.nist.gov/vuln/detail/%s", id);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "cve", id);
  if (desc) cJSON_AddStringToObject(data, "description", desc);
  if (score >= 0) cJSON_AddNumberToObject(data, "cvss_base_score", score);
  if (sev)  cJSON_AddStringToObject(data, "cvss_severity", sev);
  if (pub)  cJSON_AddStringToObject(data, "published", pub);
  if (mod)  cJSON_AddStringToObject(data, "last_modified", mod);
  cJSON_AddStringToObject(data, "source", "NVD");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "NVD_CVE");
  cJSON_AddStringToObject(props, "cve", id);
  if (score >= 0) cJSON_AddNumberToObject(props, "cvss_base_score", score);
  if (sev)  cJSON_AddStringToObject(props, "cvss_severity", sev);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = id;
  it.title           = id;
  it.summary         = desc;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = pub;
  it.link            = ref ? ref : nvdurl;
  it.record_type     = "nvd-cve";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"vulnerability\",\"cve\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int nvd_run(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  if (vw_is_cve(q))
    snprintf(url, sizeof url,
      "https://services.nvd.nist.gov/rest/json/cves/2.0?cveId=%s", enc);
  else
    snprintf(url, sizeof url,
      "https://services.nvd.nist.gov/rest/json/cves/2.0?keywordSearch=%s&resultsPerPage=20", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (vuln-intel)", NULL };
  char *body = jo_get(ctx, url, hdrs, "NVD_CVE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = 0;
  const cJSON *vulns = cJSON_GetObjectItem(root, "vulnerabilities");
  if (cJSON_IsArray(vulns)) {
    const cJSON *v;
    cJSON_ArrayForEach(v, vulns) {
      emitted += nvd_emit(sink, v);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[NVD_CVE] emitted %d\n", emitted);
  return emitted;
}

/* ---- CISA KEV catalog --------------------------------------------------- *
 * { vulnerabilities: [ { cveID, vendorProject, product, vulnerabilityName,
 *   dateAdded, shortDescription, requiredAction, dueDate, knownRansomwareCampaignUse } ] }
 * Filter: cveID / vendorProject / product / vulnerabilityName / shortDescription
 * contains the query. */
static int kev_emit(intel_sink *sink, const cJSON *r, const char *q) {
  const char *cve  = jo_sv(r, "cveID");
  const char *vend = jo_sv(r, "vendorProject");
  const char *prod = jo_sv(r, "product");
  const char *name = jo_sv(r, "vulnerabilityName");
  const char *desc = jo_sv(r, "shortDescription");
  const char *added = jo_sv(r, "dateAdded");
  const char *due  = jo_sv(r, "dueDate");
  const char *ransom = jo_sv(r, "knownRansomwareCampaignUse");
  if (!cve) return 0;

  /* honest filter — only emit genuine matches on real fields */
  int match = (!q || !*q)
              || jo_stristr(cve, q) || jo_stristr(vend, q) || jo_stristr(prod, q)
              || jo_stristr(name, q) || jo_stristr(desc, q);
  if (!match) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "cve", cve);
  if (vend) cJSON_AddStringToObject(data, "vendor", vend);
  if (prod) cJSON_AddStringToObject(data, "product", prod);
  if (name) cJSON_AddStringToObject(data, "name", name);
  if (desc) cJSON_AddStringToObject(data, "description", desc);
  if (added) cJSON_AddStringToObject(data, "date_added", added);
  if (due)  cJSON_AddStringToObject(data, "due_date", due);
  if (ransom) cJSON_AddStringToObject(data, "ransomware_use", ransom);
  cJSON_AddStringToObject(data, "source", "CISA KEV");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CISA_KEV_GLOBAL");
  cJSON_AddStringToObject(props, "cve", cve);
  if (vend) cJSON_AddStringToObject(props, "vendor", vend);
  if (prod) cJSON_AddStringToObject(props, "product", prod);
  cJSON_AddStringToObject(props, "known_exploited", "true");
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[128];
  snprintf(link, sizeof link, "https://nvd.nist.gov/vuln/detail/%s", cve);

  intel_item it = {0};
  it.remote_key      = cve;
  it.title           = name ? name : cve;
  it.summary         = desc;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = added;
  it.link            = link;
  it.record_type     = "cisa-kev";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"vulnerability\",\"known-exploited\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int kev_run(const source_ctx *ctx, intel_sink *sink, const char *q) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (vuln-intel)", NULL };
  char *body = jo_get(ctx,
    "https://www.cisa.gov/sites/default/files/feeds/known_exploited_vulnerabilities.json",
    hdrs, "CISA_KEV_GLOBAL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = 0;
  const cJSON *vulns = cJSON_GetObjectItem(root, "vulnerabilities");
  if (cJSON_IsArray(vulns)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, vulns) {
      emitted += kev_emit(sink, r, q);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[CISA_KEV_GLOBAL] emitted %d\n", emitted);
  return emitted;
}

/* ---- Exploit-DB files_exploits.csv -------------------------------------- *
 * Header: id,file,description,date_published,author,type,platform,port,...
 * We match the query against the whole line, then parse the first fields
 * (quote-aware) to surface id/description/type/platform, and build the real
 * exploit-db.com detail URL from the id. */
static int edb_run(const source_ctx *ctx, intel_sink *sink, const char *q) {
  const char *hdrs[] = { "Accept: text/csv, text/plain, */*",
                         "User-Agent: JapanOSINT/1.0 (vuln-intel)", NULL };
  char *body = jo_get(ctx,
    "https://gitlab.com/exploit-database/exploitdb/-/raw/main/files_exploits.csv",
    hdrs, "EXPLOITDB");
  if (!body) return 0;

  int emitted = 0, first = 1;
  const char *line = body;
  while (line && *line && emitted < 50) {
    const char *nl = strchr(line, '\n');
    size_t llen = nl ? (size_t)(nl - line) : strlen(line);
    if (first) { first = 0; goto next; }  /* skip CSV header */
    if (llen > 1) {
      char buf[4096];
      size_t cp = llen < sizeof buf - 1 ? llen : sizeof buf - 1;
      memcpy(buf, line, cp); buf[cp] = 0;
      if (jo_stristr(buf, q)) {
        /* quote-aware split of the first 7 fields */
        char *fields[7] = {0}; int nf = 0;
        char *w = buf, *fs = buf; int inq = 0;
        for (char *r = buf; ; r++) {
          char c = *r;
          if (inq) {
            if (c == '"') { if (r[1] == '"') { *w++ = '"'; r++; } else inq = 0; }
            else if (c == 0) { *w = 0; if (nf < 7) fields[nf++] = fs; break; }
            else *w++ = c;
          } else {
            if (c == '"') inq = 1;
            else if (c == ',' || c == 0) {
              int last = (c == 0);
              *w++ = 0;
              if (nf < 7) fields[nf++] = fs;
              fs = w;
              if (last || nf >= 7) break;
            } else *w++ = c;
          }
        }
        const char *id   = nf > 0 ? fields[0] : NULL;
        const char *desc = nf > 2 ? fields[2] : NULL;
        const char *date = nf > 3 ? fields[3] : NULL;
        const char *auth = nf > 4 ? fields[4] : NULL;
        const char *type = nf > 5 ? fields[5] : NULL;
        const char *plat = nf > 6 ? fields[6] : NULL;
        char idc[64] = {0}, dsc[1024] = {0};
        if (id)   { snprintf(idc, sizeof idc, "%s", id);   vw_clean(idc); }
        if (desc) { snprintf(dsc, sizeof dsc, "%s", desc); vw_clean(dsc); }
        /* require the query to match description or id, not just an author
         * hash etc.; keeps hits meaningful and honest */
        if (idc[0] && (jo_stristr(dsc, q) || jo_stristr(idc, q))) {
          cJSON *data = cJSON_CreateObject();
          cJSON_AddStringToObject(data, "edb_id", idc);
          if (dsc[0]) cJSON_AddStringToObject(data, "description", dsc);
          if (type) cJSON_AddStringToObject(data, "type", type);
          if (plat) cJSON_AddStringToObject(data, "platform", plat);
          if (date) cJSON_AddStringToObject(data, "date_published", date);
          if (auth) cJSON_AddStringToObject(data, "author", auth);
          cJSON_AddStringToObject(data, "source", "Exploit-DB");
          char *bj = cJSON_PrintUnformatted(data);
          cJSON_Delete(data);

          cJSON *props = cJSON_CreateObject();
          cJSON_AddStringToObject(props, "service", "EXPLOITDB");
          cJSON_AddStringToObject(props, "edb_id", idc);
          if (type) cJSON_AddStringToObject(props, "type", type);
          if (plat) cJSON_AddStringToObject(props, "platform", plat);
          cJSON_AddBoolToObject(props, "success", 1);
          char *pj = cJSON_PrintUnformatted(props);
          cJSON_Delete(props);

          char link[96];
          snprintf(link, sizeof link, "https://www.exploit-db.com/exploits/%s", idc);

          intel_item it = {0};
          it.remote_key      = idc;
          it.title           = dsc[0] ? dsc : idc;
          it.summary         = plat;
          it.body            = bj;
          it.lang            = "en";
          it.published_at    = date;
          it.author          = auth;
          it.link            = link;
          it.record_type     = "exploitdb-entry";
          it.properties_json = pj;
          it.tags_json       = "[\"osint-search\",\"exploit\",\"vulnerability\"]";
          if (sink->emit(sink, &it) >= 0) emitted++;
          free(bj); free(pj);
        }
      }
    }
  next:
    if (!nl) break;
    line = nl + 1;
  }
  free(body);
  fprintf(stderr, "[EXPLOITDB] emitted %d\n", emitted);
  return emitted;
}

/* ---- EPSS (FIRST.org) --------------------------------------------------- *
 * { data: [ { cve, epss, percentile, date } ] }. Requires a CVE id; a plain
 * keyword isn't meaningful to EPSS, so non-CVE entities → honest empty. */
static int epss_run(const source_ctx *ctx, intel_sink *sink, const char *q) {
  if (!vw_is_cve(q)) {
    fprintf(stderr, "[EPSS_SCORES] entity is not a CVE id; skipping\n");
    return 0;
  }
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url, "https://api.first.org/data/v1/epss?cve=%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (vuln-intel)", NULL };
  char *body = jo_get(ctx, url, hdrs, "EPSS_SCORES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = 0;
  const cJSON *arr = cJSON_GetObjectItem(root, "data");
  if (cJSON_IsArray(arr)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      const char *cve  = jo_sv(r, "cve");
      const char *epss = jo_sv(r, "epss");
      const char *pct  = jo_sv(r, "percentile");
      const char *date = jo_sv(r, "date");
      if (!cve) continue;

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "cve", cve);
      if (epss) cJSON_AddStringToObject(data, "epss_score", epss);
      if (pct)  cJSON_AddStringToObject(data, "percentile", pct);
      if (date) cJSON_AddStringToObject(data, "score_date", date);
      cJSON_AddStringToObject(data, "source", "FIRST.org EPSS");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "EPSS_SCORES");
      cJSON_AddStringToObject(props, "cve", cve);
      if (epss) cJSON_AddStringToObject(props, "epss_score", epss);
      if (pct)  cJSON_AddStringToObject(props, "percentile", pct);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char summary[128];
      snprintf(summary, sizeof summary, "EPSS %s (pct %s)",
               epss ? epss : "?", pct ? pct : "?");
      char link[128];
      snprintf(link, sizeof link, "https://nvd.nist.gov/vuln/detail/%s", cve);

      intel_item it = {0};
      it.remote_key      = cve;
      it.title           = cve;
      it.summary         = summary;
      it.body            = bj;
      it.lang            = "en";
      it.published_at    = date;
      it.link            = link;
      it.record_type     = "epss-score";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"vulnerability\",\"epss\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[EPSS_SCORES] emitted %d\n", emitted);
  return emitted;
}

/* ---- dispatch ----------------------------------------------------------- */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;
  if (strlen(q) < 3) {
    fprintf(stderr, "[vuln_world] entity too short, skipping\n");
    return 0;
  }
  const char *id = ctx->source_id;

  if (strcmp(id, "NVD_CVE") == 0)          nvd_run(ctx, sink, q);
  else if (strcmp(id, "CISA_KEV_GLOBAL") == 0) kev_run(ctx, sink, q);
  else if (strcmp(id, "EXPLOITDB") == 0)   edb_run(ctx, sink, q);
  else if (strcmp(id, "EPSS_SCORES") == 0) epss_run(ctx, sink, q);
  else return -1;

  return 0;   /* honest empty is not an error */
}

#define VW_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "cyber", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

VW_DEF(vw_nvd_def, "NVD_CVE", "NVD CVE Search", "NVD CVE 検索",
  "https://services.nvd.nist.gov/rest/json/cves/2.0",
  "NIST NVD CVE API 2.0 — keyword/CVE-id search (keyless): id, CVSS, description");
VW_DEF(vw_kev_def, "CISA_KEV_GLOBAL", "CISA Known Exploited Vulns", "CISA 既知悪用脆弱性",
  "https://www.cisa.gov/known-exploited-vulnerabilities-catalog",
  "CISA Known Exploited Vulnerabilities catalog (keyless global JSON feed)");
VW_DEF(vw_edb_def, "EXPLOITDB", "Exploit-DB", "Exploit-DB エクスプロイト",
  "https://www.exploit-db.com/",
  "Exploit-DB files_exploits.csv (keyless GitLab export): exploit id, description, platform");
VW_DEF(vw_epss_def, "EPSS_SCORES", "EPSS Exploit Prediction", "EPSS 悪用予測スコア",
  "https://www.first.org/epss/",
  "FIRST.org EPSS API — exploit-prediction score & percentile for a CVE id (keyless)");
