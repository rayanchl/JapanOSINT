#!/usr/bin/env python3
"""Writes docs/candidate-sources-batch40.cnmkt.txt from the rows below.
Every number in a description was measured on 2026-10-01/02 with curl/urllib
under the engine's User-Agent (see the scratch helpers next to this file)."""
import sys

OUT = "/Users/rayan/OSINTsaas/docs/candidate-sources-batch40.cnmkt.txt"
HEAD = [
    "# Batch 40 beat cnmkt: Chinese financial markets and corporate disclosure -- exchange regulatory registers (SSE, SZSE),",
    "# CSRC enforcement and licensing channels, IPO review pipelines, CFETS/chinamoney benchmark rates and bond register, CNINFO",
    "# security registers, CFFEX option books and the Eastmoney data centre (insider dealings, block trades, pledges, research).",
    "# Every row fetched 2026-10-01/02 from macOS with the engine User-Agent: record count read off the upstream's own total, page 2 compared with page 1, id_keys distinctness counted.",
    "# id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts",
]

EM = "https://datacenter-web.eastmoney.com/api/data/v1/get?reportName=%s&columns=ALL&sortColumns=%s&sortTypes=%s&pageSize=500&pageNumber=1"
SSEQ = "https://query.sse.com.cn/"
SZX = "https://www.szse.cn/api/report/ShowReport?SHOWTYPE=xlsx&CATALOGID=%s"
CSRC = "https://www.csrc.gov.cn/searchList/%s?_isAgg=true&_isJson=true&_pageSize=20&_template=index&_rangeTimeGte=&_channelName=&page=1"
CMB = "https://www.chinamoney.com.cn/ags/ms/cm-u-bond-md/BondMarketInfoList2?pageNo=1&pageSize=15&bondName=&bondCode=&issueEnty=&bondType=%s&couponType=&issueYear=&rtngShrt=&bondSpclPrjctVrty="
SSE_REF = "header1=Referer: https://www.sse.com.cn/"

rows = []
def row(id_, mode, cat, rtype, tags, portal, name, name_zh, url, desc, opts, probe=None):
    rows.append([id_, mode, "any", cat, rtype, "china," + tags, portal, name, name_zh,
                 url, probe or url, desc, opts])

# ── Eastmoney data centre (datacenter-web.eastmoney.com): 8 rows, the host cap ──
row("JO40_CNMKT_EM_EXEC_DEALINGS", "json", "finance", "insider-share-dealing",
    "eastmoney,insider-trading,directors,executives,a-shares",
    "https://data.eastmoney.com/executive/",
    "Eastmoney data centre: share dealings by directors, supervisors and senior executives of A-share companies",
    "东方财富 数据中心 董监高及相关人员持股变动明细",
    EM % ("RPT_EXECUTIVE_HOLD_DETAILS", "CHANGE_DATE,GGEID", "-1,-1"),
    "Every reported share dealing by a director, supervisor or senior executive of an SSE/SZSE/BSE-listed company or a person related to them: security code and name, dealing date, the person who traded, the executive they are linked to, position (e.g. 副总经理), relationship (本人, 配偶, 受控法人), shares changed, average price, value, reason (竞价交易, 二级市场买卖, 大宗交易, 盘后定价) and holdings after. Fetched 2026-10-02: result.count 172,291 (345 pages at the server's 500-row maximum; pageSize=1000 and 5000 still return 500), newest CHANGE_DATE 2026-09-30, page 2 shares no record with page 1. GGEID is unique but exceeds 2^53 and collides as a C double, so identity is the 7-field composite, which was distinct over 5,000 consecutive records (the 4-field code+person+date+shares key was not: 1,485 of 1,500).",
    "array_path=result.data;id_keys=SECURITY_CODE+PERSON_NAME+DSE_PERSON_NAME+CHANGE_DATE+CHANGE_SHARES+AVERAGE_PRICE+END_HOLD_NUM;title_keys=PERSON_NAME;date_keys=CHANGE_DATE;page_param=pageNumber;page_max=400;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_BLOCK_TRADES", "json", "finance", "block-trade",
    "eastmoney,block-trade,brokerage,a-shares",
    "https://data.eastmoney.com/dzjy/default.html",
    "Eastmoney data centre: A-share and fund block trades with buying and selling brokerage branches",
    "东方财富 数据中心 大宗交易每日明细",
    EM % ("RPT_DATA_BLOCKTRADE", "TRADE_DATE,SECURITY_CODE,DAILY_RANK", "-1,1,1"),
    "Every block trade on the Shanghai and Shenzhen exchanges: trade date, security, deal price and premium/discount to close, volume and value, and the named buying and selling brokerage branches (e.g. 华泰证券股份有限公司南宁民族大道证券营业部, or 机构专用) with their codes, plus post-trade price moves. Fetched 2026-10-02: result.count 682,950 (1,366 pages of 500), newest TRADE_DATE 2026-09-30. Identical trades on one day differ only by DAILY_RANK, so code+date+rank is the identity: 5,000 of 5,000 distinct over 10 pages (code+date+price+volume+buyer+seller gave only 4,416).",
    "array_path=result.data;id_keys=SECURITY_CODE+TRADE_DATE+DAILY_RANK;title_keys=SECURITY_NAME_ABBR;date_keys=TRADE_DATE;page_param=pageNumber;page_max=1580;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_HOLDER_CHANGES", "json", "finance", "major-shareholder-change",
    "eastmoney,shareholders,stake-change,a-shares",
    "https://data.eastmoney.com/executive/gdzjc.html",
    "Eastmoney data centre: increases and decreases in holdings by major shareholders of A-share companies",
    "东方财富 数据中心 股东增减持明细",
    EM % ("RPT_SHARE_HOLDER_INCREASE", "NOTICE_DATE,SECURITY_CODE,HOLDER_NAME,END_DATE,START_DATE", "-1,1,1,-1,-1"),
    "Disclosed stake changes by major (5%+) and controlling shareholders: holder name (companies, partnerships, individuals), direction (增持/减持), shares changed, percentage of free float and of total, holding after the change, average price, channel (二级市场, 大宗交易) and the change window and notice date. Fetched 2026-10-02: result.count 146,773 (294 pages of 500), newest NOTICE_DATE 2026-10-01, page 2 disjoint from page 1; code+holder+start+end+signed change+notice date distinct over 5,000 of 5,000.",
    "array_path=result.data;id_keys=SECURITY_CODE+HOLDER_NAME+START_DATE+END_DATE+CHANGE_NUM_SYMBOL+NOTICE_DATE;title_keys=HOLDER_NAME;date_keys=NOTICE_DATE;page_param=pageNumber;page_max=340;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_DRAGON_TIGER", "json", "finance", "abnormal-trading-disclosure",
    "eastmoney,dragon-tiger-list,abnormal-trading,a-shares",
    "https://data.eastmoney.com/stock/lhb.html",
    "Eastmoney data centre: Dragon-Tiger list (exchange abnormal-trading disclosures)",
    "东方财富 数据中心 龙虎榜详情",
    EM % ("RPT_DAILYBILLBOARD_DETAILSNEW", "TRADE_DATE,SECURITY_CODE,TRADE_ID", "-1,1,1"),
    "Each stock the exchanges named on a trading day for abnormal price or turnover (the 龙虎榜): trigger reason (e.g. daily close deviation over 7%), close and change, turnover, buy/sell/net amount through the disclosed top seats, seat counts, free-float cap and follow-on returns after 1-30 days. Fetched 2026-10-02: result.count 268,990 (538 pages of 500), newest TRADE_DATE 2026-09-30; TRADE_ID distinct over 3,000 of 3,000 and page 2 disjoint from page 1.",
    "array_path=result.data;id_keys=TRADE_ID;title_keys=SECURITY_NAME_ABBR;date_keys=TRADE_DATE;page_param=pageNumber;page_max=620;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_ORG_SURVEYS", "json", "finance", "institutional-visit",
    "eastmoney,investor-relations,institutional-research,a-shares",
    "https://data.eastmoney.com/jgdy/",
    "Eastmoney data centre: institutional research visits to listed companies (who visited whom)",
    "东方财富 数据中心 机构调研明细",
    EM % ("RPT_ORG_SURVEYNEW", "NOTICE_DATE,SECURITY_CODE,RECEIVE_START_DATE,RECEIVE_OBJECT", "-1,1,1,1"),
    "One record per visiting institution per company research reception: company, visiting firm (e.g. 国海证券) and its type (证券公司, 基金管理公司), named analysts (INVESTIGATORS), reception dates, place and format (特定对象调研, 业绩说明会), and the company officers who received them. Fetched 2026-10-02: result.count 287,710 (576 pages of 500), newest NOTICE_DATE 2026-10-01; code+notice+start date+visitor+sequence distinct over 3,000 of 3,000, page 2 disjoint.",
    "array_path=result.data;id_keys=SECURITY_CODE+NOTICE_DATE+RECEIVE_START_DATE+RECEIVE_OBJECT+NUMBERNEW;title_keys=SECURITY_NAME_ABBR;date_keys=NOTICE_DATE;page_param=pageNumber;page_max=660;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_SHARE_PLEDGES", "json", "finance", "share-pledge",
    "eastmoney,share-pledge,controlling-shareholder,credit,a-shares",
    "https://data.eastmoney.com/gpzy/",
    "Eastmoney data centre: share pledges by major shareholders, with pledgee and margin-call lines",
    "东方财富 数据中心 重要股东股权质押明细",
    EM % ("RPTA_APP_ACCUMDETAILS", "NOTICE_DATE,MXID", "-1,1"),
    "Each disclosed share pledge by a significant shareholder: pledgor (flagged when it is the controlling shareholder), pledgee type, shares pledged and share of holding and of total capital, cumulative pledge ratio, start and release dates and state, stated purpose, and the estimated warning and liquidation price lines against the latest close. Fetched 2026-10-02: result.count 126,872 (254 pages of 500), newest NOTICE_DATE 2026-10-01; MXID (a 32-hex row hash) distinct over 3,000 of 3,000 and page 2 disjoint.",
    "array_path=result.data;id_keys=MXID;title_keys=HOLDER_NAME;date_keys=NOTICE_DATE;page_param=pageNumber;page_max=300;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_IPO_REGISTER", "json", "finance", "ipo",
    "eastmoney,ipo,new-listings,a-shares",
    "https://data.eastmoney.com/xg/xg/default.html",
    "Eastmoney data centre: every A-share IPO since 1984 with subscription, pricing and first-day data",
    "东方财富 数据中心 新股申购与中签",
    EM % ("RPTA_APP_IPOAPPLY", "APPLY_DATE,SECURITY_CODE", "-1,1"),
    "The full A-share IPO register: security code and name, exchange and board, subscription code and date, issue price and P/E, shares issued online and offline, lottery ratio and multiple, funds raised, listing date, first-day open and close and cumulative first-run gain, including scheduled offerings (newest APPLY_DATE 2026-10-19). Fetched 2026-10-02: result.count 5,639 in 12 pages of 500, all 12 read; SECURITY_CODE distinct over all 5,639.",
    "array_path=result.data;id_keys=SECURITY_CODE;title_keys=SECURITY_NAME;date_keys=APPLY_DATE;page_param=pageNumber;page_max=16;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_PROFIT_WARNINGS", "json", "finance", "earnings-preannouncement",
    "eastmoney,earnings,profit-warning,corporate-disclosure,a-shares",
    "https://data.eastmoney.com/bbsj/yjyg.html",
    "Eastmoney data centre: earnings pre-announcements and profit warnings of listed companies since 2003",
    "东方财富 数据中心 业绩预告",
    EM % ("RPT_PUBLIC_OP_NEWPREDICT", "NOTICE_DATE,SECURITY_CODE,REPORT_DATE,PREDICT_FINANCE_CODE", "-1,1,-1,1"),
    "Every earnings pre-announcement: company, period, metric forecast (net profit attributable, deducted net profit, revenue), forecast range and change versus the prior year, forecast type (预增, 首亏, 续亏, 扭亏), the company's own forecast text and reasons. Fetched 2026-10-02: result.count 190,977 (382 pages of 500), newest NOTICE_DATE 2026-09-30, oldest 2003; code+period+metric+notice date distinct over 5,000 of 5,000 and page 2 disjoint.",
    "array_path=result.data;id_keys=SECURITY_CODE+REPORT_DATE+PREDICT_FINANCE_CODE+NOTICE_DATE;title_keys=SECURITY_NAME_ABBR+PREDICT_TYPE;date_keys=NOTICE_DATE;page_param=pageNumber;page_max=440;interval=86400;timeout_ms=60000")

# ── other Eastmoney services (different hosts, different data) ──
row("JO40_CNMKT_EM_STOCK_RESEARCH", "json", "finance", "broker-research-report",
    "eastmoney,sell-side-research,analysts,ratings",
    "https://data.eastmoney.com/report/stock.jshtml",
    "Eastmoney research centre: sell-side research reports on individual Chinese stocks",
    "东方财富 研报中心 个股研报",
    "https://reportapi.eastmoney.com/report/list?pageSize=100&pageNo=1&qType=0&beginTime=&endTime=",
    "Sell-side company research published by Chinese brokerages: title, covered stock, broker (org name and code), named analysts with ids, rating and previous rating, EPS and P/E forecasts for this year and the next two, page count and attachment size, publish date. Fetched 2026-10-02: hits 148,262 over TotalPage 1,483 at the 100-row maximum (pageSize=500 is answered with 100), newest 2026-09-30, last page dated 2017-01; infoCode distinct over 200 and page 2 disjoint; pageNo past the end returns an empty data array.",
    "array_path=data;id_keys=infoCode;title_keys=title;date_keys=publishDate;page_param=pageNo;page_max=1700;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_INDUSTRY_RESEARCH", "json", "finance", "broker-research-report",
    "eastmoney,sell-side-research,industry,analysts",
    "https://data.eastmoney.com/report/industry.jshtml",
    "Eastmoney research centre: sell-side industry research reports",
    "东方财富 研报中心 行业研报",
    "https://reportapi.eastmoney.com/report/list?pageSize=100&pageNo=1&qType=1&beginTime=&endTime=",
    "Industry research from Chinese brokerages: title, industry (e.g. 房地产开发) and Eastmoney industry code, broker, analysts, rating, page count and publish date. Fetched 2026-10-02: hits 141,219 over TotalPage 1,413 at 100 per page, newest 2026-09-30; infoCode distinct over 200 and page 2 disjoint.",
    "array_path=data;id_keys=infoCode;title_keys=title;date_keys=publishDate;page_param=pageNo;page_max=1620;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_EM_ANNOUNCEMENTS", "json", "finance", "corporate-announcement",
    "eastmoney,announcements,corporate-disclosure,a-shares",
    "https://data.eastmoney.com/notices/",
    "Eastmoney announcement stream: every A-share company announcement, newest first",
    "东方财富 公告大全 沪深京A股公告",
    "https://np-anotice-stock.eastmoney.com/api/security/ann?sr=-1&page_size=100&page_index=1&ann_type=A&client_source=web&f_node=0&s_node=0",
    "The all-market announcement stream for Shanghai, Shenzhen and Beijing A-shares: announcement code, title, company code and name, column (e.g. 调研活动, 董事会决议公告, 关联交易, 高管人员任职变动), notice and display time. Fetched 2026-10-02: 100 per page (page_size=200 and 500 still return 100), total_hits reported as 50,000, which is the window the upstream exposes: page_index 500 reaches announcements of 2026-08-27 and pages 501 and 600 return the same 100 records as page 500, so the walk is bounded at 500 pages and the ceiling stamp is true. art_code distinct over the 200 records of pages 1-2.",
    "array_path=data.list;id_keys=art_code;title_keys=title;date_keys=notice_date;page_param=page_index;page_max=500;interval=86400;timeout_ms=60000")

# ── Shanghai Stock Exchange ──
row("JO40_CNMKT_SSE_REG_MEASURES", "json", "regulatory", "regulatory-measure",
    "sse,shanghai,regulatory-measures,enforcement,listed-companies",
    "https://www.sse.com.cn/disclosure/credibility/supervision/measures/",
    "Shanghai Stock Exchange: regulatory measures against listed companies, directors and intermediaries",
    "上海证券交易所 监管措施",
    SSEQ + "commonSoaQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&sqlId=BS_KCB_GGLL_NEW&siteId=28&channelId=10007,10008,10009,10010&order=createTime%7Cdesc,stockcode%7Casc",
    "SSE's register of supervisory measures (监管工作函, 监管警示, 监管关注 and similar) on main-board and STAR companies: document title naming the company, stock code and short name, measure type, the parties concerned (上市公司, 董事, 高级管理人员, 中介机构), date and the path of the decision document. Fetched 2026-10-02 with a Referer header: pageHelp.total 5,552 over 6 pages of 1,000, newest 2026-09-30, oldest 2013-06; page 7 returns an empty data array; docId distinct over 2,000 and page 2 disjoint.",
    "array_path=pageHelp.data;id_keys=docId;title_keys=docTitle;date_keys=createTime;page_param=pageHelp.pageNo;page_max=9;interval=86400;timeout_ms=60000;" + SSE_REF)

row("JO40_CNMKT_SSE_INQUIRY_LETTERS", "json", "regulatory", "inquiry-letter",
    "sse,shanghai,inquiry-letters,disclosure-review,listed-companies",
    "https://www.sse.com.cn/disclosure/credibility/supervision/inquiries/",
    "Shanghai Stock Exchange: regulatory inquiry letters to listed companies",
    "上海证券交易所 监管问询",
    SSEQ + "commonSoaQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&sqlId=BS_KCB_GGLL_NEW&siteId=28&channelId=10743,10744,10012&order=createTime%7Cdesc,stockcode%7Casc",
    "Inquiry letters SSE sent to listed companies (问询函, periodic-report inquiries, restructuring review letters): letter title naming the company and the matter (e.g. an asset purchase), stock code, letter type, date and document path. Fetched 2026-10-02: pageHelp.total 2,932 over 3 pages of 1,000, newest 2026-09-29; docId distinct over 2,000 and page 2 disjoint.",
    "array_path=pageHelp.data;id_keys=docId;title_keys=docTitle;date_keys=createTime;page_param=pageHelp.pageNo;page_max=6;interval=86400;timeout_ms=60000;" + SSE_REF)

row("JO40_CNMKT_SSE_BOND_DISCIPLINE", "json", "regulatory", "regulatory-measure",
    "sse,shanghai,bonds,disciplinary-action,enforcement",
    "https://www.sse.com.cn/",
    "Shanghai Stock Exchange: regulatory measures and disciplinary sanctions in the bond market",
    "上海证券交易所 债券监管措施与纪律处分",
    SSEQ + "commonSoaQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&sqlId=BS_GGLL_NEW&siteId=28&channelId=9894,9895,9896,12428,12093,12094,12095,12429,9890,9891,9892,12426,12090,12091,12092,12427&order=createTime%7Cdesc,saveTime%7Cdesc",
    "SSE bond-market enforcement: written warnings, supervisory measures and disciplinary sanctions against bond issuers, underwriters, trustees, bankruptcy administrators and individuals, each with the decision title naming the party, the parties concerned (extTeacher), measure type (债券监管措施, 纪律处分) and date. Fetched 2026-10-02: pageHelp.total 1,005 over 2 pages of 1,000, newest 2026-09-21; docId distinct.",
    "array_path=pageHelp.data;id_keys=docId;title_keys=docTitle;date_keys=createTime;page_param=pageHelp.pageNo;page_max=5;interval=86400;timeout_ms=60000;" + SSE_REF)

row("JO40_CNMKT_SSE_COMMITMENTS", "json", "regulatory", "shareholder-commitment",
    "sse,shanghai,commitments,controlling-shareholder,corporate-governance",
    "https://www.sse.com.cn/disclosure/credibility/supervision/promisho/",
    "Shanghai Stock Exchange: commitments made by controlling shareholders, executives and acquirers, and whether they were kept",
    "上海证券交易所 承诺履行情况",
    SSEQ + "commonQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.cacheSize=1&pageHelp.beginPage=1&sqlId=SSE_XXPL_SSGSXX_SSGSCXJL_CNLYQK_L&promiseName=&promiseType=&itemType=&promiseStatus=&boardType=&companyCode=",
    "Undertakings registered with SSE by listed companies' controlling shareholders, actual controllers, directors and counterparties (non-competition, share lock-ups, profit guarantees): company, committing party (e.g. Yijiahe(BVI)Limited), commitment type and summary, date, expected and actual completion and status (正常履行中, 已完成, 已变更, 已豁免, 超期未履行承诺, 其他违反承诺). Fetched 2026-10-02: pageHelp.total 37,252 over 38 pages of 1,000. This endpoint pages on pageHelp.beginPage, not pageNo: pageNo=2 re-serves page 1, beginPage=2 with cacheSize=1 gives the next 1,000 (measured, page 2 disjoint). SEQ distinct over 2,000. PROMISE_CONTENT is served as a Java Clob placeholder string by the upstream.",
    "array_path=pageHelp.data;id_keys=SEQ;title_keys=COMMENTS;date_keys=PROMISE_DATE;page_param=pageHelp.beginPage;page_max=45;interval=86400;timeout_ms=60000;" + SSE_REF)

row("JO40_CNMKT_SSE_INSIDER_CHANGES", "json", "finance", "insider-share-dealing",
    "sse,shanghai,insider-trading,directors,executives",
    "https://www.sse.com.cn/disclosure/credibility/supervision/change/",
    "Shanghai Stock Exchange: shareholding changes of directors, supervisors and senior managers (exchange filing register)",
    "上海证券交易所 董监高持股变动",
    SSEQ + "commonQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.cacheSize=1&pageHelp.beginPage=1&sqlId=COMMON_SSE_XXPL_CXJL_SSGSGFBDQK_S&COMPANY_CODE=&NAME=&BEGIN_DATE=&END_DATE=&BOARDTYPE=",
    "SSE's own register of dealings by directors, supervisors and senior managers of Shanghai-listed companies as filed with the exchange: company, name, duty (董事, 高级管理人员), change date and filing date, shares changed, average price, reason (二级市场买卖, 股权激励实施, 分红送转) and holdings before and after. Fetched 2026-10-02: pageHelp.total 71,335 over 72 pages of 1,000, newest change 2026-09-28; paged on pageHelp.beginPage (pageNo is ignored by this sqlId). The 8-field composite is distinct over 1,999 of 2,000; the one collision is two rows identical in every field except the row counter NUM.",
    "array_path=pageHelp.data;id_keys=COMPANY_CODE+NAME+CHANGE_DATE+CHANGE_NUM+CURRENT_NUM+CURRENT_AVG_PRICE+FORM_DATE+DUTY;title_keys=NAME;date_keys=CHANGE_DATE;page_param=pageHelp.beginPage;page_max=85;interval=86400;timeout_ms=60000;" + SSE_REF)

row("JO40_CNMKT_SSE_LISTED_COMPANIES", "json", "registry", "listed-company",
    "sse,shanghai,listed-companies,register,star-market",
    "https://www.sse.com.cn/assortment/stock/list/share/",
    "Shanghai Stock Exchange: listed company register (main board A and B shares, STAR market)",
    "上海证券交易所 股票列表",
    SSEQ + "sseQuery/commonQuery.do?STOCK_TYPE=&REG_PROVINCE=&CSRC_CODE=&STOCK_CODE=&sqlId=COMMON_SSE_CP_GPJCTPZ_GPLB_GP_L&COMPANY_STATUS=2,4,5,7,8&type=inParams&isPagination=true&pageHelp.cacheSize=1&pageHelp.beginPage=1&pageHelp.pageSize=1000",
    "Every security listed on SSE across main-board A shares, B shares and the STAR market: company code, A and B share codes, short and full Chinese names, English name, registered province, CSRC industry, listing date, delisting date and status codes. Fetched 2026-10-02: pageHelp.total 2,361 over 3 pages of 1,000 (STOCK_TYPE=1 alone is 1,702 and STOCK_TYPE=8 is 618), page 4 empty. A company with both A and B shares appears once per share type, so company code alone repeats (1,962 distinct of 2,000); COMPANY_CODE+STOCK_TYPE is distinct over all 2,361.",
    "array_path=pageHelp.data;id_keys=COMPANY_CODE+STOCK_TYPE;title_keys=FULL_NAME,SEC_NAME_CN;page_param=pageHelp.beginPage;page_max=5;interval=86400;timeout_ms=60000;" + SSE_REF)

row("JO40_CNMKT_SSE_STAR_IPO_REVIEW", "json", "finance", "ipo-review-project",
    "sse,star-market,ipo,listing-review,intermediaries",
    "https://www.sse.com.cn/",
    "Shanghai Stock Exchange STAR Market: IPO review projects with sponsors, law firms, auditors and named signatories",
    "上海证券交易所 科创板股票发行上市审核项目动态",
    SSEQ + "statusAction.do?isPagination=true&sqlId=SH_XM_LB&pageHelp.pageSize=500&pageHelp.pageNo=1&offerType=&commitiResult=&registeResult=&province=&csrcCode=&currStatus=&order=updateDate%7Cdesc&keyword=&auditApplyDateBegin=&auditApplyDateEnd=",
    "Every STAR Market IPO application SSE has reviewed: applicant name, review number, status and suspension flag, planned raise, registration result, last update, and the intermediaries with their named people (sponsor and its 保荐代表人, law firm and lawyers, accounting firm and signing accountants). Fetched 2026-10-02 with a Referer header: pageHelp.total 1,048 over 3 pages of 500, newest update 2026-09-30; stockAuditNum distinct over 1,000 and page 2 disjoint; at 100 per page page 12 was empty.",
    "array_path=pageHelp.data;id_keys=stockAuditNum;title_keys=stockAuditName;date_keys=updateDate;page_param=pageHelp.pageNo;page_max=6;interval=86400;timeout_ms=60000;header1=Referer: https://kcb.sse.com.cn/")

row("JO40_CNMKT_SSE_LATEST_ANNOUNCEMENTS", "json", "finance", "corporate-announcement",
    "sse,shanghai,announcements,corporate-disclosure",
    "https://www.sse.com.cn/disclosure/listedinfo/announcement/",
    "Shanghai Stock Exchange: latest listed-company announcements (static publishing file)",
    "上海证券交易所 最新公告",
    "https://www.sse.com.cn/disclosure/listedinfo/announcement/json/stock_bulletin_publish_order.json",
    "The file SSE's announcement page reads for its newest listed-company announcements: disclosure id, date, title (e.g. a board-resolution notice), bulletin class, PDF path on sse.com.cn, security code and short name. Fetched 2026-10-02: publishData held 622 announcements, all dated 2026-10-01, discloseId distinct over all 622. The file is replaced as new announcements publish, so the row is scheduled hourly to keep each day's set.",
    "array_path=publishData;id_keys=discloseId;title_keys=bulletinTitle;date_keys=discloseDate;pagination_ok=a static JSON file published whole (622 records in one response), no paging parameters exist;interval=3600;timeout_ms=60000")

# ── Shenzhen Stock Exchange (xlsx exports of the full report tables) ──
SZ_PORTAL = "https://www.szse.cn/disclosure/supervision/"
row("JO40_CNMKT_SZSE_REG_MEASURES", "xlsx", "regulatory", "regulatory-measure",
    "szse,shenzhen,regulatory-measures,enforcement,listed-companies",
    "https://www.szse.cn/disclosure/supervision/measure/measure/index.html",
    "Shenzhen Stock Exchange: regulatory measures against listed companies and their officers",
    "深圳证券交易所 监管措施",
    SZX % "1800_jgxxgk&TABKEY=tab1",
    "SZSE's register of supervisory measures: company code and short name, date, measure (监管函 4,848 of them, 约见谈话 6), letter content (the PDF name for recent letters, a one-line summary of the breach for older ones) and parties concerned (上市公司, 董监高). Fetched 2026-10-02 as the report's own xlsx export: 4,855 data rows, equal to the JSON view's recordcount 4,855, newest 2026-09-04, oldest 2014-04; code+date+content distinct over all 4,855.",
    "xlsx_sheet_index=0;id_keys=公司代码+采取监管措施日期+函件内容;title_keys=公司简称;date_keys=采取监管措施日期;pagination_ok=the xlsx export returns the whole table in one file (4,855 of recordcount 4,855);interval=86400;timeout_ms=60000")

row("JO40_CNMKT_SZSE_DISCIPLINE", "xlsx", "regulatory", "disciplinary-sanction",
    "szse,shenzhen,disciplinary-action,enforcement,listed-companies",
    "https://www.szse.cn/disclosure/supervision/measure/pushish/index.html",
    "Shenzhen Stock Exchange: disciplinary sanctions (public censure, public criticism, unfit-person findings)",
    "深圳证券交易所 处罚与处分记录",
    SZX % "1759_cxda&TABKEY=tab1",
    "SZSE disciplinary sanctions, one row per sanctioned party and sanction: company code and name, date, sanction type (公开谴责 190, 通报批评 183, 公开认定 81, 暂不接受发行上市申请文件 2), the named individual or entity, decision title and decision file. Fetched 2026-10-02 as the xlsx export: 456 rows (JSON recordcount 456), newest 2026-09-18; the 6-field composite is distinct over 455 of 456, the remaining pair being identical in every column.",
    "xlsx_sheet_index=0;id_keys=公司代码+处分日期+处分类别+当事人+标题+查看全文;title_keys=标题;date_keys=处分日期;pagination_ok=the xlsx export returns the whole table in one file (456 of recordcount 456);interval=86400;timeout_ms=60000")

row("JO40_CNMKT_SZSE_INQUIRY_MAIN", "xlsx", "regulatory", "inquiry-letter",
    "szse,shenzhen,inquiry-letters,disclosure-review,main-board",
    "https://www.szse.cn/disclosure/supervision/inquire/index.html",
    "Shenzhen Stock Exchange: inquiry and concern letters to main-board companies, with company replies",
    "深圳证券交易所 问询函件 主板",
    SZX % "main_wxhj&TABKEY=tab2",
    "Every inquiry letter SZSE sent to a main-board company: company code and name, date sent, letter type (关注函 3,873, 年报问询函 2,128, 许可类重组问询函 632, 非许可类重组问询函, 半年报问询函), letter file and the company's reply file. A letter answered more than once carries the extra replies on continuation rows whose other cells are blank (99 such rows); they are real replies and the composite keeps them apart. Fetched 2026-10-02 as the xlsx export: 7,389 rows (JSON recordcount 7,389), newest 2026-08-28; code+date+type+letter+reply distinct over all 7,389.",
    "xlsx_sheet_index=0;id_keys=公司代码+发函日期+函件类别+函件内容+公司回复;title_keys=公司简称,公司回复;date_keys=发函日期;pagination_ok=the xlsx export returns the whole table in one file (7,389 of recordcount 7,389);interval=86400;timeout_ms=60000")

row("JO40_CNMKT_SZSE_INQUIRY_CHINEXT", "xlsx", "regulatory", "inquiry-letter",
    "szse,shenzhen,inquiry-letters,disclosure-review,chinext",
    "https://www.szse.cn/disclosure/supervision/inquire/index.html",
    "Shenzhen Stock Exchange: inquiry and concern letters to ChiNext companies, with company replies",
    "深圳证券交易所 问询函件 创业板",
    SZX % "main_wxhj&TABKEY=tab3",
    "Inquiry letters SZSE sent to ChiNext (创业板) companies: company code and name, date, letter type (关注函, 年报问询函, 半年报问询函, 重组问询函), letter file and reply file, with 252 continuation rows carrying additional replies. Fetched 2026-10-02 as the xlsx export: 5,282 rows (JSON recordcount 5,282), newest letter 2026-06-04; code+date+type+letter+reply distinct over all 5,282.",
    "xlsx_sheet_index=0;id_keys=公司代码+发函日期+函件类别+函件内容+公司回复;title_keys=公司简称,公司回复;date_keys=发函日期;pagination_ok=the xlsx export returns the whole table in one file (5,282 of recordcount 5,282);interval=86400;timeout_ms=60000")

row("JO40_CNMKT_SZSE_SUSPENSIONS", "xlsx", "finance", "trading-suspension",
    "szse,shenzhen,trading-suspension,market-events",
    "https://www.szse.cn/disclosure/index.html",
    "Shenzhen Stock Exchange: trading suspensions and resumptions since 2016",
    "深圳证券交易所 停复牌提示（2016年起）",
    SZX % "1798&TABKEY=tab1&txtKsrq=2016-01-01",
    "Every SZSE trading halt and resumption from 2016-01-01: security code and name, suspension and resumption time, duration and reason (重大事项 12,576, 盘中临时停牌 2,039, 临时停牌, 收益分配, 撤销退市风险警示, 退市整理期). Fetched 2026-10-02: 19,143 rows, matching the JSON recordcount for the same filter, newest 2026-09-30; code+halt+resume+reason distinct over all 19,143. Scoped by the URL's own start date because the unfiltered table (70,760 rows) exceeds the 65,535-row xlsx limit and the export silently stops there; the 51,613 halts before 2016 are therefore not in this row.",
    "xlsx_sheet_index=0;id_keys=证券代码+停牌时间+复牌时间+停牌原因;title_keys=证券简称;date_keys=停牌时间;pagination_ok=the xlsx export returns the whole filtered table in one file (19,143 of recordcount 19,143);interval=86400;timeout_ms=60000")

row("JO40_CNMKT_SZSE_INSIDER_CHANGES", "xlsx", "finance", "insider-share-dealing",
    "szse,shenzhen,insider-trading,directors,executives",
    "https://www.szse.cn/disclosure/supervision/change/index.html",
    "Shenzhen Stock Exchange: shareholding changes of directors, supervisors and senior managers since 2020",
    "深圳证券交易所 董监高持股变动（2020年起）",
    SZX % "1801_cxda&TABKEY=tab1&txtStart=2020-01-01",
    "SZSE's own register of dealings in their company's shares by directors, supervisors and senior managers and their relatives, from 2020-01-01: code and name, the officer, the person who actually traded and their relationship (本人, 配偶), duty, date, shares changed, average price, reason (竞价交易, 大宗交易) and holdings after. Fetched 2026-10-02 (20 s): 43,418 rows matching the JSON recordcount for the filter, newest 2026-09-29; the 7-field composite is distinct over 43,414, the 4 collisions being rows identical in every column. Date-scoped because the full table (123,935) exceeds the xlsx row limit.",
    "xlsx_sheet_index=0;id_keys=证券代码+董监高姓名+股份变动人姓名+变动日期+成交均价+变动比例(‰)+变动原因;title_keys=证券简称;date_keys=变动日期;pagination_ok=the xlsx export returns the whole filtered table in one file (43,418 of recordcount 43,418);interval=86400;timeout_ms=120000")

# ── SZSE listing review system (listing.szse.cn) ──
for bt, sid, what, zh, n, pages, ex in [
    (1, "IPO", "IPO applications (main board and ChiNext)", "首发", 1461, 15, "北京博华信智科技股份有限公司 (status 中止)"),
    (2, "REFINANCING", "refinancing applications (private placements, convertible bonds)", "再融资", 1215, 13, "四川安宁铁钛股份有限公司"),
    (3, "MNA", "major asset restructurings (M&A)", "重大资产重组", 145, 2, "晶瑞电子材料股份有限公司"),
]:
    row("JO40_CNMKT_SZSE_REVIEW_" + sid, "json", "finance", "listing-review-project",
        "szse,shenzhen,listing-review,ipo,intermediaries",
        "https://www.szse.cn/",
        "Shenzhen Stock Exchange listing review: " + what,
        "深圳证券交易所 发行上市审核项目动态 " + zh,
        "https://listing.szse.cn/api/ras/projectrends/query?bizType=%d&pageIndex=0&pageSize=100" % bt,
        "Projects in SZSE's registration-based review system: applicant full and short name, status (受理, 问询, 上市委会议, 提交注册, 注册生效, 中止, 终止), registered location, CSRC industry, sponsor and co-sponsor, law firm, accounting firm, planned raise, acceptance and last-update dates (e.g. %s). Fetched 2026-10-02: totalSize %s over %d pages of 100 (pageIndex is 0-based; the first page past the end returns no data), page 2 disjoint from page 1, prjid distinct." % (ex, format(n, ","), pages),
        "array_path=data;id_keys=prjid;title_keys=cmpnm;date_keys=updtdt;page_param=pageIndex;page_zero_based=1;page_max=%d;interval=86400;timeout_ms=60000" % (pages + 5))

# ── CSRC (www.csrc.gov.cn government-information channels) ──
for sid, ch, code, name, zh, n, newest, interval, rtype, extra in [
    ("PENALTIES", "17d5ff2fe43e488dba825807ae40d63f", "c101971", "administrative penalty decisions", "行政处罚决定", 2042, "2026-09-01",
     86400, "penalty-decision", "Full decision text for each case: decision number, the punished parties (people and companies), the violation found, the law applied and the fines and bans imposed; in a 60-decision sample the text cites 虚假记载 21 times, 内幕交易 19, 信息披露违法 14 and 操纵证券市场 7."),
    ("MARKET_BANS", "3795869930ca4b70bf55469270a6e641", "c101972", "securities-market ban decisions", "市场禁入决定", 366, "2024-04-25",
     86400, "market-ban", "Decisions barring named individuals from the securities market, with the banned person in the title (e.g. 市场禁入决定书（高松）), duration and full reasoning."),
    ("RECONSIDERATION", "77f99322f83040edb548974091db3f8b", "c101973", "administrative reconsideration decisions", "行政复议决定", 1540, "2023-12-29",
     604800, "reconsideration-decision", "Decisions on appeals against CSRC penalties and measures, the appellant named in the title (e.g. 行政复议决定书（马振罡）), with the full reasoning and outcome."),
    ("OVERSEAS_LISTING_FILINGS", "c822aa6e5c7248ed9cf3105891461fbc", "c101931", "overseas listing filings by Chinese companies", "境外发行上市备案", 1890, "2026-09-30",
     86400, "overseas-listing-filing", "CSRC's record of Chinese companies filing to list abroad: a 备案通知书 per named company (e.g. 关于深圳传音控股股份有限公司境外发行上市备案通知书), fortnightly 补充材料要求公示 listing the further material CSRC demanded from named applicants, filing-status tables and review progress."),
    ("LICENSING_RESULTS", "768f924c237f41b5921f6d0e4db0e8b5", "c101955", "administrative licensing results", "行政许可结果公示", 43049, "2026-09-30",
     86400, "licensing-decision", "CSRC approval and feedback documents, each naming the institution: IPO and follow-on share issuance registrations, share issuance for asset purchases, QFII licences for named foreign investors (e.g. 关于核准睿量资产管理（香港）有限公司合格境外投资者资格的批复), fund and REIT registrations and business-qualification feedback to brokers."),
    ("IPO_COMMITTEE", "f477b3febfb641b8841b1bd6ed3b4a77", "c101975", "IPO and M&A review committee meeting results", "发审会公告", 4997, "2023-02-13",
     604800, "ipo-review-result", "Announcements of the Issuance Examination Committee's meetings, naming each company reviewed and whether it passed; the channel closed with the 2023 move to registration-based IPOs."),
    ("IPO_PREDISCLOSURE", "de224a86ef974300a70142da8bfb4283", "c101974", "IPO prospectus pre-disclosures", "首发预先披露", 4814, "2020-09-18",
     604800, "ipo-prospectus", "Draft IPO prospectuses pre-disclosed by applicants under the approval system, titled with the company and filing date (e.g. 湖南丽臣实业股份有限公司首次公开发行股票招股说明书（申报稿）)."),
]:
    pages = -(-n // 20)
    row("JO40_CNMKT_CSRC_" + sid, "json", "regulatory", rtype,
        "csrc,regulator,securities," + ("enforcement" if sid in ("PENALTIES", "MARKET_BANS", "RECONSIDERATION") else "licensing"),
        "https://www.csrc.gov.cn/csrc/%s/zfxxgk_zdgk.shtml" % code,
        "China Securities Regulatory Commission: " + name,
        "中国证券监督管理委员会 " + zh,
        CSRC % ch,
        extra + " Fetched 2026-10-02 from the CSRC site's own search-list JSON: data.total %s at the server's fixed 20 per page (_pageSize=100 and 500 are answered with 20 on this endpoint), so %d pages; page %d returns an empty results array; newest publishedTimeStr %s. manuscriptId distinct over pages 1-2 and page 2 disjoint from page 1. Each record carries the title, publish time, channel, URL, the plain-text content and the HTML body." % (format(n, ","), pages, pages + 1, newest),
        "array_path=data.results;id_keys=manuscriptId;title_keys=title;date_keys=publishedTimeStr;body_keys=content;page_param=page;page_max=%d;interval=%d;timeout_ms=60000" % (int(pages * 1.12) + 3, interval))

# ── CNINFO security registers ──
for sid, f, what, zh, n, cats in [
    ("EQUITY_REGISTER", "szse_stock", "A-share, B-share and CDR security register with CNINFO organisation ids", "沪深京股票代码与机构编码", 6259, "6,179 A shares, 79 B shares and 1 CDR"),
    ("BOND_REGISTER", "bond_stock", "exchange bond register with issuer organisation ids", "债券代码与发行人机构编码", 28360, "18,239 local-government bonds, 5,143 asset-backed securities, 2,188 private placement bonds, 1,647 corporate bonds and 874 treasuries"),
    ("NEEQ_REGISTER", "gfzr_stock", "NEEQ (New Third Board) and legacy share-transfer company register", "股份转让系统挂牌公司代码", 6197, "5,895 股份报价 and 302 代办转让 companies"),
]:
    row("JO40_CNMKT_CNINFO_" + sid, "json", "registry", "security",
        "cninfo,register,securities,issuers",
        "https://www.cninfo.com.cn/new/index",
        "CNINFO: " + what,
        "巨潮资讯网 " + zh,
        "https://www.cninfo.com.cn/new/data/%s.json" % f,
        "The register CNINFO (the CSRC-designated disclosure site) uses to resolve a code to the organisation id that its announcement archive is keyed on: security code, short Chinese name, pinyin initials, category and orgId. Fetched 2026-10-02: stockList held %s records (%s); code distinct over all of them. The orgId is what links a security to every filing CNINFO holds for its issuer." % (format(n, ","), cats),
        "array_path=stockList;id_keys=code;title_keys=zwjc;pagination_ok=a static JSON file served whole (%s records in one response), no paging parameters exist;interval=604800;timeout_ms=60000" % format(n, ","))

# ── CFETS / chinamoney ──
row("JO40_CNMKT_CFETS_CCPR_HISTORY", "json", "finance", "fx-fixing",
    "cfets,chinamoney,rmb,central-parity,fx,pboc",
    "https://www.chinamoney.com.cn/chinese/bkccpr/",
    "CFETS: RMB central parity rates against 25 currencies, daily history",
    "中国外汇交易中心 人民币汇率中间价 历史数据",
    "https://www.chinamoney.com.cn/ags/ms/cm-u-bk-ccpr/CcprHisNew?startDate=&endDate=&currency=&pageNum=1&pageSize=50",
    "The PBOC-authorised RMB central parity (中间价) fixed each trading day at 9:15: one record per date whose values array follows data.head (USD/CNY, EUR/CNY, 100JPY/CNY, HKD/CNY, GBP/CNY ... CNY/THB, 25 pairs). Fetched 2026-10-02: the default window is the last month, 19 fixings from 2026-09-03 to 2026-09-30 on one page (pageTotal 1); the upstream refuses windows longer than a year, so the row is scheduled daily and accumulates the series. date distinct.",
    "array_path=records;id_keys=date;title_keys=date;date_keys=date;pagination_ok=the default window is the last month (19 to 23 fixings) and pageSize=50 holds it on one page, measured pageTotal=1;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_CFETS_SHIBOR_HISTORY", "json", "finance", "interest-rate-fixing",
    "cfets,chinamoney,shibor,interbank,interest-rates",
    "https://www.chinamoney.com.cn/chinese/bkshibor/",
    "CFETS: Shanghai Interbank Offered Rate (Shibor) daily fixings, all tenors",
    "中国外汇交易中心 上海银行间同业拆放利率 Shibor 历史",
    "https://www.chinamoney.com.cn/ags/ms/cm-u-bk-shibor/ShiborHis?lang=CN&startDate=&endDate=&pageNum=1&pageSize=50",
    "Daily Shibor fixings, one record per date with the overnight, 1-week, 2-week, 1-, 3-, 6-, 9-month and 1-year rates (e.g. 2026-09-30: O/N 1.3620, 1Y 1.4800). Fetched 2026-10-02: default window 2026-09-03 to 2026-09-30, totalCount 20 on one page (totalPageNum 1); showDateCN distinct.",
    "array_path=records;id_keys=showDateCN;title_keys=showDateCN;date_keys=showDateCN;pagination_ok=the default window is the last month (about 20 fixings) and pageSize=50 holds it on one page, measured totalPageNum=1;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_CFETS_LPR_HISTORY", "json", "finance", "interest-rate-fixing",
    "cfets,chinamoney,lpr,loan-prime-rate,interest-rates,pboc",
    "https://www.chinamoney.com.cn/chinese/bklpr/",
    "CFETS: Loan Prime Rate (LPR) monthly fixings, 1-year and 5-year",
    "中国外汇交易中心 贷款市场报价利率 LPR 历史",
    "https://www.chinamoney.com.cn/ags/ms/cm-u-bk-currency/LprHis?lang=CN&strStartDate=&strEndDate=&pageNum=1&pageSize=50",
    "China's benchmark lending rate, fixed on the 20th of each month: one record per fixing with the 1Y and 5Y LPR (2026-09-20: 3.00 and 3.50). Fetched 2026-10-02: the default window 2025-10-03 to 2026-10-02 returned 12 monthly fixings on one page; showDateCN distinct.",
    "array_path=records;id_keys=showDateCN;title_keys=showDateCN;date_keys=showDateCN;pagination_ok=the default window is one year (12 monthly fixings) on one page of 50;interval=86400;timeout_ms=60000")

row("JO40_CNMKT_CFETS_ENTERPRISE_BONDS", "json", "finance", "bond",
    "cfets,chinamoney,bonds,enterprise-bonds,lgfv,issuers",
    "https://www.chinamoney.com.cn/chinese/zqjc/",
    "CFETS bond information: enterprise bonds (企业债) registered in the interbank market",
    "中国货币网 债券信息 企业债",
    CMB % "100004",
    "Every enterprise bond (企业债) in the interbank bond register: bond short name, code, issuer full name and issuer id (e.g. 21随州建投绿色债01, 随州市城市建设综合开发投资有限公司), issue start and end dates where published, rating and ISIN fields, bond type and CFETS's own bond id. Fetched 2026-10-02: total 5,782 over pageTotal 386, page 386 holds 7 and page 387 is empty. The server returns at most 15 rows per page: pageSize=20 gives 15 rows with a pageTotal computed for 20 (76), which would skip records, and pageSize=50 or 99 is refused with HTTP 403, so the row walks 15 at a time; 40 consecutive pages walked without a refusal (0.4 s a page), 600 distinct bondDefinedCode.",
    "array_path=data.resultList;id_keys=bondDefinedCode;title_keys=bondName;date_keys=issueStartDate;page_param=pageNo;page_max=440;interval=604800;timeout_ms=60000")

row("JO40_CNMKT_CFETS_TREASURY_BONDS", "json", "finance", "bond",
    "cfets,chinamoney,bonds,treasury,sovereign",
    "https://www.chinamoney.com.cn/chinese/zqjc/",
    "CFETS bond information: Chinese government treasury bonds",
    "中国货币网 债券信息 国债",
    CMB % "100001",
    "Every central-government treasury bond in the interbank register, including discount bills and special bonds (e.g. 26贴现国债63, 26注资特别国债01, issue start 2026-10-08, issuer 中华人民共和国财政部): bond name, code, issue dates, issuer and CFETS bond id. Fetched 2026-10-02: total 1,518 over pageTotal 102 at the server's 15-row page, page 102 holds 3 and page 103 is empty; bondDefinedCode distinct over pages 1-2 and page 2 disjoint.",
    "array_path=data.resultList;id_keys=bondDefinedCode;title_keys=bondName;date_keys=issueStartDate;page_param=pageNo;page_max=120;interval=604800;timeout_ms=60000")

# ── CFFEX option books ──
for p, what, zh, n in [("IO", "CSI 300 index options", "沪深300股指期权", 236), ("MO", "CSI 1000 index options", "中证1000股指期权", 262), ("HO", "SSE 50 index options", "上证50股指期权", 158)]:
    row("JO40_CNMKT_CFFEX_%s_QUOTES" % p, "csv", "finance", "option-quote",
        "cffex,futures-exchange,options,derivatives,market-data",
        "http://www.cffex.com.cn/",
        "China Financial Futures Exchange: %s, every listed contract's quote" % what,
        "中国金融期货交易所 %s 行情" % zh,
        "http://www.cffex.com.cn/quote_%s.txt" % p,
        "The exchange's own quote file for every listed %s contract (each expiry, call and put, every strike): instrument id, open, high, low, last, change, best bid and ask with sizes, volume and open interest. Fetched 2026-10-02: %d contract rows under the header line, one per distinct instrument. The file holds the latest session's book, so the row is scheduled hourly." % (what, n),
        "id_keys=instrument;title_keys=instrument;pagination_ok=a plain CSV file holding the whole contract book (%d rows) in one response;interval=3600;timeout_ms=60000" % n)

# ── Sina Finance ──
row("JO40_CNMKT_SINA_ASHARE_SNAPSHOT", "json", "finance", "equity-quote",
    "sina-finance,a-shares,market-data,valuation",
    "https://vip.stock.finance.sina.com.cn/mkt/#hs_a",
    "Sina Finance: end-of-day snapshot of every Shanghai, Shenzhen and Beijing A-share",
    "新浪财经 沪深京A股行情中心",
    "https://vip.stock.finance.sina.com.cn/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=100&sort=symbol&asc=1&node=hs_a&_s_r_a=page",
    "Every A-share's latest quote and valuation: symbol, name, last price, change, open, high, low, bid/ask, volume, turnover, P/E, P/B, total and free-float market cap and turnover ratio. Fetched 2026-10-02: getHQNodeStockCount reports 5,571 stocks; 100 per page (num=500 and 6000 still return 100), page 56 is the last full page and page 57 returns an empty array; symbol distinct per page and the sort is by symbol, so pages do not overlap.",
    "id_keys=symbol;title_keys=name;page_param=page;page_max=62;interval=86400;timeout_ms=60000")

# ── emit ──
ids = set()
out = []
for r in rows:
    assert len(r) == 13, r[0]
    for i, f in enumerate(r):
        assert "|" not in f, (r[0], i, f)
        assert "\n" not in f, (r[0], i)
    assert r[0] not in ids, r[0]
    ids.add(r[0])
    out.append("|".join(r))
open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(HEAD + out) + "\n")
print("wrote %d rows" % len(out))
