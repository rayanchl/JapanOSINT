p = "collectors/sources/gov_money.c"
s = open(p, encoding="utf-8").read()
start = s.index("/* www.customs.go.jp serves")
end = s.index("#define DEF(SYM, ID, NAME, NAMEJA")
new = '''/* Index-page anchor scrape of the trade-statistics section. customs.go.jp
 * serves Shift_JIS with no charset header - gm_emit_anchors() sniffs and
 * transcodes. NOTE: these rows are the section's link labels, NOT import/
 * export statistics; the real numbers sit behind the e-Stat query forms. */
static int run_customs(const source_ctx *ctx, intel_sink *sink) {
  gm_emit_anchors(ctx, sink,
    "https://www.customs.go.jp/toukei/info/index.htm", "/toukei/",
    "Customs Trade Statistics", "trade-statistics", "https://www.customs.go.jp",
    NULL, 30, "customs");
  return 0;
}

'''
open(p, "w", encoding="utf-8").write(s[:start] + new + s[end:])
print("removed", end - start, "chars; added", len(new))
