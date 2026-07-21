/* collectors/cyber/sources/telegram_jp_channels.c
 * Port of server/src/collectors/telegramJpChannels.js (intelEnvelope).
 * Keyed: TGSTAT_API_KEY (channels/search) | TELEGRAM_API_ID+HASH (no userbot
 * → honest empty). No usable key → 0 rows. uid = telegram-jp-channels|<id>.
 * No seed. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *QUERIES[] = {
  "\xE6\x97\xA5\xE6\x9C\xAC",            /* 日本 */
  "\xE6\x9D\xB1\xE4\xBA\xAC",            /* 東京 */
  "\xE3\x83\x8B\xE3\x83\xA5\xE3\x83\xBC\xE3\x82\xB9",  /* ニュース */
  "OSINT",
  "\xE3\x82\xBB\xE3\x82\xAD\xE3\x83\xA5\xE3\x83\xAA\xE3\x83\x86\xE3\x82\xA3",  /* セキュリティ */
};

static void uric(const char *s, char *out, size_t n) {
  size_t o = 0;
  for (; *s && o + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
      out[o++] = (char)c;
    } else {
      snprintf(out + o, n - o, "%%%02X", c);
      o += 3;
    }
  }
  out[o] = '\0';
}

static const char *sv(cJSON *o, const char *k) {
  if (!o) return NULL;
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

/* dedup set (small, linear) */
#define MAX_SEEN 4096
static int seen_has(char **seen, int ns, const char *id) {
  for (int i = 0; i < ns; i++) if (!strcmp(seen[i], id)) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *tgstat = getenv("TGSTAT_API_KEY");
  const char *apiId  = getenv("TELEGRAM_API_ID");
  const char *apiHash = getenv("TELEGRAM_API_HASH");

  if (!(tgstat && *tgstat)) {
    if (apiId && *apiId && apiHash && *apiHash)
      fprintf(stderr, "[telegram-jp-channels] TELEGRAM_API_ID/HASH present but "
                      "no MTProto session — honest empty\n");
    else
      fprintf(stderr, "[telegram-jp-channels] gated (no TGSTAT_API_KEY)\n");
    return 0;
  }

  char *seen[MAX_SEEN];
  int ns = 0, n = 0;
  size_t nq = sizeof(QUERIES) / sizeof(QUERIES[0]);
  for (size_t qi = 0; qi < nq; qi++) {
    const char *q = QUERIES[qi];
    char qenc[256];
    uric(q, qenc, sizeof qenc);
    char url[768];
    snprintf(url, sizeof url,
      "https://api.tgstat.com/channels/search?token=%s&q=%s&country=JP&limit=30",
      tgstat, qenc);
    cJSON *data = feed_get_json(ctx->http, url, 15000);
    if (!data) continue;
    cJSON *resp = cJSON_GetObjectItem(data, "response");
    cJSON *list = resp ? cJSON_GetObjectItem(resp, "items") : NULL;
    if (!cJSON_IsArray(list))
      list = resp ? cJSON_GetObjectItem(resp, "channels") : NULL;
    if (cJSON_IsArray(list)) {
      cJSON *c;
      cJSON_ArrayForEach(c, list) {
        /* username = c.username || c.link || c.channel.username */
        const char *username = sv(c, "username");
        if (!username) username = sv(c, "link");
        if (!username) {
          cJSON *ch = cJSON_GetObjectItem(c, "channel");
          username = sv(ch, "username");
        }
        /* id = username || c.tg_id || c.id */
        char idbuf[64];
        const char *id = username;
        if (!id) {
          cJSON *tg = cJSON_GetObjectItem(c, "tg_id");
          cJSON *ci = cJSON_GetObjectItem(c, "id");
          if (tg && cJSON_IsNumber(tg)) {
            snprintf(idbuf, sizeof idbuf, "%g", tg->valuedouble); id = idbuf;
          } else if (tg && cJSON_IsString(tg) && tg->valuestring[0]) {
            id = tg->valuestring;
          } else if (ci && cJSON_IsNumber(ci)) {
            snprintf(idbuf, sizeof idbuf, "%g", ci->valuedouble); id = idbuf;
          } else if (ci && cJSON_IsString(ci) && ci->valuestring[0]) {
            id = ci->valuestring;
          }
        }
        if (!id) continue;
        if (seen_has(seen, ns, id)) continue;
        if (ns < MAX_SEEN) seen[ns++] = strdup(id);

        /* link: username ? https://t.me/<u w/o @> : c.link || null */
        char linkbuf[256]; const char *link = NULL;
        if (username) {
          const char *u = username;
          if (u[0] == '@') u++;
          snprintf(linkbuf, sizeof linkbuf, "https://t.me/%s", u);
          link = linkbuf;
        } else {
          link = sv(c, "link");
        }

        const char *title = sv(c, "title");
        if (!title) title = username ? username : id;
        const char *about = sv(c, "about");
        if (!about) about = sv(c, "description");

        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("telegram"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("channel"));
        char qtag[280];
        snprintf(qtag, sizeof qtag, "q:%s", q);
        cJSON_AddItemToArray(tags, cJSON_CreateString(qtag));
        char *tj = cJSON_PrintUnformatted(tags);

        cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
        cJSON_AddItemToObject(p, "username",
          username ? cJSON_CreateString(username) : cJSON_CreateNull());
        cJSON *pc = cJSON_GetObjectItem(c, "participants_count");
        if (!(pc && (cJSON_IsNumber(pc) || cJSON_IsString(pc))))
          pc = cJSON_GetObjectItem(c, "subscribers");
        cJSON_AddItemToObject(p, "participants",
          (pc && !cJSON_IsNull(pc)) ? cJSON_Duplicate(pc, 1)
                                    : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "matched_query", q);
        cJSON_AddStringToObject(p, "source", "tgstat_api");
        char *pj = cJSON_PrintUnformatted(p);

        intel_item it = {0};
        it.remote_key     = id;
        it.title          = title;
        it.summary        = about;
        it.body           = about;
        it.link           = link;
        it.author         = username;
        it.lang           = "ja";
        it.record_type    = "telegram-jp-channels";
        it.properties_json = pj;
        it.tags_json      = tj;
        if (sink->emit(sink, &it) >= 0) n++;

        free(pj); free(tj);
        cJSON_Delete(p); cJSON_Delete(tags);
      }
    }
    cJSON_Delete(data);
  }
  for (int i = 0; i < ns; i++) free(seen[i]);
  fprintf(stderr, "[telegram-jp-channels] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def telegram_jp_channels_def = {
  .id = "telegram-jp-channels", .collector = "cyber",
  .name = "Telegram public channels (JP)",
  .name_ja = "Telegram \xE6\x97\xA5\xE6\x9C\xAC\xE3\x83\x91\xE3\x83\x96\xE3\x83\xAA\xE3\x83\x83\xE3\x82\xAF\xE3\x83\x81\xE3\x83\xA3\xE3\x83\x8D\xE3\x83\xAB",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(telegram_jp_channels_def)
