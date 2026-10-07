OUT = "/Users/rayan/OSINTsaas/docs/rejected-sources-batch39.cn39.tsv"
R = [
("JO39_CN_DCE_DAY_QUOTES", "http://www.dce.com.cn/dcereport/publicweb/dailystat/dayQuotes",
 "POST JSON {varietyId:all, tradeDate:20260930, tradeType:1, lang:zh} answers HTTP 412 with a Ruishu JS anti-bot challenge page, as does every http path on www.dce.com.cn including the data channel pages; https://www.dce.com.cn does not connect (curl 000 on 3 tries, 2026-10-06). A date token cannot get past the challenge; no row."),
("JO39_CN_NMC_TYPHOON_LIST", "http://typhoon.nmc.cn/weatherservice/typhoon/jsons/list_default",
 "Re-checked after the engine and probe learned double-paren JSONP (2026-10-06): list_default, typhoon_jsons_list_default(({typhoonList:[...]})), now parses, but it holds the 32 storms of 2026 as positional arrays (id, English and Chinese name, number, status), and every one of them is already in JO40_CNGOV_ZJ_TYPHOON_CATALOGUE: fetched side by side on 2026-10-06, the Zhejiang 2026 list holds 32 storms, all 32 NMC numbers match a Zhejiang tfid and all 29 named NMC storms match by English name (the other 3 are unnamed depressions present in both), and Zhejiang adds start and end times and warning level. The NMC host is not in existing_urls.txt, but the data is a duplicate of a shipped row, so it stays out. Per-storm tracks (view_{id}) are one request per storm with nested forecast arrays."),
("JO39_CN_SAMR_FOOD_SAMPLING", "https://spcjsac.gsxt.gov.cn/pjgcx/cjPubInfo/getJgcxList",
 "The SPA's result list is a JSON POST, but every call carries an imageToken and slider percentage from a slide-puzzle captcha (cjPubInfo/getSlideImage); without them the API answers state 500, message 请求失败, cause 'empty String'. Captcha-gated; no row."),
("JO39_CN_GGZY_DEALS", "https://www.ggzy.gov.cn/information/pubTradingInfo/getTradList",
 "deal.ggzy.gov.cn (batch 40's endpoint) no longer resolves in DNS; the list moved to www.ggzy.gov.cn as a form POST that answers (20 notices a page) but reports total 1,000 in 50 pages for any query, answers page 100 with code 804 请细化查询条件后再次检索 (refine the query), and Guangdong alone fills the 1,000 cap in a single day (Beijing 474), so even province-by-day rows (32+ on one host against the 8-row cap) would truncate. 4 of 10 requests on 2026-10-06 dropped the connection (curl 000), and the client also handles an image-captcha reply (code 829) and a too-frequent reply (code 800)."),
("JO39_CN_CNINFO_LATEST", "https://www.cninfo.com.cn/new/disclosure",
 "Answers the form POST (column=szse_latest), but it is the same announcementId population as the archive rows written (JO39_CN_CNINFO_ANN_SZ/SH/BJ), grouped per company in classifiedAnnouncements, capped at 500 securities (totalRecordNum 500), Shenzhen only and latest day only (1,245 announcements dated 2026-10-01). Duplicate data."),
("JO39_CN_AMAC_PRIVATE_FUNDS", "https://gs.amac.org.cn/amac-infodisc/api/pof/fund?rand=0.1&page=0&size=1000",
 "Sibling of the AMAC manager register, tried with the same POST {} body: HTTP 500 with a 13-byte body on pages 0 and 1. Not on the candidate list and not pursued further."),
]
with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    fh.write("id\turl\treason\n")
    for i, u, r in R:
        assert "\t" not in r and "\n" not in r
        fh.write("%s\t%s\t%s\n" % (i, u, r))
print(len(R), "rejects")
