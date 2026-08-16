/* collectors/infrastructure/sources/wifi_hotspots_freespot.c
 * FREESPOT public Wi-Fi directory.
 *
 * The old port scraped <tr>/<td> out of https://www.freespot.com/users/map_e.html.
 * That page is no longer a venue table — it is a prefecture picker whose only
 * <tr>/<td> hold the page chrome, so the scrape emitted exactly one row whose
 * title was the copyright footer. The real directory lives in the per-prefecture
 * KSGMap XML the picker links to:
 *
 *   .../users/map_e.html   ->  href="../gmap/freespot-e.html?...&import=13tokyo-e.xml"
 *   https://www.freespot.com/gmap/xml/13tokyo-e.xml
 *     <item id="FS430" url-id="430" category="area131016" name="Sofmap Akihabara 1 go-ten"
 *           lng="139.77104" lat="35.69889" address="Chiyoda-Ku,Tokyo-to"
 *           phone="03-3253-9255" Price="1" Power="3" IEEE="b" URL="http://..." />
 *
 * so every hotspot carries real coordinates and can pin. We follow the picker
 * (never a hardcoded prefecture list) and emit one geo intel item per <item>. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDEX_URL "https://www.freespot.com/users/map_e.html"
#define XML_BASE  "https://www.freespot.com/gmap/xml/"
#define MAX_PREFS 128

/* Pull attr="value" out of a single tag slice [s,end). 1 on hit, 0 otherwise. */
static int tag_attr(const char *s, const char *end, const char *attr,
                    char *out, size_t n) {
  size_t al = strlen(attr);
  out[0] = 0;
  for (const char *p = s; p + al + 2 < end; p++) {
    if (strncasecmp(p, attr, al) != 0) continue;
    if (p > s && (isalnum((unsigned char)p[-1]) || p[-1] == '-' || p[-1] == '_'))
      continue;                                  /* "url-id" must not match "id" */
    const char *q = p + al;
    while (q < end && (*q == ' ' || *q == '\t')) q++;
    if (q >= end || *q != '=') continue;
    q++;
    while (q < end && (*q == ' ' || *q == '\t')) q++;
    if (q >= end || (*q != '"' && *q != '\'')) continue;
    char quote = *q++;
    const char *e = memchr(q, quote, (size_t)(end - q));
    if (!e) return 0;
    size_t len = (size_t)(e - q);
    if (len >= n) len = n - 1;
    memcpy(out, q, len);
    out[len] = 0;
    return 1;
  }
  return 0;
}

/* "13tokyo-e.xml" -> "tokyo" (drop leading digits and the -e/-j suffix). */
static void pref_of(const char *xmlname, char *out, size_t n) {
  const char *p = xmlname;
  while (*p && isdigit((unsigned char)*p)) p++;
  size_t i = 0;
  while (*p && *p != '-' && *p != '.' && i + 1 < n) out[i++] = *p++;
  out[i] = 0;
  if (!out[0]) snprintf(out, n, "unknown");
}

/* One prefecture XML -> emitted count. */
static int do_pref(const source_ctx *ctx, intel_sink *sink, const char *xmlname) {
  char url[512];
  snprintf(url, sizeof url, "%s%s", XML_BASE, xmlname);
  char *xml = feed_get_text(ctx->http, url, 20000);
  if (!xml) return 0;

  char pref[64];
  pref_of(xmlname, pref, sizeof pref);

  int n = 0;
  const char *cur = xml;
  for (;;) {
    const char *open = strcasestr(cur, "<item");
    if (!open) break;
    char d = open[5];
    if (d != ' ' && d != '\t' && d != '\n' && d != '\r') { cur = open + 5; continue; }
    const char *gt = strchr(open, '>');
    if (!gt) break;
    cur = gt + 1;

    char id[64], name[512], lngs[64], lats[64], addr[512], phone[64];
    char price[32], power[32], ieee[32], vurl[512], urlid[64], cat[64];
    tag_attr(open, gt, "id", id, sizeof id);
    tag_attr(open, gt, "url-id", urlid, sizeof urlid);
    tag_attr(open, gt, "category", cat, sizeof cat);
    tag_attr(open, gt, "name", name, sizeof name);
    tag_attr(open, gt, "lng", lngs, sizeof lngs);
    tag_attr(open, gt, "lat", lats, sizeof lats);
    tag_attr(open, gt, "address", addr, sizeof addr);
    tag_attr(open, gt, "phone", phone, sizeof phone);
    tag_attr(open, gt, "Price", price, sizeof price);
    tag_attr(open, gt, "Power", power, sizeof power);
    tag_attr(open, gt, "IEEE", ieee, sizeof ieee);
    tag_attr(open, gt, "URL", vurl, sizeof vurl);

    if (!name[0] && !id[0]) continue;
    int has_geo = (lats[0] && lngs[0]);
    double lat = has_geo ? strtod(lats, NULL) : 0;
    double lon = has_geo ? strtod(lngs, NULL) : 0;
    if (has_geo && lat == 0 && lon == 0) has_geo = 0;

    cJSON *pj = cJSON_CreateObject();
    if (id[0])    cJSON_AddStringToObject(pj, "freespot_id", id);
    if (urlid[0]) cJSON_AddStringToObject(pj, "url_id", urlid);
    if (name[0])  cJSON_AddStringToObject(pj, "venue", name);
    if (addr[0])  cJSON_AddStringToObject(pj, "address", addr);
    if (phone[0]) cJSON_AddStringToObject(pj, "phone", phone);
    if (cat[0])   cJSON_AddStringToObject(pj, "area_code", cat);
    cJSON_AddStringToObject(pj, "prefecture", pref);
    if (ieee[0])  cJSON_AddStringToObject(pj, "ieee_80211", ieee);
    if (price[0]) cJSON_AddStringToObject(pj, "price_class", price);
    if (power[0]) cJSON_AddStringToObject(pj, "power_outlet_class", power);
    if (vurl[0])  cJSON_AddStringToObject(pj, "venue_url", vurl);
    cJSON_AddStringToObject(pj, "source_xml", xmlname);
    char *pjs = cJSON_PrintUnformatted(pj);

    char tags[256];
    snprintf(tags, sizeof tags,
             "[\"free-wifi\",\"freespot\",\"wifi-hotspot\",\"pref:%s\"]", pref);

    char link[256];
    if (urlid[0])
      snprintf(link, sizeof link,
               "https://www.freespot.com/map/tenpo.php?n=%s", urlid);
    else
      snprintf(link, sizeof link, "%s", INDEX_URL);

    char body[1280];
    snprintf(body, sizeof body, "Venue: %s\nAddress: %s\nPhone: %s\n802.11: %s",
             name[0] ? name : "unknown", addr[0] ? addr : "unknown",
             phone[0] ? phone : "unknown", ieee[0] ? ieee : "unknown");

    intel_item it = {0};
    it.remote_key      = id[0] ? id : name;
    it.title           = name[0] ? name : id;
    it.summary         = addr[0] ? addr : NULL;
    it.body            = body;
    it.link            = link;
    it.author          = "FREESPOT";
    it.lang            = "en";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.record_type     = "wifi-hotspot";
    it.properties_json = pjs;
    it.tags_json       = tags;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pjs);
    cJSON_Delete(pj);
  }
  free(xml);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *index = feed_get_text(ctx->http, INDEX_URL, 20000);
  if (!index) {
    fprintf(stderr, "[wifi-hotspots-freespot] index unreachable\n");
    return -1;
  }

  /* Collect the distinct `import=<file>.xml` names the picker links to. */
  char *prefs[MAX_PREFS];
  int np = 0;
  for (const char *p = index; (p = strstr(p, "import=")) != NULL; ) {
    p += 7;
    const char *e = p;
    while (*e && *e != '"' && *e != '\'' && *e != '&' && *e != ' ' && *e != '>') e++;
    size_t len = (size_t)(e - p);
    if (len && len < 128) {
      char nm[128];
      memcpy(nm, p, len);
      nm[len] = 0;
      int dup = 0;
      for (int i = 0; i < np; i++) if (!strcmp(prefs[i], nm)) { dup = 1; break; }
      if (!dup && np < MAX_PREFS) prefs[np++] = strdup(nm);
    }
    p = e;
  }
  free(index);

  if (np == 0) {
    fprintf(stderr, "[wifi-hotspots-freespot] picker exposed no prefecture XMLs\n");
    return 0;                       /* honest empty, not an error */
  }

  int total = 0;
  for (int i = 0; i < np; i++) {
    total += do_pref(ctx, sink, prefs[i]);
    free(prefs[i]);
  }
  fprintf(stderr, "[wifi-hotspots-freespot] %d prefecture files, emitted %d\n",
          np, total);
  return 0;
}

static const source_def wifi_hotspots_freespot_def = {
  .id = "wifi-hotspots-freespot", .collector = "infrastructure",
  .name = "FREESPOT Directory", .name_ja = "FREESPOT 一覧",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(wifi_hotspots_freespot_def)
