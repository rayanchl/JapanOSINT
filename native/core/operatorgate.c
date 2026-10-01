#include "operatorgate.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>

/* ── requirePlatformOperator ──────────────────────────────────────────── */
static int csv_has(const char *csv, const char *needle, int lower) {
  if (!csv || !*csv || !needle || !*needle) return 0;
  char buf[4096]; snprintf(buf, sizeof buf, "%s", csv);
  /* strtok_r: this is an AUTHORIZATION check running on the HTTP thread while
   * collector threads tokenize concurrently. Sharing strtok's global cursor
   * meant a concurrent call could corrupt the operator-list walk. */
  char *save = NULL;
  for (char *p = strtok_r(buf, ",", &save); p; p = strtok_r(NULL, ",", &save)) {
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n && (p[n-1]==' '||p[n-1]=='\t')) p[--n]=0;
    if (!*p) continue;
    if (lower) { for (char *q=p; *q; q++) *q=tolower((unsigned char)*q); }
    if (strcmp(p, needle) == 0) return 1;
  }
  return 0;
}
int opgate_check(const auth_user *u) {
  if (!u || !u->id[0]) return -401;
  const char *em = getenv("PLATFORM_OPERATOR_EMAILS");
  const char *id = getenv("PLATFORM_OPERATOR_IDS");
  int has_cfg = (em && *em) || (id && *id);
  if (!has_cfg) return -403 - 1000;            /* not configured */
  /* The email arm requires a VERIFIED address. An `email` claim is whatever the
   * IdP was told at sign-up, and matching an unverified one turns "I can sign
   * up as ops@example.com" into "I am a platform operator" — which unlocks
   * /api/admin, the DB explorer, the breach corpus and the cleartext key
   * reveal. auth.c grew auth_email_verified() for exactly this hazard, and
   * tenantapi.c applied it to the tenant-invite path; the strictly
   * higher-privilege gate here was left matching the raw claim.
   *
   * The id arm is unaffected: `sub` is the IdP's own subject identifier, not
   * something a signup can choose. An operator listed by id still works when
   * their address is unconfirmed. */
  extern int auth_email_verified(void);
  char le[256]; snprintf(le, sizeof le, "%s", u->email);
  for (char *p=le; *p; p++) *p=tolower((unsigned char)*p);
  if (le[0] && auth_email_verified() && csv_has(em, le, 1)) return 0;
  if (csv_has(id, u->id, 0)) return 0;
  return -403;
}

/* ── /api/db ──────────────────────────────────────────────────────────── */
