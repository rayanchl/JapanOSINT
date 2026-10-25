/* tests/unit/test_http_tls_cause.c — which TLS failure, told apart.
 * core/httpclient.c, no network.
 *
 * WHAT IS BEING PROTECTED (open item D1-bis). A refused fetch used to disclose
 * "the request failed at the transport level", and `docs/tls-intermediates-
 * 2026-10-21.md` is what that cost: of 30 hosts written off as "TLS refused",
 * **24 were a missing intermediate** — a public CA had signed the leaf and the
 * server simply did not send the link — one was a different hostname, one was
 * this container's own egress, and only two were genuinely unreachable. Four
 * findings, four remedies, one message. So:
 *
 *   1. the OpenSSL verify result decides, for every code this tree has seen;
 *   2. the verify result OUTRANKS the message text, because a reworded or
 *      translated libcurl string must not silently reclassify a host;
 *   3. the text is read only where there is no verify result to read — a
 *      hostname mismatch arrives that way on some backends;
 *   4. a handshake refused BEFORE any certificate was judged is not a trust
 *      finding, and must not be reported as one;
 *   5. `label` and `remedy` answer for every cause, so a stored record never
 *      carries an empty string where a sentence belongs — including for a
 *      cause a future OpenSSL names that this build does not.
 *
 * Includes httpclient.c to reach the static classifier. */

#include "../../core/httpclient.c"

#include <assert.h>
#include <string.h>

/* The real libcurl sentences, as this tree has logged them. */
#define M_ISSUER  "SSL certificate problem: unable to get local issuer certificate"
#define M_EXPIRED "SSL certificate problem: certificate has expired"
#define M_HOST    "SSL: no alternative certificate subject name matches target host name 'x.example'"
#define M_HOST2   "SSL: certificate subject name 'a.example' does not match target host name 'b.example'"
#define M_SELF    "SSL certificate problem: self signed certificate in certificate chain"
#define M_CIPHER  "OpenSSL/3.0.13: error:0A000102:SSL routines::unsupported protocol"
#define M_SNI     "OpenSSL/3.0.13: error:0A000458:SSL routines::tlsv1 unrecognized name"

/* 1. The verify result decides. */
static void test_verify_result_decides(void) {
  const CURLcode v = CURLE_PEER_FAILED_VERIFICATION;
  assert(tls_cause_of(v, X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY, "") ==
         HTTP_TLS_NO_LOCAL_ISSUER);
  assert(tls_cause_of(v, X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT, "") ==
         HTTP_TLS_NO_LOCAL_ISSUER);
  assert(tls_cause_of(v, X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE, "") ==
         HTTP_TLS_NO_LOCAL_ISSUER);
  assert(tls_cause_of(v, X509_V_ERR_CERT_HAS_EXPIRED, "") == HTTP_TLS_EXPIRED);
  assert(tls_cause_of(v, X509_V_ERR_CERT_NOT_YET_VALID, "") == HTTP_TLS_NOT_YET_VALID);
  assert(tls_cause_of(v, X509_V_ERR_HOSTNAME_MISMATCH, "") == HTTP_TLS_HOSTNAME_MISMATCH);
  assert(tls_cause_of(v, X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT, "") == HTTP_TLS_SELF_SIGNED);
  assert(tls_cause_of(v, X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN, "") == HTTP_TLS_SELF_SIGNED);
  assert(tls_cause_of(v, X509_V_ERR_CERT_REVOKED, "") == HTTP_TLS_REVOKED);
}

/* 2. And it outranks the text. This is the guarantee that makes the classifier
 *    safe against a libcurl whose wording changes: the two disagree here on
 *    purpose, and the verify result must win every time. */
static void test_verify_outranks_text(void) {
  const CURLcode v = CURLE_PEER_FAILED_VERIFICATION;
  assert(tls_cause_of(v, X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY, M_EXPIRED) ==
         HTTP_TLS_NO_LOCAL_ISSUER);
  assert(tls_cause_of(v, X509_V_ERR_CERT_HAS_EXPIRED, M_ISSUER) == HTTP_TLS_EXPIRED);
  assert(tls_cause_of(v, X509_V_ERR_HOSTNAME_MISMATCH, M_SELF) ==
         HTTP_TLS_HOSTNAME_MISMATCH);
  /* A message in another language, with a verify result: still correct. */
  assert(tls_cause_of(v, X509_V_ERR_CERT_HAS_EXPIRED,
                      "Problème de certificat SSL : certificat expiré") ==
         HTTP_TLS_EXPIRED);
}

/* 3. With no verify result (X509_V_OK is 0, which is also "nothing reported"),
 *    the sentence is all there is. */
static void test_text_fallback(void) {
  const CURLcode v = CURLE_PEER_FAILED_VERIFICATION;
  assert(tls_cause_of(v, X509_V_OK, M_HOST)  == HTTP_TLS_HOSTNAME_MISMATCH);
  assert(tls_cause_of(v, X509_V_OK, M_HOST2) == HTTP_TLS_HOSTNAME_MISMATCH);
  assert(tls_cause_of(v, X509_V_OK, M_ISSUER)  == HTTP_TLS_NO_LOCAL_ISSUER);
  assert(tls_cause_of(v, X509_V_OK, M_EXPIRED) == HTTP_TLS_EXPIRED);
  assert(tls_cause_of(v, X509_V_OK, M_SELF)    == HTTP_TLS_SELF_SIGNED);
  /* "self-signed" with the hyphen, which newer OpenSSL writes. */
  assert(tls_cause_of(v, X509_V_OK,
                      "SSL certificate problem: self-signed certificate") ==
         HTTP_TLS_SELF_SIGNED);
  /* A bare "hostname" is deliberately NOT a hostname-mismatch cue: it occurs
   * in resolver and proxy messages that say nothing about a certificate. */
  assert(tls_cause_of(v, X509_V_OK, "Could not resolve hostname") != HTTP_TLS_HOSTNAME_MISMATCH);
}

/* 4. A refusal before verification is not a trust finding. */
static void test_handshake_is_not_trust(void) {
  assert(tls_cause_of(CURLE_SSL_CONNECT_ERROR, X509_V_OK, M_CIPHER) == HTTP_TLS_HANDSHAKE);
  assert(tls_cause_of(CURLE_SSL_CIPHER, X509_V_OK, "") == HTTP_TLS_HANDSHAKE);
  /* …and an unrecognised SNI name is told apart from a cipher mismatch even
   * though libcurl reports both as CURLE 35, because the two want opposite
   * remedies: one is "re-point the row", the other is "lower the cipher
   * security level". Measured message, 2026-10-23. */
  assert(tls_cause_of(CURLE_SSL_CONNECT_ERROR, X509_V_OK, M_SNI) ==
         HTTP_TLS_UNRECOGNIZED_NAME);
  /* …and a verification failure with nothing to go on is "other", never
   * "handshake" and never one of the four named causes: guessing which
   * certificate problem it was is the fabrication these records exist to
   * prevent. */
  assert(tls_cause_of(CURLE_PEER_FAILED_VERIFICATION, X509_V_OK, "") == HTTP_TLS_OTHER);
  assert(tls_cause_of(CURLE_SSL_CERTPROBLEM, X509_V_OK, "something new") == HTTP_TLS_OTHER);
}

/* 5. Both strings answer for every cause, and only HTTP_TLS_NONE is empty. */
static void test_label_and_remedy_total(void) {
  const int named[] = { HTTP_TLS_NO_LOCAL_ISSUER, HTTP_TLS_EXPIRED,
                        HTTP_TLS_NOT_YET_VALID, HTTP_TLS_HOSTNAME_MISMATCH,
                        HTTP_TLS_SELF_SIGNED, HTTP_TLS_REVOKED,
                        HTTP_TLS_UNRECOGNIZED_NAME, HTTP_TLS_HANDSHAKE,
                        HTTP_TLS_OTHER };
  for (size_t i = 0; i < sizeof named / sizeof *named; i++) {
    assert(strlen(http_tls_cause_label(named[i]))  > 20);
    assert(strlen(http_tls_cause_remedy(named[i])) > 20);
  }
  assert(http_tls_cause_label(HTTP_TLS_NONE)[0]  == 0);
  assert(http_tls_cause_remedy(HTTP_TLS_NONE)[0] == 0);
  /* A value no build of this file knows must still be a sentence, not a crash
   * and not an empty record field. */
  assert(http_tls_cause_label(9999)[0]  == 0);
  assert(http_tls_cause_remedy(9999)[0] == 0);
  /* The one remedy that must name the file that fixes it, because it is the
   * only cause of the eight that is ours to fix. */
  assert(strstr(http_tls_cause_remedy(HTTP_TLS_NO_LOCAL_ISSUER),
                "extra-intermediates.pem") != NULL);
  /* And the one remedy that must NOT: a self-signed certificate cannot be
   * completed by an intermediate, so pointing a reader at that file would send
   * them to widen trust for a host no CA vouches for. */
  assert(strstr(http_tls_cause_remedy(HTTP_TLS_SELF_SIGNED), "unreachable") != NULL);
  /* It DOES name extra-intermediates.pem — to say it is the wrong lever here,
   * which is worth more than leaving a reader to try it. The assertion that
   * matters is that the sentence says so. */
  assert(strstr(http_tls_cause_remedy(HTTP_TLS_SELF_SIGNED), "wrong lever") != NULL);
  /* A hostname remedy must never suggest not checking the hostname. */
  assert(strstr(http_tls_cause_remedy(HTTP_TLS_HOSTNAME_MISMATCH), "re-point") != NULL);
  /* An unrecognised SNI name must not be told to lower a cipher level — the
   * knob that cannot move. */
  assert(strstr(http_tls_cause_remedy(HTTP_TLS_UNRECOGNIZED_NAME), "cipher level") != NULL);
  assert(strstr(http_tls_cause_remedy(HTTP_TLS_UNRECOGNIZED_NAME), "jo_tls.py") == NULL);
}

/* 6. Nothing has failed yet on this thread, so there is no TLS verdict to
 *    report. A caller that skipped the transport-error check would otherwise
 *    attach a certificate finding to a successful fetch. */
static void test_default_is_none(void) {
  assert(http_last_tls_cause() == HTTP_TLS_NONE);
}

int main(void) {
  test_default_is_none();
  test_verify_result_decides();
  test_verify_outranks_text();
  test_text_fallback();
  test_handshake_is_not_trust();
  test_label_and_remedy_total();
  printf("test_http_tls_cause: OK\n");
  return 0;
}
