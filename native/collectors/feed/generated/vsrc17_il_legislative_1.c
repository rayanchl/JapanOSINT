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
VJSON_KEYED(il_knesset_agenda, "il-knesset-agenda", "Knesset — agenda motions (KNS_Agenda)", "Knesset — agenda motions (KNS_Agenda)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_Agenda?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Motions for the agenda: motion name, classification, Knesset number, urgency subtype, status, InitiatorPersonID and MinisterPersonID (pivot to KNS_Person), CommitteeID, government recommendation and postponement reason.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "AgendaID");

VJSON_KEYED(il_knesset_bill_name, "il-knesset-bill-name", "Knesset — bill name history (KNS_BillName)", "Knesset — bill name history (KNS_BillName)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_BillName?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every historical title a bill carried, with the reading stage the title belonged to (NameHistoryTypeDesc). Lets you track a bill through renames; BillID pivots to the bill record.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "BillNameID");

VJSON_KEYED(il_knesset_cmt_session_item, "il-knesset-cmt-session-item", "Knesset — committee agenda items (KNS_CmtSessionItem)", "Knesset — committee agenda items (KNS_CmtSessionItem)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_CmtSessionItem?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "The individual items discussed inside a committee sitting: item name, ordinal, status, item type, and the CommitteeSessionID / ItemID that link back to the session and to the underlying bill or query.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "CmtSessionItemID");

VJSON_KEYED(il_knesset_committee_session, "il-knesset-committee-session", "Knesset — committee sessions (KNS_CommitteeSession)", "Knesset — committee sessions (KNS_CommitteeSession)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_CommitteeSession?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Committee sitting records: room/location, committee id, open-vs-closed type, status, start and finish datetimes, plus SessionUrl and BroadcastUrl.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "CommitteeSessionID");

VJSON(il_knesset_dates, "il-knesset-dates", "Knesset — terms and sessions calendar (KNS_KnessetDates)", "Knesset — terms and sessions calendar (KNS_KnessetDates)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_KnessetDates?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Knesset term register: term name, Knesset number, assembly and plenum ordinals, plenum start and finish dates, IsCurrent.");

VJSON_KEYED(il_knesset_faction, "il-knesset-faction", "Knesset — parliamentary factions (KNS_Faction)", "Knesset — parliamentary factions (KNS_Faction)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_Faction?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every faction across all Knessets: name, Knesset number, start and finish dates, IsCurrent flag. Resolves faction_id seen on vote records.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "FactionID");

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

VJSON_KEYED(il_knesset_query, "il-knesset-query", "Knesset — parliamentary questions (KNS_Query)", "Knesset — parliamentary questions (KNS_Query)",
  "il_legislative", "legislative",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_Query?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"legislative\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Parliamentary questions to ministers: subject, urgency type, status, asking PersonID, answering GovMinistryID, submit date and minister reply date. Direct MK-to-ministry accountability trail.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "QueryID");
