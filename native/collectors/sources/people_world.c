/* collectors/sources/people_world.c
 * OSINT services — people / identity pivots. On-demand entity pivot
 * (ctx->entity = a username, handle or email) against public, keyless identity
 * APIs. Each run() REAL-fetches the live endpoint and emits one intel_item per
 * parsed record; fetch failure / no match / missing pivot → honest empty
 * (return 0). Nothing is synthesized.
 *
 *   GRAVATAR_GLOBAL  en.gravatar.com/<md5(lower(trim(email)))>.json — profile
 *                    (only meaningful for an email pivot; non-email → skip)
 *   GITHUB_USER      api.github.com/users/<entity> (exact) + /search/users
 *   GITLAB_USER      gitlab.com/api/v4/users?username=<entity>
 *   KEYBASE          keybase.io/_/api/1.0/user/lookup.json?usernames=<entity>
 *   MASTODON_SEARCH  mastodon.social/api/v2/search?q=<entity>&type=accounts
 *
 * All keyless & LIVE. One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- compact MD5 (RFC 1321, public-domain style) — Gravatar needs it ---- */
typedef struct { unsigned int a,b,c,d; unsigned long long len; unsigned char buf[64]; size_t n; } pw_md5;
static unsigned int pw_rol(unsigned int x, int c){ return (x<<c)|(x>>(32-c)); }
static void pw_md5_block(pw_md5 *m, const unsigned char *p){
  static const unsigned int K[64]={
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
  static const int S[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
  unsigned int M[16];
  for(int i=0;i<16;i++) M[i]=(unsigned int)p[i*4]|((unsigned int)p[i*4+1]<<8)|((unsigned int)p[i*4+2]<<16)|((unsigned int)p[i*4+3]<<24);
  unsigned int A=m->a,B=m->b,C=m->c,D=m->d;
  for(int i=0;i<64;i++){
    unsigned int F; int g;
    if(i<16){ F=(B&C)|(~B&D); g=i; }
    else if(i<32){ F=(D&B)|(~D&C); g=(5*i+1)&15; }
    else if(i<48){ F=B^C^D; g=(3*i+5)&15; }
    else { F=C^(B|~D); g=(7*i)&15; }
    F=F+A+K[i]+M[g]; A=D; D=C; C=B; B=B+pw_rol(F,S[i]);
  }
  m->a+=A; m->b+=B; m->c+=C; m->d+=D;
}
static void pw_md5_init(pw_md5 *m){ m->a=0x67452301; m->b=0xefcdab89; m->c=0x98badcfe; m->d=0x10325476; m->len=0; m->n=0; }
static void pw_md5_update(pw_md5 *m, const unsigned char *d, size_t len){
  m->len += (unsigned long long)len*8;
  while(len){
    size_t k=64-m->n; if(k>len)k=len;
    memcpy(m->buf+m->n,d,k); m->n+=k; d+=k; len-=k;
    if(m->n==64){ pw_md5_block(m,m->buf); m->n=0; }
  }
}
static void pw_md5_hex(const char *s, char out[33]){
  pw_md5 m; pw_md5_init(&m);
  pw_md5_update(&m,(const unsigned char*)s,strlen(s));
  unsigned long long bl=m.len;               /* capture BEFORE padding */
  unsigned char pad=0x80; pw_md5_update(&m,&pad,1);
  unsigned char z=0; while(m.n!=56) pw_md5_update(&m,&z,1);
  unsigned char L[8];
  for(int i=0;i<8;i++) L[i]=(unsigned char)(bl>>(i*8));
  pw_md5_update(&m,L,8);
  unsigned int v[4]={m.a,m.b,m.c,m.d};
  static const char hx[]="0123456789abcdef";
  for(int i=0;i<4;i++) for(int j=0;j<4;j++){
    unsigned char b=(unsigned char)(v[i]>>(j*8));
    out[(i*4+j)*2]=hx[b>>4]; out[(i*4+j)*2+1]=hx[b&15];
  }
  out[32]=0;
}

/* ---- shared emit -------------------------------------------------------- *
 * name/handle/url/detail are all real, parsed bytes from the live API. */
static int pw_emit(intel_sink *sink, const char *service, const char *rectype,
                   const char *query, const char *key, const char *title,
                   const char *summary, const char *link, cJSON *extra) {
  if (!title || !*title) return 0;
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  if (query) cJSON_AddStringToObject(props, "query", query);
  if (link)  cJSON_AddStringToObject(props, "url", link);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char *bj = extra ? cJSON_PrintUnformatted(extra) : NULL;

  char kb[544];
  snprintf(kb, sizeof kb, "%s|%.480s", service, key ? key : title);

  intel_item it = {0};
  it.remote_key      = kb;
  it.title           = title;
  it.summary         = summary;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.record_type     = rectype;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"identity\"]";
  int rc = sink->emit(sink, &it);
  free(pj); free(bj);
  return rc >= 0 ? 1 : 0;
}

/* ---- GitHub ------------------------------------------------------------- */
static int pw_emit_github(intel_sink *sink, const char *q, const cJSON *u) {
  const char *login = jo_sv(u, "login");
  const char *url   = jo_sv(u, "html_url");
  if (!login && !url) return 0;
  const char *name  = jo_sv(u, "name");
  const char *bio   = jo_sv(u, "bio");
  const char *comp  = jo_sv(u, "company");
  const char *loc   = jo_sv(u, "location");
  const char *blog  = jo_sv(u, "blog");
  const char *avatar= jo_sv(u, "avatar_url");
  const cJSON *rc = cJSON_GetObjectItem(u, "public_repos");
  const cJSON *fl = cJSON_GetObjectItem(u, "followers");

  cJSON *d = cJSON_CreateObject();
  if (login) cJSON_AddStringToObject(d, "login", login);
  if (name)  cJSON_AddStringToObject(d, "name", name);
  if (bio)   cJSON_AddStringToObject(d, "bio", bio);
  if (comp)  cJSON_AddStringToObject(d, "company", comp);
  if (loc)   cJSON_AddStringToObject(d, "location", loc);
  if (blog && *blog) cJSON_AddStringToObject(d, "blog", blog);
  if (avatar) cJSON_AddStringToObject(d, "avatar", avatar);
  if (rc && cJSON_IsNumber(rc)) cJSON_AddNumberToObject(d, "public_repos", rc->valuedouble);
  if (fl && cJSON_IsNumber(fl)) cJSON_AddNumberToObject(d, "followers", fl->valuedouble);
  cJSON_AddStringToObject(d, "source", "GitHub");
  int e = pw_emit(sink, "GITHUB_USER", "github-user", q, login ? login : url,
                  name ? name : login, bio ? bio : loc, url, d);
  cJSON_Delete(d);
  return e;
}

/* Exported (non-static) so PERSON_SEARCH (people_finder.c) can reuse this ONE
 * richer implementation instead of a private lighter copy — the pivot then emits
 * under the SAME remote_key scheme (GITHUB_USER|<login>) and the intel sink
 * dedups by uid. Standalone GITHUB_USER behaviour is unchanged. */
int pw_run_github(const source_ctx *ctx, intel_sink *sink, const char *q) {
  const char *hdrs[] = { "Accept: application/vnd.github+json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  int emitted = 0;

  /* exact username */
  snprintf(url, sizeof url, "https://api.github.com/users/%s", enc);
  char *body = jo_get(ctx, url, hdrs, "GITHUB_USER");
  if (body) {
    cJSON *root = cJSON_Parse(body); free(body);
    if (root) {
      if (jo_sv(root, "login")) emitted += pw_emit_github(sink, q, root);
      cJSON_Delete(root);
    }
  }

  /* broader search (login/name/bio matches) */
  snprintf(url, sizeof url,
    "https://api.github.com/search/users?q=%s&per_page=10", enc);
  body = jo_get(ctx, url, hdrs, "GITHUB_USER");
  if (body) {
    cJSON *root = cJSON_Parse(body); free(body);
    if (root) {
      cJSON *items = cJSON_GetObjectItem(root, "items");
      if (cJSON_IsArray(items)) {
        cJSON *it;
        cJSON_ArrayForEach(it, items) {
          const char *login = jo_sv(it, "login");
          if (login && strcasecmp(login, q) == 0) continue; /* already emitted */
          emitted += pw_emit_github(sink, q, it);
        }
      }
      cJSON_Delete(root);
    }
  }
  free(enc);
  fprintf(stderr, "[GITHUB_USER] emitted %d\n", emitted);
  return emitted;
}

/* ---- GitLab ------------------------------------------------------------- */
static int run_gitlab(const source_ctx *ctx, intel_sink *sink, const char *q) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url, "https://gitlab.com/api/v4/users?username=%s", enc);
  free(enc);
  char *body = jo_get(ctx, url, hdrs, "GITLAB_USER");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  if (cJSON_IsArray(root)) {
    cJSON *u;
    cJSON_ArrayForEach(u, root) {
      const char *un   = jo_sv(u, "username");
      const char *url2 = jo_sv(u, "web_url");
      if (!un && !url2) continue;
      const char *name = jo_sv(u, "name");
      const char *state= jo_sv(u, "state");
      const char *avat = jo_sv(u, "avatar_url");
      const cJSON *id  = cJSON_GetObjectItem(u, "id");
      cJSON *d = cJSON_CreateObject();
      if (un)   cJSON_AddStringToObject(d, "username", un);
      if (name) cJSON_AddStringToObject(d, "name", name);
      if (state)cJSON_AddStringToObject(d, "state", state);
      if (avat) cJSON_AddStringToObject(d, "avatar", avat);
      if (id && cJSON_IsNumber(id)) cJSON_AddNumberToObject(d, "gitlab_id", id->valuedouble);
      cJSON_AddStringToObject(d, "source", "GitLab");
      emitted += pw_emit(sink, "GITLAB_USER", "gitlab-user", q,
                         un ? un : url2, name ? name : un, un, url2, d);
      cJSON_Delete(d);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[GITLAB_USER] emitted %d\n", emitted);
  return emitted;
}

/* ---- Keybase ------------------------------------------------------------ */
static int run_keybase(const source_ctx *ctx, intel_sink *sink, const char *q) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://keybase.io/_/api/1.0/user/lookup.json?usernames=%s", enc);
  free(enc);
  char *body = jo_get(ctx, url, hdrs, "KEYBASE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *them = cJSON_GetObjectItem(root, "them");
  /* /lookup returns "them" as an array (multi) or object (single). */
  cJSON *arr = cJSON_CreateArray();
  if (cJSON_IsArray(them)) { cJSON *e; cJSON_ArrayForEach(e, them) if (e && cJSON_IsObject(e)) cJSON_AddItemReferenceToArray(arr, e); }
  else if (cJSON_IsObject(them)) cJSON_AddItemReferenceToArray(arr, them);

  cJSON *u;
  cJSON_ArrayForEach(u, arr) {
    cJSON *basics = cJSON_GetObjectItem(u, "basics");
    const char *un = basics ? jo_sv(basics, "username") : NULL;
    if (!un) continue;
    cJSON *profile = cJSON_GetObjectItem(u, "profile");
    const char *full = profile ? jo_sv(profile, "full_name") : NULL;
    const char *loc  = profile ? jo_sv(profile, "location") : NULL;
    const char *bio  = profile ? jo_sv(profile, "bio") : NULL;
    char link[256]; snprintf(link, sizeof link, "https://keybase.io/%s", un);

    /* collect proven proofs (twitter/github/etc.) — real data */
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "username", un);
    if (full) cJSON_AddStringToObject(d, "full_name", full);
    if (loc)  cJSON_AddStringToObject(d, "location", loc);
    if (bio)  cJSON_AddStringToObject(d, "bio", bio);
    cJSON *proofs_summary = cJSON_GetObjectItem(u, "proofs_summary");
    cJSON *all = proofs_summary ? cJSON_GetObjectItem(proofs_summary, "all") : NULL;
    if (cJSON_IsArray(all)) {
      cJSON *plist = cJSON_AddArrayToObject(d, "proofs");
      cJSON *pr;
      cJSON_ArrayForEach(pr, all) {
        const char *typ = jo_sv(pr, "proof_type");
        const char *nym = jo_sv(pr, "nametag");
        const char *svc = jo_sv(pr, "service_url");
        if (!typ && !nym) continue;
        cJSON *po = cJSON_CreateObject();
        if (typ) cJSON_AddStringToObject(po, "type", typ);
        if (nym) cJSON_AddStringToObject(po, "handle", nym);
        if (svc) cJSON_AddStringToObject(po, "url", svc);
        cJSON_AddItemToArray(plist, po);
      }
    }
    cJSON_AddStringToObject(d, "source", "Keybase");
    emitted += pw_emit(sink, "KEYBASE", "keybase-user", q, un,
                       full ? full : un, bio ? bio : loc, link, d);
    cJSON_Delete(d);
  }
  cJSON_Delete(arr);
  cJSON_Delete(root);
  fprintf(stderr, "[KEYBASE] emitted %d\n", emitted);
  return emitted;
}

/* ---- Mastodon (mastodon.social account search) -------------------------- */
static int run_mastodon(const source_ctx *ctx, intel_sink *sink, const char *q) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://mastodon.social/api/v2/search?q=%s&type=accounts&limit=20", enc);
  free(enc);
  char *body = jo_get(ctx, url, hdrs, "MASTODON_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *accs = cJSON_GetObjectItem(root, "accounts");
  if (cJSON_IsArray(accs)) {
    cJSON *a;
    cJSON_ArrayForEach(a, accs) {
      const char *acct = jo_sv(a, "acct");
      const char *url2 = jo_sv(a, "url");
      if (!acct && !url2) continue;
      const char *disp = jo_sv(a, "display_name");
      const char *note = jo_sv(a, "note");
      const char *avat = jo_sv(a, "avatar");
      const char *crt  = jo_sv(a, "created_at");
      const cJSON *fol = cJSON_GetObjectItem(a, "followers_count");
      const cJSON *st  = cJSON_GetObjectItem(a, "statuses_count");
      cJSON *d = cJSON_CreateObject();
      if (acct) cJSON_AddStringToObject(d, "acct", acct);
      if (disp && *disp) cJSON_AddStringToObject(d, "display_name", disp);
      if (note) cJSON_AddStringToObject(d, "note", note);
      if (avat) cJSON_AddStringToObject(d, "avatar", avat);
      if (fol && cJSON_IsNumber(fol)) cJSON_AddNumberToObject(d, "followers", fol->valuedouble);
      if (st && cJSON_IsNumber(st)) cJSON_AddNumberToObject(d, "statuses", st->valuedouble);
      cJSON_AddStringToObject(d, "source", "Mastodon");
      char title[256];
      if (disp && *disp) snprintf(title, sizeof title, "%s (@%s)", disp, acct ? acct : "");
      else snprintf(title, sizeof title, "@%s", acct ? acct : "");
      /* published_at = account creation date when present */
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "MASTODON_SEARCH");
      cJSON_AddStringToObject(props, "query", q);
      if (url2) cJSON_AddStringToObject(props, "url", url2);
      if (acct) cJSON_AddStringToObject(props, "acct", acct);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      char kb[544]; snprintf(kb, sizeof kb, "MASTODON_SEARCH|%.480s", url2 ? url2 : acct);
      intel_item it = {0};
      it.remote_key = kb; it.title = title; it.summary = note;
      it.body = bj; it.lang = "en"; it.link = url2; it.published_at = crt;
      it.record_type = "mastodon-account"; it.properties_json = pj;
      it.tags_json = "[\"osint-search\",\"identity\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj); free(bj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[MASTODON_SEARCH] emitted %d\n", emitted);
  return emitted;
}

/* ---- Gravatar (email pivot only) ---------------------------------------- */
/* Exported (non-static) — shared with PERSON_SEARCH so gravatar intel converges
 * on the GRAVATAR_GLOBAL|<hash> remote_key and dedups. Standalone
 * GRAVATAR_GLOBAL behaviour is unchanged. */
int pw_run_gravatar(const source_ctx *ctx, intel_sink *sink, const char *q) {
  /* Gravatar keys on md5(lowercased, trimmed email). Only an email pivot is
   * meaningful; a bare username → skip (honest empty). */
  if (!strchr(q, '@')) {
    fprintf(stderr, "[GRAVATAR_GLOBAL] entity is not an email, skipping\n");
    return 0;
  }
  /* normalize: trim + lowercase */
  char em[320]; size_t j = 0;
  for (const char *p = q; *p && j < sizeof em - 1; p++) {
    if (*p == ' ' || *p == '\t') continue;
    em[j++] = (char)tolower((unsigned char)*p);
  }
  em[j] = 0;
  char hash[33]; pw_md5_hex(em, hash);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char url[256];
  snprintf(url, sizeof url, "https://en.gravatar.com/%s.json", hash);
  char *body = jo_get(ctx, url, hdrs, "GRAVATAR_GLOBAL");
  if (!body) return 0;   /* 404 (no profile) → honest empty */
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *entry = cJSON_GetObjectItem(root, "entry");
  cJSON *e0 = NULL;
  if (cJSON_IsArray(entry)) e0 = cJSON_GetArrayItem(entry, 0);
  else if (cJSON_IsObject(entry)) e0 = entry;
  if (e0) {
    const char *pname = jo_sv(e0, "preferredUsername");
    const char *disp  = jo_sv(e0, "displayName");
    const char *thumb = jo_sv(e0, "thumbnailUrl");
    const char *prof  = jo_sv(e0, "profileUrl");
    cJSON *nameo = cJSON_GetObjectItem(e0, "name");
    const char *formatted = (nameo && cJSON_IsObject(nameo)) ? jo_sv(nameo, "formatted") : NULL;
    cJSON *aboutarr = cJSON_GetObjectItem(e0, "aboutMe");
    const char *about = (aboutarr && cJSON_IsString(aboutarr)) ? aboutarr->valuestring : NULL;

    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "email_hash", hash);
    if (pname)     cJSON_AddStringToObject(d, "username", pname);
    if (disp)      cJSON_AddStringToObject(d, "display_name", disp);
    if (formatted) cJSON_AddStringToObject(d, "name", formatted);
    if (thumb)     cJSON_AddStringToObject(d, "avatar", thumb);
    if (about)     cJSON_AddStringToObject(d, "about", about);
    /* linked accounts — real, parsed */
    cJSON *accts = cJSON_GetObjectItem(e0, "accounts");
    if (cJSON_IsArray(accts)) {
      cJSON *al = cJSON_AddArrayToObject(d, "accounts");
      cJSON *ac;
      cJSON_ArrayForEach(ac, accts) {
        const char *sh = jo_sv(ac, "shortname");
        const char *au = jo_sv(ac, "url");
        const char *un = jo_sv(ac, "username");
        if (!sh && !au) continue;
        cJSON *ao = cJSON_CreateObject();
        if (sh) cJSON_AddStringToObject(ao, "service", sh);
        if (un) cJSON_AddStringToObject(ao, "username", un);
        if (au) cJSON_AddStringToObject(ao, "url", au);
        cJSON_AddItemToArray(al, ao);
      }
    }
    cJSON_AddStringToObject(d, "source", "Gravatar");
    emitted += pw_emit(sink, "GRAVATAR_GLOBAL", "gravatar-profile", q,
                       hash, disp ? disp : (pname ? pname : formatted),
                       about, prof, d);
    cJSON_Delete(d);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[GRAVATAR_GLOBAL] emitted %d\n", emitted);
  return emitted;
}

/* ---- dispatch ----------------------------------------------------------- */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;
  const char *id = ctx->source_id;
  if      (strcmp(id, "GRAVATAR_GLOBAL") == 0) pw_run_gravatar(ctx, sink, q);
  else if (strcmp(id, "GITHUB_USER")     == 0) pw_run_github(ctx, sink, q);
  else if (strcmp(id, "GITLAB_USER")     == 0) run_gitlab(ctx, sink, q);
  else if (strcmp(id, "KEYBASE")         == 0) run_keybase(ctx, sink, q);
  else if (strcmp(id, "MASTODON_SEARCH") == 0) run_mastodon(ctx, sink, q);
  else return -1;
  return 0;   /* honest empty is not an error */
}

#define PEOPLE_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "investigation", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

PEOPLE_DEF(pw_gravatar_def, "GRAVATAR_GLOBAL", "Gravatar Profile", "Gravatar プロフィール",
  "https://gravatar.com",
  "Gravatar public profile by email hash (keyless): name, avatar, linked accounts");
PEOPLE_DEF(pw_github_def, "GITHUB_USER", "GitHub User Lookup", "GitHub ユーザー検索",
  "https://api.github.com",
  "GitHub user profile + user search (keyless): login, name, bio, company, repos");
PEOPLE_DEF(pw_gitlab_def, "GITLAB_USER", "GitLab User Lookup", "GitLab ユーザー検索",
  "https://gitlab.com/api/v4",
  "GitLab public user lookup by username (keyless): name, state, avatar, id");
PEOPLE_DEF(pw_keybase_def, "KEYBASE", "Keybase Identity Lookup", "Keybase ID 照会",
  "https://keybase.io",
  "Keybase user lookup (keyless): full name, bio, cross-platform proven proofs");
PEOPLE_DEF(pw_mastodon_def, "MASTODON_SEARCH", "Mastodon Account Search", "Mastodon アカウント検索",
  "https://mastodon.social",
  "Mastodon account search via mastodon.social (keyless): handle, bio, followers");
