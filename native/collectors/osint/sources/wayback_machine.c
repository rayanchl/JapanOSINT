/* collectors/osint/sources/wayback_machine.c
 * OSINT service — port of OSINTsaas osint_tools/wayback_machine.c
 * (handle_wayback_machine). Canonical SERVICE = WAYBACK_MACHINE (dispatcher row
 * {SERVICE_WAYBACK_MACHINE, handle_wayback_machine, "WAYBACK_MACHINE", true};
 * ARCHIVE_SEARCH aliases the same handler — WAYBACK_MACHINE is the first/dedup
 * name). On-demand (interval 0); ctx->entity = a URL/domain. Upstream: Internet
 * Archive (archive.org/wayback/available, web.archive.org CDX) — no key. Skips
 * email entities like the C original. Faithfully reproduces check_availability,
 * get_snapshots(limit 50), search_related_urls(limit 20) plus the six static
 * additional-archive descriptors (Archive.is, Ghost Archive, Google Cache,
 * Bing, Google). Result mirrors the per-entity object the C builder produced,
 * wrapped in the standard envelope; confidence 90. Emits ONE
 * osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* encodeURIComponent-style (matches asn_lookup.c uri_encode). */
static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

static cJSON *get_json(http_client *http, const char *url) {
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return j;
}

static cJSON *check_availability(http_client *http, const char *target) {
  cJSON *result = cJSON_CreateObject();
  char enc[1024], url[1100];
  uri_encode(target, enc, sizeof enc);
  snprintf(url, sizeof url, "https://archive.org/wayback/available?url=%s", enc);
  cJSON *json = get_json(http, url);
  if (!json) { cJSON_AddBoolToObject(result, "available", 0); return result; }
  const cJSON *arch = cJSON_GetObjectItem(json, "archived_snapshots");
  if (arch) {
    const cJSON *closest = cJSON_GetObjectItem(arch, "closest");
    if (closest) {
      cJSON_AddBoolToObject(result, "available", 1);
      const cJSON *s = cJSON_GetObjectItem(closest, "status");
      const cJSON *t = cJSON_GetObjectItem(closest, "timestamp");
      const cJSON *u = cJSON_GetObjectItem(closest, "url");
      if (s && cJSON_IsString(s)) cJSON_AddStringToObject(result, "status", s->valuestring);
      if (t && cJSON_IsString(t)) cJSON_AddStringToObject(result, "latest_snapshot", t->valuestring);
      if (u && cJSON_IsString(u)) cJSON_AddStringToObject(result, "snapshot_url", u->valuestring);
    } else cJSON_AddBoolToObject(result, "available", 0);
  }
  cJSON_Delete(json);
  return result;
}

static cJSON *get_snapshots(http_client *http, const char *target, int limit) {
  cJSON *snaps = cJSON_CreateArray();
  char enc[1024], url[1200];
  uri_encode(target, enc, sizeof enc);
  snprintf(url, sizeof url,
           "https://web.archive.org/cdx/search/cdx?url=%s&output=json&limit=%d",
           enc, limit);
  cJSON *json = get_json(http, url);
  if (json && cJSON_IsArray(json)) {
    int n = cJSON_GetArraySize(json);
    for (int i = 1; i < n; i++) {
      const cJSON *row = cJSON_GetArrayItem(json, i);
      if (!(row && cJSON_IsArray(row) && cJSON_GetArraySize(row) >= 7)) continue;
      cJSON *snap = cJSON_CreateObject();
      const cJSON *ts = cJSON_GetArrayItem(row, 1);
      const cJSON *orig = cJSON_GetArrayItem(row, 2);
      const cJSON *mt = cJSON_GetArrayItem(row, 3);
      const cJSON *sc = cJSON_GetArrayItem(row, 4);
      const cJSON *ln = cJSON_GetArrayItem(row, 6);
      if (ts && ts->valuestring) {
        cJSON_AddStringToObject(snap, "timestamp", ts->valuestring);
        if (strlen(ts->valuestring) >= 14) {
          char d[32];
          snprintf(d, sizeof d, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s",
                   ts->valuestring, ts->valuestring + 4, ts->valuestring + 6,
                   ts->valuestring + 8, ts->valuestring + 10, ts->valuestring + 12);
          cJSON_AddStringToObject(snap, "date", d);
        }
        if (orig && orig->valuestring) {
          char su[2100];
          snprintf(su, sizeof su, "https://web.archive.org/web/%s/%s",
                   ts->valuestring, orig->valuestring);
          cJSON_AddStringToObject(snap, "url", su);
          cJSON_AddStringToObject(snap, "original_url", orig->valuestring);
        }
      }
      if (mt && mt->valuestring) cJSON_AddStringToObject(snap, "mimetype", mt->valuestring);
      if (sc && sc->valuestring) cJSON_AddStringToObject(snap, "status_code", sc->valuestring);
      if (ln && ln->valuestring) cJSON_AddStringToObject(snap, "size_bytes", ln->valuestring);
      cJSON_AddItemToArray(snaps, snap);
    }
    cJSON_Delete(json);
  } else if (json) cJSON_Delete(json);
  return snaps;
}

static cJSON *search_related(http_client *http, const char *domain, int limit) {
  cJSON *results = cJSON_CreateArray();
  char url[1200];
  snprintf(url, sizeof url,
           "https://web.archive.org/cdx/search/cdx?url=%s/*&output=json&limit=%d&collapse=urlkey",
           domain, limit);
  cJSON *json = get_json(http, url);
  if (json && cJSON_IsArray(json)) {
    int n = cJSON_GetArraySize(json);
    for (int i = 1; i < n; i++) {
      const cJSON *row = cJSON_GetArrayItem(json, i);
      if (!(row && cJSON_IsArray(row) && cJSON_GetArraySize(row) >= 3)) continue;
      const cJSON *orig = cJSON_GetArrayItem(row, 2);
      if (orig && orig->valuestring) {
        cJSON *u = cJSON_CreateObject();
        cJSON_AddStringToObject(u, "url", orig->valuestring);
        cJSON_AddItemToArray(results, u);
      }
    }
    cJSON_Delete(json);
  } else if (json) cJSON_Delete(json);
  return results;
}

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
  cJSON_AddStringToObject(props, "service", "WAYBACK_MACHINE");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "wayback:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "WAYBACK_MACHINE — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "wayback snapshots" : (error ? error : "no snapshots");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WAYBACK_MACHINE\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static cJSON *static_archive(const char *src, const char *enc, const char *fmt1,
                             const char *key1, const char *fmt2, const char *key2,
                             const char *stype, const char *note) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", src);
  cJSON_AddBoolToObject(r, "real_data", 1);
  if (fmt1 && key1) { char u[600]; snprintf(u, sizeof u, fmt1, enc); cJSON_AddStringToObject(r, key1, u); }
  if (fmt2 && key2) { char u[600]; snprintf(u, sizeof u, fmt2, enc); cJSON_AddStringToObject(r, key2, u); }
  if (stype) cJSON_AddStringToObject(r, "service_type", stype);
  if (note) cJSON_AddStringToObject(r, "note", note);
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;
  if (strchr(entity, '@')) return emit_result(sink, entity, 0, 90, NULL, "skipped (email)");

  char enc[1024];
  uri_encode(entity, enc, sizeof enc);

  cJSON *er = cJSON_CreateObject();
  cJSON_AddStringToObject(er, "target", entity);
  cJSON_AddItemToObject(er, "availability", check_availability(ctx->http, entity));

  cJSON *snaps = get_snapshots(ctx->http, entity, 50);
  int sc = cJSON_GetArraySize(snaps);
  cJSON_AddNumberToObject(er, "total_snapshots", sc);
  if (sc > 0) {
    const cJSON *first = cJSON_GetArrayItem(snaps, 0);
    const cJSON *last = cJSON_GetArrayItem(snaps, sc - 1);
    const cJSON *fd = first ? cJSON_GetObjectItem(first, "date") : NULL;
    const cJSON *ld = last ? cJSON_GetObjectItem(last, "date") : NULL;
    if (fd && fd->valuestring) cJSON_AddStringToObject(er, "first_archived", fd->valuestring);
    if (ld && ld->valuestring) cJSON_AddStringToObject(er, "last_archived", ld->valuestring);
  }
  cJSON_AddItemToObject(er, "snapshots", snaps);

  cJSON *related = search_related(ctx->http, entity, 20);
  cJSON_AddNumberToObject(er, "related_urls_found", cJSON_GetArraySize(related));
  cJSON_AddItemToObject(er, "related_urls", related);

  cJSON *as = cJSON_CreateArray();
  cJSON_AddItemToArray(as, static_archive("Archive.is", enc,
    "https://archive.is/%s", "search_url",
    "https://archive.is/?run=1&url=%s", "create_archive_url",
    "Web Archive", "Independent archive service, good for news sites"));
  cJSON_AddItemToArray(as, static_archive("GhostArchive", enc,
    "https://ghostarchive.org/search?term=%s", "search_url",
    NULL, NULL, "Decentralized Web Archive", NULL));
  {
    cJSON *gc = cJSON_CreateObject();
    cJSON_AddStringToObject(gc, "source", "GoogleCache");
    cJSON_AddBoolToObject(gc, "real_data", 1);
    char cu[600]; snprintf(cu, sizeof cu,
      "https://webcache.googleusercontent.com/search?q=cache:%s", entity);
    cJSON_AddStringToObject(gc, "cache_url", cu);
    char su[600]; snprintf(su, sizeof su, "https://www.google.com/search?q=cache:%s", enc);
    cJSON_AddStringToObject(gc, "search_url", su);
    cJSON_AddStringToObject(gc, "service_type", "Search Engine Cache");
    cJSON_AddStringToObject(gc, "note", "May not always be available");
    cJSON_AddItemToArray(as, gc);
  }
  cJSON_AddItemToArray(as, static_archive("Bing", enc,
    "https://www.bing.com/search?q=url:%s", "search_url",
    NULL, NULL, "Search Engine", NULL));
  cJSON_AddItemToArray(as, static_archive("Google", enc,
    "https://www.google.com/search?q=\"%s\"", "exact_search_url",
    "https://www.google.com/search?q=site:%s", "site_search_url",
    "Search Engine", NULL));
  cJSON_AddItemToObject(er, "additional_archives", as);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "service", "WAYBACK_MACHINE");
  cJSON_AddNumberToObject(root, "total_snapshots_found", sc);
  cJSON *all = cJSON_CreateArray();
  cJSON_AddItemToArray(all, er);
  cJSON_AddItemToObject(root, "results", all);
  cJSON_AddNumberToObject(root, "archive_sources_checked", 6);
  cJSON_AddStringToObject(root, "sources",
    "Internet Archive, Archive.is, Ghost Archive, Google Cache, Bing, Google");

  return emit_result(sink, entity, 1, 90, root, NULL);
}

static const source_def wayback_machine_def = {
  .id = "WAYBACK_MACHINE", .collector = "osint",
  .name = "Wayback Machine", .name_ja = "ウェイバックマシン",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(wayback_machine_def)
