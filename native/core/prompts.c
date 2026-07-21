/* core/prompts.c — verbatim C port of server/src/osint/prompts.js.
 *
 * The literal blocks below are copied byte-for-byte from prompts.js. JS source
 * escapes ('\n', '\"', "...'...") are translated to their literal characters,
 * so the emitted bytes are identical to what the JS template literals produce.
 * `query` / `results_json` / `services_list` are spliced in raw, exactly as
 * the JS `${...}` substitutions do (no JSON-escaping — JS doesn't either). */
#include "prompts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JO_REPO_ROOT is -D'd by the Makefile to the JapanOSINT repo root (same
 * mechanism db.c/keysapi.c use); fallback keeps the file standalone. The JS
 * GRAMMAR_DIR is `../../grammars/` from server/src/utils/ -> server/grammars/. */
#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

/* ── tiny growable string buffer ──────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; int err; } sb;

static void sb_reserve(sb *b, size_t add) {
  if (b->err) return;
  if (b->len + add + 1 <= b->cap) return;
  size_t nc = b->cap ? b->cap : 256;
  while (nc < b->len + add + 1) nc *= 2;
  char *np = realloc(b->p, nc);
  if (!np) { b->err = 1; return; }
  b->p = np; b->cap = nc;
}
static void sb_add(sb *b, const char *s) {
  if (!s) s = "";
  size_t n = strlen(s);
  sb_reserve(b, n);
  if (b->err) return;
  memcpy(b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = '\0';
}
/* finalize: return the heap buffer (caller frees) or NULL on any prior OOM. */
static char *sb_take(sb *b) {
  if (b->err) { free(b->p); return NULL; }
  if (!b->p) { b->p = malloc(1); if (b->p) b->p[0] = '\0'; }
  return b->p;
}

/* ── ENTITY_TYPES_PROMPT (verbatim from prompts.js) ───────────────────── */
static const char *const ENTITY_TYPES_PROMPT =
  "Entity types to identify:\n"
  "- CORE: email, ip, domain, phone, username, person, company, address, url\n"
  "- TRANSPORT: vehicle, flight, vessel, container\n"
  "- FINANCIAL: crypto, iban, swift, credit_card, bank_account\n"
  "- NETWORK: dns, asn, mac, subnet, port\n"
  "- LOCATION: coordinates, timezone\n"
  "- IDENTITY: ssn, passport, license, national_id, tax_id\n"
  "- TECHNICAL: hash, cve, api_key, jwt, uuid, file_path, registry\n"
  "- DEVICE: imei, imsi, serial, barcode, qr\n"
  "- OTHER: image, keyword, unknown\n";

/* ── prompt_analysis — createAnalysisPrompt(query, servicesList) ──────── */
char *prompt_analysis(const char *query, const char *services_list) {
  const char *list = (services_list && services_list[0])
                       ? services_list : "(service list unavailable)";
  if (!query) query = "";
  sb b = {0};

  sb_add(&b, "Extract OSINT entities from: ");
  sb_add(&b, query);
  sb_add(&b, "\n\n");
  sb_add(&b,
    "CRITICAL: Output compact JSON on a SINGLE LINE with NO extra whitespace, newlines, or formatting.\n\n"
    "CRITICAL: The entity VALUE must contain ONLY the identifier itself, NEVER context words!\n"
    "- WRONG: {\"value\": \"weather at location of ip adress 43.45.3.23\", \"type\": \"ip\"}\n"
    "- RIGHT: {\"value\": \"43.45.3.23\", \"type\": \"ip\"}\n"
    "- WRONG: {\"value\": \"find info on john@test.com\", \"type\": \"email\"}\n"
    "- RIGHT: {\"value\": \"john@test.com\", \"type\": \"email\"}\n\n"
    "NOTE: Ignore spelling errors in queries (e.g., \"adress\" = \"address\", \"emal\" = \"email\").\n"
    "Extract the actual identifier regardless of surrounding typos or casual language.\n\n"
    "CRITICAL: Extract ALL entities found in the query. If query contains BOTH a person name AND an IP address, you MUST return BOTH in the entities array.\n"
    "CRITICAL: Comma-separated items are SEPARATE entities. Each item between commas should be its own entity.\n"
    "IP addresses are ANY dotted decimal format (e.g., 8.8.8.8, 45.33.32.1, 192.168.1.1).\n"
    "License plates can be alphanumeric (e.g., ABC123, 5334DE2434, XYZ789). Extract the plate code, not the vehicle description.\n\n"
    "PHONE vs IP DISTINCTION (CRITICAL):\n"
    "- IP addresses ALWAYS have exactly 4 dot-separated octets (e.g., 8.8.8.8, 192.168.1.1, 10.0.0.1)\n"
    "- Phone numbers are 9-15 CONTINUOUS digits, may start with 0 or +, NO dots between digits\n"
    "- Number with NO dots and 9+ digits = PHONE, not IP\n"
    "- Examples: 0781218793 = PHONE, 078.121.87.93 = IP\n\n"
    "CONTEXT WORDS ARE NOT ENTITIES: Words like 'weather', 'map', 'nearby', 'location', 'shodan', 'search', 'lookup', 'find', 'carrier' are CONTEXT, not entities.\n"
    "Do NOT create keyword entities for these context words.\n"
    "For 'weather at IP X' queries: Output ONLY the IP entity with IP_GEOLOCATION, set chain_reason:true.\n"
    "Phase 2 will extract coordinates and call weather services.\n\n");
  sb_add(&b, ENTITY_TYPES_PROMPT);
  sb_add(&b,
    "Output JSON with ALL entities found using the types above.\n\n"
    "=== CHAIN_REASON DECISION (REQUIRED BOOLEAN) ===\n"
    "chain_reason is REQUIRED and must be true or false:\n"
    "Set chain_reason: true when Phase 2 follow-up investigation is valuable:\n"
    "- Weather/map/nearby queries + IP: true (coords enable WEATHER_SERVICE, map services)\n"
    "- Person/company queries: true (may reveal employees, accounts, employers)\n"
    "- Email breach queries: true (may reveal linked accounts, employers)\n"
    "- Domain WHOIS 'who owns' queries: true (may reveal owner name/company)\n"
    "- Vehicle/phone lookups: true (may reveal owner name)\n"
    "- Flight queries: true (may reveal airports for geolocation)\n\n"
    "Set chain_reason: false for simple informational queries:\n"
    "- 'what is X' or 'lookup X': false (purely informational)\n"
    "- Hash/CVE lookups: false (technical data, no personal info)\n"
    "- Simple DNS record queries: false (no personal data to chain)\n\n");
  sb_add(&b, "Available services:\n");
  sb_add(&b, list);
  sb_add(&b, "\n\n");
  sb_add(&b,
    "Routing examples — list EVERY service whose description fits the entity in the\n"
    "per-entity services[] array (max 8); recommended_services = the 1-3 best overall:\n"
    "Query: investigate IP 45.33.32.1\n"
    "{\"entities\":[{\"value\":\"45.33.32.1\",\"type\":\"ip\",\"confidence\":\"high\",\"services\":[\"IP_GEOLOCATION\",\"REVERSE_DNS\",\"ASN_LOOKUP\",\"BGP_LOOKUP\",\"IP_REPUTATION\",\"THREAT_INTEL\",\"THREAT_FEED_LOOKUP\",\"IOC_LOOKUP\"]}],\"recommended_services\":[\"IP_GEOLOCATION\",\"REVERSE_DNS\",\"ASN_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"ip entity\",\"chain_reason\":true}\n\n"
    "Query: 8.8.8.8\n"
    "{\"entities\":[{\"value\":\"8.8.8.8\",\"type\":\"ip\",\"confidence\":\"high\",\"services\":[\"MALWARE_ANALYSIS\",\"SHODAN_SEARCH\",\"CENSYS_SEARCH\",\"PORT_SCANNER\",\"TOR_EXIT_CHECK\",\"IP_GEOLOCATION\",\"REVERSE_DNS\",\"ASN_LOOKUP\"]}],\"recommended_services\":[\"MALWARE_ANALYSIS\",\"SHODAN_SEARCH\",\"CENSYS_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"ip entity\",\"chain_reason\":true}\n\n"
    "Query: is 185.220.101.1 malicious or a tor node\n"
    "{\"entities\":[{\"value\":\"185.220.101.1\",\"type\":\"ip\",\"confidence\":\"high\",\"services\":[\"BGP_LOOKUP\",\"IP_REPUTATION\",\"THREAT_INTEL\",\"THREAT_FEED_LOOKUP\",\"IOC_LOOKUP\",\"MALWARE_ANALYSIS\",\"SHODAN_SEARCH\",\"CENSYS_SEARCH\"]}],\"recommended_services\":[\"BGP_LOOKUP\",\"IP_REPUTATION\",\"THREAT_INTEL\"],\"complexity\":\"low\",\"analysis\":\"ip entity\",\"chain_reason\":true}\n\n"
    "Query: open ports and exposure of 192.0.2.10\n"
    "{\"entities\":[{\"value\":\"192.0.2.10\",\"type\":\"ip\",\"confidence\":\"high\",\"services\":[\"PORT_SCANNER\",\"TOR_EXIT_CHECK\",\"IP_GEOLOCATION\",\"REVERSE_DNS\",\"ASN_LOOKUP\",\"BGP_LOOKUP\",\"IP_REPUTATION\",\"THREAT_INTEL\"]}],\"recommended_services\":[\"PORT_SCANNER\",\"TOR_EXIT_CHECK\",\"IP_GEOLOCATION\"],\"complexity\":\"low\",\"analysis\":\"ip entity\",\"chain_reason\":true}\n\n"
    "Query: threat reputation for 203.0.113.5\n"
    "{\"entities\":[{\"value\":\"203.0.113.5\",\"type\":\"ip\",\"confidence\":\"high\",\"services\":[\"THREAT_FEED_LOOKUP\",\"IOC_LOOKUP\",\"MALWARE_ANALYSIS\",\"SHODAN_SEARCH\",\"CENSYS_SEARCH\",\"PORT_SCANNER\",\"TOR_EXIT_CHECK\",\"IP_GEOLOCATION\"]}],\"recommended_services\":[\"THREAT_FEED_LOOKUP\",\"IOC_LOOKUP\",\"MALWARE_ANALYSIS\"],\"complexity\":\"low\",\"analysis\":\"ip entity\",\"chain_reason\":true}\n\n"
    "Query: who owns example.com\n"
    "{\"entities\":[{\"value\":\"example.com\",\"type\":\"domain\",\"confidence\":\"high\",\"services\":[\"DOMAIN_WHOIS\",\"DOMAIN_AGE\",\"DOMAIN_HISTORY\",\"HISTORICAL_WHOIS\",\"DNS_RECORDS\",\"SUBDOMAIN_FINDER\",\"CERTIFICATE_TRANSPARENCY\",\"SSL_ANALYZER\"]}],\"recommended_services\":[\"DOMAIN_WHOIS\",\"DOMAIN_AGE\",\"DOMAIN_HISTORY\"],\"complexity\":\"low\",\"analysis\":\"domain entity\",\"chain_reason\":true}\n\n"
    "Query: subdomains and certs of acme.co.jp\n"
    "{\"entities\":[{\"value\":\"acme.co.jp\",\"type\":\"domain\",\"confidence\":\"high\",\"services\":[\"EMAIL_HARVESTER\",\"TECH_STACK_DETECTION\",\"IP_REPUTATION\",\"DOMAIN_WHOIS\",\"DOMAIN_AGE\",\"DOMAIN_HISTORY\",\"HISTORICAL_WHOIS\",\"DNS_RECORDS\"]}],\"recommended_services\":[\"EMAIL_HARVESTER\",\"TECH_STACK_DETECTION\",\"IP_REPUTATION\"],\"complexity\":\"low\",\"analysis\":\"domain entity\",\"chain_reason\":true}\n\n"
    "Query: domain age and history of test.org\n"
    "{\"entities\":[{\"value\":\"test.org\",\"type\":\"domain\",\"confidence\":\"high\",\"services\":[\"SUBDOMAIN_FINDER\",\"CERTIFICATE_TRANSPARENCY\",\"SSL_ANALYZER\",\"EMAIL_HARVESTER\",\"TECH_STACK_DETECTION\",\"IP_REPUTATION\",\"DOMAIN_WHOIS\",\"DOMAIN_AGE\"]}],\"recommended_services\":[\"SUBDOMAIN_FINDER\",\"CERTIFICATE_TRANSPARENCY\",\"SSL_ANALYZER\"],\"complexity\":\"low\",\"analysis\":\"domain entity\",\"chain_reason\":true}\n\n"
    "Query: ssl and tech stack for shop.example.io\n"
    "{\"entities\":[{\"value\":\"shop.example.io\",\"type\":\"domain\",\"confidence\":\"high\",\"services\":[\"DOMAIN_HISTORY\",\"HISTORICAL_WHOIS\",\"DNS_RECORDS\",\"SUBDOMAIN_FINDER\",\"CERTIFICATE_TRANSPARENCY\",\"SSL_ANALYZER\",\"EMAIL_HARVESTER\",\"TECH_STACK_DETECTION\"]}],\"recommended_services\":[\"DOMAIN_HISTORY\",\"HISTORICAL_WHOIS\",\"DNS_RECORDS\"],\"complexity\":\"low\",\"analysis\":\"domain entity\",\"chain_reason\":true}\n\n"
    "Query: dns and whois history for legacy.example.net\n"
    "{\"entities\":[{\"value\":\"legacy.example.net\",\"type\":\"domain\",\"confidence\":\"high\",\"services\":[\"IP_REPUTATION\",\"DOMAIN_WHOIS\",\"DOMAIN_AGE\",\"DOMAIN_HISTORY\",\"HISTORICAL_WHOIS\",\"DNS_RECORDS\",\"SUBDOMAIN_FINDER\",\"CERTIFICATE_TRANSPARENCY\"]}],\"recommended_services\":[\"IP_REPUTATION\",\"DOMAIN_WHOIS\",\"DOMAIN_AGE\"],\"complexity\":\"low\",\"analysis\":\"domain entity\",\"chain_reason\":true}\n\n"
    "Query: analyze url https://example.com/login\n"
    "{\"entities\":[{\"value\":\"https://example.com/login\",\"type\":\"url\",\"confidence\":\"high\",\"services\":[\"URL_ANALYZER\",\"WAYBACK_MACHINE\",\"TECH_STACK_DETECTION\",\"DOCUMENT_ANALYZER\"]}],\"recommended_services\":[\"URL_ANALYZER\",\"WAYBACK_MACHINE\",\"TECH_STACK_DETECTION\"],\"complexity\":\"low\",\"analysis\":\"url entity\",\"chain_reason\":false}\n\n"
    "Query: archived versions of https://blog.example.org\n"
    "{\"entities\":[{\"value\":\"https://blog.example.org\",\"type\":\"url\",\"confidence\":\"high\",\"services\":[\"URL_ANALYZER\",\"WAYBACK_MACHINE\",\"TECH_STACK_DETECTION\",\"DOCUMENT_ANALYZER\"]}],\"recommended_services\":[\"URL_ANALYZER\",\"WAYBACK_MACHINE\",\"TECH_STACK_DETECTION\"],\"complexity\":\"low\",\"analysis\":\"url entity\",\"chain_reason\":false}\n\n"
    "Query: scan https://site.test/app\n"
    "{\"entities\":[{\"value\":\"https://site.test/app\",\"type\":\"url\",\"confidence\":\"high\",\"services\":[\"URL_ANALYZER\",\"WAYBACK_MACHINE\",\"TECH_STACK_DETECTION\",\"DOCUMENT_ANALYZER\"]}],\"recommended_services\":[\"URL_ANALYZER\",\"WAYBACK_MACHINE\",\"TECH_STACK_DETECTION\"],\"complexity\":\"low\",\"analysis\":\"url entity\",\"chain_reason\":false}\n\n"
    "Query: metadata of https://example.com/report.pdf\n"
    "{\"entities\":[{\"value\":\"https://example.com/report.pdf\",\"type\":\"url\",\"confidence\":\"high\",\"services\":[\"PDF_ANALYZER\",\"EXIF_EXTRACTOR\",\"DOCUMENT_ANALYZER\"]}],\"recommended_services\":[\"PDF_ANALYZER\",\"EXIF_EXTRACTOR\",\"DOCUMENT_ANALYZER\"],\"complexity\":\"low\",\"analysis\":\"url entity\",\"chain_reason\":false}\n\n"
    "Query: exif gps for https://example.com/photo.jpg\n"
    "{\"entities\":[{\"value\":\"https://example.com/photo.jpg\",\"type\":\"url\",\"confidence\":\"high\",\"services\":[\"PDF_ANALYZER\",\"EXIF_EXTRACTOR\",\"DOCUMENT_ANALYZER\"]}],\"recommended_services\":[\"PDF_ANALYZER\",\"EXIF_EXTRACTOR\",\"DOCUMENT_ANALYZER\"],\"complexity\":\"low\",\"analysis\":\"url entity\",\"chain_reason\":false}\n\n"
    "Query: extract text from https://files.test/scan.pdf\n"
    "{\"entities\":[{\"value\":\"https://files.test/scan.pdf\",\"type\":\"url\",\"confidence\":\"high\",\"services\":[\"PDF_ANALYZER\",\"EXIF_EXTRACTOR\",\"DOCUMENT_ANALYZER\"]}],\"recommended_services\":[\"PDF_ANALYZER\",\"EXIF_EXTRACTOR\",\"DOCUMENT_ANALYZER\"],\"complexity\":\"low\",\"analysis\":\"url entity\",\"chain_reason\":false}\n\n"
    "Query: john@example.com\n"
    "{\"entities\":[{\"value\":\"john@example.com\",\"type\":\"email\",\"confidence\":\"high\",\"services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\",\"PASSWORD_CHECKER\",\"DARK_WEB_MONITOR\",\"PASTE_SITE_SEARCH\",\"DATA_EXTRACTOR\"]}],\"recommended_services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\",\"PASSWORD_CHECKER\"],\"complexity\":\"low\",\"analysis\":\"email entity\",\"chain_reason\":true}\n\n"
    "Query: breach history of alice@corp.jp\n"
    "{\"entities\":[{\"value\":\"alice@corp.jp\",\"type\":\"email\",\"confidence\":\"high\",\"services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\",\"PASSWORD_CHECKER\",\"DARK_WEB_MONITOR\",\"PASTE_SITE_SEARCH\",\"DATA_EXTRACTOR\"]}],\"recommended_services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\",\"PASSWORD_CHECKER\"],\"complexity\":\"low\",\"analysis\":\"email entity\",\"chain_reason\":true}\n\n"
    "Query: osint on email user@test.net\n"
    "{\"entities\":[{\"value\":\"user@test.net\",\"type\":\"email\",\"confidence\":\"high\",\"services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\",\"PASSWORD_CHECKER\",\"DARK_WEB_MONITOR\",\"PASTE_SITE_SEARCH\",\"DATA_EXTRACTOR\"]}],\"recommended_services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\",\"PASSWORD_CHECKER\"],\"complexity\":\"low\",\"analysis\":\"email entity\",\"chain_reason\":true}\n\n"
    "Query: username darkuser99\n"
    "{\"entities\":[{\"value\":\"darkuser99\",\"type\":\"username\",\"confidence\":\"high\",\"services\":[\"SOCIAL_USERNAME\",\"GITHUB_CODE_SEARCH\",\"CREDENTIAL_LEAK_SEARCH\"]}],\"recommended_services\":[\"SOCIAL_USERNAME\",\"GITHUB_CODE_SEARCH\",\"CREDENTIAL_LEAK_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"username entity\",\"chain_reason\":true}\n\n"
    "Query: find accounts for john_doe\n"
    "{\"entities\":[{\"value\":\"john_doe\",\"type\":\"username\",\"confidence\":\"high\",\"services\":[\"SOCIAL_USERNAME\",\"GITHUB_CODE_SEARCH\",\"CREDENTIAL_LEAK_SEARCH\"]}],\"recommended_services\":[\"SOCIAL_USERNAME\",\"GITHUB_CODE_SEARCH\",\"CREDENTIAL_LEAK_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"username entity\",\"chain_reason\":true}\n\n"
    "Query: leaks for handle techguy\n"
    "{\"entities\":[{\"value\":\"techguy\",\"type\":\"username\",\"confidence\":\"high\",\"services\":[\"SOCIAL_USERNAME\",\"GITHUB_CODE_SEARCH\",\"CREDENTIAL_LEAK_SEARCH\"]}],\"recommended_services\":[\"SOCIAL_USERNAME\",\"GITHUB_CODE_SEARCH\",\"CREDENTIAL_LEAK_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"username entity\",\"chain_reason\":true}\n\n"
    "Query: background on John Smith\n"
    "{\"entities\":[{\"value\":\"John Smith\",\"type\":\"person\",\"confidence\":\"high\",\"services\":[\"PERSON_SEARCH\",\"SOCIAL_USERNAME\",\"SANCTIONS_CHECK\",\"PEP_CHECK\",\"WATCHLIST_CHECK_NEW\",\"LEGAL_SEARCH\",\"CRIMINAL_RECORDS\",\"COURT_RECORDS\"]}],\"recommended_services\":[\"PERSON_SEARCH\",\"SOCIAL_USERNAME\",\"SANCTIONS_CHECK\"],\"complexity\":\"low\",\"analysis\":\"person entity\",\"chain_reason\":true}\n\n"
    "Query: is Maria Garcia sanctioned or politically exposed\n"
    "{\"entities\":[{\"value\":\"Maria Garcia\",\"type\":\"person\",\"confidence\":\"high\",\"services\":[\"ACADEMIC_SEARCH\",\"NEWS_AGGREGATOR\",\"PERSON_SEARCH\",\"SOCIAL_USERNAME\",\"SANCTIONS_CHECK\",\"PEP_CHECK\",\"WATCHLIST_CHECK_NEW\",\"LEGAL_SEARCH\"]}],\"recommended_services\":[\"ACADEMIC_SEARCH\",\"NEWS_AGGREGATOR\",\"PERSON_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"person entity\",\"chain_reason\":true}\n\n"
    "Query: court records and papers for Robert Chen\n"
    "{\"entities\":[{\"value\":\"Robert Chen\",\"type\":\"person\",\"confidence\":\"high\",\"services\":[\"CRIMINAL_RECORDS\",\"COURT_RECORDS\",\"ACADEMIC_SEARCH\",\"NEWS_AGGREGATOR\",\"PERSON_SEARCH\",\"SOCIAL_USERNAME\",\"SANCTIONS_CHECK\",\"PEP_CHECK\"]}],\"recommended_services\":[\"CRIMINAL_RECORDS\",\"COURT_RECORDS\",\"ACADEMIC_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"person entity\",\"chain_reason\":true}\n\n"
    "Query: news and academic profile of Aisha Khan\n"
    "{\"entities\":[{\"value\":\"Aisha Khan\",\"type\":\"person\",\"confidence\":\"high\",\"services\":[\"WATCHLIST_CHECK_NEW\",\"LEGAL_SEARCH\",\"CRIMINAL_RECORDS\",\"COURT_RECORDS\",\"ACADEMIC_SEARCH\",\"NEWS_AGGREGATOR\",\"PERSON_SEARCH\",\"SOCIAL_USERNAME\"]}],\"recommended_services\":[\"WATCHLIST_CHECK_NEW\",\"LEGAL_SEARCH\",\"CRIMINAL_RECORDS\"],\"complexity\":\"low\",\"analysis\":\"person entity\",\"chain_reason\":true}\n\n"
    "Query: corporate records for Acme Corp\n"
    "{\"entities\":[{\"value\":\"Acme Corp\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"COMPANY_SEARCH\",\"COMPANY_LOOKUP\",\"LEI_SEARCH\",\"VAT_VALIDATOR\",\"SEC_EDGAR_SEARCH\",\"TRADEMARK_SEARCH\",\"PATENT_SEARCH\",\"BANKRUPTCY_SEARCH\"]}],\"recommended_services\":[\"COMPANY_SEARCH\",\"COMPANY_LOOKUP\",\"LEI_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: LEI filings trademarks and sanctions for OpenAI\n"
    "{\"entities\":[{\"value\":\"OpenAI\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"SANCTIONS_CHECK\",\"PEP_CHECK\",\"WATCHLIST_CHECK_NEW\",\"NEWS_AGGREGATOR\",\"COMPANY_SEARCH\",\"COMPANY_LOOKUP\",\"LEI_SEARCH\",\"VAT_VALIDATOR\"]}],\"recommended_services\":[\"SANCTIONS_CHECK\",\"PEP_CHECK\",\"WATCHLIST_CHECK_NEW\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: officers and bankruptcies of Globex Inc\n"
    "{\"entities\":[{\"value\":\"Globex Inc\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"SEC_EDGAR_SEARCH\",\"TRADEMARK_SEARCH\",\"PATENT_SEARCH\",\"BANKRUPTCY_SEARCH\",\"SANCTIONS_CHECK\",\"PEP_CHECK\",\"WATCHLIST_CHECK_NEW\",\"NEWS_AGGREGATOR\"]}],\"recommended_services\":[\"SEC_EDGAR_SEARCH\",\"TRADEMARK_SEARCH\",\"PATENT_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: patents trademarks and VAT for Stark Industries\n"
    "{\"entities\":[{\"value\":\"Stark Industries\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"COMPANY_SEARCH\",\"COMPANY_LOOKUP\",\"LEI_SEARCH\",\"VAT_VALIDATOR\",\"SEC_EDGAR_SEARCH\",\"TRADEMARK_SEARCH\",\"PATENT_SEARCH\",\"BANKRUPTCY_SEARCH\"]}],\"recommended_services\":[\"COMPANY_SEARCH\",\"COMPANY_LOOKUP\",\"LEI_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: SEC filings and watchlists for Umbrella Corp\n"
    "{\"entities\":[{\"value\":\"Umbrella Corp\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"SANCTIONS_CHECK\",\"PEP_CHECK\",\"WATCHLIST_CHECK_NEW\",\"NEWS_AGGREGATOR\",\"COMPANY_SEARCH\",\"COMPANY_LOOKUP\",\"LEI_SEARCH\",\"VAT_VALIDATOR\"]}],\"recommended_services\":[\"SANCTIONS_CHECK\",\"PEP_CHECK\",\"WATCHLIST_CHECK_NEW\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: Japanese company financials for Toyota Motor Corporation\n"
    "{\"entities\":[{\"value\":\"Toyota Motor Corporation\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"GBIZINFO\",\"UFOCATCH\",\"BUFFETT_CODE\",\"SHIKIHO\",\"JPX_SHORT_SELLING\",\"JP_CORPUS_LOOKUP\",\"COMPANY_SEARCH\",\"LEI_SEARCH\"]}],\"recommended_services\":[\"GBIZINFO\",\"UFOCATCH\",\"BUFFETT_CODE\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: disclosures and short selling for Sony\n"
    "{\"entities\":[{\"value\":\"Sony\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"GBIZINFO\",\"UFOCATCH\",\"BUFFETT_CODE\",\"SHIKIHO\",\"JPX_SHORT_SELLING\",\"JP_CORPUS_LOOKUP\",\"COMPANY_SEARCH\",\"LEI_SEARCH\"]}],\"recommended_services\":[\"GBIZINFO\",\"UFOCATCH\",\"BUFFETT_CODE\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: listed-company metrics and forecasts for NEC Corporation\n"
    "{\"entities\":[{\"value\":\"NEC Corporation\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"GBIZINFO\",\"UFOCATCH\",\"BUFFETT_CODE\",\"SHIKIHO\",\"JPX_SHORT_SELLING\",\"JP_CORPUS_LOOKUP\",\"COMPANY_SEARCH\",\"LEI_SEARCH\"]}],\"recommended_services\":[\"GBIZINFO\",\"UFOCATCH\",\"BUFFETT_CODE\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: registries licenses and reviews for SoftBank\n"
    "{\"entities\":[{\"value\":\"SoftBank\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"INVOICE_REGISTRY\",\"CONSTRUCTION_LICENSE\",\"FSA_FINBIZ_REGISTRY\",\"NPO_PORTAL\",\"MINPAKU_REGISTRY\",\"OPENWORK\",\"EN_LIGHTHOUSE\",\"TENSHOKU_KAIGI\"]}],\"recommended_services\":[\"INVOICE_REGISTRY\",\"CONSTRUCTION_LICENSE\",\"FSA_FINBIZ_REGISTRY\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: invoice registration and employee reviews for Rakuten\n"
    "{\"entities\":[{\"value\":\"Rakuten\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"PRTIMES\",\"ITOWNPAGE\",\"INVOICE_REGISTRY\",\"CONSTRUCTION_LICENSE\",\"FSA_FINBIZ_REGISTRY\",\"NPO_PORTAL\",\"MINPAKU_REGISTRY\",\"OPENWORK\"]}],\"recommended_services\":[\"PRTIMES\",\"ITOWNPAGE\",\"INVOICE_REGISTRY\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: licenses press releases and directory listing for Lawson\n"
    "{\"entities\":[{\"value\":\"Lawson\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"EN_LIGHTHOUSE\",\"TENSHOKU_KAIGI\",\"PRTIMES\",\"ITOWNPAGE\",\"INVOICE_REGISTRY\",\"CONSTRUCTION_LICENSE\",\"FSA_FINBIZ_REGISTRY\",\"NPO_PORTAL\"]}],\"recommended_services\":[\"EN_LIGHTHOUSE\",\"TENSHOKU_KAIGI\",\"PRTIMES\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: registries licenses and reviews for SoftBank\n"
    "{\"entities\":[{\"value\":\"SoftBank\",\"type\":\"company\",\"confidence\":\"high\",\"services\":[\"MINPAKU_REGISTRY\",\"OPENWORK\",\"EN_LIGHTHOUSE\",\"TENSHOKU_KAIGI\",\"PRTIMES\",\"ITOWNPAGE\",\"INVOICE_REGISTRY\",\"CONSTRUCTION_LICENSE\"]}],\"recommended_services\":[\"MINPAKU_REGISTRY\",\"OPENWORK\",\"EN_LIGHTHOUSE\"],\"complexity\":\"low\",\"analysis\":\"company entity\",\"chain_reason\":true}\n\n"
    "Query: +1 555-123-4567\n"
    "{\"entities\":[{\"value\":\"+1 555-123-4567\",\"type\":\"phone\",\"confidence\":\"high\",\"services\":[\"PHONE_LOOKUP\",\"CARRIER_LOOKUP\",\"PHONE_REPUTATION\"]}],\"recommended_services\":[\"PHONE_LOOKUP\",\"CARRIER_LOOKUP\",\"PHONE_REPUTATION\"],\"complexity\":\"low\",\"analysis\":\"phone entity\",\"chain_reason\":true}\n\n"
    "Query: carrier for 09012345678\n"
    "{\"entities\":[{\"value\":\"09012345678\",\"type\":\"phone\",\"confidence\":\"high\",\"services\":[\"PHONE_LOOKUP\",\"CARRIER_LOOKUP\",\"PHONE_REPUTATION\"]}],\"recommended_services\":[\"PHONE_LOOKUP\",\"CARRIER_LOOKUP\",\"PHONE_REPUTATION\"],\"complexity\":\"low\",\"analysis\":\"phone entity\",\"chain_reason\":true}\n\n"
    "Query: is +81 90-1234-5678 valid or voip\n"
    "{\"entities\":[{\"value\":\"+81 90-1234-5678\",\"type\":\"phone\",\"confidence\":\"high\",\"services\":[\"PHONE_LOOKUP\",\"CARRIER_LOOKUP\",\"PHONE_REPUTATION\"]}],\"recommended_services\":[\"PHONE_LOOKUP\",\"CARRIER_LOOKUP\",\"PHONE_REPUTATION\"],\"complexity\":\"low\",\"analysis\":\"phone entity\",\"chain_reason\":true}\n\n"
    "Query: track wallet 0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb\n"
    "{\"entities\":[{\"value\":\"0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb\",\"type\":\"crypto\",\"confidence\":\"high\",\"services\":[\"CRYPTO_TRACKER\",\"DEFI_TRACKER\",\"EXCHANGE_FLOW\",\"WHALE_ALERT\"]}],\"recommended_services\":[\"CRYPTO_TRACKER\",\"DEFI_TRACKER\",\"EXCHANGE_FLOW\"],\"complexity\":\"low\",\"analysis\":\"crypto entity\",\"chain_reason\":true}\n\n"
    "Query: defi activity of 0x28C6c06298d514Db089934071355E5743bf21d60\n"
    "{\"entities\":[{\"value\":\"0x28C6c06298d514Db089934071355E5743bf21d60\",\"type\":\"crypto\",\"confidence\":\"high\",\"services\":[\"CRYPTO_TRACKER\",\"DEFI_TRACKER\",\"EXCHANGE_FLOW\",\"WHALE_ALERT\"]}],\"recommended_services\":[\"CRYPTO_TRACKER\",\"DEFI_TRACKER\",\"EXCHANGE_FLOW\"],\"complexity\":\"low\",\"analysis\":\"crypto entity\",\"chain_reason\":true}\n\n"
    "Query: balance and flows for 0xAb5801a7D398351b8bE11C439e05C5B3259aeC9B\n"
    "{\"entities\":[{\"value\":\"0xAb5801a7D398351b8bE11C439e05C5B3259aeC9B\",\"type\":\"crypto\",\"confidence\":\"high\",\"services\":[\"CRYPTO_TRACKER\",\"DEFI_TRACKER\",\"EXCHANGE_FLOW\",\"WHALE_ALERT\"]}],\"recommended_services\":[\"CRYPTO_TRACKER\",\"DEFI_TRACKER\",\"EXCHANGE_FLOW\"],\"complexity\":\"low\",\"analysis\":\"crypto entity\",\"chain_reason\":true}\n\n"
    "Query: is 44d88612fea8a8f36de82e1278abb02f malware\n"
    "{\"entities\":[{\"value\":\"44d88612fea8a8f36de82e1278abb02f\",\"type\":\"hash\",\"confidence\":\"high\",\"services\":[\"HASH_LOOKUP\",\"MALWARE_ANALYSIS\",\"IOC_LOOKUP\"]}],\"recommended_services\":[\"HASH_LOOKUP\",\"MALWARE_ANALYSIS\",\"IOC_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"hash entity\",\"chain_reason\":false}\n\n"
    "Query: lookup file hash d41d8cd98f00b204e9800998ecf8427e\n"
    "{\"entities\":[{\"value\":\"d41d8cd98f00b204e9800998ecf8427e\",\"type\":\"hash\",\"confidence\":\"high\",\"services\":[\"HASH_LOOKUP\",\"MALWARE_ANALYSIS\",\"IOC_LOOKUP\"]}],\"recommended_services\":[\"HASH_LOOKUP\",\"MALWARE_ANALYSIS\",\"IOC_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"hash entity\",\"chain_reason\":false}\n\n"
    "Query: reputation of hash e3b0c44298fc1c149afbf4c8996fb924\n"
    "{\"entities\":[{\"value\":\"e3b0c44298fc1c149afbf4c8996fb924\",\"type\":\"hash\",\"confidence\":\"high\",\"services\":[\"HASH_LOOKUP\",\"MALWARE_ANALYSIS\",\"IOC_LOOKUP\"]}],\"recommended_services\":[\"HASH_LOOKUP\",\"MALWARE_ANALYSIS\",\"IOC_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"hash entity\",\"chain_reason\":false}\n\n"
    "Query: decode vin 1HGBH41JXMN109186\n"
    "{\"entities\":[{\"value\":\"1HGBH41JXMN109186\",\"type\":\"vehicle\",\"confidence\":\"high\",\"services\":[\"VEHICLE_LOOKUP\",\"LICENSE_PLATE_LOOKUP\"]}],\"recommended_services\":[\"VEHICLE_LOOKUP\",\"LICENSE_PLATE_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"vehicle entity\",\"chain_reason\":true}\n\n"
    "Query: red sedan plate ABC123\n"
    "{\"entities\":[{\"value\":\"ABC123\",\"type\":\"vehicle\",\"confidence\":\"high\",\"services\":[\"VEHICLE_LOOKUP\",\"LICENSE_PLATE_LOOKUP\"]}],\"recommended_services\":[\"VEHICLE_LOOKUP\",\"LICENSE_PLATE_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"vehicle entity\",\"chain_reason\":true}\n\n"
    "Query: recalls for vin JH4KA8260MC000000\n"
    "{\"entities\":[{\"value\":\"JH4KA8260MC000000\",\"type\":\"vehicle\",\"confidence\":\"high\",\"services\":[\"VEHICLE_LOOKUP\",\"LICENSE_PLATE_LOOKUP\"]}],\"recommended_services\":[\"VEHICLE_LOOKUP\",\"LICENSE_PLATE_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"vehicle entity\",\"chain_reason\":true}\n\n"
    "Query: validate VAT DE123456789\n"
    "{\"entities\":[{\"value\":\"DE123456789\",\"type\":\"tax_id\",\"confidence\":\"high\",\"services\":[\"VAT_VALIDATOR\",\"LEI_SEARCH\",\"COMPANY_SEARCH\"]}],\"recommended_services\":[\"VAT_VALIDATOR\",\"LEI_SEARCH\",\"COMPANY_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"tax_id entity\",\"chain_reason\":false}\n\n"
    "Query: check vat number FR12345678901\n"
    "{\"entities\":[{\"value\":\"FR12345678901\",\"type\":\"tax_id\",\"confidence\":\"high\",\"services\":[\"VAT_VALIDATOR\",\"LEI_SEARCH\",\"COMPANY_SEARCH\"]}],\"recommended_services\":[\"VAT_VALIDATOR\",\"LEI_SEARCH\",\"COMPANY_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"tax_id entity\",\"chain_reason\":false}\n\n"
    "Query: VAT IT12345678901 company\n"
    "{\"entities\":[{\"value\":\"IT12345678901\",\"type\":\"tax_id\",\"confidence\":\"high\",\"services\":[\"VAT_VALIDATOR\",\"LEI_SEARCH\",\"COMPANY_SEARCH\"]}],\"recommended_services\":[\"VAT_VALIDATOR\",\"LEI_SEARCH\",\"COMPANY_SEARCH\"],\"complexity\":\"low\",\"analysis\":\"tax_id entity\",\"chain_reason\":false}\n\n"
    "Query: weather and earthquakes at 35.6762,139.6503\n"
    "{\"entities\":[{\"value\":\"35.6762,139.6503\",\"type\":\"coordinates\",\"confidence\":\"high\",\"services\":[\"WEATHER_SERVICE\",\"EARTHQUAKE_MONITOR\",\"REVERSE_GEOCODING\",\"GEOCODING\",\"ADDRESS_RESOLVER\"]}],\"recommended_services\":[\"WEATHER_SERVICE\",\"EARTHQUAKE_MONITOR\",\"REVERSE_GEOCODING\"],\"complexity\":\"low\",\"analysis\":\"coordinates entity\",\"chain_reason\":true}\n\n"
    "Query: what is at 34.0522,-118.2437\n"
    "{\"entities\":[{\"value\":\"34.0522,-118.2437\",\"type\":\"coordinates\",\"confidence\":\"high\",\"services\":[\"WEATHER_SERVICE\",\"EARTHQUAKE_MONITOR\",\"REVERSE_GEOCODING\",\"GEOCODING\",\"ADDRESS_RESOLVER\"]}],\"recommended_services\":[\"WEATHER_SERVICE\",\"EARTHQUAKE_MONITOR\",\"REVERSE_GEOCODING\"],\"complexity\":\"low\",\"analysis\":\"coordinates entity\",\"chain_reason\":true}\n\n"
    "Query: place name for 51.5074,-0.1278\n"
    "{\"entities\":[{\"value\":\"51.5074,-0.1278\",\"type\":\"coordinates\",\"confidence\":\"high\",\"services\":[\"WEATHER_SERVICE\",\"EARTHQUAKE_MONITOR\",\"REVERSE_GEOCODING\",\"GEOCODING\",\"ADDRESS_RESOLVER\"]}],\"recommended_services\":[\"WEATHER_SERVICE\",\"EARTHQUAKE_MONITOR\",\"REVERSE_GEOCODING\"],\"complexity\":\"low\",\"analysis\":\"coordinates entity\",\"chain_reason\":true}\n\n"
    "Query: geocode Tokyo Tower\n"
    "{\"entities\":[{\"value\":\"Tokyo Tower\",\"type\":\"address\",\"confidence\":\"high\",\"services\":[\"GEOCODING\",\"ADDRESS_RESOLVER\",\"REVERSE_GEOCODING\",\"WEATHER_SERVICE\"]}],\"recommended_services\":[\"GEOCODING\",\"ADDRESS_RESOLVER\",\"REVERSE_GEOCODING\"],\"complexity\":\"low\",\"analysis\":\"address entity\",\"chain_reason\":true}\n\n"
    "Query: coordinates of Shibuya Crossing\n"
    "{\"entities\":[{\"value\":\"Shibuya Crossing\",\"type\":\"address\",\"confidence\":\"high\",\"services\":[\"GEOCODING\",\"ADDRESS_RESOLVER\",\"REVERSE_GEOCODING\",\"WEATHER_SERVICE\"]}],\"recommended_services\":[\"GEOCODING\",\"ADDRESS_RESOLVER\",\"REVERSE_GEOCODING\"],\"complexity\":\"low\",\"analysis\":\"address entity\",\"chain_reason\":true}\n\n"
    "Query: weather at 1600 Pennsylvania Ave\n"
    "{\"entities\":[{\"value\":\"1600 Pennsylvania Ave\",\"type\":\"address\",\"confidence\":\"high\",\"services\":[\"GEOCODING\",\"ADDRESS_RESOLVER\",\"REVERSE_GEOCODING\",\"WEATHER_SERVICE\"]}],\"recommended_services\":[\"GEOCODING\",\"ADDRESS_RESOLVER\",\"REVERSE_GEOCODING\"],\"complexity\":\"low\",\"analysis\":\"address entity\",\"chain_reason\":true}\n\n"
    "Query: mac vendor 00:1A:2B:3C:4D:5E\n"
    "{\"entities\":[{\"value\":\"00:1A:2B:3C:4D:5E\",\"type\":\"mac\",\"confidence\":\"high\",\"services\":[\"MAC_VENDOR_LOOKUP\",\"WIFI_LOOKUP\"]}],\"recommended_services\":[\"MAC_VENDOR_LOOKUP\",\"WIFI_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"mac entity\",\"chain_reason\":false}\n\n"
    "Query: geolocate bssid 00:1A:2B:3C:4D:5E\n"
    "{\"entities\":[{\"value\":\"00:1A:2B:3C:4D:5E\",\"type\":\"mac\",\"confidence\":\"high\",\"services\":[\"MAC_VENDOR_LOOKUP\",\"WIFI_LOOKUP\"]}],\"recommended_services\":[\"MAC_VENDOR_LOOKUP\",\"WIFI_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"mac entity\",\"chain_reason\":false}\n\n"
    "Query: who makes device AC:DE:48:00:11:22\n"
    "{\"entities\":[{\"value\":\"AC:DE:48:00:11:22\",\"type\":\"mac\",\"confidence\":\"high\",\"services\":[\"MAC_VENDOR_LOOKUP\",\"WIFI_LOOKUP\"]}],\"recommended_services\":[\"MAC_VENDOR_LOOKUP\",\"WIFI_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"mac entity\",\"chain_reason\":false}\n\n"
    "Query: flight AA123\n"
    "{\"entities\":[{\"value\":\"AA123\",\"type\":\"flight\",\"confidence\":\"high\",\"services\":[\"FLIGHT_TRACKER\"]}],\"recommended_services\":[\"FLIGHT_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"flight entity\",\"chain_reason\":true}\n\n"
    "Query: track flight NH205\n"
    "{\"entities\":[{\"value\":\"NH205\",\"type\":\"flight\",\"confidence\":\"high\",\"services\":[\"FLIGHT_TRACKER\"]}],\"recommended_services\":[\"FLIGHT_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"flight entity\",\"chain_reason\":true}\n\n"
    "Query: where is BA456\n"
    "{\"entities\":[{\"value\":\"BA456\",\"type\":\"flight\",\"confidence\":\"high\",\"services\":[\"FLIGHT_TRACKER\"]}],\"recommended_services\":[\"FLIGHT_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"flight entity\",\"chain_reason\":true}\n\n"
    "Query: track vessel EVER GIVEN\n"
    "{\"entities\":[{\"value\":\"EVER GIVEN\",\"type\":\"vessel\",\"confidence\":\"high\",\"services\":[\"VESSEL_TRACKER\"]}],\"recommended_services\":[\"VESSEL_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"vessel entity\",\"chain_reason\":true}\n\n"
    "Query: ship by imo 9811000\n"
    "{\"entities\":[{\"value\":\"9811000\",\"type\":\"vessel\",\"confidence\":\"high\",\"services\":[\"VESSEL_TRACKER\"]}],\"recommended_services\":[\"VESSEL_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"vessel entity\",\"chain_reason\":true}\n\n"
    "Query: locate MMSI 353136000\n"
    "{\"entities\":[{\"value\":\"353136000\",\"type\":\"vessel\",\"confidence\":\"high\",\"services\":[\"VESSEL_TRACKER\"]}],\"recommended_services\":[\"VESSEL_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"vessel entity\",\"chain_reason\":true}\n\n"
    "Query: news about semiconductor export controls\n"
    "{\"entities\":[{\"value\":\"semiconductor export controls\",\"type\":\"keyword\",\"confidence\":\"high\",\"services\":[\"NEWS_AGGREGATOR\",\"NEWS_ARCHIVE\",\"JP_CORPUS_LOOKUP\",\"DARK_WEB_MONITOR\",\"PASTE_SITE_SEARCH\"]}],\"recommended_services\":[\"NEWS_AGGREGATOR\",\"NEWS_ARCHIVE\",\"JP_CORPUS_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"keyword entity\",\"chain_reason\":false}\n\n"
    "Query: dark web mentions of acme breach\n"
    "{\"entities\":[{\"value\":\"acme breach\",\"type\":\"keyword\",\"confidence\":\"high\",\"services\":[\"NEWS_AGGREGATOR\",\"NEWS_ARCHIVE\",\"JP_CORPUS_LOOKUP\",\"DARK_WEB_MONITOR\",\"PASTE_SITE_SEARCH\"]}],\"recommended_services\":[\"NEWS_AGGREGATOR\",\"NEWS_ARCHIVE\",\"JP_CORPUS_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"keyword entity\",\"chain_reason\":false}\n\n"
    "Query: search corpus for typhoon damage\n"
    "{\"entities\":[{\"value\":\"typhoon damage\",\"type\":\"keyword\",\"confidence\":\"high\",\"services\":[\"NEWS_AGGREGATOR\",\"NEWS_ARCHIVE\",\"JP_CORPUS_LOOKUP\",\"DARK_WEB_MONITOR\",\"PASTE_SITE_SEARCH\"]}],\"recommended_services\":[\"NEWS_AGGREGATOR\",\"NEWS_ARCHIVE\",\"JP_CORPUS_LOOKUP\"],\"complexity\":\"low\",\"analysis\":\"keyword entity\",\"chain_reason\":false}\n\n"
    "Query: track satellite 25544\n"
    "{\"entities\":[{\"value\":\"25544\",\"type\":\"satellite\",\"confidence\":\"high\",\"services\":[\"SATELLITE_TRACKER\"]}],\"recommended_services\":[\"SATELLITE_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"satellite entity\",\"chain_reason\":true}\n\n"
    "Query: position of NORAD 43013\n"
    "{\"entities\":[{\"value\":\"43013\",\"type\":\"satellite\",\"confidence\":\"high\",\"services\":[\"SATELLITE_TRACKER\"]}],\"recommended_services\":[\"SATELLITE_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"satellite entity\",\"chain_reason\":true}\n\n"
    "Query: where is sat 48274\n"
    "{\"entities\":[{\"value\":\"48274\",\"type\":\"satellite\",\"confidence\":\"high\",\"services\":[\"SATELLITE_TRACKER\"]}],\"recommended_services\":[\"SATELLITE_TRACKER\"],\"complexity\":\"low\",\"analysis\":\"satellite entity\",\"chain_reason\":true}\n\n"
    "Query: john barker, 0782327674, plate 5334DE2434, bob@x.com\n"
    "{\"entities\":[{\"value\":\"john barker\",\"type\":\"person\",\"confidence\":\"high\",\"services\":[\"PERSON_SEARCH\",\"SOCIAL_USERNAME\",\"SANCTIONS_CHECK\"]},{\"value\":\"0782327674\",\"type\":\"phone\",\"confidence\":\"high\",\"services\":[\"PHONE_LOOKUP\",\"CARRIER_LOOKUP\"]},{\"value\":\"5334DE2434\",\"type\":\"vehicle\",\"confidence\":\"high\",\"services\":[\"LICENSE_PLATE_LOOKUP\",\"VEHICLE_LOOKUP\"]},{\"value\":\"bob@x.com\",\"type\":\"email\",\"confidence\":\"high\",\"services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\"]}],\"recommended_services\":[\"PERSON_SEARCH\",\"PHONE_LOOKUP\",\"SOCIAL_EMAIL\"],\"complexity\":\"high\",\"analysis\":\"multiple entities\",\"chain_reason\":true}\n\n"
    "Query: can u find info on the email addy john.doe@gmail.com please\n"
    "{\"entities\":[{\"value\":\"john.doe@gmail.com\",\"type\":\"email\",\"confidence\":\"high\",\"services\":[\"SOCIAL_EMAIL\",\"DEHASHED_SEARCH\"]}],\"recommended_services\":[\"SOCIAL_EMAIL\"],\"complexity\":\"low\",\"analysis\":\"email (typos ignored)\",\"chain_reason\":true}\n\n"
    "Now output JSON for the query:\n");

  return sb_take(&b);
}

/* ── prompt_suggestions — createSuggestionsPrompt(query) ──────────────── */
char *prompt_suggestions(const char *query) {
  if (!query) query = "";
  sb b = {0};

  sb_add(&b,
    "You are an OSINT search assistant. Output ONLY a JSON array with 9 search suggestions.\n"
    "Do not explain or add any text before or after the JSON array.\n\n"
    "Examples:\n\n"
    "Query: \"John Smith\"\n"
    "[\"John Smith LinkedIn professional profile\",\"John Smith Facebook social media\",\"John Smith public records court documents\",\"John Smith email breach databases\",\"John Smith company affiliations\",\"John Smith Twitter account\",\"John Smith username search Sherlock\",\"John Smith academic publications\",\"John Smith phone reverse lookup\"]\n\n"
    "Query: \"contact@example.com\"\n"
    "[\"contact@example.com breach HaveIBeenPwned\",\"contact@example.com email reputation\",\"contact@example.com social accounts Holehe\",\"example.com WHOIS domain\",\"example.com DNS records\",\"example.com SSL certificates\",\"example.com company info\",\"contact@example.com paste sites\",\"contact@example.com username profiles\"]\n\n"
    "Query: \"192.168.1.1\"\n"
    "[\"192.168.1.1 geolocation ISP\",\"192.168.1.1 Shodan ports\",\"192.168.1.1 reverse DNS\",\"192.168.1.1 ASN lookup\",\"192.168.1.1 threat intelligence\",\"192.168.1.1 Censys scan\",\"192.168.1.1 WHOIS network\",\"192.168.1.1 historical data\",\"192.168.1.1 port scanning\"]\n\n");
  sb_add(&b, "Query: \"");
  sb_add(&b, query);
  sb_add(&b, "\"\n");

  return sb_take(&b);
}

/* ── prompt_synthesis — final narrative analysis (NOT a JS port) ───────────
 * pipeline.js ended with a static counts string; this asks the model to write
 * the real conclusion from the gathered data. Plain prose out. */
char *prompt_synthesis(const char *query, const char *results_json) {
  if (!query) query = "";
  if (!results_json) results_json = "";
  sb b = {0};

  sb_add(&b, "You are an OSINT analyst writing the FINAL summary of a "
             "completed investigation.\n\n");
  sb_add(&b, "ORIGINAL QUERY: \"");
  sb_add(&b, query);
  sb_add(&b, "\"\n\n");
  sb_add(&b, "GATHERED SERVICE RESULTS (JSON — each entry is one service call "
             "with its returned data, or an error/empty payload):\n");
  sb_add(&b, results_json);
  sb_add(&b, "\n\n");
  sb_add(&b,
    "Write a concise intelligence summary (2-5 sentences) that:\n"
    "- Directly answers the user's query using ONLY the data actually "
    "returned above.\n"
    "- States the concrete findings (locations, coordinates, names, weather, "
    "owners, etc.) and attributes each to the service that produced it.\n"
    "- Briefly notes which services returned no data, without speculating "
    "about why or inventing results.\n"
    "- NEVER fabricates facts, values, or entities that are not present in the "
    "results JSON. If nothing substantive was found, say so plainly.\n\n"
    "Output plain prose only — no JSON, no markdown headers, no bullet list, "
    "no preamble like \"Summary:\". Just the analysis.\n");

  return sb_take(&b);
}

/* ── prompt_phase2 — createPhase2Prompt(originalQuery, resultsJson, …) ── */
char *prompt_phase2(const char *query, const char *results_json,
                    const char *services_list) {
  const char *list = (services_list && services_list[0])
                       ? services_list : "(service list unavailable)";
  if (!query) query = "";
  if (!results_json) results_json = "";
  sb b = {0};

  sb_add(&b, "You are an OSINT service orchestrator analyzing Phase 1 results.\n\n");
  sb_add(&b, "ORIGINAL QUERY: \"");
  sb_add(&b, query);
  sb_add(&b, "\"\n\n");
  sb_add(&b, "PHASE 1 RESULTS:\n");
  sb_add(&b, results_json);
  sb_add(&b, "\n\n");
  sb_add(&b,
    "TASK: Determine if additional services should be called based on:\n"
    "1. The original query intent (what did the user actually want?)\n"
    "2. Data extracted in Phase 1 (coordinates, domains, emails, etc.)\n\n"
    "SERVICE CHAINING RULES - CRITICAL:\n"
    "- WEATHER_SERVICE: If query mentions 'weather' and IP_GEOLOCATION returned lat/lon,\n"
    "  extract coordinates and pass as 'lat,lon' format (e.g., '37.38,-122.08').\n"
    "- GEOCODING/REVERSE_GEOCODING: If query mentions 'map', 'nearby', 'location',\n"
    "  extract coordinates from IP_GEOLOCATION results.\n"
    "- DOMAIN_WHOIS: Extract domain from email addresses or URLs found in results.\n"
    "- EMAIL_HARVESTER: Extract domain from email to find more addresses at same org.\n"
    "- SOCIAL_EMAIL: Pass email addresses found in social lookups or person searches.\n"
    "- COMPANY_LOOKUP: Extract employer/company names from PERSON_SEARCH or breach results.\n"
    "- SEC_EDGAR_SEARCH: Extract CIK numbers from COMPANY_LOOKUP results for SEC filings.\n"
    "- SUBDOMAIN_FINDER: Extract base domain from URLs or emails to find subdomains.\n"
    "- PERSON_SEARCH: Extract names from social profile results or breach data.\n"
    "- THREAT_INTEL: Pass IP addresses resolved from domains or found in logs.\n"
    "- ASN_LOOKUP: Pass IP addresses to get network/ISP information.\n\n"
    "IMPORTANT: Extract VALUES from Phase 1 results. Do NOT re-pass original query!\n\n");
  sb_add(&b, ENTITY_TYPES_PROMPT);
  sb_add(&b,
    "\nExtract entities of these types from the results above.\n\n");
  sb_add(&b, "AVAILABLE SERVICES:\n");
  sb_add(&b, list);
  sb_add(&b, "\n\n");
  sb_add(&b,
    "EXAMPLES:\n\n"
    "IMPORTANT: source_service is REQUIRED - it indicates which service's results the entity came from!\n\n"
    "1. Weather from IP:\n"
    "Query: 'weather at 8.8.8.8'\n"
    "Phase 1 IP_GEOLOCATION: {\"latitude\": 37.38, \"longitude\": -122.08}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Extract coords for weather\","
    "\"chain_services\":[{\"service\":\"WEATHER_SERVICE\",\"entity\":\"37.38,-122.08\",\"entity_type\":\"coords\",\"source_service\":\"IP_GEOLOCATION\"}]}\n\n"
    "2. Email breach to company:\n"
    "Query: 'investigate john@acme.com'\n"
    "Phase 1 SOCIAL_EMAIL: {\"breaches\":[\"LinkedIn2021\"],\"employer\":\"Acme Corp\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Investigate employer from breach\","
    "\"chain_services\":[{\"service\":\"COMPANY_LOOKUP\",\"entity\":\"Acme Corp\",\"entity_type\":\"company\",\"source_service\":\"SOCIAL_EMAIL\"}]}\n\n"
    "3. Domain WHOIS to registrant:\n"
    "Query: 'who owns evil.com'\n"
    "Phase 1 DOMAIN_WHOIS: {\"registrant_email\":\"admin@privacyguard.io\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Harvest emails from registrant domain\","
    "\"chain_services\":[{\"service\":\"EMAIL_HARVESTER\",\"entity\":\"privacyguard.io\",\"entity_type\":\"domain\",\"source_service\":\"DOMAIN_WHOIS\"}]}\n\n"
    "4. Username to breach:\n"
    "Query: 'find accounts for hacker123'\n"
    "Phase 1 SOCIAL_USERNAME: {\"found_email\":\"hacker123@gmail.com\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Check discovered email for breaches\","
    "\"chain_services\":[{\"service\":\"SOCIAL_EMAIL\",\"entity\":\"hacker123@gmail.com\",\"entity_type\":\"email\",\"source_service\":\"SOCIAL_USERNAME\"}]}\n\n"
    "5. Company to SEC filings:\n"
    "Query: 'investigate TechCorp financial'\n"
    "Phase 1 COMPANY_LOOKUP: {\"cik\":\"0001234567\",\"name\":\"TechCorp Inc\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Get SEC filings for company\","
    "\"chain_services\":[{\"service\":\"SEC_EDGAR_SEARCH\",\"entity\":\"0001234567\",\"entity_type\":\"cik\",\"source_service\":\"COMPANY_LOOKUP\"}]}\n\n"
    "6. Phone to person:\n"
    "Query: 'who owns +1-555-123-4567'\n"
    "Phase 1 PHONE_LOOKUP: {\"carrier\":\"Verizon\",\"possible_name\":\"John Doe\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Search for identified person\","
    "\"chain_services\":[{\"service\":\"PERSON_SEARCH\",\"entity\":\"John Doe\",\"entity_type\":\"person\",\"source_service\":\"PHONE_LOOKUP\"}]}\n\n"
    "7. Map/nearby from IP:\n"
    "Query: 'map location of 192.168.1.1'\n"
    "Phase 1 IP_GEOLOCATION: {\"latitude\": 40.71, \"longitude\": -74.00, \"city\": \"New York\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Extract coords for map/location services\","
    "\"chain_services\":[{\"service\":\"REVERSE_GEOCODING\",\"entity\":\"40.71,-74.00\",\"entity_type\":\"coords\",\"source_service\":\"IP_GEOLOCATION\"}]}\n\n"
    "8. Phone to social accounts:\n"
    "Query: 'find accounts for 0612345678'\n"
    "Phase 1 PHONE_LOOKUP: {\"carrier\":\"Orange\",\"country\":\"France\",\"line_type\":\"mobile\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Search social accounts linked to phone\","
    "\"chain_services\":[{\"service\":\"SOCIAL_USERNAME\",\"entity\":\"0612345678\",\"entity_type\":\"phone\",\"source_service\":\"PHONE_LOOKUP\"}]}\n\n"
    "9. Phone revealed email:\n"
    "Query: 'investigate phone 0781218793'\n"
    "Phase 1 PHONE_LOOKUP: {\"associated_email\":\"john.smith@gmail.com\",\"carrier\":\"SFR\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Check discovered email for breaches\","
    "\"chain_services\":[{\"service\":\"SOCIAL_EMAIL\",\"entity\":\"john.smith@gmail.com\",\"entity_type\":\"email\",\"source_service\":\"PHONE_LOOKUP\"}]}\n\n"
    "10. Phone found in breach data:\n"
    "Query: 'investigate john@corp.com'\n"
    "Phase 1 SOCIAL_EMAIL: {\"breaches\":[\"Facebook2019\"],\"phone_exposed\":\"+33612345678\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Investigate exposed phone number\","
    "\"chain_services\":[{\"service\":\"PHONE_LOOKUP\",\"entity\":\"+33612345678\",\"entity_type\":\"phone\",\"source_service\":\"SOCIAL_EMAIL\"},"
    "{\"service\":\"CARRIER_LOOKUP\",\"entity\":\"+33612345678\",\"entity_type\":\"phone\",\"source_service\":\"SOCIAL_EMAIL\"}]}\n\n"
    "11. Vehicle lookup to owner:\n"
    "Query: 'who owns plate ABC123'\n"
    "Phase 1 LICENSE_PLATE_LOOKUP: {\"plate\":\"ABC123\",\"owner_name\":\"Marie Dupont\",\"city\":\"Paris\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Search for vehicle owner\","
    "\"chain_services\":[{\"service\":\"PERSON_SEARCH\",\"entity\":\"Marie Dupont\",\"entity_type\":\"person\",\"source_service\":\"LICENSE_PLATE_LOOKUP\"}]}\n\n"
    "12. Flight to airport geolocation:\n"
    "Query: 'track flight AF123'\n"
    "Phase 1 FLIGHT_TRACKER: {\"flight\":\"AF123\",\"origin\":\"CDG\",\"destination\":\"JFK\",\"origin_coords\":{\"lat\":49.01,\"lon\":2.55}}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Get location details for airports\","
    "\"chain_services\":[{\"service\":\"REVERSE_GEOCODING\",\"entity\":\"49.01,2.55\",\"entity_type\":\"coords\",\"source_service\":\"FLIGHT_TRACKER\"}]}\n\n"
    "13. Person search found phone:\n"
    "Query: 'find info on John Smith'\n"
    "Phase 1 PERSON_SEARCH: {\"name\":\"John Smith\",\"phone\":\"+1-555-987-6543\",\"email\":\"jsmith@example.com\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Investigate discovered phone and email\","
    "\"chain_services\":[{\"service\":\"PHONE_LOOKUP\",\"entity\":\"+1-555-987-6543\",\"entity_type\":\"phone\",\"source_service\":\"PERSON_SEARCH\"},"
    "{\"service\":\"SOCIAL_EMAIL\",\"entity\":\"jsmith@example.com\",\"entity_type\":\"email\",\"source_service\":\"PERSON_SEARCH\"}]}\n\n"
    "14. Username found multiple accounts:\n"
    "Query: 'search username darkuser99'\n"
    "Phase 1 SOCIAL_USERNAME: {\"profiles\":[\"twitter.com/darkuser99\",\"github.com/darkuser99\"],\"possible_email\":\"darkuser99@proton.me\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Check email and analyze social profiles\","
    "\"chain_services\":[{\"service\":\"SOCIAL_EMAIL\",\"entity\":\"darkuser99@proton.me\",\"entity_type\":\"email\",\"source_service\":\"SOCIAL_USERNAME\"},"
    "{\"service\":\"GITHUB_CODE_SEARCH\",\"entity\":\"darkuser99\",\"entity_type\":\"username\",\"source_service\":\"SOCIAL_USERNAME\"}]}\n\n"
    "15. Vessel to port/company:\n"
    "Query: 'track vessel IMO 9074729'\n"
    "Phase 1 VESSEL_TRACKER: {\"imo\":\"9074729\",\"name\":\"Ever Given\",\"owner\":\"Evergreen Marine\",\"destination_port\":\"Rotterdam\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Investigate vessel owner company\","
    "\"chain_services\":[{\"service\":\"COMPANY_LOOKUP\",\"entity\":\"Evergreen Marine\",\"entity_type\":\"company\",\"source_service\":\"VESSEL_TRACKER\"}]}\n\n"
    "16. Email domain to subdomains:\n"
    "Query: 'investigate contact@suspicious.io'\n"
    "Phase 1 SOCIAL_EMAIL: {\"email\":\"contact@suspicious.io\",\"breaches\":[],\"domain\":\"suspicious.io\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Investigate email domain for more intel\","
    "\"chain_services\":[{\"service\":\"SUBDOMAIN_FINDER\",\"entity\":\"suspicious.io\",\"entity_type\":\"domain\",\"source_service\":\"SOCIAL_EMAIL\"},"
    "{\"service\":\"DOMAIN_WHOIS\",\"entity\":\"suspicious.io\",\"entity_type\":\"domain\",\"source_service\":\"SOCIAL_EMAIL\"}]}\n\n"
    "17. Crypto address to transactions:\n"
    "Query: 'investigate wallet 0x742d35Cc6634C0532925a3b844Bc9e7595f'\n"
    "Phase 1 CRYPTO_TRACKER: {\"address\":\"0x742d35Cc6634C0532925a3b844Bc9e7595f\",\"balance\":\"1.5 ETH\",\"exchange_deposit\":\"Binance\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Track exchange connections\","
    "\"chain_services\":[{\"service\":\"EXCHANGE_FLOW\",\"entity\":\"0x742d35Cc6634C0532925a3b844Bc9e7595f\",\"entity_type\":\"crypto\",\"source_service\":\"CRYPTO_TRACKER\"}]}\n\n"
    "=== RECURSIVE CHAINING (Phase 2 can trigger Phase 3) ===\n"
    "18. Person found employer -> investigate company:\n"
    "Query: 'investigate John Doe'\n"
    "Phase 2 PERSON_SEARCH: {\"employer\":\"TechCorp Inc\",\"linkedin\":\"linkedin.com/in/johndoe\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Investigate discovered employer\","
    "\"chain_services\":[{\"service\":\"COMPANY_LOOKUP\",\"entity\":\"TechCorp Inc\",\"entity_type\":\"company\",\"source_service\":\"PERSON_SEARCH\"}]}\n\n"
    "19. Breach revealed email -> check more breaches:\n"
    "Query: 'investigate user hacker123'\n"
    "Phase 2 SOCIAL_USERNAME: {\"emails_found\":[\"hacker123@proton.me\",\"h4ck3r@gmail.com\"]}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Check discovered emails for breaches\","
    "\"chain_services\":[{\"service\":\"SOCIAL_EMAIL\",\"entity\":\"hacker123@proton.me\",\"entity_type\":\"email\",\"source_service\":\"SOCIAL_USERNAME\"},"
    "{\"service\":\"SOCIAL_EMAIL\",\"entity\":\"h4ck3r@gmail.com\",\"entity_type\":\"email\",\"source_service\":\"SOCIAL_USERNAME\"}]}\n\n"
    "20. Company revealed executives -> search persons:\n"
    "Query: 'investigate Acme Corp'\n"
    "Phase 2 COMPANY_LOOKUP: {\"executives\":[\"Jane Smith (CEO)\",\"Bob Wilson (CFO)\"]}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Search discovered executives\","
    "\"chain_services\":[{\"service\":\"PERSON_SEARCH\",\"entity\":\"Jane Smith\",\"entity_type\":\"person\",\"source_service\":\"COMPANY_LOOKUP\"},"
    "{\"service\":\"PERSON_SEARCH\",\"entity\":\"Bob Wilson\",\"entity_type\":\"person\",\"source_service\":\"COMPANY_LOOKUP\"}]}\n\n"
    "21. Phone investigation revealed address -> geocode:\n"
    "Query: 'investigate 0612345678'\n"
    "Phase 2 PHONE_LOOKUP: {\"owner\":\"Pierre Martin\",\"address\":\"123 Rue de Paris, Lyon, France\"}\n"
    "Output: {\"needs_newphase\":true,\"reason\":\"Geocode discovered address\","
    "\"chain_services\":[{\"service\":\"GEOCODING\",\"entity\":\"123 Rue de Paris, Lyon, France\",\"entity_type\":\"address\",\"source_service\":\"PHONE_LOOKUP\"},"
    "{\"service\":\"PERSON_SEARCH\",\"entity\":\"Pierre Martin\",\"entity_type\":\"person\",\"source_service\":\"PHONE_LOOKUP\"}]}\n\n"
    "=== NEGATIVE EXAMPLES (needs_newphase: false) ===\n"
    "22. Simple IP lookup - no chainable data:\n"
    "Query: 'what is 8.8.8.8'\n"
    "Phase 1 IP_GEOLOCATION: {\"ip\":\"8.8.8.8\",\"country\":\"US\",\"city\":\"Mountain View\"}\n"
    "Output: {\"needs_newphase\":false,\"reason\":\"Simple lookup complete, no follow-up needed\",\"chain_services\":[]}\n\n"
    "23. Domain lookup - informational only:\n"
    "Query: 'lookup domain google.com'\n"
    "Phase 1 DOMAIN_WHOIS: {\"registrar\":\"MarkMonitor\",\"created\":\"1997-09-15\"}\n"
    "Output: {\"needs_newphase\":false,\"reason\":\"Domain info retrieved, no personal data to chain\",\"chain_services\":[]}\n\n"
    "24. DNS records - technical data:\n"
    "Query: 'dns records for example.org'\n"
    "Phase 1 DNS_RECORDS: {\"A\":[\"93.184.216.34\"],\"MX\":[\"mail.example.org\"]}\n"
    "Output: {\"needs_newphase\":false,\"reason\":\"Technical DNS data, no entities to investigate\",\"chain_services\":[]}\n\n"
    "25. Hash lookup - no personal info:\n"
    "Query: 'hash sha256 abc123def456'\n"
    "Phase 1 HASH_LOOKUP: {\"malware\":false,\"file_type\":\"PDF\",\"detections\":0}\n"
    "Output: {\"needs_newphase\":false,\"reason\":\"Hash analysis complete, no chainable entities\",\"chain_services\":[]}\n\n"
    "26. Weather already retrieved - chain complete:\n"
    "Query: 'weather at 8.8.8.8'\n"
    "Phase 2 WEATHER_SERVICE: {\"temperature\":72,\"conditions\":\"Sunny\",\"location\":\"Mountain View\"}\n"
    "Output: {\"needs_newphase\":false,\"reason\":\"Weather data retrieved, investigation complete\",\"chain_services\":[]}\n\n"
    "27. Phone lookup complete - no additional entities:\n"
    "Query: 'carrier for 0612345678'\n"
    "Phase 1 CARRIER_LOOKUP: {\"carrier\":\"Orange\",\"line_type\":\"mobile\",\"country\":\"France\"}\n"
    "Output: {\"needs_newphase\":false,\"reason\":\"Carrier info retrieved, no personal data found\",\"chain_services\":[]}\n\n"
    "28. Simple phone info - no owner found:\n"
    "Query: 'lookup 0781218793'\n"
    "Phase 1 PHONE_LOOKUP: {\"valid\":true,\"carrier\":\"SFR\",\"type\":\"mobile\",\"owner\":null}\n"
    "Output: {\"needs_newphase\":false,\"reason\":\"Phone validated but no chainable entities found\",\"chain_services\":[]}\n\n"
    "OUTPUT FORMAT (JSON):\n"
    "{\n"
    "  \"needs_newphase\": true/false,\n"
    "  \"reason\": \"why follow-up is/isn't needed\",\n"
    "  \"chain_services\": [\n"
    "    {\"service\": \"SERVICE_NAME\", \"entity\": \"extracted value\", \"entity_type\": \"type\", \"source_service\": \"SERVICE_THAT_FOUND_ENTITY\"}\n"
    "  ]\n"
    "}\n\n"
    "NOTE: source_service is REQUIRED for each chain_services entry - it tracks which Phase 1 service discovered the entity.\n\n"
    "Set needs_newphase=false and empty chain_services when:\n"
    "- The original query intent has been satisfied (weather retrieved, info found)\n"
    "- Results contain only technical/informational data (DNS, hashes, simple lookups)\n"
    "- No new personal entities (names, emails, companies) were discovered\n"
    "- All discovered entities have already been investigated\n\n"
    "Output JSON:");

  return sb_take(&b);
}

/* ── grammar_load — loadGrammar(name) ─────────────────────────────────────
 * JS reads <repo>/server/grammars/<name>.gbnf once and caches forever,
 * returning '' on miss. We mirror that: a fixed table of the 4 known names,
 * each slot lazily slurped and retained. Unknown names -> "". The buffers are
 * owned here for the process lifetime (bounded: 4 small files), never freed —
 * matching the JS Map cache contract (caller must not free). */
/* ── prompt_entity_extraction — llmPrompts.js buildEntityExtractionPrompt ─ */
#define ENTITY_EXTRACTION_BODY_CLIP 4000

char *prompt_entity_extraction(const char *title, const char *body,
                               const char *summary, const char *language,
                               const char *source_id) {
  /* text = [title, summary||body].filter(Boolean).join('\n\n').slice(0,4000) */
  const char *second = (summary && *summary) ? summary
                       : ((body && *body) ? body : NULL);
  sb tb = {0};
  if (title && *title) sb_add(&tb, title);
  if (title && *title && second) sb_add(&tb, "\n\n");
  if (second) sb_add(&tb, second);
  char *text = sb_take(&tb);
  if (text) {                                   /* clip 4000, UTF-8 safe */
    size_t L = strlen(text);
    if (L > ENTITY_EXTRACTION_BODY_CLIP) {
      size_t cut = ENTITY_EXTRACTION_BODY_CLIP;
      while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) cut--;
      text[cut] = '\0';
    }
  }
  sb b = {0};
  sb_add(&b, "Extract OSINT entities from the intelligence item below.\n\n");
  sb_add(&b, "CRITICAL: The entity VALUE must contain ONLY the identifier "
             "itself, NEVER context words. Output compact JSON.\n\n");
  sb_add(&b, ENTITY_TYPES_PROMPT);
  sb_add(&b, "\nFor Japanese sources, prefer the form as written (kanji/kana) "
             "for the value; the system links kanji/romaji variants "
             "separately.\n\n");
  sb_add(&b, "Source: ");
  sb_add(&b, (source_id && *source_id) ? source_id : "(unknown)");
  sb_add(&b, "\nLanguage: ");
  sb_add(&b, (language && *language) ? language : "auto");
  sb_add(&b, "\n\nContent:\n");
  sb_add(&b, text ? text : "");
  sb_add(&b, "\n\nReturn JSON: {\"entities\":[{\"value\":\"...\",\"type\":"
             "\"...\",\"confidence\":\"high|medium|low\",\"source\":"
             "\"...\"}]}");
  free(text);
  return sb_take(&b);
}

/* ── prompt_entity_dedup — entityExtractor.js buildEntityDedupPrompt ────── */
char *prompt_entity_dedup(const char *type, const char *canon_a,
                          const char *canon_b) {
  /* Flat completion prompt for llm_complete (raw /completion path, no chat
     template). The former system role is folded in as a leading instruction;
     no JSON escaping since values are no longer embedded in a JSON string. */
  sb b = {0};
  sb_add(&b, "You decide whether two extracted OSINT entities of the same "
             "type refer to the SAME real-world subject. Account for Japanese "
             "kanji/kana vs romaji spellings, company suffixes, and "
             "transliteration drift. Output JSON only.\n\nType: ");
  sb_add(&b, type ? type : "");
  sb_add(&b, "\nA: ");
  sb_add(&b, canon_a ? canon_a : "");
  sb_add(&b, "\nB: ");
  sb_add(&b, canon_b ? canon_b : "");
  sb_add(&b, "\n\nSame subject? Respond with JSON only: "
             "{\"same\": true|false, \"confidence\": 0.0-1.0, "
             "\"reason\": \"...\"}");
  return sb_take(&b);
}

static const char *const GRAMMAR_NAMES[] = {
  "osint_analysis", "entity_extraction", "page_analysis", "suggestions",
  /* maintenance pod (collector-repair) grammars */
  "triage_classification", "repair_proposal", "repair_sanity"
};
#define GRAMMAR_COUNT (int)(sizeof(GRAMMAR_NAMES) / sizeof(GRAMMAR_NAMES[0]))

const char *grammar_load(const char *name) {
  /* cached[i]: NULL = not yet read; otherwise the retained content (possibly
   * the empty string "" for a missing/unreadable file, cached like JS). */
  static char *cached[GRAMMAR_COUNT];

  if (!name) return "";
  int idx = -1;
  for (int i = 0; i < GRAMMAR_COUNT; i++)
    if (strcmp(GRAMMAR_NAMES[i], name) == 0) { idx = i; break; }
  if (idx < 0) return "";                 /* unknown name: '' like JS */
  if (cached[idx]) return cached[idx];    /* cache hit (incl. cached "") */

  char path[1024];
  snprintf(path, sizeof path, "%s/grammars/%s.gbnf",
           JO_REPO_ROOT, name);

  char *content = NULL;
  FILE *f = fopen(path, "rb");
  if (f) {
    if (fseek(f, 0, SEEK_END) == 0) {
      long n = ftell(f);
      if (n >= 0 && fseek(f, 0, SEEK_SET) == 0) {
        char *buf = malloc((size_t)n + 1);
        if (buf) {
          size_t rd = fread(buf, 1, (size_t)n, f);
          buf[rd] = '\0';
          content = buf;
        }
      }
    }
    fclose(f);
  }
  if (!content) {
    /* missing/unreadable -> cache "" (own a private copy so the slot is
     * uniformly heap-allocated and stable for the process lifetime). */
    content = malloc(1);
    if (!content) return "";              /* OOM: don't poison the cache */
    content[0] = '\0';
  }
  cached[idx] = content;
  return content;
}

/* ── schema_load — JSON-schema sibling of grammar_load ────────────────────
 * Reads <repo>/grammars/<name>.schema.json once and caches forever. Used with
 * llm_chat's response_format so a reasoning model (gpt-oss) is constrained on
 * its FINAL channel only: unlike a raw GBNF grammar (which engages from the
 * first token and suppresses the analysis channel — the degenerate-reasoning /
 * empty-entities failure), llama.cpp applies a json_schema lazily once the
 * harmony final channel begins, so the model still reasons AND conforms. Same
 * name table and cache contract as grammar_load (never free the result). */
const char *schema_load(const char *name) {
  static char *cached[GRAMMAR_COUNT];

  if (!name) return "";
  int idx = -1;
  for (int i = 0; i < GRAMMAR_COUNT; i++)
    if (strcmp(GRAMMAR_NAMES[i], name) == 0) { idx = i; break; }
  if (idx < 0) return "";
  if (cached[idx]) return cached[idx];

  char path[1024];
  snprintf(path, sizeof path, "%s/grammars/%s.schema.json",
           JO_REPO_ROOT, name);

  char *content = NULL;
  FILE *f = fopen(path, "rb");
  if (f) {
    if (fseek(f, 0, SEEK_END) == 0) {
      long n = ftell(f);
      if (n >= 0 && fseek(f, 0, SEEK_SET) == 0) {
        char *buf = malloc((size_t)n + 1);
        if (buf) {
          size_t rd = fread(buf, 1, (size_t)n, f);
          buf[rd] = '\0';
          content = buf;
        }
      }
    }
    fclose(f);
  }
  if (!content) {
    content = malloc(1);
    if (!content) return "";
    content[0] = '\0';
  }
  cached[idx] = content;
  return content;
}
