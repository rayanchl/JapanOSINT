/* Government / agency / IGO press and advisory feeds worldwide (best-effort real RSS). Official primary-source intel, via rss_collect. */
#include "source.h"
#include "lib/rss_atom.h"

#include "_source_macros.inc"

RSSX(gov_us_state_press, "us-state-press", "US State Dept Press Releases", "US State Dept Press Releases", "government", "government",
  "https://www.state.gov/rss-feed/press-releases/feed/", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US State Dept Press Releases — official government/agency feed (usa)");

RSSX(gov_us_dod_news, "us-dod-news", "US Dept of Defense News", "US Dept of Defense News", "government", "government",
  "https://www.defense.gov/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=945&max=25", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US Dept of Defense News — official government/agency feed (usa)");

RSSX(gov_us_dhs_news, "us-dhs-news", "US DHS News Releases", "US DHS News Releases", "government", "government",
  "https://www.dhs.gov/news-releases/rss.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US DHS News Releases — official government/agency feed (usa)");

RSSX(gov_us_doj_news, "us-doj-news", "US Dept of Justice News", "US Dept of Justice News", "government", "government",
  "https://www.justice.gov/news/rss?type=press_release", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US Dept of Justice News — official government/agency feed (usa)");

RSSX(gov_us_sec_press, "us-sec-press", "US SEC Press Releases", "US SEC Press Releases", "government", "government",
  "https://www.sec.gov/news/pressreleases.rss", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US SEC Press Releases — official government/agency feed (usa)");

RSSX(gov_us_ftc_press, "us-ftc-press", "US FTC Press Releases", "US FTC Press Releases", "government", "government",
  "https://www.ftc.gov/feeds/press-release.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US FTC Press Releases — official government/agency feed (usa)");

RSSX(gov_us_gao_reports, "us-gao-reports", "US GAO Reports", "US GAO Reports", "government", "government",
  "https://www.gao.gov/rss/reports.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US GAO Reports — official government/agency feed (usa)");

RSSX(gov_us_treasury_press, "us-treasury-press", "US Treasury Press", "US Treasury Press", "government", "government",
  "https://home.treasury.gov/rss/press.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US Treasury Press — official government/agency feed (usa)");

RSSX(gov_us_nhc_atlantic, "us-nhc-atlantic", "NOAA NHC Atlantic", "NOAA NHC Atlantic", "government", "government",
  "https://www.nhc.noaa.gov/index-at.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "NOAA NHC Atlantic — official government/agency feed (usa)");

RSSX(gov_us_nhc_epac, "us-nhc-epac", "NOAA NHC East Pacific", "NOAA NHC East Pacific", "government", "government",
  "https://www.nhc.noaa.gov/index-ep.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "NOAA NHC East Pacific — official government/agency feed (usa)");

RSSX(gov_uk_gov_news, "uk-gov-news", "UK Government Announcements", "UK Government Announcements", "government", "government",
  "https://www.gov.uk/search/news-and-communications.atom", "en", "[\"government\",\"official\",\"uk\"]", 3600,
  "UK Government Announcements — official government/agency feed (uk)");

RSSX(gov_uk_fcdo_news, "uk-fcdo-news", "UK FCDO News", "UK FCDO News", "government", "government",
  "https://www.gov.uk/government/organisations/foreign-commonwealth-development-office.atom", "en", "[\"government\",\"official\",\"uk\"]", 3600,
  "UK FCDO News — official government/agency feed (uk)");

RSSX(gov_uk_mod_news, "uk-mod-news", "UK Ministry of Defence", "UK Ministry of Defence", "government", "government",
  "https://www.gov.uk/government/organisations/ministry-of-defence.atom", "en", "[\"government\",\"official\",\"uk\"]", 3600,
  "UK Ministry of Defence — official government/agency feed (uk)");

RSSX(gov_ec_presscorner, "ec-presscorner", "European Commission Press", "European Commission Press", "government", "government",
  "https://ec.europa.eu/commission/presscorner/api/rss?language=en", "en", "[\"government\",\"official\",\"eu\"]", 3600,
  "European Commission Press — official government/agency feed (eu)");

RSSX(gov_eeas_news, "eeas-news", "EU EEAS News", "EU EEAS News", "government", "government",
  "https://www.eeas.europa.eu/eeas/rss_en", "en", "[\"government\",\"official\",\"eu\"]", 3600,
  "EU EEAS News — official government/agency feed (eu)");

RSSX(gov_enisa_news, "enisa-news", "ENISA News", "ENISA News", "government", "government",
  "https://www.enisa.europa.eu/media/news-items/news-wires/RSS", "en", "[\"government\",\"official\",\"eu\"]", 3600,
  "ENISA News — official government/agency feed (eu)");

RSSX(gov_ema_news, "ema-news", "European Medicines Agency", "European Medicines Agency", "government", "government",
  "https://www.ema.europa.eu/en/rss.xml", "en", "[\"government\",\"official\",\"eu\"]", 3600,
  "European Medicines Agency — official government/agency feed (eu)");

RSSX(gov_frontex_news, "frontex-news", "Frontex News", "Frontex News", "government", "government",
  "https://www.frontex.europa.eu/rss/news/", "en", "[\"government\",\"official\",\"eu\"]", 3600,
  "Frontex News — official government/agency feed (eu)");

RSSX(gov_europarl_news, "europarl-news", "European Parliament News", "European Parliament News", "government", "government",
  "https://www.europarl.europa.eu/rss/doc/top-stories/en.xml", "en", "[\"government\",\"official\",\"eu\"]", 3600,
  "European Parliament News — official government/agency feed (eu)");

RSSX(gov_osce_news, "osce-news", "OSCE News", "OSCE News", "government", "government",
  "https://www.osce.org/rss", "en", "[\"government\",\"official\",\"europe\"]", 3600,
  "OSCE News — official government/agency feed (europe)");

RSSX(gov_iaea_topnews, "iaea-topnews", "IAEA Top News", "IAEA Top News", "government", "government",
  "https://www.iaea.org/feeds/topnews", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "IAEA Top News — official government/agency feed (un)");

RSSX(gov_un_news_2, "un-news-2", "UN News Global Perspective", "UN News Global Perspective", "government", "government",
  "https://news.un.org/feed/subscribe/en/news/all/rss.xml", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "UN News Global Perspective — official government/agency feed (un)");

RSSX(gov_who_news, "who-news", "WHO News", "WHO News", "government", "government",
  "https://www.who.int/rss-feeds/news-english.xml", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "WHO News — official government/agency feed (un)");

RSSX(gov_unhcr_news, "unhcr-news", "UNHCR News", "UNHCR News", "government", "government",
  "https://www.unhcr.org/rss/news.xml", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "UNHCR News — official government/agency feed (un)");

RSSX(gov_wfp_news, "wfp-news", "World Food Programme News", "World Food Programme News", "government", "government",
  "https://www.wfp.org/rss.xml", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "World Food Programme News — official government/agency feed (un)");

RSSX(gov_iom_news, "iom-news", "IOM Migration News", "IOM Migration News", "government", "government",
  "https://www.iom.int/rss.xml", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "IOM Migration News — official government/agency feed (un)");

RSSX(gov_unodc_news, "unodc-news", "UNODC News", "UNODC News", "government", "government",
  "https://www.unodc.org/unodc/en/frontpage/rss.xml", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "UNODC News — official government/agency feed (un)");

RSSX(gov_itu_news, "itu-news", "ITU News", "ITU News", "government", "government",
  "https://www.itu.int/hub/feed/", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "ITU News — official government/agency feed (un)");

RSSX(gov_opcw_news, "opcw-news", "OPCW News", "OPCW News", "government", "government",
  "https://www.opcw.org/rss.xml", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "OPCW News — official government/agency feed (un)");

RSSX(gov_worldbank_news, "worldbank-news", "World Bank News", "World Bank News", "government", "government",
  "https://www.worldbank.org/en/news/all.rss", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "World Bank News — official government/agency feed (un)");

RSSX(gov_imf_news, "imf-news", "IMF News", "IMF News", "government", "government",
  "https://www.imf.org/en/News/rss?language=eng", "en", "[\"government\",\"official\",\"un\"]", 3600,
  "IMF News — official government/agency feed (un)");

RSSX(gov_interpol_news, "interpol-news", "INTERPOL News", "INTERPOL News", "government", "government",
  "https://www.interpol.int/en/rss", "en", "[\"government\",\"official\",\"global\"]", 3600,
  "INTERPOL News — official government/agency feed (global)");

RSSX(gov_ru_mid, "ru-mid", "Russia MFA Statements", "Russia MFA Statements", "government", "government",
  "https://mid.ru/en/rss/", "en", "[\"government\",\"official\",\"russia\"]", 3600,
  "Russia MFA Statements — official government/agency feed (russia)");

RSSX(gov_cn_mofa, "cn-mofa", "China MFA Press", "China MFA Press", "government", "government",
  "https://www.fmprc.gov.cn/mfa_eng/xwfw_665399/s2510_665401/rss.xml", "en", "[\"government\",\"official\",\"china\"]", 3600,
  "China MFA Press — official government/agency feed (china)");

RSSX(gov_in_pib, "in-pib", "India Press Info Bureau", "India Press Info Bureau", "government", "government",
  "https://pib.gov.in/RssMain.aspx?ModId=6&Lang=1&Regid=3", "en", "[\"government\",\"official\",\"india\"]", 3600,
  "India Press Info Bureau — official government/agency feed (india)");

RSSX(gov_au_dfat, "au-dfat", "Australia DFAT Media", "Australia DFAT Media", "government", "government",
  "https://www.dfat.gov.au/rss/media-releases.xml", "en", "[\"government\",\"official\",\"australia\"]", 3600,
  "Australia DFAT Media — official government/agency feed (australia)");

RSSX(gov_ca_gov_news, "ca-gov-news", "Canada Government News", "Canada Government News", "government", "government",
  "https://api.io.canada.ca/io-server/gc/news/en/v2?sort=publishedDate&orderBy=desc&format=atom", "en", "[\"government\",\"official\",\"canada\"]", 3600,
  "Canada Government News — official government/agency feed (canada)");

RSSX(gov_nato_opinions, "nato-opinions", "NATO Opinions & Speeches", "NATO Opinions & Speeches", "government", "government",
  "https://www.nato.int/cps/en/natohq/opinions.rss", "en", "[\"government\",\"official\",\"global\"]", 3600,
  "NATO Opinions & Speeches — official government/agency feed (global)");

RSSX(gov_cisa_blog, "cisa-blog", "CISA Blog", "CISA Blog", "government", "government",
  "https://www.cisa.gov/blog.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "CISA Blog — official government/agency feed (usa)");

RSSX(gov_fema_news, "fema-news", "US FEMA News", "US FEMA News", "government", "government",
  "https://www.fema.gov/feeds/news.rss", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US FEMA News — official government/agency feed (usa)");

RSSX(gov_epa_news, "epa-news", "US EPA News", "US EPA News", "government", "government",
  "https://www.epa.gov/newsreleases/search/rss", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US EPA News — official government/agency feed (usa)");

RSSX(gov_faa_news, "faa-news", "US FAA News", "US FAA News", "government", "government",
  "https://www.faa.gov/newsroom/rss", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US FAA News — official government/agency feed (usa)");

RSSX(gov_ntsb_news, "ntsb-news", "US NTSB News", "US NTSB News", "government", "government",
  "https://www.ntsb.gov/news/press-releases/Pages/rss.aspx", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "US NTSB News — official government/agency feed (usa)");

RSSX(gov_noaa_swpc, "noaa-swpc", "NOAA Space Weather Alerts", "NOAA Space Weather Alerts", "government", "government",
  "https://www.swpc.noaa.gov/rss.xml", "en", "[\"government\",\"official\",\"usa\"]", 3600,
  "NOAA Space Weather Alerts — official government/agency feed (usa)");
