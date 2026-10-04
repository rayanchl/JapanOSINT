/* Verified-live za_parliament sources (10), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(af_pmg_daily_schedule, "af-pmg-daily-schedule", "PMG South Africa — parliamentary daily programme", "PMG South Africa — parliamentary daily programme",
  "za_parliament", "parliament",
  "https://api.pmg.org.za/daily-schedule/",
  "results",
  "en", "[\"za\",\"parliament\",\"batch17\",\"high-penetrancy\"]", 86400,
  "3,674 daily order papers for both houses. Row: id, title, start_date, body (the full day's programme listing committees, venues and times), nid, house_id, url and an embedded house object. No detail hop: /daily-schedule/{id}/ returns 404 even for a real id from the list, so the list body is the complete record.");


VJSON(af_pmg_committee_meetings, "af-pmg-committee-meetings", "PMG South Africa — parliamentary committee meetings", "PMG South Africa — parliamentary committee meetings",
  "za_parliament", "parliament",
  "https://api.pmg.org.za/committee-meeting/",
  "results",
  "en", "[\"za\",\"parliament\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Paginated feed of every monitored National Assembly / NCOP committee meeting. Row: id, date, title, type, body (full minutes), summary, member_id, committee_id, house_id, chairperson, public_participation, actual and scheduled start/end times, pmg_monitor, url. Detail by id returns the complete verbatim report.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
