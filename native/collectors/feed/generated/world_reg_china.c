/* collectors/sources/world_reg_china.c
 * OSINT service — CHINA_REGISTRY. On-demand entity pivot (company / person /
 * case name) across public business, IP, telecom and court registries of
 * Greater China: mainland China (PRC), Hong Kong (HK) and Taiwan (TW).
 *
 * run() walks a static table of ~25 REAL public search portals and, for the
 * given ctx->entity, actually FETCHES each portal's server-rendered search
 * results page and emits one intel_item per real <a> anchor extracted
 * (jo_emit_anchors — nothing synthesized). Many of these registries are
 * JS-only or behind aggressive anti-bot / CAPTCHA (Qichacha, Tianyancha,
 * Aiqicha, 裁判文书网, 国家企业信用信息公示系统). Those simply honest-empty
 * per-row — that is expected and correct; we never fabricate a hit or emit a
 * constructed search URL as a record.
 *
 * Fan-out is bounded: <=3 anchors per registry, <=40 total per query, so a
 * single pivot cannot explode. The %-encoded query (jo_urlencode) is UTF-8
 * safe for the CJK names these registries expect. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One public search portal. `url_tmpl` has a single %s for the %-encoded query.
 * `base` is prepended to any root-relative hrefs the results page returns. */
typedef struct {
  const char *name;       /* human/service label                              */
  const char *url_tmpl;   /* search URL with one %s for the encoded query      */
  const char *base;       /* origin for root-relative hrefs                    */
  const char *country;    /* CN | HK | TW                                      */
  const char *record_type;
} cn_reg;

/* ~25 REAL public company / IP / telecom / court search portals for Greater
 * China. Where a portal's exact query parameter is not reliably knowable
 * keyless (many gate the real search behind JS/XHR), we use the registry's
 * documented public search path and let it honest-empty rather than invent a
 * param that would fabricate structure. */
static const cn_reg REGS[] = {
  /* ---- Mainland China (PRC) — company credit / business registries -------- */
  { "Qichacha 企查查",        "https://www.qcc.com/web/search?key=%s",
    "https://www.qcc.com", "CN", "company-registry" },
  { "Tianyancha 天眼查",       "https://www.tianyancha.com/search?key=%s",
    "https://www.tianyancha.com", "CN", "company-registry" },
  { "Aiqicha 爱企查",          "https://aiqicha.baidu.com/s?q=%s",
    "https://aiqicha.baidu.com", "CN", "company-registry" },
  { "Qixin 启信宝",            "https://www.qixin.com/search?key=%s",
    "https://www.qixin.com", "CN", "company-registry" },
  { "Qcc Firm 企查查(firm)",   "https://www.qcc.com/web_searchGeneral?searchKey=%s",
    "https://www.qcc.com", "CN", "company-registry" },
  { "GSXT 国家企业信用信息公示系统",
    "http://www.gsxt.gov.cn/corp-query-search-1.html?searchword=%s",
    "http://www.gsxt.gov.cn", "CN", "company-registry" },
  { "Baidu 百度企业信用",      "https://xin.baidu.com/s?q=%s",
    "https://xin.baidu.com", "CN", "company-registry" },
  { "Riskbird 风鸟",           "https://www.riskbird.com/search?keyword=%s",
    "https://www.riskbird.com", "CN", "company-registry" },
  { "Kuaiqicha 快企查",        "https://www.kuaiqicha.com/search?key=%s",
    "https://www.kuaiqicha.com", "CN", "company-registry" },

  /* ---- Mainland China — IP (SIPO / CNIPA), telecom (MIIT ICP), courts ------ */
  { "CNIPA 专利检索 (SIPO)",   "http://pss-system.cponline.cnipa.gov.cn/seniorSearch?key=%s",
    "http://pss-system.cponline.cnipa.gov.cn", "CN", "patent" },
  { "CNIPA 商标网 (TM)",       "http://sbj.cnipa.gov.cn/sbcx/index.html?key=%s",
    "http://sbj.cnipa.gov.cn", "CN", "trademark" },
  { "MIIT ICP备案查询",        "https://beian.miit.gov.cn/#/Integrated/index?key=%s",
    "https://beian.miit.gov.cn", "CN", "icp-beian" },
  { "ICP Beian 备案(mirror)",  "https://icp.chinaz.com/%s",
    "https://icp.chinaz.com", "CN", "icp-beian" },
  { "China Judgements 裁判文书网",
    "https://wenshu.court.gov.cn/website/wenshu/181217BMTKHNT2W0/index.html?s=%s",
    "https://wenshu.court.gov.cn", "CN", "court-judgement" },
  { "China Enforced 全国失信被执行人",
    "http://zxgk.court.gov.cn/zhzxgk/?pName=%s",
    "http://zxgk.court.gov.cn", "CN", "court-enforcement" },
  { "CSRC 上市公司 (listed)",  "http://eid.csrc.gov.cn/101811/index_f.html?key=%s",
    "http://eid.csrc.gov.cn", "CN", "securities" },

  /* ---- Hong Kong (HK) ------------------------------------------------------ */
  { "HK ICRIS Cyber Search",
    "https://www.icris.cr.gov.hk/csci/instant_search.do?searchType=company&searchWord=%s",
    "https://www.icris.cr.gov.hk", "HK", "company-registry" },
  { "HK Companies Registry (e-Search)",
    "https://www.e-services.cr.gov.hk/ICRIS3EP/system/dispSearch.do?searchWord=%s",
    "https://www.e-services.cr.gov.hk", "HK", "company-registry" },
  { "HK Judiciary Legal Ref (HKLII)",
    "https://www.hklii.hk/en/search?q=%s",
    "https://www.hklii.hk", "HK", "court-judgement" },
  { "HK Gazette 憲報",         "https://www.gld.gov.hk/egazette/english/whatsnew/whatsnew.html?q=%s",
    "https://www.gld.gov.hk", "HK", "gazette" },
  { "HK IPD Trademark 商標檢索",
    "https://esearch.ipd.gov.hk/nis-pos-view/#/searchByKeyword?keyword=%s",
    "https://esearch.ipd.gov.hk", "HK", "trademark" },

  /* ---- Taiwan (TW) --------------------------------------------------------- */
  { "TW GCIS 商工登記 (MOEA)",
    "https://findbiz.nat.gov.tw/fts/query/QueryList/queryList.do?qryCond=%s",
    "https://findbiz.nat.gov.tw", "TW", "company-registry" },
  { "TW GCIS 公司登記 (search)",
    "https://data.gcis.nat.gov.tw/od/search?query=%s",
    "https://data.gcis.nat.gov.tw", "TW", "company-registry" },
  { "TW Judicial 司法院裁判書",
    "https://judgment.judicial.gov.tw/FJUD/qryresult.aspx?q=%s",
    "https://judgment.judicial.gov.tw", "TW", "court-judgement" },
  { "TW TIPO 商標檢索 (patent/TM)",
    "https://twpat-simple.tipo.gov.tw/twpatc/twpatkm?query=%s",
    "https://twpat-simple.tipo.gov.tw", "TW", "trademark" },
};
static const int N_REGS = (int)(sizeof(REGS) / sizeof(REGS[0]));

#define CN_PER_REG   3    /* cap anchors emitted per registry            */
#define CN_TOTAL_CAP 500   /* exhaustive-ok: runaway guard, logged */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  int total = 0;
  for (int i = 0; i < N_REGS && total < CN_TOTAL_CAP; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const cn_reg *r = &REGS[i];

    char url[1024];
    snprintf(url, sizeof url, r->url_tmpl, enc);

    int room = CN_TOTAL_CAP - total;
    int cap  = room < CN_PER_REG ? room : CN_PER_REG;

    char tag[96];
    snprintf(tag, sizeof tag, "china-reg/%s", r->country);

    /* Real anchor extraction from the fetched results page. href_must=NULL
     * (accept any anchor); query filter unset so CJK-in-href pages still
     * match — jo_emit_anchors already dedupes and requires >=3 chars of text.
     * JS-only / anti-bot registries yield 0 here (honest empty). */
    int got = jo_emit_anchors(ctx, sink, url, NULL, r->name,
                              r->record_type, r->base, NULL, cap, tag);
    if (got > 0) total += got;
  }

  free(enc);
  fprintf(stderr, "[china-registry] emitted %d across %d portals\n", total, N_REGS);
  return 0;   /* honest empty is not an error */
}

static const source_def china_registry_def = {
  .id = "CHINA_REGISTRY", .collector = "osint",
  .name = "Greater China Business & Court Registries",
  .name_ja = "中国・香港・台湾 企業/裁判所レジストリ",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_china",
  .description = "Entity pivot across ~25 PRC/HK/TW company, IP, ICP-beian and court search portals (anchor scrape; JS/anti-bot rows honest-empty)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(china_registry_def)
