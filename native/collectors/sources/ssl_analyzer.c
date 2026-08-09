/* collectors/osint/sources/ssl_analyzer.c
 * OSINT service — SSL_ANALYZER. On-demand (interval 0); ctx->entity = a
 * hostname or hostname:port. No API key.
 *
 * Performs a direct TLS handshake (OpenSSL, SNI, 10s timeout) to extract the
 * leaf certificate, then aggregates crt.sh Certificate Transparency JSON.
 *
 * PER-RECORD EMIT:
 *   - ONE item for the leaf cert (remote_key="cert:<host>") with real subject/
 *     issuer/serial/validity/days_until_expiry/SANs/sig-alg/key-bits/protocol/
 *     cipher.
 *   - ONE item per crt.sh historical cert (remote_key="ctcert:<host>:<id>")
 *     with real common_name/issuer/serial/validity.
 * If the TLS handshake fails, emits NOTHING. TRUSTED_CAS[] stays a match list
 * and is never emitted as data. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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

/* Direct TLS handshake → leaf cert body object, or NULL on any failure. On
 * success the returned object carries protocol/cipher + all leaf cert fields.
 * The caller owns the returned cJSON. */
static cJSON *analyze_ssl_direct(const char *hostname, int port) {
  const SSL_METHOD *method = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) return NULL;

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) { SSL_CTX_free(ctx); return NULL; }

  struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(hostname, NULL, &hints, &res) != 0 || !res) {
    if (res) freeaddrinfo(res);
    close(sock); SSL_CTX_free(ctx);
    return NULL;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
  freeaddrinfo(res);

  if (connect(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
    close(sock); SSL_CTX_free(ctx);
    return NULL;
  }

  SSL *ssl = SSL_new(ctx);
  SSL_set_fd(ssl, sock);
  SSL_set_tlsext_host_name(ssl, hostname);
  if (SSL_connect(ssl) != 1) {
    SSL_free(ssl); close(sock); SSL_CTX_free(ctx);
    return NULL;
  }

  X509 *cert = SSL_get_peer_certificate(ssl);
  if (!cert) {
    SSL_shutdown(ssl); SSL_free(ssl); close(sock); SSL_CTX_free(ctx);
    return NULL;
  }

  cJSON *cert_info = cJSON_CreateObject();
  cJSON_AddStringToObject(cert_info, "host", hostname);
  cJSON_AddNumberToObject(cert_info, "port", port);
  cJSON_AddStringToObject(cert_info, "protocol", SSL_get_version(ssl));
  const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);
  if (cipher) {
    cJSON_AddStringToObject(cert_info, "cipher", SSL_CIPHER_get_name(cipher));
    cJSON_AddNumberToObject(cert_info, "cipher_bits", SSL_CIPHER_get_bits(cipher, NULL));
  }

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

  X509_free(cert);
  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(sock);
  SSL_CTX_free(ctx);
  return cert_info;
}

/* Emit the leaf cert as one per-record item. Returns 1 if emitted. */
static int emit_leaf(intel_sink *sink, const char *host, cJSON *cert /*owned*/) {
  char *bj = cJSON_PrintUnformatted(cert);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SSL_ANALYZER");
  cJSON_AddStringToObject(props, "host", host);
  cJSON *st = cJSON_GetObjectItem(cert, "status");
  if (st && cJSON_IsString(st)) cJSON_AddStringToObject(props, "status", st->valuestring);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[360];
  snprintf(rk, sizeof rk, "cert:%s", host);
  cJSON *iss = cJSON_GetObjectItem(cert, "issuer");
  snprintf(title, sizeof title, "TLS certificate — %s", host);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = (iss && cJSON_IsString(iss)) ? iss->valuestring : "leaf certificate";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SSL_ANALYZER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit one crt.sh historical cert. Returns 1 if emitted. */
static int emit_ct_cert(intel_sink *sink, const char *host, cJSON *cert) {
  cJSON *id_j = cJSON_GetObjectItem(cert, "id");
  if (!id_j) return 0;
  long long cert_id = (long long)id_j->valuedouble;

  cJSON *cn   = cJSON_GetObjectItem(cert, "common_name");
  cJSON *nv   = cJSON_GetObjectItem(cert, "name_value");
  cJSON *iss  = cJSON_GetObjectItem(cert, "issuer_name");
  cJSON *ser  = cJSON_GetObjectItem(cert, "serial_number");
  cJSON *nb   = cJSON_GetObjectItem(cert, "not_before");
  cJSON *na   = cJSON_GetObjectItem(cert, "not_after");

  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "host", host);
  cJSON_AddNumberToObject(body, "ct_log_id", (double)cert_id);
  if (cn && cJSON_IsString(cn))  cJSON_AddStringToObject(body, "common_name", cn->valuestring);
  if (nv && cJSON_IsString(nv))  cJSON_AddStringToObject(body, "name_value", nv->valuestring);
  if (iss && cJSON_IsString(iss)) cJSON_AddStringToObject(body, "issuer", iss->valuestring);
  if (ser && cJSON_IsString(ser)) cJSON_AddStringToObject(body, "serial_number", ser->valuestring);
  if (nb && cJSON_IsString(nb))  cJSON_AddStringToObject(body, "valid_from", nb->valuestring);
  if (na && cJSON_IsString(na))  cJSON_AddStringToObject(body, "valid_until", na->valuestring);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SSL_ANALYZER");
  cJSON_AddStringToObject(props, "host", host);
  cJSON_AddStringToObject(props, "kind", "ct_log");
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320], title[360];
  snprintf(rk, sizeof rk, "ctcert:%s:%lld", host, cert_id);
  snprintf(title, sizeof title, "CT cert — %s",
           (cn && cJSON_IsString(cn)) ? cn->valuestring : host);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = (iss && cJSON_IsString(iss)) ? iss->valuestring : "CT log certificate";
  it.published_at    = (nb && cJSON_IsString(nb)) ? nb->valuestring : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SSL_ANALYZER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *target = ctx->entity;
  if (!target || !*target) return 0;

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

  /* Handshake first: on failure emit nothing. */
  cJSON *leaf = analyze_ssl_direct(hostname, port);
  if (!leaf) return 0;
  emit_leaf(sink, hostname, leaf);
  cJSON_Delete(leaf);

  /* crt.sh historical certs → one item each. */
  char *enc = jo_urlencode(hostname);
  if (enc) {
    char url[512];
    snprintf(url, sizeof url, "https://crt.sh/?q=%s&output=json", enc);
    free(enc);
    cJSON *json = feed_get_json(ctx->http, url, 30000);
    if (json && cJSON_IsArray(json)) {
      int total = cJSON_GetArraySize(json);
      for (int i = 0; i < total && i < 100; i++) {
        cJSON *cert = cJSON_GetArrayItem(json, i);
        if (cert) emit_ct_cert(sink, hostname, cert);
      }
    }
    if (json) cJSON_Delete(json);
  }
  return 0;
}

static const source_def ssl_analyzer_def = {
  .id = "SSL_ANALYZER", .collector = "osint",
  .name = "SSL Analyzer", .name_ja = "SSL解析",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/ssl-analyzer",
  .description = "Analyze a host's TLS certificate plus crt.sh CT data.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ssl_analyzer_def)
