/* tests/unit/test_small_modules.c — the small pure modules no other test
 * reaches (OPS-07): lib/bigfile, lib/linecolor, lib/camfeature, lib/probe
 * (probe_iso_now + the NULL guards of probe_head), core/collectortab,
 * core/linegeom.  None of them needs a database; they are linked from the
 * common object, so this file includes no .c. */
#include "../../lib/bigfile.h"
#include "../../lib/linecolor.h"
#include "../../lib/camfeature.h"
#include "../../lib/probe.h"
#include "../../core/collectortab.h"
#include "../../core/linegeom.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

/* ---- bigfile ---------------------------------------------------------- */
static void t_bigfile(void) {
  printf("-- bigfile\n");
  ok(bigfile_open("/nonexistent/dir/x.txt") == NULL, "missing file -> NULL");
  ok(bigfile_skipped(NULL) == 0 && bigfile_lines(NULL) == 0, "counters are NULL-safe");
  bigfile_close(NULL);

  char path[] = "/tmp/jo-bigfile-XXXXXX";
  int fd = mkstemp(path);
  assert(fd >= 0);
  FILE *f = fdopen(fd, "wb");
  fputs("alpha\r\n\nbeta\n", f);
  for (int i = 0; i < 70000; i++) fputc('x', f);   /* > BF_MAXLINE: skipped */
  fputs("\ngamma", f);                              /* no trailing newline */
  fclose(f);

  bigfile *bf = bigfile_open(path);
  ok(bf != NULL, "opens");
  size_t n = 99;
  const char *l = bigfile_next(bf, &n);
  ok(l && n == 5 && !strcmp(l, "alpha"), "CRLF stripped");
  l = bigfile_next(bf, &n);
  ok(l && n == 0 && !*l, "blank line is a line of length 0, not EOF");
  l = bigfile_next(bf, NULL);
  ok(l && !strcmp(l, "beta"), "len pointer is optional");
  l = bigfile_next(bf, &n);
  ok(l && n == 5 && !strcmp(l, "gamma"), "oversize line skipped, next line intact");
  ok(bigfile_skipped(bf) == 1, "the skip is counted, not silent");
  l = bigfile_next(bf, &n);
  ok(l == NULL, "EOF -> NULL");
  ok(bigfile_lines(bf) == 4, "lines counts delivered lines only");
  bigfile_close(bf);
  unlink(path);
}

/* ---- linecolor -------------------------------------------------------- */
static cJSON *tags1(const char *k, const char *v) {
  cJSON *t = cJSON_CreateObject();
  cJSON_AddStringToObject(t, k, v);
  return t;
}
static int lc(const char *k, const char *v, char *out) {
  cJSON *t = tags1(k, v);
  int r = line_color(t, out);
  cJSON_Delete(t);
  return r;
}
static void t_linecolor(void) {
  printf("-- linecolor\n");
  char o[8] = {0};
  ok(line_color(NULL, o) == 0, "NULL tags -> 0");
  cJSON *e = cJSON_CreateObject();
  ok(line_color(e, o) == 0, "no colour/operator/line -> 0 (no invented colour)");
  cJSON_Delete(e);
  ok(lc("colour", "#ABC", o) && !strcmp(o, "#aabbcc"), "#rgb expands, lower-cased");
  ok(lc("colour", "#1E88E5", o) && !strcmp(o, "#1e88e5"), "#rrggbb lower-cased");
  ok(lc("colour", "  Red  ", o) && !strcmp(o, "#e53935"), "named colour, padded, mixed case");
  ok(lc("colour", "#zzz", o) == 0, "non-hex #zzz is not a colour");
  ok(lc("colour", "#abcd", o) == 0, "4 hex digits rejected");
  ok(lc("colour", "#1234567", o) == 0, "7 hex digits rejected (not truncated to 6)");
  ok(lc("colour", "#12345678", o) == 0, "8 hex digits (rgba) rejected, not read as #123456");
  ok(lc("colour", "nosuchcolour", o) == 0, "unknown name -> 0");
  ok(lc("colour", "", o) == 0, "empty colour -> 0");

  /* fallback: deterministic palette pick from operator/line */
  cJSON *t = cJSON_CreateObject();
  cJSON_AddStringToObject(t, "operator", "JR East");
  cJSON_AddStringToObject(t, "ref", "Yamanote");
  char a[8], b[8];
  ok(line_color(t, a) == 1 && a[0] == '#' && strlen(a) == 7, "hash fallback yields #rrggbb");
  line_color(t, b);
  ok(!strcmp(a, b), "hash fallback deterministic");
  cJSON_Delete(t);
  t = cJSON_CreateObject();
  cJSON_AddStringToObject(t, "operator", "jr east  ");
  cJSON_AddStringToObject(t, "line", "YAMANOTE");
  line_color(t, b);
  ok(!strcmp(a, b), "operator/line key is case-folded and trimmed; ref == line");
  cJSON_Delete(t);
  t = cJSON_CreateObject();
  cJSON_AddStringToObject(t, "operator", "   ");
  ok(line_color(t, a) == 0, "whitespace-only operator counts as absent");
  cJSON_Delete(t);
  t = cJSON_CreateObject();
  cJSON_AddNumberToObject(t, "operator", 5);
  ok(line_color(t, a) == 0, "non-string tag ignored");
  cJSON_Delete(t);
}

/* ---- camfeature ------------------------------------------------------- */
static const char *prop_s(cJSON *f, const char *k) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = cJSON_GetObjectItem(p, k);
  return cJSON_IsString(v) ? v->valuestring : NULL;
}
static void t_camfeature(void) {
  printf("-- camfeature\n");
  cJSON *f = cam_make_feature(35.123456, 139.987654, NULL, NULL, NULL, NULL, 0);
  ok(!strcmp(prop_s(f, "name"), "Unknown camera"), "NULL name defaulted");
  ok(!strcmp(prop_s(f, "camera_type"), "unknown"), "NULL type defaulted");
  ok(!strcmp(prop_s(f, "discovery_channel"), "unknown"), "NULL channel defaulted");
  ok(!strcmp(prop_s(f, "country"), "JP"), "country is JP");
  ok(strncmp(prop_s(f, "camera_uid"), "35.1235:139.9877:", 17) == 0, "uid starts with 4dp lat:lon");
  cJSON *c = cJSON_GetObjectItem(cJSON_GetObjectItem(f, "geometry"), "coordinates");
  ok(cJSON_GetArrayItem(c, 0)->valuedouble > 139.98 && cJSON_GetArrayItem(c, 1)->valuedouble < 35.2,
     "GeoJSON order is lon,lat");
  cJSON_Delete(f);

  /* 60-byte-prefix collision regression: same long prefix, different tail */
  char u1[200], u2[200];
  memset(u1, 'a', 150); memset(u2, 'a', 150);
  u1[150] = '1'; u2[150] = '2'; u1[151] = u2[151] = 0;
  cam_kv e1 = { .k = "url", .sv = u1 }, e2 = { .k = "url", .sv = u2 };
  cJSON *f1 = cam_make_feature(1, 2, "n", "t", "c", &e1, 1);
  cJSON *f2 = cam_make_feature(1, 2, "n", "t", "c", &e2, 1);
  ok(strcmp(prop_s(f1, "camera_uid"), prop_s(f2, "camera_uid")) != 0,
     "cameras sharing a long URL prefix get distinct uids");
  cJSON_Delete(f1); cJSON_Delete(f2);

  /* the uid is stable and lower-cases the readable prefix */
  cJSON *g1 = cam_make_feature(1, 2, "ShibuyaCAM", "t", "c", NULL, 0);
  cJSON *g2 = cam_make_feature(1, 2, "ShibuyaCAM", "t", "c", NULL, 0);
  ok(!strcmp(prop_s(g1, "camera_uid"), prop_s(g2, "camera_uid")), "uid is deterministic");
  ok(strstr(prop_s(g1, "camera_uid"), ":shibuyacam#") != NULL, "prefix lower-cased, '#' + hash follows");
  cJSON_Delete(g1); cJSON_Delete(g2);

  /* null-valued url extra falls back to the name */
  cam_kv nu = { .k = "url", .is_null = 1 };
  cJSON *h = cam_make_feature(1, 2, "zzz", "t", "c", &nu, 1);
  ok(strstr(prop_s(h, "camera_uid"), ":zzz#") != NULL, "null url extra -> name used for the tail");
  ok(cJSON_IsNull(cJSON_GetObjectItem(cJSON_GetObjectItem(h, "properties"), "url")), "null extra emitted as null");
  cJSON_Delete(h);

  /* a multi-byte name cut at 44 bytes must not leave a split codepoint */
  char nm[64];
  memset(nm, 'a', 43);
  strcpy(nm + 43, "\xe6\x9d\xb1\xe6\x9d\xb1");  /* 東東: bytes 43..48 */
  cJSON *j = cam_make_feature(1, 2, nm, "t", "c", NULL, 0);
  const char *uid = prop_s(j, "camera_uid");
  const char *tail = strchr(strchr(uid, ':') + 1, ':') + 1;
  const char *hash = strchr(tail, '#');
  int bad = 0;
  for (const char *p = tail; p < hash;) {
    unsigned char ch = (unsigned char)*p;
    int len = ch < 0x80 ? 1 : (ch >> 5) == 6 ? 2 : (ch >> 4) == 14 ? 3 : (ch >> 3) == 30 ? 4 : 0;
    if (!len || p + len > hash) { bad = 1; break; }
    for (int k = 1; k < len; k++) if (((unsigned char)p[k] & 0xC0) != 0x80) bad = 1;
    p += len ? len : 1;
  }
  ok(!bad, "UTF-8 prefix cut on a codepoint boundary");
  cJSON_Delete(j);

  /* extra value kinds, order preserved */
  cam_kv ex[] = { { .k = "n", .is_num = 1, .nv = 4.5 }, { .k = "b", .is_bool = 1, .bv = 1 },
                  { .k = "s", .sv = "v" }, { .k = "z", .sv = NULL } };
  cJSON *x = cam_make_feature(1, 2, "n", "t", "c", ex, 4);
  cJSON *p = cJSON_GetObjectItem(x, "properties");
  ok(cJSON_GetObjectItem(p, "n")->valuedouble == 4.5, "numeric extra");
  ok(cJSON_IsTrue(cJSON_GetObjectItem(p, "b")), "bool extra");
  ok(!strcmp(prop_s(x, "s"), "v"), "string extra");
  ok(cJSON_IsNull(cJSON_GetObjectItem(p, "z")), "NULL sv -> JSON null");
  ok(!strcmp(p->child->next->next->next->next->next->string, "n"), "extras follow the five fixed props in order");
  cJSON_Delete(x);
}

/* ---- probe ------------------------------------------------------------ */
static void t_probe(void) {
  printf("-- probe\n");
  ok(probe_head(NULL, "https://example.invalid/") == 0, "NULL client -> 0");
  ok(probe_head((http_client *)0x1, NULL) == 0, "NULL url -> 0");
  ok(probe_head((http_client *)0x1, "") == 0, "empty url -> 0");
  char b[40];
  probe_iso_now(b, sizeof b);
  int shape = strlen(b) == 24 && b[4] == '-' && b[7] == '-' && b[10] == 'T' && b[13] == ':' &&
              b[16] == ':' && b[19] == '.' && b[23] == 'Z';
  for (int i = 0; shape && i < 23; i++)
    if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && i != 19 && !isdigit((unsigned char)b[i])) shape = 0;
  ok(shape, "ISO-8601 UTC with milliseconds");
  char small[6];
  memset(small, 'Q', sizeof small);
  probe_iso_now(small, 5);
  ok(small[4] == 0 && small[5] == 'Q', "short buffer truncated and terminated, no overrun");
  probe_iso_now(small, 0);
  ok(small[5] == 'Q', "n=0 writes nothing");
}

/* ---- collectortab ----------------------------------------------------- */
static void t_collectortab(void) {
  printf("-- collectortab\n");
  int n = collector_meta_count();
  ok(n > 10, "table is populated from collectors.def");
  ok(collector_meta_at(-1) == NULL && collector_meta_at(n) == NULL, "at() bounds");
  ok(collector_meta_at(0) != NULL && collector_meta_at(n - 1) != NULL, "at() ends");
  ok(collector_meta_get(NULL) == NULL, "get(NULL)");
  ok(collector_meta_get("no-such-collector-key") == NULL, "get(unknown)");
  const collector_meta *g = collector_meta_get("grp_government");
  ok(g && collector_meta_is_group(g), "grp_government is a group");
  ok(!collector_meta_is_group(NULL), "is_group(NULL)");
  ok(!strcmp(collector_group_of("grp_government"), "grp_government"), "a group's group is itself");
  ok(!strcmp(collector_group_of("no-such-collector-key"), COLLECTOR_GROUP_UNASSIGNED), "unknown -> grp_unassigned");
  ok(!strcmp(collector_group_of(NULL), COLLECTOR_GROUP_UNASSIGNED), "NULL -> grp_unassigned");
  int dup = 0, noparent_nongroup = 0, parent_missing = 0;
  for (int i = 0; i < n; i++) {
    const collector_meta *m = collector_meta_at(i);
    if (collector_meta_get(m->key) != m) dup++;
    if (m->parent && m->parent[0]) {
      const collector_meta *p = collector_meta_get(m->parent);
      if (!p || !collector_meta_is_group(p)) parent_missing++;
    } else if (!collector_meta_is_group(m)) noparent_nongroup++;
  }
  ok(dup == 0, "keys are unique (get returns the first == itself)");
  ok(parent_missing == 0, "every parent names an existing group");
  (void)noparent_nongroup;
}

/* ---- linegeom --------------------------------------------------------- */
static cJSON *line_feat(int n, const double *xy, const char *name) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "LineString");
  cJSON *c = cJSON_CreateArray();
  for (int i = 0; i < n; i++) {
    cJSON *p = cJSON_CreateArray();
    cJSON_AddItemToArray(p, cJSON_CreateNumber(xy[2 * i]));
    cJSON_AddItemToArray(p, cJSON_CreateNumber(xy[2 * i + 1]));
    cJSON_AddItemToArray(c, p);
  }
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON *pr = cJSON_CreateObject();
  cJSON_AddStringToObject(pr, "name", name);
  cJSON_AddItemToObject(f, "properties", pr);
  return f;
}
static void t_linegeom(void) {
  printf("-- linegeom\n");
  cJSON *o = linegeom_stitch_smooth(NULL, "rail");
  ok(cJSON_IsArray(o) && cJSON_GetArraySize(o) == 0, "NULL input -> empty array");
  cJSON_Delete(o);
  cJSON *notarr = cJSON_CreateObject();
  o = linegeom_stitch_smooth(notarr, NULL);
  ok(cJSON_IsArray(o) && cJSON_GetArraySize(o) == 0, "non-array input -> empty array");
  cJSON_Delete(o); cJSON_Delete(notarr);
  cJSON *empty = cJSON_CreateArray();
  o = linegeom_stitch_smooth(empty, "rail");
  ok(cJSON_GetArraySize(o) == 0, "empty array in, empty out");
  cJSON_Delete(o); cJSON_Delete(empty);

  /* bus passes through untouched */
  const double A[] = { 0, 0, 1, 1 }, B[] = { 1, 1, 2, 0 };
  cJSON *in = cJSON_CreateArray();
  cJSON_AddItemToArray(in, line_feat(2, A, "a"));
  cJSON_AddItemToArray(in, line_feat(2, B, "b"));
  o = linegeom_stitch_smooth(in, "bus");
  ok(cJSON_GetArraySize(o) == 2, "bus mode: two fragments stay two");
  cJSON *c0 = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(o, 0), "geometry"), "coordinates");
  ok(cJSON_GetArraySize(c0) == 2, "bus mode: geometry not smoothed");
  cJSON_Delete(o);

  /* rail: two fragments meeting at (1,1) stitch into one */
  o = linegeom_stitch_smooth(in, "rail");
  ok(cJSON_GetArraySize(o) == 1, "two fragments sharing a degree-2 endpoint stitch into one line");
  cJSON *cc = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(o, 0), "geometry"), "coordinates");
  ok(cJSON_GetArraySize(cc) > 3, "stitched line is smoothed (more vertices)");
  ok(cJSON_GetArraySize(in) == 2, "input is not consumed");
  cJSON_Delete(o);

  /* a junction of three fragments (degree 3) is not stitched */
  const double C[] = { 1, 1, 1, 2 };
  cJSON_AddItemToArray(in, line_feat(2, C, "c"));
  o = linegeom_stitch_smooth(in, "rail");
  ok(cJSON_GetArraySize(o) == 3, "degree-3 junction keeps three lines");
  cJSON_Delete(o);
  cJSON_Delete(in);

  /* reverse-oriented neighbour still stitches */
  const double R1[] = { 0, 0, 1, 1 }, R2[] = { 2, 0, 1, 1 };
  in = cJSON_CreateArray();
  cJSON_AddItemToArray(in, line_feat(2, R1, "x"));
  cJSON_AddItemToArray(in, line_feat(2, R2, "y"));
  o = linegeom_stitch_smooth(in, "rail");
  ok(cJSON_GetArraySize(o) == 1, "head-to-head fragments stitch");
  cJSON_Delete(o); cJSON_Delete(in);

  /* malformed + non-LineString features pass through */
  in = cJSON_CreateArray();
  cJSON *bad = cJSON_Parse("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\","
                           "\"coordinates\":[[1,2],[3]]},\"properties\":{}}");
  cJSON *pt = cJSON_Parse("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\","
                          "\"coordinates\":[1,2]},\"properties\":{\"k\":1}}");
  cJSON *nogeo = cJSON_Parse("{\"type\":\"Feature\",\"properties\":{}}");
  cJSON *short1 = cJSON_Parse("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\","
                              "\"coordinates\":[[1,2]]},\"properties\":{}}");
  cJSON_AddItemToArray(in, bad); cJSON_AddItemToArray(in, pt);
  cJSON_AddItemToArray(in, nogeo); cJSON_AddItemToArray(in, short1);
  cJSON_AddItemToArray(in, line_feat(2, A, "a"));
  o = linegeom_stitch_smooth(in, "rail");
  ok(cJSON_GetArraySize(o) == 5, "nothing dropped: 4 passthroughs + 1 line");
  cJSON_Delete(o); cJSON_Delete(in);
}

int main(void) {
  t_bigfile();
  t_linecolor();
  t_camfeature();
  t_probe();
  t_collectortab();
  t_linegeom();
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
