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
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- small utils -------------------------------------------------------- */

/* ---- name matching ------------------------------------------------------ *
 * A raw case-insensitive SUBSTRING test was doing the screening, and it is
 * wrong in both directions:
 *
 *   FALSE NEGATIVE — the official lists store people surname-first
 *     ("PUTIN, Vladimir Vladimirovich"), so the query "Vladimir Putin" never
 *     matched and OFAC_SDN reported CLEAR for a listed person.
 *   FALSE POSITIVE — "PUTIN" is a substring of "comPUTINg", so the query
 *     "PUTIN" emitted "RANA INTELLIGENCE COMPUTING COMPANY" as a sanctions
 *     hit. Reporting an unlisted entity as sanctioned is the worse half.
 *
 * Replacement: every whitespace-separated token of the query must appear in
 * the candidate text as a WHOLE WORD (delimited by non-alphanumerics), order
 * independent. Nothing is invented — this only decides which real records are
 * echoed back. */
static int sw_word_at(const char *h, const char *tok, size_t tl) {
  for (const char *p = h; *p; p++) {
    if (tolower((unsigned char)*p) != tolower((unsigned char)*tok)) continue;
    size_t k = 1;
    while (k < tl && p[k] &&
           tolower((unsigned char)p[k]) == tolower((unsigned char)tok[k])) k++;
    if (k != tl) continue;
    int lok = (p == h) || !isalnum((unsigned char)p[-1]);
    int rok = !p[tl] || !isalnum((unsigned char)p[tl]);
    if (lok && rok) return 1;
  }
  return 0;
}

/* 1 if every token of `q` occurs as a whole word in `hay`. */
static int sw_name_match(const char *hay, const char *q) {
  if (!hay || !q || !*q) return 0;
  const char *p = q;
  int tokens = 0;
  while (*p) {
    while (*p && isspace((unsigned char)*p)) p++;
    const char *s = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    size_t tl = (size_t)(p - s);
    if (!tl) break;
    /* punctuation-only fragments ("," in "PUTIN,") carry no signal */
    int alnum = 0;
    for (size_t i = 0; i < tl; i++) if (isalnum((unsigned char)s[i])) alnum = 1;
    if (!alnum) continue;
    tokens++;
    if (!sw_word_at(hay, s, tl)) return 0;
  }
  return tokens > 0;
}

/* decode the handful of XML/HTML entities the official lists actually use */
static void sw_unescape(char *s) {
  char *w = s;
  for (char *r = s; *r; ) {
    if (*r == '&') {
      if      (!strncmp(r, "&amp;",  5)) { *w++ = '&';  r += 5; continue; }
      else if (!strncmp(r, "&lt;",   4)) { *w++ = '<';  r += 4; continue; }
      else if (!strncmp(r, "&gt;",   4)) { *w++ = '>';  r += 4; continue; }
      else if (!strncmp(r, "&quot;", 6)) { *w++ = '"';  r += 6; continue; }
      else if (!strncmp(r, "&apos;", 6)) { *w++ = '\''; r += 6; continue; }
      else if (!strncmp(r, "&#39;",  5)) { *w++ = '\''; r += 5; continue; }
    }
    *w++ = *r++;
  }
  *w = 0;
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
    /* No line-level substring prefilter: it could not see past OFAC's
     * "SURNAME, Given" ordering for multi-token queries. Parse the record,
     * then decide on the NAME field alone (see sw_name_match). */
    if (llen) {
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
      if (nm[0] && strcmp(nm, "-0-") != 0 && sw_name_match(nm, q)) {
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
                         const char *listing_url, int name_cols, int max) {
  int emitted = 0, idx = 0;
  const char *line = body;
  while (line && *line && emitted < max) {
    const char *nl = strchr(line, '\n');
    size_t llen = nl ? (size_t)(nl - line) : strlen(line);
    if (llen > 1) {
      /* 2048 truncated 64 of the UK OFSI records (their lines run to 4.4 kB),
       * silently dropping the tail of the record from both the match and the
       * emitted detail. */
      char buf[8192];
      size_t cp = llen < sizeof buf - 1 ? llen : sizeof buf - 1;
      memcpy(buf, line, cp); buf[cp] = 0;
      sw_clean(buf);

      /* Name from the leading name columns rather than 200 bytes of raw CSV:
       * OFSI puts the six name parts first ("MITHOO,Mian,,,,,"), so the first
       * `name_cols` POSITIONAL fields are the name (most usually empty). */
      char nm[256]; nm[0] = 0;
      if (name_cols > 0) {
        size_t o = 0; int nf = 0, inq = 0;
        const char *fs = buf;
        for (const char *r2 = buf; nf < name_cols; r2++) {
          if (inq) { if (*r2 == '"') inq = 0; if (*r2) continue; }
          if (*r2 == '"') { inq = 1; continue; }
          if (*r2 == ',' || *r2 == 0) {
            size_t fl = (size_t)(r2 - fs);
            while (fl && (fs[0] == ' ' || fs[0] == '"')) { fs++; fl--; }
            while (fl && (fs[fl-1] == ' ' || fs[fl-1] == '"')) fl--;
            nf++;                            /* positional, empty or not */
            if (fl && o + fl + 2 < sizeof nm) {
              if (o) nm[o++] = ' ';
              memcpy(nm + o, fs, fl); o += fl; nm[o] = 0;
            }
            if (!*r2) break;
            fs = r2 + 1;
          }
        }
        sw_unescape(nm);
      }

      /* Screen on the NAME columns when we know the layout. Matching the whole
       * CSV row matched the free-text "Statement of Reasons" as well: the
       * query "Putin" returned 708 OFSI records (capped at 50) — Abramovich,
       * Abakarov, … — because their reasons text mentions Putin. Only 10 OFSI
       * records carry Putin/Putina as a NAME. Reporting an unlisted-for-that-
       * name person as a sanctions hit is exactly the failure mode we are
       * fixing. */
      const char *hay = (name_cols > 0 && nm[0]) ? nm : buf;
      if (hay[0] && sw_name_match(hay, q)) {
        if (!nm[0]) snprintf(nm, sizeof nm, "%.200s", buf);
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

/* ---- Record-aware XML scan (UN, Canada SEMA) ---------------------------- *
 * The XML lists are pretty-printed ONE FIELD PER LINE:
 *
 *   <INDIVIDUAL>
 *     <FIRST_NAME>ERIC</FIRST_NAME>
 *     <SECOND_NAME>BADEGE</SECOND_NAME>
 *
 * so a per-LINE matcher can never see a full name: "Eric Badege" — a genuinely
 * UN-listed person — has its two tokens on two different lines and the source
 * reported CLEAR. Same for Canada (<LastName>/<GivenName>). That is a false
 * negative in a sanctions screen, so the scan is done per RECORD element: the
 * name + alias fields of one record are concatenated, matched as a unit, and
 * the emitted title is the real name instead of a raw XML fragment. */

/* append every <tag>…</tag> value inside [rec,rec+len) to out (bounded).
 * Returns the number of occurrences appended. */
static int sw_xml_collect(const char *rec, size_t len, const char *tag,
                          char *out, size_t outsz, const char *sep) {
  char open[64], close[64];
  snprintf(open, sizeof open, "<%s>", tag);
  snprintf(close, sizeof close, "</%s>", tag);
  size_t ol = strlen(open), cl = strlen(close);
  size_t o = strlen(out);
  int hits = 0;
  for (size_t i = 0; i + ol < len; i++) {
    if (rec[i] != '<' || strncmp(rec + i, open, ol) != 0) continue;
    size_t vs = i + ol, ve = vs;
    while (ve + cl <= len && strncmp(rec + ve, close, cl) != 0) ve++;
    if (ve + cl > len) break;
    size_t vl = ve - vs;
    /* trim whitespace around the value */
    while (vl && isspace((unsigned char)rec[vs])) { vs++; vl--; }
    while (vl && isspace((unsigned char)rec[vs + vl - 1])) vl--;
    if (vl) {
      if (o && sep && o + strlen(sep) < outsz - 1) {
        memcpy(out + o, sep, strlen(sep)); o += strlen(sep);
      }
      if (o + vl < outsz - 1) { memcpy(out + o, rec + vs, vl); o += vl; }
      out[o] = 0;
      hits++;
    }
    i = ve + cl - 1;
  }
  /* some values wrap their content in a further element (UN NATIONALITY holds
   * <VALUE>…</VALUE>); drop nested markup before it reaches the row. */
  {
    char *w = out; int in = 0;
    for (char *r2 = out; *r2; r2++) {
      if (*r2 == '<') { in = 1; continue; }
      if (*r2 == '>') { in = 0; continue; }
      if (!in) *w++ = *r2;
    }
    *w = 0;
  }
  sw_unescape(out);
  return hits;
}

static int sw_scan_xml(intel_sink *sink, const char *body, const char *q,
                       const char *service, const char *rectype,
                       const char *listing_url, const char *rec_tags,
                       const char *name_tags, const char *alias_tags,
                       const char *prog_tag, const char *detail_tags, int max) {
  int emitted = 0, idx = 0;
  char tags[256];
  snprintf(tags, sizeof tags, "%s", rec_tags ? rec_tags : "");
  char *save = NULL;
  for (char *rt = strtok_r(tags, ",", &save); rt && emitted < max;
       rt = strtok_r(NULL, ",", &save)) {
    char open[64], close[64];
    snprintf(open, sizeof open, "<%s>", rt);
    snprintf(close, sizeof close, "</%s>", rt);
    const char *p = body;
    for (;;) {
      const char *s = strstr(p, open);
      if (!s) break;
      const char *e = strstr(s, close);
      if (!e) break;
      size_t len = (size_t)(e + strlen(close) - s);
      idx++;
      p = e + strlen(close);

      /* the text we screen against: names + aliases only. Matching the whole
       * record (comments, addresses) would resurrect the false positives. */
      char hay[4096]; hay[0] = 0;
      char nm[512];   nm[0] = 0;
      char lst[128];  lst[0] = 0;
      char det[512];  det[0] = 0;
      char fields[256];

      snprintf(fields, sizeof fields, "%s", name_tags ? name_tags : "");
      char *s2 = NULL;
      for (char *t = strtok_r(fields, ",", &s2); t; t = strtok_r(NULL, ",", &s2))
        sw_xml_collect(s, len, t, nm, sizeof nm, " ");
      snprintf(hay, sizeof hay, "%s", nm);
      if (alias_tags) {
        snprintf(fields, sizeof fields, "%s", alias_tags);
        s2 = NULL;
        for (char *t = strtok_r(fields, ",", &s2); t; t = strtok_r(NULL, ",", &s2))
          sw_xml_collect(s, len, t, hay, sizeof hay, " | ");
      }
      if (!hay[0] || !sw_name_match(hay, q)) { if (emitted >= max) break; continue; }
      if (!nm[0]) snprintf(nm, sizeof nm, "%.200s", hay);

      if (prog_tag) sw_xml_collect(s, len, prog_tag, lst, sizeof lst, ", ");
      if (detail_tags) {
        snprintf(fields, sizeof fields, "%s", detail_tags);
        s2 = NULL;
        for (char *t = strtok_r(fields, ",", &s2); t; t = strtok_r(NULL, ",", &s2)) {
          char one[192]; one[0] = 0;
          if (sw_xml_collect(s, len, t, one, sizeof one, ", ") && one[0]) {
            size_t o = strlen(det);
            snprintf(det + o, sizeof det - o, "%s%s: %s",
                     o ? " | " : "", t, one);
          }
        }
      }
      emitted += sw_emit(sink, service, rectype, q, nm,
                         det[0] ? det : NULL, lst[0] ? lst : NULL,
                         NULL, listing_url, idx);
      if (emitted >= max) break;
    }
  }
  fprintf(stderr, "[%s] emitted %d (records scanned %d)\n", service, emitted, idx);
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
      /* same whole-word screening rule as the other lists — a raw substring
       * test here would match "PT INDO..." for the query "Indo" etc. */
      if (!sw_name_match(name, q)) { idx++; continue; }
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
typedef enum { M_OFAC_CSV, M_WB_JSON, M_SCAN, M_XML } sw_mode;
typedef struct {
  const char *id, *service, *rectype, *url, *listing, *token_env;
  sw_mode mode;
  /* M_XML only: which element is one record, which tags carry the name, the
   * aliases, the programme and the extra detail (all comma-separated). */
  const char *xml_rec, *xml_name, *xml_alias, *xml_prog, *xml_detail;
  /* M_SCAN only: how many leading CSV columns are the NAME (0 = layout
   * unknown, screen the whole row and accept the noise). */
  int csv_name_cols;
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
  /* UN Security Council consolidated — keyless XML, one record per
   * <INDIVIDUAL>/<ENTITY> element */
  { "UN_SANCTIONS", "UN_SANCTIONS", "sanctions-un",
    "https://scsanctions.un.org/resources/xml/en/consolidated.xml",
    "https://www.un.org/securitycouncil/content/un-sc-consolidated-list",
    NULL, M_XML,
    "INDIVIDUAL,ENTITY",
    "FIRST_NAME,SECOND_NAME,THIRD_NAME,FOURTH_NAME",
    "ALIAS_NAME",
    "UN_LIST_TYPE",
    "REFERENCE_NUMBER,LISTED_ON,NATIONALITY,DATE_OF_BIRTH,COMMENTS1" },
  /* UK OFSI consolidated — keyless CSV. Columns 1-6 are "Name 6" (family
   * name) followed by "Name 1".."Name 5"; everything after that is
   * biographical/narrative and must not be screened as a name. */
  { "UK_OFSI", "UK_OFSI", "sanctions-uk",
    "https://ofsistorage.blob.core.windows.net/publishlive/2022format/ConList.csv",
    "https://www.gov.uk/government/publications/financial-sanctions-consolidated-list-of-targets",
    NULL, M_SCAN, NULL, NULL, NULL, NULL, NULL, 6 },
  /* World Bank debarred firms — keyless JSON */
  { "WORLDBANK_DEBARRED", "WORLDBANK_DEBARRED", "sanctions-debarment",
    "https://apigwext.worldbank.org/dvsvc/v1.0/json/APPLICATION/ADOBE_EXPRT_WS/OFFICIAL/DEBARRED_FIRMS",
    "https://www.worldbank.org/en/projects-operations/procurement/debarred-firms",
    NULL, M_WB_JSON },
  /* Canada SEMA consolidated autonomous sanctions — keyless XML, one record
   * per <record> element (person: LastName+GivenName, else EntityOrShip) */
  { "CA_SANCTIONS", "CA_SANCTIONS", "sanctions-ca",
    "https://www.international.gc.ca/world-monde/assets/office_docs/international_relations-relations_internationales/sanctions/sema-lmes.xml",
    "https://www.international.gc.ca/world-monde/international_relations-relations_internationales/sanctions/consolidated-consolide.aspx",
    NULL, M_XML,
    "record",
    "LastName,GivenName,EntityOrShip",
    "Aliases",
    "Country",
    "Schedule,Item,DateOfListing,DateOfBirthOrShipBuildDate,ShipIMONumber,TitleOrShip" },
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
      case M_XML:
        emitted = sw_scan_xml(sink, body, q, r->service, r->rectype, r->listing,
                              r->xml_rec, r->xml_name, r->xml_alias,
                              r->xml_prog, r->xml_detail, 50);
        break;
      case M_SCAN:
      default:
        emitted = sw_scan_lines(sink, body, q, r->service, r->rectype,
                                r->listing, r->csv_name_cols, 50);
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
