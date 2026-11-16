/* Verified-live jp_government sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* 2026-11-07: maximumRecords=1 was the authoring probe's, and from=2026-01-01 a lower bound that grew
 * forever: the row read ONE sitting of 401 (measured 2026-11-07). 10 is the meeting API's
 * maximum (11 is error 19011). The API pages on startRecord/nextRecordPosition and states its
 * size as numberOfRecords. VJSON cannot advance startRecord, but since 2026-11-09 it reads
 * numberOfRecords as the declared total (lib/pagewalk.c), so a window holding more than 10
 * files a truncation notice naming the remainder. A 3-day window keeps it inside one page on most days (the
 * week of 2026-06-01, in session, held 19 sittings, about 8 per 3 days; the last 7 days hold 0,
 * the Diet being out of session). A complete read wants an hp row: page_param=startRecord,
 * page_size=10, page_start=1, array_path=meetingRecord, id_keys=issueID. */
VJSON(jp_kokkai_meeting_fulltext, "jp-kokkai-meeting-fulltext", "NDL Diet Minutes API - full meeting record (all speeches)", "NDL Diet Minutes API - full meeting record (all speeches)",
  "jp_government", "government",
  "https://kokkai.ndl.go.jp/api/meeting?nameOfHouse=%E8%A1%86%E8%AD%B0%E9%99%A2&maximumRecords=10&recordPacking=json&from={date-3d}",
  "meetingrecord",
  "ja", "[\"jp\",\"government\",\"batch16\",\"high-penetrancy\"]", 86400,
  "The verbatim-transcript endpoint, distinct from meeting_list and speech already in the tree. Each meetingRecord gives issueID, imageKind, session, nameOfHouse, nameOfMeeting, issue number, date, closing flag, plus a speechRecord array - here 18 speeches in one sitting - with speechID, speechOrder, speaker name, speakerYomi, speakerGroup (party), speakerPosition, speakerRole and the complete speech text. numberOfRecords was 321 sittings. This is the deepest hop into Japanese parliamentary proceedings: bill -> sitting -> named speaker -> full text.");
