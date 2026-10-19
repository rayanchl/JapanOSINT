/* tests/unit/test_resolve_embed.c — the resolver's embedding fallback
 * (osint_resolve_near_db in core/osint_dispatch.c).
 *
 * WHAT IS BEING PROTECTED.
 *   1. INERT: with no JO_EMBED_URL a name edit distance cannot place stays
 *      unresolved (not_implemented), exactly as before.
 *   2. Spelling still wins: a one-edit typo resolves by edit distance and is
 *      NOT reported as an embedding match.
 *   3. A name that means a registered pivot but is spelled nothing like it
 *      resolves by embedding, through the router's public query, and the
 *      result says resolved_from + resolver:"embedding".
 *   4. A name near nothing (no candidate clears the floor and the margin) is
 *      refused — a guess is not presented as a resolution.
 *
 * The fixture pivot carries a word no other service card has, so the stub's
 * bag-of-words ranking is unambiguous. Includes osint_dispatch.c directly, so
 * run.sh links every object EXCEPT obj/core/osint_dispatch.o and obj/main.o. */

#include "../../core/osint_dispatch.c"
#include "embed_stub.h"

#include <assert.h>

static int run_zebra(const source_ctx *ctx, intel_sink *sink) {
  intel_item it = {0};
  it.remote_key = "z1";
  it.title = "zebrafish genome atlas entry";
  it.summary = ctx->entity;
  it.properties_json = "{\"atlas\":\"zebrafish\"}";
  it.record_type = "record";
  sink->emit(sink, &it);
  return 0;
}
static const source_def t_zebra = {
  .id = "TEST_ZEBRAFISH_GENOME", .collector = "osint",
  .name = "Zebrafish genome atlas",
  .description = "zebrafish genome atlas lookup",
  .update_interval_sec = 0, .run = run_zebra, .category = "test" };
REGISTER_SOURCE(t_zebra)

static const char *MEANT = "GENOME_ZEBRAFISH_ATLAS_FINDER";

int main(void) {
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL"); unsetenv("JO_EMBED_MODEL");
  unsetenv("JO_EMBED_QUERY_PREFIX"); unsetenv("JO_EMBED_DOC_PREFIX");
  unsetenv("JO_ROUTE_SEMANTIC");
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);
  intel_sink ps = intel_sink_make(&db, "osint-search", "legacy");
  char out[128];
  int emb = -1;

  /* 1. inert */
  assert(osint_resolve_near_db(&db, MEANT, out, sizeof out, &emb) == 0 && emb == 0);
  osint_result r;
  osint_dispatch(&db, NULL, MEANT, "danio rerio", "keyword", &ps, &r);
  assert(r.error && !strcmp(r.error, "not_implemented") && !r.resolved_from);
  osint_result_free(&r);
  printf("  inert: unplaceable name stays not_implemented: ok\n");

  if (estub_start(18400, 256, "stub-bow") != 0) {
    printf("test_resolve_embed: stub unavailable, embedding cases skipped\n");
    intel_sink_free(&ps); db_close(&db); estub_stop();
    return 0;
  }
  estub_use();
  int built = service_vec_build(&db);
  assert(built > 0);

  /* 2. spelling first */
  assert(osint_resolve_near_db(&db, "TEST_ZEBRAFISH_GENOMX", out, sizeof out, &emb) == 1);
  assert(!strcmp(out, "TEST_ZEBRAFISH_GENOME") && emb == 0);

  /* 3. by meaning */
  assert(osint_resolve_near_db(&db, MEANT, out, sizeof out, &emb) == 1);
  assert(!strcmp(out, "TEST_ZEBRAFISH_GENOME") && emb == 1);
  osint_dispatch(&db, NULL, MEANT, "danio rerio", "keyword", &ps, &r);
  assert(r.success && r.records == 1);
  assert(r.resolved_from && !strcmp(r.resolved_from, MEANT));
  assert(r.resolver && !strcmp(r.resolver, "embedding"));
  assert(!strcmp(r.service, "TEST_ZEBRAFISH_GENOME"));
  osint_result_free(&r);
  printf("  %d services indexed; typo by spelling, %s by embedding "
         "(resolved_from + resolver reported): ok\n", built, MEANT);

  /* 4. near nothing */
  assert(osint_resolve_near_db(&db, "QQQQX_WWWWZ_VVVVY", out, sizeof out, &emb) == 0);
  assert(emb == 0);
  printf("  a name near nothing is refused, not guessed: ok\n");

  intel_sink_free(&ps);
  db_close(&db);
  estub_stop();
  printf("test_resolve_embed: ok\n");
  return 0;
}
