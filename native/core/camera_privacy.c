/* core/camera_privacy.c — see camera_privacy.h for the policy and why. */
#include "camera_privacy.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const DENY[] = {
  "insecam", "shodan", "manual", "ip", "surveillance", "citiwatch", NULL };

static const char *const ALLOW_DEFAULT[] = {
  "road", "roads", "traffic", "highway", "freeway", "expressway", "motorway",
  "river", "rivers", "kasen", "port", "harbor", "harbour", "volcano",
  "weather", "weathercam", "rwis", "mlit", "jma", "nzta", "digitraffic",
  "dot", "511", "gov", "government", NULL };

/* Does `s`, split into lowercase alphanumeric words, contain `word`? */
static int has_word(const char *s, const char *word, size_t wl) {
  if (!s || !wl) return 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    while (*p && !isalnum(*p)) p++;
    const unsigned char *b = p;
    while (*p && isalnum(*p)) p++;
    size_t n = (size_t)(p - b);
    if (n == wl) {
      size_t i = 0;
      while (i < n && tolower(b[i]) == (unsigned char)word[i]) i++;
      if (i == n) return 1;
    }
  }
  return 0;
}

static int any_word(const char *a, const char *b, const char *const *list) {
  for (int i = 0; list[i]; i++) {
    size_t wl = strlen(list[i]);
    if (has_word(a, list[i], wl) || has_word(b, list[i], wl)) return 1;
  }
  return 0;
}

/* The operator's allow words, or the default. Read per call: it is a getenv
 * and a short split, and caching would make the override untestable. */
static int allowed(const char *a, const char *b) {
  const char *ov = getenv("JO_CAMERA_INDEX_CHANNELS");
  if (!ov || !*ov) return any_word(a, b, ALLOW_DEFAULT);
  char w[64];
  const char *p = ov;
  while (*p) {
    size_t n = strcspn(p, ",");
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < sizeof w; i++)
      if (!isspace((unsigned char)p[i])) w[o++] = (char)tolower((unsigned char)p[i]);
    w[o] = '\0';
    if (o && strcmp(w, "none") != 0 && (has_word(a, w, o) || has_word(b, w, o)))
      return 1;
    p += n;
    if (*p == ',') p++;
  }
  return 0;
}

int camera_channel_indexable(const char *channel, const char *source_id) {
  if ((!channel || !*channel) && (!source_id || !*source_id)) return 0;
  if (any_word(channel, source_id, DENY)) return 0;
  return allowed(channel, source_id);
}

int camera_record_indexable(const char *props, const char *source_id) {
  cJSON *r = props ? cJSON_Parse(props) : NULL;
  const cJSON *dc = r ? cJSON_GetObjectItemCaseSensitive(r, "discovery_channel") : NULL;
  const cJSON *dcs = r ? cJSON_GetObjectItemCaseSensitive(r, "discovery_channels") : NULL;
  const char *primary = cJSON_IsString(dc) ? dc->valuestring : NULL;
  /* A deny on ANY channel wins: a camera that insecam also found is an
   * unsecured private device, whatever else listed it. */
  const cJSON *e;
  if (primary && any_word(primary, NULL, DENY)) { cJSON_Delete(r); return 0; }
  cJSON_ArrayForEach(e, dcs)
    if (cJSON_IsString(e) && any_word(e->valuestring, NULL, DENY)) {
      cJSON_Delete(r); return 0; }
  int ok = camera_channel_indexable(primary, source_id);
  if (!ok)
    cJSON_ArrayForEach(e, dcs)
      if (cJSON_IsString(e) && camera_channel_indexable(e->valuestring, source_id)) {
        ok = 1; break; }
  cJSON_Delete(r);
  return ok;
}
