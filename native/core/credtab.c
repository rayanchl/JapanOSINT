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
  {"windy-webcams",       {"WINDY_API_KEY"},{0},{0}},
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
