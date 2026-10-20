/* tests/unit/test_fewshot.c — phase-1 routing examples, from a file and by
 * retrieval (core/fewshot.c, prompt_analysis_with in core/prompts.c).
 *
 * WHAT IS BEING PROTECTED.
 *   1. The move out of prompts.c was lossless: the static block is every
 *      example of grammars/fewshot_analysis.jsonl, in file order, in the exact
 *      "Query: q\nresponse\n\n" shape, and prompt_analysis() is
 *      prompt_analysis_with(…, NULL) — the old prompt.
 *   2. INERT: no JO_EMBED_URL → fewshot_select() is NULL and sends nothing.
 *   3. With a server: the examples nearest the query plus the spine, in file
 *      order, a fraction of the bytes; a second search re-embeds no example.
 *
 * FEWSHOT_DUMP_PROMPT=<path> writes the inert analysis prompt there, for a
 * byte comparison against a build from before the move.
 *
 * Includes fewshot.c directly, so run.sh links every object EXCEPT
 * obj/core/fewshot.o and obj/main.o. */

#include "../../core/fewshot.c"
#include "../../core/prompts.h"
#include "embed_stub.h"

#include <assert.h>

static const char *Q_VIN = "decode vin 1HGBH41JXMN109186 recalls";

int main(void) {
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL"); unsetenv("JO_EMBED_MODEL");
  db_handle db;
  assert(db_open(&db, getenv("JO_DB"), NULL) == 0);

  /* 1. lossless */
  int n = fewshot_count();
  assert(n == 76);
  const char *blk = fewshot_static_block();
  size_t off = 0;
  for (int i = 0; i < g_n; i++) {
    char head[600];
    snprintf(head, sizeof head, "Query: %s\n", g_ex[i].query);
    assert(!strncmp(blk + off, head, strlen(head)));
    off += strlen(head);
    assert(!strncmp(blk + off, g_ex[i].response, strlen(g_ex[i].response)));
    off += strlen(g_ex[i].response);
    assert(!strncmp(blk + off, "\n\n", 2));
    off += 2;
  }
  assert(off == strlen(blk));
  assert(strlen(blk) == 27163);          /* the bytes prompts.c held, measured */
  char *a = prompt_analysis("who owns example.com", "X: y");
  char *b = prompt_analysis_with("who owns example.com", "X: y", NULL);
  assert(strlen(a) == strlen(b) && strstr(a, blk));
  const char *dump = getenv("FEWSHOT_DUMP_PROMPT");
  if (dump) {
    char *d = prompt_analysis("who owns example.com", "SVC_A: a\nSVC_B: b");
    FILE *f = fopen(dump, "w");
    if (f) { fputs(d, f); fclose(f); }
    free(d);
  }
  free(b);
  printf("  %d examples, static block %zu bytes in file order: ok\n", n, strlen(blk));

  /* 2. inert */
  fewshot_note note;
  assert(fewshot_select(&db, Q_VIN, &note) == NULL && note.chosen == 0);
  printf("  inert: no selection without an embedding server: ok\n");

  if (estub_start(18380, 256, "stub-bow") != 0) {
    printf("test_fewshot: stub unavailable, retrieval cases skipped\n");
    free(a); db_close(&db); estub_stop();
    return 0;
  }
  estub_use();

  /* 3. retrieval */
  char *sel = fewshot_select(&db, Q_VIN, &note);
  assert(sel);
  assert(note.total == 76 && note.by_similarity == FEWSHOT_TOP_K);
  assert(note.spine == 2 && note.chosen == FEWSHOT_TOP_K + 2);
  assert(strstr(sel, "Query: decode vin 1HGBH41JXMN109186\n"));
  assert(strstr(sel, "Query: recalls for vin JH4KA8260MC000000\n"));
  assert(strstr(sel, "Query: can u find info on the email addy"));   /* spine */
  assert(strstr(sel, "Query: john barker, 0782327674"));             /* spine */
  /* file order is kept: the spine's two lines are the last two */
  assert(strstr(sel, "Query: john barker") < strstr(sel, "Query: can u find"));
  char *c = prompt_analysis_with(Q_VIN, "X: y", sel);
  char *full = prompt_analysis(Q_VIN, "X: y");
  printf("  phase-1 prompt %zu bytes with all %d examples, %zu with %d "
         "retrieved (%zu-byte example block vs %zu)\n",
         strlen(full), n, strlen(c), note.chosen, strlen(sel), strlen(blk));
  assert(strlen(c) < strlen(full) / 2);
  free(c); free(full); free(sel);

  int posts = estub_posts();
  sel = fewshot_select(&db, "track flight NH205 status", &note);
  assert(sel && strstr(sel, "Query: track flight NH205\n"));
  assert(estub_posts() == posts + 1);        /* the query only */
  free(sel);
  assert(vecindex_count(&db, "fewshot_vec") == 76);
  printf("  retrieval: nearest + spine in file order, second search embeds "
         "only the query: ok\n");

  free(a);
  db_close(&db);
  estub_stop();
  printf("test_fewshot: ok\n");
  return 0;
}
