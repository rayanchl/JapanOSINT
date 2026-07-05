/* collectors/sources/threatfeeds_world.c
 * OSINT services — global threat-intel feeds & blocklists. These are the real,
 * publicly downloadable abuse.ch / community feeds. Each source is either run
 * on a schedule (update_interval_sec ~1h → the newest slice is ingested as
 * intel_items) or pivoted on an entity (ctx->entity = an IP / domain / URL /
 * hash → we scan the freshly-fetched feed and emit only records that literally
 * contain the pivot value).
 *
 *   URLHAUS_GLOBAL        abuse.ch URLhaus — recent malicious URLs (JSON)
 *   THREATFOX_GLOBAL      abuse.ch ThreatFox — recent IOCs (JSON)
 *   MALWAREBAZAAR         abuse.ch MalwareBazaar — recent malware samples (POST JSON)
 *   FEODO_GLOBAL          abuse.ch Feodo Tracker — botnet C2 IP blocklist (JSON)
 *   SSLBL_GLOBAL          abuse.ch SSL Blacklist — malicious SSL certs (CSV)
 *   OPENPHISH             OpenPhish community phishing feed (text)
 *   SPAMHAUS_DROP_GLOBAL  Spamhaus DROP — hijacked/leased netblocks (text)
 *   TOR_EXITS_GLOBAL      Tor Project bulk exit-node IP list (text)
 *
 * HONESTY: every run() REAL-fetches the live feed and emits ONLY parsed real
 * records, or honest-empty (return 0) on fetch failure / no entity match.
 * Nothing is synthesized. One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* case-insensitive substring (haystack may be huge; needle short). */
static const char *tf_stristr(const char *h, const char *n) {
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

/* trim CR/LF/space/quotes at both ends, in place. */
static void tf_trim(char *s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n && (s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='"'||s[n-1]=='\t')) s[--n]=0;
  char *p = s; while (*p==' '||*p=='"'||*p=='\t') p++;
  if (p != s) memmove(s, p, strlen(p)+1);
}

/* Emit one IOC/blocklist hit. All fields are real bytes from the live feed. */
static int tf_emit(intel_sink *sink, const char *service, const char *rectype,
                   const char *key, const char *title, const char *summary,
                   const char *body_json, const char *link, const char *published,
                   const char *tags) {
  if (!title || !*title) return 0;
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "feed", service);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = key ? key : title;
  it.title           = title;
  it.summary         = summary;
  it.body            = body_json;
  it.lang            = "en";
  it.published_at    = published;
  it.link            = link;
  it.record_type     = rectype;
  it.properties_json = pj;
  it.tags_json       = tags ? tags : "[\"threat-intel\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ---- URLhaus recent (JSON) --------------------------------------------- *
 * https://urlhaus.abuse.ch/downloads/json_recent/ → object keyed by numeric id,
 * each value an array with one object { id, url, url_status, host, date_added,
 * threat, tags[], urlhaus_reference }. When entity is set we filter on url/host.*/
static int tf_urlhaus(const source_ctx *ctx, intel_sink *sink, const char *body,
                      const char *q, int max) {
  (void)ctx;
  cJSON *root = cJSON_Parse(body);
  if (!root) { fprintf(stderr, "[URLHAUS_GLOBAL] parse fail\n"); return 0; }
  int emitted = 0;
  cJSON *entry;
  cJSON_ArrayForEach(entry, root) {
    if (emitted >= max) break;
    cJSON *rec = cJSON_IsArray(entry) ? cJSON_GetArrayItem(entry, 0) : entry;
    if (!cJSON_IsObject(rec)) continue;
    const char *url = jo_sv(rec, "url");
    const char *host = jo_sv(rec, "host");
    if (!url && !host) continue;
    if (q && !(url && tf_stristr(url, q)) && !(host && tf_stristr(host, q))) continue;
    const char *id   = jo_sv(rec, "id");
    const char *stat = jo_sv(rec, "url_status");
    const char *thr  = jo_sv(rec, "threat");
    const char *da   = jo_sv(rec, "date_added");
    const char *ref  = jo_sv(rec, "urlhaus_reference");

    cJSON *data = cJSON_CreateObject();
    if (url)  cJSON_AddStringToObject(data, "url", url);
    if (host) cJSON_AddStringToObject(data, "host", host);
    if (stat) cJSON_AddStringToObject(data, "url_status", stat);
    if (thr)  cJSON_AddStringToObject(data, "threat", thr);
    if (da)   cJSON_AddStringToObject(data, "date_added", da);
    cJSON_AddStringToObject(data, "feed", "URLhaus");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    char key[128];
    snprintf(key, sizeof key, "urlhaus|%s", id ? id : (url ? url : host));
    emitted += tf_emit(sink, "URLHAUS_GLOBAL", "malware-url", key,
                       url ? url : host, thr ? thr : stat, bj,
                       ref ? ref : url, da,
                       "[\"threat-intel\",\"malware-url\",\"urlhaus\"]");
    free(bj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[URLHAUS_GLOBAL] emitted %d\n", emitted);
  return emitted;
}

/* ---- ThreatFox recent (JSON) ------------------------------------------- *
 * https://threatfox.abuse.ch/export/json/recent/ → object keyed by ioc id,
 * value = array with one object { ioc_value, ioc_type, threat_type, malware,
 * first_seen_utc, confidence_level, reference, tags }. Filter on ioc_value. */
static int tf_threatfox(const source_ctx *ctx, intel_sink *sink, const char *body,
                        const char *q, int max) {
  (void)ctx;
  cJSON *root = cJSON_Parse(body);
  if (!root) { fprintf(stderr, "[THREATFOX_GLOBAL] parse fail\n"); return 0; }
  int emitted = 0;
  cJSON *entry;
  cJSON_ArrayForEach(entry, root) {
    if (emitted >= max) break;
    cJSON *rec = cJSON_IsArray(entry) ? cJSON_GetArrayItem(entry, 0) : entry;
    if (!cJSON_IsObject(rec)) continue;
    const char *ioc = jo_sv(rec, "ioc_value");
    if (!ioc) ioc = jo_sv(rec, "ioc");
    if (!ioc) continue;
    if (q && !tf_stristr(ioc, q)) continue;
    const char *ioctype = jo_sv(rec, "ioc_type");
    const char *thr  = jo_sv(rec, "threat_type");
    const char *mal  = jo_sv(rec, "malware");
    const char *malp = jo_sv(rec, "malware_printable");
    const char *seen = jo_sv(rec, "first_seen_utc");
    if (!seen) seen = jo_sv(rec, "first_seen");
    const char *ref  = jo_sv(rec, "reference");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "ioc", ioc);
    if (ioctype) cJSON_AddStringToObject(data, "ioc_type", ioctype);
    if (thr)  cJSON_AddStringToObject(data, "threat_type", thr);
    if (malp) cJSON_AddStringToObject(data, "malware", malp);
    else if (mal) cJSON_AddStringToObject(data, "malware", mal);
    if (seen) cJSON_AddStringToObject(data, "first_seen_utc", seen);
    cJSON_AddStringToObject(data, "feed", "ThreatFox");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    char key[256];
    snprintf(key, sizeof key, "threatfox|%.200s", ioc);
    emitted += tf_emit(sink, "THREATFOX_GLOBAL", "threat-ioc", key,
                       ioc, malp ? malp : (mal ? mal : thr), bj,
                       ref, seen,
                       "[\"threat-intel\",\"ioc\",\"threatfox\"]");
    free(bj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[THREATFOX_GLOBAL] emitted %d\n", emitted);
  return emitted;
}

/* ---- MalwareBazaar recent (POST → JSON) -------------------------------- *
 * POST query=get_recent&selector=time to https://mb-api.abuse.ch/api/v1/
 * → { query_status:"ok", data:[ { sha256_hash, file_name, file_type,
 * signature, first_seen, tags[] } ] }. When entity is set, also try a hash/tag
 * lookup by filtering the recent list on the pivot value. */
static int tf_malwarebazaar(const source_ctx *ctx, intel_sink *sink,
                            const char *q, int max) {
  const char *body_post = "query=get_recent&selector=time";
  const char *hdrs[] = {
    "Content-Type: application/x-www-form-urlencoded",
    "User-Agent: JapanOSINT/1.0 (threat-intel)",
    NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "POST", "https://mb-api.abuse.ch/api/v1/",
                        hdrs, body_post, strlen(body_post), 20000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[MALWAREBAZAAR] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) { fprintf(stderr, "[MALWAREBAZAAR] parse fail\n"); return 0; }
  cJSON *arr = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (emitted >= max) break;
      const char *sha = jo_sv(rec, "sha256_hash");
      const char *fn  = jo_sv(rec, "file_name");
      const char *sig = jo_sv(rec, "signature");
      if (!sha) continue;
      if (q) {
        const char *md5 = jo_sv(rec, "md5_hash");
        const char *sha1 = jo_sv(rec, "sha1_hash");
        if (!tf_stristr(sha, q) && !(fn && tf_stristr(fn, q)) &&
            !(sig && tf_stristr(sig, q)) && !(md5 && tf_stristr(md5, q)) &&
            !(sha1 && tf_stristr(sha1, q))) continue;
      }
      const char *ft  = jo_sv(rec, "file_type");
      const char *seen = jo_sv(rec, "first_seen");

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "sha256_hash", sha);
      if (fn)  cJSON_AddStringToObject(data, "file_name", fn);
      if (ft)  cJSON_AddStringToObject(data, "file_type", ft);
      if (sig) cJSON_AddStringToObject(data, "signature", sig);
      if (seen) cJSON_AddStringToObject(data, "first_seen", seen);
      cJSON_AddStringToObject(data, "feed", "MalwareBazaar");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      char key[128], link[160];
      snprintf(key, sizeof key, "mb|%s", sha);
      snprintf(link, sizeof link, "https://bazaar.abuse.ch/sample/%s/", sha);
      emitted += tf_emit(sink, "MALWAREBAZAAR", "malware-sample", key,
                         fn ? fn : sha, sig ? sig : ft, bj, link, seen,
                         "[\"threat-intel\",\"malware-sample\",\"malwarebazaar\"]");
      free(bj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[MALWAREBAZAAR] emitted %d\n", emitted);
  return emitted;
}

/* ---- Feodo Tracker (JSON array of C2 objects) -------------------------- *
 * https://feodotracker.abuse.ch/downloads/ipblocklist.json → array of
 * { ip_address, port, status, hostname, as_number, as_name, country, malware,
 * first_seen, last_online }. Filter on ip_address/hostname. */
static int tf_feodo(const source_ctx *ctx, intel_sink *sink, const char *body,
                    const char *q, int max) {
  (void)ctx;
  cJSON *root = cJSON_Parse(body);
  if (!cJSON_IsArray(root)) { fprintf(stderr, "[FEODO_GLOBAL] parse fail\n"); cJSON_Delete(root); return 0; }
  int emitted = 0;
  cJSON *rec;
  cJSON_ArrayForEach(rec, root) {
    if (emitted >= max) break;
    const char *ip = jo_sv(rec, "ip_address");
    const char *hn = jo_sv(rec, "hostname");
    if (!ip) continue;
    if (q && !tf_stristr(ip, q) && !(hn && tf_stristr(hn, q))) continue;
    const char *mal = jo_sv(rec, "malware");
    const char *stat = jo_sv(rec, "status");
    const char *ctry = jo_sv(rec, "country");
    const char *asn = jo_sv(rec, "as_name");
    const char *seen = jo_sv(rec, "first_seen");
    const cJSON *portj = cJSON_GetObjectItem(rec, "port");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "ip_address", ip);
    if (portj && cJSON_IsNumber(portj)) cJSON_AddNumberToObject(data, "port", portj->valuedouble);
    if (hn)   cJSON_AddStringToObject(data, "hostname", hn);
    if (mal)  cJSON_AddStringToObject(data, "malware", mal);
    if (stat) cJSON_AddStringToObject(data, "status", stat);
    if (ctry) cJSON_AddStringToObject(data, "country", ctry);
    if (asn)  cJSON_AddStringToObject(data, "as_name", asn);
    if (seen) cJSON_AddStringToObject(data, "first_seen", seen);
    cJSON_AddStringToObject(data, "feed", "Feodo Tracker");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    char key[128];
    snprintf(key, sizeof key, "feodo|%s", ip);
    emitted += tf_emit(sink, "FEODO_GLOBAL", "botnet-c2", key,
                       ip, mal ? mal : "botnet C2", bj,
                       "https://feodotracker.abuse.ch/browse/", seen,
                       "[\"threat-intel\",\"botnet-c2\",\"feodo\"]");
    free(bj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[FEODO_GLOBAL] emitted %d\n", emitted);
  return emitted;
}

/* ---- Plain-text / CSV line feeds --------------------------------------- *
 * Generic: iterate lines, skip comments (# or ;) and blanks. For each data line
 * take the FIRST field (up to the first comma / whitespace) as the indicator,
 * keep the remainder as detail. When entity is set, only lines containing it are
 * emitted. Every emitted indicator is a real line from the live feed. */
static int tf_lines(intel_sink *sink, const char *body, const char *q,
                    const char *service, const char *rectype, const char *link,
                    const char *tags, int max) {
  int emitted = 0;
  const char *line = body;
  while (line && *line && emitted < max) {
    const char *nl = strchr(line, '\n');
    size_t llen = nl ? (size_t)(nl - line) : strlen(line);
    if (llen && line[0] != '#' && line[0] != ';') {
      char buf[1024];
      size_t cp = llen < sizeof buf - 1 ? llen : sizeof buf - 1;
      memcpy(buf, line, cp); buf[cp] = 0;
      tf_trim(buf);
      if (buf[0] && (!q || tf_stristr(buf, q))) {
        /* indicator = first field up to comma/space/tab */
        char ind[512];
        size_t k = 0;
        for (const char *p = buf; *p && p[0] != ',' && p[0] != ' ' && p[0] != '\t'
                                    && k < sizeof ind - 1; p++)
          ind[k++] = *p;
        ind[k] = 0;
        if (ind[0]) {
          const char *detail = (strlen(ind) < strlen(buf)) ? buf : NULL;
          char key[600];
          snprintf(key, sizeof key, "%s|%.512s", service, ind);
          emitted += tf_emit(sink, service, rectype, key, ind, detail,
                             NULL, link, NULL, tags);
        }
      }
    }
    if (!nl) break;
    line = nl + 1;
  }
  fprintf(stderr, "[%s] emitted %d\n", service, emitted);
  return emitted;
}

/* ---- dispatch ----------------------------------------------------------- */
typedef enum { M_URLHAUS, M_THREATFOX, M_MALWAREBAZAAR, M_FEODO, M_TEXT } tf_mode;
typedef struct {
  const char *id, *url, *rectype, *tags;
  tf_mode mode;
} tf_row;

static const tf_row ROWS[] = {
  { "URLHAUS_GLOBAL",   "https://urlhaus.abuse.ch/downloads/json_recent/",
    "malware-url", "[\"threat-intel\",\"malware-url\",\"urlhaus\"]", M_URLHAUS },
  { "THREATFOX_GLOBAL", "https://threatfox.abuse.ch/export/json/recent/",
    "threat-ioc", "[\"threat-intel\",\"ioc\",\"threatfox\"]", M_THREATFOX },
  { "MALWAREBAZAAR",    "https://mb-api.abuse.ch/api/v1/",
    "malware-sample", "[\"threat-intel\",\"malware-sample\"]", M_MALWAREBAZAAR },
  { "FEODO_GLOBAL",     "https://feodotracker.abuse.ch/downloads/ipblocklist.json",
    "botnet-c2", "[\"threat-intel\",\"botnet-c2\",\"feodo\"]", M_FEODO },
  { "SSLBL_GLOBAL",     "https://sslbl.abuse.ch/blacklist/sslblacklist.csv",
    "malicious-ssl-cert", "[\"threat-intel\",\"ssl-blacklist\",\"sslbl\"]", M_TEXT },
  { "OPENPHISH",        "https://openphish.com/feed.txt",
    "phishing-url", "[\"threat-intel\",\"phishing\",\"openphish\"]", M_TEXT },
  { "SPAMHAUS_DROP_GLOBAL", "https://www.spamhaus.org/drop/drop.txt",
    "hijacked-netblock", "[\"threat-intel\",\"drop\",\"spamhaus\"]", M_TEXT },
  { "TOR_EXITS_GLOBAL", "https://check.torproject.org/torbulkexitlist",
    "tor-exit-node", "[\"threat-intel\",\"tor-exit\"]", M_TEXT },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* entity is optional: NULL on a scheduled feed run (ingest recent slice),
   * set on an OSINT pivot (filter feed to the pivot value). */
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;

  for (size_t i = 0; i < sizeof ROWS / sizeof ROWS[0]; i++) {
    if (strcmp(ctx->source_id, ROWS[i].id) != 0) continue;
    const tf_row *r = &ROWS[i];
    /* scheduled ingest keeps a bounded recent slice; entity pivots may match
     * more, but all feeds here are recent/bounded exports. */
    int max = q ? 100 : 200;

    if (r->mode == M_MALWAREBAZAAR) {
      tf_malwarebazaar(ctx, sink, q, max);
      return 0;
    }

    const char *hdrs[] = {
      "Accept: application/json, text/csv, text/plain, */*",
      "User-Agent: JapanOSINT/1.0 (threat-intel)",
      NULL };
    char *body = jo_get(ctx, r->url, hdrs, r->id);
    if (!body) return 0;                     /* fetch failed → honest empty */

    switch (r->mode) {
      case M_URLHAUS:   tf_urlhaus(ctx, sink, body, q, max); break;
      case M_THREATFOX: tf_threatfox(ctx, sink, body, q, max); break;
      case M_FEODO:     tf_feodo(ctx, sink, body, q, max); break;
      case M_TEXT:
      default:          tf_lines(sink, body, q, r->id, r->rectype, r->url, r->tags, max); break;
    }
    free(body);
    return 0;                                /* honest empty is not an error */
  }
  return -1;
}

#define TF_DEF(SYM, ID, NAME, NAMEJA, INTERVAL, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = INTERVAL, .run = run, \
    .category = "cyber", .type = "dataset", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

TF_DEF(tf_urlhaus_def, "URLHAUS_GLOBAL", "URLhaus Malicious URLs", "URLhaus 悪性URL",
  3600, "https://urlhaus.abuse.ch/",
  "abuse.ch URLhaus — recent malicious URLs (keyless JSON); pivot on URL/host");
TF_DEF(tf_threatfox_def, "THREATFOX_GLOBAL", "ThreatFox IOCs", "ThreatFox 脅威指標",
  3600, "https://threatfox.abuse.ch/",
  "abuse.ch ThreatFox — recent IOCs (keyless JSON); pivot on ioc value");
TF_DEF(tf_mb_def, "MALWAREBAZAAR", "MalwareBazaar Samples", "MalwareBazaar 検体",
  3600, "https://bazaar.abuse.ch/",
  "abuse.ch MalwareBazaar — recent malware samples (POST JSON); pivot on hash/name/signature");
TF_DEF(tf_feodo_def, "FEODO_GLOBAL", "Feodo Tracker C2 IPs", "Feodo Tracker C2 IP",
  3600, "https://feodotracker.abuse.ch/",
  "abuse.ch Feodo Tracker — botnet C2 IP blocklist (keyless JSON); pivot on IP/hostname");
TF_DEF(tf_sslbl_def, "SSLBL_GLOBAL", "SSL Blacklist", "SSL ブラックリスト",
  3600, "https://sslbl.abuse.ch/",
  "abuse.ch SSL Blacklist — malicious SSL certificate blacklist (keyless CSV); pivot on SHA1/IP");
TF_DEF(tf_openphish_def, "OPENPHISH", "OpenPhish Feed", "OpenPhish フィード",
  3600, "https://openphish.com/",
  "OpenPhish community phishing URL feed (keyless text); pivot on URL/host");
TF_DEF(tf_spamhaus_def, "SPAMHAUS_DROP_GLOBAL", "Spamhaus DROP", "Spamhaus DROP",
  3600, "https://www.spamhaus.org/drop/",
  "Spamhaus DROP — hijacked/leased netblocks (keyless text); pivot on CIDR");
TF_DEF(tf_tor_def, "TOR_EXITS_GLOBAL", "Tor Exit Nodes", "Tor 出口ノード",
  3600, "https://check.torproject.org/",
  "Tor Project bulk exit-node IP list (keyless text); pivot on IP");
