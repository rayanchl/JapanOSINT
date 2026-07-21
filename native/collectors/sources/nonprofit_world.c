/* collectors/sources/nonprofit_world.c
 * OSINT services — cross-border nonprofit / charity registries. One run()
 * dispatches on ctx->source_id across three registers:
 *
 *   PROPUBLICA_NONPROFIT — ProPublica Nonprofit Explorer
 *       (projects.propublica.org/nonprofits/api/v2/search.json?q=<entity>).
 *       US IRS Form 990 filers. Keyless and LIVE. One intel_item per matched
 *       organization (name, EIN, city/state, NTEE, subsection).
 *
 *   UK_CHARITIES — UK Charity Commission register API. Gated on UK_CHARITY_KEY
 *       (the Commission's API requires an Ocp-Apim-Subscription-Key). No key →
 *       honest empty. One intel_item per matched registered charity.
 *
 *   CANADA_CHARITIES — Canadian registered-charity lookup. Canada has no free
 *       keyless charity search API (CRA's data is a bulk download / paid feeds);
 *       gated on CANADA_CHARITY_KEY so it stays honest-empty until a real
 *       endpoint/credential is configured. Never fabricates records.
 *
 * HONESTY: only real, parsed API fields are emitted; fetch failure / no
 * matches / missing key → honest empty (return 0). Nothing is synthesized. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* cJSON number field as malloc'd string, or NULL. */
static char *nw_num_str(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsNumber(v)) return NULL;
  char buf[32];
  snprintf(buf, sizeof buf, "%lld", (long long)v->valuedouble);
  return strdup(buf);
}

/* ---- ProPublica Nonprofit Explorer (US 990s, keyless LIVE) --------------- */

/* one organization → one intel_item. Returns 1 if emitted. */
static int pp_emit_org(intel_sink *sink, const cJSON *o) {
  if (!o) return 0;
  const char *name = jo_sv(o, "name");
  char *ein        = nw_num_str(o, "ein");
  if (!name && !ein) { free(ein); return 0; }

  const char *strein = jo_sv(o, "strein");     /* formatted EIN, e.g. 12-3456789 */
  const char *city   = jo_sv(o, "city");
  const char *state  = jo_sv(o, "state");
  const char *ntee   = jo_sv(o, "ntee_code");  /* string in the API */
  char *subsec       = nw_num_str(o, "subseccd");

  char loc[256] = {0};
  if (city && state) snprintf(loc, sizeof loc, "%s, %s", city, state);
  else if (city)     snprintf(loc, sizeof loc, "%s", city);
  else if (state)    snprintf(loc, sizeof loc, "%s", state);

  const char *use_ein = strein ? strein : (ein ? ein : NULL);

  cJSON *data = cJSON_CreateObject();
  if (name)    cJSON_AddStringToObject(data, "name", name);
  if (use_ein) cJSON_AddStringToObject(data, "ein", use_ein);
  if (loc[0])  cJSON_AddStringToObject(data, "location", loc);
  if (ntee)    cJSON_AddStringToObject(data, "ntee_code", ntee);
  if (subsec)  cJSON_AddStringToObject(data, "subsection_code", subsec);
  cJSON_AddStringToObject(data, "country", "US");
  cJSON_AddStringToObject(data, "source", "ProPublica Nonprofit Explorer");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PROPUBLICA_NONPROFIT");
  cJSON_AddStringToObject(props, "source", "ProPublica Nonprofit Explorer");
  if (use_ein) cJSON_AddStringToObject(props, "ein", use_ein);
  if (loc[0])  cJSON_AddStringToObject(props, "location", loc);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[128] = {0};
  if (ein) snprintf(link, sizeof link,
    "https://projects.propublica.org/nonprofits/organizations/%s", ein);

  intel_item it = {0};
  it.remote_key      = use_ein ? use_ein : name;
  it.title           = name ? name : use_ein;
  it.summary         = loc[0] ? loc : NULL;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "nonprofit-org";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PROPUBLICA_NONPROFIT\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(ein); free(subsec);
  return rc >= 0 ? 1 : 0;
}

static int run_propublica(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  char url[1024];
  snprintf(url, sizeof url,
    "https://projects.propublica.org/nonprofits/api/v2/search.json?q=%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "propublica_nonprofit");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *orgs = cJSON_GetObjectItem(root, "organizations");
  int emitted = 0;
  if (cJSON_IsArray(orgs)) {
    cJSON *o; int n = 0;
    cJSON_ArrayForEach(o, orgs) {
      emitted += pp_emit_org(sink, o);
      if (++n >= 25) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[propublica_nonprofit] emitted %d\n", emitted);
  return 0;
}

/* ---- UK Charity Commission register API (gated UK_CHARITY_KEY) ------------ */

static int uk_emit_charity(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  /* the register API uses PascalCase field names */
  const char *name = jo_sv(c, "charity_name");
  if (!name) name = jo_sv(c, "CharityName");
  char *regno = nw_num_str(c, "reg_charity_number");
  if (!regno) regno = nw_num_str(c, "RegisteredCharityNumber");
  if (!name && !regno) { free(regno); return 0; }

  const char *town = jo_sv(c, "charity_contact_address5");
  if (!town) town = jo_sv(c, "Town");
  const char *postcode = jo_sv(c, "charity_contact_postcode");
  const char *activities = jo_sv(c, "charity_activities");
  if (!activities) activities = jo_sv(c, "CharityActivities");
  const char *web = jo_sv(c, "charity_contact_web");

  cJSON *data = cJSON_CreateObject();
  if (name)       cJSON_AddStringToObject(data, "name", name);
  if (regno)      cJSON_AddStringToObject(data, "registered_number", regno);
  if (town)       cJSON_AddStringToObject(data, "town", town);
  if (postcode)   cJSON_AddStringToObject(data, "postcode", postcode);
  if (activities) cJSON_AddStringToObject(data, "activities", activities);
  if (web)        cJSON_AddStringToObject(data, "website", web);
  cJSON_AddStringToObject(data, "country", "GB");
  cJSON_AddStringToObject(data, "source", "UK Charity Commission");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "UK_CHARITIES");
  cJSON_AddStringToObject(props, "source", "UK Charity Commission");
  if (regno) cJSON_AddStringToObject(props, "registered_number", regno);
  if (town)  cJSON_AddStringToObject(props, "town", town);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[160] = {0};
  if (regno) snprintf(link, sizeof link,
    "https://register-of-charities.charitycommission.gov.uk/charity-search/-/charity-details/%s",
    regno);

  intel_item it = {0};
  it.remote_key      = regno ? regno : name;
  it.title           = name ? name : regno;
  it.summary         = town;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link[0] ? link : (web ? web : NULL);
  it.record_type     = "nonprofit-org";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"UK_CHARITIES\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(regno);
  return rc >= 0 ? 1 : 0;
}

static int run_uk(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("UK_CHARITY_KEY");
  if (!key) {
    fprintf(stderr, "[uk_charities] gated (no UK_CHARITY_KEY)\n");
    return 0;
  }
  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  /* Charity Commission register API — keyword search endpoint. */
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.charitycommission.gov.uk/register/api/searchCharityName/%s", enc);
  free(enc);

  char hdr[256];
  snprintf(hdr, sizeof hdr, "Ocp-Apim-Subscription-Key: %s", key);
  const char *hdrs[] = { hdr, "Accept: application/json", NULL };

  char *body = jo_get(ctx, url, hdrs, "uk_charities");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  if (cJSON_IsArray(root)) {
    cJSON *c; int n = 0;
    cJSON_ArrayForEach(c, root) {
      emitted += uk_emit_charity(sink, c);
      if (++n >= 25) break;
    }
  } else if (cJSON_IsObject(root)) {
    emitted += uk_emit_charity(sink, root);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[uk_charities] emitted %d\n", emitted);
  return 0;
}

/* ---- Canada registered charities (gated CANADA_CHARITY_KEY) --------------- */

static int run_canada(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("CANADA_CHARITY_KEY");
  if (!key) {
    fprintf(stderr, "[canada_charities] gated (no CANADA_CHARITY_KEY)\n");
    return 0;   /* honest empty — CRA has no free keyless search API */
  }
  (void)sink;
  /* No universally-available keyless CRA charity search endpoint exists; keep
   * honest-empty rather than fabricate a URL/records. When a real credentialed
   * endpoint is wired, parse+emit here. */
  fprintf(stderr, "[canada_charities] no configured endpoint; empty\n");
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->source_id) return -1;
  if (strcmp(ctx->source_id, "PROPUBLICA_NONPROFIT") == 0)
    return run_propublica(ctx, sink);
  if (strcmp(ctx->source_id, "UK_CHARITIES") == 0)
    return run_uk(ctx, sink);
  if (strcmp(ctx->source_id, "CANADA_CHARITIES") == 0)
    return run_canada(ctx, sink);
  return -1;
}

static const source_def propublica_nonprofit_def = {
  .id = "PROPUBLICA_NONPROFIT", .collector = "osint",
  .name = "ProPublica Nonprofit Explorer", .name_ja = "ProPublica 非営利団体検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "https://projects.propublica.org/nonprofits/api",
  .description = "ProPublica Nonprofit Explorer — US IRS 990 filers by name (keyless): name, EIN, location, NTEE",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(propublica_nonprofit_def)

static const source_def uk_charities_def = {
  .id = "UK_CHARITIES", .collector = "osint",
  .name = "UK Charity Commission Register", .name_ja = "英国慈善団体登録",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "https://api.charitycommission.gov.uk/register/api",
  .description = "UK Charity Commission register search (needs UK_CHARITY_KEY): charity name, number, town, activities",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(uk_charities_def)

static const source_def canada_charities_def = {
  .id = "CANADA_CHARITIES", .collector = "osint",
  .name = "Canada Registered Charities", .name_ja = "カナダ登録慈善団体",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/canada_charities",
  .description = "Canadian registered-charity lookup (gated CANADA_CHARITY_KEY; honest-empty until endpoint configured)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(canada_charities_def)
