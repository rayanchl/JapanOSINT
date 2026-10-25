/* collectors/pivot/table/hp3b55_atparl.c — batch 55: atparl — the Austrian
 * Parliament's whole materials index, read as seven complete slices.
 *
 * WHAT THIS CLOSES, AND WHY IT IS NOT A SORT.
 *
 * `AT_PARLAMENT_RECORDS` (hp3b31_eunat.c) is an entity pivot against the
 * parliament's own search page. Beside it, the same site publishes the FILTER
 * API behind "Alle parlamentarischen Materialien" — 391,354 bills, motions,
 * ministerial answers, committee reports and enquiries — and open item B2 has
 * carried that endpoint since 2026-10-21 as "one measured sort parameter away"
 * from shipping, because an offset walk with no stable ordering silently SKIPS
 * records and the only visible symptom is a collision (CLAUDE.md calls that the
 * worst shape it records).
 *
 * Measured 2026-10-23. There is NO sort parameter, and the row ships anyway,
 * because the same measurement found a route that needs no ordering at all.
 *
 *   * **Query-string parameters are ignored.** The filter is a POST with a
 *     JSON body: `{"GP":["XXVII"]}` answers count 76,457 where
 *     `?GP=XXVII` answers the unfiltered 391,354. That one fact explains the
 *     earlier reading — 26 sort spellings were probed in the query string and
 *     14 "moved" the answer, which looked like six or more working parameters
 *     and was none of them.
 *   * **The control proves it.** `frobnicate=7` and `sortColumn=99999` move the
 *     answer exactly as `sort=28` and `sortField=inr` do, and no reading is
 *     monotonic in the column it names. Nine sort spellings inside the POST
 *     body behave the same, `frobnicate` included: an unknown body key is
 *     silently ignored (`{"ITYP":["VER"],"zzzunknown":["x"]}` still answers
 *     that filter's 316).
 *   * **`pageNumber` is not the page parameter either; `page` is.** With
 *     `page`, page 19,568 returns the last 14 rows and 19,569 returns none —
 *     so it really pages. And it really drifts: page 1 and page 2 of ONE
 *     filtered query overlapped by **8 of 20 rows**, and two reads of `page=1`
 *     returned disjoint sets. That is the skip this row must not do.
 *   * **`showAll=true` plus a filter is a COMPLETE, DETERMINISTIC, SINGLE-PAGE
 *     read.** Every slice below answers `pages: 1` with `rows` equal to its own
 *     `count`, byte-identical across two reads. No ordering is involved,
 *     because nothing is paged.
 *
 * THE PARTITION IS EXHAUSTIVE BY THE UPSTREAM'S OWN ARITHMETIC.
 *
 * `GP` is the legislative period. Enumerating I..XXVIII found 24 non-empty
 * codes summing to 389,139 against an unfiltered 391,354 — **2,215 rows that no
 * Roman numeral reaches**, which is precisely the discard rule 2 names. Sampling
 * 30 pages found the two codes that hold them, `KN` (1,897) and `PN` (316), and
 * a third, `BR` (2), that the sample missed. The 26 codes passed as ONE filter
 * answer 391,352; plus BR's 2 that is 391,354 — the unfiltered count exactly.
 * So these 27 codes partition the index with nothing outside them.
 *
 * Seven rows rather than 27, grouped so each stays under the `showAll` page cap
 * (100,000 rows: a bigger slice comes back `pages: 2` and the engine would then
 * disclose the remainder against `count`, which is in its declared-total list)
 * and under `core/httpclient.c`'s 64 MB body ceiling. Closed periods are grouped
 * with closed periods: a group that mixes a finished period with the open one
 * would creep towards the ceiling, which is the lesson the ENA tuberculosis
 * split records. Measured, each `pages=1` and `rows == count == distinct`:
 *
 *     V–XVI + KN + PN + BR   50,896   22.3 MB   4.1 s
 *     XVII–XXI               77,646   39.5 MB   5.7 s
 *     XXII, XXIII, XXVI      55,695   33.8 MB   5.2 s
 *     XXIV                   51,970   31.3 MB   4.8 s
 *     XXV                    51,979   30.0 MB   5.2 s
 *     XXVII                  76,457   47.7 MB   5.8 s
 *     XXVIII (open)          26,714   16.9 MB   3.1 s
 *                           -------
 *                           391,357
 *
 * The sum is three above the 391,354 read minutes earlier: XXVIII gained three
 * materials between the two requests, which is the open period doing what an
 * open period does rather than a hole in the partition.
 *
 * THE RECORD KEY WAS MEASURED, AND THE OBVIOUS ONE IS WRONG.
 *
 * `GP_CODE+ITYP+INR` gives 75,485 distinct over XXVII's 76,457 rows — and so do
 * `WENTRY_ID` (the site's own web-item id) and `HIS_URL`, all three agreeing on
 * the same 75,485. Reading that as "the upstream's own duplicates" would have
 * been wrong: all 972 groups DIFFER, and they differ in exactly one column,
 * `NRBR` — a material that went through both chambers is served once as `NR`
 * and once as `BR` under one web-item id. `GP_CODE+ITYP+INR+NRBR` is 76,457 of
 * 76,457, and it holds on every slice above.
 *
 * `array_path` is DECLARED and must stay declared. `rows` is an array of
 * ARRAYS; `header` is an array of 36 OBJECTS. The engine's fallback picks the
 * densest array of objects, so without `array_path = "rows"` it would mine the
 * header and emit 36 column definitions as parliamentary materials.
 * `row_header_path = "header"` is what makes the cells land under `GP_CODE`,
 * `Betreff`, `NRBR` rather than `.0`, `.6`, `.29` (hpengine 2026-10-22; the
 * header entries are objects carrying a `label`, which hp_header_name reads).
 * Four of the 36 columns carry no label at all and keep their positional name,
 * which is the engine's documented behaviour and not a loss.
 *
 * WHAT TO DO WHEN XXVIII OUTGROWS ONE READ, measured so nobody has to re-probe.
 * The previous period reached 76,457 rows / 47.7 MB over its term, so the open
 * one will approach the ceiling. `DATUM` is a recognised body filter with
 * EXACT-VALUE SET semantics, not a range: `{"DATUM":["03.12.2020"]}` answers
 * 136, a two-element array is read as two days and not as an interval, and
 * `{"DATUM":{"von":…,"bis":…}}` is ignored. So a window is expressible as an
 * explicit list of days, which the engine's own `{date:%d.%m.%Y:-N}` tokens can
 * render: seven such days measured 163 records in 98 KB, `pages: 1`. That is
 * the split to make — not an offset walk, which this file exists to avoid.
 *
 * A live-tail row on that seven-day window was measured and deliberately NOT
 * shipped: the XXVIII slice already reads the open period WHOLE on every run at
 * 16.9 MB, so a second row would store the same records under a second
 * source_id — the double-store `lint-sources`' dup-endpoint check exists to
 * prevent — and buy no freshness.
 *
 * COST, stated because it is the one thing an operator may want to change:
 * one full cycle of these seven rows transfers ~222 MB. The declared interval is
 * weekly for the six closed slices and daily for the open one, but the
 * system-wide cadence policy (`core/cadence.c`, compiled default global 6 h)
 * sits above a declared value in BOTH directions — so on an unconfigured
 * database these run every six hours, ~890 MB a day. A per-source interval
 * (Settings › Collection interval, `core/source_interval.c`) outranks the
 * policy and is the remedy; the numbers above are here so that decision can be
 * made from a measurement.
 */

#include "../../../lib/hpengine.h"

static const hp_source HP3B55_ATPARL[] = {

  /* ── The closed periods, read whole ──────────────────────────────────── */

  { .id = "JO55_ATPARL_GP_V_XVI",
    .name = "Austria Parlament — materials, periods V–XVI and the non-period series",
    .name_ja = "オーストリア議会 資料 第5〜16期および期外系列",
    .category = "government", .portal = "https://www.parlament.gv.at",
    .record_type = "at-parliamentary-material",
    .tags = "\"at\",\"parliament\",\"legislation\"", .type = "api",
    .mode = HP_JSON, .free_tier = 1,
    .url = "https://www.parlament.gv.at/Filter/api/filter/data/101"
           "?js=eval&showAll=true",   /* exhaustive-ok: showAll returns the whole slice in one page (pages=1, rows==count, measured) */
    .base = "https://www.parlament.gv.at",
    .post_body = "{\"GP\":[\"V\",\"VI\",\"VII\",\"VIII\",\"IX\",\"X\",\"XI\","
      "\"XII\",\"XIII\",\"XIV\",\"XV\",\"XVI\",\"KN\",\"PN\",\"BR\"]}",
    .array_path = "rows", .row_header_path = "header",
    .title_keys = "Betreff,Nummer", .id_keys = "GP_CODE+ITYP+INR+NRBR",
    .date_keys = "DATUM_VON",
    .timeout_ms = 120000, .interval = 604800,
    .description = "Austrian parliamentary materials of the 5th to 16th "
      "legislative periods together with the three series that carry no period "
      "at all — KN (Kundmachungen), PN and BR — 50,896 records in one 22.3 MB "
      "read, each with its subject, designation, document type, chamber, "
      "status, date and the members, parties and EuroVoc subjects attached to "
      "it. The three non-period codes are here because enumerating the Roman "
      "numerals alone reaches 389,139 of 391,354 and they are the missing "
      "2,215" },

  { .id = "JO55_ATPARL_GP_XVII_XXI",
    .name = "Austria Parlament — materials, periods XVII–XXI",
    .name_ja = "オーストリア議会 資料 第17〜21期",
    .category = "government", .portal = "https://www.parlament.gv.at",
    .record_type = "at-parliamentary-material",
    .tags = "\"at\",\"parliament\",\"legislation\"", .type = "api",
    .mode = HP_JSON, .free_tier = 1,
    .url = "https://www.parlament.gv.at/Filter/api/filter/data/101"
           "?js=eval&showAll=true",   /* exhaustive-ok: showAll returns the whole slice in one page (pages=1, rows==count, measured) */
    .base = "https://www.parlament.gv.at",
    .post_body = "{\"GP\":[\"XVII\",\"XVIII\",\"XIX\",\"XX\",\"XXI\"]}",
    .array_path = "rows", .row_header_path = "header",
    .title_keys = "Betreff,Nummer", .id_keys = "GP_CODE+ITYP+INR+NRBR",
    .date_keys = "DATUM_VON",
    .timeout_ms = 120000, .interval = 604800,
    .description = "Austrian parliamentary materials of the 17th to 21st "
      "legislative periods (1990–2002), 77,646 records in one 39.5 MB read — "
      "the largest closed group this partition allows under the filter API's "
      "100,000-row single-page cap" },

  { .id = "JO55_ATPARL_GP_XXII_XXIII_XXVI",
    .name = "Austria Parlament — materials, periods XXII, XXIII and XXVI",
    .name_ja = "オーストリア議会 資料 第22・23・26期",
    .category = "government", .portal = "https://www.parlament.gv.at",
    .record_type = "at-parliamentary-material",
    .tags = "\"at\",\"parliament\",\"legislation\"", .type = "api",
    .mode = HP_JSON, .free_tier = 1,
    .url = "https://www.parlament.gv.at/Filter/api/filter/data/101"
           "?js=eval&showAll=true",   /* exhaustive-ok: showAll returns the whole slice in one page (pages=1, rows==count, measured) */
    .base = "https://www.parlament.gv.at",
    .post_body = "{\"GP\":[\"XXII\",\"XXIII\",\"XXVI\"]}",
    .array_path = "rows", .row_header_path = "header",
    .title_keys = "Betreff,Nummer", .id_keys = "GP_CODE+ITYP+INR+NRBR",
    .date_keys = "DATUM_VON",
    .timeout_ms = 120000, .interval = 604800,
    .description = "Austrian parliamentary materials of the 22nd, 23rd and 26th "
      "legislative periods, 55,695 records in one 33.8 MB read. The three are "
      "grouped on size rather than on chronology: the 24th, 25th and 27th each "
      "need a read of their own" },

  { .id = "JO55_ATPARL_GP_XXIV",
    .name = "Austria Parlament — materials, period XXIV",
    .name_ja = "オーストリア議会 資料 第24期",
    .category = "government", .portal = "https://www.parlament.gv.at",
    .record_type = "at-parliamentary-material",
    .tags = "\"at\",\"parliament\",\"legislation\"", .type = "api",
    .mode = HP_JSON, .free_tier = 1,
    .url = "https://www.parlament.gv.at/Filter/api/filter/data/101"
           "?js=eval&showAll=true",   /* exhaustive-ok: showAll returns the whole slice in one page (pages=1, rows==count, measured) */
    .base = "https://www.parlament.gv.at",
    .post_body = "{\"GP\":[\"XXIV\"]}",
    .array_path = "rows", .row_header_path = "header",
    .title_keys = "Betreff,Nummer", .id_keys = "GP_CODE+ITYP+INR+NRBR",
    .date_keys = "DATUM_VON",
    .timeout_ms = 120000, .interval = 604800,
    .description = "Austrian parliamentary materials of the 24th legislative "
      "period (2008–2013), 51,970 records in one 31.3 MB read" },

  { .id = "JO55_ATPARL_GP_XXV",
    .name = "Austria Parlament — materials, period XXV",
    .name_ja = "オーストリア議会 資料 第25期",
    .category = "government", .portal = "https://www.parlament.gv.at",
    .record_type = "at-parliamentary-material",
    .tags = "\"at\",\"parliament\",\"legislation\"", .type = "api",
    .mode = HP_JSON, .free_tier = 1,
    .url = "https://www.parlament.gv.at/Filter/api/filter/data/101"
           "?js=eval&showAll=true",   /* exhaustive-ok: showAll returns the whole slice in one page (pages=1, rows==count, measured) */
    .base = "https://www.parlament.gv.at",
    .post_body = "{\"GP\":[\"XXV\"]}",
    .array_path = "rows", .row_header_path = "header",
    .title_keys = "Betreff,Nummer", .id_keys = "GP_CODE+ITYP+INR+NRBR",
    .date_keys = "DATUM_VON",
    .timeout_ms = 120000, .interval = 604800,
    .description = "Austrian parliamentary materials of the 25th legislative "
      "period (2013–2017), 51,979 records in one 30.0 MB read" },

  { .id = "JO55_ATPARL_GP_XXVII",
    .name = "Austria Parlament — materials, period XXVII",
    .name_ja = "オーストリア議会 資料 第27期",
    .category = "government", .portal = "https://www.parlament.gv.at",
    .record_type = "at-parliamentary-material",
    .tags = "\"at\",\"parliament\",\"legislation\"", .type = "api",
    .mode = HP_JSON, .free_tier = 1,
    .url = "https://www.parlament.gv.at/Filter/api/filter/data/101"
           "?js=eval&showAll=true",   /* exhaustive-ok: showAll returns the whole slice in one page (pages=1, rows==count, measured) */
    .base = "https://www.parlament.gv.at",
    .post_body = "{\"GP\":[\"XXVII\"]}",
    .array_path = "rows", .row_header_path = "header",
    .title_keys = "Betreff,Nummer", .id_keys = "GP_CODE+ITYP+INR+NRBR",
    .date_keys = "DATUM_VON",
    .timeout_ms = 120000, .interval = 604800,
    .description = "Austrian parliamentary materials of the 27th legislative "
      "period (2019–2024), 76,457 records in one 47.7 MB read — the slice the "
      "whole shape of this beat was measured on, including the 972 materials "
      "served once per chamber that make NRBR part of the record key" },

  /* ── The open period: the same read, on a daily cadence ───────────────── */

  { .id = "JO55_ATPARL_GP_XXVIII_CURRENT",
    .name = "Austria Parlament — materials, period XXVIII (current)",
    .name_ja = "オーストリア議会 資料 第28期（現行）",
    .category = "government", .portal = "https://www.parlament.gv.at",
    .record_type = "at-parliamentary-material",
    .tags = "\"at\",\"parliament\",\"legislation\"", .type = "api",
    .mode = HP_JSON, .free_tier = 1,
    .url = "https://www.parlament.gv.at/Filter/api/filter/data/101"
           "?js=eval&showAll=true",   /* exhaustive-ok: showAll returns the whole slice in one page (pages=1, rows==count, measured) */
    .base = "https://www.parlament.gv.at",
    .post_body = "{\"GP\":[\"XXVIII\"]}",
    .array_path = "rows", .row_header_path = "header",
    .title_keys = "Betreff,Nummer", .id_keys = "GP_CODE+ITYP+INR+NRBR",
    .date_keys = "DATUM_VON",
    .timeout_ms = 120000, .interval = 86400,
    .description = "Austrian parliamentary materials of the current 28th "
      "legislative period, read WHOLE on every run — 26,714 records in one "
      "16.9 MB read when this shipped, and growing, so it is the one slice of "
      "this beat that needs re-measuring: the previous period reached 76,457 "
      "records and 47.7 MB over its term, and past roughly 90,000 records the "
      "filter API stops answering in a single page. The split to make then is "
      "an explicit list of days in the DATUM filter, which was measured here "
      "and is written up at the top of this file — never an offset walk, whose "
      "pages overlap by 40% on this endpoint" },
};

HP_REGISTER_TABLE(HP3B55_ATPARL)
