/* collectors/sources/reg_pl_krs.c
 * OSINT service — PL_KRS. On-demand entity pivot against Poland's KRS OPEN
 * (Krajowy Rejestr Sądowy) public API at api-krs.ms.gov.pl. Keyless and LIVE.
 *
 * The API keys on a 10-digit KRS number. When the entity is exactly 10 digits
 * we GET /api/krs/OdpisAktualny/<krs>?rejestr=P&format=json (register P =
 * przedsiębiorcy / entrepreneurs) and emit one intel_item for the matched
 * entity: legal name, registered address and the management board (skład
 * organu reprezentacji). Non-numeric / wrong-length entity → honest empty
 * (return 0), never a fabricated lookup card.
 *
 * Field mapping follows the OdpisAktualny JSON shape: odpis.dane.dzial1 carries
 * the name (danePodmiotu) and seat address (siedzibaIAdres.adres); dzial2 carries
 * the representation organ (reprezentacja.sklad[]). Missing sub-objects are
 * tolerated — we emit whatever real fields are present. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Build "Kowalski Jan (PREZES ZARZĄDU), Nowak Anna (CZŁONEK)" from a
 * reprezentacja.sklad[] array. malloc'd (caller frees) or NULL if none. */
static char *krs_board(const cJSON *dzial2) {
  if (!dzial2) return NULL;
  const cJSON *repr = cJSON_GetObjectItem(dzial2, "reprezentacja");
  const cJSON *sklad = repr ? cJSON_GetObjectItem(repr, "sklad") : NULL;
  if (!cJSON_IsArray(sklad)) return NULL;
  size_t cap = 256, len = 0;
  char *out = (char *)malloc(cap);
  if (!out) return NULL;
  out[0] = 0;
  int n = 0;
  const cJSON *m;
  cJSON_ArrayForEach(m, sklad) {
    const char *nazw = jo_sv(m, "nazwisko");
    const char *imie = jo_sv(m, "imiona");
    const char *func = jo_sv(m, "funkcjaWOrganie");
    if (!nazw && !imie) continue;
    char frag[320];
    int fl = snprintf(frag, sizeof frag, "%s%s%s%s%s%s",
                      nazw ? nazw : "",
                      (nazw && imie) ? " " : "",
                      imie ? imie : "",
                      func ? " (" : "",
                      func ? func : "",
                      func ? ")" : "");
    if (fl <= 0) continue;
    size_t need = len + (size_t)fl + 3;
    if (need > cap) { cap = need * 2; char *t = realloc(out, cap); if (!t) break; out = t; }
    if (n) { strcpy(out + len, "; "); len += 2; }
    strcpy(out + len, frag); len += strlen(frag);
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
  }
  if (!n) { free(out); return NULL; }
  return out;
}

/* Assemble a one-line seat address from siedzibaIAdres.adres. malloc'd or NULL. */
static char *krs_address(const cJSON *dzial1) {
  if (!dzial1) return NULL;
  const cJSON *sia = cJSON_GetObjectItem(dzial1, "siedzibaIAdres");
  const cJSON *adr = sia ? cJSON_GetObjectItem(sia, "adres") : NULL;
  if (!adr) return NULL;
  const char *ulica = jo_sv(adr, "ulica");
  const char *nr    = jo_sv(adr, "nrDomu");
  const char *lok   = jo_sv(adr, "nrLokalu");
  const char *kod   = jo_sv(adr, "kodPocztowy");
  const char *msc   = jo_sv(adr, "miejscowosc");
  const char *kraj  = jo_sv(adr, "kraj");
  if (!ulica && !msc && !kod) return NULL;
  char buf[512];
  snprintf(buf, sizeof buf, "%s%s%s%s%s%s%s%s%s%s%s",
           ulica ? ulica : "",
           (ulica && nr) ? " " : "",
           nr ? nr : "",
           (nr && lok) ? "/" : "",
           lok ? lok : "",
           ((ulica || nr) && (kod || msc)) ? ", " : "",
           kod ? kod : "",
           (kod && msc) ? " " : "",
           msc ? msc : "",
           (kraj && (msc || kod || ulica)) ? ", " : "",
           kraj ? kraj : "");
  return strdup(buf);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  /* KRS OPEN keys on a 10-digit KRS number; anything else → honest empty. */
  int len = 0, alldig = 1;
  for (const char *p = q; *p; p++, len++)
    if (!isdigit((unsigned char)*p)) alldig = 0;
  if (!alldig || len != 10) {
    fprintf(stderr, "[pl_krs] entity not a 10-digit KRS number — empty\n");
    return 0;
  }

  char url[256];
  snprintf(url, sizeof url,
    "https://api-krs.ms.gov.pl/api/krs/OdpisAktualny/%s?rejestr=P&format=json", q);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "pl_krs");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const cJSON *odpis = cJSON_GetObjectItem(root, "odpis");
  const cJSON *dane  = odpis ? cJSON_GetObjectItem(odpis, "dane") : NULL;
  const cJSON *d1    = dane  ? cJSON_GetObjectItem(dane, "dzial1") : NULL;
  const cJSON *d2    = dane  ? cJSON_GetObjectItem(dane, "dzial2") : NULL;

  const cJSON *dp   = d1 ? cJSON_GetObjectItem(d1, "danePodmiotu") : NULL;
  const char  *name = dp ? jo_sv(dp, "nazwa") : NULL;

  if (!name) {
    /* no matched entity in the response → honest empty */
    cJSON_Delete(root);
    fprintf(stderr, "[pl_krs] no entity for KRS %s\n", q);
    return 0;
  }

  char *addr  = krs_address(d1);
  char *board = krs_board(d2);
  const char *forma = dp ? jo_sv(dp, "formaPrawna") : NULL;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  cJSON_AddStringToObject(data, "krs", q);
  if (forma) cJSON_AddStringToObject(data, "legal_form", forma);
  if (addr)  cJSON_AddStringToObject(data, "address", addr);
  if (board) cJSON_AddStringToObject(data, "board", board);
  cJSON_AddStringToObject(data, "register", "P");
  cJSON_AddStringToObject(data, "source", "KRS OPEN");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PL_KRS");
  cJSON_AddStringToObject(props, "source", "KRS OPEN");
  cJSON_AddStringToObject(props, "krs", q);
  if (addr) cJSON_AddStringToObject(props, "address", addr);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = q;
  it.title           = name;
  it.summary         = addr;
  it.body            = bj;
  it.lang            = "pl";
  it.link            = url;
  it.record_type     = "krs-entity";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PL_KRS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj); free(addr); free(board);
  cJSON_Delete(root);
  fprintf(stderr, "[pl_krs] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;   /* honest empty is not an error */
}

static const source_def pl_krs_def = {
  .id = "PL_KRS", .collector = "osint",
  .name = "Poland KRS Registry Lookup", .name_ja = "ポーランド KRS 企業登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api-krs.ms.gov.pl",
  .description = "Poland KRS OPEN (api-krs.ms.gov.pl) — entity name, seat address, management board by 10-digit KRS number (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(pl_krs_def)
