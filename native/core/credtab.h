/* core/credtab.h — apiCredentials.js CREDENTIALS table, shared by statusapi
 * (getCredentialStatus) and keysapi (getAllKnownVarNames). */
#ifndef JO_CREDTAB_H
#define JO_CREDTAB_H

typedef struct { const char *id; const char *req[3], *any[4], *opt[4]; } cred_def;

const cred_def *cred_get(const char *id);
int             cred_alen(const char *const *a);   /* NULL-terminated len */

/* getAllKnownVarNames(): unique var names with most-restrictive role
 * (required>anyOf>optional), sorted by that rank then name. Fills caller
 * arrays; returns the count (≤ max). role ∈ "required"|"anyOf"|"optional". */
int cred_known_vars(const char **names, const char **roles, int max);

#endif
