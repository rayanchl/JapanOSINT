/* collectors/osint/sources/maigret.c
 * OSINT service — faithful port of OSINTsaas osint_tools/maigret.c
 * (handle_social_deep_scan → maigret_search_username). Canonical service
 * SOCIAL_ANALYZER (osint_dispatcher.c service_registry[]:
 * {SERVICE_SOCIAL_ANALYZER, handle_social_deep_scan, "SOCIAL_ANALYZER"}).
 * On-demand (interval 0); ctx->entity = a username. Pure-C per-site HTTP
 * enumeration (no key) — the exact maigret.c sites_database + check_username_
 * advanced detection (status_code / response_text / response_url, optional
 * regex_check) reproduced verbatim. OSINTsaas ran the site loop across pthreads;
 * native sources are single-threaded so the loop is sequential — output JSON is
 * identical (order-independent: only found profiles are emitted).
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per CONFIRMED-FOUND
 * profile (keyed "profile:<site>:<username>"), not a single summary blob. Each
 * row carries the profile site/url/category/response_time so individual hits
 * surface as distinct search items, with link = the profile URL. Only the live
 * per-site probe's confirmed hits are emitted; if none are found, emits nothing
 * (honest empty — no seeded rows). */
#include "source.h"
#include "social_fuse.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <time.h>
#include <sys/time.h>

typedef struct {
  const char *name;
  const char *url_template;
  const char *check_type;   /* status_code | response_text | response_url */
  const char *error_code;
  const char *error_text;
  const char *username_claimed;
  const char *regex_check;
  const char *category;
} site_data_t;

/* Verbatim from maigret.c sites_database. */
static const site_data_t sites_database[] = {
  {"GitHub","https://github.com/{}","status_code","404","","","","social"},
  {"Twitter","https://twitter.com/{}","status_code","404","","","","social"},
  {"Instagram","https://www.instagram.com/{}/","status_code","404","","","","social"},
  {"Facebook","https://www.facebook.com/{}","response_text","","not found","","","social"},
  {"LinkedIn","https://www.linkedin.com/in/{}/","status_code","404","","","","professional"},
  {"Reddit","https://www.reddit.com/user/{}","status_code","404","","","","social"},
  {"Pinterest","https://www.pinterest.com/{}/","status_code","404","","","","social"},
  {"YouTube","https://www.youtube.com/@{}","response_text","","404 Not Found","","","media"},
  {"TikTok","https://www.tiktok.com/@{}","status_code","404","","","","media"},
  {"Twitch","https://www.twitch.tv/{}","status_code","404","","","","gaming"},
  {"AngelList","https://angel.co/u/{}","status_code","404","","","","professional"},
  {"Behance","https://www.behance.net/{}","status_code","404","","","","creative"},
  {"Dribbble","https://dribbble.com/{}","status_code","404","","","","creative"},
  {"DeviantArt","https://www.deviantart.com/{}","status_code","404","","","","creative"},
  {"Steam","https://steamcommunity.com/id/{}","response_text","","could not be found","","","gaming"},
  {"Xbox","https://xboxgamertag.com/search/{}","response_text","","Gamertag does not exist","","","gaming"},
  {"PlayStation","https://my.playstation.com/profile/{}","status_code","404","","","","gaming"},
  {"Roblox","https://www.roblox.com/users/profile?username={}","response_text","","User Not Found","","","gaming"},
  {"GitLab","https://gitlab.com/{}","status_code","404","","","","development"},
  {"BitBucket","https://bitbucket.org/{}/","status_code","404","","","","development"},
  {"CodePen","https://codepen.io/{}","status_code","404","","","","development"},
  {"HackerOne","https://hackerone.com/{}","status_code","404","","","","security"},
  {"BugCrowd","https://bugcrowd.com/{}","status_code","404","","","","security"},
  {"Medium","https://medium.com/@{}","status_code","404","","","","blogging"},
  {"Substack","https://{}.substack.com/","status_code","404","","","","blogging"},
  {"Patreon","https://www.patreon.com/{}","status_code","404","","","","content"},
  {"Tumblr","https://{}.tumblr.com/","status_code","404","","","","blogging"},
  {"SoundCloud","https://soundcloud.com/{}","status_code","404","","","","music"},
  {"Spotify","https://open.spotify.com/user/{}","status_code","404","","","","music"},
  {"Bandcamp","https://{}.bandcamp.com/","status_code","404","","","","music"},
  {"Last.fm","https://www.last.fm/user/{}","status_code","404","","","","music"},
  {"Discord","https://discord.com/users/{}","status_code","404","","","","communication"},
  {"Telegram","https://t.me/{}","response_text","","tgme_page_title","","","communication"},
  {"CashApp","https://cash.app/${}","status_code","404","","","","finance"},
  {"Venmo","https://venmo.com/{}","status_code","404","","","","finance"},
  {"HackerNews","https://news.ycombinator.com/user?id={}","response_text","","No such user","","","tech"},
  {"StackOverflow","https://stackoverflow.com/users/0/{}","status_code","404","","","","tech"},
  {"Quora","https://www.quora.com/profile/{}","status_code","404","","","","knowledge"},
  {"VK","https://vk.com/{}","response_text","","404","","","social"},
  {"AboutMe","https://about.me/{}","status_code","404","","","","personal"},
  {"Linktree","https://linktr.ee/{}","status_code","404","","","","personal"},
  {"Carrd","https://{}.carrd.co/","status_code","404","","","","personal"},
};

static void replace_placeholder(char *dst, size_t cap, const char *src,
                                const char *user) {
  const char *pos = strstr(src, "{}");
  if (pos) {
    size_t pre = (size_t)(pos - src);
    if (pre >= cap) pre = cap - 1;
    memcpy(dst, src, pre);
    dst[pre] = '\0';
    strncat(dst, user, cap - strlen(dst) - 1);
    strncat(dst, pos + 2, cap - strlen(dst) - 1);
  } else {
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
  }
}

static int validate_regex(const char *text, const char *pat) {
  if (!pat || !*pat) return 1;
  regex_t re;
  if (regcomp(&re, pat, REG_EXTENDED) != 0) return 1;
  int r = regexec(&re, text, 0, NULL, 0);
  regfree(&re);
  return r == 0;
}

/* Emit one intel row for a single confirmed-found profile. Returns 1 if emitted. */
static int emit_profile(intel_sink *sink, const char *username,
                        const site_data_t *s, const char *url, double ms) {
  /* Per-record body: the actual profile hit. */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "site", s->name);
  cJSON_AddStringToObject(data, "url", url);
  cJSON_AddStringToObject(data, "category", s->category);
  cJSON_AddNumberToObject(data, "response_time", ms);
  cJSON_AddStringToObject(data, "username", username);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SOCIAL_ANALYZER");
  cJSON_AddStringToObject(props, "entity", username);
  cJSON_AddStringToObject(props, "site", s->name);
  cJSON_AddStringToObject(props, "category", s->category);
  char *pj = cJSON_PrintUnformatted(props);

  /* Stable per-profile key = site + username (unique within this service). */
  char rk[400];
  snprintf(rk, sizeof rk, "profile:%s:%s", s->name, username);
  char title[400];
  snprintf(title, sizeof title, "%s: %s", s->name, username);
  char summary[400];
  snprintf(summary, sizeof summary, "%s on %s", username, s->name);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.link            = url;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SOCIAL_ANALYZER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

int jo_maigret_run(const source_ctx *ctx, intel_sink *sink) {
  const char *username = ctx->entity;
  if (!username || !*username) return -1;

  int num_sites = (int)(sizeof sites_database / sizeof sites_database[0]);
  int emitted = 0;

  for (int i = 0; i < num_sites; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const site_data_t *s = &sites_database[i];
    char url[512];
    replace_placeholder(url, sizeof url, s->url_template, username);

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    http_response hr = {0};
    int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 15000, 0, &hr);
    gettimeofday(&t1, NULL);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
                (t1.tv_usec - t0.tv_usec) / 1000.0;

    int found = 0;
    if (hc == 0 && hr.body) {
      if (strcmp(s->check_type, "status_code") == 0) {
        int expected = atoi(s->error_code);
        found = ((int)hr.status != expected);
      } else if (strcmp(s->check_type, "response_text") == 0) {
        if (s->error_text && *s->error_text)
          found = (strstr(hr.body, s->error_text) == NULL);
        else if (s->username_claimed && *s->username_claimed)
          found = (strstr(hr.body, s->username_claimed) != NULL);
      } else if (strcmp(s->check_type, "response_url") == 0) {
        found = (hr.status == 200);
      }
      if (found && s->regex_check && *s->regex_check)
        found = validate_regex(hr.body, s->regex_check);
    }
    http_response_free(&hr);

    /* Per-record emit: only confirmed-found profiles surface as rows. */
    if (found) emitted += emit_profile(sink, username, s, url, ms);
  }

  /* Honest empty: no confirmed profiles → emit nothing. */
  (void)emitted;
  return 0;
}

/* Fused into SOCIAL_USERNAME — exposed via social_fuse.h as jo_maigret_run. */
