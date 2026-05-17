/* collectors/osint/sources/censys_search.c
 * OSINT service — faithful port of OSINTsaas osint_tools/censys_search.c
 * (handle_censys_search). Canonical SERVICE id in osint_dispatcher.c:
 * {SERVICE_CENSYS_SEARCH, handle_censys_search, "CENSYS_SEARCH", true}.
 * Entity = ip / domain. Key-gated: CENSYS_API_ID + CENSYS_API_SECRET
 * (HTTP Basic base64(id:secret)). Upstream does NOT early-return when keys
 * are absent — it emits a structured {service,credentials_configured:false,
 * hosts_found:0,certificates_found:0,results:[{query,status:"no_credentials",
 * note}],note} object (success=true conf=85). We reproduce that exactly so
 * behaviour is faithful (no key ⇒ a no_credentials result row, not silence).
 * With creds: IP → /hosts/<ip>; domain → /hosts/search + /certificates/search
 * POST; else generic /hosts/search. Single-entity pivot form of upstream's
 * multi-entity loop. Emits one osint_service_result row (body =
 * {success,confidence,data}), like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define CENSYS_BASE "https://search.censys.io/api/v2"

static char *b64(const unsigned char *in, size_t n) {
  static const char *T =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *out = malloc(((n + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = in[i] << 16;
    if (i + 1 < n) v |= in[i + 1] << 8;
    if (i + 2 < n) v |= in[i + 2];
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? T[v & 63] : '=';
  }
  out[o] = 0;
  return out;
}

static int looks_ipv4(const char *s) {
  int n[4], c = sscanf(s, "%d.%d.%d.%d", &n[0], &n[1], &n[2], &n[3]);
  if (c != 4) return 0;
  for (int i = 0; i < 4; i++) if (n[i] < 0 || n[i] > 255) return 0;
  for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p) && *p != '.') return 0;
  return 1;
}
static int looks_domain(const char *s) {
  return strchr(s, '.') && !strchr(s, ' ') && !strchr(s, '@') && !looks_ipv4(s);
}

/* Authenticated GET/POST → parsed JSON + http code (out). NULL on transport. */
static cJSON *censys_req(http_client *http, const char *method,
                         const char *url, const char *body,
                         const char *auth, long *code) {
  const char *hdr[3] = { auth, "Accept: application/json", NULL };
  http_response hr = {0};
  int hc = http_request(http, method, url, hdr, body,
                         body ? strlen(body) : 0, 20000, 1, &hr);
  *code = hr.status;
  cJSON *j = NULL;
  if (hc == 0 && hr.body) j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (hc != 0) *code = 0;
  return j;
}

static cJSON *query_host(http_client *http, const char *ip, const char *auth) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "censys_hosts");
  cJSON_AddStringToObject(r, "ip", ip);
  char url[512];
  snprintf(url, sizeof url, "%s/hosts/%s", CENSYS_BASE, ip);
  long code = 0;
  cJSON *j = censys_req(http, "GET", url, NULL, auth, &code);
  if (!j && code == 0) { cJSON_AddStringToObject(r, "status", "request_failed"); return r; }
  if (code == 401) { cJSON_AddStringToObject(r, "status", "invalid_credentials"); if (j) cJSON_Delete(j); return r; }
  if (code == 404) { cJSON_AddStringToObject(r, "status", "not_found");
    cJSON_AddStringToObject(r, "message", "IP not in Censys database"); if (j) cJSON_Delete(j); return r; }
  if (code == 429) { cJSON_AddStringToObject(r, "status", "rate_limited");
    cJSON_AddStringToObject(r, "message",
      "Censys API rate limit exceeded (250/month free tier)"); if (j) cJSON_Delete(j); return r; }
  if (code != 200) { cJSON_AddStringToObject(r, "status", "error");
    cJSON_AddNumberToObject(r, "http_code", (double)code); if (j) cJSON_Delete(j); return r; }
  if (!j) { cJSON_AddStringToObject(r, "status", "parse_error"); return r; }
  cJSON_AddStringToObject(r, "status", "found");
  cJSON *hr = cJSON_GetObjectItem(j, "result");
  if (hr) {
    cJSON *as = cJSON_GetObjectItem(hr, "autonomous_system");
    if (as) {
      cJSON *asn = cJSON_GetObjectItem(as, "asn");
      cJSON *nm = cJSON_GetObjectItem(as, "name");
      cJSON *bp = cJSON_GetObjectItem(as, "bgp_prefix");
      cJSON *co = cJSON_GetObjectItem(as, "country_code");
      if (asn) cJSON_AddNumberToObject(r, "asn", asn->valueint);
      if (nm && nm->valuestring) cJSON_AddStringToObject(r, "as_name", nm->valuestring);
      if (bp && bp->valuestring) cJSON_AddStringToObject(r, "bgp_prefix", bp->valuestring);
      if (co && co->valuestring) cJSON_AddStringToObject(r, "country", co->valuestring);
    }
    cJSON *loc = cJSON_GetObjectItem(hr, "location");
    if (loc) {
      cJSON *lo = cJSON_CreateObject();
      const char *k[] = {"city","province","country","continent"};
      for (int i = 0; i < 4; i++) {
        cJSON *v = cJSON_GetObjectItem(loc, k[i]);
        if (v && v->valuestring) cJSON_AddStringToObject(lo, k[i], v->valuestring);
      }
      cJSON_AddItemToObject(r, "location", lo);
    }
    cJSON *os = cJSON_GetObjectItem(hr, "operating_system");
    if (os) {
      cJSON *oo = cJSON_CreateObject();
      const char *k[] = {"product","vendor","version"};
      for (int i = 0; i < 3; i++) {
        cJSON *v = cJSON_GetObjectItem(os, k[i]);
        if (v && v->valuestring) cJSON_AddStringToObject(oo, k[i], v->valuestring);
      }
      cJSON_AddItemToObject(r, "operating_system", oo);
    }
    cJSON *svcs = cJSON_GetObjectItem(hr, "services");
    if (svcs && cJSON_IsArray(svcs)) {
      cJSON *ports = cJSON_CreateArray();
      cJSON *sa = cJSON_CreateArray();
      int sc = cJSON_GetArraySize(svcs);
      for (int i = 0; i < sc && i < 50; i++) {
        cJSON *s = cJSON_GetArrayItem(svcs, i);
        cJSON *po = cJSON_GetObjectItem(s, "port");
        cJSON *snm = cJSON_GetObjectItem(s, "service_name");
        cJSON *tp = cJSON_GetObjectItem(s, "transport_protocol");
        if (po) cJSON_AddItemToArray(ports, cJSON_CreateNumber(po->valueint));
        cJSON *so = cJSON_CreateObject();
        if (po) cJSON_AddNumberToObject(so, "port", po->valueint);
        if (snm && snm->valuestring) cJSON_AddStringToObject(so, "service", snm->valuestring);
        if (tp && tp->valuestring) cJSON_AddStringToObject(so, "transport", tp->valuestring);
        cJSON *sw = cJSON_GetObjectItem(s, "software");
        if (sw && cJSON_IsArray(sw) && cJSON_GetArraySize(sw) > 0) {
          cJSON *s0 = cJSON_GetArrayItem(sw, 0);
          cJSON *pr = cJSON_GetObjectItem(s0, "product");
          cJSON *ve = cJSON_GetObjectItem(s0, "version");
          if (pr && pr->valuestring) cJSON_AddStringToObject(so, "software", pr->valuestring);
          if (ve && ve->valuestring) cJSON_AddStringToObject(so, "version", ve->valuestring);
        }
        cJSON_AddItemToArray(sa, so);
      }
      cJSON_AddItemToObject(r, "open_ports", ports);
      cJSON_AddNumberToObject(r, "port_count", cJSON_GetArraySize(ports));
      cJSON_AddItemToObject(r, "services", sa);
    }
    cJSON *lu = cJSON_GetObjectItem(hr, "last_updated_at");
    if (lu && lu->valuestring) cJSON_AddStringToObject(r, "last_updated", lu->valuestring);
    cJSON *dns = cJSON_GetObjectItem(hr, "dns");
    if (dns) {
      cJSON *nm = cJSON_GetObjectItem(dns, "names");
      if (nm && cJSON_IsArray(nm))
        cJSON_AddItemToObject(r, "hostnames", cJSON_Duplicate(nm, 1));
    }
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *search_hosts(http_client *http, const char *query, const char *auth) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "censys_search");
  cJSON_AddStringToObject(r, "query", query);
  cJSON *bo = cJSON_CreateObject();
  cJSON_AddStringToObject(bo, "q", query);
  cJSON_AddNumberToObject(bo, "per_page", 25);
  char *bs = cJSON_PrintUnformatted(bo);
  cJSON_Delete(bo);
  char url[512];
  snprintf(url, sizeof url, "%s/hosts/search", CENSYS_BASE);
  long code = 0;
  cJSON *j = censys_req(http, "POST", url, bs, auth, &code);
  free(bs);
  if (!j || code != 200) {
    if (code) cJSON_AddNumberToObject(r, "http_code", (double)code);
    cJSON_AddStringToObject(r, "status", code ? "error" : "error");
    if (j) cJSON_Delete(j);
    return r;
  }
  cJSON_AddStringToObject(r, "status", "found");
  cJSON *ro = cJSON_GetObjectItem(j, "result");
  if (ro) {
    cJSON *tot = cJSON_GetObjectItem(ro, "total");
    if (tot) cJSON_AddNumberToObject(r, "total_results", tot->valueint);
    cJSON *hits = cJSON_GetObjectItem(ro, "hits");
    if (hits && cJSON_IsArray(hits)) {
      cJSON *hosts = cJSON_CreateArray();
      int n = cJSON_GetArraySize(hits);
      for (int i = 0; i < n && i < 25; i++) {
        cJSON *hit = cJSON_GetArrayItem(hits, i);
        cJSON *ho = cJSON_CreateObject();
        cJSON *ip = cJSON_GetObjectItem(hit, "ip");
        if (ip && ip->valuestring) cJSON_AddStringToObject(ho, "ip", ip->valuestring);
        cJSON *sv = cJSON_GetObjectItem(hit, "services");
        if (sv && cJSON_IsArray(sv)) {
          cJSON *ports = cJSON_CreateArray();
          int m = cJSON_GetArraySize(sv);
          for (int k = 0; k < m; k++) {
            cJSON *s = cJSON_GetArrayItem(sv, k);
            cJSON *po = cJSON_GetObjectItem(s, "port");
            if (po) cJSON_AddItemToArray(ports, cJSON_CreateNumber(po->valueint));
          }
          cJSON_AddItemToObject(ho, "ports", ports);
        }
        cJSON *as = cJSON_GetObjectItem(hit, "autonomous_system");
        if (as) {
          cJSON *nm = cJSON_GetObjectItem(as, "name");
          if (nm && nm->valuestring) cJSON_AddStringToObject(ho, "as_name", nm->valuestring);
        }
        cJSON *loc = cJSON_GetObjectItem(hit, "location");
        if (loc) {
          cJSON *co = cJSON_GetObjectItem(loc, "country");
          if (co && co->valuestring) cJSON_AddStringToObject(ho, "country", co->valuestring);
        }
        cJSON_AddItemToArray(hosts, ho);
      }
      cJSON_AddItemToObject(r, "hosts", hosts);
    }
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *search_certs(http_client *http, const char *domain, const char *auth) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "censys_certificates");
  cJSON_AddStringToObject(r, "domain", domain);
  char q[512];
  snprintf(q, sizeof q, "names: %s", domain);
  cJSON *bo = cJSON_CreateObject();
  cJSON_AddStringToObject(bo, "q", q);
  cJSON_AddNumberToObject(bo, "per_page", 10);
  char *bs = cJSON_PrintUnformatted(bo);
  cJSON_Delete(bo);
  char url[512];
  snprintf(url, sizeof url, "%s/certificates/search", CENSYS_BASE);
  long code = 0;
  cJSON *j = censys_req(http, "POST", url, bs, auth, &code);
  free(bs);
  if (!j || code != 200) {
    if (code) cJSON_AddNumberToObject(r, "http_code", (double)code);
    cJSON_AddStringToObject(r, "status", "error");
    if (j) cJSON_Delete(j);
    return r;
  }
  cJSON_AddStringToObject(r, "status", "found");
  cJSON *ro = cJSON_GetObjectItem(j, "result");
  if (ro) {
    cJSON *tot = cJSON_GetObjectItem(ro, "total");
    if (tot) cJSON_AddNumberToObject(r, "total_certificates", tot->valueint);
    cJSON *hits = cJSON_GetObjectItem(ro, "hits");
    if (hits && cJSON_IsArray(hits)) {
      cJSON *certs = cJSON_CreateArray();
      int n = cJSON_GetArraySize(hits);
      for (int i = 0; i < n && i < 10; i++) {
        cJSON *hit = cJSON_GetArrayItem(hits, i);
        cJSON *c = cJSON_CreateObject();
        cJSON *fp = cJSON_GetObjectItem(hit, "fingerprint_sha256");
        if (fp && fp->valuestring) cJSON_AddStringToObject(c, "fingerprint", fp->valuestring);
        cJSON *nm = cJSON_GetObjectItem(hit, "names");
        if (nm && cJSON_IsArray(nm))
          cJSON_AddItemToObject(c, "names", cJSON_Duplicate(nm, 1));
        cJSON *is = cJSON_GetObjectItem(hit, "issuer");
        if (is) {
          cJSON *org = cJSON_GetObjectItem(is, "organization");
          if (org && cJSON_IsArray(org) && cJSON_GetArraySize(org) > 0) {
            cJSON *o0 = cJSON_GetArrayItem(org, 0);
            if (o0 && o0->valuestring) cJSON_AddStringToObject(c, "issuer_org", o0->valuestring);
          }
        }
        cJSON *va = cJSON_GetObjectItem(hit, "validity");
        if (va) {
          cJSON *st = cJSON_GetObjectItem(va, "start");
          cJSON *en = cJSON_GetObjectItem(va, "end");
          if (st && st->valuestring) cJSON_AddStringToObject(c, "valid_from", st->valuestring);
          if (en && en->valuestring) cJSON_AddStringToObject(c, "valid_to", en->valuestring);
        }
        cJSON_AddItemToArray(certs, c);
      }
      cJSON_AddItemToObject(r, "certificates", certs);
    }
  }
  cJSON_Delete(j);
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;
  if (strchr(entity, '@')) return -1;       /* upstream skips emails */

  const char *id = getenv("CENSYS_API_ID");
  const char *secret = getenv("CENSYS_API_SECRET");
  int has = id && secret && *id && *secret;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "service", "CENSYS_SEARCH");
  cJSON_AddBoolToObject(data, "credentials_configured", has);
  cJSON *all = cJSON_CreateArray();
  int total_hosts = 0, total_certs = 0;

  cJSON *er = cJSON_CreateObject();
  cJSON_AddStringToObject(er, "query", entity);

  if (!has) {
    fprintf(stderr, "[CENSYS_SEARCH] gated (no CENSYS_API_ID/SECRET)\n");
    cJSON_AddStringToObject(er, "status", "no_credentials");
    cJSON_AddStringToObject(er, "note",
      "Set CENSYS_API_ID and CENSYS_API_SECRET for Censys access");
  } else {
    char raw[512];
    snprintf(raw, sizeof raw, "%s:%s", id, secret);
    char *enc = b64((const unsigned char *)raw, strlen(raw));
    char auth[600];
    snprintf(auth, sizeof auth, "Authorization: Basic %s", enc ? enc : "");
    free(enc);

    if (looks_ipv4(entity)) {
      cJSON *host = query_host(ctx->http, entity, auth);
      cJSON_AddItemToObject(er, "host", host);
      cJSON *st = cJSON_GetObjectItem(host, "status");
      if (st && st->valuestring && strcmp(st->valuestring, "found") == 0)
        total_hosts++;
    } else if (looks_domain(entity)) {
      char sq[256];
      snprintf(sq, sizeof sq, "dns.names: %s", entity);
      cJSON *hosts = search_hosts(ctx->http, sq, auth);
      cJSON_AddItemToObject(er, "hosts", hosts);
      cJSON *t = cJSON_GetObjectItem(hosts, "total_results");
      if (t && t->valueint > 0) total_hosts += t->valueint;
      cJSON *certs = search_certs(ctx->http, entity, auth);
      cJSON_AddItemToObject(er, "certificates", certs);
      cJSON *ct = cJSON_GetObjectItem(certs, "total_certificates");
      if (ct && ct->valueint > 0) total_certs += ct->valueint;
    } else {
      cJSON *hosts = search_hosts(ctx->http, entity, auth);
      cJSON_AddItemToObject(er, "hosts", hosts);
      cJSON *t = cJSON_GetObjectItem(hosts, "total_results");
      if (t && t->valueint > 0) total_hosts += t->valueint;
    }
  }
  cJSON_AddItemToArray(all, er);

  cJSON_AddNumberToObject(data, "hosts_found", total_hosts);
  cJSON_AddNumberToObject(data, "certificates_found", total_certs);
  cJSON_AddItemToObject(data, "results", all);
  if (!has)
    cJSON_AddStringToObject(data, "note",
      "Censys requires API credentials. Register free at https://censys.io (250 queries/month)");

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 85);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CENSYS_SEARCH");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "censys:%s", entity);
  snprintf(title, sizeof title, "CENSYS_SEARCH — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = has ? "Censys queried" : "Censys (no credentials)";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CENSYS_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def censys_search_def = {
  .id = "CENSYS_SEARCH", .collector = "osint",
  .name = "Censys Search", .name_ja = "Censys検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(censys_search_def)
