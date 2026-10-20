/* core/camera_privacy.h — which camera imagery may be INDEXED.
 *
 * Capturing a still is an explicit, audited, per-camera operator opt-in
 * (camera_stills.h). Indexing it — embedding frames into a searchable vector
 * space (scripts/vision_embed_server.py), OCR into full-text search — is a
 * different act: it makes every frame findable by anyone who can search,
 * across every camera at once. That is only acceptable for cameras that exist
 * to show PUBLIC INFRASTRUCTURE to the public: roads, rivers, ports, volcanoes,
 * weather, and the government bodies that run them.
 *
 * So the rule is an ALLOWLIST of channels, with a hard DENYLIST in front of it:
 *
 *   denied, always (JO_CAMERA_INDEX_CHANNELS cannot re-enable these):
 *     insecam, shodan, manual/ip seeds, anything named "surveillance" or
 *     "citiwatch" — cameras found by scanning for open devices, privately
 *     owned feeds whose owners never chose to publish, and policing networks.
 *   allowed by default:
 *     road / traffic / highway / freeway / expressway / motorway, river /
 *     kasen, port / harbor, volcano, weather / weathercam / rwis, and the
 *     operators mlit, jma, nzta, digitraffic, dot, 511, gov / government.
 *   everything else (tourism webcams, broadcasters, aggregators, unknown
 *     hp rows): NOT indexed. Unknown is no.
 *
 * Matching is by WORD: the channel and the source id are lowercased and split
 * on every non-alphanumeric character, and a rule matches a whole word ("ip"
 * matches manual_ip_seed, never "ship"). Either the discovery channel
 * (camera_store.c's discovery_channel, e.g. "mlit_river", "jma_volcano",
 * "insecam_scrape") or the record's source id
 * ("digitraffic-weathercam-stations", "511-alberta-cameras") may carry it; a
 * deny word in EITHER wins over an allow word in either.
 *
 * JO_CAMERA_INDEX_CHANNELS="road,river,weather" REPLACES the default allow
 * words (comma-separated, same word matching); "none" allows nothing. The
 * deny words always apply.
 *
 * NO FACE OR PERSON FEATURES. Nothing in this tree detects, crops, embeds,
 * matches or counts faces or people in camera imagery, and nothing may be
 * added that does: pHash is a whole-frame brightness fingerprint for "did the
 * scene change", OCR reads signage, and the vision sidecar embeds whole frames
 * for scene-level search ("flooded road", "snow"). A person-level feature
 * over public cameras is surveillance of individuals, which this product does
 * not do regardless of channel.
 */
#ifndef JO_CAMERA_PRIVACY_H
#define JO_CAMERA_PRIVACY_H

/* 1 when imagery from a camera with this discovery channel and/or source id
 * may be indexed, 0 otherwise. Either argument may be NULL; both NULL → 0. */
int camera_channel_indexable(const char *channel, const char *source_id);

/* The same decision for an intel_items row: reads discovery_channel and every
 * entry of discovery_channels from `properties_json` (camera_store.c's shape)
 * — a deny word on ANY channel the camera was discovered through denies it. */
int camera_record_indexable(const char *properties_json, const char *source_id);

#endif
