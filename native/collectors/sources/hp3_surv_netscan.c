/* collectors/sources/hp3_surv_netscan.c — internet-wide sensing and telemetry.
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
 * someone else already published, and a missing credential is an honest empty. */
#include "../../lib/hpengine.h"

static const hp_source HP3_SURV_NETSCAN[] = {
  /* ── Host exposure ────────────────────────────────────────────────────── */
  { .id = "SHODAN_INTERNETDB", .name = "Shodan InternetDB — free host exposure summary",
    .name_ja = "Shodan InternetDB ホスト露出", .category = "surveillance",
    .portal = "https://internetdb.shodan.io", .record_type = "host-exposure",
    .tags = "\"scanning\",\"exposure\",\"surveillance\"", .want = HP_IP,
    .free_tier = 1,
    .url = "https://internetdb.shodan.io/{q}",
    .title_keys = "ip,hostnames", .id_keys = "ip",
    .description = "Shodan's unauthenticated exposure endpoint — every open "
      "port, the detected CPEs, the hostnames resolving to the address, the "
      "tags and the CVEs Shodan associates with the observed software. No key, "
      "no rate limit worth the name, one address per call" },

  { .id = "GREYNOISE_COMMUNITY", .name = "GreyNoise Community — internet scanner classification",
    .name_ja = "GreyNoise スキャナ分類", .category = "surveillance",
    .portal = "https://api.greynoise.io", .record_type = "scanner-classification",
    .tags = "\"scanning\",\"threat\",\"noise\"", .want = HP_IP, .free_tier = 1,
    .url = "https://api.greynoise.io/v3/community/{q}",
    .title_keys = "name,classification", .id_keys = "ip",
    .date_keys = "last_seen",
    .description = "Whether an address is mass-scanning the internet, and if "
      "so whose scanner it is — the actor name, benign or malicious "
      "classification and the last-seen date. The fastest way to discount "
      "background noise in a log before investigating it" },

  { .id = "ONYPHE_HOST_SUMMARY", .name = "ONYPHE — host intelligence summary",
    .name_ja = "ONYPHE ホスト情報", .category = "surveillance",
    .portal = "https://www.onyphe.io", .record_type = "host-exposure",
    .tags = "\"scanning\",\"exposure\"", .want = HP_IP,
    .key_env = "ONYPHE_API_KEY", .free_tier = 1,
    .url = "https://www.onyphe.io/api/v2/summary/ip/{q}",
    .headers = { "Authorization: apikey {key}", NULL },
    .array_path = "results", .title_keys = "app.http.title,protocol",
    .id_keys = "ip", .date_keys = "@timestamp",
    .page_param = "page", .page_size = 10, .page_max = 40,
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
    .description = "The complete Tor relay directory — fingerprint, nickname, "
      "contact string, exit policy, bandwidth, flags, hosting AS and country. "
      "Operator contact strings and shared hosting reveal which relays belong "
      "to the same operator" },

  { .id = "TOR_EXIT_NODE_LIST", .name = "Tor Project — bulk exit node address list",
    .name_ja = "Tor 出口ノード一覧", .category = "surveillance",
    .portal = "https://check.torproject.org", .record_type = "tor-exit",
    .tags = "\"tor\",\"anonymity\",\"network\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV, .csv_no_header = 1,
    .url = "https://check.torproject.org/torbulkexitlist",
    .filter_query = 1, .title_keys = "col0", .id_keys = "col0",
    .description = "The authoritative list of addresses currently able to exit "
      "Tor traffic — the reference set for deciding whether a connection in a "
      "log was anonymised, published by the Tor Project itself" },

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
    .id_keys = "probe_cc", .date_keys = "measurement_start_day",
    .description = "The same corpus aggregated over time and geography — how "
      "many measurements of a domain were anomalous, failed or confirmed "
      "blocked, per country and per day. Turns individual measurements into a "
      "blocking timeline" },

  { .id = "IODA_OUTAGE_ALERTS", .name = "IODA — internet outage detection & analysis",
    .name_ja = "IODA インターネット障害検知", .category = "surveillance",
    .portal = "https://ioda.inetintel.cc.gatech.edu", .record_type = "network-outage",
    .tags = "\"network\",\"outage\",\"measurement\"", .free_tier = 1,
    .url = "https://api.ioda.inetintel.cc.gatech.edu/v2/outages/alerts?limit=1000",
    .array_path = "data", .filter_query = 1,
    .title_keys = "entity.name,datasource", .id_keys = "entity.code",
    .date_keys = "time",
    .description = "Detected internet outages by country, region and ASN, "
      "corroborated across BGP withdrawals, active probing and darknet traffic. "
      "Deliberate national shutdowns and infrastructure damage both show here "
      "before either is announced" },

  { .id = "GRIP_BGP_HIJACKS", .name = "GRIP — BGP hijack & routing anomaly events",
    .name_ja = "BGPハイジャック検知", .category = "surveillance",
    .portal = "https://grip.inetintel.cc.gatech.edu", .record_type = "bgp-event",
    .tags = "\"bgp\",\"routing\",\"security\"", .free_tier = 1,
    .url = "https://api.grip.inetintel.cc.gatech.edu/json/events?length=1000",
    .array_path = "data", .filter_query = 1,
    .title_keys = "event_type,summary.ases", .id_keys = "id",
    .date_keys = "view_ts",
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
  { .id = "URLHAUS_HOST_LOOKUP", .name = "URLhaus — malware distribution host record",
    .name_ja = "URLhaus マルウェア配布ホスト", .category = "surveillance",
    .portal = "https://urlhaus-api.abuse.ch", .record_type = "malicious-host",
    .tags = "\"threat\",\"malware\",\"infrastructure\"", .free_tier = 1,
    .url = "https://urlhaus-api.abuse.ch/v1/host/",
    .post_body = "host={Q}", .content_type = "application/x-www-form-urlencoded",
    .array_path = "urls", .title_keys = "url,threat", .id_keys = "id",
    .date_keys = "date_added",
    .description = "Every malware distribution URL abuse.ch has recorded on a "
      "host — the payload family, the reporter, the status and the first and "
      "last seen dates, plus the blacklist status of the host itself" },

  { .id = "THREATFOX_IOC_SEARCH", .name = "ThreatFox — indicator of compromise search",
    .name_ja = "ThreatFox 侵害指標検索", .category = "surveillance",
    .portal = "https://threatfox-api.abuse.ch", .record_type = "threat-indicator",
    .tags = "\"threat\",\"ioc\",\"infrastructure\"", .free_tier = 1,
    .url = "https://threatfox-api.abuse.ch/api/v1/",
    .post_body = "{\"query\":\"search_ioc\",\"search_term\":\"{Q}\"}",
    .content_type = "application/json",
    .array_path = "data", .title_keys = "ioc,malware_printable",
    .id_keys = "id", .date_keys = "first_seen",
    .description = "Indicators shared by the abuse.ch community — the "
      "indicator, the malware family and its aliases, the confidence level, "
      "the reporter and the tags. Free, unauthenticated and updated "
      "continuously" },

  { .id = "FEODO_C2_TRACKER", .name = "Feodo Tracker — botnet command & control servers",
    .name_ja = "Feodo C2サーバ追跡", .category = "surveillance",
    .portal = "https://feodotracker.abuse.ch", .record_type = "c2-server",
    .tags = "\"threat\",\"botnet\",\"infrastructure\"", .free_tier = 1,
    .url = "https://feodotracker.abuse.ch/downloads/ipblocklist.json",
    .filter_query = 1, .title_keys = "malware,hostname", .id_keys = "ip_address",
    .date_keys = "first_seen",
    .description = "Active botnet command-and-control servers — the address "
      "and port, the malware family, the hosting AS and country, and the "
      "first-seen and last-online dates for each" },

  { .id = "SSL_BLACKLIST_CERTS", .name = "SSL Blacklist — malicious TLS certificates",
    .name_ja = "悪性TLS証明書ブラックリスト", .category = "surveillance",
    .portal = "https://sslbl.abuse.ch", .record_type = "malicious-certificate",
    .tags = "\"threat\",\"tls\",\"infrastructure\"", .free_tier = 1,
    .type = "dataset", .mode = HP_CSV,
    .url = "https://sslbl.abuse.ch/blacklist/sslblacklist.csv",
    .filter_query = 1, .title_keys = "Listingreason,SHA1",
    .id_keys = "SHA1", .date_keys = "Listingdate",
    .description = "TLS certificate fingerprints associated with botnet C2 and "
      "malware traffic, with the reason for listing. A JA3 or certificate hash "
      "match survives an address change, so this outlives the IP blocklists" },

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
    .description = "Cloudflare's public measurement layer — domain popularity "
      "ranking, traffic anomalies by country, attack layer distribution, BGP "
      "and routing observations and the outage annotations Cloudflare "
      "publishes when a network disappears" },
};

HP_REGISTER_TABLE(HP3_SURV_NETSCAN)
