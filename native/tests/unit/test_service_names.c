/* test_service_names.c — the vocabulary gate.
 *
 * Service names are written down in two places that are NOT generated from the
 * registry: the static analysis schema (`grammars/osint_analysis.schema.json`,
 * two enums) and the few-shot examples baked into `core/prompts.c`. Both are
 * hand-maintained, and nothing checked them until this test.
 *
 * The schema's enums are normally overwritten per request from the live
 * registry (osint_dispatch.c:osint_schema_with_ids), so a stale name there is
 * invisible in the common case and reaches a model only on the fallback path
 * (pipeline.c: `dynschema ? dynschema : schema_load("osint_analysis")`) — that
 * is, exactly when something else has already gone wrong. The few-shot names
 * are never rewritten: they are in the prompt on every single analysis call,
 * teaching the model a vocabulary that must still exist.
 *
 * Measured when this test was written (2026-09-11, 16,366 registered sources,
 * 1,838 entity pivots): schema 107/107 resolve, prompts 91 of 93 resolve (the
 * two exceptions are the literal placeholders below). The point of the test is
 * not to discover drift today — it is that renaming or retiring a service now
 * fails here instead of degrading a prompt silently, which is the failure mode
 * this repository keeps finding in other guises.
 *
 * `grammars/osint_analysis.gbnf` used to be a third copy, with 100 names of
 * which 25 no longer existed. Nothing loaded it (every grammar_load() call site
 * asks for entity_extraction / suggestions / page_analysis / triage / repair_*),
 * so it was deleted rather than repaired; this test asserts it stays gone, so
 * the dead vocabulary cannot come back by a well-meaning `git revert`.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "source.h"                    /* registry_get / registry_count */
#include "third_party/cJSON.h"

/* Names that are deliberately not services: the prompt's own metasyntax. */
static const char *const PLACEHOLDERS[] = {
  "SERVICE_NAME", "SERVICE_THAT_FOUND_ENTITY", NULL
};

static int is_placeholder(const char *n) {
  for (int i = 0; PLACEHOLDERS[i]; i++)
    if (!strcmp(PLACEHOLDERS[i], n)) return 1;
  return 0;
}

static char *slurp(const char *rel) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s", JO_REPO_ROOT, rel);
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  long n = ftell(f);
  if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
  char *buf = malloc((size_t)n + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t rd = fread(buf, 1, (size_t)n, f);
  buf[rd] = '\0';
  fclose(f);
  return buf;
}

static int resolves(const char *name) {
  if (registry_get(name)) return 1;
  /* The dispatcher canonicalises to upper case before lookup, and ids are
   * registered in both cases across the tree, so accept either form here —
   * this test is about existence, not about spelling discipline. */
  char up[256];
  size_t n = strlen(name);
  if (n >= sizeof up) return 0;
  for (size_t i = 0; i <= n; i++)
    up[i] = (name[i] >= 'a' && name[i] <= 'z') ? (char)(name[i] - 32) : name[i];
  if (registry_get(up)) return 1;
  char lo[256];
  for (size_t i = 0; i <= n; i++)
    lo[i] = (name[i] >= 'A' && name[i] <= 'Z') ? (char)(name[i] + 32) : name[i];
  return registry_get(lo) != NULL;
}

/* ---- 1. the static schema's two service enums --------------------------- */

static cJSON *enum_at(cJSON *root, const char *const *path, int n) {
  cJSON *cur = root;
  for (int i = 0; i < n && cur; i++) cur = cJSON_GetObjectItem(cur, path[i]);
  return cur;
}

static int check_enum(cJSON *arr, const char *what, cJSON **out_first) {
  assert(cJSON_IsArray(arr) && "schema enum missing");
  int n = cJSON_GetArraySize(arr), bad = 0;
  assert(n > 0);
  cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    if (!cJSON_IsString(it) || !it->valuestring) continue;
    if (!resolves(it->valuestring)) {
      printf("  MISSING %s: %s is in %s but not in the registry\n",
             what, it->valuestring, what);
      bad++;
    }
  }
  if (out_first) *out_first = arr;
  printf("  %-28s %d names, %d not registered\n", what, n, bad);
  return bad;
}

static void test_schema_enums(void) {
  char *txt = slurp("grammars/osint_analysis.schema.json");
  assert(txt && "grammars/osint_analysis.schema.json unreadable");
  cJSON *root = cJSON_Parse(txt);
  assert(root && "schema does not parse");

  static const char *const p_rec[] = { "properties", "recommended_services",
                                       "items", "enum" };
  static const char *const p_ent[] = { "properties", "entities", "items",
                                       "properties", "services", "items",
                                       "enum" };
  cJSON *rec = enum_at(root, p_rec, 4);
  cJSON *ent = enum_at(root, p_ent, 7);

  int bad = 0;
  bad += check_enum(rec, "recommended_services", NULL);
  bad += check_enum(ent, "entities[].services", NULL);
  assert(bad == 0 && "a service name in the static schema no longer exists — "
                     "regenerate the enum from the registry");

  /* The two enums are the model's whole vocabulary, in two places. They have
   * always been identical; if they diverge, one of them is wrong and nothing
   * else would notice. */
  assert(cJSON_GetArraySize(rec) == cJSON_GetArraySize(ent));
  for (int i = 0; i < cJSON_GetArraySize(rec); i++) {
    const char *a = cJSON_GetArrayItem(rec, i)->valuestring;
    const char *b = cJSON_GetArrayItem(ent, i)->valuestring;
    assert(a && b && !strcmp(a, b) && "the schema's two service enums diverged");
  }
  printf("  the two enums are identical: ok\n");

  cJSON_Delete(root);
  free(txt);
}

/* ---- 2. the few-shot examples in core/prompts.c -------------------------- */

/* Scan the C source for `services\": [ ... ]` — the examples are JSON embedded
 * in C string literals, so the bytes on disk carry backslash-escaped quotes.
 * Collect every upper-case token inside each such array. */
static void test_fewshot_names(void) {
  char *src = slurp("native/core/prompts.c");
  assert(src && "native/core/prompts.c unreadable");

  const char *needle = "services\\\":";     /* on disk: services\": */
  int total = 0, bad = 0, placeholders = 0;
  for (const char *p = strstr(src, needle); p; p = strstr(p + 1, needle)) {
    const char *open = strchr(p, '[');
    if (!open) break;
    const char *close = strchr(open, ']');
    if (!close) break;
    for (const char *q = open; q < close; q++) {
      if (*q != '\\' || q[1] != '"') continue;
      const char *s = q + 2;
      /* A service name starts with a letter. Without this the scanner also
       * collects the phone numbers and reference codes that appear as entity
       * VALUES in the same examples ("0001234567"), and reports them as
       * missing services — a false alarm that would teach the next reader to
       * distrust this gate. */
      if (!(*s >= 'A' && *s <= 'Z')) continue;
      const char *e = s;
      while (e < close && ((*e >= 'A' && *e <= 'Z') || (*e >= '0' && *e <= '9')
                           || *e == '_')) e++;
      if (e == s || !(e < close && *e == '\\' && e[1] == '"')) continue;
      char name[256];
      size_t n = (size_t)(e - s);
      if (n >= sizeof name) continue;
      memcpy(name, s, n);
      name[n] = '\0';
      q = e + 1;
      if (is_placeholder(name)) { placeholders++; continue; }
      total++;
      if (!resolves(name)) {
        printf("  MISSING few-shot: %s is taught to the model but is not "
               "registered\n", name);
        bad++;
      }
    }
  }
  printf("  %-28s %d names checked (%d placeholders skipped), %d not "
         "registered\n", "prompts.c few-shot", total, placeholders, bad);
  assert(total > 50 && "the few-shot scanner found almost nothing — the prompt "
                       "format changed and this test is no longer reading it");
  assert(bad == 0 && "a few-shot example names a service that no longer exists");
  free(src);
}

/* ---- 3. the grammars the code actually loads must exist ------------------ */

static void test_loaded_grammars_exist(void) {
  static const char *const LOADED[] = {
    "entity_extraction", "suggestions", "page_analysis",
    "triage_classification", "repair_sanity", "repair_proposal", NULL
  };
  for (int i = 0; LOADED[i]; i++) {
    char rel[256];
    snprintf(rel, sizeof rel, "grammars/%s.gbnf", LOADED[i]);
    char *g = slurp(rel);
    assert(g && "a grammar that grammar_load() asks for is missing");
    free(g);
  }
  /* and the dead one stays dead */
  char *dead = slurp("grammars/osint_analysis.gbnf");
  if (dead) {
    printf("  osint_analysis.gbnf is back — it is not loaded by anything and "
           "its names rot; delete it or wire it up deliberately\n");
    free(dead);
    assert(0);
  }
  printf("  every loaded grammar present, osint_analysis.gbnf absent: ok\n");
}

int main(void) {
  printf("-- service-name vocabulary (registry: %d sources)\n", registry_count());
  assert(registry_count() > 0 && "no sources registered — link line is wrong");
  test_schema_enums();
  test_fewshot_names();
  test_loaded_grammars_exist();
  printf("test_service_names: ok\n");
  return 0;
}
