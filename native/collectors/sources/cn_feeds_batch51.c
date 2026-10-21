/* collectors/sources/cn_feeds_batch51.c
 * Batch 51 — Hong Kong and Macao government feeds plus China News Service:
 * 136 RSS feeds, each fetched live on 2026-10-21 (HTTP 200, an RSS/Atom
 * root, at least one item) and absent from the tree before this batch.
 *
 * Found by READING the publishers' own feed directories, not guessing paths:
 * GovHK's RSS list (www.gov.hk/en/about/rss.htm and its tc twin), the HKMA's
 * RSS page, China News Service's /rss/ index, and — for 306 HK/Macao
 * government hosts from Wikidata P856 — the page each homepage links as "RSS".
 * Mainland ministries publish on WeChat, not RSS, which is why this is mostly
 * HK and Macao. Per-row readings: docs/verified-sources-batch51.tsv. */
#include "_source_macros.inc"

/* ── environment ─────────────────────────────────────────────────────── */

RSSX(cn_b51_aqhi_gov_hk_epd_ddata_html_out_aqhirss_cht, "cn-b51-aqhi-gov-hk-epd-ddata-html-out-aqhirss-cht",
     "環境保護署 - 空氣質素健康指數", "環境保護署 - 空氣質素健康指數",
     "environment", "environment",
     "https://www.aqhi.gov.hk/epd/ddata/html/out/aqhirss_ChT.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:環境保護署 - 空氣質素健康指數\",\"environment\",\"batch:51\"]",
     3600,
     "環境保護署 - 空氣質素健康指數 — feed listed in the publisher's own RSS directory; 2 entries at probe 2026-10-21")

RSSX(cn_b51_aqhi_gov_hk_epd_ddata_html_out_aqhirss_eng, "cn-b51-aqhi-gov-hk-epd-ddata-html-out-aqhirss-eng",
     "Environmental Protection Department - AQHI", "Environmental Protection Department - AQHI",
     "environment", "environment",
     "https://www.aqhi.gov.hk/epd/ddata/html/out/aqhirss_Eng.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Environmental Protection Department - AQHI\",\"environment\",\"batch:51\"]",
     3600,
     "Environmental Protection Department - AQHI — feed listed in the publisher's own RSS directory; 2 entries at probe 2026-10-21")

RSSX(cn_b51_epd_gov_hk_epd_english_rss_feeds_events_acts, "cn-b51-epd-gov-hk-epd-english-rss-feeds-events-acts",
     "Events and Activities of Environmental Protection Department", "Events and Activities of Environmental Protection Department",
     "environment", "environment",
     "https://www.epd.gov.hk/epd/english/rss_feeds/events_acts.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Events and Activities of Environmental Protection Department\",\"environment\",\"batch:51\"]",
     3600,
     "Events and Activities of Environmental Protection Department — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_epd_gov_hk_epd_english_rss_feeds_highlights, "cn-b51-epd-gov-hk-epd-english-rss-feeds-highlights",
     "Highlights of Environmental Protection Department", "Highlights of Environmental Protection Department",
     "environment", "environment",
     "https://www.epd.gov.hk/epd/english/rss_feeds/highlights.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Highlights of Environmental Protection Department\",\"environment\",\"batch:51\"]",
     3600,
     "Highlights of Environmental Protection Department — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_epd_gov_hk_epd_tc_chi_rss_feeds_events_acts, "cn-b51-epd-gov-hk-epd-tc-chi-rss-feeds-events-acts",
     "環境保護署活動", "環境保護署活動",
     "environment", "environment",
     "https://www.epd.gov.hk/epd/tc_chi/rss_feeds/events_acts.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:環境保護署活動\",\"environment\",\"batch:51\"]",
     3600,
     "環境保護署活動 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_epd_gov_hk_epd_tc_chi_rss_feeds_highlights, "cn-b51-epd-gov-hk-epd-tc-chi-rss-feeds-highlights",
     "環境保護署重點消息", "環境保護署重點消息",
     "environment", "environment",
     "https://www.epd.gov.hk/epd/tc_chi/rss_feeds/highlights.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:環境保護署重點消息\",\"environment\",\"batch:51\"]",
     3600,
     "環境保護署重點消息 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_rss_smg_gov_mo_c_actualweather_rss, "cn-b51-rss-smg-gov-mo-c-actualweather-rss",
     "澳門天氣報告", "澳門天氣報告",
     "environment", "environment",
     "http://rss.smg.gov.mo/c_ActualWeather_rss.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:澳門天氣報告\",\"environment\",\"batch:51\"]",
     3600,
     "澳門天氣報告 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_rss_smg_gov_mo_c_temperaturealert_rss, "cn-b51-rss-smg-gov-mo-c-temperaturealert-rss",
     "冷暖天氣提示", "冷暖天氣提示",
     "environment", "environment",
     "http://rss.smg.gov.mo/c_temperatureAlert_rss.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:冷暖天氣提示\",\"environment\",\"batch:51\"]",
     3600,
     "冷暖天氣提示 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_rss_smg_gov_mo_c_wcoastal_forecast_rss, "cn-b51-rss-smg-gov-mo-c-wcoastal-forecast-rss",
     "澳門海上天氣預報", "澳門海上天氣預報",
     "environment", "environment",
     "http://rss.smg.gov.mo/c_WCoastal_forecast_rss.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:澳門海上天氣預報\",\"environment\",\"batch:51\"]",
     3600,
     "澳門海上天氣預報 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_rss_smg_gov_mo_c_wforecast_rss, "cn-b51-rss-smg-gov-mo-c-wforecast-rss",
     "澳門天氣預報", "澳門天氣預報",
     "environment", "environment",
     "http://rss.smg.gov.mo/c_WForecast_rss.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:澳門天氣預報\",\"environment\",\"batch:51\"]",
     3600,
     "澳門天氣預報 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_rss_smg_gov_mo_c_wforecast7days_rss, "cn-b51-rss-smg-gov-mo-c-wforecast7days-rss",
     "澳門七日天氣預報", "澳門七日天氣預報",
     "environment", "environment",
     "http://rss.smg.gov.mo/c_WForecast7days_rss.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:澳門七日天氣預報\",\"environment\",\"batch:51\"]",
     3600,
     "澳門七日天氣預報 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_rss_smg_gov_mo_c_wsignal_rss, "cn-b51-rss-smg-gov-mo-c-wsignal-rss",
     "澳門天氣警告", "澳門天氣警告",
     "environment", "environment",
     "http://rss.smg.gov.mo/c_WSignal_rss.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:澳門天氣警告\",\"environment\",\"batch:51\"]",
     3600,
     "澳門天氣警告 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_rss_weather_gov_hk_rss_severaldaysweatherforecast_uc, "cn-b51-rss-weather-gov-hk-rss-severaldaysweatherforecast-uc",
     "九天天氣預報", "九天天氣預報",
     "environment", "environment",
     "https://rss.weather.gov.hk/rss/SeveralDaysWeatherForecast_uc.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:九天天氣預報\",\"environment\",\"batch:51\"]",
     3600,
     "九天天氣預報 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

/* ── finreg ──────────────────────────────────────────────────────────── */

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_circulars, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-circulars",
     "HKMA: Circulars", "HKMA: Circulars",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_circulars.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Circulars\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Circulars — feed listed in the publisher's own RSS directory; 215 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_code_of_practice, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-code-of-practice",
     "HKMA: Code of Practice", "HKMA: Code of Practice",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_code-of-practice.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Code of Practice\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Code of Practice — feed listed in the publisher's own RSS directory; 7 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_consultations, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-consultations",
     "HKMA: Consultations", "HKMA: Consultations",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_consultations.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Consultations\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Consultations — feed listed in the publisher's own RSS directory; 47 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_daily_monetary_statistics, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-daily-monetary-statistics",
     "HKMA: Daily Monetary Statistics", "HKMA: Daily Monetary Statistics",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_daily-monetary-statistics.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Daily Monetary Statistics\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Daily Monetary Statistics — feed listed in the publisher's own RSS directory; 246 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_explanatory_note, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-explanatory-note",
     "HKMA: Explanatory Note", "HKMA: Explanatory Note",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_explanatory-note.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Explanatory Note\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Explanatory Note — feed listed in the publisher's own RSS directory; 3 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_guide_to_authorization, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-guide-to-authorization",
     "HKMA: Guide to Authorization", "HKMA: Guide to Authorization",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_guide-to-authorization.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Guide to Authorization\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Guide to Authorization — feed listed in the publisher's own RSS directory; 8 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_guidelines, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-guidelines",
     "HKMA: Guidelines", "HKMA: Guidelines",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_guidelines.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Guidelines\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Guidelines — feed listed in the publisher's own RSS directory; 6 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_insight, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-insight",
     "HKMA: inSight", "HKMA: inSight",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_insight.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: inSight\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: inSight — feed listed in the publisher's own RSS directory; 8 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_join_us, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-join-us",
     "HKMA: Join Us", "HKMA: Join Us",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_join-us.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Join Us\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Join Us — feed listed in the publisher's own RSS directory; 9 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_legislative_council_issues, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-legislative-council-issues",
     "HKMA: Legislative Council Issues", "HKMA: Legislative Council Issues",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_legislative-council-issues.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Legislative Council Issues\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Legislative Council Issues — feed listed in the publisher's own RSS directory; 2 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_monthly_statistical_bulletin, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-monthly-statistical-bulletin",
     "HKMA: Monthly Statistical Bulletin", "HKMA: Monthly Statistical Bulletin",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_monthly-statistical-bulletin.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Monthly Statistical Bulletin\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Monthly Statistical Bulletin — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_practice_note, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-practice-note",
     "HKMA: Practice Note", "HKMA: Practice Note",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_practice-note.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Practice Note\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Practice Note — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_press_release, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-press-release",
     "HKMA: Press Releases", "HKMA: Press Releases",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_press-release.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Press Releases\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Press Releases — feed listed in the publisher's own RSS directory; 426 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_publications, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-publications",
     "HKMA: Publications", "HKMA: Publications",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_publications.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Publications\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Publications — feed listed in the publisher's own RSS directory; 8 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_regtech_knowledge_hub, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-regtech-knowledge-hub",
     "HKMA: Regtech Knowledge Hub", "HKMA: Regtech Knowledge Hub",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_regtech-knowledge-hub.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Regtech Knowledge Hub\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Regtech Knowledge Hub — feed listed in the publisher's own RSS directory; 6 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_research, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-research",
     "HKMA: Research", "HKMA: Research",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_research.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Research\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Research — feed listed in the publisher's own RSS directory; 14 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_scam_alert, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-scam-alert",
     "HKMA: Scam Alert", "HKMA: Scam Alert",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_scam-alert.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Scam Alert\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Scam Alert — feed listed in the publisher's own RSS directory; 281 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_speeches, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-speeches",
     "HKMA: Speeches", "HKMA: Speeches",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_speeches.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Speeches\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Speeches — feed listed in the publisher's own RSS directory; 47 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_supervisory_policy_manual, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-supervisory-policy-manual",
     "HKMA: Supervisory Policy Manual", "HKMA: Supervisory Policy Manual",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_supervisory-policy-manual.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Supervisory Policy Manual\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Supervisory Policy Manual — feed listed in the publisher's own RSS directory; 44 entries at probe 2026-10-21")

RSSX(cn_b51_hkma_gov_hk_eng_other_information_rss_rss_tender_invitations, "cn-b51-hkma-gov-hk-eng-other-information-rss-rss-tender-invitations",
     "HKMA: Tender Invitations", "HKMA: Tender Invitations",
     "finreg", "finance",
     "https://www.hkma.gov.hk/eng/other-information/rss/rss_tender-invitations.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:HKMA: Tender Invitations\",\"finance\",\"batch:51\"]",
     3600,
     "HKMA: Tender Invitations — feed listed in the publisher's own RSS directory; 7 entries at probe 2026-10-21")

/* ── government ──────────────────────────────────────────────────────── */

RSSX(cn_b51_consumer_gov_mo_news_rss_aspx_lang_zh, "cn-b51-consumer-gov-mo-news-rss-aspx-lang-zh",
     "澳門特別行政區政府消費者委員會-最新公佈", "澳門特別行政區政府消費者委員會-最新公佈",
     "government", "government",
     "https://www.consumer.gov.mo/News/rss.aspx?lang=zh",
     "zh", "[\"gov\",\"country:mo\",\"publisher:澳門特別行政區政府消費者委員會-最新公佈\",\"government\",\"batch:51\"]",
     3600,
     "澳門特別行政區政府消費者委員會-最新公佈 — feed listed in the publisher's own RSS directory; 300 entries at probe 2026-10-21")

RSSX(cn_b51_data_gov_hk_filestore_feeds_data_rss_tc, "cn-b51-data-gov-hk-filestore-feeds-data-rss-tc",
     "檔案的資料資訊", "檔案的資料資訊",
     "government", "government",
     "https://data.gov.hk/filestore/feeds/data_rss_tc.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:檔案的資料資訊\",\"government\",\"batch:51\"]",
     3600,
     "檔案的資料資訊 — feed listed in the publisher's own RSS directory; 145 entries at probe 2026-10-21")

RSSX(cn_b51_dsal_gov_mo_rss_zh_tw_concurso_publico, "cn-b51-dsal-gov-mo-rss-zh-tw-concurso-publico",
     "dsal.gov.mo — 公開招標", "dsal.gov.mo — 公開招標",
     "government", "government",
     "http://www.dsal.gov.mo/rss/zh_tw/concurso_publico.rss",
     "zh", "[\"gov\",\"country:mo\",\"publisher:dsal.gov.mo — 公開招標\",\"government\",\"batch:51\"]",
     3600,
     "dsal.gov.mo — 公開招標 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_dsal_gov_mo_rss_zh_tw_news, "cn-b51-dsal-gov-mo-rss-zh-tw-news",
     "dsal.gov.mo — 最新消息", "dsal.gov.mo — 最新消息",
     "government", "government",
     "http://www.dsal.gov.mo/rss/zh_tw/news.rss",
     "zh", "[\"gov\",\"country:mo\",\"publisher:dsal.gov.mo — 最新消息\",\"government\",\"batch:51\"]",
     3600,
     "dsal.gov.mo — 最新消息 — feed listed in the publisher's own RSS directory; 50 entries at probe 2026-10-21")

RSSX(cn_b51_dsal_gov_mo_rss_zh_tw_personnel_entry, "cn-b51-dsal-gov-mo-rss-zh-tw-personnel-entry",
     "dsal.gov.mo — 入職開考", "dsal.gov.mo — 入職開考",
     "government", "government",
     "http://www.dsal.gov.mo/rss/zh_tw/personnel_entry.rss",
     "zh", "[\"gov\",\"country:mo\",\"publisher:dsal.gov.mo — 入職開考\",\"government\",\"batch:51\"]",
     3600,
     "dsal.gov.mo — 入職開考 — feed listed in the publisher's own RSS directory; 1 entries at probe 2026-10-21")

RSSX(cn_b51_dsat_gov_mo_bus_tc_bus_croad_rss, "cn-b51-dsat-gov-mo-bus-tc-bus-croad-rss",
     "交通事務局 - 巴士改道訊息", "交通事務局 - 巴士改道訊息",
     "government", "government",
     "https://www.dsat.gov.mo/bus/tc/bus_croad_rss.aspx",
     "zh", "[\"gov\",\"country:mo\",\"publisher:交通事務局 - 巴士改道訊息\",\"government\",\"batch:51\"]",
     3600,
     "交通事務局 - 巴士改道訊息 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_dsat_gov_mo_bus_tc_bus_news_rss, "cn-b51-dsat-gov-mo-bus-tc-bus-news-rss",
     "交通事務局 - 巴士新聞", "交通事務局 - 巴士新聞",
     "government", "government",
     "https://www.dsat.gov.mo/bus/tc/bus_news_rss.aspx",
     "zh", "[\"gov\",\"country:mo\",\"publisher:交通事務局 - 巴士新聞\",\"government\",\"batch:51\"]",
     3600,
     "交通事務局 - 巴士新聞 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_dsat_gov_mo_tc_emergency_rss, "cn-b51-dsat-gov-mo-tc-emergency-rss",
     "交通事務局 - 特別消息", "交通事務局 - 特別消息",
     "government", "government",
     "https://www.dsat.gov.mo/tc/emergency_rss.aspx",
     "zh", "[\"gov\",\"country:mo\",\"publisher:交通事務局 - 特別消息\",\"government\",\"batch:51\"]",
     3600,
     "交通事務局 - 特別消息 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_dsat_gov_mo_tc_event_rss, "cn-b51-dsat-gov-mo-tc-event-rss",
     "交通事務局 - 活動消息", "交通事務局 - 活動消息",
     "government", "government",
     "https://www.dsat.gov.mo/tc/event_rss.aspx",
     "zh", "[\"gov\",\"country:mo\",\"publisher:交通事務局 - 活動消息\",\"government\",\"batch:51\"]",
     3600,
     "交通事務局 - 活動消息 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_dsat_gov_mo_tc_news_rss, "cn-b51-dsat-gov-mo-tc-news-rss",
     "交通事務局 - 新聞", "交通事務局 - 新聞",
     "government", "government",
     "https://www.dsat.gov.mo/tc/news_rss.aspx",
     "zh", "[\"gov\",\"country:mo\",\"publisher:交通事務局 - 新聞\",\"government\",\"batch:51\"]",
     3600,
     "交通事務局 - 新聞 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_dsat_gov_mo_tc_notice_rss, "cn-b51-dsat-gov-mo-tc-notice-rss",
     "交通事務局 - 通告", "交通事務局 - 通告",
     "government", "government",
     "https://www.dsat.gov.mo/tc/notice_rss.aspx",
     "zh", "[\"gov\",\"country:mo\",\"publisher:交通事務局 - 通告\",\"government\",\"batch:51\"]",
     3600,
     "交通事務局 - 通告 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_dsop_gov_mo_system_rss_type_news, "cn-b51-dsop-gov-mo-system-rss-type-news",
     "公共建設局 - 最新消息", "公共建設局 - 最新消息",
     "government", "government",
     "http://www.dsop.gov.mo/system/rss/?type=news",
     "zh", "[\"gov\",\"country:mo\",\"publisher:公共建設局 - 最新消息\",\"government\",\"batch:51\"]",
     3600,
     "公共建設局 - 最新消息 — feed listed in the publisher's own RSS directory; 7 entries at probe 2026-10-21")

RSSX(cn_b51_dsop_gov_mo_system_rss_type_tender, "cn-b51-dsop-gov-mo-system-rss-type-tender",
     "公共建設局 - 招標項目", "公共建設局 - 招標項目",
     "government", "government",
     "http://www.dsop.gov.mo/system/rss/?type=tender",
     "zh", "[\"gov\",\"country:mo\",\"publisher:公共建設局 - 招標項目\",\"government\",\"batch:51\"]",
     3600,
     "公共建設局 - 招標項目 — feed listed in the publisher's own RSS directory; 8 entries at probe 2026-10-21")

RSSX(cn_b51_edb_gov_hk_en_whats_new_rss, "cn-b51-edb-gov-hk-en-whats-new-rss",
     "Latest News", "Latest News",
     "government", "government",
     "https://www.edb.gov.hk/en/whats_new_rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Latest News\",\"government\",\"batch:51\"]",
     3600,
     "Latest News — feed listed in the publisher's own RSS directory; 200 entries at probe 2026-10-21")

RSSX(cn_b51_edb_gov_hk_tc_whats_new_rss, "cn-b51-edb-gov-hk-tc-whats-new-rss",
     "最新消息", "最新消息",
     "government", "government",
     "https://www.edb.gov.hk/tc/whats_new_rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:最新消息\",\"government\",\"batch:51\"]",
     3600,
     "最新消息 — feed listed in the publisher's own RSS directory; 200 entries at probe 2026-10-21")

RSSX(cn_b51_fso_gov_hk_rss_fsblog_rss_e, "cn-b51-fso-gov-hk-rss-fsblog-rss-e",
     "Financial Secretary - My Blog", "Financial Secretary - My Blog",
     "government", "government",
     "https://www.fso.gov.hk/rss/fsblog_rss_e.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Financial Secretary - My Blog\",\"government\",\"batch:51\"]",
     3600,
     "Financial Secretary - My Blog — feed listed in the publisher's own RSS directory; 523 entries at probe 2026-10-21")

RSSX(cn_b51_fso_gov_hk_rss_fsblog_rss_t, "cn-b51-fso-gov-hk-rss-fsblog-rss-t",
     "財政司司長 - 司長隨筆", "財政司司長 - 司長隨筆",
     "government", "government",
     "https://www.fso.gov.hk/rss/fsblog_rss_t.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:財政司司長 - 司長隨筆\",\"government\",\"batch:51\"]",
     3600,
     "財政司司長 - 司長隨筆 — feed listed in the publisher's own RSS directory; 523 entries at probe 2026-10-21")

RSSX(cn_b51_hongkongpost_hk_en_about_us_whats_new_notices_notices, "cn-b51-hongkongpost-hk-en-about-us-whats-new-notices-notices",
     "Hongkong Post - Notices", "Hongkong Post - Notices",
     "government", "government",
     "https://www.hongkongpost.hk/en/about_us/whats_new/notices/notices.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Hongkong Post - Notices\",\"government\",\"batch:51\"]",
     3600,
     "Hongkong Post - Notices — feed listed in the publisher's own RSS directory; 15 entries at probe 2026-10-21")

RSSX(cn_b51_hongkongpost_hk_en_about_us_whats_new_press_release_press_release, "cn-b51-hongkongpost-hk-en-about-us-whats-new-press-release-press-release",
     "Hongkong Post - Press Releases", "Hongkong Post - Press Releases",
     "government", "government",
     "https://www.hongkongpost.hk/en/about_us/whats_new/press_release/press_release.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Hongkong Post - Press Releases\",\"government\",\"batch:51\"]",
     3600,
     "Hongkong Post - Press Releases — feed listed in the publisher's own RSS directory; 15 entries at probe 2026-10-21")

RSSX(cn_b51_hongkongpost_hk_tc_about_us_whats_new_notices_notices, "cn-b51-hongkongpost-hk-tc-about-us-whats-new-notices-notices",
     "香港郵政 - 通告", "香港郵政 - 通告",
     "government", "government",
     "https://www.hongkongpost.hk/tc/about_us/whats_new/notices/notices.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港郵政 - 通告\",\"government\",\"batch:51\"]",
     3600,
     "香港郵政 - 通告 — feed listed in the publisher's own RSS directory; 15 entries at probe 2026-10-21")

RSSX(cn_b51_hongkongpost_hk_tc_about_us_whats_new_press_release_press_release, "cn-b51-hongkongpost-hk-tc-about-us-whats-new-press-release-press-release",
     "香港郵政 - 新聞公報", "香港郵政 - 新聞公報",
     "government", "government",
     "https://www.hongkongpost.hk/tc/about_us/whats_new/press_release/press_release.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港郵政 - 新聞公報\",\"government\",\"batch:51\"]",
     3600,
     "香港郵政 - 新聞公報 — feed listed in the publisher's own RSS directory; 15 entries at probe 2026-10-21")

RSSX(cn_b51_icm_gov_mo_rss_files_rssevents_c, "cn-b51-icm-gov-mo-rss-files-rssevents-c",
     "RSS - 文化局聚合活動", "RSS - 文化局聚合活動",
     "government", "government",
     "http://www.icm.gov.mo/rss/files/rssEvents_C.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:RSS - 文化局聚合活動\",\"government\",\"batch:51\"]",
     3600,
     "RSS - 文化局聚合活動 — feed listed in the publisher's own RSS directory; 59 entries at probe 2026-10-21")

RSSX(cn_b51_icm_gov_mo_rss_files_rssnews_c, "cn-b51-icm-gov-mo-rss-files-rssnews-c",
     "RSS - 文化局聚合新聞", "RSS - 文化局聚合新聞",
     "government", "government",
     "http://www.icm.gov.mo/rss/files/rssNews_C.xml",
     "zh", "[\"gov\",\"country:mo\",\"publisher:RSS - 文化局聚合新聞\",\"government\",\"batch:51\"]",
     3600,
     "RSS - 文化局聚合新聞 — feed listed in the publisher's own RSS directory; 24 entries at probe 2026-10-21")

RSSX(cn_b51_lcsd_gov_hk_en_emagazine_emag_rss, "cn-b51-lcsd-gov-hk-en-emagazine-emag-rss",
     "LCSD eMagazine - Programme Highlights", "LCSD eMagazine - Programme Highlights",
     "government", "government",
     "https://www.lcsd.gov.hk/en/emagazine/emag_rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:LCSD eMagazine - Programme Highlights\",\"government\",\"batch:51\"]",
     3600,
     "LCSD eMagazine - Programme Highlights — feed listed in the publisher's own RSS directory; 26 entries at probe 2026-10-21")

RSSX(cn_b51_lcsd_gov_hk_tc_emagazine_emag_rss, "cn-b51-lcsd-gov-hk-tc-emagazine-emag-rss",
     "康文署電子雜誌 - 精彩節目", "康文署電子雜誌 - 精彩節目",
     "government", "government",
     "https://www.lcsd.gov.hk/tc/emagazine/emag_rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:康文署電子雜誌 - 精彩節目\",\"government\",\"batch:51\"]",
     3600,
     "康文署電子雜誌 - 精彩節目 — feed listed in the publisher's own RSS directory; 26 entries at probe 2026-10-21")

RSSX(cn_b51_mardep_gov_hk_filemanager_en_share_news_rss1, "cn-b51-mardep-gov-hk-filemanager-en-share-news-rss1",
     "Release of MSN / MSIN", "Release of MSN / MSIN",
     "government", "government",
     "https://www.mardep.gov.hk/filemanager/en/share/news/rss1.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Release of MSN / MSIN\",\"government\",\"batch:51\"]",
     3600,
     "Release of MSN / MSIN — feed listed in the publisher's own RSS directory; 11 entries at probe 2026-10-21")

RSSX(cn_b51_mardep_gov_hk_filemanager_tc_share_news_rss1c, "cn-b51-mardep-gov-hk-filemanager-tc-share-news-rss1c",
     "香港商船公告/香港商船資訊的發佈", "香港商船公告/香港商船資訊的發佈",
     "government", "government",
     "https://www.mardep.gov.hk/filemanager/tc/share/news/rss1c.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港商船公告/香港商船資訊的發佈\",\"government\",\"batch:51\"]",
     3600,
     "香港商船公告/香港商船資訊的發佈 — feed listed in the publisher's own RSS directory; 11 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_categories_admin_html_articlelist_rss, "cn-b51-news-gov-hk-en-categories-admin-html-articlelist-rss",
     "news.gov.hk - Admin & Civic Affairs", "news.gov.hk - Admin & Civic Affairs",
     "government", "government",
     "https://www.news.gov.hk/en/categories/admin/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Admin & Civic Affairs\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Admin & Civic Affairs — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_categories_environment_html_articlelist_rss, "cn-b51-news-gov-hk-en-categories-environment-html-articlelist-rss",
     "news.gov.hk - Environment", "news.gov.hk - Environment",
     "government", "government",
     "https://www.news.gov.hk/en/categories/environment/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Environment\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Environment — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_categories_finance_html_articlelist_rss, "cn-b51-news-gov-hk-en-categories-finance-html-articlelist-rss",
     "news.gov.hk - Business & Finance", "news.gov.hk - Business & Finance",
     "government", "government",
     "https://www.news.gov.hk/en/categories/finance/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Business & Finance\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Business & Finance — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_categories_health_html_articlelist_rss, "cn-b51-news-gov-hk-en-categories-health-html-articlelist-rss",
     "news.gov.hk - Health", "news.gov.hk - Health",
     "government", "government",
     "https://www.news.gov.hk/en/categories/health/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Health\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Health — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_categories_infrastructure_html_articlelist_rss, "cn-b51-news-gov-hk-en-categories-infrastructure-html-articlelist-rss",
     "news.gov.hk - Infrastructure & Logistics", "news.gov.hk - Infrastructure & Logistics",
     "government", "government",
     "https://www.news.gov.hk/en/categories/infrastructure/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Infrastructure & Logistics\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Infrastructure & Logistics — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_categories_law_order_html_articlelist_rss, "cn-b51-news-gov-hk-en-categories-law-order-html-articlelist-rss",
     "news.gov.hk - Law & Order", "news.gov.hk - Law & Order",
     "government", "government",
     "https://www.news.gov.hk/en/categories/law_order/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Law & Order\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Law & Order — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_categories_school_work_html_articlelist_rss, "cn-b51-news-gov-hk-en-categories-school-work-html-articlelist-rss",
     "news.gov.hk - At School, At Work", "news.gov.hk - At School, At Work",
     "government", "government",
     "https://www.news.gov.hk/en/categories/school_work/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - At School, At Work\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - At School, At Work — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_city_life_html_articlelist_rss, "cn-b51-news-gov-hk-en-city-life-html-articlelist-rss",
     "news.gov.hk - City Life", "news.gov.hk - City Life",
     "government", "government",
     "https://www.news.gov.hk/en/city_life/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - City Life\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - City Life — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_common_html_ticker_rss, "cn-b51-news-gov-hk-en-common-html-ticker-rss",
     "news.gov.hk - News Ticker", "news.gov.hk - News Ticker",
     "government", "government",
     "https://www.news.gov.hk/en/common/html/ticker.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - News Ticker\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - News Ticker — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_common_html_topstories_rss, "cn-b51-news-gov-hk-en-common-html-topstories-rss",
     "news.gov.hk - Top Story", "news.gov.hk - Top Story",
     "government", "government",
     "https://www.news.gov.hk/en/common/html/topstories.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Top Story\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Top Story — feed listed in the publisher's own RSS directory; 15 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_feature_index_rss, "cn-b51-news-gov-hk-en-feature-index-rss",
     "news.gov.hk - Features", "news.gov.hk - Features",
     "government", "government",
     "https://www.news.gov.hk/en/feature/index.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - Features\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - Features — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_en_record_html_articlelist_rss, "cn-b51-news-gov-hk-en-record-html-articlelist-rss",
     "news.gov.hk - On The Record", "news.gov.hk - On The Record",
     "government", "government",
     "https://www.news.gov.hk/en/record/html/articlelist.rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:news.gov.hk - On The Record\",\"government\",\"batch:51\"]",
     3600,
     "news.gov.hk - On The Record — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_categories_admin_html_articlelist_rss, "cn-b51-news-gov-hk-tc-categories-admin-html-articlelist-rss",
     "香港政府新聞網 - 行政與公民事務", "香港政府新聞網 - 行政與公民事務",
     "government", "government",
     "https://www.news.gov.hk/tc/categories/admin/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 行政與公民事務\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 行政與公民事務 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_categories_environment_html_articlelist_rss, "cn-b51-news-gov-hk-tc-categories-environment-html-articlelist-rss",
     "香港政府新聞網 - 環境", "香港政府新聞網 - 環境",
     "government", "government",
     "https://www.news.gov.hk/tc/categories/environment/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 環境\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 環境 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_categories_finance_html_articlelist_rss, "cn-b51-news-gov-hk-tc-categories-finance-html-articlelist-rss",
     "香港政府新聞網 - 財經", "香港政府新聞網 - 財經",
     "government", "government",
     "https://www.news.gov.hk/tc/categories/finance/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 財經\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 財經 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_categories_health_html_articlelist_rss, "cn-b51-news-gov-hk-tc-categories-health-html-articlelist-rss",
     "香港政府新聞網 - 社區與健康", "香港政府新聞網 - 社區與健康",
     "government", "government",
     "https://www.news.gov.hk/tc/categories/health/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 社區與健康\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 社區與健康 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_categories_infrastructure_html_articlelist_rss, "cn-b51-news-gov-hk-tc-categories-infrastructure-html-articlelist-rss",
     "香港政府新聞網 - 基建與物流", "香港政府新聞網 - 基建與物流",
     "government", "government",
     "https://www.news.gov.hk/tc/categories/infrastructure/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 基建與物流\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 基建與物流 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_categories_law_order_html_articlelist_rss, "cn-b51-news-gov-hk-tc-categories-law-order-html-articlelist-rss",
     "香港政府新聞網 - 治安", "香港政府新聞網 - 治安",
     "government", "government",
     "https://www.news.gov.hk/tc/categories/law_order/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 治安\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 治安 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_categories_school_work_html_articlelist_rss, "cn-b51-news-gov-hk-tc-categories-school-work-html-articlelist-rss",
     "香港政府新聞網 - 教育與就業", "香港政府新聞網 - 教育與就業",
     "government", "government",
     "https://www.news.gov.hk/tc/categories/school_work/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 教育與就業\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 教育與就業 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_city_life_html_articlelist_rss, "cn-b51-news-gov-hk-tc-city-life-html-articlelist-rss",
     "香港政府新聞網 - 都會生活", "香港政府新聞網 - 都會生活",
     "government", "government",
     "https://www.news.gov.hk/tc/city_life/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 都會生活\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 都會生活 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_common_html_ticker_rss, "cn-b51-news-gov-hk-tc-common-html-ticker-rss",
     "香港政府新聞網 - 新聞速遞", "香港政府新聞網 - 新聞速遞",
     "government", "government",
     "https://www.news.gov.hk/tc/common/html/ticker.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 新聞速遞\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 新聞速遞 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_common_html_topstories_rss, "cn-b51-news-gov-hk-tc-common-html-topstories-rss",
     "香港政府新聞網 - 重要新聞", "香港政府新聞網 - 重要新聞",
     "government", "government",
     "https://www.news.gov.hk/tc/common/html/topstories.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 重要新聞\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 重要新聞 — feed listed in the publisher's own RSS directory; 15 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_feature_index_rss, "cn-b51-news-gov-hk-tc-feature-index-rss",
     "香港政府新聞網 - 特寫", "香港政府新聞網 - 特寫",
     "government", "government",
     "https://www.news.gov.hk/tc/feature/index.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 特寫\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 特寫 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_news_gov_hk_tc_record_html_articlelist_rss, "cn-b51-news-gov-hk-tc-record-html-articlelist-rss",
     "香港政府新聞網 - 政府評論", "香港政府新聞網 - 政府評論",
     "government", "government",
     "https://www.news.gov.hk/tc/record/html/articlelist.rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:香港政府新聞網 - 政府評論\",\"government\",\"batch:51\"]",
     3600,
     "香港政府新聞網 - 政府評論 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_ofca_gov_hk_filemanager_ofca_en_rss_index, "cn-b51-ofca-gov-hk-filemanager-ofca-en-rss-index",
     "Office of the Communications Authority : What's New", "Office of the Communications Authority : What's New",
     "government", "government",
     "https://www.ofca.gov.hk/filemanager/ofca/en/rss/index.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Office of the Communications Authority : What's New\",\"government\",\"batch:51\"]",
     3600,
     "Office of the Communications Authority : What's New — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_ofca_gov_hk_filemanager_ofca_tc_rss_index, "cn-b51-ofca-gov-hk-filemanager-ofca-tc-rss-index",
     "通訊事務管理局辦公室 : 最新消息", "通訊事務管理局辦公室 : 最新消息",
     "government", "government",
     "https://www.ofca.gov.hk/filemanager/ofca/tc/rss/index.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:通訊事務管理局辦公室 : 最新消息\",\"government\",\"batch:51\"]",
     3600,
     "通訊事務管理局辦公室 : 最新消息 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

/* ── health ──────────────────────────────────────────────────────────── */

RSSX(cn_b51_chp_gov_hk_rss_cdwatch_en_rss, "cn-b51-chp-gov-hk-rss-cdwatch-en-rss",
     "Centre for Health Protection, Communicable Diseases Watch (CD Watch)", "Centre for Health Protection, Communicable Diseases Watch (CD Watch)",
     "health", "health",
     "https://www.chp.gov.hk/rss/cdwatch_en_RSS.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Centre for Health Protection, Communicable Diseases Watch (CD Watch)\",\"health\",\"batch:51\"]",
     3600,
     "Centre for Health Protection, Communicable Diseases Watch (CD Watch) — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_cdwatch_zh_tw_rss, "cn-b51-chp-gov-hk-rss-cdwatch-zh-tw-rss",
     "衞生防護中心, 傳染病直擊 (只備英文版)", "衞生防護中心, 傳染病直擊 (只備英文版)",
     "health", "health",
     "https://www.chp.gov.hk/rss/cdwatch_zh_tw_RSS.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:衞生防護中心, 傳染病直擊 (只備英文版)\",\"health\",\"batch:51\"]",
     3600,
     "衞生防護中心, 傳染病直擊 (只備英文版) — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_ncdaware_en_rss, "cn-b51-chp-gov-hk-rss-ncdaware-en-rss",
     "Centre for Health Protection, Non-Communicable Diseases Watch (NCD Watch)", "Centre for Health Protection, Non-Communicable Diseases Watch (NCD Watch)",
     "health", "health",
     "https://www.chp.gov.hk/rss/ncdaware_en_RSS.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Centre for Health Protection, Non-Communicable Diseases Watch (NCD Watch)\",\"health\",\"batch:51\"]",
     3600,
     "Centre for Health Protection, Non-Communicable Diseases Watch (NCD Watch) — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_ncdaware_zh_tw_rss, "cn-b51-chp-gov-hk-rss-ncdaware-zh-tw-rss",
     "衞生防護中心, 非傳染病直擊", "衞生防護中心, 非傳染病直擊",
     "health", "health",
     "https://www.chp.gov.hk/rss/ncdaware_zh_tw_RSS.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:衞生防護中心, 非傳染病直擊\",\"health\",\"batch:51\"]",
     3600,
     "衞生防護中心, 非傳染病直擊 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_newsletter_en_rss, "cn-b51-chp-gov-hk-rss-newsletter-en-rss",
     "Centre for Health Protection, CHP newsletter", "Centre for Health Protection, CHP newsletter",
     "health", "health",
     "https://www.chp.gov.hk/rss/newsletter_en_RSS.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Centre for Health Protection, CHP newsletter\",\"health\",\"batch:51\"]",
     3600,
     "Centre for Health Protection, CHP newsletter — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_newsletter_zh_tw_rss, "cn-b51-chp-gov-hk-rss-newsletter-zh-tw-rss",
     "衞生防護中心, CHP通訊", "衞生防護中心, CHP通訊",
     "health", "health",
     "https://www.chp.gov.hk/rss/newsletter_zh_tw_RSS.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:衞生防護中心, CHP通訊\",\"health\",\"batch:51\"]",
     3600,
     "衞生防護中心, CHP通訊 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_pressreleases_en_rss, "cn-b51-chp-gov-hk-rss-pressreleases-en-rss",
     "Centre for Health Protection, Press Releases and Feature Topics", "Centre for Health Protection, Press Releases and Feature Topics",
     "health", "health",
     "https://www.chp.gov.hk/rss/pressreleases_en_RSS.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Centre for Health Protection, Press Releases and Feature Topics\",\"health\",\"batch:51\"]",
     3600,
     "Centre for Health Protection, Press Releases and Feature Topics — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_pressreleases_zh_tw_rss, "cn-b51-chp-gov-hk-rss-pressreleases-zh-tw-rss",
     "衞生防護中心, 新聞稿及專題", "衞生防護中心, 新聞稿及專題",
     "health", "health",
     "https://www.chp.gov.hk/rss/pressreleases_zh_tw_RSS.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:衞生防護中心, 新聞稿及專題\",\"health\",\"batch:51\"]",
     3600,
     "衞生防護中心, 新聞稿及專題 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_publication_en_rss, "cn-b51-chp-gov-hk-rss-publication-en-rss",
     "Centre for Health Protection, Periodicals and Publications", "Centre for Health Protection, Periodicals and Publications",
     "health", "health",
     "https://www.chp.gov.hk/rss/publication_en_RSS.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Centre for Health Protection, Periodicals and Publications\",\"health\",\"batch:51\"]",
     3600,
     "Centre for Health Protection, Periodicals and Publications — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_publication_zh_tw_rss, "cn-b51-chp-gov-hk-rss-publication-zh-tw-rss",
     "衞生防護中心, 期刊及刊物", "衞生防護中心, 期刊及刊物",
     "health", "health",
     "https://www.chp.gov.hk/rss/publication_zh_tw_RSS.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:衞生防護中心, 期刊及刊物\",\"health\",\"batch:51\"]",
     3600,
     "衞生防護中心, 期刊及刊物 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_tne_event_en_rss, "cn-b51-chp-gov-hk-rss-tne-event-en-rss",
     "Centre for Health Protection, Events", "Centre for Health Protection, Events",
     "health", "health",
     "https://www.chp.gov.hk/rss/tne_event_en_RSS.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Centre for Health Protection, Events\",\"health\",\"batch:51\"]",
     3600,
     "Centre for Health Protection, Events — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chp_gov_hk_rss_tne_event_zh_tw_rss, "cn-b51-chp-gov-hk-rss-tne-event-zh-tw-rss",
     "衞生防護中心, 活動", "衞生防護中心, 活動",
     "health", "health",
     "https://www.chp.gov.hk/rss/tne_event_zh_tw_RSS.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:衞生防護中心, 活動\",\"health\",\"batch:51\"]",
     3600,
     "衞生防護中心, 活動 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

/* ── news ────────────────────────────────────────────────────────────── */

RSSX(cn_b51_chinanews_com_cn_rss_aseaninfo, "cn-b51-chinanews-com-cn-rss-aseaninfo",
     "中新网中国—东盟商贸资讯平台", "中新网中国—东盟商贸资讯平台",
     "news", "media",
     "https://www.chinanews.com.cn/rss/aseaninfo.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网中国—东盟商贸资讯平台\",\"media\",\"batch:51\"]",
     3600,
     "中新网中国—东盟商贸资讯平台 — feed listed in the publisher's own RSS directory; 12 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_china, "cn-b51-chinanews-com-cn-rss-china",
     "中新网时政新闻", "中新网时政新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/china.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网时政新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网时政新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_chinese, "cn-b51-chinanews-com-cn-rss-chinese",
     "中新网华人新闻", "中新网华人新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/chinese.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网华人新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网华人新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_chuangyi, "cn-b51-chinanews-com-cn-rss-chuangyi",
     "中新网创意", "中新网创意",
     "news", "media",
     "https://www.chinanews.com.cn/rss/chuangyi.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网创意\",\"media\",\"batch:51\"]",
     3600,
     "中新网创意 — feed listed in the publisher's own RSS directory; 3 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_culture, "cn-b51-chinanews-com-cn-rss-culture",
     "中新网文化新闻", "中新网文化新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/culture.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网文化新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网文化新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_dwq, "cn-b51-chinanews-com-cn-rss-dwq",
     "中新网大湾区", "中新网大湾区",
     "news", "media",
     "https://www.chinanews.com.cn/rss/dwq.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网大湾区\",\"media\",\"batch:51\"]",
     3600,
     "中新网大湾区 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_dxw, "cn-b51-chinanews-com-cn-rss-dxw",
     "中新网东西问", "中新网东西问",
     "news", "media",
     "https://www.chinanews.com.cn/rss/dxw.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网东西问\",\"media\",\"batch:51\"]",
     3600,
     "中新网东西问 — feed listed in the publisher's own RSS directory; 9 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_edu, "cn-b51-chinanews-com-cn-rss-edu",
     "中新网教育新闻", "中新网教育新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/edu.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网教育新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网教育新闻 — feed listed in the publisher's own RSS directory; 27 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_finance, "cn-b51-chinanews-com-cn-rss-finance",
     "中新网财经新闻", "中新网财经新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/finance.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网财经新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网财经新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_fz, "cn-b51-chinanews-com-cn-rss-fz",
     "中新网法治新闻", "中新网法治新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/fz.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网法治新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网法治新闻 — feed listed in the publisher's own RSS directory; 17 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_importnews, "cn-b51-chinanews-com-cn-rss-importnews",
     "中新网要闻导读", "中新网要闻导读",
     "news", "media",
     "https://www.chinanews.com.cn/rss/importnews.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网要闻导读\",\"media\",\"batch:51\"]",
     3600,
     "中新网要闻导读 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_jk, "cn-b51-chinanews-com-cn-rss-jk",
     "中新网健康", "中新网健康",
     "news", "media",
     "https://www.chinanews.com.cn/rss/jk.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网健康\",\"media\",\"batch:51\"]",
     3600,
     "中新网健康 — feed listed in the publisher's own RSS directory; 22 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_life, "cn-b51-chinanews-com-cn-rss-life",
     "中新网生活", "中新网生活",
     "news", "media",
     "https://www.chinanews.com.cn/rss/life.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网生活\",\"media\",\"batch:51\"]",
     3600,
     "中新网生活 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_mz, "cn-b51-chinanews-com-cn-rss-mz",
     "中新网铸牢中华民族共同体意识", "中新网铸牢中华民族共同体意识",
     "news", "media",
     "https://www.chinanews.com.cn/rss/mz.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网铸牢中华民族共同体意识\",\"media\",\"batch:51\"]",
     3600,
     "中新网铸牢中华民族共同体意识 — feed listed in the publisher's own RSS directory; 7 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_photo, "cn-b51-chinanews-com-cn-rss-photo",
     "中新网图片频道", "中新网图片频道",
     "news", "media",
     "https://www.chinanews.com.cn/rss/photo.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网图片频道\",\"media\",\"batch:51\"]",
     3600,
     "中新网图片频道 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_scroll_news, "cn-b51-chinanews-com-cn-rss-scroll-news",
     "中新网即时新闻", "中新网即时新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/scroll-news.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网即时新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网即时新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_society, "cn-b51-chinanews-com-cn-rss-society",
     "中新网社会新闻", "中新网社会新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/society.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网社会新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网社会新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_sp, "cn-b51-chinanews-com-cn-rss-sp",
     "中新网-视频频道", "中新网-视频频道",
     "news", "media",
     "https://www.chinanews.com.cn/rss/sp.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网-视频频道\",\"media\",\"batch:51\"]",
     3600,
     "中新网-视频频道 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_sports, "cn-b51-chinanews-com-cn-rss-sports",
     "中新网体育新闻", "中新网体育新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/sports.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网体育新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网体育新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_theory, "cn-b51-chinanews-com-cn-rss-theory",
     "中新网理论频道", "中新网理论频道",
     "news", "media",
     "https://www.chinanews.com.cn/rss/theory.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网理论频道\",\"media\",\"batch:51\"]",
     3600,
     "中新网理论频道 — feed listed in the publisher's own RSS directory; 3 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_tx, "cn-b51-chinanews-com-cn-rss-tx",
     "中新网同心", "中新网同心",
     "news", "media",
     "https://www.chinanews.com.cn/rss/tx.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网同心\",\"media\",\"batch:51\"]",
     3600,
     "中新网同心 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_world, "cn-b51-chinanews-com-cn-rss-world",
     "中新网国际新闻", "中新网国际新闻",
     "news", "media",
     "https://www.chinanews.com.cn/rss/world.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网国际新闻\",\"media\",\"batch:51\"]",
     3600,
     "中新网国际新闻 — feed listed in the publisher's own RSS directory; 30 entries at probe 2026-10-21")

RSSX(cn_b51_chinanews_com_cn_rss_zhibo, "cn-b51-chinanews-com-cn-rss-zhibo",
     "中新网直播", "中新网直播",
     "news", "media",
     "https://www.chinanews.com.cn/rss/zhibo.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:中新网直播\",\"media\",\"batch:51\"]",
     3600,
     "中新网直播 — feed listed in the publisher's own RSS directory; 15 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_c_expressnews_cfinance, "cn-b51-rthk-hk-rthk-news-rss-c-expressnews-cfinance",
     "rthk.hk - 即時新聞: 財經", "rthk.hk - 即時新聞: 財經",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/c_expressnews_cfinance.xml",
     "zh", "[\"news\",\"country:hk\",\"publisher:rthk.hk - 即時新聞: 財經\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - 即時新聞: 財經 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_c_expressnews_cinternational, "cn-b51-rthk-hk-rthk-news-rss-c-expressnews-cinternational",
     "rthk.hk - 即時新聞: 國際", "rthk.hk - 即時新聞: 國際",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/c_expressnews_cinternational.xml",
     "zh", "[\"news\",\"country:hk\",\"publisher:rthk.hk - 即時新聞: 國際\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - 即時新聞: 國際 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_c_expressnews_clocal, "cn-b51-rthk-hk-rthk-news-rss-c-expressnews-clocal",
     "rthk.hk - 即時新聞: 本地", "rthk.hk - 即時新聞: 本地",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/c_expressnews_clocal.xml",
     "zh", "[\"news\",\"country:hk\",\"publisher:rthk.hk - 即時新聞: 本地\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - 即時新聞: 本地 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_c_expressnews_csport, "cn-b51-rthk-hk-rthk-news-rss-c-expressnews-csport",
     "rthk.hk - 即時新聞: 體育", "rthk.hk - 即時新聞: 體育",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/c_expressnews_csport.xml",
     "zh", "[\"news\",\"country:hk\",\"publisher:rthk.hk - 即時新聞: 體育\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - 即時新聞: 體育 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_c_expressnews_greaterchina, "cn-b51-rthk-hk-rthk-news-rss-c-expressnews-greaterchina",
     "rthk.hk - 即時新聞: 大中華", "rthk.hk - 即時新聞: 大中華",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/c_expressnews_greaterchina.xml",
     "zh", "[\"news\",\"country:hk\",\"publisher:rthk.hk - 即時新聞: 大中華\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - 即時新聞: 大中華 — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_e_expressnews_efinance, "cn-b51-rthk-hk-rthk-news-rss-e-expressnews-efinance",
     "rthk.hk - Express News: Finance", "rthk.hk - Express News: Finance",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/e_expressnews_efinance.xml",
     "en", "[\"news\",\"country:hk\",\"publisher:rthk.hk - Express News: Finance\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - Express News: Finance — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_e_expressnews_egreaterchina, "cn-b51-rthk-hk-rthk-news-rss-e-expressnews-egreaterchina",
     "rthk.hk - Express News: Greater China", "rthk.hk - Express News: Greater China",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/e_expressnews_egreaterchina.xml",
     "en", "[\"news\",\"country:hk\",\"publisher:rthk.hk - Express News: Greater China\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - Express News: Greater China — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_e_expressnews_einternational, "cn-b51-rthk-hk-rthk-news-rss-e-expressnews-einternational",
     "rthk.hk - Express News: World News", "rthk.hk - Express News: World News",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/e_expressnews_einternational.xml",
     "en", "[\"news\",\"country:hk\",\"publisher:rthk.hk - Express News: World News\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - Express News: World News — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_e_expressnews_elocal, "cn-b51-rthk-hk-rthk-news-rss-e-expressnews-elocal",
     "rthk.hk - Express News: Local", "rthk.hk - Express News: Local",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/e_expressnews_elocal.xml",
     "en", "[\"news\",\"country:hk\",\"publisher:rthk.hk - Express News: Local\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - Express News: Local — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

RSSX(cn_b51_rthk_hk_rthk_news_rss_e_expressnews_esport, "cn-b51-rthk-hk-rthk-news-rss-e-expressnews-esport",
     "rthk.hk - Express News: Sport", "rthk.hk - Express News: Sport",
     "news", "media",
     "https://rthk.hk/rthk/news/rss/e_expressnews_esport.xml",
     "en", "[\"news\",\"country:hk\",\"publisher:rthk.hk - Express News: Sport\",\"media\",\"batch:51\"]",
     3600,
     "rthk.hk - Express News: Sport — feed listed in the publisher's own RSS directory; 20 entries at probe 2026-10-21")

/* ── statistics ──────────────────────────────────────────────────────── */

RSSX(cn_b51_censtatd_gov_hk_data_en_press_release_rss, "cn-b51-censtatd-gov-hk-data-en-press-release-rss",
     "Census and Statistics Department", "Census and Statistics Department",
     "statistics", "statistics",
     "https://www.censtatd.gov.hk/data/en/press_release/rss.xml",
     "en", "[\"gov\",\"country:hk\",\"publisher:Census and Statistics Department\",\"statistics\",\"batch:51\"]",
     3600,
     "Census and Statistics Department — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_censtatd_gov_hk_data_tc_press_release_rss, "cn-b51-censtatd-gov-hk-data-tc-press-release-rss",
     "政府統計處", "政府統計處",
     "statistics", "statistics",
     "https://www.censtatd.gov.hk/data/tc/press_release/rss.xml",
     "zh", "[\"gov\",\"country:hk\",\"publisher:政府統計處\",\"statistics\",\"batch:51\"]",
     3600,
     "政府統計處 — feed listed in the publisher's own RSS directory; 10 entries at probe 2026-10-21")

RSSX(cn_b51_dsec_gov_mo_cmspages_dsecrss_aspx_rsstype_commonchart_lang_zh_mo, "cn-b51-dsec-gov-mo-cmspages-dsecrss-aspx-rsstype-commonchart-lang-zh-mo",
     "常用圖表", "常用圖表",
     "statistics", "statistics",
     "http://www.dsec.gov.mo/CMSPages/DSECRss.aspx?RssType=COMMONCHART&lang=zh-MO",
     "zh", "[\"gov\",\"country:mo\",\"publisher:常用圖表\",\"statistics\",\"batch:51\"]",
     3600,
     "常用圖表 — feed listed in the publisher's own RSS directory; 17 entries at probe 2026-10-21")

RSSX(cn_b51_dsec_gov_mo_cmspages_dsecrss_aspx_rsstype_commonindicator_lang_zh_mo, "cn-b51-dsec-gov-mo-cmspages-dsecrss-aspx-rsstype-commonindicator-lang-zh-mo",
     "主要指標", "主要指標",
     "statistics", "statistics",
     "http://www.dsec.gov.mo/CMSPages/DSECRss.aspx?RssType=COMMONINDICATOR&lang=zh-MO",
     "zh", "[\"gov\",\"country:mo\",\"publisher:主要指標\",\"statistics\",\"batch:51\"]",
     3600,
     "主要指標 — feed listed in the publisher's own RSS directory; 18 entries at probe 2026-10-21")

RSSX(cn_b51_dsec_gov_mo_cmspages_dsecrss_aspx_rsstype_dsecnews, "cn-b51-dsec-gov-mo-cmspages-dsecrss-aspx-rsstype-dsecnews",
     "最 新 統 計 資 料", "最 新 統 計 資 料",
     "statistics", "statistics",
     "http://www.dsec.gov.mo/CMSPages/DSECRss.aspx?RssType=DSECNEWS",
     "zh", "[\"gov\",\"country:mo\",\"publisher:最 新 統 計 資 料\",\"statistics\",\"batch:51\"]",
     3600,
     "最 新 統 計 資 料 — feed listed in the publisher's own RSS directory; 5 entries at probe 2026-10-21")
