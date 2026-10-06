/* collectors/pivot/table/hp3b31_survnet.c — batch 31: survnet — internet-wide sensing and telemetry.
 *
 * The internet is continuously scanned, measured and mapped by parties who
 * publish the result. Shodan's InternetDB answers what services an address
 * exposes without a key. Certificate transparency logs record every TLS
 * certificate ever issued for a domain, including the internal hostnames
 * organisations accidentally publish in them. RIPE Atlas publishes the
 * geolocated inventory of its measurement probes. OONI publishes, per country
 * and per network, which sites were blocked and how. Onionoo publishes the
 * complete Tor relay directory with contact strings and operating hosts. WiGLE
 * and OpenCellID publish the wireless landscape as coordinates.
 *
 * Nothing here scans anything itself — every row reads a public measurement
 * someone else already published, and a missing credential is an honest empty.
 *
 * PROVENANCE AND VERIFICATION STATUS — read before trusting a row here.
 *
 * Hand-authored in C, not scaffolded by tools/gen_hp_batch.py. This file is
 * the maintained copy. Since 2026-10-05 a manifest RECONSTRUCTED from it exists
 * at docs/candidate-sources-batch31.<beat>.txt, so the manifest-driven gates can
 * finally be pointed at batch 31. It is a record, not a source: never
 * regenerate this file from it. Its fidelity was proven rather than assumed —
 * gen_hp_batch.py run on the manifest reproduces every field of every row here
 * exactly, and a grep of field assignments agrees on both sides.
 *
 * These rows are NOT proof-of-life verified. No row was fetched over the wire,
 * so rules 4, 4b and 4d (fetching is not emitting; emitting is not storing;
 * answering is not answering THE QUESTION) are unmeasured. What HAS been
 * checked is offline and structural: clean build, zero audit-sources findings,
 * no duplicate id or endpoint, every row reachable (rule 3), and every paged
 * endpoint declares its walk (audit_batch_pagination, first run 2026-10-05).
 *
 * With network, run the MANIFEST-driven tools:
 *     python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter
 *     python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch31.*.txt \
 *             --bin ./bin/japanosint --jobs 6
 * NOT `audit_registry_emit.py --match hp3b31`, which this header used to
 * suggest: --match is a regex on the source ID, these ids carry no batch
 * prefix, so it selects zero rows and reports nothing wrong.
 * Rows with key_env need their key in the environment, and rows with
 * post_body are POSTs that probe_hp_batch does not send — judge those two
 * classes with audit_batch_emit. Retire whatever comes back EMITS_NOTHING,
 * DROPS_EVERYTHING, COLLISION or FILTER_IGNORED. The engine cannot fabricate:
 * an endpoint that moved yields an honest empty, never an invented record.
 */
#include "lib/hpengine.h"

static const hp_source HP3B31_SURVNET[] = {
  /* ── Host exposure ────────────────────────────────────────────────────── */
  { .id = "ONYPHE_HOST_SUMMARY", .name = "ONYPHE — host intelligence summary",
    .name_ja = "ONYPHE ホスト情報", .category = "surveillance",
    .portal = "https://www.onyphe.io", .record_type = "host-exposure",
    .tags = "\"scanning\",\"exposure\"", .want = HP_IP,
    .key_env = "ONYPHE_API_KEY", .free_tier = 1,
    .url = "https://www.onyphe.io/api/v2/summary/ip/{q}",
    .headers = { "Authorization: apikey {key}", NULL },
    .array_path = "results", .title_keys = "app.http.title,protocol",
    .id_keys = "ip", .date_keys = "@timestamp",
    .page_param = "page", .page_max = 40,
    .description = "ONYPHE's combined view of an address — scanned services, "
      "passive DNS, threat sightings, leaked credentials seen on it, "
      "geolocation and the organisation and ASN, drawn from its own scan and "
      "collection infrastructure" },

  { .id = "NETLAS_HOST_RESPONSES", .name = "Netlas — host & response search",
    .name_ja = "Netlas ホスト検索", .category = "surveillance",
    .portal = "https://app.netlas.io", .record_type = "host-exposure",
    .tags = "\"scanning\",\"exposure\"", .key_env = "NETLAS_API_KEY",
    .free_tier = 1,
    .url = "https://app.netlas.io/api/responses/?q={q}&start=0",
    .headers = { "X-API-Key: {key}", NULL },
    .array_path = "items", .title_keys = "data.http.title,data.host",
    .id_keys = "data.ip", .date_keys = "data.last_updated",
    .page_param = "start", .page_size = 20, .page_start = 0, .page_max = 40,
    .description = "Netlas's scan corpus queried by arbitrary expression — "
      "certificates, HTTP bodies and headers, banners and DNS records, which "
      "makes it useful for pivoting on a favicon hash, a tracking ID or an "
      "organisation string rather than an address" },

  { .id = "CRIMINALIP_ASSET_REPORT", .name = "Criminal IP — asset exposure report",
    .name_ja = "Criminal IP 資産露出", .category = "surveillance",
    .portal = "https://api.criminalip.io", .record_type = "host-exposure",
    .tags = "\"scanning\",\"exposure\"", .want = HP_IP,
    .key_env = "CRIMINALIP_API_KEY", .free_tier = 0,
    .url = "https://api.criminalip.io/v1/asset/ip/report?ip={q}&full=true",
    .headers = { "x-api-key: {key}", NULL },
    .title_keys = "ip,issues", .id_keys = "ip",
    .description = "A scan-derived report on an address requested in full "
      "form — open ports with banners, detected vulnerabilities, whether the "
      "address is a VPN, proxy, Tor node or hosting provider, and the abuse "
      "record attached to it" },

  { .id = "LEAKIX_HOST_RECORD", .name = "LeakIX — exposed service & leak index",
    .name_ja = "LeakIX 公開サービス索引", .category = "surveillance",
    .portal = "https://leakix.net", .record_type = "host-exposure",
    .tags = "\"scanning\",\"exposure\",\"leak\"", .key_env = "LEAKIX_API_KEY",
    .free_tier = 1,
    .url = "https://leakix.net/search?scope=leak&q={q}",
    .headers = { "api-key: {key}", "Accept: application/json", NULL },
    .title_keys = "event_source,host", .id_keys = "ip",
    .date_keys = "time",
    .description = "An index of exposed databases, misconfigured storage and "
      "open services with the plugin that identified each finding — the "
      "service, the software version, the host and what was reachable without "
      "authentication" },

  /* ── Certificate transparency and naming ──────────────────────────────── */
  { .id = "CRTSH_CERTIFICATES", .name = "crt.sh — certificate transparency search",
    .name_ja = "crt.sh 証明書透明性検索", .category = "surveillance",
    .portal = "https://crt.sh", .record_type = "tls-certificate",
    .tags = "\"certificates\",\"ct-log\",\"attack-surface\"",
    .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://crt.sh/?q=%25.{qh}&output=json",
    .title_keys = "name_value,issuer_name", .id_keys = "id",
    .date_keys = "not_before",
    .description = "Every certificate ever logged for a domain and its "
      "subdomains — the issuer, validity window, serial and the complete "
      "subject alternative name list. Certificates routinely disclose internal "
      "hostnames, staging environments and acquisitions before anything else "
      "does" },

  { .id = "HACKERTARGET_HOSTSEARCH", .name = "HackerTarget — subdomain & reverse DNS lookup",
    .name_ja = "サブドメイン/逆引き検索", .category = "surveillance",
    .portal = "https://api.hackertarget.com", .record_type = "dns-record",
    .tags = "\"dns\",\"attack-surface\"", .want = HP_DOMAIN, .free_tier = 1,
    .type = "dataset", .mode = HP_CSV, .csv_no_header = 1,
    .url = "https://api.hackertarget.com/hostsearch/?q={qh}",
    .title_keys = "col0", .id_keys = "col0",
    .description = "Hostnames observed under a domain with the address each "
      "resolves to, plus the reverse-DNS view of an address range. A quick "
      "unauthenticated first pass over an organisation's naming before "
      "spending a paid lookup" },

  { .id = "TOR_ONIONOO_RELAYS", .name = "Onionoo — Tor relay & bridge directory",
    .name_ja = "Tor リレー一覧", .category = "surveillance",
    .portal = "https://onionoo.torproject.org", .record_type = "tor-relay",
    .tags = "\"tor\",\"anonymity\",\"network\"", .free_tier = 1,
    .url = "https://onionoo.torproject.org/details?search={q}&limit=1000",
    .array_path = "relays", .title_keys = "nickname,as_name",
    .id_keys = "fingerprint", .date_keys = "first_seen",
    .lat_key = "latitude", .lon_key = "longitude",
    /* Onionoo returns at most `limit` relays per response and supports offset;
     * without a walk, a search matching more than 1000 relays stopped silently. */
    .page_param = "offset", .page_size = 1000, .page_max = 20,
    .description = "The complete Tor relay directory — fingerprint, nickname, "
      "contact string, exit policy, bandwidth, flags, hosting AS and country. "
      "Operator contact strings and shared hosting reveal which relays belong "
      "to the same operator" },

  /* ── Measurement, outages and censorship ──────────────────────────────── */
  { .id = "RIPE_ATLAS_PROBES", .name = "RIPE Atlas — measurement probe inventory",
    .name_ja = "RIPE Atlas 計測プローブ", .category = "surveillance",
    .portal = "https://atlas.ripe.net", .record_type = "measurement-probe",
    .tags = "\"network\",\"measurement\",\"infrastructure\"", .free_tier = 1,
    .url = "https://atlas.ripe.net/api/v2/probes/?search={q}&page_size=500",
    .array_path = "results", .title_keys = "description,asn_v4",
    .id_keys = "id", .date_keys = "first_connected",
    .lat_key = "geometry.coordinates", .lon_key = "geometry.coordinates",
    .next_path = "next", .page_max = 40,
    .description = "Every RIPE Atlas probe — its ASN, prefix, country, "
      "approximate coordinates, connection history and the tags the host "
      "applied. A geolocated inventory of measurement vantage points inside "
      "specific networks" },

  { .id = "RIPE_ATLAS_MEASUREMENTS", .name = "RIPE Atlas — public measurement catalogue",
    .name_ja = "RIPE Atlas 計測一覧", .category = "surveillance",
    .portal = "https://atlas.ripe.net", .record_type = "network-measurement",
    .tags = "\"network\",\"measurement\"", .free_tier = 1,
    .url = "https://atlas.ripe.net/api/v2/measurements/?search={q}&page_size=500",
    .array_path = "results", .title_keys = "description,type",
    .id_keys = "id", .date_keys = "start_time",
    .next_path = "next", .page_max = 40,
    .description = "Public measurements others have already run against a "
      "target — traceroutes, DNS lookups, TLS handshakes and pings, with their "
      "results retained. Frequently the target has already been measured from "
      "hundreds of vantage points" },

  { .id = "OONI_MEASUREMENTS", .name = "OONI — network interference measurements",
    .name_ja = "OONI 通信妨害計測", .category = "surveillance",
    .portal = "https://api.ooni.io", .record_type = "censorship-measurement",
    .tags = "\"censorship\",\"measurement\",\"network\"", .free_tier = 1,
    .url = "https://api.ooni.io/api/v1/measurements?domain={qh}&limit=1000",
    .array_path = "results", .title_keys = "probe_cc,test_name",
    .id_keys = "measurement_uid", .date_keys = "measurement_start_time",
    .next_path = "metadata.next_url", .page_max = 40,
    .description = "Measurements of whether a domain is blocked, from real "
      "volunteers' networks — the country, the ASN, the test, the anomaly and "
      "confirmed-blocking flags and the raw evidence. Direct observation of "
      "state filtering, per network" },

  { .id = "OONI_COUNTRY_AGGREGATION", .name = "OONI — blocking aggregation by country & network",
    .name_ja = "OONI 国別遮断集計", .category = "surveillance",
    .portal = "https://api.ooni.io", .record_type = "censorship-aggregate",
    .tags = "\"censorship\",\"measurement\"", .free_tier = 1,
    .url = "https://api.ooni.io/api/v1/aggregation?domain={qh}&axis_x=measurement_start_day"
      "&axis_y=probe_cc",
    .array_path = "result", .title_keys = "probe_cc,test_name",
    .id_keys = "measurement_start_day+probe_cc+test_name", .date_keys = "measurement_start_day",
    /* id_keys composite (+ composes, , chooses): the aggregation is per day x country x test; `probe_cc` alone kept one row
     * per country and discarded the timeline that is the point of the row. */
    .description = "The same corpus aggregated over time and geography — how "
      "many measurements of a domain were anomalous, failed or confirmed "
      "blocked, per country and per day. Turns individual measurements into a "
      "blocking timeline" },

  { .id = "GRIP_BGP_HIJACKS", .name = "GRIP — BGP hijack & routing anomaly events",
    .name_ja = "BGPハイジャック検知", .category = "surveillance",
    .portal = "https://grip.inetintel.cc.gatech.edu", .record_type = "bgp-event",
    .tags = "\"bgp\",\"routing\",\"security\"", .free_tier = 1,
    .url = "https://api.grip.inetintel.cc.gatech.edu/json/events?length=1000",
    .array_path = "data", .filter_query = 1,
    .title_keys = "event_type,summary.ases", .id_keys = "id",
    .date_keys = "view_ts",
    .interval = 900,
    .description = "Detected prefix hijacks, origin changes, route leaks and "
      "sub-moas events — the victim and attacker ASNs, the prefixes involved "
      "and the inference scores. Routing hijacks precede both traffic "
      "interception and address-space theft" },

  /* ── Wireless and physical-layer mapping ──────────────────────────────── */
  { .id = "WIGLE_NETWORK_SEARCH", .name = "WiGLE — wireless network geolocation database",
    .name_ja = "WiGLE 無線LAN位置データベース", .category = "surveillance",
    .portal = "https://api.wigle.net", .record_type = "wireless-network",
    .tags = "\"wireless\",\"geolocation\",\"surveillance\"",
    .key_env = "WIGLE_API_TOKEN", .free_tier = 1,
    .url = "https://api.wigle.net/api/v2/network/search?ssid={q}&resultsPerPage=100",
    .headers = { "Authorization: Basic {key}", NULL },
    .array_path = "results", .title_keys = "ssid,netid", .id_keys = "netid",
    .date_keys = "lastupdt",
    .lat_key = "trilat", .lon_key = "trilong",
    .next_path = "searchAfter", .page_max = 40,
    .description = "Wardriven wireless networks worldwide — BSSID, SSID, "
      "encryption, channel, the manufacturer prefix and the trilaterated "
      "coordinates with first and last observation. An SSID is often a company "
      "or household name attached to a precise location" },

  { .id = "OPENCELLID_TOWERS", .name = "OpenCelliD — open cell tower location database",
    .name_ja = "OpenCelliD 基地局位置", .category = "surveillance",
    .portal = "https://opencellid.org", .record_type = "cell-tower",
    .tags = "\"cellular\",\"geolocation\",\"surveillance\"",
    .key_env = "OPENCELLID_API_KEY", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://opencellid.org/cell/getInArea?key={key}&BBOX=-180,-90,180,90"
      "&mcc={qd}&format=json&limit=1000",
    .array_path = "cells", .title_keys = "radio,mcc", .id_keys = "cellid",
    .lat_key = "lat", .lon_key = "lon",
    .page_param = "offset", .page_size = 1000, .page_start = 0, .page_max = 30,
    .description = "Crowdsourced cell tower positions — MCC, MNC, LAC, cell "
      "ID, radio technology, estimated coordinates and range. Queried across "
      "the whole global bounding box for a country code so the operator's "
      "entire observed footprint is returned" },

  /* ── Malicious infrastructure feeds ───────────────────────────────────── */
  { .id = "CLOUDFLARE_RADAR_TRAFFIC", .name = "Cloudflare Radar — internet traffic & attack telemetry",
    .name_ja = "Cloudflare Radar 通信計測", .category = "surveillance",
    .portal = "https://api.cloudflare.com", .record_type = "internet-telemetry",
    .tags = "\"network\",\"traffic\",\"measurement\"",
    .key_env = "CLOUDFLARE_API_TOKEN", .free_tier = 1,
    .url = "https://api.cloudflare.com/client/v4/radar/ranking/top?limit=1000"
      "&format=json",
    .headers = { "Authorization: Bearer {key}", NULL },
    .array_path = "result.top_0", .filter_query = 1,
    .title_keys = "domain,rank", .id_keys = "domain",
    .interval = 3600,
    /* Bounded by design, and the bound is the request: ranking/top is a top-N
     * endpoint with no offset, so limit=1000 asks for the top 1000 rather than
     * truncating a longer answer. The full ranking is a separate dataset. */
    .description = "Cloudflare's public measurement layer — domain popularity "
      "ranking, traffic anomalies by country, attack layer distribution, BGP "
      "and routing observations and the outage annotations Cloudflare "
      "publishes when a network disappears" },
};

HP_REGISTER_TABLE(HP3B31_SURVNET)
