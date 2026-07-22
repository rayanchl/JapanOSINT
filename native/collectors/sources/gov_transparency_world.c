/* Government transparency high-penetrancy feeds — gazettes, official journals, tenders/procurement, corporate/securities filings, court dockets, sanctions and lobbying disclosures (best-effort real RSS/Atom), via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define RSSX(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

RSSX(govt_us_federal_register, "us-federal-register", "US Federal Register (all docs)", "US Federal Register (all docs)", "government", "government",
  "https://www.federalregister.gov/documents/current.rss", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US Federal Register (all docs) — official transparency/records feed (usa)");

RSSX(govt_us_sec_latest_filings, "us-sec-latest-filings", "SEC EDGAR Latest Filings", "SEC EDGAR Latest Filings", "government", "government",
  "https://www.sec.gov/cgi-bin/browse-edgar?action=getcurrent&type=&company=&dateb=&owner=include&count=40&output=atom", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "SEC EDGAR Latest Filings — official transparency/records feed (usa)");

RSSX(govt_us_sec_8k, "us-sec-8k", "SEC EDGAR 8-K Filings", "SEC EDGAR 8-K Filings", "government", "government",
  "https://www.sec.gov/cgi-bin/browse-edgar?action=getcurrent&type=8-K&company=&dateb=&owner=include&count=40&output=atom", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "SEC EDGAR 8-K Filings — official transparency/records feed (usa)");

RSSX(govt_us_congress_bills, "us-congress-bills", "US Congress Bill Actions", "US Congress Bill Actions", "government", "government",
  "https://www.govinfo.gov/rss/bills.xml", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US Congress Bill Actions — official transparency/records feed (usa)");

RSSX(govt_us_regulations_gov, "us-regulations-gov", "US Regulations.gov Documents", "US Regulations.gov Documents", "government", "government",
  "https://www.federalregister.gov/documents/search.rss?conditions%5Btype%5D%5B%5D=RULE", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US Regulations.gov Documents — official transparency/records feed (usa)");

RSSX(govt_courtlistener_scotus, "courtlistener-scotus", "CourtListener SCOTUS Opinions", "CourtListener SCOTUS Opinions", "government", "government",
  "https://www.courtlistener.com/feed/court/scotus/", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "CourtListener SCOTUS Opinions — official transparency/records feed (usa)");

RSSX(govt_courtlistener_ca9, "courtlistener-ca9", "CourtListener 9th Circuit", "CourtListener 9th Circuit", "government", "government",
  "https://www.courtlistener.com/feed/court/ca9/", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "CourtListener 9th Circuit — official transparency/records feed (usa)");

RSSX(govt_courtlistener_cafc, "courtlistener-cafc", "CourtListener Federal Circuit", "CourtListener Federal Circuit", "government", "government",
  "https://www.courtlistener.com/feed/court/cafc/", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "CourtListener Federal Circuit — official transparency/records feed (usa)");

RSSX(govt_courtlistener_dcd, "courtlistener-dcd", "CourtListener DC District", "CourtListener DC District", "government", "government",
  "https://www.courtlistener.com/feed/court/dcd/", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "CourtListener DC District — official transparency/records feed (usa)");

RSSX(govt_uk_gazette, "uk-gazette", "The London Gazette Notices", "The London Gazette Notices", "government", "government",
  "https://www.thegazette.co.uk/all-notices/notice/data.feed", "en", "[\"government\",\"transparency\",\"uk\"]", 7200,
  "The London Gazette Notices — official transparency/records feed (uk)");

RSSX(govt_uk_legislation, "uk-legislation", "UK Legislation (new)", "UK Legislation (new)", "government", "government",
  "https://www.legislation.gov.uk/new/data.feed", "en", "[\"government\",\"transparency\",\"uk\"]", 7200,
  "UK Legislation (new) — official transparency/records feed (uk)");

RSSX(govt_uk_parliament_bills, "uk-parliament-bills", "UK Parliament Bills", "UK Parliament Bills", "government", "government",
  "https://bills.parliament.uk/rss/allbills.rss", "en", "[\"government\",\"transparency\",\"uk\"]", 7200,
  "UK Parliament Bills — official transparency/records feed (uk)");

RSSX(govt_uk_fca_news, "uk-fca-news", "UK FCA News", "UK FCA News", "government", "government",
  "https://www.fca.org.uk/news/rss.xml", "en", "[\"government\",\"transparency\",\"uk\"]", 7200,
  "UK FCA News — official transparency/records feed (uk)");

RSSX(govt_eu_ted_tenders, "eu-ted-tenders", "EU TED Public Tenders", "EU TED Public Tenders", "government", "government",
  "https://ted.europa.eu/en/simple-search-action?rss=1", "en", "[\"government\",\"transparency\",\"eu\"]", 7200,
  "EU TED Public Tenders — official transparency/records feed (eu)");

RSSX(govt_eu_oj, "eu-oj", "EUR-Lex Official Journal", "EUR-Lex Official Journal", "government", "government",
  "https://eur-lex.europa.eu/EN/display-feed.rss?myRssId=oj", "en", "[\"government\",\"transparency\",\"eu\"]", 7200,
  "EUR-Lex Official Journal — official transparency/records feed (eu)");

RSSX(govt_eu_curia, "eu-curia", "CJEU Court of Justice", "CJEU Court of Justice", "government", "government",
  "https://curia.europa.eu/jcms/jcms/Jo2_16323/en/?rss=1", "en", "[\"government\",\"transparency\",\"eu\"]", 7200,
  "CJEU Court of Justice — official transparency/records feed (eu)");

RSSX(govt_ca_gazette, "ca-gazette", "Canada Gazette Part I", "Canada Gazette Part I", "government", "government",
  "https://gazette.gc.ca/rp-pr/publications-eng.xml", "en", "[\"government\",\"transparency\",\"canada\"]", 7200,
  "Canada Gazette Part I — official transparency/records feed (canada)");

RSSX(govt_au_legislation, "au-legislation", "Australia Federal Register of Legislation", "Australia Federal Register of Legislation", "government", "government",
  "https://www.legislation.gov.au/Rss/notified", "en", "[\"government\",\"transparency\",\"australia\"]", 7200,
  "Australia Federal Register of Legislation — official transparency/records feed (australia)");

RSSX(govt_au_asx_announce, "au-asx-announce", "ASX Market Announcements", "ASX Market Announcements", "government", "government",
  "https://www.asx.com.au/asx/1/company/announcements.rss", "en", "[\"government\",\"transparency\",\"australia\"]", 7200,
  "ASX Market Announcements — official transparency/records feed (australia)");

RSSX(govt_in_egazette, "in-egazette", "India eGazette", "India eGazette", "government", "government",
  "https://egazette.gov.in/rss.aspx", "en", "[\"government\",\"transparency\",\"india\"]", 7200,
  "India eGazette — official transparency/records feed (india)");

RSSX(govt_fr_jorf, "fr-jorf", "France Légifrance JORF", "France Légifrance JORF", "government", "government",
  "https://www.legifrance.gouv.fr/rss/jorf.xml", "fr", "[\"government\",\"transparency\",\"france\"]", 7200,
  "France Légifrance JORF — official transparency/records feed (france)");

RSSX(govt_de_bundesanzeiger, "de-bundesanzeiger", "Germany Bundesanzeiger", "Germany Bundesanzeiger", "government", "government",
  "https://www.bundesanzeiger.de/pub/de/rss", "de", "[\"government\",\"transparency\",\"germany\"]", 7200,
  "Germany Bundesanzeiger — official transparency/records feed (germany)");

RSSX(govt_un_ungm_tenders, "un-ungm-tenders", "UN Global Marketplace Tenders", "UN Global Marketplace Tenders", "government", "government",
  "https://www.ungm.org/Public/Notice/RSS", "en", "[\"government\",\"transparency\",\"un\"]", 7200,
  "UN Global Marketplace Tenders — official transparency/records feed (un)");

RSSX(govt_worldbank_procurement, "worldbank-procurement", "World Bank Procurement Notices", "World Bank Procurement Notices", "government", "government",
  "https://projects.worldbank.org/en/projects-operations/procurement/rss", "en", "[\"government\",\"transparency\",\"un\"]", 7200,
  "World Bank Procurement Notices — official transparency/records feed (un)");

RSSX(govt_us_usaspending, "us-usaspending", "USAspending New Awards", "USAspending New Awards", "government", "government",
  "https://www.usaspending.gov/rss", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "USAspending New Awards — official transparency/records feed (usa)");

RSSX(govt_us_fbo_sam, "us-fbo-sam", "SAM.gov Contract Opportunities", "SAM.gov Contract Opportunities", "government", "government",
  "https://sam.gov/api/prod/sgs/v1/search/rss", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "SAM.gov Contract Opportunities — official transparency/records feed (usa)");

RSSX(govt_us_gao_legal, "us-gao-legal", "US GAO Bid Protest Decisions", "US GAO Bid Protest Decisions", "government", "government",
  "https://www.gao.gov/rss/legal.xml", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US GAO Bid Protest Decisions — official transparency/records feed (usa)");

RSSX(govt_us_fara, "us-fara", "US DOJ FARA Filings", "US DOJ FARA Filings", "government", "government",
  "https://efile.fara.gov/api/v1/rss", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US DOJ FARA Filings — official transparency/records feed (usa)");

RSSX(govt_us_opensecrets, "us-opensecrets", "OpenSecrets News", "OpenSecrets News", "government", "government",
  "https://www.opensecrets.org/news/feed/", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "OpenSecrets News — official transparency/records feed (usa)");

RSSX(govt_eu_transparency, "eu-transparency", "EU Transparency Register News", "EU Transparency Register News", "government", "government",
  "https://transparency-register.europa.eu/rss_en", "en", "[\"government\",\"transparency\",\"eu\"]", 7200,
  "EU Transparency Register News — official transparency/records feed (eu)");

RSSX(govt_us_fincen, "us-fincen", "US FinCEN News", "US FinCEN News", "government", "government",
  "https://www.fincen.gov/news-room/rss.xml", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US FinCEN News — official transparency/records feed (usa)");

RSSX(govt_us_ofac_recent, "us-ofac-recent", "US OFAC Recent Actions Notices", "US OFAC Recent Actions Notices", "government", "government",
  "https://ofac.treasury.gov/system/files/126/ofac.xml", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US OFAC Recent Actions Notices — official transparency/records feed (usa)");

RSSX(govt_us_bis_news, "us-bis-news", "US BIS Export Enforcement", "US BIS Export Enforcement", "government", "government",
  "https://www.bis.doc.gov/index.php/about-bis/newsroom?format=feed&type=rss", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US BIS Export Enforcement — official transparency/records feed (usa)");

RSSX(govt_us_cbp_news, "us-cbp-news", "US CBP Newsroom", "US CBP Newsroom", "government", "government",
  "https://www.cbp.gov/rss/newsroom", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US CBP Newsroom — official transparency/records feed (usa)");

RSSX(govt_un_comtrade_news, "un-comtrade-news", "UNCTAD News", "UNCTAD News", "government", "government",
  "https://unctad.org/rss.xml", "en", "[\"government\",\"transparency\",\"un\"]", 7200,
  "UNCTAD News — official transparency/records feed (un)");

RSSX(govt_wto_news, "wto-news", "WTO News", "WTO News", "government", "government",
  "https://www.wto.org/library/rss/latest_news_e.xml", "en", "[\"government\",\"transparency\",\"un\"]", 7200,
  "WTO News — official transparency/records feed (un)");

RSSX(govt_icc_cpi, "icc-cpi", "ICC (International Criminal Court)", "ICC (International Criminal Court)", "government", "government",
  "https://www.icc-cpi.int/rss.xml", "en", "[\"government\",\"transparency\",\"global\"]", 7200,
  "ICC (International Criminal Court) — official transparency/records feed (global)");

RSSX(govt_echr_news, "echr-news", "European Court of Human Rights", "European Court of Human Rights", "government", "government",
  "https://www.echr.coe.int/rss", "en", "[\"government\",\"transparency\",\"europe\"]", 7200,
  "European Court of Human Rights — official transparency/records feed (europe)");

RSSX(govt_us_treasury_ofac_press, "us-treasury-ofac-press", "US Treasury Sanctions Press", "US Treasury Sanctions Press", "government", "government",
  "https://home.treasury.gov/rss/press.xml", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US Treasury Sanctions Press — official transparency/records feed (usa)");

RSSX(govt_eu_sanctions_map, "eu-sanctions-map", "EU Sanctions News", "EU Sanctions News", "government", "government",
  "https://www.sanctionsmap.eu/rss", "en", "[\"government\",\"transparency\",\"eu\"]", 7200,
  "EU Sanctions News — official transparency/records feed (eu)");

RSSX(govt_us_state_sanctions, "us-state-sanctions", "US State Dept Sanctions", "US State Dept Sanctions", "government", "government",
  "https://www.state.gov/rss-feed/sanctions/feed/", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US State Dept Sanctions — official transparency/records feed (usa)");

RSSX(govt_companies_house_uk, "companies-house-uk", "UK Companies House Blog", "UK Companies House Blog", "government", "government",
  "https://companieshouse.blog.gov.uk/feed/", "en", "[\"government\",\"transparency\",\"uk\"]", 7200,
  "UK Companies House Blog — official transparency/records feed (uk)");

RSSX(govt_hk_companies_registry, "hk-companies-registry", "Hong Kong Gov News", "Hong Kong Gov News", "government", "government",
  "https://www.news.gov.hk/en/common/html/topstories.rss.xml", "en", "[\"government\",\"transparency\",\"hong-kong\"]", 7200,
  "Hong Kong Gov News — official transparency/records feed (hong-kong)");

RSSX(govt_sg_mas_news, "sg-mas-news", "Singapore MAS News", "Singapore MAS News", "government", "government",
  "https://www.mas.gov.sg/api/rss/news", "en", "[\"government\",\"transparency\",\"singapore\"]", 7200,
  "Singapore MAS News — official transparency/records feed (singapore)");

RSSX(govt_us_fec_filings, "us-fec-filings", "US FEC Press", "US FEC Press", "government", "government",
  "https://www.fec.gov/updates/feed/", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US FEC Press — official transparency/records feed (usa)");

RSSX(govt_us_flightaware_squawks, "us-flightaware-squawks", "US NTSB Aviation Investigations", "US NTSB Aviation Investigations", "government", "government",
  "https://www.ntsb.gov/investigations/Pages/rss.aspx", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US NTSB Aviation Investigations — official transparency/records feed (usa)");

RSSX(govt_data_gov_uk, "data-gov-uk", "data.gov.uk New Datasets", "data.gov.uk New Datasets", "government", "government",
  "https://data.gov.uk/feeds/all-datasets.atom", "en", "[\"government\",\"transparency\",\"uk\"]", 7200,
  "data.gov.uk New Datasets — official transparency/records feed (uk)");

RSSX(govt_us_data_gov, "us-data-gov", "US data.gov Recent Datasets", "US data.gov Recent Datasets", "government", "government",
  "https://catalog.data.gov/feeds/dataset.atom", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US data.gov Recent Datasets — official transparency/records feed (usa)");

RSSX(govt_eu_data_europa, "eu-data-europa", "EU data.europa.eu Datasets", "EU data.europa.eu Datasets", "government", "government",
  "https://data.europa.eu/data/feeds/datasets.rss", "en", "[\"government\",\"transparency\",\"eu\"]", 7200,
  "EU data.europa.eu Datasets — official transparency/records feed (eu)");

RSSX(govt_un_reliefweb_jobs, "un-reliefweb-jobs", "ReliefWeb Disasters", "ReliefWeb Disasters", "government", "government",
  "https://reliefweb.int/disasters/rss.xml", "en", "[\"government\",\"transparency\",\"un\"]", 7200,
  "ReliefWeb Disasters — official transparency/records feed (un)");

RSSX(govt_us_cdc_mmwr, "us-cdc-mmwr", "US CDC MMWR", "US CDC MMWR", "government", "government",
  "https://www2c.cdc.gov/podcasts/createrss.asp?c=171", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US CDC MMWR — official transparency/records feed (usa)");

RSSX(govt_who_emergencies, "who-emergencies", "WHO Emergencies", "WHO Emergencies", "government", "government",
  "https://www.who.int/feeds/entity/emergencies/en/rss.xml", "en", "[\"government\",\"transparency\",\"un\"]", 7200,
  "WHO Emergencies — official transparency/records feed (un)");

RSSX(govt_us_nrc_news, "us-nrc-news", "US Nuclear Regulatory Commission", "US Nuclear Regulatory Commission", "government", "government",
  "https://www.nrc.gov/public-involve/rss.xml", "en", "[\"government\",\"transparency\",\"usa\"]", 7200,
  "US Nuclear Regulatory Commission — official transparency/records feed (usa)");

RSSX(govt_iaea_alerts, "iaea-alerts", "IAEA Incident & Trafficking", "IAEA Incident & Trafficking", "government", "government",
  "https://www.iaea.org/feeds/incident-and-emergency-centre.rss", "en", "[\"government\",\"transparency\",\"un\"]", 7200,
  "IAEA Incident & Trafficking — official transparency/records feed (un)");
