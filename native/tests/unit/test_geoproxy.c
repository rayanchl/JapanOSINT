/* tests/unit/test_geoproxy.c — core/geoproxyapi.c with NO network: every case is
 * answered from the 24 h in-process cache, which the test seeds through the
 * module's own cache_put().  Pins the cache (owned copies, expiry, slot reuse,
 * eviction at 256 entries, key collisions), the qAlt merge/dedupe and its
 * via_translation flags, the reverse path's fromCache stamp, the Plateau
 * LOD-preference picker and its skip rules, and the small helpers (urlenc,
 * join3, jnum, hit_key).  Includes the .c, so run.sh excludes
 * obj/core/geoproxyapi.o from this test's link line. */
#include "../../core/geoproxyapi.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static cJSON *pj(char *s) {
  assert(s);
  cJSON *j = cJSON_Parse(s);
  free(s);
  assert(j);
  return j;
}

int main(void) {
  printf("-- cache\n");
  char *g = cache_get("nope");
  ok(g == NULL, "miss -> NULL");
  cache_put("k1", "[1]", 3600);
  g = cache_get("k1");
  ok(g && !strcmp(g, "[1]"), "hit returns the value");
  cache_put("k1", "[2]", 3600);
  char *g2 = cache_get("k1");
  ok(g2 && !strcmp(g2, "[2]") && !strcmp(g, "[1]"), "overwrite replaces; an earlier copy stays valid (owned, not borrowed)");
  free(g); free(g2);
  cache_put("exp", "x", -5);
  ok(cache_get("exp") == NULL, "an expired entry is a miss");
  int before = ncache;
  cache_put("exp2", "y", 3600);   /* must reuse the expired slot, not grow */
  ok(ncache == before, "an expired slot is reused before the table grows");
  for (int i = 0; i < 400; i++) {
    char k[32];
    snprintf(k, sizeof k, "bulk-%d", i);
    cache_put(k, "v", 100000 + i);
  }
  ok(ncache == 256, "table is bounded at 256 entries");
  char *last = cache_get("bulk-399");
  ok(last != NULL, "the newest entry is cached after the table filled");
  free(last);
  char *first = cache_get("bulk-0");
  ok(first == NULL, "the soonest-expiring entry was evicted, not the newest");
  free(first);
  char longk[400];
  memset(longk, 'q', sizeof longk - 1); longk[sizeof longk - 1] = 0;
  cache_put(longk, "long", 3600);
  char *lg = cache_get(longk);
  ok(lg == NULL || !strcmp(lg, "long"), "a key longer than the slot never returns another key's value");
  free(lg);

  /* The eviction tests filled the table with long-lived entries; start the
   * lookup tests from an empty one (production TTLs are all 24 h, so the
   * "evict soonest-expiring" rule never sees the short TTLs used above). */
  for (int i = 0; i < ncache; i++) { free(CACHE[i].val); CACHE[i].val = NULL; CACHE[i].exp = 0; CACHE[i].key[0] = 0; }
  ncache = 0;

  printf("-- helpers\n");
  char e[64];
  urlenc("a b&c/日", e, sizeof e);
  ok(!strcmp(e, "a%20b%26c%2F%E6%97%A5"), "urlenc percent-encodes reserved and UTF-8 bytes");
  urlenc("AZaz09-_.~", e, sizeof e);
  ok(!strcmp(e, "AZaz09-_.~"), "unreserved set passes through");
  urlenc("日本語日本語日本語", e, 12);
  ok(strlen(e) < 12 && (strlen(e) % 3) == 0, "urlenc stops on a whole escape at the buffer limit");
  urlenc("", e, sizeof e);
  ok(e[0] == 0, "empty input");
  cJSON *o = cJSON_Parse("{\"a\":\"x\",\"b\":\"\",\"c\":\"z\",\"n\":5,\"s\":\"2.5\",\"bad\":\"zz\"}");
  cJSON *jo = join3(o, "a", "b", "c");
  ok(!strcmp(jo->valuestring, "x, z"), "join3 skips empty and joins with ', '");
  cJSON_Delete(jo);
  jo = join3(o, "nokey", "n", "b");
  ok(!strcmp(jo->valuestring, ""), "join3 of nothing is the empty string, not NULL");
  cJSON_Delete(jo);
  ok(jnum(o, "n") == 5 && jnum(o, "s") == 2.5 && isnan(jnum(o, "missing")), "jnum: number, numeric string, missing -> NaN");
  cJSON_Delete(o);
  char k[32];
  cJSON *h = cJSON_Parse("{\"lat\":35.123456,\"lon\":139.5}");
  ok(hit_key(h, k, sizeof k) && !strcmp(k, "35.12346,139.50000"), "hit_key rounds to 5 dp");
  cJSON_Delete(h);
  h = cJSON_Parse("{\"lat\":\"35\",\"lon\":139.5}");
  ok(!hit_key(h, k, sizeof k), "hit_key refuses a non-numeric coordinate instead of dereferencing it");
  cJSON_Delete(h);

  printf("-- geoproxy_geocode_forward (cache only)\n");
  cache_put("fwd:tokyo", "[{\"lat\":35.68,\"lon\":139.76,\"display_name\":\"Tokyo\",\"source\":\"gsi\"},"
                         "{\"lat\":35.0,\"lon\":139.0,\"display_name\":\"Other\",\"source\":\"gsi\"}]", 3600);
  cache_put("fwd:東京", "[{\"lat\":35.68,\"lon\":139.76,\"display_name\":\"東京\",\"source\":\"photon\"},"
                        "{\"lat\":36.0,\"lon\":140.0,\"display_name\":\"Alt\",\"source\":\"photon\"},{\"display_name\":\"no coords\"}]", 3600);
  cJSON *r = pj(geoproxy_geocode_forward("tokyo", NULL));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(r, "results")) == 2 &&
         !strcmp(cJSON_GetObjectItem(r, "provider")->valuestring, "gsi"), "single query: cached results + provider");
  cJSON_Delete(r);
  r = pj(geoproxy_geocode_forward("tokyo", "tokyo"));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(r, "results")) == 2 &&
         cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(r, "results"), 0), "via_translation") == NULL,
     "qAlt equal to q is ignored (no merge flags)");
  cJSON_Delete(r);
  r = pj(geoproxy_geocode_forward("tokyo", ""));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(r, "results")) == 2, "empty qAlt is ignored");
  cJSON_Delete(r);
  r = pj(geoproxy_geocode_forward("tokyo", "東京"));
  cJSON *res = cJSON_GetObjectItem(r, "results");
  ok(cJSON_GetArraySize(res) == 3, "merge: 2 + 3 hits, one duplicate coordinate and one coordinate-less hit dropped => 3");
  ok(cJSON_IsFalse(cJSON_GetObjectItem(cJSON_GetArrayItem(res, 0), "via_translation")), "primary hits flagged via_translation=false");
  cJSON *last_hit = cJSON_GetArrayItem(res, 2);
  ok(cJSON_IsTrue(cJSON_GetObjectItem(last_hit, "via_translation")) &&
         !strcmp(cJSON_GetObjectItem(last_hit, "matched_alt")->valuestring, "東京"), "alternate-only hit flagged via_translation=true with matched_alt");
  ok(!strcmp(cJSON_GetObjectItem(r, "provider")->valuestring, "gsi"), "provider is the primary's (copied before the trees were freed)");
  cJSON_Delete(r);
  cache_put("fwd:onlyalt", "[]", 3600);
  r = pj(geoproxy_geocode_forward("onlyalt", "東京"));
  ok(!strcmp(cJSON_GetObjectItem(r, "provider")->valuestring, "photon") &&
         cJSON_GetArraySize(cJSON_GetObjectItem(r, "results")) == 2, "empty primary: provider falls to the alternate's (its coordinate-less hit dropped)");
  cJSON_Delete(r);
  cache_put("fwd:garbage", "not json at all", 3600);
  r = pj(geoproxy_geocode_forward("garbage", NULL));
  ok(cJSON_IsArray(cJSON_GetObjectItem(r, "results")) && cJSON_GetArraySize(cJSON_GetObjectItem(r, "results")) == 0 &&
         cJSON_IsNull(cJSON_GetObjectItem(r, "provider")), "a corrupt cache entry degrades to an empty result with provider null");
  cJSON_Delete(r);
  {
    /* merge dedupe table has 64 slots: >64 distinct hits must not overflow it */
    char big[8192];
    size_t n = 0;
    n += (size_t)snprintf(big + n, sizeof big - n, "[");
    for (int i = 0; i < 80; i++)
      n += (size_t)snprintf(big + n, sizeof big - n, "%s{\"lat\":%d.5,\"lon\":1.5,\"source\":\"x\"}", i ? "," : "", i);
    snprintf(big + n, sizeof big - n, "]");
    cache_put("fwd:many", big, 3600);
    r = pj(geoproxy_geocode_forward("many", "tokyo"));
    ok(cJSON_GetArraySize(cJSON_GetObjectItem(r, "results")) >= 80, "more than 64 distinct hits: none lost, no overflow");
    cJSON_Delete(r);
  }

  printf("-- geoproxy_geocode_reverse (cache only)\n");
  cache_put("rev2:35.68000,139.76000", "{\"lat\":35.68,\"lon\":139.76,\"display_name\":\"Chiyoda\",\"source\":\"gsi\"}", 3600);
  r = pj(geoproxy_geocode_reverse(35.68, 139.76));
  ok(!strcmp(cJSON_GetObjectItem(r, "display_name")->valuestring, "Chiyoda") && cJSON_IsTrue(cJSON_GetObjectItem(r, "fromCache")),
     "cached reverse hit carries fromCache=true");
  cJSON_Delete(r);
  r = pj(geoproxy_geocode_reverse(35.680001, 139.760004));
  ok(cJSON_IsTrue(cJSON_GetObjectItem(r, "fromCache")), "coordinates are keyed at 5 dp (sub-metre jitter still hits)");
  cJSON_Delete(r);

  printf("-- plateau catalogue (cache only)\n");
  cache_put("plateau:ds",
    "[{\"id\":\"d1\",\"city\":{\"name\":\"Chiyoda\"},\"prefecture\":{\"name\":\"Tokyo\"},\"cityCode\":\"13101\",\"prefectureCode\":\"13\",\"year\":2023,"
      "\"items\":[{\"format\":\"CESIUM3DTILES\",\"lod\":0,\"url\":\"https://t/lod0\"},{\"format\":\"CESIUM3DTILES\",\"lod\":2,\"url\":\"https://t/lod2\"},"
                 "{\"format\":\"MVT\",\"lod\":1,\"url\":\"https://t/mvt\"}]},"
     "{\"id\":\"d2\",\"items\":[{\"format\":\"CESIUM3DTILES\",\"lod\":3,\"url\":\"\"}]},"
     "{\"id\":\"d3\",\"items\":[{\"format\":\"MVT\",\"lod\":1,\"url\":\"https://t/only-mvt\"}]},"
     "{\"id\":\"d4\"},"
     "{\"id\":\"d5\",\"items\":[{\"format\":\"CESIUM3DTILES\",\"lod\":4,\"url\":\"https://t/lod4\"}]}]", 3600);
  int stt = 0;
  r = pj(geoproxy_plateau_tilesets(2, &stt));
  cJSON *ts = cJSON_GetObjectItem(r, "tilesets");
  ok(stt == 200 && cJSON_GetArraySize(ts) == 2 && cJSON_GetObjectItem(r, "count")->valueint == 2,
     "datasets with no usable tileset url / no 3D-tiles item / no items array are skipped, not invented");
  ok(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(ts, 0), "tilesetUrl")->valuestring, "https://t/lod2"), "preferred LOD 2 chosen");
  ok(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(ts, 0), "city")->valuestring, "Chiyoda"), "city name carried");
  ok(cJSON_IsNull(cJSON_GetObjectItem(cJSON_GetArrayItem(ts, 1), "city")), "missing city is null, not a guess");
  ok(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(ts, 1), "tilesetUrl")->valuestring, "https://t/lod4"), "no preferred/fallback LOD: any 3D-tiles item");
  ok(cJSON_GetObjectItem(r, "preferredLod")->valueint == 2, "preferredLod echoed");
  cJSON_Delete(r);
  r = pj(geoproxy_plateau_tilesets(0, &stt));
  ts = cJSON_GetObjectItem(r, "tilesets");
  ok(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(ts, 0), "tilesetUrl")->valuestring, "https://t/lod0"), "preferred LOD 0 chosen");
  cJSON_Delete(r);
  r = pj(geoproxy_plateau_tilesets(9, &stt));
  ts = cJSON_GetObjectItem(r, "tilesets");
  ok(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(ts, 0), "tilesetUrl")->valuestring, "https://t/lod2"),
     "unavailable LOD falls back in the documented order (1, then 2)");
  cJSON_Delete(r);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
