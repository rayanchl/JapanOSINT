/* collectors/sources/osint_extras.c
 * OSINT services — net-new keyless, distinct-from-existing pivots on ctx->entity.
 * Real fetch or honest-empty. No API keys (GitHub uses GITHUB_TOKEN if present).
 *
 *   TOR_ONIONOO            onionoo.torproject.org/details?search=<e>   (Tor relays)
 *   GITHUB_COMMITS_BY_EMAIL api.github.com/search/commits?q=author-email:<e>
 *   ARCHIVE_ORG_SEARCH     archive.org/advancedsearch.php?q=<e>        (IA items)
 *   MUSICBRAINZ            musicbrainz.org/ws/2/artist?query=<e>       (artists)
 *   PHOTON_GEOCODE         photon.komoot.io/api?q=<e>                  (OSM geocode)
 *   CHRONICLING_AMERICA    chroniclingamerica.loc.gov/search/pages/... (old news) */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *OE_UA = "User-Agent: JapanOSINT/1.0 ( osint@example.org )";

static int oe_emit(intel_sink *sink, const char *service, const char *rtype,
                   const char *rk, const char *title, const char *summary,
                   const char *link, cJSON *body, cJSON *props,
                   int has_geo, double lat, double lon) {
  char *bj = body ? cJSON_PrintUnformatted(body) : NULL;
  if (props) { cJSON_AddStringToObject(props, "service", service); cJSON_AddBoolToObject(props, "success", 1); }
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = summary;
  it.body = bj; it.link = (link && link[0]) ? link : NULL; it.lang = "en";
  it.has_geo = has_geo; it.lat = lat; it.lon = lon;
  it.record_type = rtype; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  if (body) cJSON_Delete(body); if (props) cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run_onionoo(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://onionoo.torproject.org/details?search=%s&limit=20", enc);
  const char *hdrs[] = { "Accept: application/json", OE_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "TOR_ONIONOO");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *relays = cJSON_GetObjectItem(root, "relays");
  int emitted = 0;
  if (cJSON_IsArray(relays)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, relays) {
      const char *nick = jo_sv(r, "nickname");
      const char *fp = jo_sv(r, "fingerprint");
      const char *country = jo_sv(r, "country");
      const char *asname = jo_sv(r, "as_name");
      const cJSON *running = cJSON_GetObjectItem(r, "running");
      if (!fp && !nick) continue;
      const char *addr = NULL;
      const cJSON *ors = cJSON_GetObjectItem(r, "or_addresses");
      if (cJSON_IsArray(ors) && cJSON_GetArraySize(ors)) { const cJSON *a0 = cJSON_GetArrayItem(ors, 0); if (cJSON_IsString(a0)) addr = a0->valuestring; }
      cJSON *d = cJSON_CreateObject();
      if (nick) cJSON_AddStringToObject(d, "nickname", nick);
      if (fp) cJSON_AddStringToObject(d, "fingerprint", fp);
      if (addr) cJSON_AddStringToObject(d, "or_address", addr);
      if (country) cJSON_AddStringToObject(d, "country", country);
      if (asname) cJSON_AddStringToObject(d, "as_name", asname);
      if (cJSON_IsBool(running)) cJSON_AddBoolToObject(d, "running", cJSON_IsTrue(running));
      cJSON_AddStringToObject(d, "source", "Tor Onionoo");
      cJSON *p = cJSON_CreateObject();
      if (fp) cJSON_AddStringToObject(p, "fingerprint", fp);
      char link[256] = {0};
      if (fp) snprintf(link, sizeof link, "https://metrics.torproject.org/rs.html#details/%s", fp);
      emitted += oe_emit(sink, "TOR_ONIONOO", "tor-relay", fp ? fp : nick, nick ? nick : fp, asname, link, d, p, 0, 0, 0);
      if (emitted >= 20) break;
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_gh_commits(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  char q[256]; snprintf(q, sizeof q, "author-email:%s", raw);
  char *enc = jo_urlencode(q); if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url, "https://api.github.com/search/commits?q=%s&per_page=20", enc);
  free(enc);
  char authbuf[160] = {0}; const char *tok = jo_env("GITHUB_TOKEN");
  if (tok) snprintf(authbuf, sizeof authbuf, "Authorization: Bearer %s", tok);
  const char *hdrs[] = { "Accept: application/vnd.github.cloak-preview+json", OE_UA, tok ? authbuf : NULL, NULL };
  char *body = jo_get(ctx, url, hdrs, "GITHUB_COMMITS_BY_EMAIL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *items = cJSON_GetObjectItem(root, "items");
  int emitted = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *c;
    cJSON_ArrayForEach(c, items) {
      const char *sha = jo_sv(c, "sha");
      const char *html = jo_sv(c, "html_url");
      const cJSON *commit = cJSON_GetObjectItem(c, "commit");
      const cJSON *author = commit ? cJSON_GetObjectItem(commit, "author") : NULL;
      const char *aname = author ? jo_sv(author, "name") : NULL;
      const char *adate = author ? jo_sv(author, "date") : NULL;
      const char *msg = commit ? jo_sv(commit, "message") : NULL;
      const cJSON *repo = cJSON_GetObjectItem(c, "repository");
      const char *repofull = repo ? jo_sv(repo, "full_name") : NULL;
      if (!sha) continue;
      cJSON *d = cJSON_CreateObject();
      if (aname) cJSON_AddStringToObject(d, "author_name", aname);
      cJSON_AddStringToObject(d, "queried_email", raw);
      if (repofull) cJSON_AddStringToObject(d, "repository", repofull);
      if (msg) { char m[161]; snprintf(m, sizeof m, "%.160s", msg); cJSON_AddStringToObject(d, "message", m); }
      cJSON_AddStringToObject(d, "sha", sha);
      cJSON_AddStringToObject(d, "source", "GitHub Commits");
      cJSON *p = cJSON_CreateObject();
      if (aname) cJSON_AddStringToObject(p, "author_name", aname);
      if (repofull) cJSON_AddStringToObject(p, "repository", repofull);
      char title[256];
      snprintf(title, sizeof title, "%s @ %s", aname ? aname : "commit", repofull ? repofull : sha);
      emitted += oe_emit(sink, "GITHUB_COMMITS_BY_EMAIL", "github-commit", sha, title, aname, html, d, p, 0, 0, 0);
      (void)adate;
      if (emitted >= 20) break;
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_archive(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[640];
  snprintf(url, sizeof url,
    "https://archive.org/advancedsearch.php?q=%s&rows=20&output=json"
    "&fl[]=identifier&fl[]=title&fl[]=mediatype&fl[]=creator&fl[]=year", enc);
  const char *hdrs[] = { "Accept: application/json", OE_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "ARCHIVE_ORG_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *resp = cJSON_GetObjectItem(root, "response");
  const cJSON *docs = resp ? cJSON_GetObjectItem(resp, "docs") : NULL;
  int emitted = 0;
  if (cJSON_IsArray(docs)) {
    const cJSON *doc;
    cJSON_ArrayForEach(doc, docs) {
      const char *id = jo_sv(doc, "identifier");
      const char *title = jo_sv(doc, "title");
      const char *mt = jo_sv(doc, "mediatype");
      const char *creator = jo_sv(doc, "creator");
      if (!id) continue;
      cJSON *d = cJSON_CreateObject();
      if (title) cJSON_AddStringToObject(d, "title", title);
      if (mt) cJSON_AddStringToObject(d, "mediatype", mt);
      if (creator) cJSON_AddStringToObject(d, "creator", creator);
      cJSON_AddStringToObject(d, "identifier", id);
      cJSON_AddStringToObject(d, "source", "Internet Archive");
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "identifier", id);
      char link[300]; snprintf(link, sizeof link, "https://archive.org/details/%s", id);
      emitted += oe_emit(sink, "ARCHIVE_ORG_SEARCH", "archive-item", id, title ? title : id, mt, link, d, p, 0, 0, 0);
      if (emitted >= 20) break;
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_musicbrainz(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://musicbrainz.org/ws/2/artist?query=%s&fmt=json&limit=20", enc);
  const char *hdrs[] = { "Accept: application/json", OE_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "MUSICBRAINZ");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *artists = cJSON_GetObjectItem(root, "artists");
  int emitted = 0;
  if (cJSON_IsArray(artists)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, artists) {
      const char *id = jo_sv(a, "id");
      const char *name = jo_sv(a, "name");
      const char *country = jo_sv(a, "country");
      const char *type = jo_sv(a, "type");
      const char *disamb = jo_sv(a, "disambiguation");
      if (!id || !name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", name);
      if (type) cJSON_AddStringToObject(d, "type", type);
      if (country) cJSON_AddStringToObject(d, "country", country);
      if (disamb) cJSON_AddStringToObject(d, "disambiguation", disamb);
      cJSON_AddStringToObject(d, "mbid", id);
      cJSON_AddStringToObject(d, "source", "MusicBrainz");
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "mbid", id);
      char link[300]; snprintf(link, sizeof link, "https://musicbrainz.org/artist/%s", id);
      emitted += oe_emit(sink, "MUSICBRAINZ", "musicbrainz-artist", id, name, disamb ? disamb : country, link, d, p, 0, 0, 0);
      if (emitted >= 20) break;
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_photon(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://photon.komoot.io/api?q=%s&limit=10", enc);
  const char *hdrs[] = { "Accept: application/json", OE_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "PHOTON_GEOCODE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *feats = cJSON_GetObjectItem(root, "features");
  int emitted = 0;
  if (cJSON_IsArray(feats)) {
    const cJSON *f;
    cJSON_ArrayForEach(f, feats) {
      const cJSON *props = cJSON_GetObjectItem(f, "properties");
      const cJSON *geom = cJSON_GetObjectItem(f, "geometry");
      const cJSON *coords = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
      if (!props || !cJSON_IsArray(coords) || cJSON_GetArraySize(coords) < 2) continue;
      double lon = cJSON_GetArrayItem(coords, 0)->valuedouble;
      double lat = cJSON_GetArrayItem(coords, 1)->valuedouble;
      const char *name = jo_sv(props, "name");
      const char *city = jo_sv(props, "city");
      const char *country = jo_sv(props, "country");
      const cJSON *osmid = cJSON_GetObjectItem(props, "osm_id");
      cJSON *d = cJSON_CreateObject();
      if (name) cJSON_AddStringToObject(d, "name", name);
      if (city) cJSON_AddStringToObject(d, "city", city);
      if (country) cJSON_AddStringToObject(d, "country", country);
      cJSON_AddNumberToObject(d, "lat", lat); cJSON_AddNumberToObject(d, "lon", lon);
      cJSON_AddStringToObject(d, "source", "Photon (OSM)");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "PHOTON_GEOCODE"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char summ[160]; snprintf(summ, sizeof summ, "%s%s%s", city ? city : "", (city && country) ? ", " : "", country ? country : "");
      char rk[96]; snprintf(rk, sizeof rk, "photon:%.5f,%.5f", lat, lon);
      intel_item it = {0};
      it.remote_key = rk; it.title = name ? name : summ; it.summary = summ[0] ? summ : NULL;
      it.body = bj; it.lang = "en"; it.has_geo = 1; it.lat = lat; it.lon = lon;
      it.record_type = "geocode-result";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"PHOTON_GEOCODE\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
      (void)osmid;
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "TOR_ONIONOO"))             n = run_onionoo(ctx, sink, enc);
  else if (!strcmp(sid, "GITHUB_COMMITS_BY_EMAIL")) n = run_gh_commits(ctx, sink, q);
  else if (!strcmp(sid, "ARCHIVE_ORG_SEARCH"))      n = run_archive(ctx, sink, enc);
  else if (!strcmp(sid, "MUSICBRAINZ"))             n = run_musicbrainz(ctx, sink, enc);
  else if (!strcmp(sid, "PHOTON_GEOCODE"))          n = run_photon(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define OE_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/extras", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

OE_DEF(oe_onionoo_def, "TOR_ONIONOO", "Tor Onionoo Relays", "cyber",
  "Tor Onionoo keyless: relay/bridge search (nickname, fingerprint, AS, country).");
OE_DEF(oe_ghcommits_def, "GITHUB_COMMITS_BY_EMAIL", "GitHub Commits by Email", "cyber",
  "GitHub keyless commit search by author-email → committer name + repositories.");
OE_DEF(oe_archive_def, "ARCHIVE_ORG_SEARCH", "Internet Archive Search", "investigation",
  "Internet Archive keyless item search (title, mediatype, creator).");
OE_DEF(oe_musicbrainz_def, "MUSICBRAINZ", "MusicBrainz Artist", "investigation",
  "MusicBrainz keyless artist search (type, country, disambiguation).");
OE_DEF(oe_photon_def, "PHOTON_GEOCODE", "Photon OSM Geocode", "geospatial",
  "Photon (Komoot) keyless OSM geocoding (name, city, country, coordinates).");
