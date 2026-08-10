/* collectors/sources/corp_lifecycle.c
 * OSINT services — corporate lifecycle. "Is this company changing, or dying?"
 * Three entity-pivot services, all keyed on a company/domain:
 *
 *   • VENDOR_VIABILITY      — LIVE, keyless. Three independent decay signals for
 *                             a vendor you depend on: RDAP domain expiry/status,
 *                             GitHub repo archived/disabled/last-push, and the
 *                             vendor's own Atlassian Statuspage summary.
 *   • UK_CH_FILING_HISTORY  — UK Companies House filing history + officer changes
 *                             + registered charges. Free key, HTTP Basic; gated on
 *                             COMPANIES_HOUSE_API_KEY (honest empty without).
 *   • GAZETTE_INSOLVENCY    — LIVE, keyless. The Gazette (UK official public
 *                             record) insolvency notices matching the entity:
 *                             winding-up, administration, liquidation.
 *
 * Endpoints:
 *   https://rdap.org/domain/<domain>                        (IANA RDAP bootstrap)
 *   https://api.github.com/repos/<owner>/<repo>             (keyless, 60 req/h)
 *   https://<status host>/api/v2/summary.json               (Atlassian Statuspage)
 *   https://api.company-information.service.gov.uk/company/<n>/{filing-history,
 *                                                            officers,charges}
 *   https://www.thegazette.co.uk/insolvency/notice/data.feed?text=<q>  (Atom)
 *
 * Licence/terms: RDAP and The Gazette are open public records. Statuspage's
 * /api/v2/ is documented by Atlassian as the public status API. GitHub's REST
 * API permits unauthenticated read at 60 req/h; GITHUB_TOKEN raises that.
 *
 * R1 NOTE — read before editing. VENDOR_VIABILITY is a composite, and the
 * tempting shape is one row saying {vendor: X, dying: true}. That row would be
 * a verdict computed from legs that may each have failed, which is exactly the
 * "names, not data" pattern the 2026-07-31 audit removed. So: ONE ROW PER LEG
 * THAT ACTUALLY RETURNED DATA, each carrying the fetched value. A leg that 404s
 * or is not applicable emits nothing. Correlation is the analyst's job, and the
 * shared remote_key prefix + tags make it a query.
 *
 * R2: none of these upstreams return coordinates, so no row here sets has_geo. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/rss_atom.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ helpers */

/* Emit one row. `data` is TAKEN OVER (printed, then deleted) — do not reuse it.
 * body and properties are the same printed JSON: these services have no prose
 * body, and core/intel.c's FTS mirror indexes `body`, so pointing it at the
 * fetched fields is what makes the row findable. Returns 1 if emitted. */
static int cl_emit(intel_sink *sink, const char *service, cJSON *data,
                   const char *record_type, const char *rk, const char *title,
                   const char *summary, const char *link, const char *published,
                   const char *tags) {
  if (!data) return 0;
  cJSON_AddStringToObject(data, "service", service);
  char *pj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);
  if (!pj) return 0;

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.link            = link;
  it.published_at    = published;
  it.lang            = "en";
  it.record_type     = record_type;
  it.body            = pj;
  it.properties_json = pj;
  it.tags_json       = tags;
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* base64, standard alphabet, padded. malloc'd; caller frees. */
static char *cl_b64(const unsigned char *in, size_t n) {
  static const char *T =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *out = malloc(((n + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = (unsigned)in[i] << 16;
    if (i + 1 < n) v |= (unsigned)in[i + 1] << 8;
    if (i + 2 < n) v |= (unsigned)in[i + 2];
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? T[v & 63] : '=';
  }
  out[o] = 0;
  return out;
}

/* Strip scheme and any path/port from an entity so "https://acme.com/x" and
 * "acme.com" both yield "acme.com". Writes at most cap-1 bytes. Returns 0 when
 * nothing host-shaped survives (no dot, or empty). */
static int cl_hostify(const char *s, char *out, size_t cap) {
  if (!s || !cap) return 0;
  const char *p = s;
  const char *sep = strstr(p, "://");
  if (sep) p = sep + 3;
  size_t i = 0;
  for (; p[i] && i + 1 < cap; i++) {
    char c = p[i];
    if (c == '/' || c == ':' || c == '?' || c == '#' || c == ' ') break;
    out[i] = (char)jo_lc(c);
  }
  out[i] = 0;
  return (i > 0 && strchr(out, '.') != NULL) ? 1 : 0;
}

/* ------------------------------------------- VENDOR_VIABILITY: RDAP domain leg */

/* Domain expiry + registry status. A domain in clientHold / pendingDelete /
 * redemptionPeriod is the loudest single "this vendor stopped paying" signal
 * that exists in public data. */
static int viab_rdap(const source_ctx *ctx, intel_sink *sink, const char *host) {
  char url[512];
  snprintf(url, sizeof url, "https://rdap.org/domain/%s", host);
  const char *hdrs[] = { "Accept: application/rdap+json, application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "vendor-viability/rdap");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *data = cJSON_CreateObject();
  jo_add_str(data, "domain", jo_sv(root, "ldhName"));
  cJSON_AddStringToObject(data, "signal", "rdap");

  /* events[] carries registration / expiration / last changed, each an ISO date */
  const char *expiry = NULL;
  cJSON *events = cJSON_GetObjectItem(root, "events");
  if (cJSON_IsArray(events)) {
    cJSON *ev;
    cJSON_ArrayForEach(ev, events) {
      const char *act = jo_sv(ev, "eventAction");
      const char *when = jo_sv(ev, "eventDate");
      if (!act || !when) continue;
      if (!strcmp(act, "expiration"))        { expiry = when; jo_add_str(data, "expires_at", when); }
      else if (!strcmp(act, "registration")) jo_add_str(data, "registered_at", when);
      else if (!strcmp(act, "last changed")) jo_add_str(data, "last_changed_at", when);
    }
  }

  /* status[] — the distress flags live here, verbatim from the registry. */
  int distress = 0;
  cJSON *status = cJSON_GetObjectItem(root, "status");
  if (cJSON_IsArray(status)) {
    cJSON_AddItemToObject(data, "status", cJSON_Duplicate(status, 1));
    cJSON *s;
    cJSON_ArrayForEach(s, status) {
      const char *v = jo_vstr(s);
      if (!v) continue;
      if (jo_stristr(v, "hold") || jo_stristr(v, "pending delete") ||
          jo_stristr(v, "pendingdelete") || jo_stristr(v, "redemption") ||
          jo_stristr(v, "inactive"))
        distress = 1;
    }
  }
  if (distress) cJSON_AddBoolToObject(data, "registry_distress", 1);

  /* `expiry` borrows from `root`, and cl_emit runs after root is freed — so
   * copy it out first. Same reason for every stack copy in this file. */
  char exp[64] = {0};
  if (expiry) snprintf(exp, sizeof exp, "%s", expiry);
  cJSON_Delete(root);

  char rk[320], title[400];
  snprintf(rk, sizeof rk, "viability:rdap:%s", host);
  snprintf(title, sizeof title, "Domain registry status — %s%s", host,
           distress ? " (registry distress flag set)" : "");
  return cl_emit(sink, "VENDOR_VIABILITY", data, "vendor-signal", rk, title,
                 exp[0] ? exp : NULL, NULL, exp[0] ? exp : NULL,
                 "[\"osint-search\",\"vendor-viability\",\"rdap\"]");
}

/* --------------------------------------- VENDOR_VIABILITY: GitHub archive leg */

/* Only runs when the entity is literally "owner/repo". Deriving a repo from a
 * domain would be a guess, and a guessed repo that 404s reads identically to a
 * healthy project with no GitHub presence — so we do not guess (R1). */
static int viab_github(const source_ctx *ctx, intel_sink *sink, const char *slug) {
  const char *slash = strchr(slug, '/');
  if (!slash || !slash[1] || strchr(slash + 1, '/')) return 0;

  char url[512];
  snprintf(url, sizeof url, "https://api.github.com/repos/%s", slug);
  char auth[300];
  const char *tok = jo_env("GITHUB_TOKEN");
  const char *hdrs_tok[] = { auth, "Accept: application/vnd.github+json", NULL };
  const char *hdrs_anon[] = { "Accept: application/vnd.github+json", NULL };
  const char *const *hdrs = hdrs_anon;
  if (tok) {
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", tok);
    hdrs = hdrs_tok;
  }

  char *body = jo_get(ctx, url, hdrs, "vendor-viability/github");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *full = jo_sv(root, "full_name");
  if (!full) { cJSON_Delete(root); return 0; }

  cJSON *arch = cJSON_GetObjectItem(root, "archived");
  cJSON *dis  = cJSON_GetObjectItem(root, "disabled");
  int archived = cJSON_IsTrue(arch), disabled = cJSON_IsTrue(dis);
  const char *pushed = jo_sv(root, "pushed_at");

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "signal", "github");
  jo_add_str(data, "repo", full);
  if (arch) cJSON_AddBoolToObject(data, "archived", archived);
  if (dis)  cJSON_AddBoolToObject(data, "disabled", disabled);
  jo_add_str(data, "pushed_at", pushed);
  jo_add_str(data, "updated_at", jo_sv(root, "updated_at"));
  jo_copy_num(data, root, "open_issues_count");
  jo_copy_num(data, root, "stargazers_count");
  jo_copy_num(data, root, "subscribers_count");
  const char *link = jo_sv(root, "html_url");
  char linkbuf[400] = {0};
  if (link) snprintf(linkbuf, sizeof linkbuf, "%s", link);

  char rk[320], title[420];
  snprintf(rk, sizeof rk, "viability:github:%s", full);
  snprintf(title, sizeof title, "Repository health — %s%s", full,
           archived ? " (ARCHIVED)" : (disabled ? " (disabled)" : ""));

  char push[64] = {0};
  if (pushed) snprintf(push, sizeof push, "%s", pushed);   /* borrows from root */
  cJSON_Delete(root);

  return cl_emit(sink, "VENDOR_VIABILITY", data, "vendor-signal", rk, title,
                 push[0] ? push : NULL, linkbuf[0] ? linkbuf : NULL,
                 push[0] ? push : NULL,
                 "[\"osint-search\",\"vendor-viability\",\"github\"]");
}

/* ----------------------------------------- VENDOR_VIABILITY: Statuspage leg */

/* <host>/api/v2/summary.json is Atlassian's documented public status API. The
 * "status." prefix is the near-universal convention, so it is added unless the
 * entity already names the status host directly.
 *
 * A miss is silent, and that is correct: verified 2026-08-09, status.datadoghq
 * .com / status.hashicorp.com / www.githubstatus.com / metastatuspage.com all
 * answer 200, while status.stripe.com is a bespoke page that 404s this path and
 * status.gitlab.com does not exist. Plenty of healthy vendors do not run
 * Statuspage, so absence says nothing about them and must emit nothing. */
static int viab_statuspage(const source_ctx *ctx, intel_sink *sink, const char *host) {
  char url[512];
  if (strncmp(host, "status.", 7) == 0 || strstr(host, "statuspage.com"))
    snprintf(url, sizeof url, "https://%s/api/v2/summary.json", host);
  else
    snprintf(url, sizeof url, "https://status.%s/api/v2/summary.json", host);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "vendor-viability/statuspage");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *st = cJSON_GetObjectItem(root, "status");
  const char *desc = jo_sv(st, "description");
  const char *ind  = jo_sv(st, "indicator");
  if (!desc && !ind) { cJSON_Delete(root); return 0; }   /* not a Statuspage */

  cJSON *page = cJSON_GetObjectItem(root, "page");
  cJSON *incidents = cJSON_GetObjectItem(root, "incidents");
  cJSON *components = cJSON_GetObjectItem(root, "components");

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "signal", "statuspage");
  jo_add_str(data, "page_name", jo_sv(page, "name"));
  jo_add_str(data, "indicator", ind);
  jo_add_str(data, "description", desc);
  jo_add_str(data, "page_updated_at", jo_sv(page, "updated_at"));
  if (cJSON_IsArray(incidents))
    cJSON_AddNumberToObject(data, "open_incidents", cJSON_GetArraySize(incidents));
  if (cJSON_IsArray(components))
    cJSON_AddNumberToObject(data, "components", cJSON_GetArraySize(components));

  const char *updated = jo_sv(page, "updated_at");
  char upd[64] = {0};
  if (updated) snprintf(upd, sizeof upd, "%s", updated);

  /* page.url is the canonical status page; fall back to the URL we fetched
   * rather than re-deriving one that might not match. Sized to hold `url`
   * whole, so neither branch can truncate. */
  char link[sizeof url];
  const char *purl = jo_sv(page, "url");
  if (purl) snprintf(link, sizeof link, "%s", purl);
  else      snprintf(link, sizeof link, "%s", url);

  /* desc/ind borrow from root; the title and the emit both outlive it. */
  char sdesc[256] = {0};
  snprintf(sdesc, sizeof sdesc, "%s", desc ? desc : (ind ? ind : "unknown"));
  cJSON_Delete(root);

  char rk[320], title[420];
  snprintf(rk, sizeof rk, "viability:statuspage:%s", host);
  snprintf(title, sizeof title, "Service status — %s: %s", host, sdesc);
  return cl_emit(sink, "VENDOR_VIABILITY", data, "vendor-signal", rk, title,
                 sdesc, link, upd[0] ? upd : NULL,
                 "[\"osint-search\",\"vendor-viability\",\"statuspage\"]");
}

static int run_viability(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_looks_like_keyword(ctx->entity)) return 0;

  int n = 0;
  /* "owner/repo" is the GitHub form; anything else is treated as a host. */
  if (strchr(ctx->entity, '/') && !strstr(ctx->entity, "://")) {
    n += viab_github(ctx, sink, ctx->entity);
  } else {
    char host[256];
    if (!cl_hostify(ctx->entity, host, sizeof host)) {
      fprintf(stderr, "[vendor-viability] entity is neither owner/repo nor a host\n");
      return 0;
    }
    n += viab_rdap(ctx, sink, host);
    n += viab_statuspage(ctx, sink, host);
  }
  fprintf(stderr, "[vendor-viability] emitted %d\n", n);
  return 0;                                    /* honest empty is not an error */
}

/* ------------------------------------------------- UK_CH_FILING_HISTORY */

/* Build the Basic-auth header for Companies House: username=key, empty
 * password → base64("<key>:"). Writes into `out`; returns 0 on failure. */
static int ch_auth(const char *key, char *out, size_t cap) {
  char raw[256];
  snprintf(raw, sizeof raw, "%s:", key);
  char *b = cl_b64((const unsigned char *)raw, strlen(raw));
  if (!b) return 0;
  snprintf(out, cap, "Authorization: Basic %s", b);
  free(b);
  return 1;
}

/* A UK company number is 8 chars: 8 digits, or 2 letters + 6 digits. */
static int ch_is_company_number(const char *s) {
  size_t n = strlen(s);
  if (n != 8) return 0;
  int lead_alpha = isalpha((unsigned char)s[0]) && isalpha((unsigned char)s[1]);
  size_t start = lead_alpha ? 2 : 0;
  for (size_t i = start; i < n; i++)
    if (!isdigit((unsigned char)s[i])) return 0;
  return 1;
}

/* Resolve a company NAME to its number via the search endpoint. Returns 1 and
 * fills `out` on success. The dispatcher hands services free-text entities, so
 * without this the service 404s on every human-shaped query. */
static int ch_resolve(const source_ctx *ctx, const char *const *hdrs,
                      const char *name, char *out, size_t cap) {
  char *enc = jo_urlencode(name);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
           "https://api.company-information.service.gov.uk"
           "/search/companies?q=%s&items_per_page=1", enc);
  free(enc);
  char *body = jo_get(ctx, url, hdrs, "uk-ch-filing/resolve");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int ok = 0;
  cJSON *items = cJSON_GetObjectItem(root, "items");
  if (cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0) {
    const char *num = jo_sv(cJSON_GetArrayItem(items, 0), "company_number");
    if (num) { snprintf(out, cap, "%s", num); ok = 1; }
  }
  cJSON_Delete(root);
  return ok;
}

/* Emit every object in `arr` as one row, mapping the fields each Companies
 * House sub-resource actually returns. `kind` selects the field mapping. */
static int ch_emit_items(intel_sink *sink, const char *num, const char *kind,
                         cJSON *arr) {
  if (!cJSON_IsArray(arr)) return 0;
  int n = 0, idx = 0;
  cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    idx++;
    const char *date = NULL, *label = NULL, *extra = NULL;
    if (!strcmp(kind, "filing")) {
      date  = jo_sv(it, "date");
      label = jo_sv(it, "description");
      extra = jo_sv(it, "type");
    } else if (!strcmp(kind, "officer")) {
      date  = jo_sv(it, "resigned_on");
      if (!date) date = jo_sv(it, "appointed_on");
      label = jo_sv(it, "name");
      extra = jo_sv(it, "officer_role");
    } else {                                   /* charge */
      date  = jo_sv(it, "delivered_on");
      label = jo_sv(it, "status");
      extra = jo_sv(it, "charge_number");
    }
    if (!label) continue;                      /* no label → not a row (R1) */

    cJSON *data = cJSON_Duplicate(it, 1);
    if (!data) continue;
    cJSON_AddStringToObject(data, "company_number", num);
    cJSON_AddStringToObject(data, "record_kind", kind);

    char rk[300], title[440], link[300];
    snprintf(rk, sizeof rk, "ukch:%s:%s:%s:%d", num, kind, date ? date : "?", idx);
    snprintf(title, sizeof title, "%s %s — %s%s%s",
             num, kind, label, extra ? " / " : "", extra ? extra : "");
    snprintf(link, sizeof link,
             "https://find-and-update.company-information.service.gov.uk/company/%s",
             num);
    n += cl_emit(sink, "UK_CH_FILING_HISTORY", data, "uk-company-event", rk,
                 title, extra, link, date,
                 "[\"osint-search\",\"companies-house\",\"corp-lifecycle\"]");
  }
  return n;
}

static int run_uk_ch(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  const char *key = jo_env("COMPANIES_HOUSE_API_KEY");
  if (!key) {
    fprintf(stderr, "[uk-ch-filing] gated (no COMPANIES_HOUSE_API_KEY)\n");
    return 0;
  }
  char auth[400];
  if (!ch_auth(key, auth, sizeof auth)) return 0;
  const char *hdrs[] = { auth, "Accept: application/json", NULL };

  char num[64];
  if (ch_is_company_number(ctx->entity)) {
    snprintf(num, sizeof num, "%s", ctx->entity);
  } else if (!ch_resolve(ctx, hdrs, ctx->entity, num, sizeof num)) {
    fprintf(stderr, "[uk-ch-filing] no company matched '%s'\n", ctx->entity);
    return 0;                                  /* honest empty, no row (R1) */
  }

  static const struct { const char *path, *node, *kind; } LEGS[] = {
    { "filing-history", "items", "filing"  },
    { "officers",       "items", "officer" },
    { "charges",        "items", "charge"  },
  };
  int n = 0;
  for (size_t i = 0; i < sizeof LEGS / sizeof LEGS[0]; i++) {
    char url[512];
    snprintf(url, sizeof url,
             "https://api.company-information.service.gov.uk/company/%s/%s?items_per_page=50",
             num, LEGS[i].path);
    char *body = jo_get(ctx, url, hdrs, "uk-ch-filing");
    if (!body) continue;                       /* 404 on charges is normal */
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) continue;
    n += ch_emit_items(sink, num, LEGS[i].kind,
                       cJSON_GetObjectItem(root, LEGS[i].node));
    cJSON_Delete(root);
  }
  fprintf(stderr, "[uk-ch-filing] emitted %d\n", n);
  return 0;
}

/* --------------------------------------------------- GAZETTE_INSOLVENCY */

static int run_gazette(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_looks_like_keyword(ctx->entity)) return 0;
  char *enc = jo_urlencode(ctx->entity);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
           "https://www.thegazette.co.uk/insolvency/notice/data.feed?text=%s", enc);
  free(enc);
  int n = rss_collect(ctx, sink, url, "en",
                      "[\"osint-search\",\"insolvency\",\"corp-lifecycle\"]");
  fprintf(stderr, "[gazette-insolvency] emitted %d\n", n < 0 ? 0 : n);
  return n < 0 ? -1 : 0;
}

/* ------------------------------------------------------------- definitions */

static const source_def vendor_viability_def = {
  .id = "VENDOR_VIABILITY", .collector = "osint",
  .name = "Vendor Viability Signals", .name_ja = "ベンダー存続シグナル",
  .update_interval_sec = 0, .run = run_viability,
  .category = "commercial", .type = "api",
  .url = "internal://osint/vendor-viability",
  .description = "Decay signals for a vendor or dependency: RDAP domain expiry and "
                 "registry hold flags, GitHub repo archived/last-push (entity "
                 "'owner/repo'), and the vendor's public Statuspage summary. One row "
                 "per signal actually fetched — no computed verdict.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(vendor_viability_def)

static const source_def uk_ch_filing_def = {
  .id = "UK_CH_FILING_HISTORY", .collector = "osint",
  .name = "UK Companies House Events", .name_ja = "英国会社登記イベント",
  .update_interval_sec = 0, .run = run_uk_ch,
  .category = "government", .type = "api",
  .url = "internal://osint/uk-ch-filing-history",
  .description = "UK Companies House filing history, officer appointments and "
                 "resignations, and registered charges for a company number or name "
                 "(needs free COMPANIES_HOUSE_API_KEY).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(uk_ch_filing_def)

static const source_def gazette_insolvency_def = {
  .id = "GAZETTE_INSOLVENCY", .collector = "osint",
  .name = "The Gazette Insolvency Notices", .name_ja = "英国官報 破産・清算公告",
  .update_interval_sec = 0, .run = run_gazette,
  .category = "government", .type = "web_request",
  .url = "https://www.thegazette.co.uk/insolvency/notice",
  .description = "Statutory insolvency notices from the UK official public record — "
                 "winding-up petitions, administration, liquidation — matching a "
                 "company name. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(gazette_insolvency_def)
