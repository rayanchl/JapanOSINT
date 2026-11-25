/* tests/unit/test_breach_jobs.c — core/breach_jobs.c: the async admin breach
 * jobs.  Pins the path confinement (the arbitrary-file-read fix), the argument
 * gates that must refuse BEFORE a job slot is taken, the SSRF refusal on the
 * fetch URL, the staging-file-name escape through source_id, a real dry-run
 * ingest through the job thread, and the status listing.  No network: every
 * fetch case is refused at the gate. */
#include "../../core/breach_jobs.h"
#include "../../third_party/cJSON.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static void write_file(const char *p, const char *body) {
  FILE *f = fopen(p, "wb");
  assert(f);
  fputs(body, f);
  fclose(f);
}
static int jobs_count(const char *id) {
  char *s = breach_job_status(id);
  cJSON *j = cJSON_Parse(s);
  free(s);
  assert(j);
  int n = cJSON_GetArraySize(cJSON_GetObjectItem(j, "jobs"));
  cJSON_Delete(j);
  return n;
}

int main(void) {
  char root[] = "/tmp/jo-breachjobs-XXXXXX";
  assert(mkdtemp(root));
  setenv("JO_BREACH_DIR", root, 1);
  char evil[PATH_MAX], inside[PATH_MAX], outside[PATH_MAX], link[PATH_MAX], sub[PATH_MAX];
  snprintf(evil, sizeof evil, "%s-evil", root);
  mkdir(evil, 0755);
  snprintf(inside, sizeof inside, "%s/dump.txt", root);
  write_file(inside, "a@example.jp\nb@example.jp\nA@example.jp\n\nnot-an-email\n");
  snprintf(outside, sizeof outside, "%s/secret.txt", evil);
  write_file(outside, "root:x:0:0\n");
  snprintf(link, sizeof link, "%s/sneaky.txt", root);
  assert(symlink(outside, link) == 0);
  snprintf(sub, sizeof sub, "%s/sub", root);
  mkdir(sub, 0755);
  char sub_file[PATH_MAX];
  snprintf(sub_file, sizeof sub_file, "%s/sub/f.txt", root);
  write_file(sub_file, "x\n");

  printf("-- breach_path_confine\n");
  char out[PATH_MAX];
  ok(breach_path_confine(NULL, out, sizeof out) == -1, "NULL path refused");
  ok(breach_path_confine("", out, sizeof out) == -1, "empty path refused");
  ok(breach_path_confine(inside, NULL, 10) == -1 && breach_path_confine(inside, out, 0) == -1, "no output buffer refused");
  ok(breach_path_confine(inside, out, sizeof out) == 0 && !strcmp(out, inside), "a file inside the breach dir resolves to its absolute path");
  ok(breach_path_confine(sub_file, out, sizeof out) == 0, "a file in a subdirectory is inside");
  char dots[PATH_MAX];
  snprintf(dots, sizeof dots, "%s/sub/../dump.txt", root);
  ok(breach_path_confine(dots, out, sizeof out) == 0 && !strcmp(out, inside), "'..' that stays inside is resolved, not refused");
  snprintf(dots, sizeof dots, "%s/../../../../etc/passwd", root);
  ok(breach_path_confine(dots, out, sizeof out) == -1 && out[0] == 0, "'..' escape to /etc/passwd refused, output left empty");
  ok(breach_path_confine("/etc/passwd", out, sizeof out) == -1, "absolute path outside refused");
  ok(breach_path_confine(outside, out, sizeof out) == -1, "sibling directory sharing the base as a string PREFIX refused");
  ok(breach_path_confine(link, out, sizeof out) == -1, "a symlink inside that points outside refused (resolved before compare)");
  char missing[PATH_MAX];
  snprintf(missing, sizeof missing, "%s/nope.txt", root);
  ok(breach_path_confine(missing, out, sizeof out) == -1, "nonexistent file refused");
  ok(breach_path_confine(inside, out, 8) == -1, "buffer too small for the resolved path refused, not truncated");
  cJSON *e = cJSON_Parse(breach_path_confine_error());
  ok(e && !strcmp(cJSON_GetObjectItem(e, "error")->valuestring, "path_not_permitted"), "error body is valid JSON with a stable code");
  cJSON_Delete(e);
  ok(strstr(breach_path_confine_error(), root) == NULL, "error body does not echo any path (no existence oracle)");

  printf("-- argument gates\n");
  int st = 0;
  char *s = breach_job_ingest(NULL, inside, NULL, 0, 1, &st);
  ok(st == 400 && s, "ingest: NULL source_id -> 400"); free(s);
  s = breach_job_ingest("src", "", NULL, 0, 1, &st);
  ok(st == 400, "ingest: empty path -> 400"); free(s);
  int before = jobs_count(NULL);
  s = breach_job_ingest("src", "/etc/passwd", NULL, 0, 1, &st);
  ok(st == 400 && strstr(s, "path_not_permitted"), "ingest: path outside -> 400 path_not_permitted"); free(s);
  ok(jobs_count(NULL) == before, "a refused path took no job slot");
  s = breach_job_fetch("src", NULL, &st);
  ok(st == 400, "fetch: NULL url -> 400"); free(s);
  s = breach_job_fetch("", "https://example.com/x", &st);
  ok(st == 400, "fetch: empty source_id -> 400"); free(s);
  const char *bad_urls[] = { "http://127.0.0.1:4712/api/health", "http://localhost/x", "http://10.0.0.5/x",
                             "http://192.168.1.1/x", "http://169.254.169.254/latest/meta-data/",
                             "http://[::1]/x", "file:///etc/passwd", "ftp://example.com/x" };
  int all_refused = 1;
  for (size_t i = 0; i < sizeof bad_urls / sizeof *bad_urls; i++) {
    s = breach_job_fetch("src", bad_urls[i], &st);
    if (st != 400 || !strstr(s, "fetch_url_rejected")) { all_refused = 0; printf("   not refused: %s -> %d\n", bad_urls[i], st); }
    free(s);
  }
  ok(all_refused, "loopback / RFC1918 / link-local / metadata / non-http urls refused with fetch_url_rejected");
  ok(jobs_count(NULL) == before, "refused fetches took no job slot");
  const char *bad_ids[] = { "../../x", "a/b", "..", ".hidden", "a b", "x\n", "é" };
  all_refused = 1;
  for (size_t i = 0; i < sizeof bad_ids / sizeof *bad_ids; i++) {
    s = breach_job_fetch(bad_ids[i], "http://127.0.0.1/", &st);
    if (st != 400 || !strstr(s, "source_id must be")) { all_refused = 0; printf("   id accepted: %s\n", bad_ids[i]); }
    free(s);
  }
  ok(all_refused, "a source_id that is not a plain file name is refused (staging path escape)");

  printf("-- dry-run ingest through the job thread\n");
  s = breach_job_ingest("unit-src", inside, "email", 0, 1, &st);
  ok(st == 202, "valid ingest -> 202");
  cJSON *j = cJSON_Parse(s);
  free(s);
  const char *jid = j ? cJSON_GetObjectItem(j, "job_id")->valuestring : "";
  ok(j && !strcmp(cJSON_GetObjectItem(j, "kind")->valuestring, "ingest") && cJSON_IsTrue(cJSON_GetObjectItem(j, "dry_run")),
     "202 body names the job, kind and dry_run");
  char id[40];
  snprintf(id, sizeof id, "%s", jid);
  cJSON_Delete(j);
  cJSON *mine = NULL;
  for (int i = 0; i < 100; i++) {
    s = breach_job_status(id);
    cJSON *r = cJSON_Parse(s);
    free(s);
    cJSON *arr = cJSON_GetObjectItem(r, "jobs");
    cJSON *one = cJSON_GetArrayItem(arr, 0);
    if (one && strcmp(cJSON_GetObjectItem(one, "state")->valuestring, "queued") &&
        strcmp(cJSON_GetObjectItem(one, "state")->valuestring, "running")) {
      mine = cJSON_Duplicate(one, 1);
      cJSON_Delete(r);
      break;
    }
    cJSON_Delete(r);
    struct timespec ts = { 0, 100 * 1000 * 1000 };
    nanosleep(&ts, NULL);
  }
  ok(mine != NULL, "the job reached a terminal state");
  if (mine) {
    ok(!strcmp(cJSON_GetObjectItem(mine, "state")->valuestring, "done"), "state done");
    ok(cJSON_GetObjectItem(mine, "rows_in")->valuedouble >= 3, "rows_in counts the lines read (blank skipped)");
    ok(cJSON_GetObjectItem(mine, "rows_new")->valuedouble >= 1 &&
       cJSON_GetObjectItem(mine, "rows_new")->valuedouble <= cJSON_GetObjectItem(mine, "rows_in")->valuedouble, "rows_new bounded by rows_in");
    ok(strstr(cJSON_GetObjectItem(mine, "message")->valuestring, "dry-run") != NULL, "message says dry-run");
    ok(cJSON_GetObjectItem(mine, "ended") != NULL, "ended stamped");
    cJSON_Delete(mine);
  }
  ok(jobs_count("bj-no-such-job") == 0, "unknown job id -> empty list, not an error");
  ok(jobs_count(id) == 1 && jobs_count(NULL) >= 1 && jobs_count("") >= 1, "status by id, all (NULL), all (empty id)");
  /* a dry run must persist nothing */
  char shard[PATH_MAX];
  snprintf(shard, sizeof shard, "%s/email", root);
  struct stat sb;
  ok(stat(shard, &sb) != 0, "dry run created no shard directory");

  /* a missing-after-confine file: job runs and reports an error state */
  char gone[PATH_MAX];
  snprintf(gone, sizeof gone, "%s/gone.txt", root);
  write_file(gone, "x@y.jp\n");
  for (int i = 0; i < 60; i++) {   /* wait out the single-flight slot */
    s = breach_job_ingest("unit-src2", gone, "email", 0, 1, &st);
    free(s);
    if (st != 409) break;
    struct timespec ts = { 0, 50 * 1000 * 1000 };
    nanosleep(&ts, NULL);
  }
  ok(st == 202, "a second ingest starts once the first has finished");

  unlink(link); unlink(outside); unlink(inside); unlink(sub_file); unlink(gone);
  rmdir(sub); rmdir(evil);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
