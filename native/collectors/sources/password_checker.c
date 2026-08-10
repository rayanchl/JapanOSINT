/* collectors/osint/sources/password_checker.c
 * OSINT service — faithful port of OSINTsaas osint_tools/password_checker.c
 * (handle_password_checker → password_check). Canonical SERVICE name in
 * osint_dispatcher.c service_registry[] bound to handle_password_checker is
 * "PASSWORD_CHECKER" (line 324). On-demand (interval 0); the OSINT dispatcher
 * runs it with ctx->entity = a password.
 *
 * Reproduces password_check(): strength analysis (entropy, char classes,
 * repeating/sequential, common-pass list as a SCORING INPUT only, 0-100 score
 * + rating), HaveIBeenPwned k-anonymity range check (only the first 5 SHA-1 hex
 * chars are sent — https://api.pwnedpasswords.com/range/<prefix>, no API key),
 * enterprise policy check, and a recommendation computed from the real
 * score/breach data.
 *
 * PER-RECORD EMIT: emits ONE intel row keyed "pw:<sha1prefix>" (only the first
 * 5 SHA-1 hex chars — the password is never echoed into a key). body = the real
 * computed entropy/strength + the real HIBP breach_count. No
 * {success,confidence,data} envelope.
 *
 * PRIVACY (deliberate deviation from upstream — breach-check-pipeline.md §7):
 * the submitted password's FULL SHA-1/SHA-256/MD5 are NOT emitted. A SHA-1 or
 * MD5 of a password is plaintext-equivalent (rainbow-tableable), so persisting
 * it into intel_items would defeat the whole hash-only model. Only the 5-char
 * SHA-1 k-anonymity prefix is surfaced. The hardcoded security_tips advisory
 * list has been removed (COMMON_PASSWORDS is kept as a scoring input only). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/sha.h>

#define HIBP_PASSWORD_URL "https://api.pwnedpasswords.com/range/%s"

/* top-20 common passwords — identical list/order to upstream */
static const char *COMMON_PASSWORDS[] = {
  "123456", "password", "123456789", "12345678", "12345",
  "1234567", "qwerty", "abc123", "password1", "111111",
  "123123", "admin", "letmein", "welcome", "monkey",
  "1234567890", "password123", "iloveyou", "sunshine", "princess",
  NULL
};

#define HAS_LOWER   0x01
#define HAS_UPPER   0x02
#define HAS_DIGIT   0x04
#define HAS_SPECIAL 0x08

static char *sha1_hash(const char *password) {
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1((const unsigned char *)password, strlen(password), hash);
  char *hex = calloc(SHA_DIGEST_LENGTH * 2 + 1, 1);
  if (!hex) return NULL;
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    sprintf(hex + (i * 2), "%02X", hash[i]);
  return hex;
}

/* SHA-256/MD5 of the submitted password were removed with the full-digest
 * emit (breach-check-pipeline.md §7): a password digest is plaintext-equivalent
 * and must never be persisted. Only the SHA-1 k-anon prefix is used, below. */

static int is_common_password(const char *password) {
  if (!password) return 0;
  for (int i = 0; COMMON_PASSWORDS[i]; i++)
    if (strcasecmp(password, COMMON_PASSWORDS[i]) == 0) return 1;
  return 0;
}

static cJSON *analyze_strength(const char *password) {
  cJSON *strength = cJSON_CreateObject();
  if (!password) {
    cJSON_AddStringToObject(strength, "error", "No password provided");
    return strength;
  }

  size_t length = strlen(password);
  cJSON_AddNumberToObject(strength, "length", length);

  int char_classes = 0;
  int lowercase_count = 0, uppercase_count = 0, digit_count = 0, special_count = 0;
  int has_repeating = 0, has_sequential = 0;

  for (size_t i = 0; i < length; i++) {
    char c = password[i];
    if (islower((unsigned char)c)) { lowercase_count++; char_classes |= HAS_LOWER; }
    else if (isupper((unsigned char)c)) { uppercase_count++; char_classes |= HAS_UPPER; }
    else if (isdigit((unsigned char)c)) { digit_count++; char_classes |= HAS_DIGIT; }
    else { special_count++; char_classes |= HAS_SPECIAL; }

    if (i >= 2 && password[i] == password[i-1] && password[i-1] == password[i-2])
      has_repeating = 1;
    if (i >= 2) {
      if ((password[i] == password[i-1] + 1 && password[i-1] == password[i-2] + 1) ||
          (password[i] == password[i-1] - 1 && password[i-1] == password[i-2] - 1))
        has_sequential = 1;
    }
  }

  cJSON *composition = cJSON_CreateObject();
  cJSON_AddNumberToObject(composition, "lowercase", lowercase_count);
  cJSON_AddNumberToObject(composition, "uppercase", uppercase_count);
  cJSON_AddNumberToObject(composition, "digits", digit_count);
  cJSON_AddNumberToObject(composition, "special", special_count);
  cJSON_AddItemToObject(strength, "composition", composition);

  int class_count = 0;
  if (char_classes & HAS_LOWER) class_count++;
  if (char_classes & HAS_UPPER) class_count++;
  if (char_classes & HAS_DIGIT) class_count++;
  if (char_classes & HAS_SPECIAL) class_count++;
  cJSON_AddNumberToObject(strength, "character_classes", class_count);

  cJSON *weaknesses = cJSON_CreateArray();
  if (has_repeating)
    cJSON_AddItemToArray(weaknesses, cJSON_CreateString("Contains repeating characters"));
  if (has_sequential)
    cJSON_AddItemToArray(weaknesses, cJSON_CreateString("Contains sequential characters"));
  if (length < 8)
    cJSON_AddItemToArray(weaknesses, cJSON_CreateString("Too short (minimum 8 characters)"));
  if (class_count < 3)
    cJSON_AddItemToArray(weaknesses, cJSON_CreateString("Limited character variety"));
  if (!(char_classes & HAS_SPECIAL))
    cJSON_AddItemToArray(weaknesses, cJSON_CreateString("No special characters"));
  cJSON_AddItemToObject(strength, "weaknesses", weaknesses);

  int charset_size = 0;
  if (char_classes & HAS_LOWER) charset_size += 26;
  if (char_classes & HAS_UPPER) charset_size += 26;
  if (char_classes & HAS_DIGIT) charset_size += 10;
  if (char_classes & HAS_SPECIAL) charset_size += 32;

  double entropy = length * log2(charset_size > 0 ? charset_size : 1);
  cJSON_AddNumberToObject(strength, "entropy_bits", entropy);

  int score = 0;
  if (length >= 16) score += 30;
  else if (length >= 12) score += 25;
  else if (length >= 10) score += 20;
  else if (length >= 8) score += 15;
  else score += (int)length * 2;

  score += class_count * 10;

  if (entropy >= 60) score += 20;
  else if (entropy >= 45) score += 15;
  else if (entropy >= 30) score += 10;
  else score += 5;

  if (has_repeating) score -= 10;
  if (has_sequential) score -= 10;
  if (is_common_password(password)) score -= 50;

  if (score < 0) score = 0;
  if (score > 100) score = 100;
  cJSON_AddNumberToObject(strength, "score", score);

  const char *rating;
  if (score >= 80) rating = "STRONG";
  else if (score >= 60) rating = "GOOD";
  else if (score >= 40) rating = "FAIR";
  else if (score >= 20) rating = "WEAK";
  else rating = "VERY_WEAK";
  cJSON_AddStringToObject(strength, "rating", rating);

  cJSON_AddBoolToObject(strength, "is_common_password", is_common_password(password));
  return strength;
}

/* HIBP k-anonymity: send only first 5 SHA-1 hex chars; scan SUFFIX:COUNT body */
static cJSON *check_hibp(http_client *http, const char *password) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "HaveIBeenPwned");

  char *hash = sha1_hash(password);
  if (!hash) {
    cJSON_AddStringToObject(result, "error", "Hash calculation failed");
    return result;
  }

  char prefix[6] = {0};
  strncpy(prefix, hash, 5);
  char *suffix = hash + 5;

  char url[256];
  snprintf(url, sizeof url, HIBP_PASSWORD_URL, prefix);

  char *body = feed_get_text(http, url, 30000);
  if (!body) {
    free(hash);
    cJSON_AddStringToObject(result, "error", "API request failed");
    return result;
  }

  int breach_count = 0;
  char *save = NULL;
  char *line = strtok_r(body, "\r\n", &save);
  while (line) {
    char *colon = strchr(line, ':');
    if (colon) {
      *colon = '\0';
      if (strcasecmp(line, suffix) == 0) { breach_count = atoi(colon + 1); break; }
    }
    line = strtok_r(NULL, "\r\n", &save);
  }
  free(body);
  free(hash);

  cJSON_AddNumberToObject(result, "breach_count", breach_count);
  cJSON_AddBoolToObject(result, "compromised", breach_count > 0);

  if (breach_count > 0) {
    cJSON_AddStringToObject(result, "status", "COMPROMISED");
    char msg[128];
    snprintf(msg, sizeof msg,
             "Password found in %d data breaches - DO NOT USE", breach_count);
    cJSON_AddStringToObject(result, "warning", msg);
  } else {
    cJSON_AddStringToObject(result, "status", "NOT_FOUND");
    cJSON_AddStringToObject(result, "note",
      "Not found in known breaches (does not guarantee safety)");
  }
  return result;
}

static cJSON *check_policy(const char *password, int min_length,
                           int require_upper, int require_lower,
                           int require_digit, int require_special) {
  cJSON *policy = cJSON_CreateObject();
  size_t length = strlen(password);
  int has_upper = 0, has_lower = 0, has_digit = 0, has_special = 0;
  for (size_t i = 0; i < length; i++) {
    if (isupper((unsigned char)password[i])) has_upper = 1;
    else if (islower((unsigned char)password[i])) has_lower = 1;
    else if (isdigit((unsigned char)password[i])) has_digit = 1;
    else has_special = 1;
  }

  cJSON *checks = cJSON_CreateObject();
  int all_passed = 1;

  int len_ok = (int)length >= min_length;
  cJSON_AddBoolToObject(checks, "minimum_length", len_ok);
  if (!len_ok) all_passed = 0;

  if (require_upper) {
    cJSON_AddBoolToObject(checks, "uppercase", has_upper);
    if (!has_upper) all_passed = 0;
  }
  if (require_lower) {
    cJSON_AddBoolToObject(checks, "lowercase", has_lower);
    if (!has_lower) all_passed = 0;
  }
  if (require_digit) {
    cJSON_AddBoolToObject(checks, "digit", has_digit);
    if (!has_digit) all_passed = 0;
  }
  if (require_special) {
    cJSON_AddBoolToObject(checks, "special_character", has_special);
    if (!has_special) all_passed = 0;
  }

  cJSON_AddItemToObject(policy, "checks", checks);
  cJSON_AddBoolToObject(policy, "compliant", all_passed);
  return policy;
}

static int emit_result(intel_sink *sink, const char *sha1prefix,
                       const char *rating, cJSON *root /*owned*/) {
  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PASSWORD_CHECKER");
  if (rating) cJSON_AddStringToObject(props, "rating", rating);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[32], title[64];
  /* key on the SHA-1 prefix only — never echo the password into a key */
  snprintf(rk, sizeof rk, "pw:%s", sha1prefix && *sha1prefix ? sha1prefix : "00000");
  snprintf(title, sizeof title, "password strength: %s",
           rating ? rating : "checked");

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "password security check";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PASSWORD_CHECKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *password = ctx->entity;
  if (!password || !*password) return 0;

  cJSON *root = cJSON_CreateObject();
  time_t ts = time(NULL);
  cJSON_AddNumberToObject(root, "timestamp", (double)ts);
  cJSON_AddStringToObject(root, "query_type", "password_security_check");
  cJSON_AddNumberToObject(root, "password_length", strlen(password));

  cJSON *strength = analyze_strength(password);
  cJSON_AddItemToObject(root, "strength_analysis", strength);

  cJSON *hibp = check_hibp(ctx->http, password);
  cJSON_AddItemToObject(root, "breach_check", hibp);

  /* Only the 5-char SHA-1 k-anonymity prefix is surfaced. The full digest of a
   * submitted password is plaintext-equivalent and is never emitted or stored
   * (breach-check-pipeline.md §7). */
  cJSON *hashes = cJSON_CreateObject();
  char *sha1 = sha1_hash(password);
  char sha1prefix[6] = {0};
  if (sha1) { strncpy(sha1prefix, sha1, 5); free(sha1); }
  cJSON_AddStringToObject(hashes, "sha1_prefix", sha1prefix);
  cJSON_AddStringToObject(hashes, "note",
    "Only the SHA-1 k-anonymity prefix is stored; full digests are withheld.");
  cJSON_AddItemToObject(root, "hashes", hashes);

  cJSON *policy = check_policy(password, 8, 1, 1, 1, 1);
  cJSON_AddItemToObject(root, "enterprise_policy_check", policy);

  cJSON *recommendation = cJSON_CreateObject();
  cJSON *compromised = cJSON_GetObjectItem(hibp, "compromised");
  cJSON *score = cJSON_GetObjectItem(strength, "score");
  int is_compromised = compromised && cJSON_IsTrue(compromised);
  int strength_score = score ? (int)score->valuedouble : 0;

  if (is_compromised) {
    cJSON_AddStringToObject(recommendation, "verdict", "DO_NOT_USE");
    cJSON_AddStringToObject(recommendation, "reason",
      "Password found in data breaches - choose a new password");
  } else if (strength_score < 40) {
    cJSON_AddStringToObject(recommendation, "verdict", "WEAK");
    cJSON_AddStringToObject(recommendation, "reason",
      "Password is too weak - add length and character variety");
  } else if (strength_score < 60) {
    cJSON_AddStringToObject(recommendation, "verdict", "ACCEPTABLE");
    cJSON_AddStringToObject(recommendation, "reason",
      "Password is acceptable but could be stronger");
  } else {
    cJSON_AddStringToObject(recommendation, "verdict", "GOOD");
    cJSON_AddStringToObject(recommendation, "reason",
      "Password appears secure (still use unique passwords per site)");
  }
  cJSON_AddItemToObject(root, "recommendation", recommendation);

  cJSON *rating = cJSON_GetObjectItem(strength, "rating");
  const char *rating_s = (rating && cJSON_IsString(rating)) ? rating->valuestring : NULL;
  return emit_result(sink, sha1prefix, rating_s, root);
}

static const source_def password_checker_def = {
  .id = "PASSWORD_CHECKER", .collector = "osint",
  .name = "Password Checker", .name_ja = "パスワードチェッカー",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/password-checker",
  .description = "Check if a password appears in breaches (HIBP k-anon).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(password_checker_def)
