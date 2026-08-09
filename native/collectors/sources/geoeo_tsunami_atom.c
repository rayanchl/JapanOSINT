/* NOAA/NWS tsunami warning centre Atom feeds — the live message stream from
 * the two US centres.
 *
 * Endpoints (keyless):
 *   https://www.tsunami.gov/events/xml/PAAQAtom.xml   NTWC Palmer AK
 *   https://www.tsunami.gov/events/xml/PHEBAtom.xml   PTWC Honolulu HI
 * Emits per <entry>: title (the message type and number), updated, summary /
 *   message text, the entry link, and the issuing centre taken from
 *   <author><name>.
 * Licence: NOAA/NWS public domain.
 *
 * parse_notes honoured:
 *  - These are Atom, not RSS: entries are <entry> with <updated>, so the RSS
 *    <item>/<pubDate> path does not apply.
 *  - The root element DECLARES the georss and geo namespaces but the messages
 *    carry no <georss:point> and no <geo:lat>. A namespace declaration is not a
 *    coordinate, so every row is emitted with has_geo=0 and a NULL geometry
 *    rather than a guessed epicentre (R2).
 *  - PHEBAtom's self link mistakenly points at PAAQAtom.xml, so the centre is
 *    identified from <author><name>, never from the self link.
 *  - A single entry holding the latest message is the normal steady state —
 *    that is a real record, and an empty feed is R3 return 0.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "geoeo_common.inc"

static int tsunami_collect(const source_ctx *ctx, intel_sink *sink,
                           const char *sid, const char *url,
                           const char *fallback_centre) {
  char *body = feed_get_text(ctx->http, url, 25000);
  if (!body) {
    fprintf(stderr, "[%s] fetch failed\n", sid);
    return -1;
  }

  /* Issuing centre: the FEED-level <author><name>, not the self link. */
  char centre[160] = {0};
  {
    char author[512];
    if (html_tag(body, "author", author, sizeof author))
      geoeo_xml_text(author, "name", centre, sizeof centre);
  }
  if (!centre[0]) snprintf(centre, sizeof centre, "%s", fallback_centre);

  int n = 0, entries = 0;
  const char *cur = body, *inner = NULL;
  int ilen = 0;
  while ((cur = html_block(cur, "entry", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    entries++;
    char *blk = geoeo_dupn(inner, (size_t)ilen);
    if (!blk) break;

    char title[512], updated[64], summary[4096], eid[256], link[512];
    int has_title = geoeo_xml_text(blk, "title", title, sizeof title);
    geoeo_xml_text(blk, "updated", updated, sizeof updated);
    geoeo_xml_text(blk, "summary", summary, sizeof summary);
    geoeo_xml_text(blk, "id", eid, sizeof eid);
    if (!html_attr(blk, "href", link, sizeof link)) link[0] = 0;
    if (!has_title) { free(blk); continue; }

    char *plain = html_strip(summary);
    if (plain) geoeo_unescape(plain);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "title", title);
    geoeo_add_str(props, "updated", updated);
    geoeo_add_str(props, "entry_id", eid);
    geoeo_add_str(props, "link", link);
    if (plain && plain[0]) cJSON_AddStringToObject(props, "message", plain);
    cJSON_AddStringToObject(props, "centre", centre);
    cJSON_AddStringToObject(props, "geo_note",
                            "feed declares georss/geo namespaces but carries "
                            "no coordinate; no position asserted");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char key[320];
    snprintf(key, sizeof key, "%s|%s", eid[0] ? eid : title,
             updated[0] ? updated : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = (plain && plain[0]) ? plain : NULL;
    it.link = link[0] ? link : NULL;
    it.author = centre;
    it.lang = "en";
    it.published_at = updated[0] ? updated : NULL;
    it.record_type = "tsunami-message";
    it.has_geo = 0;                    /* no coordinate came back — R2 */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"tsunami\",\"warning\",\"noaa\",\"nws\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(plain);
    free(blk);
  }

  /* A feed with no <entry> is only an ERROR if what came back was not a feed
   * at all. A genuinely quiet centre is an honest empty (R3 return 0). */
  int looks_like_feed = strstr(body, "<feed") != NULL ||
                        strstr(body, "<FEED") != NULL;
  free(body);
  if (!entries && !looks_like_feed) {
    fprintf(stderr, "[%s] response is not an Atom feed\n", sid);
    return -1;
  }
  fprintf(stderr, "[%s] emitted %d of %d entries (%s)\n", sid, n, entries,
          centre);
  return 0;
}

static int ntwc_run(const source_ctx *ctx, intel_sink *sink) {
  return tsunami_collect(ctx, sink, "ntwc-tsunami-atom",
                         "https://www.tsunami.gov/events/xml/PAAQAtom.xml",
                         "NWS National Tsunami Warning Center Palmer AK");
}

static int ptwc_run(const source_ctx *ctx, intel_sink *sink) {
  return tsunami_collect(ctx, sink, "ptwc-tsunami-atom",
                         "https://www.tsunami.gov/events/xml/PHEBAtom.xml",
                         "NWS Pacific Tsunami Warning Center Honolulu HI");
}

static const source_def geoeo_ntwc_def = {
  .id = "ntwc-tsunami-atom", .collector = "environment",
  .name = "US National Tsunami Warning Center Atom Feed",
  .update_interval_sec = 300, .run = ntwc_run,
  .category = "environment", .type = "api",
  .url = "https://www.tsunami.gov/events/xml/PAAQAtom.xml",
  .description = "Live tsunami messages for Alaska, Canada, the US West Coast, Puerto Rico and the Virgin Islands from the NTWC — warnings, advisories, watches and information statements as issued.",
  .license = "NOAA/NWS public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ntwc_def)

static const source_def geoeo_ptwc_def = {
  .id = "ptwc-tsunami-atom", .collector = "environment",
  .name = "Pacific Tsunami Warning Center Atom Feed",
  .update_interval_sec = 300, .run = ptwc_run,
  .category = "environment", .type = "api",
  .url = "https://www.tsunami.gov/events/xml/PHEBAtom.xml",
  .description = "PTWC Honolulu tsunami messages for the Pacific basin and Caribbean — the international warning product for far-field events outside JMA's area.",
  .license = "NOAA/NWS public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ptwc_def)
