/* collectors/social/sources/chan_5ch.c
 * Port of server/src/collectors/chan5ch.js (intelEnvelope).
 * Per-board GET of Shift_JIS subject.txt → decode → parse
 *   "<tid>.dat<TAB>title (replyCount)"  → trending-thread intel rows.
 * uid = chan-5ch|<host>_<board>_<tid> (intelUid → remote_key, sink derives).
 * SEED/_meta/extraMeta NOT ported (live-only; 0 rows when 5ch geo-blocks). */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/csv.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DEFAULT_BOARDS — host:board, mirrored verbatim from the JS default set. */
static const char *BOARDS[] = {
  "greta.5ch.net:newsplus",    /* news+ */
  "medaka.5ch.net:liveplus",   /* news jikkyo */
  "fate.5ch.net:bizplus",      /* business */
  "mevius.5ch.net:sec",        /* security */
  "mao.5ch.net:internet",      /* internet */
};
#define NBOARD ((int)(sizeof(BOARDS) / sizeof(BOARDS[0])))

#define TIMEOUT_MS       12000
#define PER_BOARD_LIMIT  40

/* Parse one subject.txt line into tid / title / replies.
 *   JS: /^(\d+)\.dat\s*[<\t> ]+(.*?)\s*\((\d+)\)\s*$/  (fallback variant
 *   collapses to the same shape). i.e.
 *     leading digits, ".dat", separators, title, "(", digits, ")", trailing ws.
 * Returns 1 on match (out params filled), 0 otherwise. */
static int parse_line(const char *line, char *tid, size_t tn,
                      char *title, size_t tln, long *replies) {
  const char *p = line;
  /* (\d+) */
  const char *ds = p;
  while (*p >= '0' && *p <= '9') p++;
  if (p == ds) return 0;
  size_t dl = (size_t)(p - ds);
  if (dl >= tn) return 0;
  memcpy(tid, ds, dl);
  tid[dl] = 0;
  /* \.dat */
  if (strncmp(p, ".dat", 4) != 0) return 0;
  p += 4;
  /* separators: \s* [<\t> ]+  — accept any run of <, >, tab, space, CR/LF */
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'
         || *p == '<' || *p == '>')
    p++;
  /* title = (.*?) non-greedy up to the FIRST " (NNN)" whose ')' is followed
   * only by optional whitespace to EOL (regex end-anchor + non-greedy). */
  const char *best = NULL;
  for (const char *q = p; *q; q++) {
    if (*q != '(') continue;
    const char *r = q + 1;
    const char *nd = r;
    while (*r >= '0' && *r <= '9') r++;
    if (r == nd || *r != ')') continue;
    const char *e = r + 1;
    while (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n') e++;
    if (*e == 0) { best = q; break; }   /* first match anchored to EOL */
  }
  if (!best || best == p) return 0;
  /* trim trailing \s* before '(' from the title span [p, best) */
  const char *te = best;
  while (te > p && (te[-1] == ' ' || te[-1] == '\t'
                    || te[-1] == '\r' || te[-1] == '\n'))
    te--;
  size_t tl = (size_t)(te - p);
  if (tl == 0 || tl >= tln) return 0;
  memcpy(title, p, tl);
  title[tl] = 0;
  *replies = strtol(best + 1, NULL, 10);
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = {
    "User-Agent: Monazilla/1.00 (JapanOSINT)",
    "Accept: text/plain, */*",
    NULL
  };
  int n = 0;
  for (int b = 0; b < NBOARD; b++) {
    const char *spec = BOARDS[b];
    const char *colon = strchr(spec, ':');
    if (!colon) continue;
    size_t hl = (size_t)(colon - spec);
    char host[128], board[64];
    if (hl >= sizeof host) continue;
    memcpy(host, spec, hl);
    host[hl] = 0;
    snprintf(board, sizeof board, "%s", colon + 1);
    if (!host[0] || !board[0]) continue;

    char url[256];
    snprintf(url, sizeof url, "https://%s/%s/subject.txt", host, board);

    char *raw = feed_get_text(ctx->http, url, TIMEOUT_MS);
    if (!raw) continue;
    /* feed_get_text returns the raw body bytes; subject.txt is Shift_JIS. */
    char *text = csv_decode_sjis(raw, strlen(raw));
    free(raw);
    if (!text) continue;

    /* split on \r?\n, drop empties, slice(0, PER_BOARD_LIMIT) */
    int count = 0;
    char *save = text;
    while (count < PER_BOARD_LIMIT) {
      char *nl = strpbrk(save, "\r\n");
      char *line = save;
      if (nl) {
        char c = *nl;
        *nl = 0;
        save = nl + 1;
        if (c == '\r' && *save == '\n') save++;
      }
      if (line[0]) {
        char tid[64], title[1024];
        long replies = 0;
        if (parse_line(line, tid, sizeof tid,
                        title, sizeof title, &replies)) {
          count++;

          char rk[256];
          snprintf(rk, sizeof rk, "%s_%s_%s", host, board, tid);

          char summary[128];
          snprintf(summary, sizeof summary, "%ld replies on /%s",
                   replies, board);

          char threadurl[320];
          snprintf(threadurl, sizeof threadurl,
                   "https://%s/test/read.cgi/%s/%s/", host, board, tid);

          /* properties — EXACT JS key order */
          cJSON *props = cJSON_CreateObject();
          cJSON_AddStringToObject(props, "board", board);
          cJSON_AddStringToObject(props, "host", host);
          cJSON_AddStringToObject(props, "thread_id", tid);
          cJSON_AddNumberToObject(props, "replies", (double)replies);
          char *pj = cJSON_PrintUnformatted(props);

          char tags[160];
          snprintf(tags, sizeof tags, "[\"5ch\",\"board:%s\"]", board);

          intel_item it = {0};
          it.remote_key      = rk;
          it.title           = title;
          it.summary         = summary;
          it.link            = threadurl;
          it.lang            = "ja";
          it.properties_json = pj;
          it.tags_json       = tags;
          if (sink->emit(sink, &it) >= 0) n++;

          free(pj);
          cJSON_Delete(props);
        }
      }
      if (!nl) break;
    }
    free(text);
  }
  fprintf(stderr, "[chan-5ch] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def chan_5ch_def = {
  .id = "chan-5ch", .collector = "social",
  .name = "5ch Threads", .name_ja = "5ちゃんねる スレッド",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(chan_5ch_def)
