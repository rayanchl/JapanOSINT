/* tests/unit/test_keyqual.c — contract test for lib/keyqual.c.
 *
 * WHAT IS BEING PROTECTED. Ten hand-written collectors used to give the FIRST
 * record of a colliding group the plain key and qualify only the later ones,
 * so the plain uid belonged to whichever record the upstream served first and
 * moved to a different record whenever the order changed (audit 2026-10-02,
 * open item 7). keyqual qualifies every member of a colliding group from a
 * count over the whole set, so the uid a record gets cannot depend on order.
 *
 * The properties pinned here are the ones a collector relies on without
 * re-checking: a unique key is returned untouched (rows already stored keep
 * their uid), every member of a group is qualified, the result is the same
 * for any permutation of the input, a discriminator shared inside a group
 * falls back to the content hash, byte-identical content collapses to one uid
 * that keyqual_claim() hands out once, and a buffer too small for the
 * composite never produces two records with one uid.
 *
 * Includes keyqual.c directly; the harness links every object EXCEPT
 * obj/lib/keyqual.o and obj/main.o. See tests/unit/run.sh. */

#include "../../lib/keyqual.c"
#include "../../lib/seenset.h"

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}
static void eq(const char *got, const char *want, const char *what) {
  int c = got && want && strcmp(got, want) == 0;
  printf("%s  %s", c ? "  ok  " : "FAIL  ", what);
  if (!c) printf("  (got \"%s\", want \"%s\")", got ? got : "(null)", want ? want : "(null)");
  printf("\n");
  if (!c) g_fail++;
}

typedef struct { const char *base, *disc, *content; } rec;

/* Run the two-pass rule over recs[order[i]] and write each record's uid
 * (indexed by RECORD, not by position) into out[]. */
static void qualify(const rec *r, const int *order, int n, char out[][256]) {
  keyqual q = {0};
  for (int i = 0; i < n; i++) keyqual_add(&q, r[order[i]].base, r[order[i]].disc);
  keyqual_seal(&q);
  for (int i = 0; i < n; i++) {
    const rec *x = &r[order[i]];
    char buf[256];
    const char *u = keyqual_uid(&q, x->base, x->disc, x->content, buf, sizeof buf);
    snprintf(out[order[i]], 256, "%s", u ? u : "(null)");
  }
  keyqual_free(&q);
}

/* The rule this replaced, kept here only to show what the test would catch. */
static void first_occurrence(const rec *r, const int *order, int n, char out[][256]) {
  seen_set s = {0};
  for (int i = 0; i < n; i++) {
    const rec *x = &r[order[i]];
    if (seen_add(&s, x->base)) snprintf(out[order[i]], 256, "%s", x->base);
    else snprintf(out[order[i]], 256, "%s|%s", x->base, x->disc ? x->disc : "");
  }
  seen_free(&s);
}

static void test_unique_and_group(void) {
  printf("-- unique keys stay plain; EVERY member of a group is qualified\n");
  /* RDW's live case: areaid 599_HART is published by two area managers. */
  rec r[] = {
    { "599_HART", "2448", "{\"desc\":\"Hart van Zuid\"}" },
    { "600_ABC",  "2448", "{\"desc\":\"elsewhere\"}" },
    { "599_HART", "2459", "{\"desc\":\"Hart van IJsselmonde\"}" },
  };
  int order[] = { 0, 1, 2 };
  char u[3][256];
  qualify(r, order, 3, u);
  eq(u[1], "600_ABC", "a unique key is returned unchanged");
  eq(u[0], "599_HART|2448", "first member of the group is qualified too");
  eq(u[2], "599_HART|2459", "second member qualified by its own discriminator");

  keyqual q = {0};
  keyqual_add(&q, "k", NULL);
  char buf[64];
  const char *p = keyqual_uid(&q, "k", NULL, "x", buf, sizeof buf);
  ok(p && strcmp(p, "k") == 0 && p != buf, "unique base is returned as the caller's own pointer");
  ok(keyqual_claim(&q, "k", p) && keyqual_claim(&q, "k", p),
     "claim never refuses a unique base");
  ok(keyqual_count(&q, "absent") == 0, "count of an absent key is 0");
  keyqual_free(&q);
}

static void test_order_independent(void) {
  printf("-- the uid a record gets does not depend on encounter order\n");
  /* Chicago contracts: one contract number, three revisions, plus noise. */
  rec r[] = {
    { "347521", "0", "{\"rev\":0,\"amt\":100}" },
    { "347521", "1", "{\"rev\":1,\"amt\":150}" },
    { "347521", "2", "{\"rev\":2,\"amt\":175}" },
    { "900001", "0", "{\"rev\":0,\"amt\":9}" },
    { "urn:a",  NULL, "{\"t\":\"10:00\"}" },
    { "urn:a",  NULL, "{\"t\":\"10:15\"}" },
  };
  int n = 6;
  int fwd[] = { 0, 1, 2, 3, 4, 5 };
  int rev[] = { 5, 4, 3, 2, 1, 0 };
  int mix[] = { 2, 5, 0, 3, 1, 4 };
  char a[6][256], b[6][256], c[6][256];
  qualify(r, fwd, n, a);
  qualify(r, rev, n, b);
  qualify(r, mix, n, c);
  int same = 1;
  for (int i = 0; i < n; i++)
    if (strcmp(a[i], b[i]) || strcmp(a[i], c[i])) same = 0;
  ok(same, "three permutations give every record the same uid");
  int distinct = 1;
  for (int i = 0; i < n; i++)
    for (int j = i + 1; j < n; j++) if (!strcmp(a[i], a[j])) distinct = 0;
  ok(distinct, "six records that differ get six uids");
  eq(a[3], "900001", "the unique contract keeps its plain uid");
  ok(strncmp(a[4], "urn:a|", 6) == 0 && strlen(a[4]) == 6 + 20,
     "no discriminator -> base|<20-hex content hash>");

  /* What the old rule did with the same input: the plain uid belongs to
   * whichever revision arrived first, so it changes owner with the order. */
  char oa[6][256], ob[6][256];
  first_occurrence(r, fwd, n, oa);
  first_occurrence(r, rev, n, ob);
  ok(strcmp(oa[0], ob[0]) != 0,
     "(reference) first-occurrence gives revision 0 a different uid when the order flips");
}

static void test_shared_disc_and_identical(void) {
  printf("-- a discriminator shared inside a group falls back to the content hash\n");
  /* NGA List of Lights: same feature/name/position, charNo 2 twice — one pair
   * differs only in remarks, and one pair is byte-identical. */
  rec r[] = {
    { "F1", "2", "{\"charNo\":2,\"remarks\":\"A\"}" },
    { "F1", "2", "{\"charNo\":2,\"remarks\":\"B\"}" },
    { "F1", "3", "{\"charNo\":3}" },
    { "F2", "2", "{\"charNo\":2,\"x\":1}" },
    { "F2", "2", "{\"charNo\":2,\"x\":1}" },
  };
  int order[] = { 0, 1, 2, 3, 4 };
  char u[5][256];
  qualify(r, order, 5, u);
  eq(u[2], "F1|3", "the member whose discriminator is unique uses it");
  ok(strcmp(u[0], u[1]) != 0 && strncmp(u[0], "F1|", 3) == 0 &&
     strlen(u[0]) == 3 + 20, "members sharing charNo but differing are hashed apart");
  ok(strcmp(u[3], u[4]) == 0, "byte-identical records collapse to one uid");

  keyqual q = {0};
  for (int i = 3; i < 5; i++) keyqual_add(&q, r[i].base, r[i].disc);
  char b1[64], b2[64];
  const char *x = keyqual_uid(&q, r[3].base, r[3].disc, r[3].content, b1, sizeof b1);
  const char *y = keyqual_uid(&q, r[4].base, r[4].disc, r[4].content, b2, sizeof b2);
  ok(keyqual_claim(&q, r[3].base, x) == 1, "first copy of an identical record is claimed");
  ok(keyqual_claim(&q, r[4].base, y) == 0, "second copy is refused (so emitted == stored)");
  keyqual_free(&q);
}

static void test_truncation(void) {
  printf("-- a composite that does not fit is shortened without colliding\n");
  char base[200];
  memset(base, 'k', sizeof base - 1);
  base[sizeof base - 1] = 0;
  rec r[] = {
    { base, "discriminator-one", "{\"a\":1}" },
    { base, "discriminator-two", "{\"a\":2}" },
  };
  keyqual q = {0};
  keyqual_add(&q, r[0].base, r[0].disc);
  keyqual_add(&q, r[1].base, r[1].disc);
  char b1[64], b2[64];
  const char *x = keyqual_uid(&q, r[0].base, r[0].disc, r[0].content, b1, sizeof b1);
  const char *y = keyqual_uid(&q, r[1].base, r[1].disc, r[1].content, b2, sizeof b2);
  ok(strlen(x) < sizeof b1 && strlen(y) < sizeof b2, "both fit their buffers");
  ok(strcmp(x, y) != 0, "and they are still different uids");
  keyqual_free(&q);

  /* Many groups, growing every table past its first allocation. */
  keyqual big = {0};
  char k[32], d[32];
  for (int i = 0; i < 3000; i++) {
    snprintf(k, sizeof k, "g%d", i / 3);
    snprintf(d, sizeof d, "d%d", i % 3);
    keyqual_add(&big, k, d);
  }
  int good = !big.oom;
  char buf[64];
  for (int i = 0; i < 3000 && good; i++) {
    snprintf(k, sizeof k, "g%d", i / 3);
    snprintf(d, sizeof d, "d%d", i % 3);
    const char *u = keyqual_uid(&big, k, d, "", buf, sizeof buf);
    char want[64];
    snprintf(want, sizeof want, "%s|%s", k, d);
    if (strcmp(u, want) || !keyqual_claim(&big, k, u)) good = 0;
  }
  ok(good, "3,000 records in 1,000 groups: every uid base|disc, every claim fresh");
  keyqual_free(&big);
}

int main(void) {
  test_unique_and_group();
  test_order_independent();
  test_shared_disc_and_identical();
  test_truncation();
  printf(g_fail ? "keyqual: %d FAILED\n" : "keyqual: all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
