/* Government / agency / IGO press and advisory feeds worldwide (best-effort real RSS). Official primary-source intel, via rss_collect. */

/* 2026-09-08 EMITS_NOTHING triage of the 28 rows this file was reported for. Every URL
   below was fetched live from this host with a full browser User-Agent.
   Repaired in place (see per-row notes): ema-news, frontex-news.

   ANSWERING NORMALLY RIGHT NOW — the reported EMITS_NOTHING was a transient upstream
   outage, not a defect; nothing was changed:
     cisa-blog        200 application/rss+xml, 10 items
     europarl-news    200 application/rss+xml, 10 items
     in-pib           200 text/xml, 20 items (upstream 302s to a longer query string)
     us-ftc-press     200 application/rss+xml, 10 items
     us-state-press   200 application/rss+xml, 10 items

   Diagnosed, NOT repairable by changing the URL. Feed retired, HTTP 404, and no
   replacement is advertised by the site (checked link rel=alternate on the newsroom
   page, and the site's own /rss landing page where one exists):
     au-dfat, eeas-news, enisa-news, faa-news, nato-opinions, noaa-swpc, osce-news,
     unhcr-news, unodc-news, us-dhs-news, us-treasury-press, wfp-news, worldbank-news
   Bot wall / WAF refusal, so the path may still be correct and only the fetch is
   blocked — these would revive unchanged behind a fetch path that gets past it:
     fema-news 403, imf-news 403, iom-news 403, interpol-news 503, epa-news 405
   Upstream shape change, needs a non-RSS collector rather than a new URL:
     ca-gov-news  200 application/atom+xml but a 405-byte feed with ZERO entry
                  elements, "<updated>Invalid date</updated>", and a self link pointing
                  at http://localhost:8181 — the API is misconfigured upstream.
     cn-mofa      302s to www.mfa.gov.cn/web/system/index_17321.shtml, an HTML page.
     ntsb-news    the .aspx path now returns an HTML page, not RSS.
   None of these five are fixable here without inventing content, so they are left
   registered and emitting an honest nothing rather than repointed at a guess. */

/* 2026-09-11 RE-VERIFICATION of the 27 rows the next sweep still reported, this
   time with the EXACT headers rss_collect sends (its RSS/any Accept header and
   the JapanOSINT/1.0 contact UA) rather than a browser UA — which is the
   measurement that matters, because several of these hosts answer one and refuse
   the other. Three corrections and one repair:

   1. ca-gov-news is NOT an upstream misconfiguration. It defaults to a page size
      of zero; `&pick=500` returns 500 real entries. REPAIRED in place below.

   2. The five rows recorded above as "answering normally, transient outage" do
      NOT answer this collector. With rss_collect's own headers, today:
        cisa-blog 403, us-ftc-press 403, us-state-press 403 (all three also 403
        to a bare `curl` UA — a generic bot wall, not our token)
        europarl-news  HTTP 202 with a ZERO-BYTE body, on every path and every
        retry — a WAF holding pattern that rss_collect counts as success (any
        2xx), so the run reports rc=0 with nothing stored
        in-pib is the interesting one: 403 with our UA, 200 with 20 items to the
        bare product token `JapanOSINT/1.0`. Reproduced 3x each. The discriminator
        is the "(+https://...)" contact parenthetical in lib/rss_atom.c's shared
        UA, the same class of refusal the ReliefWeb note in that file documents.
        There is no per-source UA hook on the RSSX path, so it cannot be fixed
        from this file; it is a live source lost to a header we control.

   3. The "retired, no replacement" set was re-checked against each site's
      current newsroom and feed-index pages (link rel=alternate plus every
      href containing rss/feed/atom) and stands: eeas-news, enisa-news,
      faa-news, iom-news, nato-opinions, noaa-swpc, osce-news, unodc-news,
      wfp-news, worldbank-news, us-dhs-news(403), unhcr-news(403),
      us-treasury-press (now a 404 HTML page, no longer a timeout).
      Candidate paths were tried and each 404'd; none is guessed into the tree.
      au-dfat could not be measured at all — the TLS connection to
      www.dfat.gov.au never completes (HTTP/2 INTERNAL_ERROR, then timeouts on
      HTTP/1.1), so it is recorded as unreachable rather than as retired.
      noaa-swpc's content is not lost: SWPC's JSON products are already
      collected by ~30 vsrc_science_* rows, and repointing this row at one of
      them would be a duplicate endpoint.
      frontex-news likewise stays dead-by-design — see its own note below. */
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

/* 2026-09-08 EMITS_NOTHING fix: /en/rss.xml 404s. EMA's own feed index
   (/en/news-events/rss-feeds) lists twenty live feeds; /en/news.xml is the news one.
   Verified live: HTTP 200, application/rss+xml, 3 populated item elements. */
RSSX(gov_ema_news, "ema-news", "European Medicines Agency", "European Medicines Agency", "government", "government",
  "https://www.ema.europa.eu/en/news.xml", "en", "[\"government\",\"official\",\"eu\"]", 3600,
  "European Medicines Agency — official government/agency feed (eu)");

/* 2026-09-08: /rss/news/ 404s and the live feed the news-release page advertises
 * is ALREADY collected by `sec-frontex-news`
 * (collectors/feed/generated/vsrc_geopolitics_1.c). Re-pointing this row there
 * would have two registered sources polling one endpoint and storing the same
 * entries under two source_ids — `make lint-sources` caught it as a
 * dup-endpoint regression. So this row stays on its dead URL and is reported
 * dead; the CONTENT is not lost, it arrives via sec-frontex-news. Retiring
 * this row or aliasing it to that id is a decision, not a repair. */
RSSX(gov_frontex_news, "frontex-news", "Frontex News", "Frontex News", "government", "government",
  "https://frontex.europa.eu/rss/news/", "en", "[\"government\",\"official\",\"eu\"]", 3600,
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

/* 2026-09-11 EMITS_NOTHING fix, and the earlier triage above was wrong about
 * this one: the API is not misconfigured, it just defaults to a page size of
 * ZERO. Without `pick` it answers HTTP 200 with a 405-byte Atom document that
 * has no <entry> at all — which is why the run looked successful and stored
 * nothing. Measured the same minute, same headers, one parameter added:
 *   ...&format=atom            200, 405 bytes,     0 entries
 *   ...&pick=25&format=atom    200, 5,443 bytes,  25 entries
 *   ...&pick=100&format=atom   200, 15,770 bytes, 100 entries
 *   ...&pick=500&format=atom   200, 79,327 bytes, 500 entries, 500 distinct ids
 * Entries carry real titles, canada.ca links, summaries and departmental
 * authors. 500 is chosen because it is also rss_collect's own per-run item
 * ceiling (JO_RSS_MAX_ITEMS), so this asks for exactly as much as the RSS path
 * can carry and nothing is dropped at the seam. The localhost:8181 self-link
 * the upstream emits is cosmetic and does not affect the entries.
 *
 * OVERLAP, stated rather than hidden: collectors/feed/generated/vsrc_government_1.c
 * polls the same unfiltered feed at `pick=50`. The two rows differ only in page
 * size, so this row is a strict superset and 50 entries per hour are stored under
 * both source ids. lint-sources does not see it (the query strings differ) and the
 * overlap predates this change — the row was always pointed at this feed, it just
 * returned nothing. Retiring one of the two is a judgement for a human, not a
 * repair to make silently. */
RSSX(gov_ca_gov_news, "ca-gov-news", "Canada Government News", "Canada Government News", "government", "government",
  "https://api.io.canada.ca/io-server/gc/news/en/v2?sort=publishedDate&orderBy=desc&pick=500&format=atom", "en", "[\"government\",\"official\",\"canada\"]", 3600,
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
