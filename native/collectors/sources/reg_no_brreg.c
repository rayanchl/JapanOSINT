/* collectors/sources/reg_no_brreg.c
 * OSINT service — NO_BRREG. On-demand company pivot (entity = company name)
 * against Norway's Brønnøysund Register Centre Enhetsregisteret
 * (data.brreg.no), the authoritative, fully-open Norwegian business register.
 * No key required — keyless LIVE. One intel_item per matched entity, keyed by
 * organisasjonsnummer (orgnr). Emits org name, orgnr, org form, address and
 * industry (NACE) code. No matches / fetch failure → emits nothing. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Build "Street, 0123 City" from Brønnøysund address object (arrays of lines). */
static char *brreg_addr(const cJSON *a) {
  if (!cJSON_IsObject(a)) return NULL;
  char buf[512]; size_t j = 0; buf[0] = 0;
  const cJSON *lines = cJSON_GetObjectItem(a, "adresse");
  if (cJSON_IsArray(lines)) {
    const cJSON *ln;
    cJSON_ArrayForEach(ln, lines) {
      if (!cJSON_IsString(ln) || !ln->valuestring || !ln->valuestring[0]) continue;
      int n = snprintf(buf + j, sizeof buf - j, "%s%s", j ? ", " : "", ln->valuestring);
      /* snprintf returns the length it WOULD have written, so on truncation j
       * ran PAST the buffer — and the break below happened only after the
       * increment, leaving the post-loop snprintf writing at `buf + j` with a
       * `sizeof buf - j` that wrapped to ~SIZE_MAX. Clamp to the terminator. */
      if (n > 0) j += (size_t)n;
      if (j >= sizeof buf - 1) { j = sizeof buf - 1; break; }
    }
  }
  const char *post = jo_sv(a, "postnummer");
  const char *city = jo_sv(a, "poststed");
  if (post || city)
    snprintf(buf + j, sizeof buf - j, "%s%s%s%s", j ? ", " : "",
             post ? post : "", (post && city) ? " " : "", city ? city : "");
  return buf[0] ? strdup(buf) : NULL;
}

/* one entity → one intel_item. Returns 1 if emitted. */
static int emit_enhet(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  const char *orgnr = jo_sv(c, "organisasjonsnummer");
  const char *name  = jo_sv(c, "navn");
  if (!name && !orgnr) return 0;

  const cJSON *form = cJSON_GetObjectItem(c, "organisasjonsform");
  const char *formcode = form ? jo_sv(form, "kode") : NULL;
  const char *formdesc = form ? jo_sv(form, "beskrivelse") : NULL;

  const cJSON *nace = cJSON_GetObjectItem(c, "naeringskode1");
  const char *nacecode = nace ? jo_sv(nace, "kode") : NULL;
  const char *nacedesc = nace ? jo_sv(nace, "beskrivelse") : NULL;

  const char *reg = jo_sv(c, "registreringsdatoEnhetsregisteret");
  char *addr = brreg_addr(cJSON_GetObjectItem(c, "forretningsadresse"));

  cJSON *data = cJSON_CreateObject();
  if (name)     cJSON_AddStringToObject(data, "name", name);
  if (orgnr)    cJSON_AddStringToObject(data, "orgnr", orgnr);
  if (formcode) cJSON_AddStringToObject(data, "org_form_code", formcode);
  if (formdesc) cJSON_AddStringToObject(data, "org_form", formdesc);
  if (nacecode) cJSON_AddStringToObject(data, "industry_code", nacecode);
  if (nacedesc) cJSON_AddStringToObject(data, "industry", nacedesc);
  if (addr)     cJSON_AddStringToObject(data, "address", addr);
  if (reg)      cJSON_AddStringToObject(data, "registered", reg);
  cJSON_AddStringToObject(data, "source", "Brønnøysund Enhetsregisteret");
  cJSON_AddStringToObject(data, "country", "NO");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  char link[128] = {0};
  if (orgnr)
    snprintf(link, sizeof link,
             "https://virksomhet.brreg.no/nb/oppslag/enheter/%s", orgnr);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "NO_BRREG");
  cJSON_AddStringToObject(props, "source", "Brønnøysund Enhetsregisteret");
  cJSON_AddStringToObject(props, "country", "NO");
  if (orgnr)    cJSON_AddStringToObject(props, "orgnr", orgnr);
  if (formcode) cJSON_AddStringToObject(props, "org_form", formcode);
  if (nacecode) cJSON_AddStringToObject(props, "industry_code", nacecode);
  if (addr)     cJSON_AddStringToObject(props, "address", addr);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = orgnr ? orgnr : name;
  it.title           = name ? name : orgnr;
  it.summary         = addr ? addr : formdesc;
  it.body            = bj;
  it.lang            = "no";
  it.published_at    = reg;
  it.link            = orgnr ? link : NULL;
  it.record_type     = "brreg-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"NO_BRREG\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(addr);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  snprintf(url, sizeof url,
    "https://data.brreg.no/enhetsregisteret/api/enheter?navn=%s&size=15", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "no_brreg");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* HAL/JSON: { "_embedded": { "enheter": [ ... ] } } */
  int emitted = 0;
  const cJSON *emb = cJSON_GetObjectItem(root, "_embedded");
  const cJSON *arr = emb ? cJSON_GetObjectItem(emb, "enheter") : NULL;
  if (cJSON_IsArray(arr)) {
    const cJSON *c;
    cJSON_ArrayForEach(c, arr) emitted += emit_enhet(sink, c);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[no_brreg] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def no_brreg_def = {
  .id = "NO_BRREG", .collector = "osint",
  .name = "Norway Brønnøysund Company Registry",
  .name_ja = "ノルウェー ブレンネイスン企業登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/no_brreg",
  .description = "Norway Brønnøysund Enhetsregisteret — open company registry (name, orgnr, form, address, NACE); keyless",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(no_brreg_def)
