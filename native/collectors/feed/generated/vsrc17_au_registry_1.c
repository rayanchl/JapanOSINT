/* Verified-live au_registry sources (8), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(au_acnc_charity_register, "au-acnc-charity-register", "ACNC — Australian charities register", "ACNC — Australian charities register",
  "au_registry", "registry",
  "https://data.gov.au/data/api/3/action/datastore_search?resource_id=eb1e6be4-5b13-4feb-b28e-388bf7c26f93&limit=100",
  "result.records",
  "en", "[\"au\",\"registry\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every registered Australian charity: ABN, Charity_Legal_Name, Other_Organisation_Names, full street address, Charity_Website, Registration_Date, Date_Organisation_Established, Charity_Size, Number_of_Responsible_Persons, financial year end and per-state operating flags.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");

VJSON(au_asic_company_register, "au-asic-company-register", "ASIC — Australian company register (full)", "ASIC — Australian company register (full)",
  "au_registry", "registry",
  "https://data.gov.au/data/api/3/action/datastore_search?resource_id=5c3914e6-413e-4a2c-b890-bf8efe3eabf2&limit=100",
  "result.records",
  "en", "[\"au\",\"registry\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every company ever registered in Australia: Company Name, ACN, Type, Class, Sub Class, Status (REGD/DRGD), Date of Registration, Date of Deregistration, Previous State of Registration, State Registration number, ABN, Current Name and Current Name Start Date. 4.4M rows, and &q=<name> full-text searches it server-side.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");

VJSON(au_qld_resource_authority_name_changes, "au-qld-resource-authority-name-changes", "Queensland — resource authority holder name changes", "Queensland — resource authority holder name changes",
  "au_registry", "registry",
  "https://www.data.qld.gov.au/api/3/action/datastore_search?resource_id=ad42f0d7-6cb4-45b4-995b-317fbc7a74d4&limit=100",
  "result.records",
  "en", "[\"au\",\"registry\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Mining and petroleum permit holder renames: Permit Number, Permit Type, Activity ID, Authorised Holder, Completion Date, LGA, and Notes spelling out the old and new corporate names verbatim. Tiny but it is a direct corporate-rename ledger for resource tenements.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");

VJSON(au_qld_water_service_providers, "au-qld-water-service-providers", "Queensland — registered water service providers", "Queensland — registered water service providers",
  "au_registry", "registry",
  "https://www.data.qld.gov.au/api/3/action/datastore_search?resource_id=bf61b1eb-fe8b-4c90-9699-86b35cc43fc2&limit=100",
  "result.records",
  "en", "[\"au\",\"registry\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Registered drinking-water and sewerage service providers: Name (literal field), SPID, and boolean flags for drinking water provider, water service and sewerage service. Small register but the label field is an exact 'Name'.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");

VJSON(au_vic_building_practitioner_register, "au-vic-building-practitioner-register", "Victorian Building Authority — building practitioner register", "Victorian Building Authority — building practitioner register",
  "au_registry", "registry",
  "https://discover.data.vic.gov.au/api/3/action/datastore_search?resource_id=3599fa1f-29f3-417e-8679-1842e2e6e2df&limit=100",
  "result.records",
  "en", "[\"au\",\"registry\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Registered Victorian building practitioners: Account Name, Type (Person / Company), Accreditation ID, Accreditation Status, ABN, ACN, Limitation (e.g. Commercial Builder - Limited) and commenced/expires dates. The ABN and ACN pivot into the ASIC company register.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
