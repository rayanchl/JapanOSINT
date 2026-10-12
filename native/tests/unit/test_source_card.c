/* tests/unit/test_source_card.c — the card text every source is found by
 * (core/source_card.c) and the curated alias table (core/service_aliases.c).
 *
 * WHAT IS BEING PROTECTED.
 *   1. Authoring-time noise ("33 datasets at authoring time", "At probe time
 *      (2026-09-14) …", "MEASURED 2026-10-05 …") is removed at CLAUSE level:
 *      the clause goes, the description it was attached to stays. A cleaner
 *      that ate the whole sentence would turn "Tochigi press releases" into
 *      an empty card; one that kept the clause leaves the date and the count
 *      as the strongest tokens on near-identical template rows.
 *   2. Nothing is invented: a card is the id, the name and the source's own
 *      sentences. Checked by requiring every card to START with its id.
 *   3. Every alias names a registered source (registry_get), so a retired
 *      source cannot keep phrases pointing at nothing — and its phrases are
 *      on that source's card.
 *   4. Kinds partition the registry exactly as the dispatcher's pivot rule
 *      does: collector "osint" + interval 0, and nothing else, is a pivot.
 *
 * Includes source_card.c directly, so run.sh links every object EXCEPT
 * obj/core/source_card.o and obj/main.o. */

#include "../../core/source_card.c"

#include <assert.h>

static void expect_clean(const char *in, const char *want) {
  char *got = source_card_clean(in);
  if (!got || strcmp(got, want) != 0) {
    fprintf(stderr, "clean(\"%s\")\n  got:  \"%s\"\n  want: \"%s\"\n", in,
            got ? got : "(null)", want);
    assert(0);
  }
  free(got);
}

static void test_noise(void) {
  expect_clean("Tochigi press releases, 50 items at probe time.",
               "Tochigi press releases.");
  expect_clean("Every dataset of Gifu (岐阜県オープンデータカタログ; 17 datasets "
               "at authoring time): title, licence.",
               "Every dataset of Gifu (岐阜県オープンデータカタログ): title, licence.");
  expect_clean("Sejm votes. At probe time (2026-09-14) returnCountOnly reported "
               "1,234 features. Keyed on vote id.",
               "Sejm votes. Keyed on vote id.");
  expect_clean("Transport note, MEASURED 2026-10-06: api answers 200.", "");
  expect_clean("The full list of main roll-call votes (2,421 at authoring time), "
               "with the result.",
               "The full list of main roll-call votes, with the result.");
  /* not noise: "measured" as a description of the data */
  expect_clean("Traffic volumes measured by image recognition on MLIT road CCTV.",
               "Traffic volumes measured by image recognition on MLIT road CCTV.");
  /* abbreviations are not sentence ends; a repeated sentence is said once */
  expect_clean("Filings e.g. 10-K and 8-K. Filings e.g. 10-K and 8-K.",
               "Filings e.g. 10-K and 8-K.");
  expect_clean("", "");
  printf("  noise removed at clause level, descriptions kept: ok\n");
}

static void test_cards(void) {
  const source_card *c = NULL;
  int n = source_cards_all(&c);
  assert(n == registry_count() && c);
  int kinds[SC_NKINDS] = {0}, noisy = 0;
  for (int i = 0; i < n; i++) {
    if (!c[i].text) continue;
    /* every card is the source's own id first: nothing generated in front */
    size_t il = strlen(c[i].def->id);
    assert(!strncmp(c[i].text, c[i].def->id, il));
    assert(c[i].hash == vecindex_hash(c[i].text));
    kinds[c[i].kind]++;
    const source_def *d = c[i].def;
    int pivot = d->collector && !strcmp(d->collector, "osint") &&
                d->update_interval_sec == 0;
    assert((c[i].kind == SC_KIND_PIVOT) == pivot);
    if (c[i].kind == SC_KIND_POD) assert(d->collector[0] == '_');
    if (noise_at(c[i].text, strlen(c[i].text)) >= 0) noisy++;
    assert(source_card_get(d->id) != NULL);
  }
  assert(noisy == 0);
  assert(kinds[SC_KIND_PIVOT] > 0 && kinds[SC_KIND_SCHEDULED] > 0);
  printf("  %d cards: %d pivot, %d scheduled, %d pod, %d manual; none carries "
         "an authoring-time clause: ok\n", n, kinds[0], kinds[1], kinds[2], kinds[3]);
}

static void test_aliases(void) {
  const service_alias *a = NULL;
  int n = service_aliases_all(&a);
  assert(n > 0 && a);
  for (int i = 0; i < n; i++) {
    if (!registry_get(a[i].id)) {
      fprintf(stderr, "alias for unregistered id %s\n", a[i].id);
      assert(0);
    }
    assert(a[i].phrases && *a[i].phrases);
    const source_card *c = source_card_get(a[i].id);
    assert(c && strstr(c->text, a[i].phrases));     /* it is on the card */
    for (int j = 0; j < i; j++) assert(strcmp(a[i].id, a[j].id));   /* once */
  }
  printf("  %d aliases, every id registered and on its card: ok\n", n);
}

int main(void) {
  test_noise();
  test_cards();
  test_aliases();
  assert(source_kind_parse("pivot") == SC_KIND_PIVOT);
  assert(source_kind_parse("scheduled") == SC_KIND_SCHEDULED);
  assert(source_kind_parse("nope") == -1);
  printf("test_source_card: ok\n");
  return 0;
}
