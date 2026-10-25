/* test_jsonlist_title.c — the title/label picker in lib/jsonlist.c, and the
 * one shape it was taught to read: a DCAT language map.
 *
 * WHAT IS BEING PROTECTED. `pick()` deliberately SKIPS a nested value, because
 * `{"location":{"lat":…,"lon":…},"country":"Japan"}` must resolve to Japan and
 * not to a coordinate pair — its own comment records that as a whole-source
 * silent zero when it got that wrong. So reading one KIND of object (a map of
 * language tag to string, which is how DCAT publishes a title) has to leave
 * that behaviour exactly as it was. Each case below is one way of getting it
 * wrong:
 *   1. a language map resolves, and `en` wins over document order;
 *   2. no `en` falls back to the first member, deterministically;
 *   3. a coordinate object is still skipped and the walk still continues;
 *   4. an object whose keys are not language tags is not a language map —
 *      including `{"id":…,"label":…}`, whose `id` is two letters and would
 *      pass a naive "2 or 3 letters" test;
 *   5. a single-member object is not a map (one translation is indistinguish-
 *      able from a one-field record);
 *   6. a member whose value is not a non-empty string disqualifies the whole
 *      object, so `{"lat":…}` cannot sneak through on key length alone;
 *   7. a region subtag (`pt-BR`) is accepted;
 *   8. a record that resolved BEFORE this change resolves to the same node,
 *      which is the additive guarantee the change rests on.
 */
#include "../../lib/jsonlist.c"

#include <assert.h>
#include <stdio.h>

static int fails;
static void ok(int cond, const char *what) {
  printf("  %-4s %s\n", cond ? "ok" : "FAIL", what);
  if (!cond) fails++;
}

/* pick() through the real K_TITLE list, as every record goes through it. */
static const char *title_of(const char *json) {
  cJSON *o = cJSON_Parse(json);
  assert(o);
  cJSON *v = pick(o, K_TITLE);
  static char buf[256];
  buf[0] = 0;
  if (v && cJSON_IsString(v) && v->valuestring)
    snprintf(buf, sizeof buf, "%s", v->valuestring);
  else if (v && cJSON_IsNumber(v))
    snprintf(buf, sizeof buf, "%.17g", v->valuedouble);
  cJSON_Delete(o);
  return buf;
}

int main(void) {
  printf("-- 1. a language map resolves, en first\n");
  ok(!strcmp(title_of("{\"title\":{\"fr\":\"Fr\",\"en\":\"En\",\"de\":\"De\"}}"), "En"),
     "en wins over document order");
  ok(!strcmp(title_of("{\"title\":{\"en\":\"En\",\"fr\":\"Fr\"}}"), "En"),
     "en first in document order too");

  printf("-- 2. no en: the first member, deterministically\n");
  ok(!strcmp(title_of("{\"title\":{\"fr\":\"Fr\",\"de\":\"De\"}}"), "Fr"),
     "first member when there is no en");
  ok(!strcmp(title_of("{\"title\":{\"eng\":\"Eng\",\"fra\":\"Fra\"}}"), "Eng"),
     "three-letter tags are tags too");

  printf("-- 3. a coordinate object is still skipped (the case pick() exists for)\n");
  ok(!strcmp(title_of("{\"location\":{\"lat\":35.6,\"lon\":139.7},\"country\":\"Japan\"}"),
             "Japan"),
     "location is skipped and country is reached");

  printf("-- 4. keys that are not language tags\n");
  ok(!strcmp(title_of("{\"name\":{\"id\":\"x\",\"label\":\"y\"},\"headline\":\"H\"}"), "H"),
     "{id,label} is not a language map (id is two letters)");
  ok(!strcmp(title_of("{\"title\":{\"id\":\"x\",\"en\":\"y\"},\"headline\":\"H\"}"), "H"),
     "an id member disqualifies the object even beside an en");
  ok(!strcmp(title_of("{\"title\":{\"label\":\"L\",\"resource\":\"R\"},\"headline\":\"H\"}"),
             "H"),
     "long keys are not tags");
  ok(!strcmp(title_of("{\"title\":{\"en\":\"E\",\"english\":\"X\"},\"headline\":\"H\"}"), "H"),
     "ONE non-tag key is enough to disqualify");

  printf("-- 5. a single member is not a map\n");
  ok(!strcmp(title_of("{\"title\":{\"en\":\"E\"},\"headline\":\"H\"}"), "H"),
     "one translation is indistinguishable from a one-field record");

  printf("-- 6. every value must be a non-empty string\n");
  ok(!strcmp(title_of("{\"title\":{\"en\":\"E\",\"fr\":\"\"},\"headline\":\"H\"}"), "H"),
     "an empty string disqualifies");
  ok(!strcmp(title_of("{\"title\":{\"en\":\"E\",\"fr\":7},\"headline\":\"H\"}"), "H"),
     "a number disqualifies");
  ok(!strcmp(title_of("{\"title\":{\"en\":\"E\",\"fr\":{\"a\":\"b\"}},\"headline\":\"H\"}"),
             "H"),
     "a nested object disqualifies");

  printf("-- 7. a region subtag\n");
  ok(!strcmp(title_of("{\"title\":{\"pt-BR\":\"Pt\",\"en\":\"En\"}}"), "En"),
     "pt-BR is a tag, and en still wins");
  ok(!strcmp(title_of("{\"title\":{\"pt-BR\":\"Pt\",\"fr-CA\":\"Fr\"}}"), "Pt"),
     "region subtags alone are a language map");
  ok(!strcmp(title_of("{\"title\":{\"en-\":\"X\",\"fr\":\"Fr\"},\"headline\":\"H\"}"), "H"),
     "a dangling hyphen is not a tag");

  printf("-- 8. the additive guarantee: a plain record is unchanged\n");
  ok(!strcmp(title_of("{\"title\":\"Plain\"}"), "Plain"), "a string title");
  ok(!strcmp(title_of("{\"name\":\"N\",\"title\":\"T\"}"), "T"), "precedence unchanged");
  ok(!strcmp(title_of("{\"nome\":\"Nome\"}"), "Nome"), "the non-English key list still works");
  ok(!strcmp(title_of("{\"nothing\":\"here\"}"), ""), "no title key is still no title");

  printf(fails ? "\nFAILED (%d)\n" : "\nall passed\n", fails);
  return fails ? 1 : 0;
}
