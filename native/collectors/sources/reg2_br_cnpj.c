/* collectors/sources/reg2_br_cnpj.c
 *
 * Brazil — two keyless CNPJ company pivots over Receita Federal's public open
 * data, both returning the QSA / socios list (partners and officers with role
 * and entry date), which is Brazil's de-facto beneficial-ownership view.
 *
 *  BR_MINHARECEITA  GET https://minhareceita.org/<14-digit CNPJ>
 *    Independent open-source (MIT) mirror of the complete Receita Federal CNPJ
 *    dataset; unthrottled, so this is the primary.
 *    Emits: razao_social, nome_fantasia, cnpj, situacao_cadastral /
 *    descricao_situacao_cadastral, cnae_fiscal + descricao, capital_social,
 *    logradouro/municipio/uf/cep, data_inicio_atividade, and qsa[] as
 *    {nome_socio, qualificacao_socio, data_entrada_sociedade}.
 *    Licence: open-source service over Receita Federal open data, no key.
 *
 *  BR_CNPJ_WS       GET https://publica.cnpj.ws/cnpj/<14-digit CNPJ>
 *    Emits: cnpj_raiz, razao_social, capital_social, natureza_juridica.descricao,
 *    porte.descricao, atualizado_em, estabelecimento.{situacao_cadastral,
 *    atividade_principal.descricao, city/state}, and socios[] as
 *    {nome, qualificacao_socio.descricao, data_entrada, cpf_cnpj_socio}.
 *    Partner CPFs are masked (***612937**) by the publisher — that is the
 *    correct public-record form and is passed through unchanged.
 *    Licence: redistribution of Receita Federal's public CNPJ open data; free
 *    tier is rate-limited to ~3 req/min, so this stays a pivot lookup and never
 *    a sweep. HTTP 429 → honest empty (return 0), never a fabricated row.
 *
 * Entity must reduce to exactly 14 digits (dots/slashes/dashes stripped);
 * anything else is a no-op. Neither endpoint returns coordinates, so no row
 * claims geo and no address is geocoded (R2).
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* entity -> 14 digit CNPJ, or 0 when it is not one. */
static int br_cnpj(const char *q, char *out15) {
  if (!q) return 0;
  int nd = 0;
  for (const char *p = q; *p && nd < 15; p++)
    if (isdigit((unsigned char)*p)) out15[nd++] = *p;
  out15[nd] = 0;
  return nd == 14;
}

/* Number-or-string field copied into props under `key`. */
static void br_any(cJSON *props, const cJSON *o, const char *field,
                   const char *key) {
  const cJSON *v = cJSON_GetObjectItem(o, field);
  if (!v) return;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    cJSON_AddStringToObject(props, key, v->valuestring);
  else if (cJSON_IsNumber(v))
    cJSON_AddNumberToObject(props, key, v->valuedouble);
}

/* ------------------------------------------------------- minhareceita.org */
static int mr_run(const source_ctx *ctx, intel_sink *sink) {
  char cnpj[16];
  if (!br_cnpj(ctx->entity, cnpj)) return 0;

  char url[96];
  snprintf(url, sizeof url, "https://minhareceita.org/%s", cnpj);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "BR_MINHARECEITA");
  if (!body) return 0;
  cJSON *r = cJSON_Parse(body);
  free(body);
  if (!r) return 0;

  const char *razao = jo_sv(r, "razao_social");
  if (!razao) { cJSON_Delete(r); return 0; }
  const char *fantasia = jo_sv(r, "nome_fantasia");
  const char *sit      = jo_sv(r, "descricao_situacao_cadastral");
  const char *cnae     = jo_sv(r, "cnae_fiscal_descricao");
  const char *uf       = jo_sv(r, "uf");
  const char *mun      = jo_sv(r, "municipio");
  const char *inicio   = jo_sv(r, "data_inicio_atividade");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BR_MINHARECEITA");
  cJSON_AddStringToObject(props, "source", "Receita Federal (via minhareceita.org)");
  cJSON_AddStringToObject(props, "cnpj", cnpj);
  cJSON_AddStringToObject(props, "razao_social", razao);
  if (fantasia) cJSON_AddStringToObject(props, "nome_fantasia", fantasia);
  if (sit)      cJSON_AddStringToObject(props, "situacao_cadastral", sit);
  br_any(props, r, "situacao_cadastral", "situacao_cadastral_codigo");
  if (cnae)     cJSON_AddStringToObject(props, "cnae_fiscal_descricao", cnae);
  br_any(props, r, "cnae_fiscal", "cnae_fiscal");
  br_any(props, r, "capital_social", "capital_social");
  if (jo_sv(r, "logradouro")) cJSON_AddStringToObject(props, "logradouro", jo_sv(r, "logradouro"));
  if (mun)    cJSON_AddStringToObject(props, "municipio", mun);
  if (uf)     cJSON_AddStringToObject(props, "uf", uf);
  if (jo_sv(r, "cep")) cJSON_AddStringToObject(props, "cep", jo_sv(r, "cep"));
  if (inicio) cJSON_AddStringToObject(props, "data_inicio_atividade", inicio);
  if (jo_sv(r, "porte")) cJSON_AddStringToObject(props, "porte", jo_sv(r, "porte"));

  cJSON *qsa = cJSON_CreateArray();
  int nsoc = 0;
  const cJSON *ql = cJSON_GetObjectItem(r, "qsa");
  if (cJSON_IsArray(ql)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, ql) {
      const char *nm = jo_sv(s, "nome_socio");
      if (!nm) continue;
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "nome_socio", nm);
      if (jo_sv(s, "qualificacao_socio"))
        cJSON_AddStringToObject(e, "qualificacao_socio", jo_sv(s, "qualificacao_socio"));
      if (jo_sv(s, "data_entrada_sociedade"))
        cJSON_AddStringToObject(e, "data_entrada_sociedade", jo_sv(s, "data_entrada_sociedade"));
      if (jo_sv(s, "cnpj_cpf_do_socio"))
        cJSON_AddStringToObject(e, "cnpj_cpf_do_socio", jo_sv(s, "cnpj_cpf_do_socio"));
      cJSON_AddItemToArray(qsa, e);
      nsoc++;
    }
  }
  cJSON_AddItemToObject(props, "qsa", qsa);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[460];
  snprintf(summary, sizeof summary, "CNPJ %s%s%s%s%s · %d socio(s)",
           cnpj,
           sit ? " · " : "", sit ? sit : "",
           uf ? " · " : "", uf ? uf : "", nsoc);

  intel_item it = {0};
  it.remote_key      = cnpj;
  it.title           = razao;
  it.summary         = summary;
  it.body            = pj;
  it.link            = url;
  it.lang            = "pt";
  it.published_at    = inicio;
  it.record_type     = "br-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"registry\",\"brazil\",\"beneficial-ownership\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(r);
  fprintf(stderr, "[BR_MINHARECEITA] emitted 1 (%s, %d socios)\n", cnpj, nsoc);
  return 0;
}

/* ---------------------------------------------------------- publica.cnpj.ws */
static int ws_run(const source_ctx *ctx, intel_sink *sink) {
  char cnpj[16];
  if (!br_cnpj(ctx->entity, cnpj)) return 0;

  char url[96];
  snprintf(url, sizeof url, "https://publica.cnpj.ws/cnpj/%s", cnpj);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "BR_CNPJ_WS");
  if (!body) return 0;                 /* 404 / 429 throttle → honest empty */
  cJSON *r = cJSON_Parse(body);
  free(body);
  if (!r) return 0;

  const char *razao = jo_sv(r, "razao_social");
  if (!razao) { cJSON_Delete(r); return 0; }

  const cJSON *nat  = cJSON_GetObjectItem(r, "natureza_juridica");
  const cJSON *por  = cJSON_GetObjectItem(r, "porte");
  const cJSON *est  = cJSON_GetObjectItem(r, "estabelecimento");
  const char *natd  = nat ? jo_sv(nat, "descricao") : NULL;
  const char *pord  = por ? jo_sv(por, "descricao") : NULL;
  const char *situ  = est ? jo_sv(est, "situacao_cadastral") : NULL;
  const char *atual = jo_sv(r, "atualizado_em");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BR_CNPJ_WS");
  cJSON_AddStringToObject(props, "source", "Receita Federal (via publica.cnpj.ws)");
  cJSON_AddStringToObject(props, "cnpj", cnpj);
  cJSON_AddStringToObject(props, "razao_social", razao);
  if (jo_sv(r, "cnpj_raiz")) cJSON_AddStringToObject(props, "cnpj_raiz", jo_sv(r, "cnpj_raiz"));
  br_any(props, r, "capital_social", "capital_social");
  if (natd)  cJSON_AddStringToObject(props, "natureza_juridica", natd);
  if (pord)  cJSON_AddStringToObject(props, "porte", pord);
  if (situ)  cJSON_AddStringToObject(props, "situacao_cadastral", situ);
  if (atual) cJSON_AddStringToObject(props, "atualizado_em", atual);
  if (est) {
    const cJSON *ap = cJSON_GetObjectItem(est, "atividade_principal");
    if (ap && jo_sv(ap, "descricao"))
      cJSON_AddStringToObject(props, "atividade_principal", jo_sv(ap, "descricao"));
    if (jo_sv(est, "logradouro"))
      cJSON_AddStringToObject(props, "logradouro", jo_sv(est, "logradouro"));
    const cJSON *cid = cJSON_GetObjectItem(est, "cidade");
    if (cid && jo_sv(cid, "nome"))
      cJSON_AddStringToObject(props, "cidade", jo_sv(cid, "nome"));
    const cJSON *uf = cJSON_GetObjectItem(est, "estado");
    if (uf && jo_sv(uf, "sigla"))
      cJSON_AddStringToObject(props, "uf", jo_sv(uf, "sigla"));
    if (jo_sv(est, "data_inicio_atividade"))
      cJSON_AddStringToObject(props, "data_inicio_atividade",
                              jo_sv(est, "data_inicio_atividade"));
  }

  cJSON *socios = cJSON_CreateArray();
  int nsoc = 0;
  const cJSON *sl = cJSON_GetObjectItem(r, "socios");
  if (cJSON_IsArray(sl)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, sl) {
      const char *nm = jo_sv(s, "nome");
      if (!nm) continue;
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "nome", nm);
      const cJSON *qs = cJSON_GetObjectItem(s, "qualificacao_socio");
      if (qs && jo_sv(qs, "descricao"))
        cJSON_AddStringToObject(e, "qualificacao_socio", jo_sv(qs, "descricao"));
      if (jo_sv(s, "data_entrada"))
        cJSON_AddStringToObject(e, "data_entrada", jo_sv(s, "data_entrada"));
      if (jo_sv(s, "cpf_cnpj_socio"))   /* masked by the publisher */
        cJSON_AddStringToObject(e, "cpf_cnpj_socio", jo_sv(s, "cpf_cnpj_socio"));
      if (jo_sv(s, "tipo"))
        cJSON_AddStringToObject(e, "tipo", jo_sv(s, "tipo"));
      cJSON_AddItemToArray(socios, e);
      nsoc++;
    }
  }
  cJSON_AddItemToObject(props, "socios", socios);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[460];
  snprintf(summary, sizeof summary, "CNPJ %s%s%s%s%s · %d socio(s)",
           cnpj,
           situ ? " · " : "", situ ? situ : "",
           natd ? " · " : "", natd ? natd : "", nsoc);

  intel_item it = {0};
  it.remote_key      = cnpj;
  it.title           = razao;
  it.summary         = summary;
  it.body            = pj;
  it.link            = url;
  it.lang            = "pt";
  it.record_type     = "br-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"registry\",\"brazil\",\"beneficial-ownership\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(r);
  fprintf(stderr, "[BR_CNPJ_WS] emitted 1 (%s, %d socios)\n", cnpj, nsoc);
  return 0;
}

static const source_def reg2_br_minhareceita_def = {
  .id = "BR_MINHARECEITA", .collector = "osint",
  .name = "Brazil Minha Receita CNPJ service",
  .update_interval_sec = 0, .run = mr_run,
  .category = "economy", .type = "api",
  .url = "https://minhareceita.org/",
  .description = "Open-source mirror of the complete Receita Federal CNPJ "
                 "dataset — full company record plus the QSA (quadro de socios "
                 "e administradores) in one keyless call.",
  .license = "Open-source (MIT) service over Receita Federal open data; free "
             "public instance, no key.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_br_minhareceita_def)

static const source_def reg2_br_cnpj_ws_def = {
  .id = "BR_CNPJ_WS", .collector = "osint",
  .name = "Brazil CNPJ open API (with shareholders)",
  .update_interval_sec = 0, .run = ws_run,
  .category = "economy", .type = "api",
  .url = "https://publica.cnpj.ws/cnpj/",
  .description = "CNPJ → full Receita Federal company record including the "
                 "socios (partner/officer) list with names, roles and entry "
                 "dates. Keyless; free tier rate-limited, so pivot-only.",
  .license = "Redistribution of Receita Federal's public CNPJ open data; free "
             "public tier is rate-limited (3 req/min).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_br_cnpj_ws_def)
