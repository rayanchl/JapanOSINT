/* collectors/osint/sources/tech_stack.c
 * OSINT service — port of OSINTsaas osint_tools/tech_stack.c (handle_tech_stack).
 * Canonical SERVICE = TECH_STACK_DETECTION (dispatcher row
 * {SERVICE_DOMAIN_REPUTATION, handle_tech_stack, "TECH_STACK_DETECTION", true}).
 * On-demand (interval 0); ctx->entity = a URL/domain. Upstream: a single GET of
 * the target page (no key); technologies are fingerprinted from the response
 * body via the SAME signature table & regex matching as the C original
 * (detect_tech). Faithful note: the OSINTsaas tool only ever matched against
 * response->data (body), including its header-pattern regex "Header:.*value";
 * http_request likewise exposes only the body, so behaviour is identical.
 * Result envelope mirrors detect_tech() output (url,http_code,detected[],
 * by_category{},technologies_found) wrapped in the standard service envelope;
 * confidence 75 like result->confidence_score. Emits ONE osint_service_result
 * row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

typedef struct {
  const char *name, *category, *pattern, *header, *header_pattern;
} tech_signature_t;

static const tech_signature_t signatures[] = {
  {"React", "js-framework", "react(-dom)?[\"'].*[0-9]+\\.[0-9]+", NULL, NULL},
  {"Vue.js", "js-framework", "vue[\"'].*[0-9]+\\.[0-9]+|Vue\\.js", NULL, NULL},
  {"Angular", "js-framework", "ng-version|angular[\"']", NULL, NULL},
  {"jQuery", "js-library", "jquery.*[0-9]+\\.[0-9]+|jQuery", NULL, NULL},
  {"Next.js", "js-framework", "_next/static|__NEXT_DATA__", NULL, NULL},
  {"Nuxt.js", "js-framework", "__NUXT__|_nuxt/", NULL, NULL},
  {"WordPress", "cms", "/wp-content/|/wp-includes/", NULL, NULL},
  {"Drupal", "cms", "Drupal|drupal\\.org", NULL, NULL},
  {"Joomla", "cms", "/media/jui/|Joomla!", NULL, NULL},
  {"Shopify", "ecommerce", "cdn\\.shopify\\.com|Shopify", NULL, NULL},
  {"Magento", "ecommerce", "Mage\\.Cookies|/skin/frontend/", NULL, NULL},
  {"WooCommerce", "ecommerce", "woocommerce", NULL, NULL},
  {"nginx", "web-server", NULL, "Server", "nginx"},
  {"Apache", "web-server", NULL, "Server", "Apache"},
  {"LiteSpeed", "web-server", NULL, "Server", "LiteSpeed"},
  {"IIS", "web-server", NULL, "Server", "Microsoft-IIS"},
  {"Cloudflare", "cdn", NULL, "Server", "cloudflare"},
  {"Cloudflare", "cdn", NULL, "cf-ray", ".*"},
  {"Akamai", "cdn", NULL, "X-Akamai-Transformed", ".*"},
  {"Fastly", "cdn", NULL, "X-Fastly-Request-ID", ".*"},
  {"AWS CloudFront", "cdn", NULL, "X-Amz-Cf-Id", ".*"},
  {"HSTS", "security", NULL, "Strict-Transport-Security", ".*"},
  {"CSP", "security", NULL, "Content-Security-Policy", ".*"},
  {"X-Frame-Options", "security", NULL, "X-Frame-Options", ".*"},
  {"X-XSS-Protection", "security", NULL, "X-XSS-Protection", ".*"},
  {"PHP", "backend", NULL, "X-Powered-By", "PHP"},
  {"ASP.NET", "backend", NULL, "X-Powered-By", "ASP\\.NET"},
  {"Express.js", "backend", NULL, "X-Powered-By", "Express"},
  {"Django", "backend", NULL, "X-Frame-Options", "SAMEORIGIN"},
  {"Ruby on Rails", "backend", NULL, "X-Request-Id", ".*"},
  {"Google Analytics", "analytics", "google-analytics\\.com|gtag\\(|ga\\(", NULL, NULL},
  {"Google Tag Manager", "analytics", "googletagmanager\\.com", NULL, NULL},
  {"Facebook Pixel", "analytics", "connect\\.facebook\\.net|fbevents\\.js", NULL, NULL},
  {"Hotjar", "analytics", "hotjar\\.com", NULL, NULL},
  {"Webpack", "build-tool", "webpackJsonp|/webpack/", NULL, NULL},
  {"Vite", "build-tool", "/@vite/|/@fs/", NULL, NULL},
  {"Bootstrap", "css-framework", "bootstrap.*css|bootstrap.*js", NULL, NULL},
  {"Tailwind CSS", "css-framework", "tailwind", NULL, NULL},
  {"Font Awesome", "icon-library", "fontawesome|fa-[a-z]", NULL, NULL},
  {"reCAPTCHA", "security", "recaptcha|grecaptcha", NULL, NULL},
};

static int emit_result(intel_sink *sink, const char *entity, int success,
                        int confidence, cJSON *data /*owned*/, const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "TECH_STACK_DETECTION");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "techstack:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "TECH_STACK_DETECTION — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "tech stack detected" : (error ? error : "fetch failed");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"TECH_STACK_DETECTION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = ctx->entity;
  if (!url || !*url) return -1;

  char full[2048];
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    snprintf(full, sizeof full, "https://%s", url);
  else { strncpy(full, url, sizeof full - 1); full[sizeof full - 1] = 0; }

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", full, NULL, NULL, 0, 20000, 1, &hr);
  long status = hr.status;
  char *body = (hc == 0 && hr.body) ? strdup(hr.body) : NULL;
  http_response_free(&hr);

  cJSON *res = cJSON_CreateObject();
  cJSON_AddStringToObject(res, "url", url);

  if (hc != 0 || status != 200 || !body) {
    cJSON_AddStringToObject(res, "error", "Failed to fetch URL");
    cJSON_AddNumberToObject(res, "http_code", (double)status);
    if (body) free(body);
    return emit_result(sink, url, 0, 75, res, "Failed to fetch URL");
  }

  cJSON_AddNumberToObject(res, "http_code", (double)status);
  cJSON *detected = cJSON_CreateArray();
  cJSON *categories = cJSON_CreateObject();
  int n = (int)(sizeof signatures / sizeof signatures[0]);

  for (int i = 0; i < n; i++) {
    const tech_signature_t *sig = &signatures[i];
    int found = 0;
    if (sig->pattern) {
      regex_t re;
      if (regcomp(&re, sig->pattern, REG_EXTENDED | REG_ICASE) == 0) {
        if (regexec(&re, body, 0, NULL, 0) == 0) found = 1;
        regfree(&re);
      }
    }
    if (!found && sig->header && sig->header_pattern) {
      char hs[256];
      snprintf(hs, sizeof hs, "%s:.*%s", sig->header, sig->header_pattern);
      regex_t re;
      if (regcomp(&re, hs, REG_EXTENDED | REG_ICASE) == 0) {
        if (regexec(&re, body, 0, NULL, 0) == 0) found = 1;
        regfree(&re);
      }
    }
    if (found) {
      cJSON *tech = cJSON_CreateObject();
      cJSON_AddStringToObject(tech, "name", sig->name);
      cJSON_AddStringToObject(tech, "category", sig->category);
      cJSON_AddItemToArray(detected, tech);
      cJSON *ca = cJSON_GetObjectItem(categories, sig->category);
      if (!ca) { ca = cJSON_CreateArray(); cJSON_AddItemToObject(categories, sig->category, ca); }
      cJSON_AddItemToArray(ca, cJSON_CreateString(sig->name));
    }
  }
  free(body);

  cJSON_AddItemToObject(res, "detected", detected);
  cJSON_AddItemToObject(res, "by_category", categories);
  int tf = cJSON_GetArraySize(detected);
  cJSON_AddNumberToObject(res, "technologies_found", tf);

  /* handle_tech_stack root shape (single-entity dispatch) */
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "service", "TECH_STACK_DETECTION");
  cJSON_AddNumberToObject(root, "urls_analyzed", 1);
  cJSON_AddNumberToObject(root, "total_technologies_found", tf);
  cJSON *results = cJSON_CreateArray();
  cJSON_AddItemToArray(results, res);
  cJSON_AddItemToObject(root, "results", results);

  return emit_result(sink, url, 1, 75, root, NULL);
}

static const source_def tech_stack_def = {
  .id = "TECH_STACK_DETECTION", .collector = "osint",
  .name = "Tech Stack Detection", .name_ja = "技術スタック検出",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(tech_stack_def)
