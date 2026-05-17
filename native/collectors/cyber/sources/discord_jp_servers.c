/* collectors/cyber/sources/discord_jp_servers.c
 * Port of server/src/collectors/discordJpServers.js (intelEnvelope).
 * Key-free best-effort: scrape Disboard JP-tagged listing HTML, extract
 * /server/join/<id> ... </a> pairs. Honest empty on failure. No seed.
 * uid = discord-jp-servers|<id>. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *LIST_URLS[] = {
  "https://disboard.org/ja/servers/tag/japanese",
  "https://disboard.org/ja/servers/tag/\xE6\x97\xA5\xE6\x9C\xAC", /* 日本 */
};

#define MAX_SEEN 8192

static int seen_has(char **seen, int ns, const char *id) {
  for (int i = 0; i < ns; i++) if (!strcmp(seen[i], id)) return 1;
  return 0;
}

/* decodeEntities + strip tags + collapse ws + trim → out (caller buf). */
static void clean_text(const char *raw, size_t rawlen, char *out, size_t on) {
  /* strip tags into tmp */
  char *tmp = malloc(rawlen + 1);
  if (!tmp) { out[0] = '\0'; return; }
  size_t t = 0;
  int intag = 0;
  for (size_t i = 0; i < rawlen; i++) {
    char c = raw[i];
    if (c == '<') { intag = 1; tmp[t++] = ' '; continue; }
    if (c == '>') { intag = 0; continue; }
    if (!intag) tmp[t++] = c;
  }
  tmp[t] = '\0';

  /* decode a small set of entities + collapse whitespace + trim */
  size_t o = 0;
  int pending_ws = 0, started = 0;
  for (size_t i = 0; i < t && o + 8 < on;) {
    unsigned char c = (unsigned char)tmp[i];
    unsigned int cp = 0;
    int consumed = 0;
    if (c == '&') {
      if (tmp[i + 1] == '#' && (tmp[i + 2] == 'x' || tmp[i + 2] == 'X')) {
        char *e; long v = strtol(tmp + i + 3, &e, 16);
        if (*e == ';') { cp = (unsigned int)v; consumed = (int)(e - (tmp + i)) + 1; }
      } else if (tmp[i + 1] == '#') {
        char *e; long v = strtol(tmp + i + 2, &e, 10);
        if (*e == ';') { cp = (unsigned int)v; consumed = (int)(e - (tmp + i)) + 1; }
      } else if (!strncmp(tmp + i, "&amp;", 5))  { cp = '&'; consumed = 5; }
      else if (!strncmp(tmp + i, "&lt;", 4))   { cp = '<'; consumed = 4; }
      else if (!strncmp(tmp + i, "&gt;", 4))   { cp = '>'; consumed = 4; }
      else if (!strncmp(tmp + i, "&quot;", 6)) { cp = '"'; consumed = 6; }
      else if (!strncmp(tmp + i, "&#39;", 5))  { cp = '\''; consumed = 5; }
      else if (!strncmp(tmp + i, "&apos;", 6)) { cp = '\''; consumed = 6; }
    }
    char enc[8]; size_t el = 0;
    if (consumed) {
      i += (size_t)consumed;
      if (cp < 0x80) enc[el++] = (char)cp;
      else if (cp < 0x800) {
        enc[el++] = (char)(0xC0 | (cp >> 6));
        enc[el++] = (char)(0x80 | (cp & 0x3F));
      } else if (cp < 0x10000) {
        enc[el++] = (char)(0xE0 | (cp >> 12));
        enc[el++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        enc[el++] = (char)(0x80 | (cp & 0x3F));
      } else {
        enc[el++] = (char)(0xF0 | (cp >> 18));
        enc[el++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        enc[el++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        enc[el++] = (char)(0x80 | (cp & 0x3F));
      }
    } else {
      enc[el++] = (char)c;
      i++;
    }
    for (size_t k = 0; k < el && o + 8 < on; k++) {
      unsigned char ec = (unsigned char)enc[k];
      if (ec == ' ' || ec == '\t' || ec == '\n' || ec == '\r' ||
          ec == '\f' || ec == '\v') {
        if (started) pending_ws = 1;
      } else {
        if (pending_ws) { out[o++] = ' '; pending_ws = 0; }
        out[o++] = (char)ec;
        started = 1;
      }
    }
  }
  out[o] = '\0';
  free(tmp);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *seen[MAX_SEEN];
  int ns = 0, n = 0, gotAny = 0;

  for (size_t ui = 0; ui < sizeof(LIST_URLS) / sizeof(LIST_URLS[0]); ui++) {
    char *html = feed_get_text(ctx->http, LIST_URLS[ui], 15000);
    if (!html) continue;
    gotAny = 1;

    /* re: /\/server\/join\/(\d{6,25})[^"']*["'][^>]*>([\s\S]*?)<\/a>/g */
    const char *p = html;
    const char *needle = "/server/join/";
    size_t nlen = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
      const char *q = p + nlen;
      const char *ds = q;
      while (*q && isdigit((unsigned char)*q)) q++;
      size_t dl = (size_t)(q - ds);
      if (dl < 6 || dl > 25) { p += nlen; continue; }
      char id[26];
      memcpy(id, ds, dl); id[dl] = '\0';

      /* [^"']* then ["'] */
      const char *r = q;
      while (*r && *r != '"' && *r != '\'') r++;
      if (!*r) break;
      r++;                                  /* past the quote */
      /* [^>]*> */
      while (*r && *r != '>') r++;
      if (!*r) break;
      r++;                                  /* past '>' */
      /* ([\s\S]*?)</a> — lazy: find first </a> */
      const char *end = strstr(r, "</a>");
      if (!end) { p = q; continue; }

      if (!seen_has(seen, ns, id)) {
        if (ns < MAX_SEEN) seen[ns++] = strdup(id);
        char name[1024];
        clean_text(r, (size_t)(end - r), name, sizeof name);
        char namebuf[1100];
        if (name[0]) snprintf(namebuf, sizeof namebuf, "%s", name);
        else snprintf(namebuf, sizeof namebuf, "Discord server %s", id);

        char link[64];
        snprintf(link, sizeof link, "https://disboard.org/server/%s", id);

        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("discord"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("server"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("japanese"));
        char *tj = cJSON_PrintUnformatted(tags);

        cJSON *pr = cJSON_CreateObject();          /* EXACT JS key order */
        cJSON_AddStringToObject(pr, "server_id", id);
        cJSON_AddStringToObject(pr, "source", "disboard");
        char *pj = cJSON_PrintUnformatted(pr);

        intel_item it = {0};
        it.remote_key     = id;
        it.title          = namebuf;
        it.summary        = "Japan-tagged public Discord server (Disboard listing)";
        it.link           = link;
        it.lang           = "ja";
        it.record_type    = "discord-jp-servers";
        it.properties_json = pj;
        it.tags_json      = tj;
        if (sink->emit(sink, &it) >= 0) n++;

        free(pj); free(tj);
        cJSON_Delete(pr); cJSON_Delete(tags);
      }
      p = end + 4;
    }
    free(html);
  }
  for (int i = 0; i < ns; i++) free(seen[i]);

  if (!gotAny) {
    fprintf(stderr, "[discord-jp-servers] unavailable\n");
    return -1;
  }
  fprintf(stderr, "[discord-jp-servers] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def discord_jp_servers_def = {
  .id = "discord-jp-servers", .collector = "cyber",
  .name = "Discord public servers (JP)",
  .name_ja = "Discord \xE3\x83\x91\xE3\x83\x96\xE3\x83\xAA\xE3\x83\x83\xE3\x82\xAF\xE3\x82\xB5\xE3\x83\xBC\xE3\x83\x90\xE3\x83\xBC (\xE6\x97\xA5\xE6\x9C\xAC)",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(discord_jp_servers_def)
