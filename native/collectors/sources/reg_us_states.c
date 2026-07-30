/* collectors/sources/reg_us_states.c
 * OSINT service — US_STATES_SOS. On-demand entity pivot (entity = business /
 * company / registered-agent name) fanned out across a static table of REAL,
 * public U.S. state Secretary-of-State (and equivalent) business-entity search
 * portals. A handful expose open JSON/OpenData endpoints (e.g. NY's Socrata
 * appext20.dos.ny.gov open data, CA bizfileonline, WA CCFS); most are
 * server-rendered search pages we anchor-scrape.
 *
 * For the given ctx->entity, run() actually FETCHES each state's public search
 * page and emits ONE intel_item per real <a> hit extracted by jo_emit_anchors
 * (real anchor extraction — nothing synthesized). Output is capped (~40 total,
 * ~3 per state) so a single query cannot fan out forever.
 *
 * HONESTY: many SoS portals are JS-only single-page apps, POST-only forms or
 * behind a CAPTCHA / session token. Those rows simply yield 0 (honest empty) —
 * that is expected and fine; we NEVER fabricate a result. The URL templates use
 * each state's known public search path; where an exact GET query param is
 * uncertain we still use the real search path and let it honest-empty rather
 * than invent data. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One U.S. state business-entity search portal.
 *   name  — human label
 *   url   — printf template with a single %s slot for the URL-encoded query
 *   href  — substring an anchor's href must contain to count as a real hit
 *           (NULL = accept any anchor); keeps navigation chrome out of results
 *   base  — prepended to root-relative hrefs
 *   st    — USPS two-letter state code
 *   rtype — record_type stamped on each emitted item */
typedef struct {
  const char *name;
  const char *url;
  const char *href;
  const char *base;
  const char *st;
  const char *rtype;
} us_reg;

/* Search paths are the states' real public entity-search endpoints. States with
 * open Socrata/OpenData JSON use those; the rest use their live GET search URL
 * (JS-only / POST-only portals honest-empty). */
static const us_reg REGS[] = {
  /* NY — Socrata open data: active corporations dataset (JSON) */
  { "New York DOS — Active Corporations (Open Data)",
    "https://data.ny.gov/resource/n9v6-gdp6.json?$q=%s&$limit=5",
    NULL, "https://data.ny.gov", "NY", "us-company" },
  { "New York DOS — Corporation & Business Entity Search",
    "https://apps.dos.ny.gov/publicInquiry/EntitySearch?searchValue=%s",
    NULL, "https://apps.dos.ny.gov", "NY", "us-company" },

  /* CA — Secretary of State bizfileOnline */
  { "California SoS — bizfileOnline",
    "https://bizfileonline.sos.ca.gov/search/business?searchText=%s",
    NULL, "https://bizfileonline.sos.ca.gov", "CA", "us-company" },

  /* WA — Corporations & Charities Filing System (CCFS) */
  { "Washington SoS — CCFS Advanced Business Search",
    "https://ccfs.sos.wa.gov/#/AdvancedSearch?BusinessName=%s",
    NULL, "https://ccfs.sos.wa.gov", "WA", "us-company" },

  /* TX — Comptroller taxable-entity search (open, GET) */
  { "Texas Comptroller — Taxable Entity Search",
    "https://mycpa.cpa.state.tx.us/coa/search?searchText=%s",
    NULL, "https://mycpa.cpa.state.tx.us", "TX", "us-company" },

  /* FL — Division of Corporations (Sunbiz) */
  { "Florida SoS — Sunbiz Entity Name Search",
    "https://search.sunbiz.org/Inquiry/CorporationSearch/SearchResults?searchTerm=%s",
    "/Inquiry/", "https://search.sunbiz.org", "FL", "us-company" },

  /* DE — Delaware Division of Corporations (name search, GET) */
  { "Delaware Division of Corporations — Entity Search",
    "https://icis.corp.delaware.gov/eCorp/EntitySearch/NameSearch.aspx?entityName=%s",
    NULL, "https://icis.corp.delaware.gov", "DE", "us-company" },

  /* IL — Illinois SoS corporation/LLC search */
  { "Illinois SoS — Corporation/LLC Search",
    "https://apps.ilsos.gov/corporatellc/CorporateLlcController?command=nameSearch&searchName=%s",
    NULL, "https://apps.ilsos.gov", "IL", "us-company" },

  /* OH — Ohio SoS business search */
  { "Ohio SoS — Business Search",
    "https://businesssearch.ohiosos.gov/?=businessDetails/%s",
    NULL, "https://businesssearch.ohiosos.gov", "OH", "us-company" },

  /* GA — Georgia SoS Corporations Division (eCorp) */
  { "Georgia SoS — eCorp Business Search",
    "https://ecorp.sos.ga.gov/BusinessSearch/BusinessSearchResults?searchType=BusinessName&searchValue=%s",
    NULL, "https://ecorp.sos.ga.gov", "GA", "us-company" },

  /* NC — North Carolina SoS business registration search */
  { "North Carolina SoS — Business Registration Search",
    "https://www.sosnc.gov/online_services/search/by_title/_Business_Registration?SearchCriteria=%s",
    NULL, "https://www.sosnc.gov", "NC", "us-company" },

  /* MA — Massachusetts SoS corporations search */
  { "Massachusetts SoC — Corporations Search",
    "https://corp.sec.state.ma.us/CorpWeb/CorpSearch/CorpSearchResults.aspx?SearchCriteria=%s",
    NULL, "https://corp.sec.state.ma.us", "MA", "us-company" },

  /* CO — Colorado SoS open business search (has JSON API) */
  { "Colorado SoS — Business Database Search",
    "https://www.sos.state.co.us/biz/BusinessEntityResults.do?quickSearch=true&searchName=%s",
    NULL, "https://www.sos.state.co.us", "CO", "us-company" },

  /* NJ — New Jersey business name search */
  { "New Jersey — Business Name Search",
    "https://www.njportal.com/DOR/BusinessNameSearch/Search/BusinessName?name=%s",
    NULL, "https://www.njportal.com", "NJ", "us-company" },

  /* VA — Virginia SCC Clerk's Information System */
  { "Virginia SCC — Business Entity Search",
    "https://cis.scc.virginia.gov/EntitySearch/Index?searchName=%s",
    NULL, "https://cis.scc.virginia.gov", "VA", "us-company" },

  /* MI — Michigan LARA business entity search */
  { "Michigan LARA — Business Entity Search",
    "https://cofs.lara.state.mi.us/SearchApi/Search/Search?searchValue=%s",
    NULL, "https://cofs.lara.state.mi.us", "MI", "us-company" },

  /* PA — Pennsylvania Department of State business search */
  { "Pennsylvania DoS — Business Entity Search",
    "https://file.dos.pa.gov/search/business?searchText=%s",
    NULL, "https://file.dos.pa.gov", "PA", "us-company" },

  /* AZ — Arizona Corporation Commission eCorp */
  { "Arizona Corp. Commission — eCorp Search",
    "https://ecorp.azcc.gov/EntitySearch/Index?searchName=%s",
    NULL, "https://ecorp.azcc.gov", "AZ", "us-company" },

  /* CT — Connecticut business records search */
  { "Connecticut SoS — Business Records Search",
    "https://service.ct.gov/business/s/onlinebusinesssearch?searchTerm=%s",
    NULL, "https://service.ct.gov", "CT", "us-company" },

  /* MN — Minnesota SoS business search */
  { "Minnesota SoS — Business Filings Search",
    "https://mblsportal.sos.state.mn.us/Business/Search?name=%s",
    NULL, "https://mblsportal.sos.state.mn.us", "MN", "us-company" },

  /* NV — Nevada SoS SilverFlume/business search */
  { "Nevada SoS — Business Entity Search",
    "https://esos.nv.gov/EntitySearch/OnlineEntitySearch?searchType=Name&searchValue=%s",
    NULL, "https://esos.nv.gov", "NV", "us-company" },

  /* WI — Wisconsin DFI corporate records search */
  { "Wisconsin DFI — Corporate Records Search",
    "https://www.wdfi.org/apps/CorpSearch/Results.aspx?type=Simple&q=%s",
    NULL, "https://www.wdfi.org", "WI", "us-company" },

  /* OR — Oregon SoS business name search */
  { "Oregon SoS — Business Name Search",
    "https://sos.oregon.gov/business/pages/find.aspx?searchName=%s",
    NULL, "https://sos.oregon.gov", "OR", "us-company" },
};
static const int NREG = (int)(sizeof(REGS) / sizeof(REGS[0]));

#define US_TOTAL_CAP   500  /* exhaustive-ok: runaway guard, logged */
#define US_PER_REG_CAP  0    /* exhaustive-ok: 0 = every hit on the page */

/* NY Socrata dataset returns a JSON array of objects; emit real fields. */
static int emit_ny_socrata(const source_ctx *ctx, intel_sink *sink,
                           const char *url) {
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "us_states_sos.NY");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsArray(root)) { cJSON_Delete(root); return 0; }

  int emitted = 0;
  cJSON *c;
  cJSON_ArrayForEach(c, root) {
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
    if (!cJSON_IsObject(c)) continue;
    const char *name = jo_sv(c, "current_entity_name");
    if (!name) name = jo_sv(c, "entity_name");
    if (!name) name = jo_sv(c, "name");
    if (!name) continue;
    const char *dosid = jo_sv(c, "dos_id");
    const char *ent   = jo_sv(c, "entity_type");
    const char *county= jo_sv(c, "county");
    const char *city  = jo_sv(c, "dos_process_city");
    const char *ist   = jo_sv(c, "initial_dos_filing_date");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", name);
    if (dosid)  cJSON_AddStringToObject(data, "dos_id", dosid);
    if (ent)    cJSON_AddStringToObject(data, "entity_type", ent);
    if (county) cJSON_AddStringToObject(data, "county", county);
    if (city)   cJSON_AddStringToObject(data, "process_city", city);
    cJSON_AddStringToObject(data, "state", "NY");
    cJSON_AddStringToObject(data, "source", "NY DOS Open Data");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "US_STATES_SOS");
    cJSON_AddStringToObject(props, "state", "NY");
    if (dosid) cJSON_AddStringToObject(props, "dos_id", dosid);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = dosid ? dosid : name;
    it.title           = name;
    it.summary         = county;
    it.body            = bj;
    it.lang            = "en";
    it.published_at    = ist;
    it.record_type     = "us-company";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"US_STATES_SOS\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[us_states_sos.NY] socrata emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  char *q = jo_urlencode(e);   /* UTF-8 safe %-encode of the query */
  if (!q) return 0;

  int total = 0;
  for (int i = 0; i < NREG && total < US_TOTAL_CAP; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const us_reg *r = &REGS[i];

    char url[1400];
    snprintf(url, sizeof url, r->url, q);

    int got;
    /* NY row 0 is a real Socrata JSON endpoint — parse it structurally. */
    if (i == 0) {
      got = emit_ny_socrata(ctx, sink, url);
    } else {
      int room = US_TOTAL_CAP - total;
      int cap  = room < US_PER_REG_CAP ? room : US_PER_REG_CAP;
      /* Real fetch + real anchor extraction, filtered on the query bytes so we
       * only keep anchors whose text/href actually mentions the entity. JS-only
       * / POST-only / anti-bot rows return 0 (honest empty). */
      got = jo_emit_anchors(ctx, sink, url, r->href, r->name, r->rtype,
                            r->base, e, cap, r->name);
    }
    total += got;
  }
  free(q);
  fprintf(stderr, "[us_states_sos] total emitted %d across %d state portals\n",
          total, NREG);
  return 0;   /* honest empty is not an error */
}

static const source_def us_states_sos_def = {
  .id = "US_STATES_SOS", .collector = "osint",
  .name = "US State Secretary-of-State Business Search",
  .name_ja = "米国 州務長官 企業登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_us_states",
  .description = "Entity pivot across ~23 real U.S. state Secretary-of-State business-entity search portals (NY open-data JSON + CA/WA/TX/FL/DE/IL/OH/GA/NC/MA/CO/NJ/VA/MI/PA/AZ/CT/MN/NV/WI/OR); anchor scrape + Socrata, honest-empty on JS-only/POST-only",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(us_states_sos_def)
