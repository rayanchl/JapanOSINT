/* lib/joenv.h — the ONE typed environment reader.
 *
 * Every numeric / boolean knob read from the environment goes through here so
 * that "unset", "empty", "garbage" and "out of range" mean the same thing
 * everywhere (they used to mean something different in each of ~40 private
 * env_int/env_long copies: atoi accepting "12abc", strtol accepting "abc" as 0,
 * a bound enforced here and not there).
 *
 *   unset or empty            -> the default, silently
 *   non-numeric / trailing
 *   garbage / NaN / overflow
 *   of the target type        -> the default, ONE stderr line per variable name
 *   outside [min, max]        -> jo_env_*        : clamped to the bound
 *                                jo_env_*_strict : the default
 *                                (both: ONE stderr line per variable name)
 *
 * Trailing whitespace is tolerated ("30\n" from a shell). A leading sign is
 * accepted. The reader calls getenv() on every call and caches nothing, so a
 * test that setenv()s between calls sees the new value; the only state is the
 * set of names already complained about (mutex-protected). Dependency-free:
 * no DB, no cJSON, no logging layer. Callers pick the bound; pass LONG_MAX /
 * INT_MAX / DBL_MAX when a knob genuinely has none.
 *
 * Presence flags (`getenv(JO_NO_...) != NULL`, where "=0" still means ON) are a
 * different contract and stay as they are; jo_env_bool is for "=0"/"=false"
 * knobs. Credentials are not read here (key_env / credtab). */
#ifndef JO_JOENV_H
#define JO_JOENV_H

#include <float.h>   /* DBL_MAX for callers with no upper bound */
#include <limits.h>  /* LONG_MAX, INT_MAX, ... */

long long jo_env_ll(const char *name, long long def, long long min, long long max);
long long jo_env_ll_strict(const char *name, long long def, long long min, long long max);
long      jo_env_long(const char *name, long def, long min, long max);
long      jo_env_long_strict(const char *name, long def, long min, long max);
int       jo_env_int(const char *name, int def, int min, int max);
int       jo_env_int_strict(const char *name, int def, int min, int max);
double    jo_env_double(const char *name, double def, double min, double max);
double    jo_env_double_strict(const char *name, double def, double min, double max);

/* 1/true/yes/on/y/t -> 1; 0/false/no/off/n/f -> 0 (case-insensitive).
 * Unset/empty -> def; any other spelling -> def plus the one-time line. */
int         jo_env_bool(const char *name, int def);

/* Non-empty value, else def (def may be NULL). Pointer is getenv's own. */
const char *jo_env_str(const char *name, const char *def);

#endif
