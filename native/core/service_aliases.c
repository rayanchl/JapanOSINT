/* core/service_aliases.c — see service_aliases.h for the rules. Every id here
 * is checked against the registry by tests/unit/test_service_aliases.c, and
 * every phrase restates what that source's own description says it answers
 * (read off `japanosint --dump-source-cards` when this was written): DNS_RECORDS
 * resolves A/AAAA only, so it is not "mail servers"; HISTORICAL_WHOIS finds
 * domains BY registrant, so it is "reverse whois", not "past owners". */
#include "service_aliases.h"
#include <string.h>

static const service_alias ALIASES[] = {
  { "DOMAIN_WHOIS",
    "who owns a domain, registrar of a domain, name servers of a domain, "
    "domain registration date, domain expiry" },
  { "HISTORICAL_WHOIS",
    "reverse whois, other domains registered by the same registrant" },
  { "DOMAIN_HISTORY",
    "registration and expiry dates of a domain over time" },
  { "DOMAIN_AGE",
    "how old is a domain, when was a domain first registered" },
  { "DNS_RECORDS",
    "what IP address does a domain point to, resolve a hostname" },
  { "REVERSE_DNS",
    "hostname of an IP address, what domain is on this IP" },
  { "IP_GEOLOCATION",
    "where is an IP address, which country or city an IP is in" },
  { "IP_REPUTATION",
    "is this IP malicious, abuse reports for an IP address" },
  { "ASN_LOOKUP",
    "which network or ISP owns an IP, autonomous system of an address" },
  { "SUBDOMAIN_FINDER",
    "subdomains of a domain, hosts under a domain" },
  { "CERTIFICATE_TRANSPARENCY",
    "TLS certificates issued for a domain, subdomains seen in certificates" },
  { "WAYBACK_MACHINE",
    "old versions of a website, archived copies of a page" },
  { "HASH_LOOKUP",
    "is this file hash malware, antivirus verdict for a hash" },
  { "COMPANY_LOOKUP",
    "company registration details, corporate registry record" },
  { "COMPANY_SEARCH",
    "find a company by name, company officers, business registry search" },
  { "LEI_SEARCH",
    "legal entity identifier of a company, LEI code" },
  { "SANCTIONS_CHECK",
    "is this person or company sanctioned, sanctions screening, watchlist check" },
  { "PEP_CHECK",
    "is this person a politically exposed person, politician screening" },
  { "PHONE_LOOKUP",
    "which carrier and country a phone number belongs to, is a phone number valid" },
  { "SOCIAL_USERNAME",
    "accounts using this username, find a person's social media profiles" },
  { "SOCIAL_EMAIL",
    "accounts registered with an email address" },
  { "BREACH_INDEX_EMAIL",
    "has this email been leaked, email in a data breach" },
  { "VESSEL_TRACKER",
    "where is a ship, vessel position, track a ship by name or MMSI" },
  { "FLIGHT_TRACKER",
    "where is a flight, aircraft position, track a plane" },
  { "REVERSE_GEOCODING",
    "address of a coordinate, what place is at this latitude and longitude" },
  { "TRADEMARK_SEARCH",
    "who owns a trademark, registered brand names" },
  { "PATENT_SEARCH",
    "find granted patents by title" },
};

const char *service_alias_for(const char *id) {
  if (!id) return NULL;
  for (size_t i = 0; i < sizeof ALIASES / sizeof *ALIASES; i++)
    if (!strcmp(ALIASES[i].id, id)) return ALIASES[i].phrases;
  return NULL;
}

int service_aliases_all(const service_alias **out) {
  if (out) *out = ALIASES;
  return (int)(sizeof ALIASES / sizeof *ALIASES);
}
