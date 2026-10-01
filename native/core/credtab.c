#include "credtab.h"
#include <string.h>
#include <stddef.h>

static const cred_def CREDS[] = {
  {"shodan-iot",          {"SHODAN_API_KEY"},{0},{0}},
  {"shodan-japan",        {"SHODAN_API_KEY"},{0},{0}},
  {"shodan-cameras-jp",   {"SHODAN_API_KEY"},{0},{0}},
  {"wifi-networks-wigle", {"WIGLE_API_KEY"},{0},{0}},
  {"wifi-networks-shodan",{"SHODAN_API_KEY"},{0},{0}},
  {"wifi-networks-mls",   {"MLS_API_KEY"},{0},{0}},
  {"fofa-jp",             {"FOFA_API_KEY"},{0},{0}},
  {"greynoise-jp",        {"GREYNOISE_API_KEY"},{0},{0}},
  {"quake360-jp",         {"QUAKE_API_KEY"},{0},{0}},
  {"grayhat-buckets",     {"GRAYHAT_API_KEY"},{0},{0}},
  {"trickest-cve",        {0},{0},{"GITHUB_TOKEN"}},
  {"poc-in-github",       {0},{0},{"GITHUB_TOKEN"}},
  {"github-leaks-jp",     {0},{0},{"GITHUB_TOKEN"}},
  {"ghsa-advisories",     {0},{0},{"GITHUB_TOKEN"}},
  {"ev-charging",         {0},{0},{"OPENCHARGEMAP_KEY"}},
  {"twitter-geo",         {"TWITTER_BEARER_TOKEN"},{0},{0}},
  {"facebook-geo",        {"FACEBOOK_ACCESS_TOKEN"},{0},{0}},
  {"marine-traffic",      {"MARINETRAFFIC_API_KEY"},{0},{0}},
  {"vessel-finder",       {"VESSELFINDER_API_KEY"},{0},{0}},
  {"maritime-ais",        {0},{"MARINETRAFFIC_API_KEY","VESSELFINDER_API_KEY"},{0}},
  {"msil-umishiru",       {"UMISHIRU_API_KEY"},{0},{0}},
  {"flight-adsb",         {0},{0},{"OPENSKY_CLIENT_ID","OPENSKY_CLIENT_SECRET","AERODATABOX_KEY"}},
  {"estat-population",    {0},{"ESTAT_API_KEY","ESTAT_APP_ID"},{0}},
  {"resas-industry",      {"RESAS_API_KEY"},{0},{0}},
  {"resas-tourism",       {"RESAS_API_KEY"},{0},{0}},
  {"resas-municipality",  {"RESAS_API_KEY"},{0},{0}},
  {"odpt-train",          {0},{"ODPT_TOKEN","ODPT_CONSUMER_KEY","ODPT_CHALLENGE_TOKEN"},{0}},
  {"odpt-bus",            {0},{"ODPT_TOKEN","ODPT_CONSUMER_KEY","ODPT_CHALLENGE_TOKEN"},{0}},
  {"odpt-station",        {0},{"ODPT_TOKEN","ODPT_CONSUMER_KEY","ODPT_CHALLENGE_TOKEN"},{0}},
  {"sentinel-japan",      {"SENTINELHUB_CLIENT_ID","SENTINELHUB_CLIENT_SECRET"},{0},{0}},
  {"satellite-imagery",   {0},{0},{"USGS_M2M_TOKEN"}},
  {"tabelog-restaurants", {"HOTPEPPER_API_KEY"},{0},{0}},
  {"google-my-maps",      {"GOOGLE_MYMAPS_IDS"},{0},{0}},
  {"cell-towers",         {"OPENCELLID_KEY"},{0},{0}},
  {"mlit-n02-stations",   {0},{0},{"MLIT_N02_GEOJSON_URL"}},
  {"edinet-filings",      {"EDINET_API_KEY"},{0},{0}},
  {"misskey-timeline",    {"MISSKEY_TOKEN"},{0},{0}},
  {"cam-windy_api",       {"WINDY_API_KEY"},{0},{0}},
  {"cam-youtube_live",    {"YOUTUBE_API_KEY"},{0},{0}},
  /* ── OSINTsaas-ported entity-pivot services (Batches 1–5) ── */
  {"PHONE_LOOKUP",        {0},{0},{"NUMVERIFY_API_KEY"}},
  {"CARRIER_LOOKUP",      {0},{0},{"NUMVERIFY_API_KEY"}},
  {"PHONE_REPUTATION",    {0},{0},{"NUMVERIFY_API_KEY"}},
  {"PERSON_SEARCH",       {0},{0},{"GITHUB_TOKEN","OPENSANCTIONS_API_KEY","COMPANIES_HOUSE_API_KEY"}},
  {"COMPANY_SEARCH",      {0},{0},{"COMPANIES_HOUSE_API_KEY","OPENCORPORATES_API_KEY"}},
  {"CREDENTIAL_LEAK_SEARCH",{0},{"GITHUB_TOKEN","GITHUB_API_TOKEN"},{0}},
  {"PEP_CHECK",           {"OPENSANCTIONS_API_KEY"},{0},{0}},
  {"WATCHLIST_CHECK_NEW", {"OPENSANCTIONS_API_KEY"},{0},{0}},
  {"PATENT_SEARCH",       {"PATENTSVIEW_API_KEY"},{0},{0}},
  {"DEFI_TRACKER",        {"ETHERSCAN_API_KEY"},{0},{0}},
  {"EXCHANGE_FLOW",       {"ETHERSCAN_API_KEY"},{0},{0}},
  {"SOCIAL_USERNAME",     {0},{0},{"GITHUB_TOKEN","TWITTER_BEARER_TOKEN","FACEBOOK_ACCESS_TOKEN"}},
  {"SOCIAL_EMAIL",        {0},{0},{"HIBP_API_KEY","HUNTER_API_KEY"}},
  {"LEGAL_SEARCH",        {0},{0},{"COURTLISTENER_API_KEY"}},
  {"BANKRUPTCY_SEARCH",   {0},{0},{"COURTLISTENER_API_KEY"}},
  {"CRIMINAL_RECORDS",    {0},{0},{"COURTLISTENER_API_KEY"}},
  {"LICENSE_PLATE_LOOKUP",{0},{0},{"PLATE_LOOKUP_URL","PLATE_LOOKUP_API_KEY"}},
  /* ── collectors that GATE on a credential but had no entry here ────────
   * Derived by scanning every collector for a getenv()/jo_env() of a
   * credential-shaped name that is followed by an early return — i.e. the
   * collector refuses to run without it — and keeping only files with exactly
   * one source_def, so the id↔var mapping is unambiguous.
   *
   * Without a row here three things were true at once: /api/keys REFUSED to
   * set the variable ("Unknown key"), /api/status asserted requiresKey:0 /
   * configured:1 for the source, and the dashboard's "needs key" filter could
   * never surface it. So a scheduled collector could gate on a credential the
   * operator had no way to supply and no way to discover, and re-gate on every
   * tick forever.
   *
   * Note COURTLISTENER_OPINIONS below: it reads COURTLISTENER_TOKEN, while the
   * pre-existing rows expose COURTLISTENER_API_KEY. Same family, different
   * variable — setting the documented one did nothing for this source. The
   * name recorded here is the one the collector actually reads. */
  {"CENSYS_SEARCH",             {"CENSYS_API_SECRET"},{0},{0}},
  {"COURTLISTENER_OPINIONS",    {"COURTLISTENER_TOKEN"},{0},{0}},
  {"DARK_WEB_MONITOR",          {"INTELX_API_KEY"},{0},{0}},
  {"DATALASTIC_VESSEL",         {"DATALASTIC_KEY"},{0},{0}},
  {"DEHASHED_SEARCH",           {"DEHASHED_API_KEY"},{0},{0}},
  {"GBIZINFO",                  {"GBIZINFO_TOKEN"},{0},{0}},
  {"GEONAMES",                  {"GEONAMES_USER"},{0},{0}},
  {"HASH_LOOKUP",               {"VIRUSTOTAL_API_KEY"},{0},{0}},
  {"IE_CRO",                    {"CRO_API_KEY"},{0},{0}},
  {"IN_MCA",                    {"INDIA_DATA_KEY"},{0},{0}},
  {"IP_REPUTATION",             {0},{"ABUSEIPDB_API_KEY","IPQS_API_KEY"},{0}},
  {"KR_DART",                   {"DART_API_KEY"},{0},{0}},
  {"MEDIACLOUD",                {"MEDIACLOUD_API_KEY"},{0},{0}},
  {"NEWS_AGGREGATOR",           {"NEWSAPI_KEY"},{0},{0}},
  {"NEWS_ARCHIVE",              {"NEWSAPI_KEY"},{0},{0}},
  {"OPENCORPORATES",            {"OPENCORPORATES_API_KEY"},{0},{0}},
  {"RO_COMPANIES",              {0},{"OPENAPI_RO_KEY","OPENAPIRO_KEY"},{0}},
  {"SATELLITE_TRACKER",         {"N2YO_API_KEY"},{0},{0}},
  {"THREAT_FEED_LOOKUP",        {"OTX_API_KEY"},{0},{0}},
  {"THREAT_INTEL",              {"ABUSEIPDB_API_KEY"},{0},{0}},
  {"UK_COMPANIES",              {"COMPANIES_HOUSE_API_KEY"},{0},{0}},
  {"WHALE_ALERT",               {0},{"ETHERSCAN_API_KEY","WHALE_ALERT_API_KEY"},{0}},
  {"WIFI_LOOKUP",               {0},{"WIGLE_API_NAME","WIGLE_API_TOKEN"},{0}},
  {"YOUTUBE_SEARCH",            {"YOUTUBE_API_KEY"},{0},{0}},
  {"agoop-flow",                {"AGOOP_API_KEY"},{0},{0}},
  {"censys-japan",              {"CENSYS_API_SECRET"},{0},{0}},
  {"docomo-insight",            {"DOCOMO_INSIGHT_API_KEY"},{0},{0}},
  {"docomo-population",         {"DOCOMO_POPULATION_API_KEY"},{0},{0}},
  {"estat-census",              {"ESTAT_APP_ID"},{0},{0}},
  {"estat-education",           {"ESTAT_APP_ID"},{0},{0}},
  {"estat-employment",          {"ESTAT_APP_ID"},{0},{0}},
  {"estat-household",           {"ESTAT_APP_ID"},{0},{0}},
  {"estat-industry",            {"ESTAT_APP_ID"},{0},{0}},
  {"flickr-geo",                {"FLICKR_API_KEY"},{0},{0}},
  {"google-dorking",            {0},{"GOOGLE_CSE_KEY","SERPAPI_KEY"},{0}},
  {"here-japan",                {"HERE_API_KEY"},{0},{0}},
  {"houjin-bangou",             {"HOUJIN_BANGOU_KEY"},{0},{0}},
  {"houmukyoku-commercial",     {"HOUMUKYOKU_API_KEY"},{0},{0}},
  {"jstat-map",                 {"ESTAT_APP_ID"},{0},{0}},
  {"mapfan-api",                {"MAPFAN_API_KEY"},{0},{0}},
  {"marinetraffic-jp",          {"MARINETRAFFIC_API_KEY"},{0},{0}},
  {"nasa-firms-jp",             {0},{"NASA_FIRMS_MAP_KEY","FIRMS_MAP_KEY"},{0}},
  {"navitime-api",              {"NAVITIME_API_KEY"},{0},{0}},
  {"nvd-cpe-dictionary",        {"NVD_API_KEY"},{0},{0}},
  {"psn-xbox-jp",               {"XBOX_API_KEY"},{0},{0}},
  {"reinfolib",                 {"REINFOLIB_API_KEY"},{0},{0}},
  {"resas-population",          {"RESAS_API_KEY"},{0},{0}},
  {"softbank-crowd",            {"SOFTBANK_CROWD_API_KEY"},{0},{0}},
  {"telegram-jp-channels",      {"TGSTAT_API_KEY"},{0},{0}},
  {"tellus-satellite",          {"TELLUS_TOKEN"},{0},{0}},
  {"tiktok-geo",                {"TIKTOK_MS_TOKEN"},{0},{0}},
  {"windy-japan",               {"WINDY_API_KEY"},{0},{0}},
  {"yahoo-map-api",             {"YAHOO_APP_ID"},{0},{0}},
};

const cred_def *cred_get(const char *id) {
  for (size_t i = 0; i < sizeof CREDS / sizeof *CREDS; i++)
    if (strcmp(CREDS[i].id, id) == 0) return &CREDS[i];
  return NULL;
}
int cred_alen(const char *const *a) { int n = 0; while (a[n]) n++; return n; }

int cred_known_vars(const char **names, const char **roles, int max) {
  /* ROLE_RANK required=0,anyOf=1,optional=2; keep the most-restrictive. */
  int n = 0;
  for (size_t i = 0; i < sizeof CREDS / sizeof *CREDS; i++) {
    const cred_def *e = &CREDS[i];
    const char *const *grp[3] = { e->req, e->any, e->opt };
    const char *rn[3] = { "required", "anyOf", "optional" };
    for (int g = 0; g < 3; g++) {
      for (int k = 0; grp[g][k]; k++) {
        const char *nm = grp[g][k];
        int at = -1;
        for (int j = 0; j < n; j++) if (strcmp(names[j], nm) == 0) { at = j; break; }
        if (at < 0) {
          if (n >= max) continue;
          names[n] = nm; roles[n] = rn[g]; n++;
        } else {
          int cur = strcmp(roles[at],"required")==0?0
                  : strcmp(roles[at],"anyOf")==0?1:2;
          if (g < cur) roles[at] = rn[g];
        }
      }
    }
  }
  /* sort by rank then name (ASCII; the var-name set has no '_'-vs-letter
   * prefix collisions where ICU localeCompare would differ from strcmp). */
  for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
    int ri = strcmp(roles[i],"required")==0?0:strcmp(roles[i],"anyOf")==0?1:2;
    int rj = strcmp(roles[j],"required")==0?0:strcmp(roles[j],"anyOf")==0?1:2;
    if (rj < ri || (rj == ri && strcmp(names[j], names[i]) < 0)) {
      const char *tn=names[i]; names[i]=names[j]; names[j]=tn;
      const char *tr=roles[i]; roles[i]=roles[j]; roles[j]=tr;
    }
  }
  return n;
}
