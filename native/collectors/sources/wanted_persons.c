/* collectors/crime/sources/wanted_persons.c
 * Port of server/src/collectors/wantedPersons.js (fetchText multi-page).
 * 1) GET the NPA shimeitehai index as a reachability check; if it lacks
 *    'shimeitehai' / '指名手配' bail (emit nothing). 2) For each of 10
 *    prefectural-police pages: regex-count name/age/crime occurrences,
 *    caseCount = max of the three; emit one jittered Feature per case,
 *    spread around that HQ centroid. Every case resolves to its issuing
 *    prefecture (the PREF_POLICE row) so there is no unresolved-intel
 *    split here. SEED branch returns [] -> dropped (port guide rule 8).
 *    Feature uid/title derived by the geojson sink. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define NPA_WANTED_HTML "https://www.npa.go.jp/sousa/shimeitehai/index.html"

typedef struct {
  const char *name, *host, *path; double lat, lon; const char *pref;
} police_t;
/* PREF_POLICE — verbatim order/values from wantedPersons.js. */
static const police_t PREF_POLICE[] = {
 {"\xe8\xad\xa6\xe8\xa6\x96\xe5\xba\x81",
  "www.keishicho.metro.tokyo.lg.jp","/jiken/joho/shimei/index.html",
  35.6783,139.7528,"\xe6\x9d\xb1\xe4\xba\xac\xe9\x83\xbd"},
 {"\xe5\xa4\xa7\xe9\x98\xaa\xe5\xba\x9c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.police.pref.osaka.lg.jp","/seian/shimei_tehai/index.html",
  34.6864,135.5197,"\xe5\xa4\xa7\xe9\x98\xaa\xe5\xba\x9c"},
 {"\xe7\xa5\x9e\xe5\xa5\x88\xe5\xb7\x9d\xe7\x9c\x8c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.police.pref.kanagawa.jp","/mes/mesf2008.htm",
  35.4437,139.6380,"\xe7\xa5\x9e\xe5\xa5\x88\xe5\xb7\x9d\xe7\x9c\x8c"},
 {"\xe6\x84\x9b\xe7\x9f\xa5\xe7\x9c\x8c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.pref.aichi.jp","/police/anzen/joho/shimei/index.html",
  35.1814,136.9069,"\xe6\x84\x9b\xe7\x9f\xa5\xe7\x9c\x8c"},
 {"\xe5\x9f\xbc\xe7\x8e\x89\xe7\x9c\x8c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.police.pref.saitama.lg.jp","/anzen/jiken/shimei.html",
  35.8617,139.6455,"\xe5\x9f\xbc\xe7\x8e\x89\xe7\x9c\x8c"},
 {"\xe5\x8d\x83\xe8\x91\x89\xe7\x9c\x8c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.police.pref.chiba.jp","/sousakuji/shimeitehai.html",
  35.6083,140.1233,"\xe5\x8d\x83\xe8\x91\x89\xe7\x9c\x8c"},
 {"\xe5\x85\xb5\xe5\xba\xab\xe7\x9c\x8c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.police.pref.hyogo.lg.jp","/sou/shimei/index.htm",
  34.6913,135.1830,"\xe5\x85\xb5\xe5\xba\xab\xe7\x9c\x8c"},
 {"\xe5\x8c\x97\xe6\xb5\xb7\xe9\x81\x93\xe8\xad\xa6\xe5\xaf\x9f",
  "www.police.pref.hokkaido.lg.jp","/info/jiken/shimei/index.html",
  43.0628,141.3478,"\xe5\x8c\x97\xe6\xb5\xb7\xe9\x81\x93"},
 {"\xe7\xa6\x8f\xe5\xb2\xa1\xe7\x9c\x8c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.police.pref.fukuoka.jp","/seikatsu_anzen/shimei/index.html",
  33.5904,130.4017,"\xe7\xa6\x8f\xe5\xb2\xa1\xe7\x9c\x8c"},
 {"\xe4\xba\xac\xe9\x83\xbd\xe5\xba\x9c\xe8\xad\xa6\xe5\xaf\x9f",
  "www.pref.kyoto.jp","/fukei/anzen/shimei.html",
  35.0116,135.7681,"\xe4\xba\xac\xe9\x83\xbd\xe5\xba\x9c"},
};
#define NPP ((int)(sizeof PREF_POLICE / sizeof PREF_POLICE[0]))

/* a UTF-8 byte is part of a kanji in [一-龥] (U+4E00..U+9FA5)? Returns the
 * codepoint length (3) and the decoded value via *cp, or 0 if not kanji. */
static int kanji_at(const unsigned char *p, unsigned *cp) {
  if ((p[0] & 0xF0) != 0xE0 || (p[1] & 0xC0) != 0x80 ||
      (p[2] & 0xC0) != 0x80) return 0;
  unsigned v = ((p[0]&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F);
  if (v >= 0x4E00 && v <= 0x9FA5) { *cp = v; return 3; }
  return 0;
}

/* one UTF-8 codepoint length (1/2/3/4), min 1. */
static int u8len(unsigned char c) {
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

/* Is the codepoint starting at p one of the [^\s<>「」] class members, i.e.
 * NOT ascii-space/tab/nl/cr/ff/vt, NOT '<' '>' , NOT 「(E3 80 8C) 」(E3 80
 * 8D)? *adv set to its byte length. */
static int is_namechar(const unsigned char *p, int *adv) {
  unsigned char c = p[0];
  if (c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v'||
      c=='<'||c=='>') { *adv = 1; return 0; }
  if (c==0xE3 && p[1]==0x80 && (p[2]==0x8C || p[2]==0x8D)) {
    *adv = 3; return 0;                              /* 「 or 」 */
  }
  *adv = u8len(c);
  return 1;
}

/* JS /(?:LBL)[^<>]{0,N}(CLASS{lo,hi})/g — find the next match starting at
 * or after *cur. LBL = one of the two alternatives (lit1/lit2). On a match
 * advance *cur past it (global lastIndex) and copy the full match (label +
 * gap + captured run) into m[]. Returns 1 if matched, else 0. */
static int rx_find(const char **cur, const char *lit1, const char *lit2,
                   int gap_max, int run_lo, int run_hi,
                   char *m, size_t mn, char *cap, size_t cn) {
  const char *s = *cur;
  size_t l1 = strlen(lit1), l2 = lit2 ? strlen(lit2) : 0;
  while (*s) {
    const char *lbl = NULL; size_t ll = 0;
    if (strncmp(s, lit1, l1) == 0) { lbl = s; ll = l1; }
    else if (lit2 && strncmp(s, lit2, l2) == 0) { lbl = s; ll = l2; }
    if (!lbl) { s += u8len((unsigned char)*s); continue; }
    /* [^<>]{0,gap_max} — codepoints, none being '<' or '>' (the JS char
     * class is byte-oriented but '<'/'>' are ascii so this is equivalent;
     * quantifier counts regex "characters", here we count codepoints). */
    const char *g = lbl + ll;
    int used = 0;
    while (used < gap_max) {
      unsigned char gc = (unsigned char)*g;
      if (!gc || gc=='<' || gc=='>') break;
      g += u8len(gc); used++;
    }
    /* greedy CLASS{run_lo,run_hi}; backtrack down to run_lo. The gap is
     * also backtrackable: emulate by trying the longest gap first then
     * shrinking (regex semantics) until the run can satisfy run_lo. */
    for (int gtry = used; gtry >= 0; gtry--) {
      const char *r = lbl + ll;
      for (int k = 0; k < gtry; k++) r += u8len((unsigned char)*r);
      const char *rs = r;
      int rc = 0;
      while (rc < run_hi) {
        int adv;
        if (!is_namechar((const unsigned char *)r, &adv)) break;
        r += adv; rc++;
      }
      if (rc >= run_lo) {
        size_t mlen = (size_t)(r - lbl);
        if (mlen >= mn) mlen = mn - 1;
        memcpy(m, lbl, mlen); m[mlen] = 0;
        size_t clen = (size_t)(r - rs);
        if (clen >= cn) clen = cn - 1;
        memcpy(cap, rs, clen); cap[clen] = 0;
        *cur = r;                       /* global lastIndex */
        return 1;
      }
    }
    s = lbl + ll;                       /* no match here, keep scanning */
  }
  return 0;
}

/* JS /(?:年齢|当時)[^<>]{0,10}(\d{1,2})\s*(?:歳|才)/g — ascii digits then
 * optional ws then 歳(E6 AD B3) or 才(E6 89 8D). */
static int rx_find_age(const char **cur, char *m, size_t mn) {
  static const char *L1 = "\xe5\xb9\xb4\xe9\xbd\xa2"; /* 年齢 */
  static const char *L2 = "\xe5\xbd\x93\xe6\x99\x82"; /* 当時 */
  const char *s = *cur;
  size_t l1 = strlen(L1), l2 = strlen(L2);
  while (*s) {
    const char *lbl = NULL; size_t ll = 0;
    if (strncmp(s, L1, l1) == 0) { lbl = s; ll = l1; }
    else if (strncmp(s, L2, l2) == 0) { lbl = s; ll = l2; }
    if (!lbl) { s += u8len((unsigned char)*s); continue; }
    const char *g = lbl + ll;
    int used = 0;
    while (used < 10) {
      unsigned char gc = (unsigned char)*g;
      if (!gc || gc=='<' || gc=='>') break;
      g += u8len(gc); used++;
    }
    for (int gtry = used; gtry >= 0; gtry--) {
      const char *r = lbl + ll;
      for (int k = 0; k < gtry; k++) r += u8len((unsigned char)*r);
      const char *ds = r;
      int dn = 0;
      while (dn < 2 && isdigit((unsigned char)*r)) { r++; dn++; }
      if (dn < 1) continue;
      for (int dtry = dn; dtry >= 1; dtry--) {
        const char *rr = ds + dtry;
        while (*rr==' '||*rr=='\t'||*rr=='\n'||*rr=='\r'||
               *rr=='\f'||*rr=='\v') rr++;
        int ok = (strncmp(rr, "\xe6\xad\xb3", 3) == 0) ||  /* 歳 */
                 (strncmp(rr, "\xe6\x89\x8d", 3) == 0);     /* 才 */
        if (ok) {
          const char *end = rr + 3;
          size_t mlen = (size_t)(end - lbl);
          if (mlen >= mn) mlen = mn - 1;
          memcpy(m, lbl, mlen); m[mlen] = 0;
          *cur = end;
          return 1;
        }
      }
    }
    s = lbl + ll;
  }
  return 0;
}

/* name: JS does match.replace of label-then-non-kanji-run with empty string,
 * where label is the alternation 氏名 or 被疑者 and the trailing run is the
 * greedy non-kanji class. We strip the FIRST occurrence of (氏名|被疑者)
 * plus the following non-kanji run. */
static void strip_name(const char *src, char *out, size_t n) {
  static const char *A = "\xe6\xb0\x8f\xe5\x90\x8d";          /* 氏名 */
  static const char *B = "\xe8\xa2\xab\xe7\x96\x91\xe8\x80\x85"; /* 被疑者 */
  size_t la = strlen(A), lb = strlen(B);
  const char *s = src;
  const char *hit = NULL; size_t hl = 0;
  for (; *s; s += u8len((unsigned char)*s)) {
    if (strncmp(s, A, la) == 0) { hit = s; hl = la; break; }
    if (strncmp(s, B, lb) == 0) { hit = s; hl = lb; break; }
  }
  if (!hit) { snprintf(out, n, "%s", src); return; }
  const char *p = hit + hl;
  unsigned cp;
  while (*p && !kanji_at((const unsigned char *)p, &cp))
    p += u8len((unsigned char)*p);
  /* result = prefix-before-hit + (rest from p) */
  size_t pre = (size_t)(hit - src);
  size_t w = 0;
  for (size_t i = 0; i < pre && w < n - 1; i++) out[w++] = src[i];
  for (const char *q = p; *q && w < n - 1; q++) out[w++] = *q;
  out[w] = 0;
}

/* crime: same shape — strip the FIRST (罪名|容疑) plus the following
 * non-kanji run, replacing with empty. */
static void strip_crime(const char *src, char *out, size_t n) {
  static const char *A = "\xe7\xbd\xaa\xe5\x90\x8d";   /* 罪名 */
  static const char *B = "\xe5\xae\xb9\xe7\x96\x91";   /* 容疑 */
  size_t la = strlen(A), lb = strlen(B);
  const char *s = src;
  const char *hit = NULL; size_t hl = 0;
  for (; *s; s += u8len((unsigned char)*s)) {
    if (strncmp(s, A, la) == 0) { hit = s; hl = la; break; }
    if (strncmp(s, B, lb) == 0) { hit = s; hl = lb; break; }
  }
  if (!hit) { snprintf(out, n, "%s", src); return; }
  const char *p = hit + hl;
  unsigned cp;
  while (*p && !kanji_at((const unsigned char *)p, &cp))
    p += u8len((unsigned char)*p);
  size_t pre = (size_t)(hit - src);
  size_t w = 0;
  for (size_t i = 0; i < pre && w < n - 1; i++) out[w++] = src[i];
  for (const char *q = p; *q && w < n - 1; q++) out[w++] = *q;
  out[w] = 0;
}

/* first run of ascii digits in s -> out (JS match(/\d+/)?.[0]). */
static int first_digits(const char *s, char *out, size_t n) {
  while (*s && !isdigit((unsigned char)*s)) s++;
  if (!*s) return 0;
  size_t k = 0;
  while (*s && isdigit((unsigned char)*s) && k < n - 1) out[k++] = *s++;
  out[k] = 0;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* Reachability check: NPA index must contain 指名手配 or shimeitehai. */
  char *idx_html = feed_get_text(ctx->http, NPA_WANTED_HTML, 8000);
  if (!idx_html) {
    /* both of these bailed silently, so an operator could not tell an
     * unreachable NPA index from "no wanted persons published" */
    fprintf(stderr, "[wanted-persons] NPA index unreachable (%s)\n",
            NPA_WANTED_HTML);
    return 0;
  }
  int ok = strstr(idx_html, "shimeitehai") != NULL ||
           strstr(idx_html,
             "\xe6\x8c\x87\xe5\x90\x8d\xe6\x89\x8b\xe9\x85\x8d") != NULL;
  free(idx_html);
  if (!ok) {
    fprintf(stderr, "[wanted-persons] NPA index fetched but carries no "
                    "shimeitehai marker — page layout changed\n");
    return 0;
  }

  cJSON *features = cJSON_CreateArray();
  int idx = 0;

  for (int pi = 0; pi < NPP; pi++) {
    const police_t *pp = &PREF_POLICE[pi];
    char url[256];
    snprintf(url, sizeof url, "https://%s%s", pp->host, pp->path);
    char *html = feed_get_text(ctx->http, url, 10000);
    if (!html) continue;

    /* collect all nameMatches / ageMatches / crimeMatches (global). */
    char (*names)[256] = NULL; int nn = 0, nc = 0;
    char (*ages)[64]  = NULL; int an = 0, ac = 0;
    char (*crimes)[256] = NULL; int cn = 0, cc = 0;

    const char *cur = html;
    char mbuf[512], cbuf[256];
    /* /(?:氏名|被疑者)[^<>]{0,40}([^\s<>「」]{2,8})/g */
    while (rx_find(&cur, "\xe6\xb0\x8f\xe5\x90\x8d",
                   "\xe8\xa2\xab\xe7\x96\x91\xe8\x80\x85", 40, 2, 8,
                   mbuf, sizeof mbuf, cbuf, sizeof cbuf)) {
      if (nn == nc) { nc = nc ? nc*2 : 16;
        names = realloc(names, (size_t)nc * sizeof *names); }
      snprintf(names[nn++], 256, "%s", mbuf);
    }
    cur = html;
    /* /(?:年齢|当時)[^<>]{0,10}(\d{1,2})\s*(?:歳|才)/g */
    while (rx_find_age(&cur, mbuf, sizeof mbuf)) {
      if (an == ac) { ac = ac ? ac*2 : 16;
        ages = realloc(ages, (size_t)ac * sizeof *ages); }
      snprintf(ages[an++], 64, "%s", mbuf);
    }
    cur = html;
    /* /(?:罪名|容疑)[^<>]{0,30}([^\s<>「」]{2,16})/g */
    while (rx_find(&cur, "\xe7\xbd\xaa\xe5\x90\x8d",
                   "\xe5\xae\xb9\xe7\x96\x91", 30, 2, 16,
                   mbuf, sizeof mbuf, cbuf, sizeof cbuf)) {
      if (cn == cc) { cc = cc ? cc*2 : 16;
        crimes = realloc(crimes, (size_t)cc * sizeof *crimes); }
      snprintf(crimes[cn++], 256, "%s", mbuf);
    }

    /* caseCount used to be max(names, ages, crimes) — three INDEPENDENT regex
     * passes over the page with no positional relationship to each other. That
     * invented cases (a page with 4 names and 9 age strings produced 9 wanted
     * persons, 5 of them nameless) and it cross-wired attributes (case i took
     * ages[i] regardless of whether that age belonged to names[i]). A case is a
     * named individual: count names, and attach age/crime only when the passes
     * are aligned 1:1, which is the only condition under which the pairing is
     * defensible. */
    int caseCount = nn;
    int age_aligned = (an == nn), crime_aligned = (cn == nn);

    for (int i = 0; i < caseCount; i++) {
      idx++;

      /* No geometry is invented. The previous code placed each case on an
       * index-derived ring 0.005–0.011 deg (≈0.5–1.2 km) around the issuing
       * force's HQ, which the map presented as the person's location. The HQ
       * point is real, so it is kept — but flagged as what it is, and never as
       * an exact position. */
      cJSON *f = gj_point_feature(pp->lon, pp->lat);

      cJSON *pr = cJSON_CreateObject();      /* EXACT JS key order */
      char cid[64];
      snprintf(cid, sizeof cid, "NPA_%s_%d", pp->pref, idx);
      cJSON_AddStringToObject(pr, "case_id", cid);
      cJSON_AddStringToObject(pr, "issuing_force", pp->name);

      /* name: nameMatches[i] ? strip : null */
      if (i < nn) {
        char nm[256]; strip_name(names[i], nm, sizeof nm);
        if (nm[0]) cJSON_AddStringToObject(pr, "name", nm);
        else cJSON_AddNullToObject(pr, "name");
      } else cJSON_AddNullToObject(pr, "name");

      /* age: ageMatches[i]?.match(/\d+/)?.[0] || null — only when the age pass
       * produced exactly one match per name, otherwise the index is meaningless */
      if (age_aligned && i < an) {
        char ag[32];
        if (first_digits(ages[i], ag, sizeof ag))
          cJSON_AddStringToObject(pr, "age", ag);
        else cJSON_AddNullToObject(pr, "age");
      } else cJSON_AddNullToObject(pr, "age");

      /* crime: crimeMatches[i] ? strip : null — same alignment condition */
      if (crime_aligned && i < cn) {
        char cm[256]; strip_crime(crimes[i], cm, sizeof cm);
        if (cm[0]) cJSON_AddStringToObject(pr, "crime", cm);
        else cJSON_AddNullToObject(pr, "crime");
      } else cJSON_AddNullToObject(pr, "crime");

      cJSON_AddStringToObject(pr, "prefecture", pp->pref);
      cJSON_AddStringToObject(pr, "source_url", url);
      cJSON_AddStringToObject(pr, "country", "JP");
      cJSON_AddStringToObject(pr, "source", "npa_pref_police_html");
      /* Unified provenance vocabulary: this point is the issuing force's
       * headquarters, NOT the wanted person's whereabouts. */
      cJSON_AddStringToObject(pr, "geo_provenance", "issuing-force-hq");
      cJSON_AddStringToObject(pr, "geo_precision", "publisher-hq");
      cJSON_AddBoolToObject(pr, "geo_uncertain", 1);
      cJSON_AddStringToObject(pr, "geo_note",
        "point is the issuing prefectural police headquarters; the publication "
        "carries no location for the individual");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
    }

    free(names); free(ages); free(crimes);
    free(html);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wanted-persons] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wanted_persons_def = {
  .id = "wanted-persons", .collector = "crime",
  .name = "NPA Wanted Persons",
  .name_ja = "\xe8\xad\xa6\xe5\xaf\x9f\xe5\xba\x81 \xe6\x8c\x87\xe5\x90\x8d\xe6\x89\x8b\xe9\x85\x8d",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(wanted_persons_def)
