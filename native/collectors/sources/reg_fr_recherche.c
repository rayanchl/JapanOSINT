/* collectors/sources/reg_fr_recherche.c
 * FR_ENTREPRISES — France "recherche-entreprises" open-government company
 * registry (recherche-entreprises.api.gouv.fr). Keyless, LIVE. On-demand
 * company pivot: ctx->entity = company name (or SIREN). One intel_item per
 * matched establishment, keyed by SIREN. Emits nom, siren, siege address,
 * dirigeants and activite — all REAL parsed data. No entity / fetch failure /
 * no results → honest empty (return 0). Nothing fabricated. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Build a human-readable single-line address from a siege object. Returns a
 * malloc'd string (caller frees) or NULL when nothing usable is present. */
static char *fr_siege_addr(const cJSON *siege) {
  if (!siege) return NULL;
  const char *geo = jo_sv(siege, "geo_adresse");
  if (geo) return strdup(geo);
  const char *adr  = jo_sv(siege, "adresse");
  const char *cp   = jo_sv(siege, "code_postal");
  const char *ville = jo_sv(siege, "libelle_commune");
  if (!adr && !cp && !ville) return NULL;
  char buf[512];
  snprintf(buf, sizeof buf, "%s%s%s%s%s",
           adr ? adr : "",
           adr && (cp || ville) ? ", " : "",
           cp ? cp : "",
           cp && ville ? " " : "",
           ville ? ville : "");
  return strdup(buf);
}

/* one company result → one intel_item. Returns 1 if emitted. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  const char *nom = jo_sv(c, "nom_complet");
  if (!nom) nom = jo_sv(c, "nom_raison_sociale");
  const char *siren = jo_sv(c, "siren");
  if (!nom && !siren) return 0;

  const cJSON *siege = cJSON_GetObjectItem(c, "siege");
  char *addr = fr_siege_addr(siege);
  const char *activite = jo_sv(c, "activite_principale");
  const char *cat = jo_sv(c, "categorie_entreprise");
  const char *etat = jo_sv(c, "etat_administratif");
  const char *created = jo_sv(c, "date_creation");

  /* dirigeants: array of {nom, prenoms|prenom, denomination, qualite} */
  cJSON *dirs_out = cJSON_CreateArray();
  const cJSON *dirs = cJSON_GetObjectItem(c, "dirigeants");
  if (cJSON_IsArray(dirs)) {
    const cJSON *d;
    cJSON_ArrayForEach(d, dirs) {
      const char *dn   = jo_sv(d, "nom");
      const char *dp   = jo_sv(d, "prenoms");
      if (!dp) dp = jo_sv(d, "prenom");
      const char *deno = jo_sv(d, "denomination");
      const char *qual = jo_sv(d, "qualite");
      char who[256];
      if (deno)                who[0] = 0, snprintf(who, sizeof who, "%s", deno);
      else if (dn && dp)       snprintf(who, sizeof who, "%s %s", dp, dn);
      else if (dn)             snprintf(who, sizeof who, "%s", dn);
      else                     continue;
      cJSON *dobj = cJSON_CreateObject();
      cJSON_AddStringToObject(dobj, "name", who);
      if (qual) cJSON_AddStringToObject(dobj, "role", qual);
      cJSON_AddItemToArray(dirs_out, dobj);
    }
  }

  cJSON *data = cJSON_CreateObject();
  if (nom)      cJSON_AddStringToObject(data, "nom", nom);
  if (siren)    cJSON_AddStringToObject(data, "siren", siren);
  if (addr)     cJSON_AddStringToObject(data, "siege", addr);
  if (activite) cJSON_AddStringToObject(data, "activite_principale", activite);
  if (cat)      cJSON_AddStringToObject(data, "categorie_entreprise", cat);
  if (etat)     cJSON_AddStringToObject(data, "etat_administratif", etat);
  if (created)  cJSON_AddStringToObject(data, "date_creation", created);
  cJSON_AddItemToObject(data, "dirigeants", cJSON_Duplicate(dirs_out, 1));
  cJSON_AddStringToObject(data, "source", "recherche-entreprises.api.gouv.fr");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "FR_ENTREPRISES");
  cJSON_AddStringToObject(props, "source", "recherche-entreprises.api.gouv.fr");
  if (siren)    cJSON_AddStringToObject(props, "siren", siren);
  if (addr)     cJSON_AddStringToObject(props, "siege", addr);
  if (activite) cJSON_AddStringToObject(props, "activite_principale", activite);
  cJSON_AddItemToObject(props, "dirigeants", dirs_out);   /* transfers ownership */
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[128];
  if (siren) snprintf(link, sizeof link,
                      "https://annuaire-entreprises.data.gouv.fr/entreprise/%s", siren);
  else link[0] = 0;

  intel_item it = {0};
  it.remote_key      = siren ? siren : nom;
  it.title           = nom ? nom : siren;
  it.summary         = addr;
  it.body            = bj;
  it.lang            = "fr";
  it.published_at    = created;
  it.link            = siren ? link : NULL;
  it.record_type     = "fr-entreprise";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"FR_ENTREPRISES\"]";
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
    "https://recherche-entreprises.api.gouv.fr/search?q=%s&per_page=15", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "fr_entreprises");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *c;
    cJSON_ArrayForEach(c, results) emitted += emit_company(sink, c);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[fr_entreprises] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def fr_entreprises_def = {
  .id = "FR_ENTREPRISES", .collector = "osint",
  .name = "France Recherche-Entreprises", .name_ja = "フランス企業登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/fr_entreprises",
  .description = "France recherche-entreprises open-gov registry — nom, SIREN, siege, dirigeants, activite (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(fr_entreprises_def)
