/* collectors/sources/hp_netintel_deep.c — network-intelligence depth.
 *
 * RIPEstat is the clearest example of the penetrancy gap this batch closes: the
 * repo used exactly one of its data calls (a country resource list), while the
 * same keyless API answers routing status, announced prefixes, AS neighbours,
 * BGP update history, whois, abuse contacts, address-space hierarchy, reverse
 * DNS delegation, historical whois and blocklist membership for any resource.
 * Same for BGPView (asn → prefixes → peers → upstreams) and PeeringDB (asn →
 * IX presence → facilities): each hop narrows from "who announces this" to
 * "which building is it in".
 *
 * All RIPEstat/BGPView/PeeringDB rows are keyless. The rows that need a
 * credential say so and honest-empty without it. */
#include "../../lib/hpengine.h"

#define RIPE(call) "https://stat.ripe.net/data/" call "/data.json?resource={q}"

static const hp_source HP_NETINTEL[] = {
  /* ── RIPEstat data calls ───────────────────────────────────────────────── */
  { .id = "RIPE_AS_OVERVIEW", .name = "RIPEstat — AS overview",
    .name_ja = "RIPEstat — AS概要", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "asn-overview",
    .tags = "\"bgp\",\"asn\"", .want = HP_ASN, .free_tier = 1,
    .url = RIPE("as-overview"), .array_path = "data",
    .title_keys = "holder,resource", .id_keys = "resource",
    .description = "Registered holder, announcement status, resource type and block "
      "for an AS number — the identity of whoever operates the network" },

  { .id = "RIPE_ANNOUNCED_PREFIXES", .name = "RIPEstat — announced prefixes",
    .name_ja = "RIPEstat — 広告プレフィックス", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "bgp-prefix",
    .tags = "\"bgp\",\"asn\"", .want = HP_ASN, .free_tier = 1,
    .url = RIPE("announced-prefixes"), .array_path = "data.prefixes",
    .title_keys = "prefix", .id_keys = "prefix", .max_items = 60,
    .description = "Every IP prefix an AS currently announces, with the timelines of "
      "each announcement — the actual address space an organisation controls, as "
      "opposed to what it is allocated" },

  { .id = "RIPE_ASN_NEIGHBOURS", .name = "RIPEstat — AS neighbours",
    .name_ja = "RIPEstat — 隣接AS", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "bgp-neighbour",
    .tags = "\"bgp\",\"asn\",\"topology\"", .want = HP_ASN, .free_tier = 1,
    .url = RIPE("asn-neighbours"), .array_path = "data.neighbours",
    .title_keys = "asn", .id_keys = "asn", .max_items = 60,
    .description = "The ASes that peer with or transit for this AS, with left/right "
      "position and observation counts — the network's real business relationships, "
      "visible even when contracts are not" },

  { .id = "RIPE_ROUTING_STATUS", .name = "RIPEstat — routing status",
    .name_ja = "RIPEstat — 経路状態", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "bgp-routing-status",
    .tags = "\"bgp\"", .free_tier = 1, .url = RIPE("routing-status"),
    .array_path = "data", .title_keys = "resource", .id_keys = "resource",
    .description = "Announced/visible status of a prefix or AS right now — first-seen "
      "timestamps, visibility across RIS collectors, announced space totals and the "
      "origin ASes observed" },

  { .id = "RIPE_BGP_UPDATES", .name = "RIPEstat — BGP update history",
    .name_ja = "RIPEstat — BGP更新履歴", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "bgp-update",
    .tags = "\"bgp\",\"history\"", .free_tier = 1, .url = RIPE("bgp-updates"),
    .array_path = "data.updates", .title_keys = "type,attrs.target_prefix",
    .date_keys = "timestamp", .max_items = 60,
    .description = "Recent BGP announcements and withdrawals touching a resource — "
      "hijacks, leaks, flapping and provider changes appear here before they appear "
      "anywhere else" },

  { .id = "RIPE_PREFIX_OVERVIEW", .name = "RIPEstat — prefix overview",
    .name_ja = "RIPEstat — プレフィックス概要", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "bgp-prefix",
    .tags = "\"bgp\",\"prefix\"", .free_tier = 1, .url = RIPE("prefix-overview"),
    .array_path = "data", .title_keys = "resource,block.desc", .id_keys = "resource",
    .description = "Who announces a prefix or address, the enclosing allocation "
      "block and its description, and whether the announcement is currently visible" },

  { .id = "RIPE_WHOIS", .name = "RIPEstat — registry whois objects",
    .name_ja = "RIPEstat — レジストリWHOIS", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "rir-whois",
    .tags = "\"whois\",\"rir\"", .free_tier = 1, .url = RIPE("whois"),
    .array_path = "data.records", .max_items = 40,
    .title_keys = "0.value,1.value",
    .description = "Raw RIR registry objects for an IP, prefix or AS — inetnum, "
      "organisation, admin/tech contacts, mnt-by and the irr route objects, straight "
      "from the authoritative registry" },

  { .id = "RIPE_ABUSE_CONTACT", .name = "RIPEstat — abuse contact resolution",
    .name_ja = "RIPEstat — 不正利用連絡先", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "abuse-contact",
    .tags = "\"whois\",\"abuse\"", .free_tier = 1,
    .url = RIPE("abuse-contact-finder"), .array_path = "data",
    .title_keys = "abuse_contacts.0,authoritative_rir",
    .description = "The authoritative abuse contact for an IP, prefix or AS plus the "
      "responsible RIR — the operator email that actually answers, resolved through "
      "the hierarchy rather than guessed" },

  { .id = "RIPE_ADDRESS_HIERARCHY", .name = "RIPEstat — address space hierarchy",
    .name_ja = "RIPEstat — アドレス空間階層", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "rir-allocation",
    .tags = "\"whois\",\"rir\"", .free_tier = 1,
    .url = RIPE("address-space-hierarchy"), .array_path = "data.exact",
    .title_keys = "inetnum,netname", .max_items = 40,
    .description = "Exact, less-specific and more-specific registry allocations "
      "around a prefix — shows the upstream LIR above a customer assignment and any "
      "sub-delegations beneath it" },

  { .id = "RIPE_REVERSE_DNS", .name = "RIPEstat — reverse DNS delegation",
    .name_ja = "RIPEstat — 逆引きDNS委任", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "dns-delegation",
    .tags = "\"dns\",\"rir\"", .free_tier = 1, .url = RIPE("reverse-dns"),
    .array_path = "data.delegations", .max_items = 40,
    .title_keys = "0.value,1.value",
    .description = "Reverse-DNS delegation records for an address block — which "
      "nameservers a network operator has authority over, a strong signal of real "
      "operational control of the space" },

  { .id = "RIPE_HISTORICAL_WHOIS", .name = "RIPEstat — historical whois",
    .name_ja = "RIPEstat — WHOIS履歴", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "rir-whois-history",
    .tags = "\"whois\",\"history\"", .free_tier = 1,
    .url = RIPE("historical-whois"), .array_path = "data.objects", .max_items = 40,
    .title_keys = "type,key",
    .description = "Previous versions of registry objects for a resource — who held "
      "an AS or prefix before the current holder, and when it changed hands. The RIR "
      "equivalent of a chain of title" },

  { .id = "RIPE_BLOCKLIST", .name = "RIPEstat — blocklist observations",
    .name_ja = "RIPEstat — ブロックリスト履歴", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "blocklist-entry",
    .tags = "\"reputation\",\"abuse\"", .free_tier = 1, .url = RIPE("blocklist"),
    .array_path = "data.sources", .max_items = 40, .title_keys = "0.prefix",
    .description = "Which blocklists have carried a prefix or address, and over which "
      "time windows — a long blocklist history is the difference between a one-off "
      "incident and a bulletproof host" },

  { .id = "RIPE_DNS_CHAIN", .name = "RIPEstat — DNS chain",
    .name_ja = "RIPEstat — DNSチェーン", .category = "cyber",
    .portal = "https://stat.ripe.net", .record_type = "dns-chain",
    .tags = "\"dns\"", .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://stat.ripe.net/data/dns-chain/data.json?resource={qh}",
    .array_path = "data", .title_keys = "resource",
    .description = "Forward and reverse DNS chain for a hostname — authoritative "
      "nameservers, A/AAAA answers and the PTR records back, resolved through the "
      "delegation path rather than a single lookup" },

  /* ── BGPView: ASN → prefixes → peers → upstreams ───────────────────────── */
  { .id = "BGPVIEW_ASN_DETAIL", .name = "BGPView — ASN detail",
    .name_ja = "BGPView — AS詳細", .category = "cyber",
    .portal = "https://bgpview.io", .record_type = "asn-overview",
    .tags = "\"bgp\",\"asn\"", .want = HP_ASN, .free_tier = 1,
    .url = "https://api.bgpview.io/asn/{qd}", .array_path = "data",
    .title_keys = "name,description_short", .id_keys = "asn",
    .description = "AS record with organisation name, country, RIR, traffic ratio, "
      "traffic estimate, looking-glass URL and the abuse/technical contact emails "
      "published in whois" },

  { .id = "BGPVIEW_ASN_PREFIXES", .name = "BGPView — ASN prefixes",
    .name_ja = "BGPView — AS配下プレフィックス", .category = "cyber",
    .portal = "https://bgpview.io", .record_type = "bgp-prefix",
    .tags = "\"bgp\",\"asn\"", .want = HP_ASN, .free_tier = 1,
    .url = "https://api.bgpview.io/asn/{qd}/prefixes",
    .array_path = "data.ipv4_prefixes", .title_keys = "prefix,description",
    .id_keys = "prefix", .max_items = 60,
    .description = "IPv4 and IPv6 prefixes under an AS with per-prefix names, "
      "descriptions and the parent allocation — the inventory an attack-surface sweep "
      "starts from" },

  { .id = "BGPVIEW_ASN_PEERS", .name = "BGPView — ASN peers",
    .name_ja = "BGPView — ピアAS", .category = "cyber",
    .portal = "https://bgpview.io", .record_type = "bgp-neighbour",
    .tags = "\"bgp\",\"topology\"", .want = HP_ASN, .free_tier = 1,
    .url = "https://api.bgpview.io/asn/{qd}/peers", .array_path = "data.ipv4_peers",
    .title_keys = "name,asn", .id_keys = "asn", .max_items = 60,
    .description = "Peering relationships of an AS with each peer's name and country "
      "— maps an operator into its regional interconnection community" },

  { .id = "BGPVIEW_ASN_UPSTREAMS", .name = "BGPView — ASN upstreams",
    .name_ja = "BGPView — 上位プロバイダ", .category = "cyber",
    .portal = "https://bgpview.io", .record_type = "bgp-upstream",
    .tags = "\"bgp\",\"topology\"", .want = HP_ASN, .free_tier = 1,
    .url = "https://api.bgpview.io/asn/{qd}/upstreams",
    .array_path = "data.ipv4_upstreams", .title_keys = "name,asn", .id_keys = "asn",
    .max_items = 40,
    .description = "The transit providers an AS buys from, including the graph of "
      "upstream-of-upstream relationships — who to serve notice on when the operator "
      "itself does not answer" },

  { .id = "BGPVIEW_IP_DETAIL", .name = "BGPView — IP address detail",
    .name_ja = "BGPView — IP詳細", .category = "cyber",
    .portal = "https://bgpview.io", .record_type = "ip-detail",
    .tags = "\"bgp\",\"ip\"", .want = HP_IP, .free_tier = 1,
    .url = "https://api.bgpview.io/ip/{q}", .array_path = "data",
    .title_keys = "ip,ptr_record", .id_keys = "ip",
    .description = "Everything BGPView knows about one address — PTR record, the "
      "prefixes containing it, announcing ASes, RIR allocation and the maintainer "
      "objects behind it" },

  /* ── PeeringDB: the physical layer ─────────────────────────────────────── */
  { .id = "PEERINGDB_NET", .name = "PeeringDB — network record",
    .name_ja = "PeeringDB — ネットワーク登録", .category = "cyber",
    .portal = "https://www.peeringdb.com", .record_type = "peeringdb-net",
    .tags = "\"bgp\",\"peering\"", .want = HP_ASN, .free_tier = 1,
    .url = "https://www.peeringdb.com/api/net?asn={qd}", .array_path = "data",
    .title_keys = "name,aka", .id_keys = "asn",
    .description = "The operator's own PeeringDB record — legal name, aka names, "
      "website, IRR-as-set, policy, traffic levels and NOC contact details, all "
      "self-published by the network" },

  { .id = "PEERINGDB_IX_PRESENCE", .name = "PeeringDB — internet exchange presence",
    .name_ja = "PeeringDB — IX接続状況", .category = "cyber",
    .portal = "https://www.peeringdb.com", .record_type = "peeringdb-ixlan",
    .tags = "\"bgp\",\"peering\",\"ix\"", .want = HP_ASN, .free_tier = 1,
    .url = "https://www.peeringdb.com/api/netixlan?asn={qd}", .array_path = "data",
    .title_keys = "name,ix_id", .id_keys = "id", .max_items = 60,
    .description = "Every internet exchange an AS is present on, with the peering "
      "IPv4/IPv6 addresses and port speeds — geographic footprint inferred from "
      "interconnection rather than from a marketing page" },

  { .id = "PEERINGDB_FACILITIES", .name = "PeeringDB — data-centre facilities",
    .name_ja = "PeeringDB — データセンター施設", .category = "infrastructure",
    .portal = "https://www.peeringdb.com", .record_type = "peeringdb-fac",
    .tags = "\"peering\",\"datacenter\"", .free_tier = 1,
    .url = "https://www.peeringdb.com/api/fac?name__contains={q}",
    .array_path = "data", .title_keys = "name", .id_keys = "id",
    .lat_key = "latitude", .lon_key = "longitude", .max_items = 40,
    .description = "Data-centre facilities matching a name or operator — street "
      "address, coordinates, campus and the networks present. Turns an AS number "
      "into a building" },

  /* ── Certificates, passive DNS, archives, scanning ─────────────────────── */
  { .id = "CERTSPOTTER_ISSUANCES", .name = "Cert Spotter — certificate issuances",
    .name_ja = "Cert Spotter — 証明書発行履歴", .category = "cyber",
    .portal = "https://sslmate.com/certspotter/", .record_type = "certificate",
    .tags = "\"certificates\",\"ct-logs\"", .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://api.certspotter.com/v1/issuances?domain={qh}"
           "&include_subdomains=true&expand=dns_names&expand=issuer&expand=cert_der",
    .title_keys = "issuer.name,dns_names.0", .id_keys = "id",
    .date_keys = "not_before", .max_items = 50,
    .description = "Certificate-transparency issuances for a domain and all its "
      "subdomains — issuer, validity window, SHA-256 and the full SAN list, which is "
      "the cheapest complete subdomain inventory that exists" },

  { .id = "MNEMONIC_PASSIVE_DNS", .name = "Mnemonic — passive DNS history",
    .name_ja = "Mnemonic — パッシブDNS履歴", .category = "cyber",
    .portal = "https://passivedns.mnemonic.no", .record_type = "passive-dns",
    .tags = "\"dns\",\"history\"", .free_tier = 1,
    .url = "https://api.mnemonic.no/pdns/v3/{q}?limit=100",
    .array_path = "data", .title_keys = "query,answer", .id_keys = "answer",
    .date_keys = "lastSeenTimestamp", .max_items = 60,
    .description = "Historical DNS resolutions for a domain or IP with first/last-"
      "seen timestamps — where a hostname used to point, and every other hostname "
      "that resolved to the same address" },

  { .id = "WAYBACK_CDX_DOMAIN", .name = "Wayback CDX — full domain capture index",
    .name_ja = "Wayback CDX — ドメイン全捕捉索引", .category = "investigation",
    .portal = "https://web.archive.org", .record_type = "web-archive-capture",
    .tags = "\"archive\",\"history\"", .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://web.archive.org/cdx/search/cdx?url={qh}&matchType=domain"
           "&output=json&limit=200&collapse=urlkey&fl=timestamp,original,mimetype,statuscode,digest",
    .csv_no_header = 0, .max_items = 60, .title_keys = "1", .id_keys = "4",
    .description = "Every distinct URL the Internet Archive holds for a whole domain, "
      "not just its front page — timestamps, MIME types, status codes and content "
      "digests, which is how deleted subpages and staging hosts are recovered" },

  { .id = "COMMONCRAWL_INDEX", .name = "Common Crawl — URL index for a domain",
    .name_ja = "Common Crawl — ドメインURL索引", .category = "investigation",
    .portal = "https://commoncrawl.org", .record_type = "web-crawl-record",
    .tags = "\"archive\",\"crawl\"", .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://index.commoncrawl.org/CC-MAIN-2024-33-index?url=*.{qh}"
           "&output=json&limit=100",
    .max_items = 60, .title_keys = "url", .id_keys = "digest",
    .description = "Common Crawl's captured URLs across all subdomains of a domain — "
      "an independent second archive when the Wayback Machine has been excluded by "
      "robots or a takedown" },

  { .id = "URLSCAN_DOMAIN_HISTORY", .name = "urlscan.io — scan history for a domain",
    .name_ja = "urlscan.io — ドメインスキャン履歴", .category = "cyber",
    .portal = "https://urlscan.io", .record_type = "url-scan",
    .tags = "\"phishing\",\"web\"", .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://urlscan.io/api/v1/search/?q=domain%3A{qh}&size=40",
    .array_path = "results", .title_keys = "task.url,page.domain", .id_keys = "_id",
    .date_keys = "task.time", .max_items = 40,
    .description = "Every submitted scan of a domain — the resolved IP, ASN, server, "
      "page title, screenshot and DOM for each visit, which reconstructs what a site "
      "looked like on a given day" },

  { .id = "OTX_PASSIVE_DNS", .name = "AlienVault OTX — passive DNS & URL list",
    .name_ja = "OTX — パッシブDNS/URL一覧", .category = "cyber",
    .portal = "https://otx.alienvault.com", .record_type = "passive-dns",
    .tags = "\"dns\",\"threat-intel\"", .want = HP_DOMAIN,
    .key_env = "OTX_API_KEY", .free_tier = 1,
    .url = "https://otx.alienvault.com/api/v1/indicators/domain/{qh}/passive_dns",
    .headers = { "X-OTX-API-KEY: {key}", NULL },
    .array_path = "passive_dns", .title_keys = "hostname,address",
    .id_keys = "address", .date_keys = "last", .max_items = 60,
    .description = "OTX passive-DNS observations for a domain — historical hostname→"
      "address mappings with first/last seen and the ASN each address sat in" },
};

HP_REGISTER_TABLE(HP_NETINTEL)
