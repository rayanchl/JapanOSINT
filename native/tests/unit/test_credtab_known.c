/* test_credtab_known.c — every credential the table names can be listed.
 *
 * keysapi.c built its listings (GET /api/keys, GET /api/tenant-keys) and its
 * "is this a known key?" check from cred_known_vars(N, R, 64). The table names
 * more than 64 distinct variables and the function silently kept the first 64
 * in table order, so 25 keys — NVD_API_KEY among them — were absent from both
 * lists and answered "Unknown key" to GET and PUT: a credential the code needs
 * that the API could never accept.
 *
 * Holds: the capacity covers the table, the full list includes the names that
 * used to fall off, the list is unique, and a caller that passes too small a
 * bound gets -1, not a quietly shortened list. */
#include "core/credtab.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  int cap = cred_known_capacity();
  assert(cap > 0);
  const char **N = calloc((size_t) cap, sizeof *N), **R = calloc((size_t) cap, sizeof *R);
  assert(N && R);
  int n = cred_known_vars(N, R, cap);
  printf("  %d distinct credential names (capacity %d)\n", n, cap);
  assert(n > 64 && "the table has outgrown the old fixed bound; this test is about that");
  int nvd = 0;
  for (int i = 0; i < n; i++) {
    assert(N[i] && R[i]);
    if (!strcmp(N[i], "NVD_API_KEY")) nvd = 1;
    for (int j = i + 1; j < n; j++) assert(strcmp(N[i], N[j]) != 0 && "duplicate name");
  }
  assert(nvd && "NVD_API_KEY is in the table but not in the list");

  /* too small a bound is an error, not a truncation */
  assert(cred_known_vars(N, R, 64) == -1);
  printf("  a 64-entry bound is refused (-1), not truncated: ok\n");
  free(N); free(R);
  printf("test_credtab_known: ok\n");
  return 0;
}
