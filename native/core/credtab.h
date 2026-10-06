/* core/credtab.h — apiCredentials.js CREDENTIALS table, shared by statusapi
 * (getCredentialStatus) and keysapi (getAllKnownVarNames). */
#ifndef JO_CREDTAB_H
#define JO_CREDTAB_H

typedef struct { const char *id; const char *req[3], *any[4], *opt[4]; } cred_def;

const cred_def *cred_get(const char *id);
int             cred_alen(const char *const *a);   /* NULL-terminated len */

/* getAllKnownVarNames(): unique var names with most-restrictive role
 * (required>anyOf>optional), sorted by that rank then name. Fills caller
 * arrays; returns the count. role ∈ "required"|"anyOf"|"optional".
 *
 * Returns -1 — loudly, on stderr — if the names do not fit in `max`. It used to
 * keep the first `max` in TABLE order and drop the rest without a word: every
 * caller passed 64, 89 names exist, and 25 keys (NVD_API_KEY among them) could
 * never be listed or set ("Unknown key"). Size the arrays with
 * cred_known_capacity(), which is derived from the table and cannot be
 * outgrown. */
int cred_known_vars(const char **names, const char **roles, int max);

/* An upper bound on cred_known_vars()'s count: every name slot in the table,
 * duplicates included. */
int cred_known_capacity(void);

#endif
