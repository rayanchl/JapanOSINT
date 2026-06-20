/* collectors/osint/sources/tech_stack.c
 * OSINT service — TECH_STACK_DETECTION. On-demand (interval 0); ctx->entity =
 * a URL/domain. Fetches the target page once (no key) and fingerprints
 * technologies from the response body via the signature table below.
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per DETECTED technology
 * (remote_key="tech:<url>:<tech>", body={technology,category,url}). The
 * signature table stays a matching table — it is never emitted as data. If
 * the fetch fails or nothing is detected, emits NOTHING (honest empty). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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

/* Emit one intel row for a single detected technology. Returns 1 if emitted. */
static int emit_tech(intel_sink *sink, const char *url, const char *name,
                     const char *category) {
  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "technology", name);
  cJSON_AddStringToObject(body, "category", category);
  cJSON_AddStringToObject(body, "url", url);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "TECH_STACK_DETECTION");
  cJSON_AddStringToObject(props, "technology", name);
  cJSON_AddStringToObject(props, "category", category);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[640];
  snprintf(rk, sizeof rk, "tech:%s:%s", url, name);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = name;
  it.body            = bj;
  it.summary         = category;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"TECH_STACK_DETECTION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = ctx->entity;
  if (!url || !*url) return 0;

  char full[2048];
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    snprintf(full, sizeof full, "https://%s", url);
  else { strncpy(full, url, sizeof full - 1); full[sizeof full - 1] = 0; }

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", full, NULL, NULL, 0, 20000, 1, &hr);
  long status = hr.status;
  char *body = (hc == 0 && hr.body) ? strdup(hr.body) : NULL;
  http_response_free(&hr);

  if (hc != 0 || status != 200 || !body) { if (body) free(body); return 0; }

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
    if (found) emit_tech(sink, url, sig->name, sig->category);
  }
  free(body);
  return 0;   /* honest empty (nothing detected) is not an error */
}

static const source_def tech_stack_def = {
  .id = "TECH_STACK_DETECTION", .collector = "osint",
  .name = "Tech Stack Detection", .name_ja = "技術スタック検出",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/tech-stack-detection",
  .description = "Fingerprint web technologies from a page's response body.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(tech_stack_def)
