/* collectors/osint/sources/ssl_analyzer.c
 * OSINT service — faithful port of OSINTsaas osint_tools/ssl_analyzer.c
 * (handle_ssl_analyzer → ssl_analyze). Canonical SERVICE name in
 * osint_dispatcher.c service_registry[] bound to handle_ssl_analyzer is
 * "SSL_ANALYZER" (line 317; SERVICE_CERTIFICATE_TRANSPARENCY). On-demand
 * (interval 0); dispatcher runs it with ctx->entity = a hostname or
 * hostname:port.
 *
 * Reproduces ssl_analyze() faithfully: a direct TLS connection to the host
 * (OpenSSL, SNI, 10s timeout) extracts the leaf certificate (subject, issuer,
 * trusted-CA flag, serial, validity dates, days_until_expiry + status, SANs,
 * signature algorithm, public-key type/bits + weak-key flag) plus negotiated
 * protocol/cipher; then crt.sh Certificate Transparency JSON is aggregated
 * (total_certificates, issuers map, domains_found map, recent_certificates).
 * Security recommendations are derived identically. Output uses the OSINTsaas
 * result_builder envelope, reproduced exactly: { query, query_type, results:
 * [{source, found, confidence, detection_method, data, llama_analysis:null}],
 * summary:{total_sources, found_count, detection_breakdown}, _meta }.
 * success = TLS-connection-ok, confidence 90/50. No API key. Emits one
 * osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/bn.h>
#include <openssl/objects.h>

#define CONFIDENCE_HIGH   90
#define CONFIDENCE_LOW    30
#define DETECTION_DIRECT  "direct"

static const char *TRUSTED_CAS[] = {
  "DigiCert", "Let's Encrypt", "Comodo", "GlobalSign", "GoDaddy",
  "Sectigo", "Amazon", "Google Trust Services", "Microsoft",
  "Cloudflare", "ZeroSSL", NULL
};

static int is_trusted_ca(const char *issuer) {
  if (!issuer) return 0;
  for (int i = 0; TRUSTED_CAS[i]; i++)
    if (strcasestr(issuer, TRUSTED_CAS[i])) return 1;
  return 0;
}

static char *url_encode_dup(const char *in) {
  size_t n = strlen(in);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[w++] = (char)c;
    } else {
      sprintf(out + w, "%%%02X", c);
      w += 3;
    }
  }
  out[w] = 0;
  return out;
}

static char *x509_name_to_string(X509_NAME *name) {
  if (!name) return NULL;
  BIO *bio = BIO_new(BIO_s_mem());
  if (!bio) return NULL;
  X509_NAME_print_ex(bio, name, 0, XN_FLAG_ONELINE);
  char *buffer = NULL;
  long len = BIO_get_mem_data(bio, &buffer);
  char *result = NULL;
  if (len > 0 && buffer) {
    result = calloc(len + 1, 1);
    if (result) memcpy(result, buffer, len);
  }
  BIO_free(bio);
  return result;
}

static cJSON *extract_sans(X509 *cert) {
  cJSON *sans = cJSON_CreateArray();
  GENERAL_NAMES *names = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
  if (!names) return sans;
  int count = sk_GENERAL_NAME_num(names);
  for (int i = 0; i < count; i++) {
    GENERAL_NAME *entry = sk_GENERAL_NAME_value(names, i);
    if (!entry) continue;
    if (entry->type == GEN_DNS) {
      const char *dns = (const char *)ASN1_STRING_get0_data(entry->d.dNSName);
      if (dns) cJSON_AddItemToArray(sans, cJSON_CreateString(dns));
    } else if (entry->type == GEN_IPADD) {
      const unsigned char *ip = ASN1_STRING_get0_data(entry->d.iPAddress);
      int ip_len = ASN1_STRING_length(entry->d.iPAddress);
      char ip_str[64] = {0};
      if (ip_len == 4)
        snprintf(ip_str, sizeof ip_str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      else if (ip_len == 16)
        inet_ntop(AF_INET6, ip, ip_str, sizeof ip_str);
      if (ip_str[0]) cJSON_AddItemToArray(sans, cJSON_CreateString(ip_str));
    }
  }
  GENERAL_NAMES_free(names);
  return sans;
}

static cJSON *analyze_ssl_direct(const char *hostname, int port) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "method", "direct_ssl_connection");

  const SSL_METHOD *method = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) { cJSON_AddStringToObject(result, "error", "Failed to create SSL context"); return result; }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) { SSL_CTX_free(ctx); cJSON_AddStringToObject(result, "error", "Failed to create socket"); return result; }

  struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(hostname, NULL, &hints, &res) != 0 || !res) {
    if (res) freeaddrinfo(res);
    close(sock); SSL_CTX_free(ctx);
    cJSON_AddStringToObject(result, "error", "Failed to resolve hostname");
    return result;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
  freeaddrinfo(res);

  if (connect(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
    close(sock); SSL_CTX_free(ctx);
    cJSON_AddStringToObject(result, "error", "Failed to connect");
    return result;
  }

  SSL *ssl = SSL_new(ctx);
  SSL_set_fd(ssl, sock);
  SSL_set_tlsext_host_name(ssl, hostname);
  if (SSL_connect(ssl) != 1) {
    SSL_free(ssl); close(sock); SSL_CTX_free(ctx);
    cJSON_AddStringToObject(result, "error", "SSL handshake failed");
    return result;
  }

  X509 *cert = SSL_get_peer_certificate(ssl);
  if (!cert) {
    SSL_shutdown(ssl); SSL_free(ssl); close(sock); SSL_CTX_free(ctx);
    cJSON_AddStringToObject(result, "error", "No certificate received");
    return result;
  }

  cJSON_AddBoolToObject(result, "success", 1);
  cJSON_AddStringToObject(result, "protocol", SSL_get_version(ssl));
  const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);
  if (cipher) {
    cJSON_AddStringToObject(result, "cipher", SSL_CIPHER_get_name(cipher));
    cJSON_AddNumberToObject(result, "cipher_bits", SSL_CIPHER_get_bits(cipher, NULL));
  }

  cJSON *cert_info = cJSON_CreateObject();
  char *subject = x509_name_to_string(X509_get_subject_name(cert));
  if (subject) { cJSON_AddStringToObject(cert_info, "subject", subject); free(subject); }
  char *issuer = x509_name_to_string(X509_get_issuer_name(cert));
  if (issuer) {
    cJSON_AddStringToObject(cert_info, "issuer", issuer);
    cJSON_AddBoolToObject(cert_info, "trusted_ca", is_trusted_ca(issuer));
    free(issuer);
  }

  ASN1_INTEGER *serial = X509_get_serialNumber(cert);
  if (serial) {
    BIGNUM *bn = ASN1_INTEGER_to_BN(serial, NULL);
    if (bn) {
      char *sh = BN_bn2hex(bn);
      if (sh) { cJSON_AddStringToObject(cert_info, "serial_number", sh); OPENSSL_free(sh); }
      BN_free(bn);
    }
  }

  const ASN1_TIME *not_before = X509_get0_notBefore(cert);
  const ASN1_TIME *not_after = X509_get0_notAfter(cert);
  if (not_before) {
    BIO *bio = BIO_new(BIO_s_mem());
    ASN1_TIME_print(bio, not_before);
    char buf[64];
    int len = BIO_read(bio, buf, sizeof buf - 1);
    if (len < 0) len = 0;
    buf[len] = '\0';
    cJSON_AddStringToObject(cert_info, "valid_from", buf);
    BIO_free(bio);
  }
  if (not_after) {
    BIO *bio = BIO_new(BIO_s_mem());
    ASN1_TIME_print(bio, not_after);
    char buf[64];
    int len = BIO_read(bio, buf, sizeof buf - 1);
    if (len < 0) len = 0;
    buf[len] = '\0';
    cJSON_AddStringToObject(cert_info, "valid_until", buf);
    BIO_free(bio);
    int days_left = 0, secs_left = 0;
    ASN1_TIME_diff(&days_left, &secs_left, NULL, not_after);
    cJSON_AddNumberToObject(cert_info, "days_until_expiry", days_left);
    if (days_left < 0) cJSON_AddStringToObject(cert_info, "status", "EXPIRED");
    else if (days_left < 30) cJSON_AddStringToObject(cert_info, "status", "EXPIRING_SOON");
    else cJSON_AddStringToObject(cert_info, "status", "VALID");
  }

  cJSON *sans = extract_sans(cert);
  cJSON_AddItemToObject(cert_info, "subject_alt_names", sans);
  cJSON_AddNumberToObject(cert_info, "san_count", cJSON_GetArraySize(sans));

  int sig_nid = X509_get_signature_nid(cert);
  cJSON_AddStringToObject(cert_info, "signature_algorithm", OBJ_nid2ln(sig_nid));

  EVP_PKEY *pubkey = X509_get_pubkey(cert);
  if (pubkey) {
    int key_type = EVP_PKEY_base_id(pubkey);
    int key_bits = EVP_PKEY_bits(pubkey);
    const char *kts = "Unknown";
    switch (key_type) {
      case EVP_PKEY_RSA: kts = "RSA"; break;
      case EVP_PKEY_EC: kts = "EC"; break;
      case EVP_PKEY_DSA: kts = "DSA"; break;
      case EVP_PKEY_ED25519: kts = "Ed25519"; break;
    }
    cJSON_AddStringToObject(cert_info, "public_key_type", kts);
    cJSON_AddNumberToObject(cert_info, "public_key_bits", key_bits);
    int weak_key = 0;
    if (key_type == EVP_PKEY_RSA && key_bits < 2048) weak_key = 1;
    if (key_type == EVP_PKEY_EC && key_bits < 256) weak_key = 1;
    cJSON_AddBoolToObject(cert_info, "weak_key", weak_key);
    EVP_PKEY_free(pubkey);
  }

  cJSON_AddItemToObject(result, "certificate", cert_info);

  X509_free(cert);
  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(sock);
  SSL_CTX_free(ctx);
  return result;
}

static cJSON *query_ct_logs(http_client *http, const char *domain) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "crt.sh");

  char *enc = url_encode_dup(domain);
  if (!enc) { cJSON_AddStringToObject(result, "error", "URL encoding failed"); return result; }
  char url[512];
  snprintf(url, sizeof url, "https://crt.sh/?q=%s&output=json", enc);
  free(enc);

  cJSON *json = feed_get_json(http, url, 30000);
  if (!json || !cJSON_IsArray(json)) {
    if (json) cJSON_Delete(json);
    cJSON_AddStringToObject(result, "error", "Invalid response");
    return result;
  }

  int total = cJSON_GetArraySize(json);
  cJSON_AddNumberToObject(result, "total_certificates", total);

  cJSON *issuers = cJSON_CreateObject();
  cJSON *domains = cJSON_CreateObject();
  cJSON *recent_certs = cJSON_CreateArray();
  int recent_count = 0;

  for (int i = 0; i < total && i < 100; i++) {
    cJSON *cert = cJSON_GetArrayItem(json, i);
    if (!cert) continue;
    cJSON *issuer_name = cJSON_GetObjectItem(cert, "issuer_name");
    if (issuer_name && cJSON_IsString(issuer_name)) {
      cJSON *e = cJSON_GetObjectItem(issuers, issuer_name->valuestring);
      if (e) cJSON_SetNumberValue(e, e->valuedouble + 1);
      else cJSON_AddNumberToObject(issuers, issuer_name->valuestring, 1);
    }
    cJSON *common_name = cJSON_GetObjectItem(cert, "common_name");
    if (common_name && cJSON_IsString(common_name)) {
      cJSON *e = cJSON_GetObjectItem(domains, common_name->valuestring);
      if (e) cJSON_SetNumberValue(e, e->valuedouble + 1);
      else cJSON_AddNumberToObject(domains, common_name->valuestring, 1);
    }
    cJSON *not_before = cJSON_GetObjectItem(cert, "not_before");
    if (not_before && cJSON_IsString(not_before) && recent_count < 10) {
      cJSON *recent = cJSON_CreateObject();
      cJSON_AddStringToObject(recent, "common_name",
        (common_name && cJSON_IsString(common_name)) ? common_name->valuestring : "N/A");
      cJSON_AddStringToObject(recent, "issuer",
        (issuer_name && cJSON_IsString(issuer_name)) ? issuer_name->valuestring : "N/A");
      cJSON_AddStringToObject(recent, "not_before", not_before->valuestring);
      cJSON *not_after = cJSON_GetObjectItem(cert, "not_after");
      if (not_after && cJSON_IsString(not_after))
        cJSON_AddStringToObject(recent, "not_after", not_after->valuestring);
      cJSON_AddItemToArray(recent_certs, recent);
      recent_count++;
    }
  }
  cJSON_AddItemToObject(result, "issuers", issuers);
  cJSON_AddItemToObject(result, "domains_found", domains);
  cJSON_AddItemToObject(result, "recent_certificates", recent_certs);
  cJSON_Delete(json);
  return result;
}

/* faithful reproduction of result_builder_add_item's per-item JSON */
static void add_builder_item(cJSON *results_arr, int *total, int *found_count,
                             const char *source, int found, int confidence,
                             cJSON *data /*owned*/) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "source", source ? source : "unknown");
  cJSON_AddBoolToObject(o, "found", found);
  cJSON_AddNumberToObject(o, "confidence", confidence);
  cJSON_AddStringToObject(o, "detection_method", DETECTION_DIRECT);
  if (data) cJSON_AddItemToObject(o, "data", data);
  else cJSON_AddNullToObject(o, "data");
  cJSON_AddNullToObject(o, "llama_analysis");
  cJSON_AddItemToArray(results_arr, o);
  (*total)++;
  if (found) (*found_count)++;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *target = ctx->entity;
  if (!target || !*target) return -1;

  char hostname[256] = {0};
  int port = 443;
  const char *colon = strchr(target, ':');
  if (colon && isdigit((unsigned char)colon[1])) {
    size_t hl = (size_t)(colon - target);
    if (hl >= sizeof hostname) hl = sizeof hostname - 1;
    memcpy(hostname, target, hl);
    hostname[hl] = '\0';
    port = atoi(colon + 1);
  } else {
    strncpy(hostname, target, sizeof hostname - 1);
  }

  time_t start_time = time(NULL);
  cJSON *results_arr = cJSON_CreateArray();
  int total = 0, found_count = 0;

  cJSON *direct = analyze_ssl_direct(hostname, port);
  cJSON *ssl_success = cJSON_GetObjectItem(direct, "success");
  int connection_ok = ssl_success && cJSON_IsTrue(ssl_success);
  cJSON *direct_for_rec = cJSON_Duplicate(direct, 1);
  add_builder_item(results_arr, &total, &found_count, "SSL Connection",
                   connection_ok, connection_ok ? CONFIDENCE_HIGH : CONFIDENCE_LOW,
                   direct);

  cJSON *ct = query_ct_logs(ctx->http, hostname);
  cJSON *total_certs = cJSON_GetObjectItem(ct, "total_certificates");
  int has_ct = total_certs && cJSON_IsNumber(total_certs) && total_certs->valuedouble > 0;
  add_builder_item(results_arr, &total, &found_count, "Certificate Transparency",
                   has_ct, has_ct ? CONFIDENCE_HIGH : CONFIDENCE_LOW, ct);

  cJSON *recommendations = cJSON_CreateArray();
  cJSON *cert = direct_for_rec ? cJSON_GetObjectItem(direct_for_rec, "certificate") : NULL;
  if (cert) {
    cJSON *status = cJSON_GetObjectItem(cert, "status");
    if (status && cJSON_IsString(status)) {
      if (strcmp(status->valuestring, "EXPIRED") == 0)
        cJSON_AddItemToArray(recommendations, cJSON_CreateString("CRITICAL: Certificate is expired - renew immediately"));
      else if (strcmp(status->valuestring, "EXPIRING_SOON") == 0)
        cJSON_AddItemToArray(recommendations, cJSON_CreateString("WARNING: Certificate expiring soon - plan renewal"));
    }
    cJSON *weak_key = cJSON_GetObjectItem(cert, "weak_key");
    if (weak_key && cJSON_IsTrue(weak_key))
      cJSON_AddItemToArray(recommendations, cJSON_CreateString("WARNING: Weak key detected - upgrade to 2048+ bit RSA or 256+ bit EC"));
    cJSON *trusted = cJSON_GetObjectItem(cert, "trusted_ca");
    if (trusted && !cJSON_IsTrue(trusted))
      cJSON_AddItemToArray(recommendations, cJSON_CreateString("INFO: Certificate not from well-known CA - may cause trust issues"));
  }
  cJSON *protocol = direct_for_rec ? cJSON_GetObjectItem(direct_for_rec, "protocol") : NULL;
  if (protocol && cJSON_IsString(protocol)) {
    if (strstr(protocol->valuestring, "TLSv1.0") ||
        strstr(protocol->valuestring, "TLSv1.1") ||
        strstr(protocol->valuestring, "SSLv"))
      cJSON_AddItemToArray(recommendations, cJSON_CreateString("CRITICAL: Outdated SSL/TLS version - upgrade to TLS 1.2+"));
  }
  if (cJSON_GetArraySize(recommendations) > 0) {
    cJSON *rw = cJSON_CreateObject();
    cJSON_AddItemToObject(rw, "recommendations", recommendations);
    add_builder_item(results_arr, &total, &found_count, "Security Recommendations",
                     1, CONFIDENCE_HIGH, rw);
  } else {
    cJSON_Delete(recommendations);
  }
  cJSON_Delete(direct_for_rec);

  /* result_builder_finalize envelope */
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", target);
  cJSON_AddStringToObject(root, "query_type", "ssl_certificate");
  cJSON_AddItemToObject(root, "results", results_arr);
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_sources", total);
  cJSON_AddNumberToObject(summary, "found_count", found_count);
  cJSON *breakdown = cJSON_CreateObject();
  cJSON_AddNumberToObject(breakdown, "direct", total);
  cJSON_AddNumberToObject(breakdown, "llama", 0);
  cJSON_AddItemToObject(summary, "detection_breakdown", breakdown);
  cJSON_AddItemToObject(root, "summary", summary);
  cJSON *meta = cJSON_CreateObject();
  time_t end_time = time(NULL);
  cJSON_AddNumberToObject(meta, "execution_time_ms", (int)((end_time - start_time) * 1000));
  cJSON_AddBoolToObject(meta, "llama_fallback_used", 0);
  cJSON_AddNumberToObject(meta, "timestamp", (double)end_time);
  cJSON_AddItemToObject(root, "_meta", meta);

  char *bj = cJSON_PrintUnformatted(root);
  int confidence = connection_ok ? 90 : 50;

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SSL_ANALYZER");
  cJSON_AddStringToObject(props, "entity", target);
  cJSON_AddBoolToObject(props, "success", connection_ok);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "ssl:%s", target);
  char title[320];
  snprintf(title, sizeof title, "SSL_ANALYZER — %s", target);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = connection_ok ? "TLS certificate analyzed" : "TLS connection failed";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SSL_ANALYZER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def ssl_analyzer_def = {
  .id = "SSL_ANALYZER", .collector = "osint",
  .name = "SSL Analyzer", .name_ja = "SSL解析",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(ssl_analyzer_def)
