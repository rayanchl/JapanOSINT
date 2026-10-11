import json,sys,collections
from sse import q
B="https://query.sse.com.cn/"
cands={
 "ann":B+"security/stock/queryCompanyBulletinNew.do?isPagination=true&pageHelp.pageSize=100&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&START_DATE=&END_DATE=&SECURITY_CODE=&TITLE=&BULLETIN_TYPE=&stockType=",
 "stocklist":B+"sseQuery/commonQuery.do?STOCK_TYPE=1&REG_PROVINCE=&CSRC_CODE=&STOCK_CODE=&sqlId=COMMON_SSE_CP_GPJCTPZ_GPLB_GP_L&COMPANY_STATUS=2,4,5,7,8&type=inParams&isPagination=true&pageHelp.cacheSize=1&pageHelp.beginPage=1&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.endPage=1",
 "tfp":B+"commonSoaQuery.do?isPagination=true&sqlId=GW_PL_JYTS_TFPXX&pageHelp.pageSize=1000&pageHelp.pageNo=1&productCode=&keyWords=&startStopDate=&endStopDate=",
 "inq":B+"commonSoaQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&sqlId=BS_KCB_GGLL_NEW&siteId=28&channelId=10743,10744,10012&order=createTime%7Cdesc,stockcode%7Casc",
 "promise":B+"commonQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&sqlId=SSE_XXPL_SSGSXX_SSGSCXJL_CNLYQK_L&promiseName=&promiseType=&itemType=&promiseStatus=&boardType=&companyCode=",
 "change":B+"commonQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&sqlId=COMMON_SSE_XXPL_CXJL_SSGSGFBDQK_S&COMPANY_CODE=&NAME=&BEGIN_DATE=&END_DATE=&BOARDTYPE=",
 "bondreg":B+"commonSoaQuery.do?isPagination=true&pageHelp.pageSize=1000&pageHelp.pageNo=1&pageHelp.beginPage=1&pageHelp.cacheSize=1&pageHelp.endPage=1&sqlId=BS_GGLL_NEW&siteId=28&channelId=9894,9895,9896,12428,12093,12094,12095,12429,9890,9891,9892,12426,12090,12091,12092,12427&order=createTime%7Cdesc,saveTime%7Cdesc",
 "intraday":B+"commonSoaQuery.do?isPagination=true&sqlId=SSE_PZLSTP_ZW&pageHelp.pageSize=1000&pageHelp.pageNo=1",
}
for k,u in (cands.items() if len(sys.argv)<2 else [(a,cands[a]) for a in sys.argv[1:]]):
    try:
        d=q(u)
    except Exception as e:
        print(k,"ERR",str(e)[:100]); continue
    ph=d.get("pageHelp") or {}
    data=ph.get("data") or d.get("result") or []
    print(k,"total",ph.get("total"),"pageCount",ph.get("pageCount"),"n",len(data), "keys",list(data[0].keys())[:25] if data else str(d)[:200])
    if data: print("   ",json.dumps(data[0],ensure_ascii=False)[:500])
    json.dump(d,open("sse_%s.json"%k,"w"),ensure_ascii=False)
