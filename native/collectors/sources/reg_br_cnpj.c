/* collectors/sources/reg_br_cnpj.c
 * OSINT service — Brazil CNPJ (open company registry). On-demand entity pivot.
 * If entity is a 14-digit CNPJ (dashes/dots/slashes stripped), GET the keyless,
 * open BrasilAPI CNPJ endpoint and emit one intel_item for the company with
 * razao_social / nome_fantasia / situacao / address / socios (partners).
 *
 * Keyless LIVE. Non-numeric (or non-14-digit) entity → honest empty (there is
 * no free razao-social name search here). Fetch failure / not found → nothing.
 * Nothing is synthesized: every emitted field comes from the parsed response. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Build "R. Foo, 123 - Bairro, Cidade - UF, CEP" from the flat address fields
 * BrasilAPI returns. malloc'd; caller frees. NULL if nothing usable. */
static char *br_address(const cJSON *r) {
  const char *log = jo_sv(r, "logradouro");
  const char *num = jo_sv(r, "numero");
  const char *comp = jo_sv(r, "complemento");
  const char *bairro = jo_sv(r, "bairro");
  const char *mun = jo_sv(r, "municipio");
  const char *uf = jo_sv(r, "uf");
  const char *cep = jo_sv(r, "cep");
  if (!log && !mun && !uf && !cep) return NULL;
  char buf[1024]; size_t j = 0;
  buf[0] = 0;
#define BR_APP(pfx, val) do { \
    if ((val)) { int n = snprintf(buf + j, sizeof buf - j, "%s%s", (pfx), (val)); \
      if (n > 0 && (size_t)n < sizeof buf - j) j += (size_t)n; } } while (0)
  BR_APP("", log);
  BR_APP(", ", num);
  BR_APP(" ", comp);
  BR_APP(" - ", bairro);
  BR_APP(", ", mun);
  BR_APP(" - ", uf);
  BR_APP(", CEP ", cep);
#undef BR_APP
  return buf[0] ? strdup(buf) : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  /* Strip CNPJ punctuation (dots, dashes, slashes, spaces); need exactly 14 digits. */
  char digits[16]; int nd = 0;
  for (const char *p = q; *p && nd < 15; p++)
    if (isdigit((unsigned char)*p)) digits[nd++] = *p;
  digits[nd] = 0;
  if (nd != 14) {
    fprintf(stderr, "[br_cnpj] not a 14-digit CNPJ (no free name search); honest empty\n");
    return 0;
  }

  char url[128];
  snprintf(url, sizeof url, "https://brasilapi.com.br/api/cnpj/v1/%s", digits);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "br_cnpj");
  if (!body) return 0;                 /* 404 not-found / network → honest empty */

  cJSON *r = cJSON_Parse(body);
  free(body);
  if (!r) return 0;

  const char *razao = jo_sv(r, "razao_social");
  const char *fantasia = jo_sv(r, "nome_fantasia");
  const char *situacao = jo_sv(r, "descricao_situacao_cadastral");
  const char *atividade = jo_sv(r, "cnae_fiscal_descricao");
  const char *abertura = jo_sv(r, "data_inicio_atividade");
  const char *email = jo_sv(r, "email");
  const char *phone = jo_sv(r, "ddd_telefone_1");
  if (!razao && !fantasia) { cJSON_Delete(r); return 0; }

  char *addr = br_address(r);

  /* socios (partners) — array of {nome_socio, qualificacao_socio, ...}. */
  cJSON *socios_out = cJSON_CreateArray();
  const cJSON *qsa = cJSON_GetObjectItem(r, "qsa");
  if (cJSON_IsArray(qsa)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, qsa) {
      const char *snome = jo_sv(s, "nome_socio");
      const char *squal = jo_sv(s, "qualificacao_socio");
      const char *sinicio = jo_sv(s, "data_entrada_sociedade");
      if (!snome) continue;
      cJSON *so = cJSON_CreateObject();
      cJSON_AddStringToObject(so, "nome", snome);
      if (squal) cJSON_AddStringToObject(so, "qualificacao", squal);
      if (sinicio) cJSON_AddStringToObject(so, "data_entrada", sinicio);
      cJSON_AddItemToArray(socios_out, so);
    }
  }

  cJSON *data = cJSON_CreateObject();
  if (razao) cJSON_AddStringToObject(data, "razao_social", razao);
  if (fantasia) cJSON_AddStringToObject(data, "nome_fantasia", fantasia);
  cJSON_AddStringToObject(data, "cnpj", digits);
  if (situacao) cJSON_AddStringToObject(data, "situacao", situacao);
  if (atividade) cJSON_AddStringToObject(data, "atividade_principal", atividade);
  if (addr) cJSON_AddStringToObject(data, "endereco", addr);
  if (email) cJSON_AddStringToObject(data, "email", email);
  if (phone) cJSON_AddStringToObject(data, "telefone", phone);
  if (abertura) cJSON_AddStringToObject(data, "data_inicio_atividade", abertura);
  cJSON_AddItemToObject(data, "socios", cJSON_Duplicate(socios_out, 1));
  cJSON_AddStringToObject(data, "source", "BrasilAPI/CNPJ");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BR_CNPJ");
  cJSON_AddStringToObject(props, "source", "BrasilAPI");
  cJSON_AddStringToObject(props, "cnpj", digits);
  if (situacao) cJSON_AddStringToObject(props, "situacao", situacao);
  if (addr) cJSON_AddStringToObject(props, "endereco", addr);
  cJSON_AddItemToObject(props, "socios", socios_out);   /* transfers ownership */
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = digits;
  it.title           = razao ? razao : fantasia;
  it.summary         = fantasia ? fantasia : situacao;
  it.body            = bj;
  it.lang            = "pt";
  it.published_at    = abertura;
  it.link            = url;
  it.record_type     = "br-cnpj-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"BR_CNPJ\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj); free(addr);
  cJSON_Delete(r);
  fprintf(stderr, "[br_cnpj] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

static const source_def br_cnpj_def = {
  .id = "BR_CNPJ", .collector = "osint",
  .name = "Brazil CNPJ Company Lookup", .name_ja = "ブラジル法人番号(CNPJ)照会",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/br_cnpj",
  .description = "Brazil CNPJ open registry (BrasilAPI) — razao social, situacao, address, socios; keyless, entity = 14-digit CNPJ",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(br_cnpj_def)
