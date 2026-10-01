/* Verified-live il_legislative sources (10), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Knesset OData rows carry their identity in a per-table *ID column (AgendaID,
 * BillNameID, ...) that jsonlist's fixed id precedence does not know, so the
 * uid fell back to a hash of (Name, link, date) and rows sharing a Name
 * across $skip pages upserted over each other (sweep: 101 of 2,000 agenda
 * rows lost, 151 of 2,000 committee items). Every field named below was
 * read off the live response 2026-09-07 and is 100/100 distinct per page. */

VJSON(il_knesset_dates, "il-knesset-dates", "Knesset — terms and sessions calendar (KNS_KnessetDates)", "Knesset — terms and sessions calendar (KNS_KnessetDates)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_KnessetDates?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Knesset term register: term name, Knesset number, assembly and plenum ordinals, plenum start and finish dates, IsCurrent.");

VJSON_KEYED(il_knesset_mk_individual, "il-knesset-mk-individual", "Knesset — MK identity index (voting service)", "Knesset — MK identity index (voting service)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/Votes.svc/View_Vote_MK_Individual?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\"]", 86400,
  "MK roster used by the voting service: vip_id, mk_individual_id, and surname and first name in both Hebrew and English. The bridge between vote rows and named people.",
  "mk_individual_id");

VJSON_KEYED(il_knesset_mmm_keywords, "il-knesset-mmm-keywords", "Knesset Research and Information Center — keyword vocabulary", "Knesset Research and Information Center — keyword vocabulary",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/MMM.svc/keywords?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Controlled subject vocabulary for MMM papers, Hebrew and English, resolving keyword_id on the document_keyword join table.",
  "keyword_id");

VJSON_KEYED(il_knesset_plm_session_item, "il-knesset-plm-session-item", "Knesset — plenum agenda items (KNS_PlmSessionItem)", "Knesset — plenum agenda items (KNS_PlmSessionItem)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_PlmSessionItem?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Items on each plenary sitting agenda: item name, item type description, ordinal, status and IsDiscussion, linked by PlenumSessionID and ItemID.",
  "plmPlenumSessionID");

