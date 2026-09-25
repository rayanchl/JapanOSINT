/* Verified-live int_aid sources (7), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(iaticloud_activity, "iaticloud-activity", "IATI Datastore (iati.cloud) — aid activities", "IATI Datastore (iati.cloud) — aid activities",
  "int_aid", "aid",
  "https://iati.cloud/api/v2/activity/?format=json&page_size=100&q=*:*&fl=id,iati_identifier,title_narrative_text",
  "response.docs",
  "en", "[\"int\",\"aid\",\"batch17\",\"high-penetrancy\"]", 86400,
  "425,402 IATI activities published by every bilateral and multilateral donor — iati_identifier, title narratives, reporting org, participating orgs with role, recipient countries, sectors, budgets, planned disbursements and transactions. This is the keyless mirror of the IATI Datastore (api.iatistandard.org requires a subscription key). The q and fl parameters are BOTH required: without q the docs array is empty, and without fl=id the records carry only multi-valued arrays and every one is dropped for want of a scalar label.");

VJSON(iaticloud_organisation, "iaticloud-organisation", "IATI Datastore (iati.cloud) — publishing organisations", "IATI Datastore (iati.cloud) — publishing organisations",
  "int_aid", "aid",
  "https://iati.cloud/api/v2/organisation/?format=json&page_size=100&q=*:*&fl=id,organisation_identifier,organisation_name",
  "response.docs",
  "en", "[\"int\",\"aid\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Organisation files: organisation-identifier (US-GOV-1), name narratives, reporting org ref and type, total budget with period and budget lines, recipient-country and recipient-region budgets, total expenditure with expense lines, and document links. The org-level financial statement behind every IATI publisher.");

VJSON(iaticloud_result, "iaticloud-result", "IATI Datastore (iati.cloud) — activity results and indicators", "IATI Datastore (iati.cloud) — activity results and indicators",
  "int_aid", "aid",
  "https://iati.cloud/api/v2/result/?format=json&page_size=2&q=*:*",
  "response.docs",
  "en", "[\"int\",\"aid\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Results framework attached to aid activities: result title and description, indicator measure and title, baseline, period start/end, target and actual values with location refs and comments, plus the parent activity's iati-identifier, reporting org and dataset provenance. Note the label value is the result type code ('1'), so titles are weak — result_title and the indicator narrative are in props.");

VJSON(iaticloud_transaction, "iaticloud-transaction", "IATI Datastore (iati.cloud) — aid transactions", "IATI Datastore (iati.cloud) — aid transactions",
  "int_aid", "aid",
  "https://iati.cloud/api/v2/transaction/?format=json&page_size=100&q=*:*&fl=id,iati_identifier,transaction_value_usd,transaction_type",
  "response.docs",
  "en", "[\"int\",\"aid\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Individual aid transactions under each activity: transaction_ref, type (commitment/disbursement/expenditure), date, value in original currency and USD, provider and receiver organisation refs and narratives, recipient country and sector. Money-level granularity beneath the activity. Same fl=id requirement as the activity endpoint.");

VJSON(ocha_fts_flow_2026, "ocha-fts-flow-2026", "UN OCHA FTS — humanitarian funding flows 2026", "UN OCHA FTS — humanitarian funding flows 2026",
  "int_aid", "aid",
  "https://api.hpc.tools/v1/public/fts/flow?year=2026&limit=1000", /* limit=1000 is the server's page cap (5000 is answered with 1000); 20,161 flows on 2026-09-15 = 21 pages, followed through meta.nextLink */
  "data.flows",
  "en", "[\"int\",\"aid\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Donor-to-recipient money movements: amountUSD, original amount and currency, exchange rate, contribution type, flow type, earmarking, decision/first-reported dates, sourceObjects and destinationObjects (naming the donor organisation, the receiving agency, the plan, the cluster and the location), parent/child flow ids and reportDetails. Note the array is data.flows — the auto-detector would pick a metadata array instead.");

VJSON(ocha_fts_location, "ocha-fts-location", "UN OCHA FTS — location registry", "UN OCHA FTS — location registry",
  "int_aid", "aid",
  "https://api.hpc.tools/v1/public/location",
  "data",
  "en", "[\"int\",\"aid\",\"batch17\",\"high-penetrancy\"]", 86400,
  "258 countries/territories with FTS internal id, ISO3, pcode, admin level and isRegion flag — the join key that turns an FTS flow's destination id into a country.");

/* `project_id` is not in the uid precedence list, so every project fell back
 * to a hash of (title, link, date) and UNDP reuses a project title across
 * countries and phases — 4,000 emitted, 3,968 stored. Verified live
 * 2026-09-07 on one page: 200 records, 200 distinct project_id, 199 distinct
 * titles, which is the collision itself. `offset=0` is added because the
 * keyed path walks with lib/pagewalk.c, which advances only a cursor the URL
 * already names; without it the walk would stop at page 1. Verified live:
 * offset=200 returns a different first project. */
VJSON_KEYED(undp_transparency_projects, "undp-transparency-projects", "UNDP — transparency portal projects", "UNDP — transparency portal projects",
  "int_aid", "aid",
  "https://api.open.undp.org/api/project_list/?year=2024&limit=200&offset=0",
  "data.data",
  "en", "[\"int\",\"aid\",\"batch17\",\"high-penetrancy\"]", 86400,
  "UNDP project register: project_id, title, description, budget and expense in USD, country, sector list with codes, SDG mappings, signature solution, donor list and gender/policy markers. The year parameter is mandatory — without it the API returns 400.",
  "project_id");
