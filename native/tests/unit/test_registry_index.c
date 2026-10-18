/* test_registry_index.c — the registry's hash index answers exactly what the
 * linear scans it replaced answered.
 *
 * registry_get() was a linear strcmp over 18,643 sources and the duplicate
 * check inside registry_add() was O(n^2) across the constructors; with
 * db_seed_sources() calling src_meta_get() per source, `--list-sources` took
 * ~2.4 s, nearly all of it in strcmp. The index must not change a single
 * answer to buy that back:
 *   - every registered id resolves to its FIRST definition, as before;
 *   - an unknown id (and NULL) resolves to nothing;
 *   - a duplicate id is still reported with the message the registry-floor
 *     gate greps for ("DUPLICATE id"), and still counted;
 *   - src_meta_get()'s curated-table lookup agrees with gen_meta_get(). */
#include "source.h"
#include "core/source_registry.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const src_meta *gen_meta_get(const char *id);
const src_meta *gen_meta_at(int i);
int            gen_meta_count(void);

static int run_nop(const source_ctx *c, intel_sink *s) { (void)c; (void)s; return 0; }
/* Two definitions of one id: the second must be reported and unreachable. */
static const source_def D_DUP_A = { .id = "UNIT_REG_DUP", .collector = "unit_a",
  .name = "a", .run = run_nop, .category = "test" };
static const source_def D_DUP_B = { .id = "UNIT_REG_DUP", .collector = "unit_b",
  .name = "b", .run = run_nop, .category = "test" };
REGISTER_SOURCE(D_DUP_A)
REGISTER_SOURCE(D_DUP_B)

int main(void) {
  /* Capture what the first index build prints. */
  char cap[] = "/tmp/jo-unit-registry-XXXXXX";
  int fd = mkstemp(cap);
  assert(fd >= 0);
  fflush(stderr);
  int saved = dup(2);
  dup2(fd, 2); close(fd);
  int dups = registry_duplicate_count();       /* builds the index */
  fflush(stderr);
  dup2(saved, 2); close(saved);
  FILE *f = fopen(cap, "r");
  assert(f);
  char line[512]; int saw = 0;
  while (fgets(line, sizeof line, f))
    if (strstr(line, "DUPLICATE id 'UNIT_REG_DUP'")) saw++;
  fclose(f);
  unlink(cap);
  printf("  duplicates reported: %d (UNIT_REG_DUP lines: %d)\n", dups, saw);
  assert(saw == 1 && dups >= 1);

  const source_def **all = registry_all();
  int n = registry_count();
  assert(n > 2);
  /* The first definition wins, as with the linear scan. */
  int first = -1;
  for (int i = 0; i < n; i++)
    if (!strcmp(all[i]->id, "UNIT_REG_DUP")) { first = i; break; }
  assert(registry_index("UNIT_REG_DUP") == first);
  assert(registry_get("UNIT_REG_DUP") == all[first]);

  /* Every id agrees with a linear first-match scan. Sampled (every 13th),
   * because the full cross-check IS the O(n^2) this replaced. */
  for (int i = 0; i < n; i += 13) {
    int lin = -1;
    for (int j = 0; j < n; j++)
      if (!strcmp(all[j]->id, all[i]->id)) { lin = j; break; }
    assert(registry_index(all[i]->id) == lin);
    assert(registry_get(all[i]->id) == all[lin]);
  }
  assert(registry_index("UNIT_REG_NO_SUCH_ID") == -1);
  assert(registry_get("UNIT_REG_NO_SUCH_ID") == NULL);
  assert(registry_get(NULL) == NULL && registry_index(NULL) == -1);
  printf("  registry_get agrees with the linear scan over %d sources\n", n);

  /* The curated overlay lookup behind src_meta_get(). */
  int g = gen_meta_count(), checked = 0;
  for (int i = 0; i < g; i++) {
    const src_meta *cur = gen_meta_at(i);
    if (!cur || !cur->id) continue;
    const src_meta *m = src_meta_get(cur->id);
    assert(m && !strcmp(m->id, cur->id));
    /* Where the source_def has no name of its own the curated one shows,
     * and the curated name is the one the linear lookup found. */
    const src_meta *lin = gen_meta_get(cur->id);
    assert(lin);
    if (lin->name && lin->name[0]) assert(m->name && !strcmp(m->name, lin->name));
    checked++;
  }
  assert(src_meta_get("UNIT_REG_NO_SUCH_ID") == NULL);
  printf("  src_meta_get agrees with gen_meta_get on %d curated rows\n", checked);

  printf("test_registry_index: ok\n");
  return 0;
}
