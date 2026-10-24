/* collectors/pivot/table/hp3_legco.c -- the Hong Kong Legislative Council's
 * three OData feeds, read whole.
 *
 * These were generated VJSON rows (vsrc13_eastasia_data_2.c, and a fourth in
 * vsrc_legal_1.c that read the bills feed a second time at 25 a page). Under
 * the 20-page VJSON ceiling they stored 2,000 bills, 1,200 speeches (then a
 * failed page) and 2,000 votes, of — the feeds' own odata.count, 2026-10-24:
 *
 *   BillsDB/odata/Vbills              6,884 bills
 *   OpenData/HansardDB/Speeches     551,619 speeches
 *   vrdb/odata/vVotingResult        542,997 member votes
 *
 * The server is slow and each feed has its own way to be walked, measured
 * from this machine on 2026-10-24/25:
 *   - bills: 100 rows ~20 s a page ($top=1000 did not finish in 180 s), so
 *     offset pages of 100 in key order, the whole feed (69 pages) every day;
 *   - votes: keyset on `id` is flat (38 s and 47 s for consecutive pages of
 *     5,000), so `id gt cursor` with page_resume=1 keeping the cursor across
 *     runs. But the server's throughput swings: the same 7.5 MB 5,000-row page
 *     took 27 s, 172 s and then stalled at 416 KB after 300 s within an hour,
 *     and the engine's 180 s timeout failed it twice. So pages of 1,000
 *     (1.5 MB, 18 s when measured), 60 a run, a 300 s timeout: ~9 six-hourly
 *     runs a cycle;
 *   - speeches: a keyset filter is unusable (`RundownID gt …` took 1,818 s
 *     for ONE page), but offset paging in the server's natural order is
 *     stable (the same 100 rows in the same order on a repeat at $skip
 *     300000) and affordable (15 s at $skip 0 and 300,000, 53 s at 550,000;
 *     500 rows in 68 s). So pages of 500 by $skip, 30 a run, page_resume=1:
 *     ~1,100 pages, a cycle every ~9 days at four runs a day. */
#include "lib/hpengine.h"

static const hp_source HP3_LEGCO[] = {
  { .id = "eas-hk-legco-bills", .name = "Legislative Council of Hong Kong — bills database",
    .name_ja = "香港立法会 — 法案データベース",
    .collector = "eastasia_data", .category = "gazette", .portal = "https://app.legco.gov.hk/BillsDB/",
    .record_type = "bill",
    .tags = "\"hkg\",\"legislature\",\"gazette\",\"politics\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "http://app.legco.gov.hk/BillsDB/odata/Vbills?$format=json&$orderby=internal_key&$top=100",
    .array_path = "value",
    .page_param = "$skip", .page_size = 100, .page_zero_based = 1, .page_max = 100,
    .id_keys = "internal_key",
    .title_keys = "bill_title_eng,bill_title_chi,ordinance_title_eng",
    .date_keys = "bill_gazette_date",
    .timeout_ms = 120000,
    .interval = 43200,
    .description = "Every bill introduced in the Hong Kong Legislative Council "
      "(6,884): titles in English and Chinese, the ordinance it amends, "
      "proposer, gazettal, first, second and third reading dates with their "
      "Hansard links, the bills committee and its report, and the resulting "
      "ordinance number." },

  { .id = "eas-hk-legco-speeches", .name = "Legislative Council of Hong Kong — Hansard speeches",
    .name_ja = "香港立法会 — 議事録発言記録",
    .collector = "eastasia_data", .category = "gazette", .portal = "https://app.legco.gov.hk/OpenData/HansardDB/",
    .record_type = "hansard-speech",
    .tags = "\"hkg\",\"legislature\",\"hansard\",\"politics\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "http://app.legco.gov.hk/OpenData/HansardDB/Speeches?$format=json&$top=500",
    .array_path = "value",
    .page_param = "$skip", .page_size = 500, .page_zero_based = 1, .page_max = 30,
    .page_resume = 1,
    .id_keys = "RundownID",
    .title_keys = "SpeakerName+SectionName,SectionName",
    .date_keys = "MeetingDate",
    .timeout_ms = 300000,
    .interval = 21600,
    .description = "Speech-level Hansard of Hong Kong Legislative Council sittings "
      "(551,619 speeches): meeting date, Hansard language, section, "
      "speaker and the text spoken, with the Hansard file. Read whole "
      "across runs, 15,000 speeches a run." },

  { .id = "eas-hk-legco-votes", .name = "Legislative Council of Hong Kong — voting results",
    .name_ja = "香港立法会 — 投票結果",
    .collector = "eastasia_data", .category = "gazette", .portal = "https://app.legco.gov.hk/vrdb/",
    .record_type = "legislative-vote",
    .tags = "\"hkg\",\"legislature\",\"voting\",\"politics\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "http://app.legco.gov.hk/vrdb/odata/vVotingResult?$format=json&$orderby=id&$top=1000", /* exhaustive-ok: keyset-paged by next_path and resumed across runs (page_resume=1) */
    .array_path = "value",
    .next_path = "value.$last.id",
    .next_tmpl = "http://app.legco.gov.hk/vrdb/odata/vVotingResult?$format=json&$orderby=id&$top=1000&$filter=id%20gt%20{v}",
    .page_max = 60, .page_resume = 1,
    .id_keys = "id",
    .title_keys = "motion_en,motion_ch",
    .date_keys = "vote_date",
    .timeout_ms = 300000,
    .interval = 21600,
    .description = "Every recorded division of the Hong Kong Legislative Council "
      "(542,997 member votes): committee or council, term, vote number, "
      "date and time, the motion in English and Chinese, mover, the "
      "separate-mechanism flag and the per-constituency and overall "
      "counts and results. Read whole across runs, 60,000 votes a run." },
};
HP_REGISTER_TABLE(HP3_LEGCO)
