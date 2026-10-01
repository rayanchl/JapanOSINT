/* collectors/osint/sources/censys_search.c
 * OSINT service — CENSYS_SEARCH. On-demand (interval 0); ctx->entity = ip /
 * domain. Key-gated: CENSYS_API_ID + CENSYS_API_SECRET (HTTP Basic
 * base64(id:secret)).
 *
 * PER-RECORD EMIT (only with credentials):
 *   - IP → /hosts/<ip>: emit ONE item per host (remote_key="censys:<ip>",
 *     body=real ASN/location/OS/ports/services).
 *   - domain → /hosts/search (one item per matching host) + /certificates/
 *     search (one item per cert, remote_key="censyscert:<fp>").
 * WITHOUT credentials: emit NOTHING (no no_credentials note row). Net/parse
 * failures or empty results: emit nothing. Never fabricate. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
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

static int emit_item(intel_sink *sink, const char *rk, const char *title,
                     const char *summary, cJSON *body /*not owned*/,
                     int has_geo, double lat, double lon, const char *kind) {
  char *bj = cJSON_PrintUnformatted(body);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CENSYS_SEARCH");
  cJSON_AddStringToObject(props, "kind", kind);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.has_geo         = has_geo;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CENSYS_SEARCH\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* GET /hosts/<ip>: emit one host item if found. Returns count emitted. */
static int query_host(intel_sink *sink, http_client *http,
                      const char *ip, const char *auth) {
  char url[512];
  snprintf(url, sizeof url, "%s/hosts/%s", CENSYS_BASE, ip);
  long code = 0;
  cJSON *j = censys_req(http, "GET", url, NULL, auth, &code);
  if (!j || code != 200) { if (j) cJSON_Delete(j); return 0; }
  cJSON *hr = cJSON_GetObjectItem(j, "result");
  if (!hr) { cJSON_Delete(j); return 0; }

  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "ip", ip);

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
  int has_geo = 0; double lat = 0, lon = 0;
  cJSON *loc = cJSON_GetObjectItem(hr, "location");
  if (loc) {
    cJSON *lo = cJSON_CreateObject();
    const char *k[] = {"city","province","country","continent"};
    for (int i = 0; i < 4; i++) {
      cJSON *v = cJSON_GetObjectItem(loc, k[i]);
      if (v && v->valuestring) cJSON_AddStringToObject(lo, k[i], v->valuestring);
    }
    cJSON *coord = cJSON_GetObjectItem(loc, "coordinates");
    if (coord) {
      cJSON *la = cJSON_GetObjectItem(coord, "latitude");
      cJSON *ln = cJSON_GetObjectItem(coord, "longitude");
      if (la && cJSON_IsNumber(la) && ln && cJSON_IsNumber(ln)) {
        lat = la->valuedouble; lon = ln->valuedouble; has_geo = 1;
        cJSON_AddNumberToObject(lo, "latitude", lat);
        cJSON_AddNumberToObject(lo, "longitude", lon);
      }
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
        cJSON *s0 = cJSON_GetArrayItem(sw, 0);  /* exhaustive-ok: display pick; software_all carries every value */
        cJSON *pr = cJSON_GetObjectItem(s0, "product");
        cJSON *ve = cJSON_GetObjectItem(s0, "version");
        if (pr && pr->valuestring) cJSON_AddStringToObject(so, "software", pr->valuestring);
        if (ve && ve->valuestring) cJSON_AddStringToObject(so, "version", ve->valuestring);
      }
      /* a port usually runs several detected products — keep every one */
      cJSON_AddItemToObject(so, "software_all", cJSON_Duplicate(sw, 1));
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

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "censys:%s", ip);
  cJSON *asn_j = cJSON_GetObjectItem(r, "as_name");
  snprintf(title, sizeof title, "Censys host %s", ip);
  int e = emit_item(sink, rk, title,
                    (asn_j && asn_j->valuestring) ? asn_j->valuestring : "Censys host",
                    r, has_geo, lat, lon, "host");
  cJSON_Delete(r);
  cJSON_Delete(j);
  return e;
}

/* POST /hosts/search: emit one item per matching host. Returns count emitted. */
static int search_hosts(intel_sink *sink, http_client *http,
                        const char *query, const char *auth) {
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
  if (!j || code != 200) { if (j) cJSON_Delete(j); return 0; }

  cJSON *ro = cJSON_GetObjectItem(j, "result");
  cJSON *hits = ro ? cJSON_GetObjectItem(ro, "hits") : NULL;
  int emitted = 0;
  if (hits && cJSON_IsArray(hits)) {
    int n = cJSON_GetArraySize(hits);
    for (int i = 0; i < n && i < 25; i++) {
      cJSON *hit = cJSON_GetArrayItem(hits, i);
      cJSON *ip = cJSON_GetObjectItem(hit, "ip");
      if (!(ip && ip->valuestring)) continue;
      cJSON *ho = cJSON_CreateObject();
      cJSON_AddStringToObject(ho, "ip", ip->valuestring);
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
      int has_geo = 0; double lat = 0, lon = 0;
      cJSON *loc = cJSON_GetObjectItem(hit, "location");
      if (loc) {
        cJSON *co = cJSON_GetObjectItem(loc, "country");
        if (co && co->valuestring) cJSON_AddStringToObject(ho, "country", co->valuestring);
        cJSON *coord = cJSON_GetObjectItem(loc, "coordinates");
        if (coord) {
          cJSON *la = cJSON_GetObjectItem(coord, "latitude");
          cJSON *ln = cJSON_GetObjectItem(coord, "longitude");
          if (la && cJSON_IsNumber(la) && ln && cJSON_IsNumber(ln)) {
            lat = la->valuedouble; lon = ln->valuedouble; has_geo = 1;
          }
        }
      }
      char rk[300], title[320];
      snprintf(rk, sizeof rk, "censys:%s", ip->valuestring);
      snprintf(title, sizeof title, "Censys host %s", ip->valuestring);
      cJSON *an = cJSON_GetObjectItem(ho, "as_name");
      emitted += emit_item(sink, rk, title,
                           (an && an->valuestring) ? an->valuestring : "Censys host",
                           ho, has_geo, lat, lon, "host");
      cJSON_Delete(ho);
    }
  }
  cJSON_Delete(j);
  return emitted;
}

/* POST /certificates/search: emit one item per cert. Returns count emitted. */
static int search_certs(intel_sink *sink, http_client *http,
                        const char *domain, const char *auth) {
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
  if (!j || code != 200) { if (j) cJSON_Delete(j); return 0; }

  cJSON *ro = cJSON_GetObjectItem(j, "result");
  cJSON *hits = ro ? cJSON_GetObjectItem(ro, "hits") : NULL;
  int emitted = 0;
  if (hits && cJSON_IsArray(hits)) {
    int n = cJSON_GetArraySize(hits);
    for (int i = 0; i < n && i < 10; i++) {
      cJSON *hit = cJSON_GetArrayItem(hits, i);
      cJSON *fp = cJSON_GetObjectItem(hit, "fingerprint_sha256");
      if (!(fp && fp->valuestring)) continue;
      cJSON *c = cJSON_CreateObject();
      cJSON_AddStringToObject(c, "domain", domain);
      cJSON_AddStringToObject(c, "fingerprint", fp->valuestring);
      cJSON *nm = cJSON_GetObjectItem(hit, "names");
      if (nm && cJSON_IsArray(nm))
        cJSON_AddItemToObject(c, "names", cJSON_Duplicate(nm, 1));
      cJSON *is = cJSON_GetObjectItem(hit, "issuer");
      if (is) {
        cJSON *org = cJSON_GetObjectItem(is, "organization");
        if (org && cJSON_IsArray(org) && cJSON_GetArraySize(org) > 0) {
          cJSON *o0 = cJSON_GetArrayItem(org, 0);  /* exhaustive-ok: display pick; issuer_org_all carries every value */
          if (o0 && o0->valuestring) cJSON_AddStringToObject(c, "issuer_org", o0->valuestring);
        }
        cJSON_AddItemToObject(c, "issuer_org_all", cJSON_Duplicate(org, 1));
      }
      cJSON *va = cJSON_GetObjectItem(hit, "validity");
      if (va) {
        cJSON *st = cJSON_GetObjectItem(va, "start");
        cJSON *en = cJSON_GetObjectItem(va, "end");
        if (st && st->valuestring) cJSON_AddStringToObject(c, "valid_from", st->valuestring);
        if (en && en->valuestring) cJSON_AddStringToObject(c, "valid_to", en->valuestring);
      }
      char rk[320], title[320];
      snprintf(rk, sizeof rk, "censyscert:%s", fp->valuestring);
      snprintf(title, sizeof title, "Censys cert — %s", domain);
      cJSON *io = cJSON_GetObjectItem(c, "issuer_org");
      emitted += emit_item(sink, rk, title,
                           (io && io->valuestring) ? io->valuestring : "Censys certificate",
                           c, 0, 0, 0, "certificate");
      cJSON_Delete(c);
    }
  }
  cJSON_Delete(j);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return 0;
  if (strchr(entity, '@')) return 0;       /* upstream skips emails */

  const char *id = getenv("CENSYS_API_ID");
  const char *secret = getenv("CENSYS_API_SECRET");
  if (!(id && secret && *id && *secret)) {
    fprintf(stderr, "[CENSYS_SEARCH] gated (no CENSYS_API_ID/SECRET)\n");
    return 0;                              /* key-gated: emit nothing */
  }

  char raw[512];
  snprintf(raw, sizeof raw, "%s:%s", id, secret);
  char *enc = b64((const unsigned char *)raw, strlen(raw));
  char auth[600];
  snprintf(auth, sizeof auth, "Authorization: Basic %s", enc ? enc : "");
  free(enc);

  if (looks_ipv4(entity)) {
    query_host(sink, ctx->http, entity, auth);
  } else if (looks_domain(entity)) {
    char sq[256];
    snprintf(sq, sizeof sq, "dns.names: %s", entity);
    search_hosts(sink, ctx->http, sq, auth);
    search_certs(sink, ctx->http, entity, auth);
  } else {
    search_hosts(sink, ctx->http, entity, auth);
  }
  return 0;
}

static const source_def censys_search_def = {
  .id = "CENSYS_SEARCH", .collector = "osint",
  .name = "Censys Search", .name_ja = "Censys検索",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/censys-search",
  .description = "Query Censys hosts/certificates for an IP or domain.",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(censys_search_def)
