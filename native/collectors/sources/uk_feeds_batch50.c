/* collectors/sources/uk_feeds_batch50.c
 * Batch 50 — 1111 United Kingdom government / institutional RSS-Atom feeds.
 *
 * Every row was fetched live on 2026-10-20: HTTP 200, an <rss>/<feed>/<rdf> root,
 * and at least one <item>/<entry>. URLs already registered anywhere in the tree
 * were dropped before probing.
 *
 * 32 of these were found by AUTODISCOVERY: the official homepage (from
 * Wikidata P856, or a host already in the tree) was fetched and its own
 * <link rel="alternate" type="application/rss+xml"> declarations were read —
 * the site was asked what it serves rather than probed at guessed paths.
 * Closed gov.uk bodies (gov.uk organisations API govuk_status=closed) poll weekly.
 * Per-row probe counts: docs/verified-sources-batch50.tsv. */
#include "_source_macros.inc"

/* ── crime ───────────────────────────────────────────────────────────── */

RSSX(uk_b50_govuk_british_transport_police_authority, "uk-b50-govuk-british-transport-police-authority",
     "British Transport Police Authority", "British Transport Police Authority",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/british-transport-police-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Transport Police Authority\",\"safety\",\"batch:50\"]",
     21600,
     "British Transport Police Authority — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_central_police_training_and_development_authority, "uk-b50-govuk-central-police-training-and-development-authority",
     "Central Police Training and Development Authority", "Central Police Training and Development Authority",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/central-police-training-and-development-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Central Police Training and Development Authority\",\"safety\",\"batch:50\"]",
     604800,
     "Central Police Training and Development Authority — safety feed (2 entries at probe)")

RSSX(uk_b50_govuk_civil_nuclear_police_authority, "uk-b50-govuk-civil-nuclear-police-authority",
     "UK CNC", "UK CNC",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/civil-nuclear-police-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK CNC\",\"safety\",\"batch:50\"]",
     3600,
     "UK CNC — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_college_of_policing, "uk-b50-govuk-college-of-policing",
     "UK College of Policing", "UK College of Policing",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/college-of-policing.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK College of Policing\",\"safety\",\"batch:50\"]",
     21600,
     "UK College of Policing — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_police_complaint_commission, "uk-b50-govuk-independent-police-complaint-commission",
     "Independent Police Complaints Commission", "Independent Police Complaints Commission",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/independent-police-complaint-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Police Complaints Commission\",\"safety\",\"batch:50\"]",
     604800,
     "Independent Police Complaints Commission — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_review_of_police_officer_and_staff_remuneration_and_conditions, "uk-b50-govuk-independent-review-of-police-officer-and-staff-remuneration-and-conditions",
     "Independent Review of Police Officer and Staff Remuneration and Conditions", "Independent Review of Police Officer and Staff Remuneration and Conditions",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/independent-review-of-police-officer-and-staff-remuneration-and-conditions.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Review of Police Officer and Staff Remuneration and Conditions\",\"safety\",\"batch:50\"]",
     604800,
     "Independent Review of Police Officer and Staff Remuneration and Conditions — safety feed (2 entries at probe)")

RSSX(uk_b50_govuk_ministry_of_defence_police_and_guarding_agency, "uk-b50-govuk-ministry-of-defence-police-and-guarding-agency",
     "Ministry of Defence Police and Guarding Agency", "Ministry of Defence Police and Guarding Agency",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/ministry-of-defence-police-and-guarding-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ministry of Defence Police and Guarding Agency\",\"safety\",\"batch:50\"]",
     604800,
     "Ministry of Defence Police and Guarding Agency — safety feed (1 entries at probe)")

RSSX(uk_b50_govuk_national_crime_agency_remuneration_review_body, "uk-b50-govuk-national-crime-agency-remuneration-review-body",
     "National Crime Agency Remuneration Review Body", "National Crime Agency Remuneration Review Body",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/national-crime-agency-remuneration-review-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Crime Agency Remuneration Review Body\",\"safety\",\"batch:50\"]",
     3600,
     "National Crime Agency Remuneration Review Body — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_crime_squad, "uk-b50-govuk-national-crime-squad",
     "National Crime Squad", "National Crime Squad",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/national-crime-squad.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Crime Squad\",\"safety\",\"batch:50\"]",
     604800,
     "National Crime Squad — safety feed (2 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_police_fund, "uk-b50-govuk-northern-ireland-police-fund",
     "Northern Ireland Police Fund", "Northern Ireland Police Fund",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/northern-ireland-police-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Police Fund\",\"safety\",\"batch:50\"]",
     604800,
     "Northern Ireland Police Fund — safety feed (3 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_police_ombudsman_for_northern_ireland, "uk-b50-govuk-office-of-the-police-ombudsman-for-northern-ireland",
     "Office of the Police Ombudsman for Northern Ireland", "Office of the Police Ombudsman for Northern Ireland",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/office-of-the-police-ombudsman-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Police Ombudsman for Northern Ireland\",\"safety\",\"batch:50\"]",
     21600,
     "Office of the Police Ombudsman for Northern Ireland — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_police_advisory_board_for_england_and_wales, "uk-b50-govuk-police-advisory-board-for-england-and-wales",
     "Police Advisory Board for England and Wales", "Police Advisory Board for England and Wales",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/police-advisory-board-for-england-and-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Police Advisory Board for England and Wales\",\"safety\",\"batch:50\"]",
     3600,
     "Police Advisory Board for England and Wales — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_police_complaints_authority, "uk-b50-govuk-police-complaints-authority",
     "Police Complaints Authority", "Police Complaints Authority",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/police-complaints-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Police Complaints Authority\",\"safety\",\"batch:50\"]",
     604800,
     "Police Complaints Authority — safety feed (1 entries at probe)")

RSSX(uk_b50_govuk_police_information_technology_organisation, "uk-b50-govuk-police-information-technology-organisation",
     "Police Information Technology Organisation", "Police Information Technology Organisation",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/police-information-technology-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Police Information Technology Organisation\",\"safety\",\"batch:50\"]",
     604800,
     "Police Information Technology Organisation — safety feed (2 entries at probe)")

RSSX(uk_b50_govuk_police_negotiating_board, "uk-b50-govuk-police-negotiating-board",
     "Police Negotiating Board", "Police Negotiating Board",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/police-negotiating-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Police Negotiating Board\",\"safety\",\"batch:50\"]",
     604800,
     "Police Negotiating Board — safety feed (2 entries at probe)")

RSSX(uk_b50_govuk_police_remuneration_review_body, "uk-b50-govuk-police-remuneration-review-body",
     "Police Remuneration Review Body", "Police Remuneration Review Body",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/police-remuneration-review-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Police Remuneration Review Body\",\"safety\",\"batch:50\"]",
     3600,
     "Police Remuneration Review Body — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_police_service_of_northern_ireland, "uk-b50-govuk-police-service-of-northern-ireland",
     "Police Service of Northern Ireland", "Police Service of Northern Ireland",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/police-service-of-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Police Service of Northern Ireland\",\"safety\",\"batch:50\"]",
     21600,
     "Police Service of Northern Ireland — safety feed (20 entries at probe)")

RSSX(uk_b50_govuk_serious_organised_crime_agency, "uk-b50-govuk-serious-organised-crime-agency",
     "Serious Organised Crime Agency", "Serious Organised Crime Agency",
     "crime", "safety",
     "https://www.gov.uk/government/organisations/serious-organised-crime-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Serious Organised Crime Agency\",\"safety\",\"batch:50\"]",
     604800,
     "Serious Organised Crime Agency — safety feed (12 entries at probe)")

/* ── defense ─────────────────────────────────────────────────────────── */

RSSX(uk_b50_govuk_advisory_group_on_military_medicine, "uk-b50-govuk-advisory-group-on-military-medicine",
     "Advisory Group on Military and Emergency Response Medicine", "Advisory Group on Military and Emergency Response Medicine",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/advisory-group-on-military-medicine.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Group on Military and Emergency Response Medicine\",\"defense\",\"batch:50\"]",
     3600,
     "Advisory Group on Military and Emergency Response Medicine — defense feed (1 entries at probe)")

RSSX(uk_b50_govuk_army_base_repair_organisation, "uk-b50-govuk-army-base-repair-organisation",
     "Army Base Repair Organisation", "Army Base Repair Organisation",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/army-base-repair-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Army Base Repair Organisation\",\"defense\",\"batch:50\"]",
     604800,
     "Army Base Repair Organisation — defense feed (2 entries at probe)")

RSSX(uk_b50_govuk_centre_for_defence_enterprise, "uk-b50-govuk-centre-for-defence-enterprise",
     "Centre for Defence Enterprise", "Centre for Defence Enterprise",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/centre-for-defence-enterprise.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Centre for Defence Enterprise\",\"defense\",\"batch:50\"]",
     604800,
     "Centre for Defence Enterprise — defense feed (20 entries at probe)")

RSSX(uk_b50_govuk_chemical_and_biological_defence_establishment, "uk-b50-govuk-chemical-and-biological-defence-establishment",
     "Chemical and Biological Defence Establishment", "Chemical and Biological Defence Establishment",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/chemical-and-biological-defence-establishment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Chemical and Biological Defence Establishment\",\"defense\",\"batch:50\"]",
     604800,
     "Chemical and Biological Defence Establishment — defense feed (1 entries at probe)")

RSSX(uk_b50_govuk_defence_academy, "uk-b50-govuk-defence-academy",
     "Defence Academy of the United Kingdom", "Defence Academy of the United Kingdom",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-academy.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Academy of the United Kingdom\",\"defense\",\"batch:50\"]",
     21600,
     "Defence Academy of the United Kingdom — defense feed (9 entries at probe)")

RSSX(uk_b50_govuk_defence_analytical_services_agency, "uk-b50-govuk-defence-analytical-services-agency",
     "Defence Analytical Services Agency", "Defence Analytical Services Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-analytical-services-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Analytical Services Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Analytical Services Agency — defense feed (4 entries at probe)")

RSSX(uk_b50_govuk_defence_aviation_repair_agency, "uk-b50-govuk-defence-aviation-repair-agency",
     "Defence Aviation Repair Agency", "Defence Aviation Repair Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-aviation-repair-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Aviation Repair Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Aviation Repair Agency — defense feed (4 entries at probe)")

RSSX(uk_b50_govuk_defence_bills_agency, "uk-b50-govuk-defence-bills-agency",
     "Defence Bills Agency", "Defence Bills Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-bills-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Bills Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Bills Agency — defense feed (1 entries at probe)")

RSSX(uk_b50_govuk_defence_communication_services_agency, "uk-b50-govuk-defence-communication-services-agency",
     "Defence Communication Services Agency", "Defence Communication Services Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-communication-services-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Communication Services Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Communication Services Agency — defense feed (2 entries at probe)")

RSSX(uk_b50_govuk_defence_estates, "uk-b50-govuk-defence-estates",
     "Defence Estates", "Defence Estates",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-estates.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Estates\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Estates — defense feed (3 entries at probe)")

RSSX(uk_b50_govuk_defence_intelligence_and_security_centre, "uk-b50-govuk-defence-intelligence-and-security-centre",
     "Defence Intelligence and Security Centre", "Defence Intelligence and Security Centre",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-intelligence-and-security-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Intelligence and Security Centre\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Intelligence and Security Centre — defense feed (1 entries at probe)")

RSSX(uk_b50_govuk_defence_nuclear_enterprise, "uk-b50-govuk-defence-nuclear-enterprise",
     "Defence Nuclear Enterprise", "Defence Nuclear Enterprise",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-nuclear-enterprise.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Nuclear Enterprise\",\"defense\",\"batch:50\"]",
     3600,
     "Defence Nuclear Enterprise — defense feed (10 entries at probe)")

RSSX(uk_b50_govuk_defence_nuclear_organisation, "uk-b50-govuk-defence-nuclear-organisation",
     "Defence Nuclear Organisation", "Defence Nuclear Organisation",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-nuclear-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Nuclear Organisation\",\"defense\",\"batch:50\"]",
     3600,
     "Defence Nuclear Organisation — defense feed (20 entries at probe)")

RSSX(uk_b50_govuk_defence_nuclear_safety_committee, "uk-b50-govuk-defence-nuclear-safety-committee",
     "Defence Nuclear Safety Expert Committee", "Defence Nuclear Safety Expert Committee",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-nuclear-safety-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Nuclear Safety Expert Committee\",\"defense\",\"batch:50\"]",
     3600,
     "Defence Nuclear Safety Expert Committee — defense feed (3 entries at probe)")

RSSX(uk_b50_govuk_defence_procurement_agency, "uk-b50-govuk-defence-procurement-agency",
     "Defence Procurement Agency", "Defence Procurement Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-procurement-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Procurement Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Procurement Agency — defense feed (2 entries at probe)")

RSSX(uk_b50_govuk_defence_scientific_advisory_council, "uk-b50-govuk-defence-scientific-advisory-council",
     "Defence Scientific Advisory Council", "Defence Scientific Advisory Council",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-scientific-advisory-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Scientific Advisory Council\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Scientific Advisory Council — defense feed (4 entries at probe)")

RSSX(uk_b50_govuk_defence_storage_and_distribution_agency, "uk-b50-govuk-defence-storage-and-distribution-agency",
     "Defence Storage and Distribution Agency", "Defence Storage and Distribution Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-storage-and-distribution-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Storage and Distribution Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Storage and Distribution Agency — defense feed (4 entries at probe)")

RSSX(uk_b50_govuk_defence_support_group, "uk-b50-govuk-defence-support-group",
     "Defence Support Group", "Defence Support Group",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-support-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Support Group\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Support Group — defense feed (7 entries at probe)")

RSSX(uk_b50_govuk_defence_transport_and_movements_agency, "uk-b50-govuk-defence-transport-and-movements-agency",
     "Defence Transport and Movements Agency", "Defence Transport and Movements Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-transport-and-movements-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Transport and Movements Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Transport and Movements Agency — defense feed (2 entries at probe)")

RSSX(uk_b50_govuk_defence_vetting_agency, "uk-b50-govuk-defence-vetting-agency",
     "Defence Vetting Agency", "Defence Vetting Agency",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/defence-vetting-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Vetting Agency\",\"defense\",\"batch:50\"]",
     604800,
     "Defence Vetting Agency — defense feed (5 entries at probe)")

RSSX(uk_b50_govuk_department_for_international_trade_defence_and_security_organisation, "uk-b50-govuk-department-for-international-trade-defence-and-security-organisation",
     "Department for International Trade Defence & Security Organisation", "Department for International Trade Defence & Security Organisation",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/department-for-international-trade-defence-and-security-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for International Trade Defence & Security Organisation\",\"defense\",\"batch:50\"]",
     604800,
     "Department for International Trade Defence & Security Organisation — defense feed (20 entries at probe)")

RSSX(uk_b50_govuk_jhub_defence_innovation, "uk-b50-govuk-jhub-defence-innovation",
     "jHub Defence Innovation", "jHub Defence Innovation",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/jhub-defence-innovation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:jHub Defence Innovation\",\"defense\",\"batch:50\"]",
     3600,
     "jHub Defence Innovation — defense feed (16 entries at probe)")

RSSX(uk_b50_govuk_military_aviation_authority, "uk-b50-govuk-military-aviation-authority",
     "Military Aviation Authority", "Military Aviation Authority",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/military-aviation-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Military Aviation Authority\",\"defense\",\"batch:50\"]",
     3600,
     "Military Aviation Authority — defense feed (20 entries at probe)")

RSSX(uk_b50_govuk_royal_air_force_museum, "uk-b50-govuk-royal-air-force-museum",
     "Royal Air Force Museum", "Royal Air Force Museum",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/royal-air-force-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Air Force Museum\",\"defense\",\"batch:50\"]",
     21600,
     "Royal Air Force Museum — defense feed (12 entries at probe)")

RSSX(uk_b50_govuk_royal_marines_museum, "uk-b50-govuk-royal-marines-museum",
     "Royal Marines Museum", "Royal Marines Museum",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/royal-marines-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Marines Museum\",\"defense\",\"batch:50\"]",
     21600,
     "Royal Marines Museum — defense feed (7 entries at probe)")

RSSX(uk_b50_govuk_royal_naval_museum, "uk-b50-govuk-royal-naval-museum",
     "Royal Navy Museums", "Royal Navy Museums",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/royal-naval-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Navy Museums\",\"defense\",\"batch:50\"]",
     21600,
     "Royal Navy Museums — defense feed (16 entries at probe)")

RSSX(uk_b50_govuk_royal_navy_submarine_museum, "uk-b50-govuk-royal-navy-submarine-museum",
     "Royal Navy Submarine Museum", "Royal Navy Submarine Museum",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/royal-navy-submarine-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Navy Submarine Museum\",\"defense\",\"batch:50\"]",
     21600,
     "Royal Navy Submarine Museum — defense feed (7 entries at probe)")

RSSX(uk_b50_govuk_uk_defence_and_security_exports, "uk-b50-govuk-uk-defence-and-security-exports",
     "UK Defence and Security Exports", "UK Defence and Security Exports",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/uk-defence-and-security-exports.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Defence and Security Exports\",\"defense\",\"batch:50\"]",
     3600,
     "UK Defence and Security Exports — defense feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_defence_innovation, "uk-b50-govuk-uk-defence-innovation",
     "UK Defence Innovation", "UK Defence Innovation",
     "defense", "defense",
     "https://www.gov.uk/government/organisations/uk-defence-innovation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Defence Innovation\",\"defense\",\"batch:50\"]",
     3600,
     "UK Defence Innovation — defense feed (20 entries at probe)")

/* ── environment ─────────────────────────────────────────────────────── */

RSSX(uk_b50_govuk_accelerated_capability_environment, "uk-b50-govuk-accelerated-capability-environment",
     "Accelerated Capability Environment", "Accelerated Capability Environment",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/accelerated-capability-environment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Accelerated Capability Environment\",\"environment\",\"batch:50\"]",
     3600,
     "Accelerated Capability Environment — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_releases_to_the_environment, "uk-b50-govuk-advisory-committee-on-releases-to-the-environment",
     "Advisory Committee on Releases to the Environment", "Advisory Committee on Releases to the Environment",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/advisory-committee-on-releases-to-the-environment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on Releases to the Environment\",\"environment\",\"batch:50\"]",
     3600,
     "Advisory Committee on Releases to the Environment — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_commission_for_architecture_and_the_built_environment_cabe, "uk-b50-govuk-commission-for-architecture-and-the-built-environment-cabe",
     "Commission for Architecture and the Built Environment (CABE)", "Commission for Architecture and the Built Environment (CABE)",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/commission-for-architecture-and-the-built-environment-cabe.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for Architecture and the Built Environment (CABE)\",\"environment\",\"batch:50\"]",
     604800,
     "Commission for Architecture and the Built Environment (CABE) — environment feed (7 entries at probe)")

RSSX(uk_b50_govuk_committee_on_climate_change, "uk-b50-govuk-committee-on-climate-change",
     "Committee on Climate Change", "Committee on Climate Change",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/committee-on-climate-change.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Committee on Climate Change\",\"environment\",\"batch:50\"]",
     21600,
     "Committee on Climate Change — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_committee_on_mutagenicity_of_chemicals_in_food_consumer_products_and_the_environment, "uk-b50-govuk-committee-on-mutagenicity-of-chemicals-in-food-consumer-products-and-the-environment",
     "Committee on Mutagenicity of Chemicals in Food, Consumer Products and the Environment", "Committee on Mutagenicity of Chemicals in Food, Consumer Products and the Environment",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/committee-on-mutagenicity-of-chemicals-in-food-consumer-products-and-the-environment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Committee on Mutagenicity of Chemicals in Food, Consumer Products and the Environment\",\"environment\",\"batch:50\"]",
     3600,
     "Committee on Mutagenicity of Chemicals in Food, Consumer Products and the Environment — environment feed (13 entries at probe)")

RSSX(uk_b50_govuk_committee_on_toxicity_of_chemicals_in_food_consumer_products_and_the_environment, "uk-b50-govuk-committee-on-toxicity-of-chemicals-in-food-consumer-products-and-the-environment",
     "Committee on Toxicity of Chemicals in Food, Consumer Products and the Environment", "Committee on Toxicity of Chemicals in Food, Consumer Products and the Environment",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/committee-on-toxicity-of-chemicals-in-food-consumer-products-and-the-environment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Committee on Toxicity of Chemicals in Food, Consumer Products and the Environment\",\"environment\",\"batch:50\"]",
     3600,
     "Committee on Toxicity of Chemicals in Food, Consumer Products and the Environment — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_agriculture_environment_and_rural_affairs_northern_ireland, "uk-b50-govuk-department-of-agriculture-environment-and-rural-affairs-northern-ireland",
     "Department of Agriculture, Environment and Rural Affairs (Northern Ireland)", "Department of Agriculture, Environment and Rural Affairs (Northern Ireland)",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/department-of-agriculture-environment-and-rural-affairs-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Agriculture, Environment and Rural Affairs (Northern Ireland)\",\"environment\",\"batch:50\"]",
     21600,
     "Department of Agriculture, Environment and Rural Affairs (Northern Ireland) — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_energy_climate_change, "uk-b50-govuk-department-of-energy-climate-change",
     "Department of Energy & Climate Change", "Department of Energy & Climate Change",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/department-of-energy-climate-change.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Energy & Climate Change\",\"environment\",\"batch:50\"]",
     604800,
     "Department of Energy & Climate Change — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_the_environment, "uk-b50-govuk-department-of-the-environment",
     "Department of the Environment (Northern Ireland)", "Department of the Environment (Northern Ireland)",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/department-of-the-environment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of the Environment (Northern Ireland)\",\"environment\",\"batch:50\"]",
     604800,
     "Department of the Environment (Northern Ireland) — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_the_environment_transport_and_the_regions, "uk-b50-govuk-department-of-the-environment-transport-and-the-regions",
     "Department of the Environment, Transport and the Regions", "Department of the Environment, Transport and the Regions",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/department-of-the-environment-transport-and-the-regions.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of the Environment, Transport and the Regions\",\"environment\",\"batch:50\"]",
     604800,
     "Department of the Environment, Transport and the Regions — environment feed (2 entries at probe)")

RSSX(uk_b50_govuk_environment_and_heritage_service, "uk-b50-govuk-environment-and-heritage-service",
     "Environment and Heritage Service", "Environment and Heritage Service",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/environment-and-heritage-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Environment and Heritage Service\",\"environment\",\"batch:50\"]",
     604800,
     "Environment and Heritage Service — environment feed (2 entries at probe)")

RSSX(uk_b50_govuk_forest_enterprise_england, "uk-b50-govuk-forest-enterprise-england",
     "Forestry England", "Forestry England",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/forest-enterprise-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Forestry England\",\"environment\",\"batch:50\"]",
     21600,
     "Forestry England — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_forestry_commission, "uk-b50-govuk-forestry-commission",
     "UK Forestry Commission", "UK Forestry Commission",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/forestry-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Forestry Commission\",\"environment\",\"batch:50\"]",
     3600,
     "UK Forestry Commission — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_natural_history_museum, "uk-b50-govuk-natural-history-museum",
     "Natural History Museum", "Natural History Museum",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/natural-history-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Natural History Museum\",\"environment\",\"batch:50\"]",
     21600,
     "Natural History Museum — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_natural_resources_institute, "uk-b50-govuk-natural-resources-institute",
     "Natural Resources Institute", "Natural Resources Institute",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/natural-resources-institute.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Natural Resources Institute\",\"environment\",\"batch:50\"]",
     604800,
     "Natural Resources Institute — environment feed (1 entries at probe)")

RSSX(uk_b50_govuk_natural_resources_wales, "uk-b50-govuk-natural-resources-wales",
     "Natural Resources Wales", "Natural Resources Wales",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/natural-resources-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Natural Resources Wales\",\"environment\",\"batch:50\"]",
     21600,
     "Natural Resources Wales — environment feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_environmental_protection, "uk-b50-govuk-office-for-environmental-protection",
     "Office for Environmental Protection", "Office for Environmental Protection",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/office-for-environmental-protection.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Environmental Protection\",\"environment\",\"batch:50\"]",
     21600,
     "Office for Environmental Protection — environment feed (15 entries at probe)")

RSSX(uk_b50_govuk_royal_commission_on_environmental_pollution, "uk-b50-govuk-royal-commission-on-environmental-pollution",
     "Royal Commission on Environmental Pollution", "Royal Commission on Environmental Pollution",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/royal-commission-on-environmental-pollution.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Commission on Environmental Pollution\",\"environment\",\"batch:50\"]",
     604800,
     "Royal Commission on Environmental Pollution — environment feed (5 entries at probe)")

RSSX(uk_b50_govuk_the_food_and_environment_research_agency, "uk-b50-govuk-the-food-and-environment-research-agency",
     "The Food and Environment Research Agency", "The Food and Environment Research Agency",
     "environment", "environment",
     "https://www.gov.uk/government/organisations/the-food-and-environment-research-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Food and Environment Research Agency\",\"environment\",\"batch:50\"]",
     604800,
     "The Food and Environment Research Agency — environment feed (12 entries at probe)")

/* ── finreg ──────────────────────────────────────────────────────────── */

RSSX(uk_b50_govuk_uk_debt_management_office, "uk-b50-govuk-uk-debt-management-office",
     "UK DMO", "UK DMO",
     "finreg", "finance",
     "https://www.gov.uk/government/organisations/uk-debt-management-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK DMO\",\"finance\",\"batch:50\"]",
     21600,
     "UK DMO — finance feed (20 entries at probe)")

/* ── government ──────────────────────────────────────────────────────── */

RSSX(uk_b50_braintree_gov_uk_news, "uk-b50-braintree-gov-uk-news",
     "Braintree", "Braintree",
     "government", "government",
     "https://www.braintree.gov.uk:443/rss/news",
     "en", "[\"gov\",\"country:uk\",\"publisher:Braintree\",\"government\",\"batch:50\"]",
     3600,
     "Braintree — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(uk_b50_caselaw_nationalarchives_gov_uk, "uk-b50-caselaw-nationalarchives-gov-uk",
     "Find Case Law", "Find Case Law",
     "government", "government",
     "https://caselaw.nationalarchives.gov.uk/atom.xml",
     "en", "[\"gov\",\"country:uk\",\"publisher:Find Case Law\",\"government\",\"batch:50\"]",
     3600,
     "Find Case Law — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 50 entries at probe")

RSSX(uk_b50_cso_scot_nhs_uk, "uk-b50-cso-scot-nhs-uk",
     "Chief Scientist Office", "Chief Scientist Office",
     "government", "government",
     "https://www.cso.scot.nhs.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Chief Scientist Office\",\"government\",\"batch:50\"]",
     3600,
     "Chief Scientist Office — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_derbyshiredales_gov_uk_format_feed_type_atom, "uk-b50-derbyshiredales-gov-uk-format-feed-type-atom",
     "Derbyshire Dales — Home - Derbyshire Dales District Council", "Derbyshire Dales — Home - Derbyshire Dales District Council",
     "government", "government",
     "https://www.derbyshiredales.gov.uk/?format=feed&type=atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Derbyshire Dales — Home - Derbyshire Dales District Council\",\"government\",\"batch:50\"]",
     3600,
     "Derbyshire Dales — Home - Derbyshire Dales District Council — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 3 entries at probe")

RSSX(uk_b50_derbyshiredales_gov_uk_format_feed_type_rss, "uk-b50-derbyshiredales-gov-uk-format-feed-type-rss",
     "Derbyshire Dales — Home - Derbyshire Dales District Council", "Derbyshire Dales — Home - Derbyshire Dales District Council",
     "government", "government",
     "https://www.derbyshiredales.gov.uk/?format=feed&type=rss",
     "en", "[\"gov\",\"country:uk\",\"publisher:Derbyshire Dales — Home - Derbyshire Dales District Council\",\"government\",\"batch:50\"]",
     3600,
     "Derbyshire Dales — Home - Derbyshire Dales District Council — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 3 entries at probe")

RSSX(uk_b50_feeds_feedburner_com_uhievents, "uk-b50-feeds-feedburner-com-uhievents",
     "University of the Highlands and Islands — University of the Highlands and Islands Events", "University of the Highlands and Islands — University of the Highlands and Islands Events",
     "government", "government",
     "http://feeds.feedburner.com/uhievents",
     "en", "[\"gov\",\"country:uk\",\"publisher:University of the Highlands and Islands — University of the Highlands and Islands Events\",\"government\",\"batch:50\"]",
     3600,
     "University of the Highlands and Islands — University of the Highlands and Islands Events — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 1 entries at probe")

RSSX(uk_b50_feeds_feedburner_com_uhinews, "uk-b50-feeds-feedburner-com-uhinews",
     "University of the Highlands and Islands — University of the Highlands and Islands Press Releases", "University of the Highlands and Islands — University of the Highlands and Islands Press Releases",
     "government", "government",
     "http://feeds.feedburner.com/uhinews",
     "en", "[\"gov\",\"country:uk\",\"publisher:University of the Highlands and Islands — University of the Highlands and Islands Press Releases\",\"government\",\"batch:50\"]",
     3600,
     "University of the Highlands and Islands — University of the Highlands and Islands Press Releases — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 15 entries at probe")

RSSX(uk_b50_glamorganuniversity_org, "uk-b50-glamorganuniversity-org",
     "University of Glamorgan", "University of Glamorgan",
     "government", "government",
     "https://glamorganuniversity.org/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:University of Glamorgan\",\"government\",\"batch:50\"]",
     3600,
     "University of Glamorgan — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 1 entries at probe")

RSSX(uk_b50_gov_wales_topic_35_latest, "uk-b50-gov-wales-topic-35-latest",
     "Department for Education and Skills — GOV.WALES RSS feed: Education and skills, Latest", "Department for Education and Skills — GOV.WALES RSS feed: Education and skills, Latest",
     "government", "government",
     "https://www.gov.wales/topic/35/latest/rss",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Education and Skills — GOV.WALES RSS feed: Education and skills, Latest\",\"government\",\"batch:50\"]",
     3600,
     "Department for Education and Skills — GOV.WALES RSS feed: Education and skills, Latest — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 100 entries at probe")

RSSX(uk_b50_govuk_academy_for_social_justice, "uk-b50-govuk-academy-for-social-justice",
     "UK Academy For Social Justice", "UK Academy For Social Justice",
     "government", "government",
     "https://www.gov.uk/government/organisations/academy-for-social-justice.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Academy For Social Justice\",\"government\",\"batch:50\"]",
     3600,
     "UK Academy For Social Justice — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_acas, "uk-b50-govuk-acas",
     "Advisory, Conciliation and Arbitration Service", "Advisory, Conciliation and Arbitration Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/acas.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory, Conciliation and Arbitration Service\",\"government\",\"batch:50\"]",
     21600,
     "Advisory, Conciliation and Arbitration Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_accelerated_access_review, "uk-b50-govuk-accelerated-access-review",
     "Accelerated Access Review", "Accelerated Access Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/accelerated-access-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Accelerated Access Review\",\"government\",\"batch:50\"]",
     604800,
     "Accelerated Access Review — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_active_travel_england, "uk-b50-govuk-active-travel-england",
     "Active Travel England", "Active Travel England",
     "government", "government",
     "https://www.gov.uk/government/organisations/active-travel-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Active Travel England\",\"government\",\"batch:50\"]",
     21600,
     "Active Travel England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_administration_of_radioactive_substances_advisory_committee, "uk-b50-govuk-administration-of-radioactive-substances-advisory-committee",
     "Administration of Radioactive Substances Advisory Committee", "Administration of Radioactive Substances Advisory Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/administration-of-radioactive-substances-advisory-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Administration of Radioactive Substances Advisory Committee\",\"government\",\"batch:50\"]",
     3600,
     "Administration of Radioactive Substances Advisory Committee — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_administrative_justice_and_tribunals_council, "uk-b50-govuk-administrative-justice-and-tribunals-council",
     "Administrative Justice and Tribunals Council", "Administrative Justice and Tribunals Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/administrative-justice-and-tribunals-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Administrative Justice and Tribunals Council\",\"government\",\"batch:50\"]",
     604800,
     "Administrative Justice and Tribunals Council — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_administrative_justice_and_tribunals_council_welsh_committee, "uk-b50-govuk-administrative-justice-and-tribunals-council-welsh-committee",
     "Administrative Justice and Tribunals Council Welsh Committee", "Administrative Justice and Tribunals Council Welsh Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/administrative-justice-and-tribunals-council-welsh-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Administrative Justice and Tribunals Council Welsh Committee\",\"government\",\"batch:50\"]",
     604800,
     "Administrative Justice and Tribunals Council Welsh Committee — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_adult_learning_inspectorate, "uk-b50-govuk-adult-learning-inspectorate",
     "Adult Learning Inspectorate", "Adult Learning Inspectorate",
     "government", "government",
     "https://www.gov.uk/government/organisations/adult-learning-inspectorate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Adult Learning Inspectorate\",\"government\",\"batch:50\"]",
     604800,
     "Adult Learning Inspectorate — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_advantage_west_midlands, "uk-b50-govuk-advantage-west-midlands",
     "Advantage West Midlands", "Advantage West Midlands",
     "government", "government",
     "https://www.gov.uk/government/organisations/advantage-west-midlands.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advantage West Midlands\",\"government\",\"batch:50\"]",
     604800,
     "Advantage West Midlands — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_animal_feedingstuffs, "uk-b50-govuk-advisory-committee-on-animal-feedingstuffs",
     "Advisory Committee on Animal Feedingstuffs", "Advisory Committee on Animal Feedingstuffs",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-animal-feedingstuffs.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on Animal Feedingstuffs\",\"government\",\"batch:50\"]",
     3600,
     "Advisory Committee on Animal Feedingstuffs — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_business_appointments, "uk-b50-govuk-advisory-committee-on-business-appointments",
     "UK Advisory Committee On Business Appointments", "UK Advisory Committee On Business Appointments",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-business-appointments.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Advisory Committee On Business Appointments\",\"government\",\"batch:50\"]",
     3600,
     "UK Advisory Committee On Business Appointments — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_clinical_excellence_awards, "uk-b50-govuk-advisory-committee-on-clinical-excellence-awards",
     "Advisory Committee on Clinical Excellence Awards", "Advisory Committee on Clinical Excellence Awards",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-clinical-excellence-awards.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on Clinical Excellence Awards\",\"government\",\"batch:50\"]",
     604800,
     "Advisory Committee on Clinical Excellence Awards — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_clinical_impact_awards, "uk-b50-govuk-advisory-committee-on-clinical-impact-awards",
     "Advisory Committee on Clinical Impact Awards", "Advisory Committee on Clinical Impact Awards",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-clinical-impact-awards.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on Clinical Impact Awards\",\"government\",\"batch:50\"]",
     3600,
     "Advisory Committee on Clinical Impact Awards — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_conscientious_objectors, "uk-b50-govuk-advisory-committee-on-conscientious-objectors",
     "Advisory Committee on Conscientious Objectors", "Advisory Committee on Conscientious Objectors",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-conscientious-objectors.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on Conscientious Objectors\",\"government\",\"batch:50\"]",
     3600,
     "Advisory Committee on Conscientious Objectors — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_novel_foods_and_processes, "uk-b50-govuk-advisory-committee-on-novel-foods-and-processes",
     "Advisory Committee on Novel Foods and Processes", "Advisory Committee on Novel Foods and Processes",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-novel-foods-and-processes.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on Novel Foods and Processes\",\"government\",\"batch:50\"]",
     3600,
     "Advisory Committee on Novel Foods and Processes — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_pesticides, "uk-b50-govuk-advisory-committee-on-pesticides",
     "Advisory Committee on Pesticides", "Advisory Committee on Pesticides",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-pesticides.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on Pesticides\",\"government\",\"batch:50\"]",
     604800,
     "Advisory Committee on Pesticides — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_on_the_microbiological_safety_of_food, "uk-b50-govuk-advisory-committee-on-the-microbiological-safety-of-food",
     "Advisory Committee on the Microbiological Safety of Food", "Advisory Committee on the Microbiological Safety of Food",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-committee-on-the-microbiological-safety-of-food.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee on the Microbiological Safety of Food\",\"government\",\"batch:50\"]",
     3600,
     "Advisory Committee on the Microbiological Safety of Food — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_advisory_council_on_the_misuse_of_drugs, "uk-b50-govuk-advisory-council-on-the-misuse-of-drugs",
     "UK Advisory Council On The Misuse Of Drugs", "UK Advisory Council On The Misuse Of Drugs",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-council-on-the-misuse-of-drugs.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Advisory Council On The Misuse Of Drugs\",\"government\",\"batch:50\"]",
     3600,
     "UK Advisory Council On The Misuse Of Drugs — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_advisory_panel_on_public_sector_information, "uk-b50-govuk-advisory-panel-on-public-sector-information",
     "Advisory Panel on Public Sector Information", "Advisory Panel on Public Sector Information",
     "government", "government",
     "https://www.gov.uk/government/organisations/advisory-panel-on-public-sector-information.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Panel on Public Sector Information\",\"government\",\"batch:50\"]",
     604800,
     "Advisory Panel on Public Sector Information — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_agriculture_and_horticulture_development_board, "uk-b50-govuk-agriculture-and-horticulture-development-board",
     "Agriculture and Horticulture Development Board", "Agriculture and Horticulture Development Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/agriculture-and-horticulture-development-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Agriculture and Horticulture Development Board\",\"government\",\"batch:50\"]",
     21600,
     "Agriculture and Horticulture Development Board — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_ai_safety_institute, "uk-b50-govuk-ai-safety-institute",
     "AI Safety Institute", "AI Safety Institute",
     "government", "government",
     "https://www.gov.uk/government/organisations/ai-safety-institute.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:AI Safety Institute\",\"government\",\"batch:50\"]",
     604800,
     "AI Safety Institute — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_ai_security_institute, "uk-b50-govuk-ai-security-institute",
     "AI Security Institute", "AI Security Institute",
     "government", "government",
     "https://www.gov.uk/government/organisations/ai-security-institute.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:AI Security Institute\",\"government\",\"batch:50\"]",
     21600,
     "AI Security Institute — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_air_accidents_investigation_branch, "uk-b50-govuk-air-accidents-investigation-branch",
     "UK Air Accidents Investigation Branch", "UK Air Accidents Investigation Branch",
     "government", "government",
     "https://www.gov.uk/government/organisations/air-accidents-investigation-branch.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Air Accidents Investigation Branch\",\"government\",\"batch:50\"]",
     3600,
     "UK Air Accidents Investigation Branch — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_airports_commission, "uk-b50-govuk-airports-commission",
     "Airports Commission", "Airports Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/airports-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Airports Commission\",\"government\",\"batch:50\"]",
     604800,
     "Airports Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_ancient_monuments_board_for_scotland, "uk-b50-govuk-ancient-monuments-board-for-scotland",
     "Ancient Monuments Board for Scotland", "Ancient Monuments Board for Scotland",
     "government", "government",
     "https://www.gov.uk/government/organisations/ancient-monuments-board-for-scotland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ancient Monuments Board for Scotland\",\"government\",\"batch:50\"]",
     604800,
     "Ancient Monuments Board for Scotland — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_animal_procedures_committee, "uk-b50-govuk-animal-procedures-committee",
     "Animal Procedures Committee", "Animal Procedures Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/animal-procedures-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Animal Procedures Committee\",\"government\",\"batch:50\"]",
     604800,
     "Animal Procedures Committee — government feed (15 entries at probe)")

RSSX(uk_b50_govuk_appeals_service_agency, "uk-b50-govuk-appeals-service-agency",
     "Appeals Service Agency", "Appeals Service Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/appeals-service-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Appeals Service Agency\",\"government\",\"batch:50\"]",
     604800,
     "Appeals Service Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_architects_registration_board, "uk-b50-govuk-architects-registration-board",
     "Architects Registration Board", "Architects Registration Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/architects-registration-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Architects Registration Board\",\"government\",\"batch:50\"]",
     21600,
     "Architects Registration Board — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_armagh_observatory_and_planetarium, "uk-b50-govuk-armagh-observatory-and-planetarium",
     "Armagh Observatory and Planetarium", "Armagh Observatory and Planetarium",
     "government", "government",
     "https://www.gov.uk/government/organisations/armagh-observatory-and-planetarium.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Armagh Observatory and Planetarium\",\"government\",\"batch:50\"]",
     604800,
     "Armagh Observatory and Planetarium — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_armed_forces_pay_review_body, "uk-b50-govuk-armed-forces-pay-review-body",
     "Armed Forces' Pay Review Body", "Armed Forces' Pay Review Body",
     "government", "government",
     "https://www.gov.uk/government/organisations/armed-forces-pay-review-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Armed Forces' Pay Review Body\",\"government\",\"batch:50\"]",
     3600,
     "Armed Forces' Pay Review Body — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_armed_forces_personnel_administration_agency, "uk-b50-govuk-armed-forces-personnel-administration-agency",
     "Armed Forces Personnel Administration Agency", "Armed Forces Personnel Administration Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/armed-forces-personnel-administration-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Armed Forces Personnel Administration Agency\",\"government\",\"batch:50\"]",
     604800,
     "Armed Forces Personnel Administration Agency — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_arts_council_england, "uk-b50-govuk-arts-council-england",
     "UK Arts Council", "UK Arts Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/arts-council-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Arts Council\",\"government\",\"batch:50\"]",
     21600,
     "UK Arts Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_arts_council_of_northern_ireland, "uk-b50-govuk-arts-council-of-northern-ireland",
     "Arts Council of Northern Ireland", "Arts Council of Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/arts-council-of-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Arts Council of Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Arts Council of Northern Ireland — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_arts_council_of_wales, "uk-b50-govuk-arts-council-of-wales",
     "Arts Council of Wales", "Arts Council of Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/arts-council-of-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Arts Council of Wales\",\"government\",\"batch:50\"]",
     21600,
     "Arts Council of Wales — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_ashington_education_action_zone, "uk-b50-govuk-ashington-education-action-zone",
     "Ashington Education Action Zone", "Ashington Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/ashington-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ashington Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Ashington Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_asset_protection_agency, "uk-b50-govuk-asset-protection-agency",
     "Asset Protection Agency", "Asset Protection Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/asset-protection-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Asset Protection Agency\",\"government\",\"batch:50\"]",
     604800,
     "Asset Protection Agency — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_assets_recovery_agency, "uk-b50-govuk-assets-recovery-agency",
     "Assets Recovery Agency", "Assets Recovery Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/assets-recovery-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Assets Recovery Agency\",\"government\",\"batch:50\"]",
     604800,
     "Assets Recovery Agency — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_atomic_weapons_establishment, "uk-b50-govuk-atomic-weapons-establishment",
     "Atomic Weapons Establishment", "Atomic Weapons Establishment",
     "government", "government",
     "https://www.gov.uk/government/organisations/atomic-weapons-establishment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Atomic Weapons Establishment\",\"government\",\"batch:50\"]",
     604800,
     "Atomic Weapons Establishment — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_audit_commission, "uk-b50-govuk-audit-commission",
     "Audit Commission", "Audit Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/audit-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Audit Commission\",\"government\",\"batch:50\"]",
     604800,
     "Audit Commission — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_azelle_rodney_inquiry, "uk-b50-govuk-azelle-rodney-inquiry",
     "Azelle Rodney Inquiry", "Azelle Rodney Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/azelle-rodney-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Azelle Rodney Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Azelle Rodney Inquiry — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_baha_mousa_inquiry, "uk-b50-govuk-baha-mousa-inquiry",
     "Baha Mousa Inquiry", "Baha Mousa Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/baha-mousa-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Baha Mousa Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Baha Mousa Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_bank_of_england, "uk-b50-govuk-bank-of-england",
     "Bank of England", "Bank of England",
     "government", "government",
     "https://www.gov.uk/government/organisations/bank-of-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Bank of England\",\"government\",\"batch:50\"]",
     21600,
     "Bank of England — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_barker_review_of_land_use_planning, "uk-b50-govuk-barker-review-of-land-use-planning",
     "Barker Review of Land Use Planning", "Barker Review of Land Use Planning",
     "government", "government",
     "https://www.gov.uk/government/organisations/barker-review-of-land-use-planning.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Barker Review of Land Use Planning\",\"government\",\"batch:50\"]",
     604800,
     "Barker Review of Land Use Planning — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_barrow_education_action_zone, "uk-b50-govuk-barrow-education-action-zone",
     "Barrow Education Action Zone", "Barrow Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/barrow-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Barrow Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Barrow Education Action Zone — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_bbc, "uk-b50-govuk-bbc",
     "BBC", "BBC",
     "government", "government",
     "https://www.gov.uk/government/organisations/bbc.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:BBC\",\"government\",\"batch:50\"]",
     21600,
     "BBC — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_bbc_world_service, "uk-b50-govuk-bbc-world-service",
     "BBC World Service", "BBC World Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/bbc-world-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:BBC World Service\",\"government\",\"batch:50\"]",
     21600,
     "BBC World Service — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_bedford_education_action_zone, "uk-b50-govuk-bedford-education-action-zone",
     "Bedford Education Action Zone", "Bedford Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/bedford-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Bedford Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Bedford Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_behavioural_insights_team, "uk-b50-govuk-behavioural-insights-team",
     "Behavioural Insights Team", "Behavioural Insights Team",
     "government", "government",
     "https://www.gov.uk/government/organisations/behavioural-insights-team.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Behavioural Insights Team\",\"government\",\"batch:50\"]",
     604800,
     "Behavioural Insights Team — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_better_regulation_delivery_office, "uk-b50-govuk-better-regulation-delivery-office",
     "Better Regulation Delivery Office", "Better Regulation Delivery Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/better-regulation-delivery-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Better Regulation Delivery Office\",\"government\",\"batch:50\"]",
     604800,
     "Better Regulation Delivery Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_big_lottery_fund, "uk-b50-govuk-big-lottery-fund",
     "Big Lottery Fund", "Big Lottery Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/big-lottery-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Big Lottery Fund\",\"government\",\"batch:50\"]",
     604800,
     "Big Lottery Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_billy_wright_inquiry, "uk-b50-govuk-billy-wright-inquiry",
     "Billy Wright Inquiry", "Billy Wright Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/billy-wright-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Billy Wright Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Billy Wright Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_biometrics_and_forensics_ethics_group, "uk-b50-govuk-biometrics-and-forensics-ethics-group",
     "Biometrics and Forensics Ethics Group", "Biometrics and Forensics Ethics Group",
     "government", "government",
     "https://www.gov.uk/government/organisations/biometrics-and-forensics-ethics-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Biometrics and Forensics Ethics Group\",\"government\",\"batch:50\"]",
     604800,
     "Biometrics and Forensics Ethics Group — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_biometrics_and_surveillance_camera_commissioner, "uk-b50-govuk-biometrics-and-surveillance-camera-commissioner",
     "Biometrics and Surveillance Camera Commissioner", "Biometrics and Surveillance Camera Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/biometrics-and-surveillance-camera-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Biometrics and Surveillance Camera Commissioner\",\"government\",\"batch:50\"]",
     3600,
     "Biometrics and Surveillance Camera Commissioner — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_biometrics_commissioner, "uk-b50-govuk-biometrics-commissioner",
     "Biometrics Commissioner", "Biometrics Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/biometrics-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Biometrics Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Biometrics Commissioner — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_bloody_sunday_inquiry, "uk-b50-govuk-bloody-sunday-inquiry",
     "Bloody Sunday Inquiry", "Bloody Sunday Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/bloody-sunday-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Bloody Sunday Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Bloody Sunday Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_board_of_banking_supervision, "uk-b50-govuk-board-of-banking-supervision",
     "Board of Banking Supervision", "Board of Banking Supervision",
     "government", "government",
     "https://www.gov.uk/government/organisations/board-of-banking-supervision.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Board of Banking Supervision\",\"government\",\"batch:50\"]",
     604800,
     "Board of Banking Supervision — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_board_of_inland_revenue, "uk-b50-govuk-board-of-inland-revenue",
     "Board of Inland Revenue", "Board of Inland Revenue",
     "government", "government",
     "https://www.gov.uk/government/organisations/board-of-inland-revenue.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Board of Inland Revenue\",\"government\",\"batch:50\"]",
     604800,
     "Board of Inland Revenue — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_board_of_trustees_of_the_royal_botanic_gardens_kew, "uk-b50-govuk-board-of-trustees-of-the-royal-botanic-gardens-kew",
     "Board of Trustees of the Royal Botanic Gardens Kew", "Board of Trustees of the Royal Botanic Gardens Kew",
     "government", "government",
     "https://www.gov.uk/government/organisations/board-of-trustees-of-the-royal-botanic-gardens-kew.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Board of Trustees of the Royal Botanic Gardens Kew\",\"government\",\"batch:50\"]",
     21600,
     "Board of Trustees of the Royal Botanic Gardens Kew — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_bolton_education_action_zone, "uk-b50-govuk-bolton-education-action-zone",
     "Bolton Education Action Zone", "Bolton Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/bolton-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Bolton Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Bolton Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_bona_vacantia, "uk-b50-govuk-bona-vacantia",
     "Bona Vacantia", "Bona Vacantia",
     "government", "government",
     "https://www.gov.uk/government/organisations/bona-vacantia.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Bona Vacantia\",\"government\",\"batch:50\"]",
     3600,
     "Bona Vacantia — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_border_security_command, "uk-b50-govuk-border-security-command",
     "Border Security Command", "Border Security Command",
     "government", "government",
     "https://www.gov.uk/government/organisations/border-security-command.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Border Security Command\",\"government\",\"batch:50\"]",
     3600,
     "Border Security Command — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_boundary_commission_for_england, "uk-b50-govuk-boundary-commission-for-england",
     "Boundary Commission for England", "Boundary Commission for England",
     "government", "government",
     "https://www.gov.uk/government/organisations/boundary-commission-for-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Boundary Commission for England\",\"government\",\"batch:50\"]",
     21600,
     "Boundary Commission for England — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_boundary_commission_for_northern_ireland, "uk-b50-govuk-boundary-commission-for-northern-ireland",
     "Boundary Commission for Northern Ireland", "Boundary Commission for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/boundary-commission-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Boundary Commission for Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Boundary Commission for Northern Ireland — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_boundary_commission_for_scotland, "uk-b50-govuk-boundary-commission-for-scotland",
     "Boundary Commission for Scotland", "Boundary Commission for Scotland",
     "government", "government",
     "https://www.gov.uk/government/organisations/boundary-commission-for-scotland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Boundary Commission for Scotland\",\"government\",\"batch:50\"]",
     21600,
     "Boundary Commission for Scotland — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_boundary_commission_for_wales, "uk-b50-govuk-boundary-commission-for-wales",
     "Boundary Commission for Wales", "Boundary Commission for Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/boundary-commission-for-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Boundary Commission for Wales\",\"government\",\"batch:50\"]",
     21600,
     "Boundary Commission for Wales — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_bpdts_ltd, "uk-b50-govuk-bpdts-ltd",
     "BPDTS Ltd", "BPDTS Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/bpdts-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:BPDTS Ltd\",\"government\",\"batch:50\"]",
     604800,
     "BPDTS Ltd — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_brb_residuary_ltd, "uk-b50-govuk-brb-residuary-ltd",
     "BRB (Residuary) Ltd", "BRB (Residuary) Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/brb-residuary-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:BRB (Residuary) Ltd\",\"government\",\"batch:50\"]",
     604800,
     "BRB (Residuary) Ltd — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_bridgwater_education_action_zone, "uk-b50-govuk-bridgwater-education-action-zone",
     "Bridgwater Education Action Zone", "Bridgwater Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/bridgwater-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Bridgwater Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Bridgwater Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_bristol_education_action_zone, "uk-b50-govuk-bristol-education-action-zone",
     "Bristol Education Action Zone", "Bristol Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/bristol-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Bristol Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Bristol Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_british_business_bank, "uk-b50-govuk-british-business-bank",
     "British Business Bank", "British Business Bank",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-business-bank.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Business Bank\",\"government\",\"batch:50\"]",
     21600,
     "British Business Bank — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_british_cattle_movement_service, "uk-b50-govuk-british-cattle-movement-service",
     "British Cattle Movement Service", "British Cattle Movement Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-cattle-movement-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Cattle Movement Service\",\"government\",\"batch:50\"]",
     3600,
     "British Cattle Movement Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_british_council, "uk-b50-govuk-british-council",
     "UK British Council", "UK British Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK British Council\",\"government\",\"batch:50\"]",
     21600,
     "UK British Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_british_educational_communications_and_technology_agency, "uk-b50-govuk-british-educational-communications-and-technology-agency",
     "British Educational Communications and Technology Agency", "British Educational Communications and Technology Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-educational-communications-and-technology-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Educational Communications and Technology Agency\",\"government\",\"batch:50\"]",
     604800,
     "British Educational Communications and Technology Agency — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_british_film_institute, "uk-b50-govuk-british-film-institute",
     "British Film Institute", "British Film Institute",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-film-institute.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Film Institute\",\"government\",\"batch:50\"]",
     21600,
     "British Film Institute — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_british_forces_post_office, "uk-b50-govuk-british-forces-post-office",
     "British Forces Post Office", "British Forces Post Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-forces-post-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Forces Post Office\",\"government\",\"batch:50\"]",
     604800,
     "British Forces Post Office — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_british_hallmarking_council, "uk-b50-govuk-british-hallmarking-council",
     "British Hallmarking Council", "British Hallmarking Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-hallmarking-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Hallmarking Council\",\"government\",\"batch:50\"]",
     3600,
     "British Hallmarking Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_british_library, "uk-b50-govuk-british-library",
     "British Library", "British Library",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-library.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Library\",\"government\",\"batch:50\"]",
     21600,
     "British Library — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_british_museum, "uk-b50-govuk-british-museum",
     "British Museum", "British Museum",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Museum\",\"government\",\"batch:50\"]",
     21600,
     "British Museum — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_british_pharmacopoeia, "uk-b50-govuk-british-pharmacopoeia",
     "British Pharmacopoeia Commission", "British Pharmacopoeia Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-pharmacopoeia.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Pharmacopoeia Commission\",\"government\",\"batch:50\"]",
     21600,
     "British Pharmacopoeia Commission — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_british_potato_council, "uk-b50-govuk-british-potato-council",
     "British Potato Council", "British Potato Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-potato-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Potato Council\",\"government\",\"batch:50\"]",
     604800,
     "British Potato Council — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_british_steel_limited, "uk-b50-govuk-british-steel-limited",
     "British Steel Limited", "British Steel Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-steel-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Steel Limited\",\"government\",\"batch:50\"]",
     21600,
     "British Steel Limited — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_british_wool, "uk-b50-govuk-british-wool",
     "British Wool", "British Wool",
     "government", "government",
     "https://www.gov.uk/government/organisations/british-wool.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:British Wool\",\"government\",\"batch:50\"]",
     21600,
     "British Wool — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_broadcasting_standards_commission, "uk-b50-govuk-broadcasting-standards-commission",
     "Broadcasting Standards Commission", "Broadcasting Standards Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/broadcasting-standards-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Broadcasting Standards Commission\",\"government\",\"batch:50\"]",
     604800,
     "Broadcasting Standards Commission — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_broads_authority, "uk-b50-govuk-broads-authority",
     "Broads Authority", "Broads Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/broads-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Broads Authority\",\"government\",\"batch:50\"]",
     21600,
     "Broads Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_building_control_independent_panel, "uk-b50-govuk-building-control-independent-panel",
     "Building Control Independent Panel", "Building Control Independent Panel",
     "government", "government",
     "https://www.gov.uk/government/organisations/building-control-independent-panel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Building Control Independent Panel\",\"government\",\"batch:50\"]",
     3600,
     "Building Control Independent Panel — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_building_digital_uk, "uk-b50-govuk-building-digital-uk",
     "UK Building Digital Uk", "UK Building Digital Uk",
     "government", "government",
     "https://www.gov.uk/government/organisations/building-digital-uk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Building Digital Uk\",\"government\",\"batch:50\"]",
     3600,
     "UK Building Digital Uk — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_building_regulations_advisory_committee, "uk-b50-govuk-building-regulations-advisory-committee",
     "Building Regulations Advisory Committee", "Building Regulations Advisory Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/building-regulations-advisory-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Building Regulations Advisory Committee\",\"government\",\"batch:50\"]",
     604800,
     "Building Regulations Advisory Committee — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_building_safety_regulator, "uk-b50-govuk-building-safety-regulator",
     "Building Safety Regulator", "Building Safety Regulator",
     "government", "government",
     "https://www.gov.uk/government/organisations/building-safety-regulator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Building Safety Regulator\",\"government\",\"batch:50\"]",
     3600,
     "Building Safety Regulator — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_business_development_service, "uk-b50-govuk-business-development-service",
     "Business Development Service", "Business Development Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/business-development-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Business Development Service\",\"government\",\"batch:50\"]",
     604800,
     "Business Development Service — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_buying_solutions, "uk-b50-govuk-buying-solutions",
     "Buying Solutions", "Buying Solutions",
     "government", "government",
     "https://www.gov.uk/government/organisations/buying-solutions.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Buying Solutions\",\"government\",\"batch:50\"]",
     604800,
     "Buying Solutions — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_camborne_pool_and_redruth_education_action_zone, "uk-b50-govuk-camborne-pool-and-redruth-education-action-zone",
     "Camborne, Pool and Redruth Education Action Zone", "Camborne, Pool and Redruth Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/camborne-pool-and-redruth-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Camborne, Pool and Redruth Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Camborne, Pool and Redruth Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_capacitybuilders, "uk-b50-govuk-capacitybuilders",
     "Capacitybuilders", "Capacitybuilders",
     "government", "government",
     "https://www.gov.uk/government/organisations/capacitybuilders.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Capacitybuilders\",\"government\",\"batch:50\"]",
     604800,
     "Capacitybuilders — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_care_quality_commission, "uk-b50-govuk-care-quality-commission",
     "UK Care Quality Commission", "UK Care Quality Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/care-quality-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Care Quality Commission\",\"government\",\"batch:50\"]",
     21600,
     "UK Care Quality Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_careers_wales, "uk-b50-govuk-careers-wales",
     "Careers Wales", "Careers Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/careers-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Careers Wales\",\"government\",\"batch:50\"]",
     21600,
     "Careers Wales — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_central_advisory_committee_on_pensions_and_compensation, "uk-b50-govuk-central-advisory-committee-on-pensions-and-compensation",
     "Central Advisory Committee on Pensions and Compensation", "Central Advisory Committee on Pensions and Compensation",
     "government", "government",
     "https://www.gov.uk/government/organisations/central-advisory-committee-on-pensions-and-compensation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Central Advisory Committee on Pensions and Compensation\",\"government\",\"batch:50\"]",
     604800,
     "Central Advisory Committee on Pensions and Compensation — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_central_arbitration_committee, "uk-b50-govuk-central-arbitration-committee",
     "Central Arbitration Committee", "Central Arbitration Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/central-arbitration-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Central Arbitration Committee\",\"government\",\"batch:50\"]",
     3600,
     "Central Arbitration Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_central_office_of_information, "uk-b50-govuk-central-office-of-information",
     "Central Office of Information", "Central Office of Information",
     "government", "government",
     "https://www.gov.uk/government/organisations/central-office-of-information.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Central Office of Information\",\"government\",\"batch:50\"]",
     604800,
     "Central Office of Information — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_centre_for_the_protection_of_national_infrastructure, "uk-b50-govuk-centre-for-the-protection-of-national-infrastructure",
     "Centre for the Protection of National Infrastructure", "Centre for the Protection of National Infrastructure",
     "government", "government",
     "https://www.gov.uk/government/organisations/centre-for-the-protection-of-national-infrastructure.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Centre for the Protection of National Infrastructure\",\"government\",\"batch:50\"]",
     604800,
     "Centre for the Protection of National Infrastructure — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_certification_officer, "uk-b50-govuk-certification-officer",
     "Certification Officer", "Certification Officer",
     "government", "government",
     "https://www.gov.uk/government/organisations/certification-officer.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Certification Officer\",\"government\",\"batch:50\"]",
     3600,
     "Certification Officer — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_cesg, "uk-b50-govuk-cesg",
     "CESG", "CESG",
     "government", "government",
     "https://www.gov.uk/government/organisations/cesg.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:CESG\",\"government\",\"batch:50\"]",
     604800,
     "CESG — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_channel_4, "uk-b50-govuk-channel-4",
     "Channel 4", "Channel 4",
     "government", "government",
     "https://www.gov.uk/government/organisations/channel-4.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Channel 4\",\"government\",\"batch:50\"]",
     21600,
     "Channel 4 — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_chevening_foundation, "uk-b50-govuk-chevening-foundation",
     "Chevening Scholarship Programme", "Chevening Scholarship Programme",
     "government", "government",
     "https://www.gov.uk/government/organisations/chevening-foundation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Chevening Scholarship Programme\",\"government\",\"batch:50\"]",
     21600,
     "Chevening Scholarship Programme — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_chief_fire_and_rescue_adviser_unit, "uk-b50-govuk-chief-fire-and-rescue-adviser-unit",
     "Chief Fire and Rescue Adviser", "Chief Fire and Rescue Adviser",
     "government", "government",
     "https://www.gov.uk/government/organisations/chief-fire-and-rescue-adviser-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Chief Fire and Rescue Adviser\",\"government\",\"batch:50\"]",
     604800,
     "Chief Fire and Rescue Adviser — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_child_maintenance_and_enforcement_commission, "uk-b50-govuk-child-maintenance-and-enforcement-commission",
     "Child Maintenance and Enforcement Commission", "Child Maintenance and Enforcement Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/child-maintenance-and-enforcement-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Child Maintenance and Enforcement Commission\",\"government\",\"batch:50\"]",
     604800,
     "Child Maintenance and Enforcement Commission — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_child_safeguarding_practice_review_panel, "uk-b50-govuk-child-safeguarding-practice-review-panel",
     "Child Safeguarding Practice Review Panel", "Child Safeguarding Practice Review Panel",
     "government", "government",
     "https://www.gov.uk/government/organisations/child-safeguarding-practice-review-panel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Child Safeguarding Practice Review Panel\",\"government\",\"batch:50\"]",
     21600,
     "Child Safeguarding Practice Review Panel — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_children_and_family_court_advisory_and_support_service, "uk-b50-govuk-children-and-family-court-advisory-and-support-service",
     "Cafcass", "Cafcass",
     "government", "government",
     "https://www.gov.uk/government/organisations/children-and-family-court-advisory-and-support-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Cafcass\",\"government\",\"batch:50\"]",
     21600,
     "Cafcass — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_childrens_workforce_development_council, "uk-b50-govuk-childrens-workforce-development-council",
     "Children's Workforce Development Council", "Children's Workforce Development Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/childrens-workforce-development-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Children's Workforce Development Council\",\"government\",\"batch:50\"]",
     604800,
     "Children's Workforce Development Council — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_churches_conservation_trust, "uk-b50-govuk-churches-conservation-trust",
     "Churches Conservation Trust", "Churches Conservation Trust",
     "government", "government",
     "https://www.gov.uk/government/organisations/churches-conservation-trust.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Churches Conservation Trust\",\"government\",\"batch:50\"]",
     21600,
     "Churches Conservation Trust — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_civil_justice_council, "uk-b50-govuk-civil-justice-council",
     "Civil Justice Council", "Civil Justice Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-justice-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Civil Justice Council\",\"government\",\"batch:50\"]",
     21600,
     "Civil Justice Council — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_civil_nuclear_constabulary, "uk-b50-govuk-civil-nuclear-constabulary",
     "UK Civil Nuclear Constabulary", "UK Civil Nuclear Constabulary",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-nuclear-constabulary.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Civil Nuclear Constabulary\",\"government\",\"batch:50\"]",
     3600,
     "UK Civil Nuclear Constabulary — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_civil_procedure_rules_committee, "uk-b50-govuk-civil-procedure-rules-committee",
     "Civil Procedure Rule Committee", "Civil Procedure Rule Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-procedure-rules-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Civil Procedure Rule Committee\",\"government\",\"batch:50\"]",
     3600,
     "Civil Procedure Rule Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_civil_service, "uk-b50-govuk-civil-service",
     "Civil Service", "Civil Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Civil Service\",\"government\",\"batch:50\"]",
     3600,
     "Civil Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_civil_service_commission, "uk-b50-govuk-civil-service-commission",
     "UK Civil Service Commission", "UK Civil Service Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Civil Service Commission\",\"government\",\"batch:50\"]",
     21600,
     "UK Civil Service Commission — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_civil_service_fast_stream, "uk-b50-govuk-civil-service-fast-stream",
     "Civil Service Fast Stream", "Civil Service Fast Stream",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-fast-stream.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Civil Service Fast Stream\",\"government\",\"batch:50\"]",
     21600,
     "Civil Service Fast Stream — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_civil_service_fast_track_apprenticeship, "uk-b50-govuk-civil-service-fast-track-apprenticeship",
     "Civil Service Fast Track Apprenticeship", "Civil Service Fast Track Apprenticeship",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-fast-track-apprenticeship.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Civil Service Fast Track Apprenticeship\",\"government\",\"batch:50\"]",
     604800,
     "Civil Service Fast Track Apprenticeship — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_communication_service, "uk-b50-govuk-civil-service-government-communication-service",
     "Government Communication Service", "Government Communication Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-government-communication-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Communication Service\",\"government\",\"batch:50\"]",
     3600,
     "Government Communication Service — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_legal_profession, "uk-b50-govuk-civil-service-government-legal-profession",
     "Government Legal Profession", "Government Legal Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-government-legal-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Legal Profession\",\"government\",\"batch:50\"]",
     3600,
     "Government Legal Profession — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_legal_service, "uk-b50-govuk-civil-service-government-legal-service",
     "Government Legal Service", "Government Legal Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-government-legal-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Legal Service\",\"government\",\"batch:50\"]",
     604800,
     "Government Legal Service — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_property_profession, "uk-b50-govuk-civil-service-government-property-profession",
     "Government Property Profession", "Government Property Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-government-property-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Property Profession\",\"government\",\"batch:50\"]",
     604800,
     "Government Property Profession — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_statistical_service, "uk-b50-govuk-civil-service-government-statistical-service",
     "Government Statistical Service", "Government Statistical Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-government-statistical-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Statistical Service\",\"government\",\"batch:50\"]",
     21600,
     "Government Statistical Service — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_tax_profession, "uk-b50-govuk-civil-service-government-tax-profession",
     "Government Tax Profession", "Government Tax Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-government-tax-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Tax Profession\",\"government\",\"batch:50\"]",
     3600,
     "Government Tax Profession — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_veterinary_services, "uk-b50-govuk-civil-service-government-veterinary-services",
     "Government Veterinary Services", "Government Veterinary Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-government-veterinary-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Veterinary Services\",\"government\",\"batch:50\"]",
     3600,
     "Government Veterinary Services — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_civil_service_internal_audit_profession, "uk-b50-govuk-civil-service-internal-audit-profession",
     "Internal Audit Profession", "Internal Audit Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-internal-audit-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Internal Audit Profession\",\"government\",\"batch:50\"]",
     3600,
     "Internal Audit Profession — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_civil_service_operational_delivery_profession, "uk-b50-govuk-civil-service-operational-delivery-profession",
     "Operational Delivery Profession", "Operational Delivery Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-operational-delivery-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Operational Delivery Profession\",\"government\",\"batch:50\"]",
     3600,
     "Operational Delivery Profession — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_civil_service_policy_profession, "uk-b50-govuk-civil-service-policy-profession",
     "Policy Profession", "Policy Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-policy-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Policy Profession\",\"government\",\"batch:50\"]",
     3600,
     "Policy Profession — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_civil_service_reform, "uk-b50-govuk-civil-service-reform",
     "Civil Service Reform", "Civil Service Reform",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-reform.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Civil Service Reform\",\"government\",\"batch:50\"]",
     604800,
     "Civil Service Reform — government feed (14 entries at probe)")

RSSX(uk_b50_govuk_civil_service_resourcing, "uk-b50-govuk-civil-service-resourcing",
     "Civil Service Resourcing", "Civil Service Resourcing",
     "government", "government",
     "https://www.gov.uk/government/organisations/civil-service-resourcing.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Civil Service Resourcing\",\"government\",\"batch:50\"]",
     604800,
     "Civil Service Resourcing — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_clacton_and_harwich_education_action_zone, "uk-b50-govuk-clacton-and-harwich-education-action-zone",
     "Clacton and Harwich Education Action Zone", "Clacton and Harwich Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/clacton-and-harwich-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Clacton and Harwich Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Clacton and Harwich Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_co2sense, "uk-b50-govuk-co2sense",
     "CO2Sense", "CO2Sense",
     "government", "government",
     "https://www.gov.uk/government/organisations/co2sense.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:CO2Sense\",\"government\",\"batch:50\"]",
     604800,
     "CO2Sense — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_commission_for_countering_extremism, "uk-b50-govuk-commission-for-countering-extremism",
     "Commission for Countering Extremism", "Commission for Countering Extremism",
     "government", "government",
     "https://www.gov.uk/government/organisations/commission-for-countering-extremism.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for Countering Extremism\",\"government\",\"batch:50\"]",
     3600,
     "Commission for Countering Extremism — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_commission_for_racial_equality, "uk-b50-govuk-commission-for-racial-equality",
     "Commission for Racial Equality", "Commission for Racial Equality",
     "government", "government",
     "https://www.gov.uk/government/organisations/commission-for-racial-equality.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for Racial Equality\",\"government\",\"batch:50\"]",
     604800,
     "Commission for Racial Equality — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_commission_for_rural_communities, "uk-b50-govuk-commission-for-rural-communities",
     "Commission for Rural Communities", "Commission for Rural Communities",
     "government", "government",
     "https://www.gov.uk/government/organisations/commission-for-rural-communities.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for Rural Communities\",\"government\",\"batch:50\"]",
     604800,
     "Commission for Rural Communities — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_commission_for_social_care_inspection, "uk-b50-govuk-commission-for-social-care-inspection",
     "Commission for Social Care Inspection", "Commission for Social Care Inspection",
     "government", "government",
     "https://www.gov.uk/government/organisations/commission-for-social-care-inspection.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for Social Care Inspection\",\"government\",\"batch:50\"]",
     604800,
     "Commission for Social Care Inspection — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_commission_for_the_compact, "uk-b50-govuk-commission-for-the-compact",
     "Commission for the Compact", "Commission for the Compact",
     "government", "government",
     "https://www.gov.uk/government/organisations/commission-for-the-compact.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for the Compact\",\"government\",\"batch:50\"]",
     604800,
     "Commission for the Compact — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_commission_on_race_and_ethnic_disparities, "uk-b50-govuk-commission-on-race-and-ethnic-disparities",
     "UK Commission On Race And Ethnic Disparities", "UK Commission On Race And Ethnic Disparities",
     "government", "government",
     "https://www.gov.uk/government/organisations/commission-on-race-and-ethnic-disparities.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Commission On Race And Ethnic Disparities\",\"government\",\"batch:50\"]",
     604800,
     "UK Commission On Race And Ethnic Disparities — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_commissioner_for_public_appointments, "uk-b50-govuk-commissioner-for-public-appointments",
     "Commissioner for Public Appointments", "Commissioner for Public Appointments",
     "government", "government",
     "https://www.gov.uk/government/organisations/commissioner-for-public-appointments.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commissioner for Public Appointments\",\"government\",\"batch:50\"]",
     21600,
     "Commissioner for Public Appointments — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_committee_on_fuel_poverty, "uk-b50-govuk-committee-on-fuel-poverty",
     "Committee on Fuel Poverty", "Committee on Fuel Poverty",
     "government", "government",
     "https://www.gov.uk/government/organisations/committee-on-fuel-poverty.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Committee on Fuel Poverty\",\"government\",\"batch:50\"]",
     3600,
     "Committee on Fuel Poverty — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_committee_on_radioactive_waste_management, "uk-b50-govuk-committee-on-radioactive-waste-management",
     "Committee on Radioactive Waste Management", "Committee on Radioactive Waste Management",
     "government", "government",
     "https://www.gov.uk/government/organisations/committee-on-radioactive-waste-management.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Committee on Radioactive Waste Management\",\"government\",\"batch:50\"]",
     3600,
     "Committee on Radioactive Waste Management — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_commonwealth_scholarship_commission_in_the_uk, "uk-b50-govuk-commonwealth-scholarship-commission-in-the-uk",
     "Commonwealth Scholarship Commission in the UK", "Commonwealth Scholarship Commission in the UK",
     "government", "government",
     "https://www.gov.uk/government/organisations/commonwealth-scholarship-commission-in-the-uk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commonwealth Scholarship Commission in the UK\",\"government\",\"batch:50\"]",
     3600,
     "Commonwealth Scholarship Commission in the UK — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_community_development_foundation, "uk-b50-govuk-community-development-foundation",
     "Community Development Foundation", "Community Development Foundation",
     "government", "government",
     "https://www.gov.uk/government/organisations/community-development-foundation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Community Development Foundation\",\"government\",\"batch:50\"]",
     604800,
     "Community Development Foundation — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_community_fund, "uk-b50-govuk-community-fund",
     "Community Fund", "Community Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/community-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Community Fund\",\"government\",\"batch:50\"]",
     604800,
     "Community Fund — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_company_names_tribunal, "uk-b50-govuk-company-names-tribunal",
     "Company Names Tribunal", "Company Names Tribunal",
     "government", "government",
     "https://www.gov.uk/government/organisations/company-names-tribunal.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Company Names Tribunal\",\"government\",\"batch:50\"]",
     3600,
     "Company Names Tribunal — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_compensation_agency, "uk-b50-govuk-compensation-agency",
     "Compensation Agency", "Compensation Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/compensation-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Compensation Agency\",\"government\",\"batch:50\"]",
     604800,
     "Compensation Agency — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_competition_appeal_tribunal, "uk-b50-govuk-competition-appeal-tribunal",
     "Competition Appeal Tribunal", "Competition Appeal Tribunal",
     "government", "government",
     "https://www.gov.uk/government/organisations/competition-appeal-tribunal.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Competition Appeal Tribunal\",\"government\",\"batch:50\"]",
     21600,
     "Competition Appeal Tribunal — government feed (14 entries at probe)")

RSSX(uk_b50_govuk_competition_commission, "uk-b50-govuk-competition-commission",
     "Competition Commission", "Competition Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/competition-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Competition Commission\",\"government\",\"batch:50\"]",
     604800,
     "Competition Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_competition_service, "uk-b50-govuk-competition-service",
     "Competition Service", "Competition Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/competition-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Competition Service\",\"government\",\"batch:50\"]",
     21600,
     "Competition Service — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_conflict_stability_and_security_fund, "uk-b50-govuk-conflict-stability-and-security-fund",
     "Conflict, Stability and Security Fund", "Conflict, Stability and Security Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/conflict-stability-and-security-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Conflict, Stability and Security Fund\",\"government\",\"batch:50\"]",
     604800,
     "Conflict, Stability and Security Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_construction_industry_training_board, "uk-b50-govuk-construction-industry-training-board",
     "Construction Industry Training Board", "Construction Industry Training Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/construction-industry-training-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Construction Industry Training Board\",\"government\",\"batch:50\"]",
     21600,
     "Construction Industry Training Board — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_consumer_council_for_postal_services, "uk-b50-govuk-consumer-council-for-postal-services",
     "Consumer Council for Postal Services", "Consumer Council for Postal Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/consumer-council-for-postal-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Consumer Council for Postal Services\",\"government\",\"batch:50\"]",
     604800,
     "Consumer Council for Postal Services — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_consumer_council_for_water, "uk-b50-govuk-consumer-council-for-water",
     "Consumer Council for Water", "Consumer Council for Water",
     "government", "government",
     "https://www.gov.uk/government/organisations/consumer-council-for-water.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Consumer Council for Water\",\"government\",\"batch:50\"]",
     21600,
     "Consumer Council for Water — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_consumer_focus, "uk-b50-govuk-consumer-focus",
     "Consumer Futures", "Consumer Futures",
     "government", "government",
     "https://www.gov.uk/government/organisations/consumer-focus.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Consumer Futures\",\"government\",\"batch:50\"]",
     604800,
     "Consumer Futures — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_cooksey_review, "uk-b50-govuk-cooksey-review",
     "Cooksey Review", "Cooksey Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/cooksey-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Cooksey Review\",\"government\",\"batch:50\"]",
     604800,
     "Cooksey Review — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_copyright_tribunal, "uk-b50-govuk-copyright-tribunal",
     "Copyright Tribunal", "Copyright Tribunal",
     "government", "government",
     "https://www.gov.uk/government/organisations/copyright-tribunal.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Copyright Tribunal\",\"government\",\"batch:50\"]",
     3600,
     "Copyright Tribunal — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_corby_education_action_zone, "uk-b50-govuk-corby-education-action-zone",
     "Corby Education Action Zone", "Corby Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/corby-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Corby Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Corby Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_council_for_catholic_maintained_schools, "uk-b50-govuk-council-for-catholic-maintained-schools",
     "Council for Catholic Maintained Schools", "Council for Catholic Maintained Schools",
     "government", "government",
     "https://www.gov.uk/government/organisations/council-for-catholic-maintained-schools.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Council for Catholic Maintained Schools\",\"government\",\"batch:50\"]",
     604800,
     "Council for Catholic Maintained Schools — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_council_on_tribunals, "uk-b50-govuk-council-on-tribunals",
     "Council on Tribunals", "Council on Tribunals",
     "government", "government",
     "https://www.gov.uk/government/organisations/council-on-tribunals.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Council on Tribunals\",\"government\",\"batch:50\"]",
     604800,
     "Council on Tribunals — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_counter_fraud_and_security_management_service, "uk-b50-govuk-counter-fraud-and-security-management-service",
     "Counter Fraud and Security Management Service", "Counter Fraud and Security Management Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/counter-fraud-and-security-management-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Counter Fraud and Security Management Service\",\"government\",\"batch:50\"]",
     604800,
     "Counter Fraud and Security Management Service — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_countryside_agency, "uk-b50-govuk-countryside-agency",
     "Countryside Agency", "Countryside Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/countryside-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Countryside Agency\",\"government\",\"batch:50\"]",
     604800,
     "Countryside Agency — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_court_of_judicature_of_northern_ireland, "uk-b50-govuk-court-of-judicature-of-northern-ireland",
     "Court of Judicature of Northern Ireland", "Court of Judicature of Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/court-of-judicature-of-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Court of Judicature of Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Court of Judicature of Northern Ireland — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_covent_garden_market_authority, "uk-b50-govuk-covent-garden-market-authority",
     "Covent Garden Market Authority", "Covent Garden Market Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/covent-garden-market-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Covent Garden Market Authority\",\"government\",\"batch:50\"]",
     21600,
     "Covent Garden Market Authority — government feed (14 entries at probe)")

RSSX(uk_b50_govuk_coventry_education_action_zone, "uk-b50-govuk-coventry-education-action-zone",
     "Coventry Education Action Zone", "Coventry Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/coventry-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Coventry Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Coventry Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_creative_scotland, "uk-b50-govuk-creative-scotland",
     "Creative Scotland", "Creative Scotland",
     "government", "government",
     "https://www.gov.uk/government/organisations/creative-scotland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Creative Scotland\",\"government\",\"batch:50\"]",
     604800,
     "Creative Scotland — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_criminal_cases_review_commission, "uk-b50-govuk-criminal-cases-review-commission",
     "UK Criminal Cases Review Commission", "UK Criminal Cases Review Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/criminal-cases-review-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Criminal Cases Review Commission\",\"government\",\"batch:50\"]",
     21600,
     "UK Criminal Cases Review Commission — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_criminal_injuries_compensation_appeals_panel, "uk-b50-govuk-criminal-injuries-compensation-appeals-panel",
     "Criminal Injuries Compensation Appeals Panel", "Criminal Injuries Compensation Appeals Panel",
     "government", "government",
     "https://www.gov.uk/government/organisations/criminal-injuries-compensation-appeals-panel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Criminal Injuries Compensation Appeals Panel\",\"government\",\"batch:50\"]",
     604800,
     "Criminal Injuries Compensation Appeals Panel — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_criminal_injuries_compensation_appeals_panel_for_northern_ireland, "uk-b50-govuk-criminal-injuries-compensation-appeals-panel-for-northern-ireland",
     "Criminal Injuries Compensation Appeals Panel for Northern Ireland", "Criminal Injuries Compensation Appeals Panel for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/criminal-injuries-compensation-appeals-panel-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Criminal Injuries Compensation Appeals Panel for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Criminal Injuries Compensation Appeals Panel for Northern Ireland — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_criminal_injuries_compensation_authority, "uk-b50-govuk-criminal-injuries-compensation-authority",
     "Criminal Injuries Compensation Authority", "Criminal Injuries Compensation Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/criminal-injuries-compensation-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Criminal Injuries Compensation Authority\",\"government\",\"batch:50\"]",
     3600,
     "Criminal Injuries Compensation Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_criminal_justice_inspection_northern_ireland, "uk-b50-govuk-criminal-justice-inspection-northern-ireland",
     "Criminal Justice Inspection Northern Ireland", "Criminal Justice Inspection Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/criminal-justice-inspection-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Criminal Justice Inspection Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Criminal Justice Inspection Northern Ireland — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_criminal_procedure_rule_committee, "uk-b50-govuk-criminal-procedure-rule-committee",
     "Criminal Procedure Rule Committee", "Criminal Procedure Rule Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/criminal-procedure-rule-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Criminal Procedure Rule Committee\",\"government\",\"batch:50\"]",
     3600,
     "Criminal Procedure Rule Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_criminal_records_bureau, "uk-b50-govuk-criminal-records-bureau",
     "Criminal Records Bureau", "Criminal Records Bureau",
     "government", "government",
     "https://www.gov.uk/government/organisations/criminal-records-bureau.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Criminal Records Bureau\",\"government\",\"batch:50\"]",
     604800,
     "Criminal Records Bureau — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_crossrail_international, "uk-b50-govuk-crossrail-international",
     "Crossrail International", "Crossrail International",
     "government", "government",
     "https://www.gov.uk/government/organisations/crossrail-international.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Crossrail International\",\"government\",\"batch:50\"]",
     21600,
     "Crossrail International — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_crown_agents_holding_and_realisation_board, "uk-b50-govuk-crown-agents-holding-and-realisation-board",
     "Crown Agents Holding and Realisation Board", "Crown Agents Holding and Realisation Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/crown-agents-holding-and-realisation-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Crown Agents Holding and Realisation Board\",\"government\",\"batch:50\"]",
     604800,
     "Crown Agents Holding and Realisation Board — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_crown_commercial_service, "uk-b50-govuk-crown-commercial-service",
     "UK Crown Commercial Service", "UK Crown Commercial Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/crown-commercial-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Crown Commercial Service\",\"government\",\"batch:50\"]",
     604800,
     "UK Crown Commercial Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_cyber_and_specialist_operations_command, "uk-b50-govuk-cyber-and-specialist-operations-command",
     "Cyber & Specialist Operations Command", "Cyber & Specialist Operations Command",
     "government", "government",
     "https://www.gov.uk/government/organisations/cyber-and-specialist-operations-command.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Cyber & Specialist Operations Command\",\"government\",\"batch:50\"]",
     3600,
     "Cyber & Specialist Operations Command — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_dartmoor_national_park_authority, "uk-b50-govuk-dartmoor-national-park-authority",
     "Dartmoor National Park Authority", "Dartmoor National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/dartmoor-national-park-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Dartmoor National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "Dartmoor National Park Authority — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_deepcut_review, "uk-b50-govuk-deepcut-review",
     "Deepcut Review", "Deepcut Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/deepcut-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Deepcut Review\",\"government\",\"batch:50\"]",
     604800,
     "Deepcut Review — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_dental_practice_board, "uk-b50-govuk-dental-practice-board",
     "Dental Practice Board", "Dental Practice Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/dental-practice-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Dental Practice Board\",\"government\",\"batch:50\"]",
     604800,
     "Dental Practice Board — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_dental_vocational_training_authority, "uk-b50-govuk-dental-vocational-training-authority",
     "Dental Vocational Training Authority", "Dental Vocational Training Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/dental-vocational-training-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Dental Vocational Training Authority\",\"government\",\"batch:50\"]",
     604800,
     "Dental Vocational Training Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_department_for_business_enterprise_and_regulatory_reform, "uk-b50-govuk-department-for-business-enterprise-and-regulatory-reform",
     "Department for Business, Enterprise and Regulatory Reform", "Department for Business, Enterprise and Regulatory Reform",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-business-enterprise-and-regulatory-reform.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Business, Enterprise and Regulatory Reform\",\"government\",\"batch:50\"]",
     604800,
     "Department for Business, Enterprise and Regulatory Reform — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_children_schools_and_families, "uk-b50-govuk-department-for-children-schools-and-families",
     "Department for Children, Schools and Families", "Department for Children, Schools and Families",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-children-schools-and-families.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Children, Schools and Families\",\"government\",\"batch:50\"]",
     604800,
     "Department for Children, Schools and Families — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_communities_northern_ireland, "uk-b50-govuk-department-for-communities-northern-ireland",
     "Department for Communities (Northern Ireland)", "Department for Communities (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-communities-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Communities (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Department for Communities (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_constitutional_affairs, "uk-b50-govuk-department-for-constitutional-affairs",
     "Department for Constitutional Affairs", "Department for Constitutional Affairs",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-constitutional-affairs.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Constitutional Affairs\",\"government\",\"batch:50\"]",
     604800,
     "Department for Constitutional Affairs — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_culture_media_and_sport, "uk-b50-govuk-department-for-culture-media-and-sport",
     "UK Culture Media Sport", "UK Culture Media Sport",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-culture-media-and-sport.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Culture Media Sport\",\"government\",\"batch:50\"]",
     604800,
     "UK Culture Media Sport — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_digital_culture_media_and_sport, "uk-b50-govuk-department-for-digital-culture-media-and-sport",
     "Department for Digital, Culture, Media and Sport", "Department for Digital, Culture, Media and Sport",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-digital-culture-media-and-sport.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Digital, Culture, Media and Sport\",\"government\",\"batch:50\"]",
     3600,
     "Department for Digital, Culture, Media and Sport — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_digital_culture_media_sport, "uk-b50-govuk-department-for-digital-culture-media-sport",
     "Department for Digital, Culture, Media & Sport (2017 to 2023)", "Department for Digital, Culture, Media & Sport (2017 to 2023)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-digital-culture-media-sport.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Digital, Culture, Media & Sport (2017 to 2023)\",\"government\",\"batch:50\"]",
     604800,
     "Department for Digital, Culture, Media & Sport (2017 to 2023) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_education_and_skills, "uk-b50-govuk-department-for-education-and-skills",
     "Department for Education and Skills", "Department for Education and Skills",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-education-and-skills.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Education and Skills\",\"government\",\"batch:50\"]",
     604800,
     "Department for Education and Skills — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_employment_and_learning, "uk-b50-govuk-department-for-employment-and-learning",
     "Department for Employment and Learning", "Department for Employment and Learning",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-employment-and-learning.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Employment and Learning\",\"government\",\"batch:50\"]",
     604800,
     "Department for Employment and Learning — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_department_for_exiting_the_european_union, "uk-b50-govuk-department-for-exiting-the-european-union",
     "Department for Exiting the European Union", "Department for Exiting the European Union",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-exiting-the-european-union.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Exiting the European Union\",\"government\",\"batch:50\"]",
     604800,
     "Department for Exiting the European Union — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_infrastructure_northern_ireland, "uk-b50-govuk-department-for-infrastructure-northern-ireland",
     "Department for Infrastructure (Northern Ireland)", "Department for Infrastructure (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-infrastructure-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Infrastructure (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Department for Infrastructure (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_regional_development, "uk-b50-govuk-department-for-regional-development",
     "Department for Regional Development (Northern Ireland)", "Department for Regional Development (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-regional-development.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Regional Development (Northern Ireland)\",\"government\",\"batch:50\"]",
     604800,
     "Department for Regional Development (Northern Ireland) — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_department_for_social_development, "uk-b50-govuk-department-for-social-development",
     "Department for Social Development", "Department for Social Development",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-social-development.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Social Development\",\"government\",\"batch:50\"]",
     604800,
     "Department for Social Development — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_the_economy_northern_ireland, "uk-b50-govuk-department-for-the-economy-northern-ireland",
     "Department for the Economy (Northern Ireland)", "Department for the Economy (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-for-the-economy-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for the Economy (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Department for the Economy (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_agriculture_and_rural_development, "uk-b50-govuk-department-of-agriculture-and-rural-development",
     "Department of Agriculture and Rural Development", "Department of Agriculture and Rural Development",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-agriculture-and-rural-development.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Agriculture and Rural Development\",\"government\",\"batch:50\"]",
     604800,
     "Department of Agriculture and Rural Development — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_culture_arts_and_leisure, "uk-b50-govuk-department-of-culture-arts-and-leisure",
     "Department of Culture, Arts and Leisure", "Department of Culture, Arts and Leisure",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-culture-arts-and-leisure.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Culture, Arts and Leisure\",\"government\",\"batch:50\"]",
     604800,
     "Department of Culture, Arts and Leisure — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_department_of_education, "uk-b50-govuk-department-of-education",
     "Department of Education (Northern Ireland)", "Department of Education (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-education.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Education (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Department of Education (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_enterprise_trade_and_investment, "uk-b50-govuk-department-of-enterprise-trade-and-investment",
     "Department of Enterprise, Trade and Investment (Northern Ireland)", "Department of Enterprise, Trade and Investment (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-enterprise-trade-and-investment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Enterprise, Trade and Investment (Northern Ireland)\",\"government\",\"batch:50\"]",
     604800,
     "Department of Enterprise, Trade and Investment (Northern Ireland) — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_department_of_finance_and_personnel_for_northern_ireland, "uk-b50-govuk-department-of-finance-and-personnel-for-northern-ireland",
     "Department of Finance and Personnel for Northern Ireland", "Department of Finance and Personnel for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-finance-and-personnel-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Finance and Personnel for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Department of Finance and Personnel for Northern Ireland — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_department_of_finance_northern_ireland, "uk-b50-govuk-department-of-finance-northern-ireland",
     "Department of Finance (Northern Ireland)", "Department of Finance (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-finance-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Finance (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Department of Finance (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_inland_revenue, "uk-b50-govuk-department-of-inland-revenue",
     "Department of Inland Revenue", "Department of Inland Revenue",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-inland-revenue.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Inland Revenue\",\"government\",\"batch:50\"]",
     604800,
     "Department of Inland Revenue — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_department_of_justice_northern_ireland, "uk-b50-govuk-department-of-justice-northern-ireland",
     "Department of Justice (Northern Ireland)", "Department of Justice (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-justice-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Justice (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Department of Justice (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_national_heritage, "uk-b50-govuk-department-of-national-heritage",
     "Department of National Heritage", "Department of National Heritage",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-national-heritage.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of National Heritage\",\"government\",\"batch:50\"]",
     604800,
     "Department of National Heritage — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_department_of_social_security, "uk-b50-govuk-department-of-social-security",
     "Department of Social Security", "Department of Social Security",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-social-security.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Social Security\",\"government\",\"batch:50\"]",
     604800,
     "Department of Social Security — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_department_of_trade_and_industry, "uk-b50-govuk-department-of-trade-and-industry",
     "Department of Trade and Industry", "Department of Trade and Industry",
     "government", "government",
     "https://www.gov.uk/government/organisations/department-of-trade-and-industry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Trade and Industry\",\"government\",\"batch:50\"]",
     604800,
     "Department of Trade and Industry — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_deputy_prime_ministers_office, "uk-b50-govuk-deputy-prime-ministers-office",
     "Deputy Prime Minister's Office", "Deputy Prime Minister's Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/deputy-prime-ministers-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Deputy Prime Minister's Office\",\"government\",\"batch:50\"]",
     604800,
     "Deputy Prime Minister's Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_derby_north_east_education_action_zone, "uk-b50-govuk-derby-north-east-education-action-zone",
     "Derby North East Education Action Zone", "Derby North East Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/derby-north-east-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Derby North East Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Derby North East Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_design_council, "uk-b50-govuk-design-council",
     "Design Council", "Design Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/design-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Design Council\",\"government\",\"batch:50\"]",
     604800,
     "Design Council — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_dft_olr_holdings_limited, "uk-b50-govuk-dft-olr-holdings-limited",
     "DfT OLR Holdings Limited", "DfT OLR Holdings Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/dft-olr-holdings-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:DfT OLR Holdings Limited\",\"government\",\"batch:50\"]",
     604800,
     "DfT OLR Holdings Limited — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_dft_operator_limited, "uk-b50-govuk-dft-operator-limited",
     "DfT Operator Limited", "DfT Operator Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/dft-operator-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:DfT Operator Limited\",\"government\",\"batch:50\"]",
     3600,
     "DfT Operator Limited — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_dingle_granby_toxteth_education_action_zone, "uk-b50-govuk-dingle-granby-toxteth-education-action-zone",
     "Dingle Granby Toxteth Education Action Zone", "Dingle Granby Toxteth Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/dingle-granby-toxteth-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Dingle Granby Toxteth Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Dingle Granby Toxteth Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_directly_operated_railways_limited, "uk-b50-govuk-directly-operated-railways-limited",
     "Directly Operated Railways Limited", "Directly Operated Railways Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/directly-operated-railways-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Directly Operated Railways Limited\",\"government\",\"batch:50\"]",
     604800,
     "Directly Operated Railways Limited — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_disability_and_carers_service, "uk-b50-govuk-disability-and-carers-service",
     "Disability and Carers Service", "Disability and Carers Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/disability-and-carers-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Disability and Carers Service\",\"government\",\"batch:50\"]",
     604800,
     "Disability and Carers Service — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_disability_rights_commission, "uk-b50-govuk-disability-rights-commission",
     "Disability Rights Commission", "Disability Rights Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/disability-rights-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Disability Rights Commission\",\"government\",\"batch:50\"]",
     604800,
     "Disability Rights Commission — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_disability_unit, "uk-b50-govuk-disability-unit",
     "Disability Unit", "Disability Unit",
     "government", "government",
     "https://www.gov.uk/government/organisations/disability-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Disability Unit\",\"government\",\"batch:50\"]",
     3600,
     "Disability Unit — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_disabled_peoples_employment_corporation, "uk-b50-govuk-disabled-peoples-employment-corporation",
     "Disabled People’s Employment Corporation (GB) Ltd", "Disabled People’s Employment Corporation (GB) Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/disabled-peoples-employment-corporation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Disabled People’s Employment Corporation (GB) Ltd\",\"government\",\"batch:50\"]",
     604800,
     "Disabled People’s Employment Corporation (GB) Ltd — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_disabled_persons_transport_advisory_committee, "uk-b50-govuk-disabled-persons-transport-advisory-committee",
     "Disabled Persons Transport Advisory Committee", "Disabled Persons Transport Advisory Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/disabled-persons-transport-advisory-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Disabled Persons Transport Advisory Committee\",\"government\",\"batch:50\"]",
     3600,
     "Disabled Persons Transport Advisory Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_disclosure_and_barring_service, "uk-b50-govuk-disclosure-and-barring-service",
     "UK DBS", "UK DBS",
     "government", "government",
     "https://www.gov.uk/government/organisations/disclosure-and-barring-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK DBS\",\"government\",\"batch:50\"]",
     3600,
     "UK DBS — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_disposal_services_agency, "uk-b50-govuk-disposal-services-agency",
     "Disposal Services Agency", "Disposal Services Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/disposal-services-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Disposal Services Agency\",\"government\",\"batch:50\"]",
     604800,
     "Disposal Services Agency — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_district_valuer_services_dvs, "uk-b50-govuk-district-valuer-services-dvs",
     "District Valuer Services", "District Valuer Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/district-valuer-services-dvs.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:District Valuer Services\",\"government\",\"batch:50\"]",
     3600,
     "District Valuer Services — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_dormant_assets_commission, "uk-b50-govuk-dormant-assets-commission",
     "Independent Dormant Assets Commission", "Independent Dormant Assets Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/dormant-assets-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Dormant Assets Commission\",\"government\",\"batch:50\"]",
     604800,
     "Independent Dormant Assets Commission — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_dounreay, "uk-b50-govuk-dounreay",
     "Dounreay", "Dounreay",
     "government", "government",
     "https://www.gov.uk/government/organisations/dounreay.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Dounreay\",\"government\",\"batch:50\"]",
     604800,
     "Dounreay — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_downham_and_bellingham_education_action_zone, "uk-b50-govuk-downham-and-bellingham-education-action-zone",
     "Downham and Bellingham Education Action Zone", "Downham and Bellingham Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/downham-and-bellingham-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Downham and Bellingham Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Downham and Bellingham Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_driver_and_vehicle_testing_agency, "uk-b50-govuk-driver-and-vehicle-testing-agency",
     "Driver and Vehicle Testing Agency", "Driver and Vehicle Testing Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/driver-and-vehicle-testing-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Driver and Vehicle Testing Agency\",\"government\",\"batch:50\"]",
     604800,
     "Driver and Vehicle Testing Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_driving_standards_agency, "uk-b50-govuk-driving-standards-agency",
     "Driving Standards Agency", "Driving Standards Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/driving-standards-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Driving Standards Agency\",\"government\",\"batch:50\"]",
     604800,
     "Driving Standards Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_dudley_education_action_zone, "uk-b50-govuk-dudley-education-action-zone",
     "Dudley Education Action Zone", "Dudley Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/dudley-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Dudley Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Dudley Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_easington_and_seaham_education_action_zone, "uk-b50-govuk-easington-and-seaham-education-action-zone",
     "Easington and Seaham Education Action Zone", "Easington and Seaham Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/easington-and-seaham-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Easington and Seaham Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Easington and Seaham Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_east_cleveland_education_action_zone, "uk-b50-govuk-east-cleveland-education-action-zone",
     "East Cleveland Education Action Zone", "East Cleveland Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/east-cleveland-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:East Cleveland Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "East Cleveland Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_east_manchester_education_action_zone, "uk-b50-govuk-east-manchester-education-action-zone",
     "East Manchester Education Action Zone", "East Manchester Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/east-manchester-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:East Manchester Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "East Manchester Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_east_midlands_development_agency_emda, "uk-b50-govuk-east-midlands-development-agency-emda",
     "East Midlands Development Agency (emda)", "East Midlands Development Agency (emda)",
     "government", "government",
     "https://www.gov.uk/government/organisations/east-midlands-development-agency-emda.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:East Midlands Development Agency (emda)\",\"government\",\"batch:50\"]",
     604800,
     "East Midlands Development Agency (emda) — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_east_of_england_development_agency, "uk-b50-govuk-east-of-england-development-agency",
     "East of England Development Agency", "East of England Development Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/east-of-england-development-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:East of England Development Agency\",\"government\",\"batch:50\"]",
     604800,
     "East of England Development Agency — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_east_west_railway_company, "uk-b50-govuk-east-west-railway-company",
     "UK East West Railway Company", "UK East West Railway Company",
     "government", "government",
     "https://www.gov.uk/government/organisations/east-west-railway-company.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK East West Railway Company\",\"government\",\"batch:50\"]",
     21600,
     "UK East West Railway Company — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_ebbsfleet_development_corporation, "uk-b50-govuk-ebbsfleet-development-corporation",
     "Ebbsfleet Development Corporation", "Ebbsfleet Development Corporation",
     "government", "government",
     "https://www.gov.uk/government/organisations/ebbsfleet-development-corporation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ebbsfleet Development Corporation\",\"government\",\"batch:50\"]",
     21600,
     "Ebbsfleet Development Corporation — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_education_and_skills_funding_agency, "uk-b50-govuk-education-and-skills-funding-agency",
     "UK Education And Skills Funding Agency", "UK Education And Skills Funding Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/education-and-skills-funding-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Education And Skills Funding Agency\",\"government\",\"batch:50\"]",
     604800,
     "UK Education And Skills Funding Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_education_funding_agency, "uk-b50-govuk-education-funding-agency",
     "Education Funding Agency", "Education Funding Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/education-funding-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Education Funding Agency\",\"government\",\"batch:50\"]",
     604800,
     "Education Funding Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_efficiency_and_reform_group, "uk-b50-govuk-efficiency-and-reform-group",
     "Efficiency and Reform Group", "Efficiency and Reform Group",
     "government", "government",
     "https://www.gov.uk/government/organisations/efficiency-and-reform-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Efficiency and Reform Group\",\"government\",\"batch:50\"]",
     604800,
     "Efficiency and Reform Group — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_electricity_settlements_company, "uk-b50-govuk-electricity-settlements-company",
     "Electricity Settlements Company", "Electricity Settlements Company",
     "government", "government",
     "https://www.gov.uk/government/organisations/electricity-settlements-company.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Electricity Settlements Company\",\"government\",\"batch:50\"]",
     21600,
     "Electricity Settlements Company — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_eliasch_review, "uk-b50-govuk-eliasch-review",
     "Eliasch Review", "Eliasch Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/eliasch-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Eliasch Review\",\"government\",\"batch:50\"]",
     604800,
     "Eliasch Review — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_ellesmere_port_education_action_zone, "uk-b50-govuk-ellesmere-port-education-action-zone",
     "Ellesmere Port Education Action Zone", "Ellesmere Port Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/ellesmere-port-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ellesmere Port Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Ellesmere Port Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_employment_agency_standards_inspectorate, "uk-b50-govuk-employment-agency-standards-inspectorate",
     "Employment Agency Standards Inspectorate", "Employment Agency Standards Inspectorate",
     "government", "government",
     "https://www.gov.uk/government/organisations/employment-agency-standards-inspectorate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Employment Agency Standards Inspectorate\",\"government\",\"batch:50\"]",
     604800,
     "Employment Agency Standards Inspectorate — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_engineering_construction_industry_training_board, "uk-b50-govuk-engineering-construction-industry-training-board",
     "Engineering Construction Industry Training Board", "Engineering Construction Industry Training Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/engineering-construction-industry-training-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Engineering Construction Industry Training Board\",\"government\",\"batch:50\"]",
     21600,
     "Engineering Construction Industry Training Board — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_english_heritage, "uk-b50-govuk-english-heritage",
     "UK English Heritage", "UK English Heritage",
     "government", "government",
     "https://www.gov.uk/government/organisations/english-heritage.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK English Heritage\",\"government\",\"batch:50\"]",
     604800,
     "UK English Heritage — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_english_institute_of_sport, "uk-b50-govuk-english-institute-of-sport",
     "UK English Institute Of Sport", "UK English Institute Of Sport",
     "government", "government",
     "https://www.gov.uk/government/organisations/english-institute-of-sport.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK English Institute Of Sport\",\"government\",\"batch:50\"]",
     21600,
     "UK English Institute Of Sport — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_english_nature, "uk-b50-govuk-english-nature",
     "English Nature", "English Nature",
     "government", "government",
     "https://www.gov.uk/government/organisations/english-nature.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:English Nature\",\"government\",\"batch:50\"]",
     604800,
     "English Nature — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_english_partnerships, "uk-b50-govuk-english-partnerships",
     "English Partnerships", "English Partnerships",
     "government", "government",
     "https://www.gov.uk/government/organisations/english-partnerships.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:English Partnerships\",\"government\",\"batch:50\"]",
     604800,
     "English Partnerships — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_english_sports_council, "uk-b50-govuk-english-sports-council",
     "English Sports Council", "English Sports Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/english-sports-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:English Sports Council\",\"government\",\"batch:50\"]",
     604800,
     "English Sports Council — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_enterprise_ulster, "uk-b50-govuk-enterprise-ulster",
     "Enterprise Ulster", "Enterprise Ulster",
     "government", "government",
     "https://www.gov.uk/government/organisations/enterprise-ulster.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Enterprise Ulster\",\"government\",\"batch:50\"]",
     604800,
     "Enterprise Ulster — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_equal_opportunities_commission, "uk-b50-govuk-equal-opportunities-commission",
     "Equal Opportunities Commission", "Equal Opportunities Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/equal-opportunities-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Equal Opportunities Commission\",\"government\",\"batch:50\"]",
     604800,
     "Equal Opportunities Commission — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_equality_2025, "uk-b50-govuk-equality-2025",
     "Equality 2025", "Equality 2025",
     "government", "government",
     "https://www.gov.uk/government/organisations/equality-2025.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Equality 2025\",\"government\",\"batch:50\"]",
     604800,
     "Equality 2025 — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_equality_and_human_rights_commission, "uk-b50-govuk-equality-and-human-rights-commission",
     "UK Equality And Human Rights Commission", "UK Equality And Human Rights Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/equality-and-human-rights-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Equality And Human Rights Commission\",\"government\",\"batch:50\"]",
     21600,
     "UK Equality And Human Rights Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_equality_commission_for_northern_ireland, "uk-b50-govuk-equality-commission-for-northern-ireland",
     "Equality Commission for Northern Ireland", "Equality Commission for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/equality-commission-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Equality Commission for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Equality Commission for Northern Ireland — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_equitable_life_inquiry, "uk-b50-govuk-equitable-life-inquiry",
     "Equitable Life Inquiry", "Equitable Life Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/equitable-life-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Equitable Life Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Equitable Life Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_estyn, "uk-b50-govuk-estyn",
     "Estyn", "Estyn",
     "government", "government",
     "https://www.gov.uk/government/organisations/estyn.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Estyn\",\"government\",\"batch:50\"]",
     21600,
     "Estyn — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_evaluation_task_force, "uk-b50-govuk-evaluation-task-force",
     "Evaluation Task Force", "Evaluation Task Force",
     "government", "government",
     "https://www.gov.uk/government/organisations/evaluation-task-force.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Evaluation Task Force\",\"government\",\"batch:50\"]",
     3600,
     "Evaluation Task Force — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_examination_team_on_child_care_procedures_and_practice_in_north_wales, "uk-b50-govuk-examination-team-on-child-care-procedures-and-practice-in-north-wales",
     "Examination team on child care procedures and practice in North Wales", "Examination team on child care procedures and practice in North Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/examination-team-on-child-care-procedures-and-practice-in-north-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Examination team on child care procedures and practice in North Wales\",\"government\",\"batch:50\"]",
     604800,
     "Examination team on child care procedures and practice in North Wales — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_exchequer_and_audit_department, "uk-b50-govuk-exchequer-and-audit-department",
     "Exchequer and Audit Department", "Exchequer and Audit Department",
     "government", "government",
     "https://www.gov.uk/government/organisations/exchequer-and-audit-department.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Exchequer and Audit Department\",\"government\",\"batch:50\"]",
     604800,
     "Exchequer and Audit Department — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_exmoor_national_park_authority, "uk-b50-govuk-exmoor-national-park-authority",
     "Exmoor National Park Authority", "Exmoor National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/exmoor-national-park-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Exmoor National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "Exmoor National Park Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_export_guarantees_advisory_council, "uk-b50-govuk-export-guarantees-advisory-council",
     "Export Guarantees Advisory Council", "Export Guarantees Advisory Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/export-guarantees-advisory-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Export Guarantees Advisory Council\",\"government\",\"batch:50\"]",
     3600,
     "Export Guarantees Advisory Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_fair_work_agency, "uk-b50-govuk-fair-work-agency",
     "Fair Work Agency", "Fair Work Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/fair-work-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Fair Work Agency\",\"government\",\"batch:50\"]",
     3600,
     "Fair Work Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_family_justice_council, "uk-b50-govuk-family-justice-council",
     "Family Justice Council", "Family Justice Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/family-justice-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Family Justice Council\",\"government\",\"batch:50\"]",
     21600,
     "Family Justice Council — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_family_procedure_rule_committee, "uk-b50-govuk-family-procedure-rule-committee",
     "Family Procedure Rule Committee", "Family Procedure Rule Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/family-procedure-rule-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Family Procedure Rule Committee\",\"government\",\"batch:50\"]",
     3600,
     "Family Procedure Rule Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_fcdo_services, "uk-b50-govuk-fcdo-services",
     "FCDO Services", "FCDO Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/fcdo-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:FCDO Services\",\"government\",\"batch:50\"]",
     21600,
     "FCDO Services — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_fco_services, "uk-b50-govuk-fco-services",
     "FCO Services", "FCO Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/fco-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:FCO Services\",\"government\",\"batch:50\"]",
     604800,
     "FCO Services — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_financial_conduct_authority, "uk-b50-govuk-financial-conduct-authority",
     "UK FCA", "UK FCA",
     "government", "government",
     "https://www.gov.uk/government/organisations/financial-conduct-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK FCA\",\"government\",\"batch:50\"]",
     21600,
     "UK FCA — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_financial_reporting_council, "uk-b50-govuk-financial-reporting-council",
     "UK Financial Reporting Council", "UK Financial Reporting Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/financial-reporting-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Financial Reporting Council\",\"government\",\"batch:50\"]",
     21600,
     "UK Financial Reporting Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_financial_services_organisation, "uk-b50-govuk-financial-services-organisation",
     "Financial Services Organisation", "Financial Services Organisation",
     "government", "government",
     "https://www.gov.uk/government/organisations/financial-services-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Financial Services Organisation\",\"government\",\"batch:50\"]",
     604800,
     "Financial Services Organisation — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_financial_services_trade_and_investment_board, "uk-b50-govuk-financial-services-trade-and-investment-board",
     "Financial Services Trade and Investment Board", "Financial Services Trade and Investment Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/financial-services-trade-and-investment-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Financial Services Trade and Investment Board\",\"government\",\"batch:50\"]",
     604800,
     "Financial Services Trade and Investment Board — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_fire_authority_for_northern_ireland, "uk-b50-govuk-fire-authority-for-northern-ireland",
     "Fire Authority for Northern Ireland", "Fire Authority for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/fire-authority-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Fire Authority for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Fire Authority for Northern Ireland — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_fire_service_college, "uk-b50-govuk-fire-service-college",
     "Fire Service College", "Fire Service College",
     "government", "government",
     "https://www.gov.uk/government/organisations/fire-service-college.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Fire Service College\",\"government\",\"batch:50\"]",
     21600,
     "Fire Service College — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_firearms_consultative_committee, "uk-b50-govuk-firearms-consultative-committee",
     "Firearms Consultative Committee", "Firearms Consultative Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/firearms-consultative-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Firearms Consultative Committee\",\"government\",\"batch:50\"]",
     604800,
     "Firearms Consultative Committee — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_firebuy, "uk-b50-govuk-firebuy",
     "Firebuy", "Firebuy",
     "government", "government",
     "https://www.gov.uk/government/organisations/firebuy.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Firebuy\",\"government\",\"batch:50\"]",
     604800,
     "Firebuy — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_fisheries_conservancy_board_for_northern_ireland, "uk-b50-govuk-fisheries-conservancy-board-for-northern-ireland",
     "Fisheries Conservancy Board for Northern Ireland", "Fisheries Conservancy Board for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/fisheries-conservancy-board-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Fisheries Conservancy Board for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Fisheries Conservancy Board for Northern Ireland — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_fleet_air_arm_museum, "uk-b50-govuk-fleet-air-arm-museum",
     "UK Fleet Air Arm Museum", "UK Fleet Air Arm Museum",
     "government", "government",
     "https://www.gov.uk/government/organisations/fleet-air-arm-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Fleet Air Arm Museum\",\"government\",\"batch:50\"]",
     21600,
     "UK Fleet Air Arm Museum — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_flood_forecasting_centre, "uk-b50-govuk-flood-forecasting-centre",
     "Flood Forecasting Centre", "Flood Forecasting Centre",
     "government", "government",
     "https://www.gov.uk/government/organisations/flood-forecasting-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Flood Forecasting Centre\",\"government\",\"batch:50\"]",
     3600,
     "Flood Forecasting Centre — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_flood_re, "uk-b50-govuk-flood-re",
     "Flood Re", "Flood Re",
     "government", "government",
     "https://www.gov.uk/government/organisations/flood-re.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Flood Re\",\"government\",\"batch:50\"]",
     21600,
     "Flood Re — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_food_from_britain, "uk-b50-govuk-food-from-britain",
     "Food from Britain", "Food from Britain",
     "government", "government",
     "https://www.gov.uk/government/organisations/food-from-britain.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Food from Britain\",\"government\",\"batch:50\"]",
     604800,
     "Food from Britain — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_food_safety_promotion_board, "uk-b50-govuk-food-safety-promotion-board",
     "Food Safety Promotion Board", "Food Safety Promotion Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/food-safety-promotion-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Food Safety Promotion Board\",\"government\",\"batch:50\"]",
     604800,
     "Food Safety Promotion Board — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_football_licensing_authority, "uk-b50-govuk-football-licensing-authority",
     "Football Licensing Authority", "Football Licensing Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/football-licensing-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Football Licensing Authority\",\"government\",\"batch:50\"]",
     604800,
     "Football Licensing Authority — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_foreign_commonwealth_office, "uk-b50-govuk-foreign-commonwealth-office",
     "Foreign & Commonwealth Office", "Foreign & Commonwealth Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/foreign-commonwealth-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Foreign & Commonwealth Office\",\"government\",\"batch:50\"]",
     604800,
     "Foreign & Commonwealth Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_foreign_compensation_commission, "uk-b50-govuk-foreign-compensation-commission",
     "Foreign Compensation Commission", "Foreign Compensation Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/foreign-compensation-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Foreign Compensation Commission\",\"government\",\"batch:50\"]",
     604800,
     "Foreign Compensation Commission — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_forensic_science_regulator, "uk-b50-govuk-forensic-science-regulator",
     "Forensic Science Regulator", "Forensic Science Regulator",
     "government", "government",
     "https://www.gov.uk/government/organisations/forensic-science-regulator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Forensic Science Regulator\",\"government\",\"batch:50\"]",
     3600,
     "Forensic Science Regulator — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_forest_service, "uk-b50-govuk-forest-service",
     "Forest Service", "Forest Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/forest-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Forest Service\",\"government\",\"batch:50\"]",
     604800,
     "Forest Service — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_further_and_higher_education_funding_councils_for_wales, "uk-b50-govuk-further-and-higher-education-funding-councils-for-wales",
     "Further and Higher Education Funding Councils for Wales", "Further and Higher Education Funding Councils for Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/further-and-higher-education-funding-councils-for-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Further and Higher Education Funding Councils for Wales\",\"government\",\"batch:50\"]",
     604800,
     "Further and Higher Education Funding Councils for Wales — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_further_education_commissioner, "uk-b50-govuk-further-education-commissioner",
     "Further Education Commissioner", "Further Education Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/further-education-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Further Education Commissioner\",\"government\",\"batch:50\"]",
     3600,
     "Further Education Commissioner — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_gaming_board_for_great_britain, "uk-b50-govuk-gaming-board-for-great-britain",
     "Gaming Board for Great Britain", "Gaming Board for Great Britain",
     "government", "government",
     "https://www.gov.uk/government/organisations/gaming-board-for-great-britain.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Gaming Board for Great Britain\",\"government\",\"batch:50\"]",
     604800,
     "Gaming Board for Great Britain — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_gangmasters_and_labour_abuse_authority, "uk-b50-govuk-gangmasters-and-labour-abuse-authority",
     "UK Gangmasters And Labour Abuse Authority", "UK Gangmasters And Labour Abuse Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/gangmasters-and-labour-abuse-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Gangmasters And Labour Abuse Authority\",\"government\",\"batch:50\"]",
     604800,
     "UK Gangmasters And Labour Abuse Authority — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_gangmasters_licensing_authority, "uk-b50-govuk-gangmasters-licensing-authority",
     "Gangmasters Licensing Authority", "Gangmasters Licensing Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/gangmasters-licensing-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Gangmasters Licensing Authority\",\"government\",\"batch:50\"]",
     604800,
     "Gangmasters Licensing Authority — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_gas_and_electricity_consumer_council_energywatch, "uk-b50-govuk-gas-and-electricity-consumer-council-energywatch",
     "Gas and Electricity Consumer Council (Energywatch)", "Gas and Electricity Consumer Council (Energywatch)",
     "government", "government",
     "https://www.gov.uk/government/organisations/gas-and-electricity-consumer-council-energywatch.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Gas and Electricity Consumer Council (Energywatch)\",\"government\",\"batch:50\"]",
     604800,
     "Gas and Electricity Consumer Council (Energywatch) — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_geffrye_museum, "uk-b50-govuk-geffrye-museum",
     "Museum of the Home", "Museum of the Home",
     "government", "government",
     "https://www.gov.uk/government/organisations/geffrye-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Museum of the Home\",\"government\",\"batch:50\"]",
     21600,
     "Museum of the Home — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_general_consumer_council_for_northern_ireland, "uk-b50-govuk-general-consumer-council-for-northern-ireland",
     "General Consumer Council for Northern Ireland", "General Consumer Council for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/general-consumer-council-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:General Consumer Council for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "General Consumer Council for Northern Ireland — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_general_social_care_council, "uk-b50-govuk-general-social-care-council",
     "General Social Care Council", "General Social Care Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/general-social-care-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:General Social Care Council\",\"government\",\"batch:50\"]",
     604800,
     "General Social Care Council — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_general_teaching_council_for_england, "uk-b50-govuk-general-teaching-council-for-england",
     "General Teaching Council for England", "General Teaching Council for England",
     "government", "government",
     "https://www.gov.uk/government/organisations/general-teaching-council-for-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:General Teaching Council for England\",\"government\",\"batch:50\"]",
     604800,
     "General Teaching Council for England — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_geospatial_commission, "uk-b50-govuk-geospatial-commission",
     "UK Geospatial Commission", "UK Geospatial Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/geospatial-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Geospatial Commission\",\"government\",\"batch:50\"]",
     604800,
     "UK Geospatial Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_gloucester_education_action_zone, "uk-b50-govuk-gloucester-education-action-zone",
     "Gloucester Education Action Zone", "Gloucester Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/gloucester-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Gloucester Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Gloucester Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_gov_facility_services_limited, "uk-b50-govuk-gov-facility-services-limited",
     "Gov Facility Services Limited", "Gov Facility Services Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/gov-facility-services-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Gov Facility Services Limited\",\"government\",\"batch:50\"]",
     3600,
     "Gov Facility Services Limited — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_actuarys_department, "uk-b50-govuk-government-actuarys-department",
     "Government Actuary's Department", "Government Actuary's Department",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-actuarys-department.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Actuary's Department\",\"government\",\"batch:50\"]",
     3600,
     "Government Actuary's Department — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_analysis_function, "uk-b50-govuk-government-analysis-function",
     "Government Analysis Function", "Government Analysis Function",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-analysis-function.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Analysis Function\",\"government\",\"batch:50\"]",
     3600,
     "Government Analysis Function — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_car_and_despatch_agency, "uk-b50-govuk-government-car-and-despatch-agency",
     "Government Car and Despatch Agency", "Government Car and Despatch Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-car-and-despatch-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Car and Despatch Agency\",\"government\",\"batch:50\"]",
     604800,
     "Government Car and Despatch Agency — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_government_chemist, "uk-b50-govuk-government-chemist",
     "Government Chemist", "Government Chemist",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-chemist.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Chemist\",\"government\",\"batch:50\"]",
     3600,
     "Government Chemist — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_commercial_agency, "uk-b50-govuk-government-commercial-agency",
     "Government Commercial Agency", "Government Commercial Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-commercial-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Commercial Agency\",\"government\",\"batch:50\"]",
     21600,
     "Government Commercial Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_commercial_function, "uk-b50-govuk-government-commercial-function",
     "Government Commercial Function", "Government Commercial Function",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-commercial-function.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Commercial Function\",\"government\",\"batch:50\"]",
     3600,
     "Government Commercial Function — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_debt_management_function, "uk-b50-govuk-government-debt-management-function",
     "Government Debt Management Function", "Government Debt Management Function",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-debt-management-function.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Debt Management Function\",\"government\",\"batch:50\"]",
     3600,
     "Government Debt Management Function — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_government_digital_service, "uk-b50-govuk-government-digital-service",
     "UK Government Digital Service", "UK Government Digital Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-digital-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Government Digital Service\",\"government\",\"batch:50\"]",
     3600,
     "UK Government Digital Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_equalities_office, "uk-b50-govuk-government-equalities-office",
     "Government Equalities Office", "Government Equalities Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-equalities-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Equalities Office\",\"government\",\"batch:50\"]",
     604800,
     "Government Equalities Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_finance_function, "uk-b50-govuk-government-finance-function",
     "Government Finance Function", "Government Finance Function",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-finance-function.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Finance Function\",\"government\",\"batch:50\"]",
     3600,
     "Government Finance Function — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_government_geography_profession, "uk-b50-govuk-government-geography-profession",
     "Government Geography Profession", "Government Geography Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-geography-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Geography Profession\",\"government\",\"batch:50\"]",
     3600,
     "Government Geography Profession — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_internal_audit_agency, "uk-b50-govuk-government-internal-audit-agency",
     "Government Internal Audit Agency", "Government Internal Audit Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-internal-audit-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Internal Audit Agency\",\"government\",\"batch:50\"]",
     3600,
     "Government Internal Audit Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_office_for_technology_transfer, "uk-b50-govuk-government-office-for-technology-transfer",
     "Government Office for Technology Transfer", "Government Office for Technology Transfer",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-office-for-technology-transfer.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Office for Technology Transfer\",\"government\",\"batch:50\"]",
     3600,
     "Government Office for Technology Transfer — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_people_function, "uk-b50-govuk-government-people-function",
     "Government People Function", "Government People Function",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-people-function.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government People Function\",\"government\",\"batch:50\"]",
     3600,
     "Government People Function — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_government_people_group, "uk-b50-govuk-government-people-group",
     "Government People Group", "Government People Group",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-people-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government People Group\",\"government\",\"batch:50\"]",
     3600,
     "Government People Group — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_procurement_service, "uk-b50-govuk-government-procurement-service",
     "Government Procurement Service", "Government Procurement Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-procurement-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Procurement Service\",\"government\",\"batch:50\"]",
     604800,
     "Government Procurement Service — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_government_project_delivery, "uk-b50-govuk-government-project-delivery",
     "Government Project Delivery", "Government Project Delivery",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-project-delivery.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Project Delivery\",\"government\",\"batch:50\"]",
     3600,
     "Government Project Delivery — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_government_property_agency, "uk-b50-govuk-government-property-agency",
     "UK GPA", "UK GPA",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-property-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK GPA\",\"government\",\"batch:50\"]",
     3600,
     "UK GPA — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_property_function, "uk-b50-govuk-government-property-function",
     "Government Property Function", "Government Property Function",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-property-function.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Property Function\",\"government\",\"batch:50\"]",
     3600,
     "Government Property Function — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_recruitment_service, "uk-b50-govuk-government-recruitment-service",
     "Government Recruitment Service", "Government Recruitment Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-recruitment-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Recruitment Service\",\"government\",\"batch:50\"]",
     3600,
     "Government Recruitment Service — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_government_security_profession, "uk-b50-govuk-government-security-profession",
     "Government Security Profession", "Government Security Profession",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-security-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Security Profession\",\"government\",\"batch:50\"]",
     3600,
     "Government Security Profession — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_government_skills, "uk-b50-govuk-government-skills",
     "Government Skills", "Government Skills",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-skills.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Skills\",\"government\",\"batch:50\"]",
     3600,
     "Government Skills — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_skills_and_curriculum_unit, "uk-b50-govuk-government-skills-and-curriculum-unit",
     "Government Skills and Curriculum Unit", "Government Skills and Curriculum Unit",
     "government", "government",
     "https://www.gov.uk/government/organisations/government-skills-and-curriculum-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Skills and Curriculum Unit\",\"government\",\"batch:50\"]",
     604800,
     "Government Skills and Curriculum Unit — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_gowers_review_of_intellectual_property, "uk-b50-govuk-gowers-review-of-intellectual-property",
     "Gowers Review of Intellectual Property", "Gowers Review of Intellectual Property",
     "government", "government",
     "https://www.gov.uk/government/organisations/gowers-review-of-intellectual-property.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Gowers Review of Intellectual Property\",\"government\",\"batch:50\"]",
     604800,
     "Gowers Review of Intellectual Property — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_great_britain_china_centre, "uk-b50-govuk-great-britain-china-centre",
     "Great Britain-China Centre", "Great Britain-China Centre",
     "government", "government",
     "https://www.gov.uk/government/organisations/great-britain-china-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Great Britain-China Centre\",\"government\",\"batch:50\"]",
     21600,
     "Great Britain-China Centre — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_great_british_energy, "uk-b50-govuk-great-british-energy",
     "Great British Energy", "Great British Energy",
     "government", "government",
     "https://www.gov.uk/government/organisations/great-british-energy.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Great British Energy\",\"government\",\"batch:50\"]",
     21600,
     "Great British Energy — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_great_yarmouth_education_action_zone, "uk-b50-govuk-great-yarmouth-education-action-zone",
     "Great Yarmouth Education Action Zone", "Great Yarmouth Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/great-yarmouth-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Great Yarmouth Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Great Yarmouth Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_greenwich_education_action_zone, "uk-b50-govuk-greenwich-education-action-zone",
     "Greenwich Education Action Zone", "Greenwich Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/greenwich-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Greenwich Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Greenwich Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_groceries_code_adjudicator, "uk-b50-govuk-groceries-code-adjudicator",
     "Groceries Code Adjudicator", "Groceries Code Adjudicator",
     "government", "government",
     "https://www.gov.uk/government/organisations/groceries-code-adjudicator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Groceries Code Adjudicator\",\"government\",\"batch:50\"]",
     3600,
     "Groceries Code Adjudicator — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hackney_education_action_zone, "uk-b50-govuk-hackney-education-action-zone",
     "Hackney Education Action Zone", "Hackney Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/hackney-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Hackney Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Hackney Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_hamilton_oxford_education_action_zone, "uk-b50-govuk-hamilton-oxford-education-action-zone",
     "Hamilton Oxford Education Action Zone", "Hamilton Oxford Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/hamilton-oxford-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Hamilton Oxford Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Hamilton Oxford Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_hastings_and_st_leonards_education_action_zone, "uk-b50-govuk-hastings-and-st-leonards-education-action-zone",
     "Hastings and St Leonards Education Action Zone", "Hastings and St Leonards Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/hastings-and-st-leonards-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Hastings and St Leonards Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Hastings and St Leonards Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_health_and_safety_executive, "uk-b50-govuk-health-and-safety-executive",
     "UK HSE", "UK HSE",
     "government", "government",
     "https://www.gov.uk/government/organisations/health-and-safety-executive.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK HSE\",\"government\",\"batch:50\"]",
     21600,
     "UK HSE — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_health_education_england, "uk-b50-govuk-health-education-england",
     "UK Health Education England", "UK Health Education England",
     "government", "government",
     "https://www.gov.uk/government/organisations/health-education-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Health Education England\",\"government\",\"batch:50\"]",
     604800,
     "UK Health Education England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hearing_aid_council, "uk-b50-govuk-hearing-aid-council",
     "Hearing Aid Council", "Hearing Aid Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/hearing-aid-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Hearing Aid Council\",\"government\",\"batch:50\"]",
     604800,
     "Hearing Aid Council — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_heart_of_slough_education_action_zone, "uk-b50-govuk-heart-of-slough-education-action-zone",
     "Heart of Slough Education Action Zone", "Heart of Slough Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/heart-of-slough-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Heart of Slough Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Heart of Slough Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_her_majestys_magistrates_courts_service_inspectorate, "uk-b50-govuk-her-majestys-magistrates-courts-service-inspectorate",
     "Her Majesty's Magistrates Courts Service Inspectorate", "Her Majesty's Magistrates Courts Service Inspectorate",
     "government", "government",
     "https://www.gov.uk/government/organisations/her-majestys-magistrates-courts-service-inspectorate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Her Majesty's Magistrates Courts Service Inspectorate\",\"government\",\"batch:50\"]",
     604800,
     "Her Majesty's Magistrates Courts Service Inspectorate — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_heritage_lottery_fund, "uk-b50-govuk-heritage-lottery-fund",
     "Heritage Lottery Fund (administered by the NHMF)", "Heritage Lottery Fund (administered by the NHMF)",
     "government", "government",
     "https://www.gov.uk/government/organisations/heritage-lottery-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Heritage Lottery Fund (administered by the NHMF)\",\"government\",\"batch:50\"]",
     604800,
     "Heritage Lottery Fund (administered by the NHMF) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_high_speed_two_limited, "uk-b50-govuk-high-speed-two-limited",
     "High Speed Two (HS2) Limited", "High Speed Two (HS2) Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/high-speed-two-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:High Speed Two (HS2) Limited\",\"government\",\"batch:50\"]",
     3600,
     "High Speed Two (HS2) Limited — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_higher_education_funding_council_for_england, "uk-b50-govuk-higher-education-funding-council-for-england",
     "Higher Education Funding Council for England", "Higher Education Funding Council for England",
     "government", "government",
     "https://www.gov.uk/government/organisations/higher-education-funding-council-for-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Higher Education Funding Council for England\",\"government\",\"batch:50\"]",
     604800,
     "Higher Education Funding Council for England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_highways_agency, "uk-b50-govuk-highways-agency",
     "Highways Agency", "Highways Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/highways-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Highways Agency\",\"government\",\"batch:50\"]",
     604800,
     "Highways Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_highways_england, "uk-b50-govuk-highways-england",
     "UK National Highways", "UK National Highways",
     "government", "government",
     "https://www.gov.uk/government/organisations/highways-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK National Highways\",\"government\",\"batch:50\"]",
     604800,
     "UK National Highways — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hillsborough_independent_panel, "uk-b50-govuk-hillsborough-independent-panel",
     "Hillsborough Independent Panel", "Hillsborough Independent Panel",
     "government", "government",
     "https://www.gov.uk/government/organisations/hillsborough-independent-panel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Hillsborough Independent Panel\",\"government\",\"batch:50\"]",
     604800,
     "Hillsborough Independent Panel — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_historic_england, "uk-b50-govuk-historic-england",
     "UK Historic England", "UK Historic England",
     "government", "government",
     "https://www.gov.uk/government/organisations/historic-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Historic England\",\"government\",\"batch:50\"]",
     21600,
     "UK Historic England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_historic_royal_palaces, "uk-b50-govuk-historic-royal-palaces",
     "Historic Royal Palaces", "Historic Royal Palaces",
     "government", "government",
     "https://www.gov.uk/government/organisations/historic-royal-palaces.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Historic Royal Palaces\",\"government\",\"batch:50\"]",
     21600,
     "Historic Royal Palaces — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_hm_court_service, "uk-b50-govuk-hm-court-service",
     "HM Courts Service", "HM Courts Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-court-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:HM Courts Service\",\"government\",\"batch:50\"]",
     604800,
     "HM Courts Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_crown_prosecution_service_inspectorate, "uk-b50-govuk-hm-crown-prosecution-service-inspectorate",
     "HM Crown Prosecution Service Inspectorate", "HM Crown Prosecution Service Inspectorate",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-crown-prosecution-service-inspectorate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:HM Crown Prosecution Service Inspectorate\",\"government\",\"batch:50\"]",
     21600,
     "HM Crown Prosecution Service Inspectorate — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_customs_and_excise, "uk-b50-govuk-hm-customs-and-excise",
     "H.M. Customs and Excise", "H.M. Customs and Excise",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-customs-and-excise.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:H.M. Customs and Excise\",\"government\",\"batch:50\"]",
     604800,
     "H.M. Customs and Excise — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_hm_government_communications_centre, "uk-b50-govuk-hm-government-communications-centre",
     "HM Government Communications Centre", "HM Government Communications Centre",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-government-communications-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:HM Government Communications Centre\",\"government\",\"batch:50\"]",
     21600,
     "HM Government Communications Centre — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_hm_inspectorate_of_constabulary, "uk-b50-govuk-hm-inspectorate-of-constabulary",
     "UK Hm Inspectorate Of Constabulary", "UK Hm Inspectorate Of Constabulary",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-inspectorate-of-constabulary.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Hm Inspectorate Of Constabulary\",\"government\",\"batch:50\"]",
     604800,
     "UK Hm Inspectorate Of Constabulary — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_inspectorate_of_constabulary_and_fire_rescue_services, "uk-b50-govuk-hm-inspectorate-of-constabulary-and-fire-rescue-services",
     "HM Inspectorate of Constabulary and Fire & Rescue Services", "HM Inspectorate of Constabulary and Fire & Rescue Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-inspectorate-of-constabulary-and-fire-rescue-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:HM Inspectorate of Constabulary and Fire & Rescue Services\",\"government\",\"batch:50\"]",
     21600,
     "HM Inspectorate of Constabulary and Fire & Rescue Services — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_inspectorate_of_court_administration, "uk-b50-govuk-hm-inspectorate-of-court-administration",
     "H.M. Inspectorate of Court Administration", "H.M. Inspectorate of Court Administration",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-inspectorate-of-court-administration.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:H.M. Inspectorate of Court Administration\",\"government\",\"batch:50\"]",
     604800,
     "H.M. Inspectorate of Court Administration — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_hm_inspectorate_of_explosives_for_northern_ireland, "uk-b50-govuk-hm-inspectorate-of-explosives-for-northern-ireland",
     "H.M. Inspectorate of Explosives for Northern Ireland", "H.M. Inspectorate of Explosives for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-inspectorate-of-explosives-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:H.M. Inspectorate of Explosives for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "H.M. Inspectorate of Explosives for Northern Ireland — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_hm_inspectorate_of_prisons, "uk-b50-govuk-hm-inspectorate-of-prisons",
     "UK Hm Inspectorate Of Prisons", "UK Hm Inspectorate Of Prisons",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-inspectorate-of-prisons.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Hm Inspectorate Of Prisons\",\"government\",\"batch:50\"]",
     21600,
     "UK Hm Inspectorate Of Prisons — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_inspectorate_of_probation, "uk-b50-govuk-hm-inspectorate-of-probation",
     "UK Hm Inspectorate Of Probation", "UK Hm Inspectorate Of Probation",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-inspectorate-of-probation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Hm Inspectorate Of Probation\",\"government\",\"batch:50\"]",
     21600,
     "UK Hm Inspectorate Of Probation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_nautical_almanac_office, "uk-b50-govuk-hm-nautical-almanac-office",
     "HM Nautical Almanac Office", "HM Nautical Almanac Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-nautical-almanac-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:HM Nautical Almanac Office\",\"government\",\"batch:50\"]",
     3600,
     "HM Nautical Almanac Office — government feed (15 entries at probe)")

RSSX(uk_b50_govuk_hm_passport_office, "uk-b50-govuk-hm-passport-office",
     "UK Hm Passport Office", "UK Hm Passport Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-passport-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Hm Passport Office\",\"government\",\"batch:50\"]",
     3600,
     "UK Hm Passport Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_prison_and_probation_service, "uk-b50-govuk-hm-prison-and-probation-service",
     "UK HMPPS", "UK HMPPS",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-prison-and-probation-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK HMPPS\",\"government\",\"batch:50\"]",
     3600,
     "UK HMPPS — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hm_prison_service, "uk-b50-govuk-hm-prison-service",
     "HM Prison Service", "HM Prison Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/hm-prison-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:HM Prison Service\",\"government\",\"batch:50\"]",
     3600,
     "HM Prison Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_home_grown_cereals_authority, "uk-b50-govuk-home-grown-cereals-authority",
     "Home-Grown Cereals Authority", "Home-Grown Cereals Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/home-grown-cereals-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Home-Grown Cereals Authority\",\"government\",\"batch:50\"]",
     604800,
     "Home-Grown Cereals Authority — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_homes_and_communities_agency, "uk-b50-govuk-homes-and-communities-agency",
     "Homes and Communities Agency", "Homes and Communities Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/homes-and-communities-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Homes and Communities Agency\",\"government\",\"batch:50\"]",
     604800,
     "Homes and Communities Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_homes_england, "uk-b50-govuk-homes-england",
     "UK Homes England", "UK Homes England",
     "government", "government",
     "https://www.gov.uk/government/organisations/homes-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Homes England\",\"government\",\"batch:50\"]",
     3600,
     "UK Homes England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_horniman_museum, "uk-b50-govuk-horniman-museum",
     "Horniman Public Museum and Public Park Trust", "Horniman Public Museum and Public Park Trust",
     "government", "government",
     "https://www.gov.uk/government/organisations/horniman-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Horniman Public Museum and Public Park Trust\",\"government\",\"batch:50\"]",
     21600,
     "Horniman Public Museum and Public Park Trust — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_horserace_betting_levy_board, "uk-b50-govuk-horserace-betting-levy-board",
     "Horserace Betting Levy Board", "Horserace Betting Levy Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/horserace-betting-levy-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Horserace Betting Levy Board\",\"government\",\"batch:50\"]",
     21600,
     "Horserace Betting Levy Board — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_horticultural_development_council, "uk-b50-govuk-horticultural-development-council",
     "Horticultural Development Council", "Horticultural Development Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/horticultural-development-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Horticultural Development Council\",\"government\",\"batch:50\"]",
     604800,
     "Horticultural Development Council — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_house_of_lords_appointments_commission, "uk-b50-govuk-house-of-lords-appointments-commission",
     "House of Lords Appointments Commission", "House of Lords Appointments Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/house-of-lords-appointments-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:House of Lords Appointments Commission\",\"government\",\"batch:50\"]",
     21600,
     "House of Lords Appointments Commission — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_housing_corporation, "uk-b50-govuk-housing-corporation",
     "Housing Corporation", "Housing Corporation",
     "government", "government",
     "https://www.gov.uk/government/organisations/housing-corporation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Housing Corporation\",\"government\",\"batch:50\"]",
     604800,
     "Housing Corporation — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_housing_ombudsman, "uk-b50-govuk-housing-ombudsman",
     "Housing Ombudsman", "Housing Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/housing-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Housing Ombudsman\",\"government\",\"batch:50\"]",
     21600,
     "Housing Ombudsman — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_hsc_business_services_organisation_northern_ireland, "uk-b50-govuk-hsc-business-services-organisation-northern-ireland",
     "HSC Business Services Organisation (Northern Ireland)", "HSC Business Services Organisation (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/hsc-business-services-organisation-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:HSC Business Services Organisation (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "HSC Business Services Organisation (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_human_fertilisation_and_embryology_authority, "uk-b50-govuk-human-fertilisation-and-embryology-authority",
     "Human Fertilisation and Embryology Authority", "Human Fertilisation and Embryology Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/human-fertilisation-and-embryology-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Human Fertilisation and Embryology Authority\",\"government\",\"batch:50\"]",
     21600,
     "Human Fertilisation and Embryology Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_human_tissue_authority, "uk-b50-govuk-human-tissue-authority",
     "Human Tissue Authority", "Human Tissue Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/human-tissue-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Human Tissue Authority\",\"government\",\"batch:50\"]",
     21600,
     "Human Tissue Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_hybu_cig_cymru_meat_promotion_wales, "uk-b50-govuk-hybu-cig-cymru-meat-promotion-wales",
     "Hybu Cig Cymru - Meat Promotion Wales", "Hybu Cig Cymru - Meat Promotion Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/hybu-cig-cymru-meat-promotion-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Hybu Cig Cymru - Meat Promotion Wales\",\"government\",\"batch:50\"]",
     604800,
     "Hybu Cig Cymru - Meat Promotion Wales — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_icl_inquiry, "uk-b50-govuk-icl-inquiry",
     "ICL Inquiry", "ICL Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/icl-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:ICL Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "ICL Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_identity_and_passport_service, "uk-b50-govuk-identity-and-passport-service",
     "Identity and Passport Service", "Identity and Passport Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/identity-and-passport-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Identity and Passport Service\",\"government\",\"batch:50\"]",
     604800,
     "Identity and Passport Service — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_immigration_advice_authority, "uk-b50-govuk-immigration-advice-authority",
     "Immigration Advice Authority", "Immigration Advice Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/immigration-advice-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Immigration Advice Authority\",\"government\",\"batch:50\"]",
     3600,
     "Immigration Advice Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_immigration_enforcement, "uk-b50-govuk-immigration-enforcement",
     "UK Immigration Enforcement", "UK Immigration Enforcement",
     "government", "government",
     "https://www.gov.uk/government/organisations/immigration-enforcement.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Immigration Enforcement\",\"government\",\"batch:50\"]",
     3600,
     "UK Immigration Enforcement — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_imperial_war_museum, "uk-b50-govuk-imperial-war-museum",
     "Imperial War Museum", "Imperial War Museum",
     "government", "government",
     "https://www.gov.uk/government/organisations/imperial-war-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Imperial War Museum\",\"government\",\"batch:50\"]",
     21600,
     "Imperial War Museum — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_adviser_on_ministers_interests, "uk-b50-govuk-independent-adviser-on-ministers-interests",
     "Independent Adviser on Ministerial Standards", "Independent Adviser on Ministerial Standards",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-adviser-on-ministers-interests.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Adviser on Ministerial Standards\",\"government\",\"batch:50\"]",
     3600,
     "Independent Adviser on Ministerial Standards — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_advisory_panel_on_deaths_in_custody, "uk-b50-govuk-independent-advisory-panel-on-deaths-in-custody",
     "Independent Advisory Panel on Deaths in Custody", "Independent Advisory Panel on Deaths in Custody",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-advisory-panel-on-deaths-in-custody.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Advisory Panel on Deaths in Custody\",\"government\",\"batch:50\"]",
     604800,
     "Independent Advisory Panel on Deaths in Custody — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_independent_anti_slavery_commissioner, "uk-b50-govuk-independent-anti-slavery-commissioner",
     "Independent Anti-slavery Commissioner", "Independent Anti-slavery Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-anti-slavery-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Anti-slavery Commissioner\",\"government\",\"batch:50\"]",
     21600,
     "Independent Anti-slavery Commissioner — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_independent_case_examiner, "uk-b50-govuk-independent-case-examiner",
     "Independent Case Examiner", "Independent Case Examiner",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-case-examiner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Case Examiner\",\"government\",\"batch:50\"]",
     3600,
     "Independent Case Examiner — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_chief_inspector_of_borders_and_immigration, "uk-b50-govuk-independent-chief-inspector-of-borders-and-immigration",
     "Independent Chief Inspector of Borders and Immigration", "Independent Chief Inspector of Borders and Immigration",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-chief-inspector-of-borders-and-immigration.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Chief Inspector of Borders and Immigration\",\"government\",\"batch:50\"]",
     3600,
     "Independent Chief Inspector of Borders and Immigration — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_commission_for_aid_impact, "uk-b50-govuk-independent-commission-for-aid-impact",
     "Independent Commission for Aid Impact", "Independent Commission for Aid Impact",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-commission-for-aid-impact.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Commission for Aid Impact\",\"government\",\"batch:50\"]",
     21600,
     "Independent Commission for Aid Impact — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_independent_commission_on_civil_aviation_noise, "uk-b50-govuk-independent-commission-on-civil-aviation-noise",
     "Independent Commission on Civil Aviation Noise", "Independent Commission on Civil Aviation Noise",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-commission-on-civil-aviation-noise.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Commission on Civil Aviation Noise\",\"government\",\"batch:50\"]",
     604800,
     "Independent Commission on Civil Aviation Noise — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_independent_commission_on_freedom_of_information, "uk-b50-govuk-independent-commission-on-freedom-of-information",
     "Independent Commission on Freedom of Information", "Independent Commission on Freedom of Information",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-commission-on-freedom-of-information.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Commission on Freedom of Information\",\"government\",\"batch:50\"]",
     604800,
     "Independent Commission on Freedom of Information — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_independent_family_returns_panel, "uk-b50-govuk-independent-family-returns-panel",
     "Independent Family Returns Panel", "Independent Family Returns Panel",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-family-returns-panel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Family Returns Panel\",\"government\",\"batch:50\"]",
     3600,
     "Independent Family Returns Panel — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_football_regulator, "uk-b50-govuk-independent-football-regulator",
     "Independent Football Regulator", "Independent Football Regulator",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-football-regulator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Football Regulator\",\"government\",\"batch:50\"]",
     21600,
     "Independent Football Regulator — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_independent_living_fund, "uk-b50-govuk-independent-living-fund",
     "Independent Living Fund", "Independent Living Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-living-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Living Fund\",\"government\",\"batch:50\"]",
     604800,
     "Independent Living Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_monitoring_authority_for_the_citizens_rights_agreements, "uk-b50-govuk-independent-monitoring-authority-for-the-citizens-rights-agreements",
     "Independent Monitoring Authority for the Citizens’ Rights Agreements", "Independent Monitoring Authority for the Citizens’ Rights Agreements",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-monitoring-authority-for-the-citizens-rights-agreements.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Monitoring Authority for the Citizens’ Rights Agreements\",\"government\",\"batch:50\"]",
     21600,
     "Independent Monitoring Authority for the Citizens’ Rights Agreements — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_independent_monitoring_boards_of_prisons_immigration_removal_centres_and_short_term_holding_rooms, "uk-b50-govuk-independent-monitoring-boards-of-prisons-immigration-removal-centres-and-short-term-holding-rooms",
     "Independent Monitoring Boards", "Independent Monitoring Boards",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-monitoring-boards-of-prisons-immigration-removal-centres-and-short-term-holding-rooms.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Monitoring Boards\",\"government\",\"batch:50\"]",
     21600,
     "Independent Monitoring Boards — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_independent_monitoring_commission, "uk-b50-govuk-independent-monitoring-commission",
     "Independent Monitoring Commission", "Independent Monitoring Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-monitoring-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Monitoring Commission\",\"government\",\"batch:50\"]",
     604800,
     "Independent Monitoring Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_office_for_police_conduct, "uk-b50-govuk-independent-office-for-police-conduct",
     "UK Independent Office For Police Conduct", "UK Independent Office For Police Conduct",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-office-for-police-conduct.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Independent Office For Police Conduct\",\"government\",\"batch:50\"]",
     21600,
     "UK Independent Office For Police Conduct — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_reconfiguration_panel, "uk-b50-govuk-independent-reconfiguration-panel",
     "Independent Reconfiguration Panel", "Independent Reconfiguration Panel",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-reconfiguration-panel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Reconfiguration Panel\",\"government\",\"batch:50\"]",
     3600,
     "Independent Reconfiguration Panel — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_review_mechanism, "uk-b50-govuk-independent-review-mechanism",
     "Independent Review Mechanism", "Independent Review Mechanism",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-review-mechanism.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Review Mechanism\",\"government\",\"batch:50\"]",
     3600,
     "Independent Review Mechanism — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_independent_review_of_the_uk_postal_services_sector, "uk-b50-govuk-independent-review-of-the-uk-postal-services-sector",
     "Independent Review of the UK Postal Services Sector", "Independent Review of the UK Postal Services Sector",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-review-of-the-uk-postal-services-sector.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Review of the UK Postal Services Sector\",\"government\",\"batch:50\"]",
     604800,
     "Independent Review of the UK Postal Services Sector — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_independent_reviewer_of_terrorism_legislation, "uk-b50-govuk-independent-reviewer-of-terrorism-legislation",
     "UK Independent Reviewer Of Terrorism Legislation", "UK Independent Reviewer Of Terrorism Legislation",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-reviewer-of-terrorism-legislation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Independent Reviewer Of Terrorism Legislation\",\"government\",\"batch:50\"]",
     21600,
     "UK Independent Reviewer Of Terrorism Legislation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_independent_safeguarding_authority, "uk-b50-govuk-independent-safeguarding-authority",
     "Independent Safeguarding Authority", "Independent Safeguarding Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-safeguarding-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Safeguarding Authority\",\"government\",\"batch:50\"]",
     604800,
     "Independent Safeguarding Authority — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_independent_television_commission, "uk-b50-govuk-independent-television-commission",
     "Independent Television Commission", "Independent Television Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-television-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Television Commission\",\"government\",\"batch:50\"]",
     604800,
     "Independent Television Commission — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_independent_water_commission, "uk-b50-govuk-independent-water-commission",
     "Independent Water Commission", "Independent Water Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/independent-water-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Water Commission\",\"government\",\"batch:50\"]",
     3600,
     "Independent Water Commission — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_industrial_development_advisory_board, "uk-b50-govuk-industrial-development-advisory-board",
     "Industrial Development Advisory Board", "Industrial Development Advisory Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/industrial-development-advisory-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Industrial Development Advisory Board\",\"government\",\"batch:50\"]",
     3600,
     "Industrial Development Advisory Board — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_industrial_injuries_advisory_council, "uk-b50-govuk-industrial-injuries-advisory-council",
     "Industrial Injuries Advisory Council", "Industrial Injuries Advisory Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/industrial-injuries-advisory-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Industrial Injuries Advisory Council\",\"government\",\"batch:50\"]",
     3600,
     "Industrial Injuries Advisory Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_industrial_strategy_advisory_council, "uk-b50-govuk-industrial-strategy-advisory-council",
     "Industrial Strategy Advisory Council", "Industrial Strategy Advisory Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/industrial-strategy-advisory-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Industrial Strategy Advisory Council\",\"government\",\"batch:50\"]",
     3600,
     "Industrial Strategy Advisory Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_infected_blood_compensation_authority, "uk-b50-govuk-infected-blood-compensation-authority",
     "Infected Blood Compensation Authority", "Infected Blood Compensation Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/infected-blood-compensation-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Infected Blood Compensation Authority\",\"government\",\"batch:50\"]",
     21600,
     "Infected Blood Compensation Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_information_commissioner_s_office, "uk-b50-govuk-information-commissioner-s-office",
     "Information Commissioner's Office", "Information Commissioner's Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/information-commissioner-s-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Information Commissioner's Office\",\"government\",\"batch:50\"]",
     21600,
     "Information Commissioner's Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_information_services_division_scotland, "uk-b50-govuk-information-services-division-scotland",
     "Information Services Division (Scotland)", "Information Services Division (Scotland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/information-services-division-scotland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Information Services Division (Scotland)\",\"government\",\"batch:50\"]",
     604800,
     "Information Services Division (Scotland) — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_infrastructure_and_projects_authority, "uk-b50-govuk-infrastructure-and-projects-authority",
     "Infrastructure and Projects Authority", "Infrastructure and Projects Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/infrastructure-and-projects-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Infrastructure and Projects Authority\",\"government\",\"batch:50\"]",
     604800,
     "Infrastructure and Projects Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_infrastructure_planning_commission, "uk-b50-govuk-infrastructure-planning-commission",
     "Infrastructure Planning Commission", "Infrastructure Planning Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/infrastructure-planning-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Infrastructure Planning Commission\",\"government\",\"batch:50\"]",
     604800,
     "Infrastructure Planning Commission — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_infrastructure_uk, "uk-b50-govuk-infrastructure-uk",
     "Infrastructure UK", "Infrastructure UK",
     "government", "government",
     "https://www.gov.uk/government/organisations/infrastructure-uk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Infrastructure UK\",\"government\",\"batch:50\"]",
     604800,
     "Infrastructure UK — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_innovate_uk, "uk-b50-govuk-innovate-uk",
     "Innovate UK", "Innovate UK",
     "government", "government",
     "https://www.gov.uk/government/organisations/innovate-uk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Innovate UK\",\"government\",\"batch:50\"]",
     21600,
     "Innovate UK — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_inquiry_into_the_supervision_of_the_bank_of_credit_and_commerce_international, "uk-b50-govuk-inquiry-into-the-supervision-of-the-bank-of-credit-and-commerce-international",
     "Inquiry into the supervision of the Bank of Credit and Commerce International", "Inquiry into the supervision of the Bank of Credit and Commerce International",
     "government", "government",
     "https://www.gov.uk/government/organisations/inquiry-into-the-supervision-of-the-bank-of-credit-and-commerce-international.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Inquiry into the supervision of the Bank of Credit and Commerce International\",\"government\",\"batch:50\"]",
     604800,
     "Inquiry into the supervision of the Bank of Credit and Commerce International — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_insolvency_rules_committee, "uk-b50-govuk-insolvency-rules-committee",
     "Insolvency Rules Committee", "Insolvency Rules Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/insolvency-rules-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Insolvency Rules Committee\",\"government\",\"batch:50\"]",
     3600,
     "Insolvency Rules Committee — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_institute_for_apprenticeships, "uk-b50-govuk-institute-for-apprenticeships",
     "Institute for Apprenticeships", "Institute for Apprenticeships",
     "government", "government",
     "https://www.gov.uk/government/organisations/institute-for-apprenticeships.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Institute for Apprenticeships\",\"government\",\"batch:50\"]",
     604800,
     "Institute for Apprenticeships — government feed (14 entries at probe)")

RSSX(uk_b50_govuk_institute_for_apprenticeships_and_technical_education, "uk-b50-govuk-institute-for-apprenticeships-and-technical-education",
     "Institute for Apprenticeships and Technical Education", "Institute for Apprenticeships and Technical Education",
     "government", "government",
     "https://www.gov.uk/government/organisations/institute-for-apprenticeships-and-technical-education.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Institute for Apprenticeships and Technical Education\",\"government\",\"batch:50\"]",
     604800,
     "Institute for Apprenticeships and Technical Education — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_intellectual_property_office, "uk-b50-govuk-intellectual-property-office",
     "UK IPO", "UK IPO",
     "government", "government",
     "https://www.gov.uk/government/organisations/intellectual-property-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK IPO\",\"government\",\"batch:50\"]",
     3600,
     "UK IPO — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_interception_of_communications_commissioner, "uk-b50-govuk-interception-of-communications-commissioner",
     "Interception of Communications Commissioner", "Interception of Communications Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/interception-of-communications-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Interception of Communications Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Interception of Communications Commissioner — government feed (17 entries at probe)")

RSSX(uk_b50_govuk_invest_northern_ireland, "uk-b50-govuk-invest-northern-ireland",
     "Invest Northern Ireland", "Invest Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/invest-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Invest Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Invest Northern Ireland — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_investigation_into_the_role_of_jimmy_savile_at_broadmoor_hospital, "uk-b50-govuk-investigation-into-the-role-of-jimmy-savile-at-broadmoor-hospital",
     "Broadmoor Hospital investigation", "Broadmoor Hospital investigation",
     "government", "government",
     "https://www.gov.uk/government/organisations/investigation-into-the-role-of-jimmy-savile-at-broadmoor-hospital.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Broadmoor Hospital investigation\",\"government\",\"batch:50\"]",
     604800,
     "Broadmoor Hospital investigation — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_investigatory_powers_commissioners_office, "uk-b50-govuk-investigatory-powers-commissioners-office",
     "Investigatory Powers Commissioner's Office", "Investigatory Powers Commissioner's Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/investigatory-powers-commissioners-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Investigatory Powers Commissioner's Office\",\"government\",\"batch:50\"]",
     21600,
     "Investigatory Powers Commissioner's Office — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_investors_in_people_uk, "uk-b50-govuk-investors-in-people-uk",
     "Investors in People UK", "Investors in People UK",
     "government", "government",
     "https://www.gov.uk/government/organisations/investors-in-people-uk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Investors in People UK\",\"government\",\"batch:50\"]",
     604800,
     "Investors in People UK — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_iraq_inquiry, "uk-b50-govuk-iraq-inquiry",
     "Iraq Inquiry", "Iraq Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/iraq-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Iraq Inquiry\",\"government\",\"batch:50\"]",
     21600,
     "Iraq Inquiry — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_jeffrey_review, "uk-b50-govuk-jeffrey-review",
     "The Jeffrey Review", "The Jeffrey Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/jeffrey-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Jeffrey Review\",\"government\",\"batch:50\"]",
     604800,
     "The Jeffrey Review — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_joint_forces_command, "uk-b50-govuk-joint-forces-command",
     "UK Joint Forces Command", "UK Joint Forces Command",
     "government", "government",
     "https://www.gov.uk/government/organisations/joint-forces-command.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Joint Forces Command\",\"government\",\"batch:50\"]",
     604800,
     "UK Joint Forces Command — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_joint_nature_conservation_committee, "uk-b50-govuk-joint-nature-conservation-committee",
     "UK Joint Nature Conservation Committee", "UK Joint Nature Conservation Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/joint-nature-conservation-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Joint Nature Conservation Committee\",\"government\",\"batch:50\"]",
     21600,
     "UK Joint Nature Conservation Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_judicial_appointments_commission, "uk-b50-govuk-judicial-appointments-commission",
     "UK Judicial Appointments Commission", "UK Judicial Appointments Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/judicial-appointments-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Judicial Appointments Commission\",\"government\",\"batch:50\"]",
     21600,
     "UK Judicial Appointments Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_judicial_office, "uk-b50-govuk-judicial-office",
     "Judicial Office", "Judicial Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/judicial-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Judicial Office\",\"government\",\"batch:50\"]",
     21600,
     "Judicial Office — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_kent_and_somerset_education_action_zone, "uk-b50-govuk-kent-and-somerset-education-action-zone",
     "Kent and Somerset Education Action Zone", "Kent and Somerset Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/kent-and-somerset-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Kent and Somerset Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Kent and Somerset Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_kerr_haslam_inquiry, "uk-b50-govuk-kerr-haslam-inquiry",
     "Kerr / Haslam Inquiry", "Kerr / Haslam Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/kerr-haslam-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Kerr / Haslam Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Kerr / Haslam Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_kitts_green_and_shard_end_education_action_zone, "uk-b50-govuk-kitts-green-and-shard-end-education-action-zone",
     "Kitts Green and Shard End Education Action Zone", "Kitts Green and Shard End Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/kitts-green-and-shard-end-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Kitts Green and Shard End Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Kitts Green and Shard End Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_labour_market_evidence_group, "uk-b50-govuk-labour-market-evidence-group",
     "Labour Market Evidence Group", "Labour Market Evidence Group",
     "government", "government",
     "https://www.gov.uk/government/organisations/labour-market-evidence-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Labour Market Evidence Group\",\"government\",\"batch:50\"]",
     3600,
     "Labour Market Evidence Group — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_labour_relations_agency, "uk-b50-govuk-labour-relations-agency",
     "Labour Relations Agency (Northern Ireland)", "Labour Relations Agency (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/labour-relations-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Labour Relations Agency (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Labour Relations Agency (Northern Ireland) — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_lake_district_national_park_authority, "uk-b50-govuk-lake-district-national-park-authority",
     "Lake District National Park Authority", "Lake District National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/lake-district-national-park-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Lake District National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "Lake District National Park Authority — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_lammy_review, "uk-b50-govuk-lammy-review",
     "Lammy Review", "Lammy Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/lammy-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Lammy Review\",\"government\",\"batch:50\"]",
     604800,
     "Lammy Review — government feed (15 entries at probe)")

RSSX(uk_b50_govuk_land_registers_of_northern_ireland, "uk-b50-govuk-land-registers-of-northern-ireland",
     "Land Registers of Northern Ireland", "Land Registers of Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/land-registers-of-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Land Registers of Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Land Registers of Northern Ireland — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_law_commission, "uk-b50-govuk-law-commission",
     "UK Law Commission", "UK Law Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/law-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Law Commission\",\"government\",\"batch:50\"]",
     21600,
     "UK Law Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_leadership_college_for_government, "uk-b50-govuk-leadership-college-for-government",
     "Leadership College for Government", "Leadership College for Government",
     "government", "government",
     "https://www.gov.uk/government/organisations/leadership-college-for-government.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Leadership College for Government\",\"government\",\"batch:50\"]",
     3600,
     "Leadership College for Government — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_learning_and_skills_council, "uk-b50-govuk-learning-and-skills-council",
     "Learning and Skills Council", "Learning and Skills Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/learning-and-skills-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Learning and Skills Council\",\"government\",\"batch:50\"]",
     604800,
     "Learning and Skills Council — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_leasehold_advisory_service, "uk-b50-govuk-leasehold-advisory-service",
     "Leasehold Advisory Service", "Leasehold Advisory Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/leasehold-advisory-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Leasehold Advisory Service\",\"government\",\"batch:50\"]",
     21600,
     "Leasehold Advisory Service — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_legal_aid_agency, "uk-b50-govuk-legal-aid-agency",
     "UK Legal Aid Agency", "UK Legal Aid Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/legal-aid-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Legal Aid Agency\",\"government\",\"batch:50\"]",
     3600,
     "UK Legal Aid Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_legal_aid_board, "uk-b50-govuk-legal-aid-board",
     "Legal Aid Board", "Legal Aid Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/legal-aid-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Legal Aid Board\",\"government\",\"batch:50\"]",
     604800,
     "Legal Aid Board — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_legal_services_agency_northern_ireland, "uk-b50-govuk-legal-services-agency-northern-ireland",
     "Legal Services Agency (Northern Ireland)", "Legal Services Agency (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/legal-services-agency-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Legal Services Agency (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Legal Services Agency (Northern Ireland) — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_legal_services_board, "uk-b50-govuk-legal-services-board",
     "UK Legal Services Board", "UK Legal Services Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/legal-services-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Legal Services Board\",\"government\",\"batch:50\"]",
     21600,
     "UK Legal Services Board — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_legal_services_commission, "uk-b50-govuk-legal-services-commission",
     "Legal Services Commission", "Legal Services Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/legal-services-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Legal Services Commission\",\"government\",\"batch:50\"]",
     604800,
     "Legal Services Commission — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_leigh_park_education_action_zone, "uk-b50-govuk-leigh-park-education-action-zone",
     "Leigh Park Education Action Zone", "Leigh Park Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/leigh-park-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Leigh Park Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Leigh Park Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_leitch_review_of_skills, "uk-b50-govuk-leitch-review-of-skills",
     "Leitch Review of Skills", "Leitch Review of Skills",
     "government", "government",
     "https://www.gov.uk/government/organisations/leitch-review-of-skills.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Leitch Review of Skills\",\"government\",\"batch:50\"]",
     604800,
     "Leitch Review of Skills — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_leveson_inquiry, "uk-b50-govuk-leveson-inquiry",
     "Leveson Inquiry", "Leveson Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/leveson-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Leveson Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Leveson Inquiry — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_lgbt_veterans_independent_review, "uk-b50-govuk-lgbt-veterans-independent-review",
     "LGBT Veterans Independent Review", "LGBT Veterans Independent Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/lgbt-veterans-independent-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:LGBT Veterans Independent Review\",\"government\",\"batch:50\"]",
     3600,
     "LGBT Veterans Independent Review — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_life_sentence_review_commissioners, "uk-b50-govuk-life-sentence-review-commissioners",
     "Life Sentence Review Commissioners", "Life Sentence Review Commissioners",
     "government", "government",
     "https://www.gov.uk/government/organisations/life-sentence-review-commissioners.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Life Sentence Review Commissioners\",\"government\",\"batch:50\"]",
     604800,
     "Life Sentence Review Commissioners — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_livestock_and_meat_commission_for_northern_ireland, "uk-b50-govuk-livestock-and-meat-commission-for-northern-ireland",
     "Livestock and Meat Commission for Northern Ireland", "Livestock and Meat Commission for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/livestock-and-meat-commission-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Livestock and Meat Commission for Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Livestock and Meat Commission for Northern Ireland — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_local_better_regulation_office, "uk-b50-govuk-local-better-regulation-office",
     "Local Better Regulation Office", "Local Better Regulation Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/local-better-regulation-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Local Better Regulation Office\",\"government\",\"batch:50\"]",
     604800,
     "Local Better Regulation Office — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_local_government_ombudsman, "uk-b50-govuk-local-government-ombudsman",
     "Local Government and Social Care Ombudsman", "Local Government and Social Care Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/local-government-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Local Government and Social Care Ombudsman\",\"government\",\"batch:50\"]",
     21600,
     "Local Government and Social Care Ombudsman — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_located, "uk-b50-govuk-located",
     "LocatED", "LocatED",
     "government", "government",
     "https://www.gov.uk/government/organisations/located.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:LocatED\",\"government\",\"batch:50\"]",
     604800,
     "LocatED — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_london_and_continental_railways_ltd, "uk-b50-govuk-london-and-continental-railways-ltd",
     "London and Continental Railways Limited", "London and Continental Railways Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/london-and-continental-railways-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:London and Continental Railways Limited\",\"government\",\"batch:50\"]",
     604800,
     "London and Continental Railways Limited — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_london_thames_gateway_development_corporation, "uk-b50-govuk-london-thames-gateway-development-corporation",
     "London Thames Gateway Development Corporation", "London Thames Gateway Development Corporation",
     "government", "government",
     "https://www.gov.uk/government/organisations/london-thames-gateway-development-corporation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:London Thames Gateway Development Corporation\",\"government\",\"batch:50\"]",
     604800,
     "London Thames Gateway Development Corporation — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_lord_chancellors_department, "uk-b50-govuk-lord-chancellors-department",
     "Lord Chancellor's Department", "Lord Chancellor's Department",
     "government", "government",
     "https://www.gov.uk/government/organisations/lord-chancellors-department.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Lord Chancellor's Department\",\"government\",\"batch:50\"]",
     604800,
     "Lord Chancellor's Department — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_loughs_agency, "uk-b50-govuk-loughs-agency",
     "Loughs Agency", "Loughs Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/loughs-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Loughs Agency\",\"government\",\"batch:50\"]",
     604800,
     "Loughs Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_low_carbon_contracts_company, "uk-b50-govuk-low-carbon-contracts-company",
     "Low Carbon Contracts Company", "Low Carbon Contracts Company",
     "government", "government",
     "https://www.gov.uk/government/organisations/low-carbon-contracts-company.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Low Carbon Contracts Company\",\"government\",\"batch:50\"]",
     21600,
     "Low Carbon Contracts Company — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_low_level_waste_repository_ltd, "uk-b50-govuk-low-level-waste-repository-ltd",
     "Low Level Waste Repository Ltd", "Low Level Waste Repository Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/low-level-waste-repository-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Low Level Waste Repository Ltd\",\"government\",\"batch:50\"]",
     604800,
     "Low Level Waste Repository Ltd — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_low_pay_commission, "uk-b50-govuk-low-pay-commission",
     "UK Low Pay Commission", "UK Low Pay Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/low-pay-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Low Pay Commission\",\"government\",\"batch:50\"]",
     3600,
     "UK Low Pay Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_lyons_inquiry_into_local_government, "uk-b50-govuk-lyons-inquiry-into-local-government",
     "Lyons Inquiry into Local Government", "Lyons Inquiry into Local Government",
     "government", "government",
     "https://www.gov.uk/government/organisations/lyons-inquiry-into-local-government.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Lyons Inquiry into Local Government\",\"government\",\"batch:50\"]",
     604800,
     "Lyons Inquiry into Local Government — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_macur_review, "uk-b50-govuk-macur-review",
     "Macur Review", "Macur Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/macur-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Macur Review\",\"government\",\"batch:50\"]",
     604800,
     "Macur Review — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_magnox_ltd, "uk-b50-govuk-magnox-ltd",
     "Magnox Ltd", "Magnox Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/magnox-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Magnox Ltd\",\"government\",\"batch:50\"]",
     604800,
     "Magnox Ltd — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_marine_accident_investigation_branch, "uk-b50-govuk-marine-accident-investigation-branch",
     "UK Marine Accident Investigation Branch", "UK Marine Accident Investigation Branch",
     "government", "government",
     "https://www.gov.uk/government/organisations/marine-accident-investigation-branch.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Marine Accident Investigation Branch\",\"government\",\"batch:50\"]",
     3600,
     "UK Marine Accident Investigation Branch — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_marine_fisheries_agency, "uk-b50-govuk-marine-fisheries-agency",
     "Marine Fisheries Agency", "Marine Fisheries Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/marine-fisheries-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Marine Fisheries Agency\",\"government\",\"batch:50\"]",
     604800,
     "Marine Fisheries Agency — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_marshall_aid_commemoration_commission, "uk-b50-govuk-marshall-aid-commemoration-commission",
     "Marshall Aid Commemoration Commission", "Marshall Aid Commemoration Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/marshall-aid-commemoration-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Marshall Aid Commemoration Commission\",\"government\",\"batch:50\"]",
     21600,
     "Marshall Aid Commemoration Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_meat_and_livestock_commission, "uk-b50-govuk-meat-and-livestock-commission",
     "Meat and Livestock Commission", "Meat and Livestock Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/meat-and-livestock-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Meat and Livestock Commission\",\"government\",\"batch:50\"]",
     604800,
     "Meat and Livestock Commission — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_meat_hygiene_service, "uk-b50-govuk-meat-hygiene-service",
     "Meat Hygiene Service", "Meat Hygiene Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/meat-hygiene-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Meat Hygiene Service\",\"government\",\"batch:50\"]",
     604800,
     "Meat Hygiene Service — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_medical_research_council, "uk-b50-govuk-medical-research-council",
     "UK Medical Research Council", "UK Medical Research Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/medical-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Medical Research Council\",\"government\",\"batch:50\"]",
     21600,
     "UK Medical Research Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_migration_advisory_committee, "uk-b50-govuk-migration-advisory-committee",
     "UK Migration Advisory Committee", "UK Migration Advisory Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/migration-advisory-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Migration Advisory Committee\",\"government\",\"batch:50\"]",
     3600,
     "UK Migration Advisory Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_milk_development_council, "uk-b50-govuk-milk-development-council",
     "Milk Development Council", "Milk Development Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/milk-development-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Milk Development Council\",\"government\",\"batch:50\"]",
     604800,
     "Milk Development Council — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_millennium_commission, "uk-b50-govuk-millennium-commission",
     "Millennium Commission", "Millennium Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/millennium-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Millennium Commission\",\"government\",\"batch:50\"]",
     604800,
     "Millennium Commission — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_mining_remediation_authority, "uk-b50-govuk-mining-remediation-authority",
     "Mining Remediation Authority", "Mining Remediation Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/mining-remediation-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Mining Remediation Authority\",\"government\",\"batch:50\"]",
     3600,
     "Mining Remediation Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_ministry_of_housing_communities_and_local_government_2018_2021, "uk-b50-govuk-ministry-of-housing-communities-and-local-government-2018-2021",
     "Ministry of Housing, Communities & Local Government (2018 to 2021)", "Ministry of Housing, Communities & Local Government (2018 to 2021)",
     "government", "government",
     "https://www.gov.uk/government/organisations/ministry-of-housing-communities-and-local-government-2018-2021.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ministry of Housing, Communities & Local Government (2018 to 2021)\",\"government\",\"batch:50\"]",
     604800,
     "Ministry of Housing, Communities & Local Government (2018 to 2021) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_modernisation_and_reform, "uk-b50-govuk-modernisation-and-reform",
     "Modernisation and Reform", "Modernisation and Reform",
     "government", "government",
     "https://www.gov.uk/government/organisations/modernisation-and-reform.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Modernisation and Reform\",\"government\",\"batch:50\"]",
     3600,
     "Modernisation and Reform — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_monitor, "uk-b50-govuk-monitor",
     "Monitor", "Monitor",
     "government", "government",
     "https://www.gov.uk/government/organisations/monitor.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Monitor\",\"government\",\"batch:50\"]",
     604800,
     "Monitor — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_monopolies_and_mergers_commission, "uk-b50-govuk-monopolies-and-mergers-commission",
     "Monopolies and Mergers Commission", "Monopolies and Mergers Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/monopolies-and-mergers-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Monopolies and Mergers Commission\",\"government\",\"batch:50\"]",
     604800,
     "Monopolies and Mergers Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_morecambe_bay_investigation, "uk-b50-govuk-morecambe-bay-investigation",
     "Morecambe Bay Investigation", "Morecambe Bay Investigation",
     "government", "government",
     "https://www.gov.uk/government/organisations/morecambe-bay-investigation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Morecambe Bay Investigation\",\"government\",\"batch:50\"]",
     604800,
     "Morecambe Bay Investigation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_mull_of_kintyre_review, "uk-b50-govuk-mull-of-kintyre-review",
     "Mull of Kintyre Review", "Mull of Kintyre Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/mull-of-kintyre-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Mull of Kintyre Review\",\"government\",\"batch:50\"]",
     604800,
     "Mull of Kintyre Review — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_museums_libraries_and_archives_council, "uk-b50-govuk-museums-libraries-and-archives-council",
     "Museums, Libraries and Archives Council", "Museums, Libraries and Archives Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/museums-libraries-and-archives-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Museums, Libraries and Archives Council\",\"government\",\"batch:50\"]",
     604800,
     "Museums, Libraries and Archives Council — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_national_army_museum, "uk-b50-govuk-national-army-museum",
     "UK National Army Museum", "UK National Army Museum",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-army-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK National Army Museum\",\"government\",\"batch:50\"]",
     21600,
     "UK National Army Museum — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_national_biological_standards_board, "uk-b50-govuk-national-biological-standards-board",
     "National Biological Standards Board", "National Biological Standards Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-biological-standards-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Biological Standards Board\",\"government\",\"batch:50\"]",
     604800,
     "National Biological Standards Board — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_national_blood_authority, "uk-b50-govuk-national-blood-authority",
     "National Blood Authority", "National Blood Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-blood-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Blood Authority\",\"government\",\"batch:50\"]",
     604800,
     "National Blood Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_national_care_standards_commission, "uk-b50-govuk-national-care-standards-commission",
     "National Care Standards Commission", "National Care Standards Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-care-standards-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Care Standards Commission\",\"government\",\"batch:50\"]",
     604800,
     "National Care Standards Commission — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_national_citizen_service, "uk-b50-govuk-national-citizen-service",
     "National Citizen Service", "National Citizen Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-citizen-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Citizen Service\",\"government\",\"batch:50\"]",
     604800,
     "National Citizen Service — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_national_college_for_school_leadership, "uk-b50-govuk-national-college-for-school-leadership",
     "National College for School Leadership", "National College for School Leadership",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-college-for-school-leadership.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National College for School Leadership\",\"government\",\"batch:50\"]",
     604800,
     "National College for School Leadership — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_national_college_for_teaching_and_leadership, "uk-b50-govuk-national-college-for-teaching-and-leadership",
     "National College for Teaching and Leadership", "National College for Teaching and Leadership",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-college-for-teaching-and-leadership.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National College for Teaching and Leadership\",\"government\",\"batch:50\"]",
     604800,
     "National College for Teaching and Leadership — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_counter_terrorism_security_office, "uk-b50-govuk-national-counter-terrorism-security-office",
     "National Counter Terrorism Security Office", "National Counter Terrorism Security Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-counter-terrorism-security-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Counter Terrorism Security Office\",\"government\",\"batch:50\"]",
     21600,
     "National Counter Terrorism Security Office — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_national_cyber_force, "uk-b50-govuk-national-cyber-force",
     "National Cyber Force", "National Cyber Force",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-cyber-force.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Cyber Force\",\"government\",\"batch:50\"]",
     3600,
     "National Cyber Force — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_national_employer_advisory_board, "uk-b50-govuk-national-employer-advisory-board",
     "National Employer Advisory Board", "National Employer Advisory Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-employer-advisory-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Employer Advisory Board\",\"government\",\"batch:50\"]",
     604800,
     "National Employer Advisory Board — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_national_employment_savings_trust, "uk-b50-govuk-national-employment-savings-trust",
     "National Employment Savings Trust (NEST) Corporation", "National Employment Savings Trust (NEST) Corporation",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-employment-savings-trust.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Employment Savings Trust (NEST) Corporation\",\"government\",\"batch:50\"]",
     21600,
     "National Employment Savings Trust (NEST) Corporation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_energy_system_operator, "uk-b50-govuk-national-energy-system-operator",
     "National Energy System Operator", "National Energy System Operator",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-energy-system-operator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Energy System Operator\",\"government\",\"batch:50\"]",
     21600,
     "National Energy System Operator — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_national_forest_company, "uk-b50-govuk-national-forest-company",
     "National Forest Company", "National Forest Company",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-forest-company.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Forest Company\",\"government\",\"batch:50\"]",
     21600,
     "National Forest Company — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_fraud_authority, "uk-b50-govuk-national-fraud-authority",
     "National Fraud Authority", "National Fraud Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-fraud-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Fraud Authority\",\"government\",\"batch:50\"]",
     604800,
     "National Fraud Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_gallery, "uk-b50-govuk-national-gallery",
     "National Gallery", "National Gallery",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-gallery.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Gallery\",\"government\",\"batch:50\"]",
     21600,
     "National Gallery — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_guardians_office, "uk-b50-govuk-national-guardians-office",
     "National Guardian's Office", "National Guardian's Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-guardians-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Guardian's Office\",\"government\",\"batch:50\"]",
     21600,
     "National Guardian's Office — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_national_heritage_memorial_fund, "uk-b50-govuk-national-heritage-memorial-fund",
     "National Heritage Memorial Fund", "National Heritage Memorial Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-heritage-memorial-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Heritage Memorial Fund\",\"government\",\"batch:50\"]",
     21600,
     "National Heritage Memorial Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_highways, "uk-b50-govuk-national-highways",
     "National Highways", "National Highways",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-highways.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Highways\",\"government\",\"batch:50\"]",
     21600,
     "National Highways — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_housing_bank, "uk-b50-govuk-national-housing-bank",
     "National Housing Bank", "National Housing Bank",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-housing-bank.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Housing Bank\",\"government\",\"batch:50\"]",
     3600,
     "National Housing Bank — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_information_board, "uk-b50-govuk-national-information-board",
     "National Information Board", "National Information Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-information-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Information Board\",\"government\",\"batch:50\"]",
     604800,
     "National Information Board — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_infrastructure_and_service_transformation_authority, "uk-b50-govuk-national-infrastructure-and-service-transformation-authority",
     "National Infrastructure and Service Transformation Authority", "National Infrastructure and Service Transformation Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-infrastructure-and-service-transformation-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Infrastructure and Service Transformation Authority\",\"government\",\"batch:50\"]",
     3600,
     "National Infrastructure and Service Transformation Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_infrastructure_commission, "uk-b50-govuk-national-infrastructure-commission",
     "National Infrastructure Commission", "National Infrastructure Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-infrastructure-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Infrastructure Commission\",\"government\",\"batch:50\"]",
     604800,
     "National Infrastructure Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_leadership_centre, "uk-b50-govuk-national-leadership-centre",
     "National Leadership Centre", "National Leadership Centre",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-leadership-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Leadership Centre\",\"government\",\"batch:50\"]",
     604800,
     "National Leadership Centre — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_national_lottery_heritage_fund, "uk-b50-govuk-national-lottery-heritage-fund",
     "National Lottery Heritage Fund", "National Lottery Heritage Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-lottery-heritage-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Lottery Heritage Fund\",\"government\",\"batch:50\"]",
     21600,
     "National Lottery Heritage Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_measurement_and_regulation_office, "uk-b50-govuk-national-measurement-and-regulation-office",
     "National Measurement and Regulation Office", "National Measurement and Regulation Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-measurement-and-regulation-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Measurement and Regulation Office\",\"government\",\"batch:50\"]",
     604800,
     "National Measurement and Regulation Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_measurement_office, "uk-b50-govuk-national-measurement-office",
     "National Measurement Office", "National Measurement Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-measurement-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Measurement Office\",\"government\",\"batch:50\"]",
     604800,
     "National Measurement Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_museums_and_galleries_of_northern_ireland, "uk-b50-govuk-national-museums-and-galleries-of-northern-ireland",
     "National Museums and Galleries of Northern Ireland", "National Museums and Galleries of Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-museums-and-galleries-of-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Museums and Galleries of Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "National Museums and Galleries of Northern Ireland — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_national_museums_liverpool, "uk-b50-govuk-national-museums-liverpool",
     "National Museums Liverpool", "National Museums Liverpool",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-museums-liverpool.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Museums Liverpool\",\"government\",\"batch:50\"]",
     21600,
     "National Museums Liverpool — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_museums_of_scotland, "uk-b50-govuk-national-museums-of-scotland",
     "National Museums of Scotland", "National Museums of Scotland",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-museums-of-scotland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Museums of Scotland\",\"government\",\"batch:50\"]",
     604800,
     "National Museums of Scotland — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_national_nuclear_laboratory, "uk-b50-govuk-national-nuclear-laboratory",
     "UK National Nuclear Laboratory", "UK National Nuclear Laboratory",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-nuclear-laboratory.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK National Nuclear Laboratory\",\"government\",\"batch:50\"]",
     604800,
     "UK National Nuclear Laboratory — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_national_offender_management_service, "uk-b50-govuk-national-offender-management-service",
     "National Offender Management Service", "National Offender Management Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-offender-management-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Offender Management Service\",\"government\",\"batch:50\"]",
     604800,
     "National Offender Management Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_patient_safety_agency, "uk-b50-govuk-national-patient-safety-agency",
     "National Patient Safety Agency", "National Patient Safety Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-patient-safety-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Patient Safety Agency\",\"government\",\"batch:50\"]",
     604800,
     "National Patient Safety Agency — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_national_physical_laboratory, "uk-b50-govuk-national-physical-laboratory",
     "National Physical Laboratory", "National Physical Laboratory",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-physical-laboratory.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Physical Laboratory\",\"government\",\"batch:50\"]",
     21600,
     "National Physical Laboratory — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_national_policing_improvement_agency, "uk-b50-govuk-national-policing-improvement-agency",
     "National Policing Improvement Agency", "National Policing Improvement Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-policing-improvement-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Policing Improvement Agency\",\"government\",\"batch:50\"]",
     604800,
     "National Policing Improvement Agency — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_national_portrait_gallery, "uk-b50-govuk-national-portrait-gallery",
     "National Portrait Gallery", "National Portrait Gallery",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-portrait-gallery.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Portrait Gallery\",\"government\",\"batch:50\"]",
     21600,
     "National Portrait Gallery — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_protective_security_authority, "uk-b50-govuk-national-protective-security-authority",
     "National Protective Security Authority", "National Protective Security Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-protective-security-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Protective Security Authority\",\"government\",\"batch:50\"]",
     21600,
     "National Protective Security Authority — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_national_radiological_protection_board, "uk-b50-govuk-national-radiological-protection-board",
     "National Radiological Protection Board", "National Radiological Protection Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-radiological-protection-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Radiological Protection Board\",\"government\",\"batch:50\"]",
     604800,
     "National Radiological Protection Board — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_national_records_of_scotland, "uk-b50-govuk-national-records-of-scotland",
     "National Records of Scotland", "National Records of Scotland",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-records-of-scotland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Records of Scotland\",\"government\",\"batch:50\"]",
     604800,
     "National Records of Scotland — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_school_of_government, "uk-b50-govuk-national-school-of-government",
     "National School of Government", "National School of Government",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-school-of-government.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National School of Government\",\"government\",\"batch:50\"]",
     604800,
     "National School of Government — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_national_treatment_agency_for_substance_misuse, "uk-b50-govuk-national-treatment-agency-for-substance-misuse",
     "National Treatment Agency for Substance Misuse", "National Treatment Agency for Substance Misuse",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-treatment-agency-for-substance-misuse.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Treatment Agency for Substance Misuse\",\"government\",\"batch:50\"]",
     604800,
     "National Treatment Agency for Substance Misuse — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_national_wealth_fund, "uk-b50-govuk-national-wealth-fund",
     "National Wealth Fund", "National Wealth Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-wealth-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Wealth Fund\",\"government\",\"batch:50\"]",
     3600,
     "National Wealth Fund — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_national_weights_and_measures_laboratory, "uk-b50-govuk-national-weights-and-measures-laboratory",
     "National Weights and Measures Laboratory", "National Weights and Measures Laboratory",
     "government", "government",
     "https://www.gov.uk/government/organisations/national-weights-and-measures-laboratory.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Weights and Measures Laboratory\",\"government\",\"batch:50\"]",
     604800,
     "National Weights and Measures Laboratory — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_natural_environment_research_council, "uk-b50-govuk-natural-environment-research-council",
     "UK Natural Environment Research Council", "UK Natural Environment Research Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/natural-environment-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Natural Environment Research Council\",\"government\",\"batch:50\"]",
     21600,
     "UK Natural Environment Research Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_nature_conservancy_council, "uk-b50-govuk-nature-conservancy-council",
     "Nature Conservancy Council", "Nature Conservancy Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/nature-conservancy-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nature Conservancy Council\",\"government\",\"batch:50\"]",
     604800,
     "Nature Conservancy Council — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_network_rail, "uk-b50-govuk-network-rail",
     "UK Network Rail", "UK Network Rail",
     "government", "government",
     "https://www.gov.uk/government/organisations/network-rail.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Network Rail\",\"government\",\"batch:50\"]",
     21600,
     "UK Network Rail — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_new_forest_national_park_authority, "uk-b50-govuk-new-forest-national-park-authority",
     "New Forest National Park Authority", "New Forest National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/new-forest-national-park-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:New Forest National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "New Forest National Park Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_new_opportunities_fund, "uk-b50-govuk-new-opportunities-fund",
     "New Opportunities Fund", "New Opportunities Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/new-opportunities-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:New Opportunities Fund\",\"government\",\"batch:50\"]",
     604800,
     "New Opportunities Fund — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_nhs_business_services_authority, "uk-b50-govuk-nhs-business-services-authority",
     "UK Nhs Business Services Authority", "UK Nhs Business Services Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/nhs-business-services-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Nhs Business Services Authority\",\"government\",\"batch:50\"]",
     21600,
     "UK Nhs Business Services Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_nhs_england, "uk-b50-govuk-nhs-england",
     "UK Nhs England", "UK Nhs England",
     "government", "government",
     "https://www.gov.uk/government/organisations/nhs-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Nhs England\",\"government\",\"batch:50\"]",
     21600,
     "UK Nhs England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_nimrod_review, "uk-b50-govuk-nimrod-review",
     "Nimrod Review", "Nimrod Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/nimrod-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nimrod Review\",\"government\",\"batch:50\"]",
     604800,
     "Nimrod Review — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_north_east_derbyshire_coalfields_education_action_zone, "uk-b50-govuk-north-east-derbyshire-coalfields-education-action-zone",
     "North East Derbyshire Coalfields Education Action Zone", "North East Derbyshire Coalfields Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-east-derbyshire-coalfields-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North East Derbyshire Coalfields Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "North East Derbyshire Coalfields Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_north_east_education_and_library_board, "uk-b50-govuk-north-east-education-and-library-board",
     "North East Education and Library Board", "North East Education and Library Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-east-education-and-library-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North East Education and Library Board\",\"government\",\"batch:50\"]",
     604800,
     "North East Education and Library Board — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_north_gillingham_education_action_zone, "uk-b50-govuk-north-gillingham-education-action-zone",
     "North Gillingham Education Action Zone", "North Gillingham Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-gillingham-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North Gillingham Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "North Gillingham Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_north_islington_education_action_zone, "uk-b50-govuk-north-islington-education-action-zone",
     "North Islington Education Action Zone", "North Islington Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-islington-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North Islington Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "North Islington Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_north_sea_transition_authority, "uk-b50-govuk-north-sea-transition-authority",
     "North Sea Transition Authority", "North Sea Transition Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-sea-transition-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North Sea Transition Authority\",\"government\",\"batch:50\"]",
     21600,
     "North Sea Transition Authority — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_north_stockton_education_action_zone, "uk-b50-govuk-north-stockton-education-action-zone",
     "North Stockton Education Action Zone", "North Stockton Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-stockton-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North Stockton Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "North Stockton Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_north_west_development_agency, "uk-b50-govuk-north-west-development-agency",
     "North West Development Agency", "North West Development Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-west-development-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North West Development Agency\",\"government\",\"batch:50\"]",
     604800,
     "North West Development Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_north_west_shropshire_education_action_zone, "uk-b50-govuk-north-west-shropshire-education-action-zone",
     "North West Shropshire Education Action Zone", "North West Shropshire Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-west-shropshire-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North West Shropshire Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "North West Shropshire Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_north_york_moors_national_park, "uk-b50-govuk-north-york-moors-national-park",
     "North York Moors National Park Authority", "North York Moors National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/north-york-moors-national-park.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:North York Moors National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "North York Moors National Park Authority — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_audit_office, "uk-b50-govuk-northern-ireland-audit-office",
     "Northern Ireland Audit Office", "Northern Ireland Audit Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-audit-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Audit Office\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Audit Office — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_authority_for_energy_regulation, "uk-b50-govuk-northern-ireland-authority-for-energy-regulation",
     "Northern Ireland Authority for Energy Regulation", "Northern Ireland Authority for Energy Regulation",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-authority-for-energy-regulation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Authority for Energy Regulation\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Authority for Energy Regulation — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_cancer_registry, "uk-b50-govuk-northern-ireland-cancer-registry",
     "Northern Ireland Cancer Registry", "Northern Ireland Cancer Registry",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-cancer-registry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Cancer Registry\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Cancer Registry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_child_support_agency, "uk-b50-govuk-northern-ireland-child-support-agency",
     "Northern Ireland Child Support Agency", "Northern Ireland Child Support Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-child-support-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Child Support Agency\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Child Support Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_commissioner_for_children_and_young_people, "uk-b50-govuk-northern-ireland-commissioner-for-children-and-young-people",
     "Northern Ireland Commissioner for Children and Young People", "Northern Ireland Commissioner for Children and Young People",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-commissioner-for-children-and-young-people.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Commissioner for Children and Young People\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Commissioner for Children and Young People — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_council_for_the_curriculum_examinations_and_assessment, "uk-b50-govuk-northern-ireland-council-for-the-curriculum-examinations-and-assessment",
     "Northern Ireland Council for the Curriculum, Examinations and Assessment", "Northern Ireland Council for the Curriculum, Examinations and Assessment",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-council-for-the-curriculum-examinations-and-assessment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Council for the Curriculum, Examinations and Assessment\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Council for the Curriculum, Examinations and Assessment — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_court_service, "uk-b50-govuk-northern-ireland-court-service",
     "Northern Ireland Courts and Tribunals Service", "Northern Ireland Courts and Tribunals Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-court-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Courts and Tribunals Service\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Courts and Tribunals Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_executive, "uk-b50-govuk-northern-ireland-executive",
     "Northern Ireland Executive", "Northern Ireland Executive",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-executive.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Executive\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Executive — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_housing_executive, "uk-b50-govuk-northern-ireland-housing-executive",
     "Northern Ireland Housing Executive", "Northern Ireland Housing Executive",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-housing-executive.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Housing Executive\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Housing Executive — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_human_rights_commission, "uk-b50-govuk-northern-ireland-human-rights-commission",
     "Northern Ireland Human Rights Commission", "Northern Ireland Human Rights Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-human-rights-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Human Rights Commission\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Human Rights Commission — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_judicial_appointments_commission, "uk-b50-govuk-northern-ireland-judicial-appointments-commission",
     "Northern Ireland Judicial Appointments Commission", "Northern Ireland Judicial Appointments Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-judicial-appointments-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Judicial Appointments Commission\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Judicial Appointments Commission — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_judicial_appointments_ombudsman, "uk-b50-govuk-northern-ireland-judicial-appointments-ombudsman",
     "Northern Ireland Judicial Appointments Ombudsman", "Northern Ireland Judicial Appointments Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-judicial-appointments-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Judicial Appointments Ombudsman\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Judicial Appointments Ombudsman — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_law_commission, "uk-b50-govuk-northern-ireland-law-commission",
     "Northern Ireland Law Commission", "Northern Ireland Law Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-law-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Law Commission\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Law Commission — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_legal_services_commission, "uk-b50-govuk-northern-ireland-legal-services-commission",
     "Northern Ireland Legal Services Commission", "Northern Ireland Legal Services Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-legal-services-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Legal Services Commission\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Legal Services Commission — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_local_government_officers_superannuation_committee, "uk-b50-govuk-northern-ireland-local-government-officers-superannuation-committee",
     "Northern Ireland Local Government Officers' Superannuation Committee", "Northern Ireland Local Government Officers' Superannuation Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-local-government-officers-superannuation-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Local Government Officers' Superannuation Committee\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Local Government Officers' Superannuation Committee — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_office, "uk-b50-govuk-northern-ireland-office",
     "UK Northern Ireland Office", "UK Northern Ireland Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Northern Ireland Office\",\"government\",\"batch:50\"]",
     3600,
     "UK Northern Ireland Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_ombudsman, "uk-b50-govuk-northern-ireland-ombudsman",
     "Northern Ireland Ombudsman", "Northern Ireland Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Ombudsman\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Ombudsman — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_policing_board, "uk-b50-govuk-northern-ireland-policing-board",
     "Northern Ireland Policing Board", "Northern Ireland Policing Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-policing-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Policing Board\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Policing Board — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_practice_and_education_council, "uk-b50-govuk-northern-ireland-practice-and-education-council",
     "Northern Ireland Practice and Education Council", "Northern Ireland Practice and Education Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-practice-and-education-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Practice and Education Council\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Practice and Education Council — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_prison_service, "uk-b50-govuk-northern-ireland-prison-service",
     "Northern Ireland Prison Service", "Northern Ireland Prison Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-prison-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Prison Service\",\"government\",\"batch:50\"]",
     21600,
     "Northern Ireland Prison Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_social_care_council, "uk-b50-govuk-northern-ireland-social-care-council",
     "Northern Ireland Social Care Council", "Northern Ireland Social Care Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-social-care-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Social Care Council\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Social Care Council — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_social_security_agency, "uk-b50-govuk-northern-ireland-social-security-agency",
     "Northern Ireland Social Security Agency", "Northern Ireland Social Security Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-social-security-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Social Security Agency\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Social Security Agency — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_tourist_board, "uk-b50-govuk-northern-ireland-tourist-board",
     "Northern Ireland Tourist Board", "Northern Ireland Tourist Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-ireland-tourist-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Tourist Board\",\"government\",\"batch:50\"]",
     604800,
     "Northern Ireland Tourist Board — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_lighthouse_board, "uk-b50-govuk-northern-lighthouse-board",
     "UK Northern Lighthouse Board", "UK Northern Lighthouse Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/northern-lighthouse-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Northern Lighthouse Board\",\"government\",\"batch:50\"]",
     21600,
     "UK Northern Lighthouse Board — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_northumberland_national_park_authority, "uk-b50-govuk-northumberland-national-park-authority",
     "Northumberland National Park Authority", "Northumberland National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/northumberland-national-park-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northumberland National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "Northumberland National Park Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_northwest_business_link, "uk-b50-govuk-northwest-business-link",
     "Northwest Business Link", "Northwest Business Link",
     "government", "government",
     "https://www.gov.uk/government/organisations/northwest-business-link.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northwest Business Link\",\"government\",\"batch:50\"]",
     604800,
     "Northwest Business Link — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_northwest_regional_development_agency, "uk-b50-govuk-northwest-regional-development-agency",
     "Northwest Regional Development Agency", "Northwest Regional Development Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/northwest-regional-development-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northwest Regional Development Agency\",\"government\",\"batch:50\"]",
     604800,
     "Northwest Regional Development Agency — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_nsandi, "uk-b50-govuk-nsandi",
     "National Savings and Investments", "National Savings and Investments",
     "government", "government",
     "https://www.gov.uk/government/organisations/nsandi.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Savings and Investments\",\"government\",\"batch:50\"]",
     21600,
     "National Savings and Investments — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_nuclear_decommissioning_authority, "uk-b50-govuk-nuclear-decommissioning-authority",
     "UK NDA", "UK NDA",
     "government", "government",
     "https://www.gov.uk/government/organisations/nuclear-decommissioning-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK NDA\",\"government\",\"batch:50\"]",
     3600,
     "UK NDA — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_oak_national_academy, "uk-b50-govuk-oak-national-academy",
     "Oak National Academy", "Oak National Academy",
     "government", "government",
     "https://www.gov.uk/government/organisations/oak-national-academy.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Oak National Academy\",\"government\",\"batch:50\"]",
     21600,
     "Oak National Academy — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_ofcom, "uk-b50-govuk-ofcom",
     "UK Ofcom", "UK Ofcom",
     "government", "government",
     "https://www.gov.uk/government/organisations/ofcom.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Ofcom\",\"government\",\"batch:50\"]",
     21600,
     "UK Ofcom — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_civil_society, "uk-b50-govuk-office-for-civil-society",
     "Office for Civil Society", "Office for Civil Society",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-civil-society.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Civil Society\",\"government\",\"batch:50\"]",
     604800,
     "Office for Civil Society — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_criminal_justice_reform, "uk-b50-govuk-office-for-criminal-justice-reform",
     "Office for Criminal Justice Reform", "Office for Criminal Justice Reform",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-criminal-justice-reform.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Criminal Justice Reform\",\"government\",\"batch:50\"]",
     604800,
     "Office for Criminal Justice Reform — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_office_for_digital_identities_and_attributes, "uk-b50-govuk-office-for-digital-identities-and-attributes",
     "Office for Digital Identities and Attributes", "Office for Digital Identities and Attributes",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-digital-identities-and-attributes.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Digital Identities and Attributes\",\"government\",\"batch:50\"]",
     3600,
     "Office for Digital Identities and Attributes — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_disability_issues, "uk-b50-govuk-office-for-disability-issues",
     "Office for Disability Issues", "Office for Disability Issues",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-disability-issues.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Disability Issues\",\"government\",\"batch:50\"]",
     604800,
     "Office for Disability Issues — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_equality_and_opportunity, "uk-b50-govuk-office-for-equality-and-opportunity",
     "Office for Equality and Opportunity", "Office for Equality and Opportunity",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-equality-and-opportunity.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Equality and Opportunity\",\"government\",\"batch:50\"]",
     3600,
     "Office for Equality and Opportunity — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_fair_access, "uk-b50-govuk-office-for-fair-access",
     "Office for Fair Access", "Office for Fair Access",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-fair-access.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Fair Access\",\"government\",\"batch:50\"]",
     604800,
     "Office for Fair Access — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_office_for_low_emission_vehicles, "uk-b50-govuk-office-for-low-emission-vehicles",
     "Office for Low Emission Vehicles", "Office for Low Emission Vehicles",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-low-emission-vehicles.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Low Emission Vehicles\",\"government\",\"batch:50\"]",
     604800,
     "Office for Low Emission Vehicles — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_nuclear_regulation, "uk-b50-govuk-office-for-nuclear-regulation",
     "UK ONR", "UK ONR",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-nuclear-regulation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK ONR\",\"government\",\"batch:50\"]",
     21600,
     "UK ONR — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_place, "uk-b50-govuk-office-for-place",
     "Office for Place", "Office for Place",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-place.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Place\",\"government\",\"batch:50\"]",
     604800,
     "Office for Place — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_product_safety_and_standards, "uk-b50-govuk-office-for-product-safety-and-standards",
     "UK Office For Product Safety And Standards", "UK Office For Product Safety And Standards",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-product-safety-and-standards.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Office For Product Safety And Standards\",\"government\",\"batch:50\"]",
     3600,
     "UK Office For Product Safety And Standards — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_responsible_business_conduct, "uk-b50-govuk-office-for-responsible-business-conduct",
     "Office for Responsible Business Conduct", "Office for Responsible Business Conduct",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-responsible-business-conduct.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Responsible Business Conduct\",\"government\",\"batch:50\"]",
     3600,
     "Office for Responsible Business Conduct — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_students, "uk-b50-govuk-office-for-students",
     "UK Office For Students", "UK Office For Students",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-students.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Office For Students\",\"government\",\"batch:50\"]",
     21600,
     "UK Office For Students — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_tenants_and_social_landlords, "uk-b50-govuk-office-for-tenants-and-social-landlords",
     "Office for Tenants and Social Landlords", "Office for Tenants and Social Landlords",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-tenants-and-social-landlords.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Tenants and Social Landlords\",\"government\",\"batch:50\"]",
     604800,
     "Office for Tenants and Social Landlords — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_office_for_the_independent_examiner_of_complaints, "uk-b50-govuk-office-for-the-independent-examiner-of-complaints",
     "Office for the Independent Examiner of Complaints", "Office for the Independent Examiner of Complaints",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-the-independent-examiner-of-complaints.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for the Independent Examiner of Complaints\",\"government\",\"batch:50\"]",
     3600,
     "Office for the Independent Examiner of Complaints — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_office_for_the_internal_market, "uk-b50-govuk-office-for-the-internal-market",
     "Office for the Internal Market", "Office for the Internal Market",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-the-internal-market.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for the Internal Market\",\"government\",\"batch:50\"]",
     3600,
     "Office for the Internal Market — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_the_pay_review_bodies, "uk-b50-govuk-office-for-the-pay-review-bodies",
     "Office for the Pay Review Bodies", "Office for the Pay Review Bodies",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-the-pay-review-bodies.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for the Pay Review Bodies\",\"government\",\"batch:50\"]",
     3600,
     "Office for the Pay Review Bodies — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_veterans_affairs, "uk-b50-govuk-office-for-veterans-affairs",
     "Office for Veterans' Affairs", "Office for Veterans' Affairs",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-veterans-affairs.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Veterans' Affairs\",\"government\",\"batch:50\"]",
     3600,
     "Office for Veterans' Affairs — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_zero_emission_vehicles, "uk-b50-govuk-office-for-zero-emission-vehicles",
     "Office for Zero Emission Vehicles", "Office for Zero Emission Vehicles",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-for-zero-emission-vehicles.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Zero Emission Vehicles\",\"government\",\"batch:50\"]",
     3600,
     "Office for Zero Emission Vehicles — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_fair_trading, "uk-b50-govuk-office-of-fair-trading",
     "Office of Fair Trading", "Office of Fair Trading",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-fair-trading.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of Fair Trading\",\"government\",\"batch:50\"]",
     604800,
     "Office of Fair Trading — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_financial_sanctions_implementation, "uk-b50-govuk-office-of-financial-sanctions-implementation",
     "Office of Financial Sanctions Implementation", "Office of Financial Sanctions Implementation",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-financial-sanctions-implementation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of Financial Sanctions Implementation\",\"government\",\"batch:50\"]",
     3600,
     "Office of Financial Sanctions Implementation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_hm_paymaster_general, "uk-b50-govuk-office-of-hm-paymaster-general",
     "Office of HM Paymaster General", "Office of HM Paymaster General",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-hm-paymaster-general.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of HM Paymaster General\",\"government\",\"batch:50\"]",
     604800,
     "Office of HM Paymaster General — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_office_of_manpower_economics, "uk-b50-govuk-office-of-manpower-economics",
     "UK Office Of Manpower Economics", "UK Office Of Manpower Economics",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-manpower-economics.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Office Of Manpower Economics\",\"government\",\"batch:50\"]",
     604800,
     "UK Office Of Manpower Economics — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_rail_regulation, "uk-b50-govuk-office-of-rail-regulation",
     "Office of Rail Regulation", "Office of Rail Regulation",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-rail-regulation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of Rail Regulation\",\"government\",\"batch:50\"]",
     604800,
     "Office of Rail Regulation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_surveillance_commissioners, "uk-b50-govuk-office-of-surveillance-commissioners",
     "Office of Surveillance Commissioners", "Office of Surveillance Commissioners",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-surveillance-commissioners.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of Surveillance Commissioners\",\"government\",\"batch:50\"]",
     604800,
     "Office of Surveillance Commissioners — government feed (15 entries at probe)")

RSSX(uk_b50_govuk_office_of_tax_simplification, "uk-b50-govuk-office-of-tax-simplification",
     "Office of Tax Simplification", "Office of Tax Simplification",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-tax-simplification.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of Tax Simplification\",\"government\",\"batch:50\"]",
     604800,
     "Office of Tax Simplification — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_telecommunications, "uk-b50-govuk-office-of-telecommunications",
     "Office of Telecommunications", "Office of Telecommunications",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-telecommunications.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of Telecommunications\",\"government\",\"batch:50\"]",
     604800,
     "Office of Telecommunications — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_advocate_general_for_scotland, "uk-b50-govuk-office-of-the-advocate-general-for-scotland",
     "Office of the Advocate General for Scotland", "Office of the Advocate General for Scotland",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-advocate-general-for-scotland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Advocate General for Scotland\",\"government\",\"batch:50\"]",
     3600,
     "Office of the Advocate General for Scotland — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_chief_electoral_officer_for_northern_ireland, "uk-b50-govuk-office-of-the-chief-electoral-officer-for-northern-ireland",
     "Office of the Chief Electoral Officer for Northern Ireland", "Office of the Chief Electoral Officer for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-chief-electoral-officer-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Chief Electoral Officer for Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Office of the Chief Electoral Officer for Northern Ireland — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_children_s_commissioner, "uk-b50-govuk-office-of-the-children-s-commissioner",
     "Office of the Children's Commissioner", "Office of the Children's Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-children-s-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Children's Commissioner\",\"government\",\"batch:50\"]",
     21600,
     "Office of the Children's Commissioner — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_commissioner_for_protection_against_unlawful_industrial_action, "uk-b50-govuk-office-of-the-commissioner-for-protection-against-unlawful-industrial-action",
     "Office of the Commissioner for Protection Against Unlawful Industrial Action", "Office of the Commissioner for Protection Against Unlawful Industrial Action",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-commissioner-for-protection-against-unlawful-industrial-action.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Commissioner for Protection Against Unlawful Industrial Action\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Commissioner for Protection Against Unlawful Industrial Action — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_commissioner_for_the_rights_of_trade_union_members, "uk-b50-govuk-office-of-the-commissioner-for-the-rights-of-trade-union-members",
     "Office of the Commissioner for the Rights of Trade Union Members", "Office of the Commissioner for the Rights of Trade Union Members",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-commissioner-for-the-rights-of-trade-union-members.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Commissioner for the Rights of Trade Union Members\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Commissioner for the Rights of Trade Union Members — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_first_minister_and_deputy_first_minister, "uk-b50-govuk-office-of-the-first-minister-and-deputy-first-minister",
     "Office of the First Minister and Deputy First Minister", "Office of the First Minister and Deputy First Minister",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-first-minister-and-deputy-first-minister.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the First Minister and Deputy First Minister\",\"government\",\"batch:50\"]",
     604800,
     "Office of the First Minister and Deputy First Minister — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_identity_commissioner, "uk-b50-govuk-office-of-the-identity-commissioner",
     "Office of the Identity Commissioner", "Office of the Identity Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-identity-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Identity Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Identity Commissioner — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_immigration_services_commissioner, "uk-b50-govuk-office-of-the-immigration-services-commissioner",
     "Office of the Immigration Services Commissioner", "Office of the Immigration Services Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-immigration-services-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Immigration Services Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Immigration Services Commissioner — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_independent_prevent_commissioner, "uk-b50-govuk-office-of-the-independent-prevent-commissioner",
     "Office of the Independent Prevent Commissioner", "Office of the Independent Prevent Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-independent-prevent-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Independent Prevent Commissioner\",\"government\",\"batch:50\"]",
     3600,
     "Office of the Independent Prevent Commissioner — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_lay_observer, "uk-b50-govuk-office-of-the-lay-observer",
     "Office of the Lay Observer", "Office of the Lay Observer",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-lay-observer.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Lay Observer\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Lay Observer — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_leader_of_the_house_of_lords, "uk-b50-govuk-office-of-the-leader-of-the-house-of-lords",
     "UK Office Of The Leader Of The House Of Lords", "UK Office Of The Leader Of The House Of Lords",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-leader-of-the-house-of-lords.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Office Of The Leader Of The House Of Lords\",\"government\",\"batch:50\"]",
     3600,
     "UK Office Of The Leader Of The House Of Lords — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_legal_services_complaints_commissioner, "uk-b50-govuk-office-of-the-legal-services-complaints-commissioner",
     "Office of the Legal Services Complaints Commissioner", "Office of the Legal Services Complaints Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-legal-services-complaints-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Legal Services Complaints Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Legal Services Complaints Commissioner — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_legal_services_ombudsman, "uk-b50-govuk-office-of-the-legal-services-ombudsman",
     "Office of the Legal Services Ombudsman", "Office of the Legal Services Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-legal-services-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Legal Services Ombudsman\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Legal Services Ombudsman — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_oversight_commissioner, "uk-b50-govuk-office-of-the-oversight-commissioner",
     "Office of the Oversight Commissioner", "Office of the Oversight Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-oversight-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Oversight Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Office of the Oversight Commissioner — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_parliamentary_counsel, "uk-b50-govuk-office-of-the-parliamentary-counsel",
     "Office of the Parliamentary Counsel", "Office of the Parliamentary Counsel",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-parliamentary-counsel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Parliamentary Counsel\",\"government\",\"batch:50\"]",
     3600,
     "Office of the Parliamentary Counsel — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_public_guardian, "uk-b50-govuk-office-of-the-public-guardian",
     "UK Office Of The Public Guardian", "UK Office Of The Public Guardian",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-public-guardian.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Office Of The Public Guardian\",\"government\",\"batch:50\"]",
     3600,
     "UK Office Of The Public Guardian — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_registrar_of_consultant_lobbyists, "uk-b50-govuk-office-of-the-registrar-of-consultant-lobbyists",
     "Office of the Registrar of Consultant Lobbyists", "Office of the Registrar of Consultant Lobbyists",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-registrar-of-consultant-lobbyists.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Registrar of Consultant Lobbyists\",\"government\",\"batch:50\"]",
     21600,
     "Office of the Registrar of Consultant Lobbyists — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_regulator_of_community_interest_companies, "uk-b50-govuk-office-of-the-regulator-of-community-interest-companies",
     "UK Office Of The Regulator Of Community Interest Companies", "UK Office Of The Regulator Of Community Interest Companies",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-regulator-of-community-interest-companies.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Office Of The Regulator Of Community Interest Companies\",\"government\",\"batch:50\"]",
     3600,
     "UK Office Of The Regulator Of Community Interest Companies — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_schools_adjudicator, "uk-b50-govuk-office-of-the-schools-adjudicator",
     "Office of the Schools Adjudicator", "Office of the Schools Adjudicator",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-schools-adjudicator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Schools Adjudicator\",\"government\",\"batch:50\"]",
     3600,
     "Office of the Schools Adjudicator — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_secretary_of_state_for_wales, "uk-b50-govuk-office-of-the-secretary-of-state-for-wales",
     "UK Wales Office", "UK Wales Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-secretary-of-state-for-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Wales Office\",\"government\",\"batch:50\"]",
     604800,
     "UK Wales Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_windrush_commissioner, "uk-b50-govuk-office-of-the-windrush-commissioner",
     "Office of the Windrush Commissioner", "Office of the Windrush Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-the-windrush-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Windrush Commissioner\",\"government\",\"batch:50\"]",
     21600,
     "Office of the Windrush Commissioner — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_office_of_trade_sanctions_implementation, "uk-b50-govuk-office-of-trade-sanctions-implementation",
     "Office of Trade Sanctions Implementation", "Office of Trade Sanctions Implementation",
     "government", "government",
     "https://www.gov.uk/government/organisations/office-of-trade-sanctions-implementation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of Trade Sanctions Implementation\",\"government\",\"batch:50\"]",
     3600,
     "Office of Trade Sanctions Implementation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_official_solicitor_and_public_trustee, "uk-b50-govuk-official-solicitor-and-public-trustee",
     "Official Solicitor and Public Trustee", "Official Solicitor and Public Trustee",
     "government", "government",
     "https://www.gov.uk/government/organisations/official-solicitor-and-public-trustee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Official Solicitor and Public Trustee\",\"government\",\"batch:50\"]",
     3600,
     "Official Solicitor and Public Trustee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_offshore_petroleum_regulator_for_environment_and_decommissioning, "uk-b50-govuk-offshore-petroleum-regulator-for-environment-and-decommissioning",
     "Offshore Petroleum Regulator for Environment and Decommissioning", "Offshore Petroleum Regulator for Environment and Decommissioning",
     "government", "government",
     "https://www.gov.uk/government/organisations/offshore-petroleum-regulator-for-environment-and-decommissioning.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Offshore Petroleum Regulator for Environment and Decommissioning\",\"government\",\"batch:50\"]",
     3600,
     "Offshore Petroleum Regulator for Environment and Decommissioning — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_ofgem, "uk-b50-govuk-ofgem",
     "UK Ofgem", "UK Ofgem",
     "government", "government",
     "https://www.gov.uk/government/organisations/ofgem.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Ofgem\",\"government\",\"batch:50\"]",
     21600,
     "UK Ofgem — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_oftel, "uk-b50-govuk-oftel",
     "Oftel", "Oftel",
     "government", "government",
     "https://www.gov.uk/government/organisations/oftel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Oftel\",\"government\",\"batch:50\"]",
     604800,
     "Oftel — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_ogcbuyingsolutions, "uk-b50-govuk-ogcbuyingsolutions",
     "OGCbuyingsolutions", "OGCbuyingsolutions",
     "government", "government",
     "https://www.gov.uk/government/organisations/ogcbuyingsolutions.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:OGCbuyingsolutions\",\"government\",\"batch:50\"]",
     604800,
     "OGCbuyingsolutions — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_oil_and_gas_authority, "uk-b50-govuk-oil-and-gas-authority",
     "Oil and Gas Authority", "Oil and Gas Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/oil-and-gas-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Oil and Gas Authority\",\"government\",\"batch:50\"]",
     604800,
     "Oil and Gas Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_olympic_delivery_authority, "uk-b50-govuk-olympic-delivery-authority",
     "Olympic Delivery Authority", "Olympic Delivery Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/olympic-delivery-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Olympic Delivery Authority\",\"government\",\"batch:50\"]",
     604800,
     "Olympic Delivery Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_one_north_east, "uk-b50-govuk-one-north-east",
     "One North East", "One North East",
     "government", "government",
     "https://www.gov.uk/government/organisations/one-north-east.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:One North East\",\"government\",\"batch:50\"]",
     604800,
     "One North East — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_online_procedure_rule_committee, "uk-b50-govuk-online-procedure-rule-committee",
     "Online Procedure Rule Committee", "Online Procedure Rule Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/online-procedure-rule-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Online Procedure Rule Committee\",\"government\",\"batch:50\"]",
     3600,
     "Online Procedure Rule Committee — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_open_public_services, "uk-b50-govuk-open-public-services",
     "Open Public Services", "Open Public Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/open-public-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Open Public Services\",\"government\",\"batch:50\"]",
     3600,
     "Open Public Services — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_packuk, "uk-b50-govuk-packuk",
     "PackUK", "PackUK",
     "government", "government",
     "https://www.gov.uk/government/organisations/packuk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:PackUK\",\"government\",\"batch:50\"]",
     3600,
     "PackUK — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_parades_commission_for_northern_ireland, "uk-b50-govuk-parades-commission-for-northern-ireland",
     "Parades Commission for Northern Ireland", "Parades Commission for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/parades-commission-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Parades Commission for Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Parades Commission for Northern Ireland — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_parliamentary_contributory_pension_fund, "uk-b50-govuk-parliamentary-contributory-pension-fund",
     "Parliamentary Contributory Pension Fund", "Parliamentary Contributory Pension Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/parliamentary-contributory-pension-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Parliamentary Contributory Pension Fund\",\"government\",\"batch:50\"]",
     21600,
     "Parliamentary Contributory Pension Fund — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_parole_commissioners_for_northern_ireland, "uk-b50-govuk-parole-commissioners-for-northern-ireland",
     "Parole Commissioners for Northern Ireland", "Parole Commissioners for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/parole-commissioners-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Parole Commissioners for Northern Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Parole Commissioners for Northern Ireland — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_passenger_focus, "uk-b50-govuk-passenger-focus",
     "Passenger Focus", "Passenger Focus",
     "government", "government",
     "https://www.gov.uk/government/organisations/passenger-focus.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Passenger Focus\",\"government\",\"batch:50\"]",
     604800,
     "Passenger Focus — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_patrick_finucane_review, "uk-b50-govuk-patrick-finucane-review",
     "Patrick Finucane Review", "Patrick Finucane Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/patrick-finucane-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Patrick Finucane Review\",\"government\",\"batch:50\"]",
     604800,
     "Patrick Finucane Review — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_payment_systems_regulator, "uk-b50-govuk-payment-systems-regulator",
     "UK Payment Systems Regulator", "UK Payment Systems Regulator",
     "government", "government",
     "https://www.gov.uk/government/organisations/payment-systems-regulator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Payment Systems Regulator\",\"government\",\"batch:50\"]",
     21600,
     "UK Payment Systems Regulator — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_peak_district_national_park, "uk-b50-govuk-peak-district-national-park",
     "Peak District National Park Authority", "Peak District National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/peak-district-national-park.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Peak District National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "Peak District National Park Authority — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_pension_disability_and_carers_service, "uk-b50-govuk-pension-disability-and-carers-service",
     "Pension, Disability and Carers Service", "Pension, Disability and Carers Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/pension-disability-and-carers-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Pension, Disability and Carers Service\",\"government\",\"batch:50\"]",
     604800,
     "Pension, Disability and Carers Service — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_pension_protection_fund, "uk-b50-govuk-pension-protection-fund",
     "UK Pension Protection Fund", "UK Pension Protection Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/pension-protection-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Pension Protection Fund\",\"government\",\"batch:50\"]",
     21600,
     "UK Pension Protection Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_pension_protection_fund_ombudsman, "uk-b50-govuk-pension-protection-fund-ombudsman",
     "The Pension Protection Fund Ombudsman", "The Pension Protection Fund Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/pension-protection-fund-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Pension Protection Fund Ombudsman\",\"government\",\"batch:50\"]",
     21600,
     "The Pension Protection Fund Ombudsman — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_pension_service, "uk-b50-govuk-pension-service",
     "Pension Service", "Pension Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/pension-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Pension Service\",\"government\",\"batch:50\"]",
     604800,
     "Pension Service — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_pensions_ombudsman, "uk-b50-govuk-pensions-ombudsman",
     "The Pensions Ombudsman", "The Pensions Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/pensions-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Pensions Ombudsman\",\"government\",\"batch:50\"]",
     21600,
     "The Pensions Ombudsman — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_people_pay_and_pensions_agency, "uk-b50-govuk-people-pay-and-pensions-agency",
     "People, Pay and Pensions Agency", "People, Pay and Pensions Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/people-pay-and-pensions-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:People, Pay and Pensions Agency\",\"government\",\"batch:50\"]",
     604800,
     "People, Pay and Pensions Agency — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_personal_accounts_delivery_authority, "uk-b50-govuk-personal-accounts-delivery-authority",
     "Personal Accounts Delivery Authority", "Personal Accounts Delivery Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/personal-accounts-delivery-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Personal Accounts Delivery Authority\",\"government\",\"batch:50\"]",
     604800,
     "Personal Accounts Delivery Authority — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_pesticides_safety_directorate, "uk-b50-govuk-pesticides-safety-directorate",
     "Pesticides Safety Directorate", "Pesticides Safety Directorate",
     "government", "government",
     "https://www.gov.uk/government/organisations/pesticides-safety-directorate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Pesticides Safety Directorate\",\"government\",\"batch:50\"]",
     604800,
     "Pesticides Safety Directorate — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_peterlee_education_action_zone, "uk-b50-govuk-peterlee-education-action-zone",
     "Peterlee Education Action Zone", "Peterlee Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/peterlee-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Peterlee Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Peterlee Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_phoenix_sports, "uk-b50-govuk-phoenix-sports",
     "Phoenix Sports", "Phoenix Sports",
     "government", "government",
     "https://www.gov.uk/government/organisations/phoenix-sports.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Phoenix Sports\",\"government\",\"batch:50\"]",
     604800,
     "Phoenix Sports — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_phone_paid_services_authority, "uk-b50-govuk-phone-paid-services-authority",
     "Phone-paid Services Authority", "Phone-paid Services Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/phone-paid-services-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Phone-paid Services Authority\",\"government\",\"batch:50\"]",
     604800,
     "Phone-paid Services Authority — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_planning_service, "uk-b50-govuk-planning-service",
     "Planning Service", "Planning Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/planning-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Planning Service\",\"government\",\"batch:50\"]",
     604800,
     "Planning Service — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_platform4, "uk-b50-govuk-platform4",
     "Platform4", "Platform4",
     "government", "government",
     "https://www.gov.uk/government/organisations/platform4.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Platform4\",\"government\",\"batch:50\"]",
     21600,
     "Platform4 — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_plymouth_education_action_zone, "uk-b50-govuk-plymouth-education-action-zone",
     "Plymouth Education Action Zone", "Plymouth Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/plymouth-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Plymouth Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Plymouth Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_policy_lab, "uk-b50-govuk-policy-lab",
     "Policy Lab", "Policy Lab",
     "government", "government",
     "https://www.gov.uk/government/organisations/policy-lab.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Policy Lab\",\"government\",\"batch:50\"]",
     21600,
     "Policy Lab — government feed (14 entries at probe)")

RSSX(uk_b50_govuk_polytechnics_and_colleges_funding_council, "uk-b50-govuk-polytechnics-and-colleges-funding-council",
     "Polytechnics and Colleges Funding Council", "Polytechnics and Colleges Funding Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/polytechnics-and-colleges-funding-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Polytechnics and Colleges Funding Council\",\"government\",\"batch:50\"]",
     604800,
     "Polytechnics and Colleges Funding Council — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_post_office, "uk-b50-govuk-post-office",
     "Post Office", "Post Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/post-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Post Office\",\"government\",\"batch:50\"]",
     21600,
     "Post Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_postal_services_commission, "uk-b50-govuk-postal-services-commission",
     "Postal Services Commission", "Postal Services Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/postal-services-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Postal Services Commission\",\"government\",\"batch:50\"]",
     604800,
     "Postal Services Commission — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_prescription_pricing_authority, "uk-b50-govuk-prescription-pricing-authority",
     "Prescription Pricing Authority", "Prescription Pricing Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/prescription-pricing-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Prescription Pricing Authority\",\"government\",\"batch:50\"]",
     604800,
     "Prescription Pricing Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_preventing_sexual_violence_in_conflict_initiative, "uk-b50-govuk-preventing-sexual-violence-in-conflict-initiative",
     "Preventing Sexual Violence in Conflict Initiative", "Preventing Sexual Violence in Conflict Initiative",
     "government", "government",
     "https://www.gov.uk/government/organisations/preventing-sexual-violence-in-conflict-initiative.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Preventing Sexual Violence in Conflict Initiative\",\"government\",\"batch:50\"]",
     3600,
     "Preventing Sexual Violence in Conflict Initiative — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_prison_services_pay_review_body, "uk-b50-govuk-prison-services-pay-review-body",
     "Prison Service Pay Review Body", "Prison Service Pay Review Body",
     "government", "government",
     "https://www.gov.uk/government/organisations/prison-services-pay-review-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Prison Service Pay Review Body\",\"government\",\"batch:50\"]",
     3600,
     "Prison Service Pay Review Body — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_prisons_and_probation_ombudsman, "uk-b50-govuk-prisons-and-probation-ombudsman",
     "Prisons and Probation Ombudsman", "Prisons and Probation Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/prisons-and-probation-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Prisons and Probation Ombudsman\",\"government\",\"batch:50\"]",
     21600,
     "Prisons and Probation Ombudsman — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_privy_council_office, "uk-b50-govuk-privy-council-office",
     "Privy Council Office", "Privy Council Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/privy-council-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Privy Council Office\",\"government\",\"batch:50\"]",
     21600,
     "Privy Council Office — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_probation_board_for_northern_ireland, "uk-b50-govuk-probation-board-for-northern-ireland",
     "Probation Board for Northern Ireland", "Probation Board for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/probation-board-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Probation Board for Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Probation Board for Northern Ireland — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_probation_service, "uk-b50-govuk-probation-service",
     "Probation Service", "Probation Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/probation-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Probation Service\",\"government\",\"batch:50\"]",
     3600,
     "Probation Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_professional_oversight_board, "uk-b50-govuk-professional-oversight-board",
     "Professional Oversight Board", "Professional Oversight Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/professional-oversight-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Professional Oversight Board\",\"government\",\"batch:50\"]",
     604800,
     "Professional Oversight Board — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_public_accounts_commission, "uk-b50-govuk-public-accounts-commission",
     "Public Accounts Commission", "Public Accounts Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/public-accounts-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Public Accounts Commission\",\"government\",\"batch:50\"]",
     604800,
     "Public Accounts Commission — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_public_lending_right_office, "uk-b50-govuk-public-lending-right-office",
     "Public Lending Right Office", "Public Lending Right Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/public-lending-right-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Public Lending Right Office\",\"government\",\"batch:50\"]",
     604800,
     "Public Lending Right Office — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_public_prosecution_service_for_northern_ireland, "uk-b50-govuk-public-prosecution-service-for-northern-ireland",
     "Public Prosecution Service for Northern Ireland", "Public Prosecution Service for Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/public-prosecution-service-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Public Prosecution Service for Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Public Prosecution Service for Northern Ireland — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_public_sector_fraud_authority, "uk-b50-govuk-public-sector-fraud-authority",
     "UK Public Sector Fraud Authority", "UK Public Sector Fraud Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/public-sector-fraud-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Public Sector Fraud Authority\",\"government\",\"batch:50\"]",
     3600,
     "UK Public Sector Fraud Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_public_service_ombudsman, "uk-b50-govuk-public-service-ombudsman",
     "Public Service Ombudsman", "Public Service Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/public-service-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Public Service Ombudsman\",\"government\",\"batch:50\"]",
     21600,
     "Public Service Ombudsman — government feed (17 entries at probe)")

RSSX(uk_b50_govuk_pubs_code_adjudicator, "uk-b50-govuk-pubs-code-adjudicator",
     "Pubs Code Adjudicator", "Pubs Code Adjudicator",
     "government", "government",
     "https://www.gov.uk/government/organisations/pubs-code-adjudicator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Pubs Code Adjudicator\",\"government\",\"batch:50\"]",
     21600,
     "Pubs Code Adjudicator — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_qualifications_and_curriculum_authority, "uk-b50-govuk-qualifications-and-curriculum-authority",
     "Qualifications and Curriculum Authority", "Qualifications and Curriculum Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/qualifications-and-curriculum-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Qualifications and Curriculum Authority\",\"government\",\"batch:50\"]",
     604800,
     "Qualifications and Curriculum Authority — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_qualifications_and_curriculum_development_agency, "uk-b50-govuk-qualifications-and-curriculum-development-agency",
     "Qualifications and Curriculum Development Agency", "Qualifications and Curriculum Development Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/qualifications-and-curriculum-development-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Qualifications and Curriculum Development Agency\",\"government\",\"batch:50\"]",
     604800,
     "Qualifications and Curriculum Development Agency — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_queen_elizabeth_ii_conference_centre, "uk-b50-govuk-queen-elizabeth-ii-conference-centre",
     "Queen Elizabeth II Conference Centre", "Queen Elizabeth II Conference Centre",
     "government", "government",
     "https://www.gov.uk/government/organisations/queen-elizabeth-ii-conference-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Queen Elizabeth II Conference Centre\",\"government\",\"batch:50\"]",
     21600,
     "Queen Elizabeth II Conference Centre — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_race_disparity_unit, "uk-b50-govuk-race-disparity-unit",
     "Race Disparity Unit", "Race Disparity Unit",
     "government", "government",
     "https://www.gov.uk/government/organisations/race-disparity-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Race Disparity Unit\",\"government\",\"batch:50\"]",
     604800,
     "Race Disparity Unit — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_race_equality_unit, "uk-b50-govuk-race-equality-unit",
     "Race Equality Unit", "Race Equality Unit",
     "government", "government",
     "https://www.gov.uk/government/organisations/race-equality-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Race Equality Unit\",\"government\",\"batch:50\"]",
     3600,
     "Race Equality Unit — government feed (11 entries at probe)")

RSSX(uk_b50_govuk_radioactive_waste_management, "uk-b50-govuk-radioactive-waste-management",
     "Radioactive Waste Management", "Radioactive Waste Management",
     "government", "government",
     "https://www.gov.uk/government/organisations/radioactive-waste-management.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Radioactive Waste Management\",\"government\",\"batch:50\"]",
     604800,
     "Radioactive Waste Management — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_rail_accident_investigation_branch, "uk-b50-govuk-rail-accident-investigation-branch",
     "Rail Accident Investigation Branch", "Rail Accident Investigation Branch",
     "government", "government",
     "https://www.gov.uk/government/organisations/rail-accident-investigation-branch.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Rail Accident Investigation Branch\",\"government\",\"batch:50\"]",
     3600,
     "Rail Accident Investigation Branch — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_rail_safety_and_standards_board, "uk-b50-govuk-rail-safety-and-standards-board",
     "Rail Safety and Standards Board", "Rail Safety and Standards Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/rail-safety-and-standards-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Rail Safety and Standards Board\",\"government\",\"batch:50\"]",
     21600,
     "Rail Safety and Standards Board — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_reclaim_fund_ltd, "uk-b50-govuk-reclaim-fund-ltd",
     "Reclaim Fund Ltd", "Reclaim Fund Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/reclaim-fund-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Reclaim Fund Ltd\",\"government\",\"batch:50\"]",
     3600,
     "Reclaim Fund Ltd — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_redfern_inquiry, "uk-b50-govuk-redfern-inquiry",
     "Redfern Inquiry", "Redfern Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/redfern-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Redfern Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Redfern Inquiry — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_regeneration_investment_organisation, "uk-b50-govuk-regeneration-investment-organisation",
     "Regeneration Investment Organisation", "Regeneration Investment Organisation",
     "government", "government",
     "https://www.gov.uk/government/organisations/regeneration-investment-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Regeneration Investment Organisation\",\"government\",\"batch:50\"]",
     604800,
     "Regeneration Investment Organisation — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_regional_department_for_education_dfe_directors, "uk-b50-govuk-regional-department-for-education-dfe-directors",
     "Regional Department for Education (DfE) Directors", "Regional Department for Education (DfE) Directors",
     "government", "government",
     "https://www.gov.uk/government/organisations/regional-department-for-education-dfe-directors.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Regional Department for Education (DfE) Directors\",\"government\",\"batch:50\"]",
     3600,
     "Regional Department for Education (DfE) Directors — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_regional_schools_commissioners, "uk-b50-govuk-regional-schools-commissioners",
     "Regional Schools Commissioners", "Regional Schools Commissioners",
     "government", "government",
     "https://www.gov.uk/government/organisations/regional-schools-commissioners.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Regional Schools Commissioners\",\"government\",\"batch:50\"]",
     604800,
     "Regional Schools Commissioners — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_regulator_of_social_housing, "uk-b50-govuk-regulator-of-social-housing",
     "UK Regulator Of Social Housing", "UK Regulator Of Social Housing",
     "government", "government",
     "https://www.gov.uk/government/organisations/regulator-of-social-housing.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Regulator Of Social Housing\",\"government\",\"batch:50\"]",
     3600,
     "UK Regulator Of Social Housing — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_regulatory_delivery, "uk-b50-govuk-regulatory-delivery",
     "Regulatory Delivery", "Regulatory Delivery",
     "government", "government",
     "https://www.gov.uk/government/organisations/regulatory-delivery.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Regulatory Delivery\",\"government\",\"batch:50\"]",
     604800,
     "Regulatory Delivery — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_regulatory_horizons_council, "uk-b50-govuk-regulatory-horizons-council",
     "Regulatory Horizons Council", "Regulatory Horizons Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/regulatory-horizons-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Regulatory Horizons Council\",\"government\",\"batch:50\"]",
     604800,
     "Regulatory Horizons Council — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_regulatory_innovation_office, "uk-b50-govuk-regulatory-innovation-office",
     "Regulatory Innovation Office", "Regulatory Innovation Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/regulatory-innovation-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Regulatory Innovation Office\",\"government\",\"batch:50\"]",
     3600,
     "Regulatory Innovation Office — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_regulatory_policy_committee, "uk-b50-govuk-regulatory-policy-committee",
     "UK Regulatory Policy Committee", "UK Regulatory Policy Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/regulatory-policy-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Regulatory Policy Committee\",\"government\",\"batch:50\"]",
     3600,
     "UK Regulatory Policy Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_remploy_ltd, "uk-b50-govuk-remploy-ltd",
     "Remploy Ltd", "Remploy Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/remploy-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Remploy Ltd\",\"government\",\"batch:50\"]",
     604800,
     "Remploy Ltd — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_renewable_fuels_agency, "uk-b50-govuk-renewable-fuels-agency",
     "Renewable Fuels Agency", "Renewable Fuels Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/renewable-fuels-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Renewable Fuels Agency\",\"government\",\"batch:50\"]",
     604800,
     "Renewable Fuels Agency — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_rent_service, "uk-b50-govuk-rent-service",
     "Rent Service", "Rent Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/rent-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Rent Service\",\"government\",\"batch:50\"]",
     604800,
     "Rent Service — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_reserve_forces_and_cadets_associations, "uk-b50-govuk-reserve-forces-and-cadets-associations",
     "Reserve Forces' and Cadets' Associations (RFCA)", "Reserve Forces' and Cadets' Associations (RFCA)",
     "government", "government",
     "https://www.gov.uk/government/organisations/reserve-forces-and-cadets-associations.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Reserve Forces' and Cadets' Associations (RFCA)\",\"government\",\"batch:50\"]",
     3600,
     "Reserve Forces' and Cadets' Associations (RFCA) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_revenue_and_customs_prosecutions_office, "uk-b50-govuk-revenue-and-customs-prosecutions-office",
     "Revenue and Customs Prosecutions Office", "Revenue and Customs Prosecutions Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/revenue-and-customs-prosecutions-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Revenue and Customs Prosecutions Office\",\"government\",\"batch:50\"]",
     604800,
     "Revenue and Customs Prosecutions Office — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_review_board_for_government_contracts, "uk-b50-govuk-review-board-for-government-contracts",
     "Review Board for Government Contracts", "Review Board for Government Contracts",
     "government", "government",
     "https://www.gov.uk/government/organisations/review-board-for-government-contracts.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Review Board for Government Contracts\",\"government\",\"batch:50\"]",
     604800,
     "Review Board for Government Contracts — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_review_body_on_doctors_and_dentists_remuneration, "uk-b50-govuk-review-body-on-doctors-and-dentists-remuneration",
     "Review Body on Doctors' and Dentists' Remuneration", "Review Body on Doctors' and Dentists' Remuneration",
     "government", "government",
     "https://www.gov.uk/government/organisations/review-body-on-doctors-and-dentists-remuneration.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Review Body on Doctors' and Dentists' Remuneration\",\"government\",\"batch:50\"]",
     3600,
     "Review Body on Doctors' and Dentists' Remuneration — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_review_body_on_senior_salaries, "uk-b50-govuk-review-body-on-senior-salaries",
     "Senior Salaries Review Body", "Senior Salaries Review Body",
     "government", "government",
     "https://www.gov.uk/government/organisations/review-body-on-senior-salaries.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Senior Salaries Review Body\",\"government\",\"batch:50\"]",
     3600,
     "Senior Salaries Review Body — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_rivers_agency, "uk-b50-govuk-rivers-agency",
     "Rivers Agency", "Rivers Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/rivers-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Rivers Agency\",\"government\",\"batch:50\"]",
     604800,
     "Rivers Agency — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_roads_service, "uk-b50-govuk-roads-service",
     "Roads Service", "Roads Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/roads-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Roads Service\",\"government\",\"batch:50\"]",
     604800,
     "Roads Service — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_rosemary_nelson_inquiry, "uk-b50-govuk-rosemary-nelson-inquiry",
     "Rosemary Nelson Inquiry", "Rosemary Nelson Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/rosemary-nelson-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Rosemary Nelson Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Rosemary Nelson Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_royal_armouries_museum, "uk-b50-govuk-royal-armouries-museum",
     "Royal Armouries Museum", "Royal Armouries Museum",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-armouries-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Armouries Museum\",\"government\",\"batch:50\"]",
     21600,
     "Royal Armouries Museum — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_royal_commission_on_criminal_justice, "uk-b50-govuk-royal-commission-on-criminal-justice",
     "Royal Commission on Criminal Justice", "Royal Commission on Criminal Justice",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-commission-on-criminal-justice.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Commission on Criminal Justice\",\"government\",\"batch:50\"]",
     604800,
     "Royal Commission on Criminal Justice — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_royal_liverpool_childrens_inquiry, "uk-b50-govuk-royal-liverpool-childrens-inquiry",
     "Royal Liverpool Childrens Inquiry", "Royal Liverpool Childrens Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-liverpool-childrens-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Liverpool Childrens Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Royal Liverpool Childrens Inquiry — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_royal_mail, "uk-b50-govuk-royal-mail",
     "Royal Mail", "Royal Mail",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-mail.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Mail\",\"government\",\"batch:50\"]",
     604800,
     "Royal Mail — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_royal_mint, "uk-b50-govuk-royal-mint",
     "UK Royal Mint", "UK Royal Mint",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-mint.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Royal Mint\",\"government\",\"batch:50\"]",
     21600,
     "UK Royal Mint — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_royal_mint_advisory_committee, "uk-b50-govuk-royal-mint-advisory-committee",
     "Royal Mint Advisory Committee", "Royal Mint Advisory Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-mint-advisory-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Mint Advisory Committee\",\"government\",\"batch:50\"]",
     3600,
     "Royal Mint Advisory Committee — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_royal_museums_greenwich, "uk-b50-govuk-royal-museums-greenwich",
     "Royal Museums Greenwich", "Royal Museums Greenwich",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-museums-greenwich.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Museums Greenwich\",\"government\",\"batch:50\"]",
     21600,
     "Royal Museums Greenwich — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_royal_parks, "uk-b50-govuk-royal-parks",
     "Royal Parks", "Royal Parks",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-parks.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Parks\",\"government\",\"batch:50\"]",
     21600,
     "Royal Parks — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_royal_trustees_office, "uk-b50-govuk-royal-trustees-office",
     "Royal Trustees' Office", "Royal Trustees' Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/royal-trustees-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Royal Trustees' Office\",\"government\",\"batch:50\"]",
     604800,
     "Royal Trustees' Office — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_rural_payments_agency, "uk-b50-govuk-rural-payments-agency",
     "UK Rural Payments Agency", "UK Rural Payments Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/rural-payments-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Rural Payments Agency\",\"government\",\"batch:50\"]",
     3600,
     "UK Rural Payments Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_s4c, "uk-b50-govuk-s4c",
     "S4C", "S4C",
     "government", "government",
     "https://www.gov.uk/government/organisations/s4c.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:S4C\",\"government\",\"batch:50\"]",
     21600,
     "S4C — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_salix_finance_ltd, "uk-b50-govuk-salix-finance-ltd",
     "Salix Finance Ltd", "Salix Finance Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/salix-finance-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Salix Finance Ltd\",\"government\",\"batch:50\"]",
     21600,
     "Salix Finance Ltd — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_school_food_trust, "uk-b50-govuk-school-food-trust",
     "School Food Trust", "School Food Trust",
     "government", "government",
     "https://www.gov.uk/government/organisations/school-food-trust.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:School Food Trust\",\"government\",\"batch:50\"]",
     604800,
     "School Food Trust — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_school_teachers_review_body, "uk-b50-govuk-school-teachers-review-body",
     "School Teachers' Review Body", "School Teachers' Review Body",
     "government", "government",
     "https://www.gov.uk/government/organisations/school-teachers-review-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:School Teachers' Review Body\",\"government\",\"batch:50\"]",
     3600,
     "School Teachers' Review Body — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_scientific_advisory_group_for_emergencies, "uk-b50-govuk-scientific-advisory-group-for-emergencies",
     "Scientific Advisory Group for Emergencies", "Scientific Advisory Group for Emergencies",
     "government", "government",
     "https://www.gov.uk/government/organisations/scientific-advisory-group-for-emergencies.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scientific Advisory Group for Emergencies\",\"government\",\"batch:50\"]",
     3600,
     "Scientific Advisory Group for Emergencies — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_scotland_office, "uk-b50-govuk-scotland-office",
     "UK Scotland Office", "UK Scotland Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/scotland-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Scotland Office\",\"government\",\"batch:50\"]",
     3600,
     "UK Scotland Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_scottish_arts_council, "uk-b50-govuk-scottish-arts-council",
     "Scottish Arts Council", "Scottish Arts Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/scottish-arts-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scottish Arts Council\",\"government\",\"batch:50\"]",
     604800,
     "Scottish Arts Council — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_scottish_hospital_trust, "uk-b50-govuk-scottish-hospital-trust",
     "Scottish Hospital Trust", "Scottish Hospital Trust",
     "government", "government",
     "https://www.gov.uk/government/organisations/scottish-hospital-trust.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scottish Hospital Trust\",\"government\",\"batch:50\"]",
     604800,
     "Scottish Hospital Trust — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_scottish_law_commission, "uk-b50-govuk-scottish-law-commission",
     "Scottish Law Commission", "Scottish Law Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/scottish-law-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scottish Law Commission\",\"government\",\"batch:50\"]",
     604800,
     "Scottish Law Commission — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_scottish_office, "uk-b50-govuk-scottish-office",
     "Scottish Office", "Scottish Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/scottish-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scottish Office\",\"government\",\"batch:50\"]",
     604800,
     "Scottish Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_scottish_screen, "uk-b50-govuk-scottish-screen",
     "Scottish Screen", "Scottish Screen",
     "government", "government",
     "https://www.gov.uk/government/organisations/scottish-screen.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scottish Screen\",\"government\",\"batch:50\"]",
     604800,
     "Scottish Screen — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_scottish_sports_council, "uk-b50-govuk-scottish-sports-council",
     "Scottish Sports Council", "Scottish Sports Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/scottish-sports-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scottish Sports Council\",\"government\",\"batch:50\"]",
     604800,
     "Scottish Sports Council — government feed (12 entries at probe)")

RSSX(uk_b50_govuk_sea_fish_industry_authority, "uk-b50-govuk-sea-fish-industry-authority",
     "UK Sea Fish Industry Authority", "UK Sea Fish Industry Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/sea-fish-industry-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Sea Fish Industry Authority\",\"government\",\"batch:50\"]",
     3600,
     "UK Sea Fish Industry Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_seafish, "uk-b50-govuk-seafish",
     "Seafish", "Seafish",
     "government", "government",
     "https://www.gov.uk/government/organisations/seafish.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Seafish\",\"government\",\"batch:50\"]",
     21600,
     "Seafish — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_security_industry_authority, "uk-b50-govuk-security-industry-authority",
     "UK Security Industry Authority", "UK Security Industry Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/security-industry-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Security Industry Authority\",\"government\",\"batch:50\"]",
     3600,
     "UK Security Industry Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_security_vetting_appeals_panel, "uk-b50-govuk-security-vetting-appeals-panel",
     "Security Vetting Appeals Panel", "Security Vetting Appeals Panel",
     "government", "government",
     "https://www.gov.uk/government/organisations/security-vetting-appeals-panel.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Security Vetting Appeals Panel\",\"government\",\"batch:50\"]",
     3600,
     "Security Vetting Appeals Panel — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_sellafield_ltd, "uk-b50-govuk-sellafield-ltd",
     "UK Sellafield", "UK Sellafield",
     "government", "government",
     "https://www.gov.uk/government/organisations/sellafield-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Sellafield\",\"government\",\"batch:50\"]",
     3600,
     "UK Sellafield — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_sentence_review_commissioners, "uk-b50-govuk-sentence-review-commissioners",
     "Sentence Review Commissioners", "Sentence Review Commissioners",
     "government", "government",
     "https://www.gov.uk/government/organisations/sentence-review-commissioners.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sentence Review Commissioners\",\"government\",\"batch:50\"]",
     604800,
     "Sentence Review Commissioners — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_serious_fraud_office, "uk-b50-govuk-serious-fraud-office",
     "UK Serious Fraud Office", "UK Serious Fraud Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/serious-fraud-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Serious Fraud Office\",\"government\",\"batch:50\"]",
     3600,
     "UK Serious Fraud Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_service_children_s_education, "uk-b50-govuk-service-children-s-education",
     "Service Children's Education", "Service Children's Education",
     "government", "government",
     "https://www.gov.uk/government/organisations/service-children-s-education.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Service Children's Education\",\"government\",\"batch:50\"]",
     604800,
     "Service Children's Education — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_service_complaints_commissioner, "uk-b50-govuk-service-complaints-commissioner",
     "Service Complaints Commissioner", "Service Complaints Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/service-complaints-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Service Complaints Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Service Complaints Commissioner — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_service_complaints_ombudsman, "uk-b50-govuk-service-complaints-ombudsman",
     "Service Complaints Ombudsman", "Service Complaints Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/service-complaints-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Service Complaints Ombudsman\",\"government\",\"batch:50\"]",
     604800,
     "Service Complaints Ombudsman — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_service_personnel_and_veterans_agency, "uk-b50-govuk-service-personnel-and-veterans-agency",
     "Service Personnel and Veterans Agency", "Service Personnel and Veterans Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/service-personnel-and-veterans-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Service Personnel and Veterans Agency\",\"government\",\"batch:50\"]",
     604800,
     "Service Personnel and Veterans Agency — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_service_prosecuting_authority, "uk-b50-govuk-service-prosecuting-authority",
     "Service Prosecuting Authority", "Service Prosecuting Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/service-prosecuting-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Service Prosecuting Authority\",\"government\",\"batch:50\"]",
     21600,
     "Service Prosecuting Authority — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_sheffield_forgemasters_international_ltd, "uk-b50-govuk-sheffield-forgemasters-international-ltd",
     "Sheffield Forgemasters International Ltd", "Sheffield Forgemasters International Ltd",
     "government", "government",
     "https://www.gov.uk/government/organisations/sheffield-forgemasters-international-ltd.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sheffield Forgemasters International Ltd\",\"government\",\"batch:50\"]",
     21600,
     "Sheffield Forgemasters International Ltd — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_shipman_inquiry, "uk-b50-govuk-shipman-inquiry",
     "Shipman Inquiry", "Shipman Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/shipman-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Shipman Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Shipman Inquiry — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_single_financial_guidance_body, "uk-b50-govuk-single-financial-guidance-body",
     "Money and Pensions Service", "Money and Pensions Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/single-financial-guidance-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Money and Pensions Service\",\"government\",\"batch:50\"]",
     21600,
     "Money and Pensions Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_single_source_regulations_office, "uk-b50-govuk-single-source-regulations-office",
     "Single Source Regulations Office", "Single Source Regulations Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/single-source-regulations-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Single Source Regulations Office\",\"government\",\"batch:50\"]",
     21600,
     "Single Source Regulations Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_sir_john_soane_s_museum, "uk-b50-govuk-sir-john-soane-s-museum",
     "Sir John Soane's Museum", "Sir John Soane's Museum",
     "government", "government",
     "https://www.gov.uk/government/organisations/sir-john-soane-s-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sir John Soane's Museum\",\"government\",\"batch:50\"]",
     21600,
     "Sir John Soane's Museum — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_sitpro, "uk-b50-govuk-sitpro",
     "SITPRO", "SITPRO",
     "government", "government",
     "https://www.gov.uk/government/organisations/sitpro.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:SITPRO\",\"government\",\"batch:50\"]",
     604800,
     "SITPRO — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_skills_england, "uk-b50-govuk-skills-england",
     "Skills England", "Skills England",
     "government", "government",
     "https://www.gov.uk/government/organisations/skills-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Skills England\",\"government\",\"batch:50\"]",
     3600,
     "Skills England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_skills_funding_agency, "uk-b50-govuk-skills-funding-agency",
     "Skills Funding Agency", "Skills Funding Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/skills-funding-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Skills Funding Agency\",\"government\",\"batch:50\"]",
     604800,
     "Skills Funding Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_small_business_commissioner, "uk-b50-govuk-small-business-commissioner",
     "Small Business Commissioner", "Small Business Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/small-business-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Small Business Commissioner\",\"government\",\"batch:50\"]",
     3600,
     "Small Business Commissioner — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_social_mobility_and_child_poverty_commission, "uk-b50-govuk-social-mobility-and-child-poverty-commission",
     "Social Mobility and Child Poverty Commission", "Social Mobility and Child Poverty Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/social-mobility-and-child-poverty-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Social Mobility and Child Poverty Commission\",\"government\",\"batch:50\"]",
     604800,
     "Social Mobility and Child Poverty Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_social_mobility_commission, "uk-b50-govuk-social-mobility-commission",
     "Social Mobility Commission", "Social Mobility Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/social-mobility-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Social Mobility Commission\",\"government\",\"batch:50\"]",
     21600,
     "Social Mobility Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_social_security_advisory_committee, "uk-b50-govuk-social-security-advisory-committee",
     "Social Security Advisory Committee", "Social Security Advisory Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/social-security-advisory-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Social Security Advisory Committee\",\"government\",\"batch:50\"]",
     3600,
     "Social Security Advisory Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_social_security_agency, "uk-b50-govuk-social-security-agency",
     "Social Security Agency", "Social Security Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/social-security-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Social Security Agency\",\"government\",\"batch:50\"]",
     604800,
     "Social Security Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_social_work_england, "uk-b50-govuk-social-work-england",
     "UK Social Work England", "UK Social Work England",
     "government", "government",
     "https://www.gov.uk/government/organisations/social-work-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Social Work England\",\"government\",\"batch:50\"]",
     21600,
     "UK Social Work England — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_south_bradford_education_action_zone, "uk-b50-govuk-south-bradford-education-action-zone",
     "South Bradford Education Action Zone", "South Bradford Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/south-bradford-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:South Bradford Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "South Bradford Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_south_downs_national_park_authority, "uk-b50-govuk-south-downs-national-park-authority",
     "South Downs National Park Authority", "South Downs National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/south-downs-national-park-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:South Downs National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "South Downs National Park Authority — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_south_east_england_development_agency, "uk-b50-govuk-south-east-england-development-agency",
     "South East England Development Agency", "South East England Development Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/south-east-england-development-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:South East England Development Agency\",\"government\",\"batch:50\"]",
     604800,
     "South East England Development Agency — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_south_east_england_virtual_education_action_zone, "uk-b50-govuk-south-east-england-virtual-education-action-zone",
     "South East England Virtual Education Action Zone", "South East England Virtual Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/south-east-england-virtual-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:South East England Virtual Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "South East England Virtual Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_south_east_sheffield_education_action_zone, "uk-b50-govuk-south-east-sheffield-education-action-zone",
     "South East Sheffield Education Action Zone", "South East Sheffield Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/south-east-sheffield-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:South East Sheffield Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "South East Sheffield Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_south_eastern_education_and_library_board, "uk-b50-govuk-south-eastern-education-and-library-board",
     "South Eastern Education and Library Board", "South Eastern Education and Library Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/south-eastern-education-and-library-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:South Eastern Education and Library Board\",\"government\",\"batch:50\"]",
     604800,
     "South Eastern Education and Library Board — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_south_west_of_england_regional_development_agency, "uk-b50-govuk-south-west-of-england-regional-development-agency",
     "South West of England Regional Development Agency", "South West of England Regional Development Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/south-west-of-england-regional-development-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:South West of England Regional Development Agency\",\"government\",\"batch:50\"]",
     604800,
     "South West of England Regional Development Agency — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_southend_education_action_zone, "uk-b50-govuk-southend-education-action-zone",
     "Southend Education Action Zone", "Southend Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/southend-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Southend Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Southend Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_special_eu_programmes_body, "uk-b50-govuk-special-eu-programmes-body",
     "Special EU Programmes Body", "Special EU Programmes Body",
     "government", "government",
     "https://www.gov.uk/government/organisations/special-eu-programmes-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Special EU Programmes Body\",\"government\",\"batch:50\"]",
     604800,
     "Special EU Programmes Body — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_speke_garston_education_action_zone, "uk-b50-govuk-speke-garston-education-action-zone",
     "Speke Garston Education Action Zone", "Speke Garston Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/speke-garston-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Speke Garston Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Speke Garston Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_sport_england, "uk-b50-govuk-sport-england",
     "UK Sport England", "UK Sport England",
     "government", "government",
     "https://www.gov.uk/government/organisations/sport-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Sport England\",\"government\",\"batch:50\"]",
     21600,
     "UK Sport England — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_sports_council_for_northern_ireland, "uk-b50-govuk-sports-council-for-northern-ireland",
     "Sport Northern Ireland (Sports Council for Northern Ireland)", "Sport Northern Ireland (Sports Council for Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/sports-council-for-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sport Northern Ireland (Sports Council for Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "Sport Northern Ireland (Sports Council for Northern Ireland) — government feed (14 entries at probe)")

RSSX(uk_b50_govuk_sports_council_for_wales, "uk-b50-govuk-sports-council-for-wales",
     "Sports Council for Wales", "Sports Council for Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/sports-council-for-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sports Council for Wales\",\"government\",\"batch:50\"]",
     21600,
     "Sports Council for Wales — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_sports_grounds_safety_authority, "uk-b50-govuk-sports-grounds-safety-authority",
     "Sports Grounds Safety Authority", "Sports Grounds Safety Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/sports-grounds-safety-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sports Grounds Safety Authority\",\"government\",\"batch:50\"]",
     21600,
     "Sports Grounds Safety Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_stabilisation_unit, "uk-b50-govuk-stabilisation-unit",
     "Stabilisation Unit", "Stabilisation Unit",
     "government", "government",
     "https://www.gov.uk/government/organisations/stabilisation-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Stabilisation Unit\",\"government\",\"batch:50\"]",
     604800,
     "Stabilisation Unit — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_staff_commission_for_education_and_library_boards, "uk-b50-govuk-staff-commission-for-education-and-library-boards",
     "Staff Commission for Education and Library Boards", "Staff Commission for Education and Library Boards",
     "government", "government",
     "https://www.gov.uk/government/organisations/staff-commission-for-education-and-library-boards.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Staff Commission for Education and Library Boards\",\"government\",\"batch:50\"]",
     604800,
     "Staff Commission for Education and Library Boards — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_standards_and_testing_agency, "uk-b50-govuk-standards-and-testing-agency",
     "Standards and Testing Agency", "Standards and Testing Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/standards-and-testing-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Standards and Testing Agency\",\"government\",\"batch:50\"]",
     3600,
     "Standards and Testing Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_standards_board_for_england, "uk-b50-govuk-standards-board-for-england",
     "Standards Board for England", "Standards Board for England",
     "government", "government",
     "https://www.gov.uk/government/organisations/standards-board-for-england.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Standards Board for England\",\"government\",\"batch:50\"]",
     604800,
     "Standards Board for England — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_state_veterinary_service, "uk-b50-govuk-state-veterinary-service",
     "State Veterinary Service", "State Veterinary Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/state-veterinary-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:State Veterinary Service\",\"government\",\"batch:50\"]",
     604800,
     "State Veterinary Service — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_stoke_education_action_zone, "uk-b50-govuk-stoke-education-action-zone",
     "Stoke Education Action Zone", "Stoke Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/stoke-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Stoke Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Stoke Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_strategic_rail_authority, "uk-b50-govuk-strategic-rail-authority",
     "Strategic Rail Authority", "Strategic Rail Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/strategic-rail-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Strategic Rail Authority\",\"government\",\"batch:50\"]",
     604800,
     "Strategic Rail Authority — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_student_loans_company, "uk-b50-govuk-student-loans-company",
     "UK Student Loans Company", "UK Student Loans Company",
     "government", "government",
     "https://www.gov.uk/government/organisations/student-loans-company.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Student Loans Company\",\"government\",\"batch:50\"]",
     3600,
     "UK Student Loans Company — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_subsidy_advice_unit, "uk-b50-govuk-subsidy-advice-unit",
     "Subsidy Advice Unit", "Subsidy Advice Unit",
     "government", "government",
     "https://www.gov.uk/government/organisations/subsidy-advice-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Subsidy Advice Unit\",\"government\",\"batch:50\"]",
     3600,
     "Subsidy Advice Unit — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_sunderland_education_action_zone, "uk-b50-govuk-sunderland-education-action-zone",
     "Sunderland Education Action Zone", "Sunderland Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/sunderland-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sunderland Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Sunderland Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_supreme_court_of_the_united_kingdom, "uk-b50-govuk-supreme-court-of-the-united-kingdom",
     "UK Supreme Court Of The United Kingdom", "UK Supreme Court Of The United Kingdom",
     "government", "government",
     "https://www.gov.uk/government/organisations/supreme-court-of-the-united-kingdom.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Supreme Court Of The United Kingdom\",\"government\",\"batch:50\"]",
     21600,
     "UK Supreme Court Of The United Kingdom — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_surveillance_camera_commissioner, "uk-b50-govuk-surveillance-camera-commissioner",
     "Surveillance Camera Commissioner", "Surveillance Camera Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/surveillance-camera-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Surveillance Camera Commissioner\",\"government\",\"batch:50\"]",
     604800,
     "Surveillance Camera Commissioner — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_sutherland_inquiry, "uk-b50-govuk-sutherland-inquiry",
     "Sutherland Inquiry", "Sutherland Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/sutherland-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sutherland Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Sutherland Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_tate, "uk-b50-govuk-tate",
     "Tate", "Tate",
     "government", "government",
     "https://www.gov.uk/government/organisations/tate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Tate\",\"government\",\"batch:50\"]",
     21600,
     "Tate — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_teaching_regulation_agency, "uk-b50-govuk-teaching-regulation-agency",
     "Teaching Regulation Agency", "Teaching Regulation Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/teaching-regulation-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Teaching Regulation Agency\",\"government\",\"batch:50\"]",
     3600,
     "Teaching Regulation Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_technical_advisory_board, "uk-b50-govuk-technical-advisory-board",
     "Technical Advisory Board", "Technical Advisory Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/technical-advisory-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Technical Advisory Board\",\"government\",\"batch:50\"]",
     3600,
     "Technical Advisory Board — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_technology_strategy_board, "uk-b50-govuk-technology-strategy-board",
     "Technology Strategy Board", "Technology Strategy Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/technology-strategy-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Technology Strategy Board\",\"government\",\"batch:50\"]",
     604800,
     "Technology Strategy Board — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_telford_and_wrekin_education_action_zone, "uk-b50-govuk-telford-and-wrekin-education-action-zone",
     "Telford and Wrekin Education Action Zone", "Telford and Wrekin Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/telford-and-wrekin-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Telford and Wrekin Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Telford and Wrekin Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_tenant_services_authority, "uk-b50-govuk-tenant-services-authority",
     "Tenant Services Authority", "Tenant Services Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/tenant-services-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Tenant Services Authority\",\"government\",\"batch:50\"]",
     604800,
     "Tenant Services Authority — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_the_adjudicator_s_office, "uk-b50-govuk-the-adjudicator-s-office",
     "The Adjudicator’s Office", "The Adjudicator’s Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-adjudicator-s-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Adjudicator’s Office\",\"government\",\"batch:50\"]",
     3600,
     "The Adjudicator’s Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_advisory_council_on_national_records_and_archives, "uk-b50-govuk-the-advisory-council-on-national-records-and-archives",
     "The Advisory Council on National Records and Archives", "The Advisory Council on National Records and Archives",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-advisory-council-on-national-records-and-archives.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Advisory Council on National Records and Archives\",\"government\",\"batch:50\"]",
     21600,
     "The Advisory Council on National Records and Archives — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_the_coal_authority, "uk-b50-govuk-the-coal-authority",
     "Coal Authority", "Coal Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-coal-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Coal Authority\",\"government\",\"batch:50\"]",
     604800,
     "Coal Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_committee_on_standards_in_public_life, "uk-b50-govuk-the-committee-on-standards-in-public-life",
     "Committee on Standards in Public Life", "Committee on Standards in Public Life",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-committee-on-standards-in-public-life.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Committee on Standards in Public Life\",\"government\",\"batch:50\"]",
     604800,
     "Committee on Standards in Public Life — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_crown_estate, "uk-b50-govuk-the-crown-estate",
     "The Crown Estate", "The Crown Estate",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-crown-estate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Crown Estate\",\"government\",\"batch:50\"]",
     21600,
     "The Crown Estate — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_electoral_commission, "uk-b50-govuk-the-electoral-commission",
     "The Electoral Commission", "The Electoral Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-electoral-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Electoral Commission\",\"government\",\"batch:50\"]",
     21600,
     "The Electoral Commission — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_equality_hub, "uk-b50-govuk-the-equality-hub",
     "Equality Hub", "Equality Hub",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-equality-hub.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Equality Hub\",\"government\",\"batch:50\"]",
     604800,
     "Equality Hub — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_executive_office_northern_ireland, "uk-b50-govuk-the-executive-office-northern-ireland",
     "The Executive Office (Northern Ireland)", "The Executive Office (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-executive-office-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Executive Office (Northern Ireland)\",\"government\",\"batch:50\"]",
     21600,
     "The Executive Office (Northern Ireland) — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_fuel_poverty_advisory_group, "uk-b50-govuk-the-fuel-poverty-advisory-group",
     "Fuel Poverty Advisory Group", "Fuel Poverty Advisory Group",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-fuel-poverty-advisory-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Fuel Poverty Advisory Group\",\"government\",\"batch:50\"]",
     604800,
     "Fuel Poverty Advisory Group — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_the_independent_commission_for_reconciliation_and_information_recovery, "uk-b50-govuk-the-independent-commission-for-reconciliation-and-information-recovery",
     "The Independent Commission for Reconciliation and Information Recovery", "The Independent Commission for Reconciliation and Information Recovery",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-independent-commission-for-reconciliation-and-information-recovery.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Independent Commission for Reconciliation and Information Recovery\",\"government\",\"batch:50\"]",
     21600,
     "The Independent Commission for Reconciliation and Information Recovery — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_the_infected_blood_memorial_committee, "uk-b50-govuk-the-infected-blood-memorial-committee",
     "Infected Blood Memorial Committee", "Infected Blood Memorial Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-infected-blood-memorial-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Infected Blood Memorial Committee\",\"government\",\"batch:50\"]",
     3600,
     "Infected Blood Memorial Committee — government feed (19 entries at probe)")

RSSX(uk_b50_govuk_the_legal_ombudsman, "uk-b50-govuk-the-legal-ombudsman",
     "The Legal Ombudsman", "The Legal Ombudsman",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-legal-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Legal Ombudsman\",\"government\",\"batch:50\"]",
     21600,
     "The Legal Ombudsman — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_the_national_lottery_community_fund, "uk-b50-govuk-the-national-lottery-community-fund",
     "The National Lottery Community Fund", "The National Lottery Community Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-national-lottery-community-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The National Lottery Community Fund\",\"government\",\"batch:50\"]",
     21600,
     "The National Lottery Community Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_office_of_the_leader_of_the_house_of_commons, "uk-b50-govuk-the-office-of-the-leader-of-the-house-of-commons",
     "Office of the Leader of the House of Commons", "Office of the Leader of the House of Commons",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-office-of-the-leader-of-the-house-of-commons.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Leader of the House of Commons\",\"government\",\"batch:50\"]",
     3600,
     "Office of the Leader of the House of Commons — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_pensions_advisory_service, "uk-b50-govuk-the-pensions-advisory-service",
     "The Pensions Advisory Service", "The Pensions Advisory Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-pensions-advisory-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Pensions Advisory Service\",\"government\",\"batch:50\"]",
     604800,
     "The Pensions Advisory Service — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_the_pensions_regulator, "uk-b50-govuk-the-pensions-regulator",
     "The Pensions Regulator", "The Pensions Regulator",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-pensions-regulator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Pensions Regulator\",\"government\",\"batch:50\"]",
     21600,
     "The Pensions Regulator — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_queen_elizabeth_memorial_committee, "uk-b50-govuk-the-queen-elizabeth-memorial-committee",
     "The Queen Elizabeth Memorial Committee", "The Queen Elizabeth Memorial Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-queen-elizabeth-memorial-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Queen Elizabeth Memorial Committee\",\"government\",\"batch:50\"]",
     3600,
     "The Queen Elizabeth Memorial Committee — government feed (17 entries at probe)")

RSSX(uk_b50_govuk_the_reviewing_committee_on_the_export_of_works_of_art_and_objects_of_cultural_interest, "uk-b50-govuk-the-reviewing-committee-on-the-export-of-works-of-art-and-objects-of-cultural-interest",
     "The Reviewing Committee on the Export of Works of Art and Objects of Cultural Interest", "The Reviewing Committee on the Export of Works of Art and Objects of Cultural Interest",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-reviewing-committee-on-the-export-of-works-of-art-and-objects-of-cultural-interest.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Reviewing Committee on the Export of Works of Art and Objects of Cultural Interest\",\"government\",\"batch:50\"]",
     21600,
     "The Reviewing Committee on the Export of Works of Art and Objects of Cultural Interest — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_scottish_government, "uk-b50-govuk-the-scottish-government",
     "The Scottish Government", "The Scottish Government",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-scottish-government.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Scottish Government\",\"government\",\"batch:50\"]",
     21600,
     "The Scottish Government — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_sentencing_council_for_england_and_wales, "uk-b50-govuk-the-sentencing-council-for-england-and-wales",
     "Sentencing Council for England and Wales", "Sentencing Council for England and Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-sentencing-council-for-england-and-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Sentencing Council for England and Wales\",\"government\",\"batch:50\"]",
     21600,
     "Sentencing Council for England and Wales — government feed (18 entries at probe)")

RSSX(uk_b50_govuk_the_shareholder_executive, "uk-b50-govuk-the-shareholder-executive",
     "The Shareholder Executive", "The Shareholder Executive",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-shareholder-executive.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Shareholder Executive\",\"government\",\"batch:50\"]",
     604800,
     "The Shareholder Executive — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_theatres_trust, "uk-b50-govuk-the-theatres-trust",
     "The Theatres Trust", "The Theatres Trust",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-theatres-trust.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:The Theatres Trust\",\"government\",\"batch:50\"]",
     21600,
     "The Theatres Trust — government feed (15 entries at probe)")

RSSX(uk_b50_govuk_the_water_services_regulation_authority, "uk-b50-govuk-the-water-services-regulation-authority",
     "Ofwat", "Ofwat",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-water-services-regulation-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ofwat\",\"government\",\"batch:50\"]",
     21600,
     "Ofwat — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_west_northamptonshire_development_corporation, "uk-b50-govuk-the-west-northamptonshire-development-corporation",
     "West Northamptonshire Development Corporation", "West Northamptonshire Development Corporation",
     "government", "government",
     "https://www.gov.uk/government/organisations/the-west-northamptonshire-development-corporation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:West Northamptonshire Development Corporation\",\"government\",\"batch:50\"]",
     604800,
     "West Northamptonshire Development Corporation — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_third_party_campaigning_review, "uk-b50-govuk-third-party-campaigning-review",
     "Third Party Campaigning Review", "Third Party Campaigning Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/third-party-campaigning-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Third Party Campaigning Review\",\"government\",\"batch:50\"]",
     604800,
     "Third Party Campaigning Review — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_thurrock_thames_gateway_development_corporation, "uk-b50-govuk-thurrock-thames-gateway-development-corporation",
     "Thurrock Thames Gateway Development Corporation", "Thurrock Thames Gateway Development Corporation",
     "government", "government",
     "https://www.gov.uk/government/organisations/thurrock-thames-gateway-development-corporation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Thurrock Thames Gateway Development Corporation\",\"government\",\"batch:50\"]",
     604800,
     "Thurrock Thames Gateway Development Corporation — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_trade_and_agriculture_commission, "uk-b50-govuk-trade-and-agriculture-commission",
     "Trade and Agriculture Commission", "Trade and Agriculture Commission",
     "government", "government",
     "https://www.gov.uk/government/organisations/trade-and-agriculture-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Trade and Agriculture Commission\",\"government\",\"batch:50\"]",
     3600,
     "Trade and Agriculture Commission — government feed (14 entries at probe)")

RSSX(uk_b50_govuk_trade_remedies_authority, "uk-b50-govuk-trade-remedies-authority",
     "UK Trade Remedies Authority", "UK Trade Remedies Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/trade-remedies-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Trade Remedies Authority\",\"government\",\"batch:50\"]",
     3600,
     "UK Trade Remedies Authority — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_trade_remedies_investigations_directorate, "uk-b50-govuk-trade-remedies-investigations-directorate",
     "Trade Remedies Investigations Directorate", "Trade Remedies Investigations Directorate",
     "government", "government",
     "https://www.gov.uk/government/organisations/trade-remedies-investigations-directorate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Trade Remedies Investigations Directorate\",\"government\",\"batch:50\"]",
     604800,
     "Trade Remedies Investigations Directorate — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_traffic_commissioners, "uk-b50-govuk-traffic-commissioners",
     "Traffic Commissioners for Great Britain", "Traffic Commissioners for Great Britain",
     "government", "government",
     "https://www.gov.uk/government/organisations/traffic-commissioners.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Traffic Commissioners for Great Britain\",\"government\",\"batch:50\"]",
     3600,
     "Traffic Commissioners for Great Britain — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_training_and_development_agency_for_schools, "uk-b50-govuk-training-and-development-agency-for-schools",
     "Training and Development Agency for Schools", "Training and Development Agency for Schools",
     "government", "government",
     "https://www.gov.uk/government/organisations/training-and-development-agency-for-schools.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Training and Development Agency for Schools\",\"government\",\"batch:50\"]",
     604800,
     "Training and Development Agency for Schools — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_transport_focus, "uk-b50-govuk-transport-focus",
     "Transport Focus", "Transport Focus",
     "government", "government",
     "https://www.gov.uk/government/organisations/transport-focus.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Transport Focus\",\"government\",\"batch:50\"]",
     21600,
     "Transport Focus — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_treasure_valuation_committee, "uk-b50-govuk-treasure-valuation-committee",
     "Treasure Valuation Committee", "Treasure Valuation Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/treasure-valuation-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Treasure Valuation Committee\",\"government\",\"batch:50\"]",
     21600,
     "Treasure Valuation Committee — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_treasury_solicitor_s_department, "uk-b50-govuk-treasury-solicitor-s-department",
     "Treasury Solicitor’s Department", "Treasury Solicitor’s Department",
     "government", "government",
     "https://www.gov.uk/government/organisations/treasury-solicitor-s-department.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Treasury Solicitor’s Department\",\"government\",\"batch:50\"]",
     604800,
     "Treasury Solicitor’s Department — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_tribunal_procedure_committee, "uk-b50-govuk-tribunal-procedure-committee",
     "Tribunal Procedure Committee", "Tribunal Procedure Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/tribunal-procedure-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Tribunal Procedure Committee\",\"government\",\"batch:50\"]",
     3600,
     "Tribunal Procedure Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_trinity_house, "uk-b50-govuk-trinity-house",
     "UK Trinity House", "UK Trinity House",
     "government", "government",
     "https://www.gov.uk/government/organisations/trinity-house.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Trinity House\",\"government\",\"batch:50\"]",
     21600,
     "UK Trinity House — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_anti_doping, "uk-b50-govuk-uk-anti-doping",
     "UK Uk Anti Doping", "UK Uk Anti Doping",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-anti-doping.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Uk Anti Doping\",\"government\",\"batch:50\"]",
     21600,
     "UK Uk Anti Doping — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_asset_resolution_limited, "uk-b50-govuk-uk-asset-resolution-limited",
     "UK Asset Resolution Limited", "UK Asset Resolution Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-asset-resolution-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Asset Resolution Limited\",\"government\",\"batch:50\"]",
     21600,
     "UK Asset Resolution Limited — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_uk_border_agency, "uk-b50-govuk-uk-border-agency",
     "UK Border Agency", "UK Border Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-border-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Border Agency\",\"government\",\"batch:50\"]",
     604800,
     "UK Border Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_co_ordinating_body, "uk-b50-govuk-uk-co-ordinating-body",
     "UK Co-ordinating Body", "UK Co-ordinating Body",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-co-ordinating-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Co-ordinating Body\",\"government\",\"batch:50\"]",
     604800,
     "UK Co-ordinating Body — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_uk_commission_for_employment_and_skills, "uk-b50-govuk-uk-commission-for-employment-and-skills",
     "UK Commission for Employment and Skills", "UK Commission for Employment and Skills",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-commission-for-employment-and-skills.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Commission for Employment and Skills\",\"government\",\"batch:50\"]",
     604800,
     "UK Commission for Employment and Skills — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_commission_on_covid_commemoration, "uk-b50-govuk-uk-commission-on-covid-commemoration",
     "UK Commission on Covid Commemoration", "UK Commission on Covid Commemoration",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-commission-on-covid-commemoration.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Commission on Covid Commemoration\",\"government\",\"batch:50\"]",
     3600,
     "UK Commission on Covid Commemoration — government feed (15 entries at probe)")

RSSX(uk_b50_govuk_uk_council_for_internet_safety, "uk-b50-govuk-uk-council-for-internet-safety",
     "UK Council for Internet Safety", "UK Council for Internet Safety",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-council-for-internet-safety.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Council for Internet Safety\",\"government\",\"batch:50\"]",
     3600,
     "UK Council for Internet Safety — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_uk_film_council, "uk-b50-govuk-uk-film-council",
     "UK Film Council", "UK Film Council",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-film-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Film Council\",\"government\",\"batch:50\"]",
     604800,
     "UK Film Council — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_uk_financial_investments_limited, "uk-b50-govuk-uk-financial-investments-limited",
     "UK Financial Investments Limited", "UK Financial Investments Limited",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-financial-investments-limited.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Financial Investments Limited\",\"government\",\"batch:50\"]",
     604800,
     "UK Financial Investments Limited — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_uk_fusion_energy, "uk-b50-govuk-uk-fusion-energy",
     "UK Fusion Energy", "UK Fusion Energy",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-fusion-energy.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Fusion Energy\",\"government\",\"batch:50\"]",
     3600,
     "UK Fusion Energy — government feed (9 entries at probe)")

RSSX(uk_b50_govuk_uk_government_decontamination_service, "uk-b50-govuk-uk-government-decontamination-service",
     "UK Government Decontamination Service", "UK Government Decontamination Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-government-decontamination-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Government Decontamination Service\",\"government\",\"batch:50\"]",
     604800,
     "UK Government Decontamination Service — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_uk_government_investments, "uk-b50-govuk-uk-government-investments",
     "UK Uk Government Investments", "UK Uk Government Investments",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-government-investments.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Uk Government Investments\",\"government\",\"batch:50\"]",
     21600,
     "UK Uk Government Investments — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_green_investment_bank, "uk-b50-govuk-uk-green-investment-bank",
     "UK Green Investment Bank", "UK Green Investment Bank",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-green-investment-bank.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Green Investment Bank\",\"government\",\"batch:50\"]",
     604800,
     "UK Green Investment Bank — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_uk_holocaust_memorial_foundation, "uk-b50-govuk-uk-holocaust-memorial-foundation",
     "UK Holocaust Memorial Foundation", "UK Holocaust Memorial Foundation",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-holocaust-memorial-foundation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Holocaust Memorial Foundation\",\"government\",\"batch:50\"]",
     3600,
     "UK Holocaust Memorial Foundation — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_industrial_fusion_solutions, "uk-b50-govuk-uk-industrial-fusion-solutions",
     "UK Industrial Fusion Solutions", "UK Industrial Fusion Solutions",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-industrial-fusion-solutions.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Industrial Fusion Solutions\",\"government\",\"batch:50\"]",
     604800,
     "UK Industrial Fusion Solutions — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_infrastructure_bank, "uk-b50-govuk-uk-infrastructure-bank",
     "UK Infrastructure Bank", "UK Infrastructure Bank",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-infrastructure-bank.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Infrastructure Bank\",\"government\",\"batch:50\"]",
     3600,
     "UK Infrastructure Bank — government feed (8 entries at probe)")

RSSX(uk_b50_govuk_uk_integrated_security_fund, "uk-b50-govuk-uk-integrated-security-fund",
     "UK Integrated Security Fund", "UK Integrated Security Fund",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-integrated-security-fund.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Integrated Security Fund\",\"government\",\"batch:50\"]",
     3600,
     "UK Integrated Security Fund — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_national_authority_for_counter_eavesdropping, "uk-b50-govuk-uk-national-authority-for-counter-eavesdropping",
     "UK National Authority for Counter-Eavesdropping", "UK National Authority for Counter-Eavesdropping",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-national-authority-for-counter-eavesdropping.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK National Authority for Counter-Eavesdropping\",\"government\",\"batch:50\"]",
     21600,
     "UK National Authority for Counter-Eavesdropping — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_uk_national_contact_point, "uk-b50-govuk-uk-national-contact-point",
     "UK National Contact Point", "UK National Contact Point",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-national-contact-point.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK National Contact Point\",\"government\",\"batch:50\"]",
     604800,
     "UK National Contact Point — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_national_screening_committee, "uk-b50-govuk-uk-national-screening-committee",
     "UK Uk National Screening Committee", "UK Uk National Screening Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-national-screening-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Uk National Screening Committee\",\"government\",\"batch:50\"]",
     3600,
     "UK Uk National Screening Committee — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_passport_service, "uk-b50-govuk-uk-passport-service",
     "UK Passport Service", "UK Passport Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-passport-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Passport Service\",\"government\",\"batch:50\"]",
     604800,
     "UK Passport Service — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_uk_resilience_academy, "uk-b50-govuk-uk-resilience-academy",
     "UK Resilience Academy", "UK Resilience Academy",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-resilience-academy.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Resilience Academy\",\"government\",\"batch:50\"]",
     3600,
     "UK Resilience Academy — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_uk_sport, "uk-b50-govuk-uk-sport",
     "UK Sport", "UK Sport",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-sport.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Sport\",\"government\",\"batch:50\"]",
     21600,
     "UK Sport — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_trade_investment, "uk-b50-govuk-uk-trade-investment",
     "UK Trade & Investment", "UK Trade & Investment",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-trade-investment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Trade & Investment\",\"government\",\"batch:50\"]",
     604800,
     "UK Trade & Investment — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_transplant, "uk-b50-govuk-uk-transplant",
     "UK Transplant", "UK Transplant",
     "government", "government",
     "https://www.gov.uk/government/organisations/uk-transplant.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Transplant\",\"government\",\"batch:50\"]",
     604800,
     "UK Transplant — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_united_kingdom_blood_transfusion_services, "uk-b50-govuk-united-kingdom-blood-transfusion-services",
     "United Kingdom Blood Transfusion Services", "United Kingdom Blood Transfusion Services",
     "government", "government",
     "https://www.gov.uk/government/organisations/united-kingdom-blood-transfusion-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:United Kingdom Blood Transfusion Services\",\"government\",\"batch:50\"]",
     604800,
     "United Kingdom Blood Transfusion Services — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_united_kingdom_security_vetting, "uk-b50-govuk-united-kingdom-security-vetting",
     "United Kingdom Security Vetting", "United Kingdom Security Vetting",
     "government", "government",
     "https://www.gov.uk/government/organisations/united-kingdom-security-vetting.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:United Kingdom Security Vetting\",\"government\",\"batch:50\"]",
     3600,
     "United Kingdom Security Vetting — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_valuation_and_lands_agency, "uk-b50-govuk-valuation-and-lands-agency",
     "Valuation and Lands Agency", "Valuation and Lands Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/valuation-and-lands-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Valuation and Lands Agency\",\"government\",\"batch:50\"]",
     604800,
     "Valuation and Lands Agency — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_valuation_tribunal_service_for_england_valuation_tribunal_service, "uk-b50-govuk-valuation-tribunal-service-for-england-valuation-tribunal-service",
     "Valuation Tribunal Service", "Valuation Tribunal Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/valuation-tribunal-service-for-england-valuation-tribunal-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Valuation Tribunal Service\",\"government\",\"batch:50\"]",
     21600,
     "Valuation Tribunal Service — government feed (16 entries at probe)")

RSSX(uk_b50_govuk_varney_review, "uk-b50-govuk-varney-review",
     "Varney Review", "Varney Review",
     "government", "government",
     "https://www.gov.uk/government/organisations/varney-review.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Varney Review\",\"government\",\"batch:50\"]",
     604800,
     "Varney Review — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_vehicle_and_operator_services_agency, "uk-b50-govuk-vehicle-and-operator-services-agency",
     "Vehicle and Operator Services Agency", "Vehicle and Operator Services Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/vehicle-and-operator-services-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Vehicle and Operator Services Agency\",\"government\",\"batch:50\"]",
     604800,
     "Vehicle and Operator Services Agency — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_vehicle_certification_agency, "uk-b50-govuk-vehicle-certification-agency",
     "UK VCA", "UK VCA",
     "government", "government",
     "https://www.gov.uk/government/organisations/vehicle-certification-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK VCA\",\"government\",\"batch:50\"]",
     21600,
     "UK VCA — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_veterans_advisory_and_pensions_committees_x13, "uk-b50-govuk-veterans-advisory-and-pensions-committees-x13",
     "Veterans Advisory and Pensions Committees", "Veterans Advisory and Pensions Committees",
     "government", "government",
     "https://www.gov.uk/government/organisations/veterans-advisory-and-pensions-committees-x13.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Veterans Advisory and Pensions Committees\",\"government\",\"batch:50\"]",
     3600,
     "Veterans Advisory and Pensions Committees — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_veterans_uk, "uk-b50-govuk-veterans-uk",
     "UK Veterans Uk", "UK Veterans Uk",
     "government", "government",
     "https://www.gov.uk/government/organisations/veterans-uk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Veterans Uk\",\"government\",\"batch:50\"]",
     3600,
     "UK Veterans Uk — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_veterinary_laboratories_agency, "uk-b50-govuk-veterinary-laboratories-agency",
     "Veterinary Laboratories Agency", "Veterinary Laboratories Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/veterinary-laboratories-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Veterinary Laboratories Agency\",\"government\",\"batch:50\"]",
     604800,
     "Veterinary Laboratories Agency — government feed (7 entries at probe)")

RSSX(uk_b50_govuk_veterinary_products_committee, "uk-b50-govuk-veterinary-products-committee",
     "Veterinary Products Committee", "Veterinary Products Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/veterinary-products-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Veterinary Products Committee\",\"government\",\"batch:50\"]",
     3600,
     "Veterinary Products Committee — government feed (4 entries at probe)")

RSSX(uk_b50_govuk_veterinary_residues_committee, "uk-b50-govuk-veterinary-residues-committee",
     "Veterinary Residues Committee", "Veterinary Residues Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/veterinary-residues-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Veterinary Residues Committee\",\"government\",\"batch:50\"]",
     604800,
     "Veterinary Residues Committee — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_victims_commissioner, "uk-b50-govuk-victims-commissioner",
     "Victims' Commissioner", "Victims' Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/victims-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Victims' Commissioner\",\"government\",\"batch:50\"]",
     21600,
     "Victims' Commissioner — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_victoria_and_albert_museum, "uk-b50-govuk-victoria-and-albert-museum",
     "Victoria and Albert Museum", "Victoria and Albert Museum",
     "government", "government",
     "https://www.gov.uk/government/organisations/victoria-and-albert-museum.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Victoria and Albert Museum\",\"government\",\"batch:50\"]",
     21600,
     "Victoria and Albert Museum — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_victoria_climbie_inquiry, "uk-b50-govuk-victoria-climbie-inquiry",
     "Victoria Climbié Inquiry", "Victoria Climbié Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/victoria-climbie-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Victoria Climbié Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Victoria Climbié Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_visitbritain, "uk-b50-govuk-visitbritain",
     "VisitBritain", "VisitBritain",
     "government", "government",
     "https://www.gov.uk/government/organisations/visitbritain.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:VisitBritain\",\"government\",\"batch:50\"]",
     21600,
     "VisitBritain — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_visitengland, "uk-b50-govuk-visitengland",
     "VisitEngland", "VisitEngland",
     "government", "government",
     "https://www.gov.uk/government/organisations/visitengland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:VisitEngland\",\"government\",\"batch:50\"]",
     21600,
     "VisitEngland — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_wakefield_education_action_zone, "uk-b50-govuk-wakefield-education-action-zone",
     "Wakefield Education Action Zone", "Wakefield Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/wakefield-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Wakefield Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Wakefield Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_wales_office, "uk-b50-govuk-wales-office",
     "UK Wales Office", "UK Wales Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/wales-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Wales Office\",\"government\",\"batch:50\"]",
     3600,
     "UK Wales Office — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_wallace_collection, "uk-b50-govuk-wallace-collection",
     "Wallace Collection", "Wallace Collection",
     "government", "government",
     "https://www.gov.uk/government/organisations/wallace-collection.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Wallace Collection\",\"government\",\"batch:50\"]",
     21600,
     "Wallace Collection — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_warship_support_agency, "uk-b50-govuk-warship-support-agency",
     "Warship Support Agency", "Warship Support Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/warship-support-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Warship Support Agency\",\"government\",\"batch:50\"]",
     604800,
     "Warship Support Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_water_service_northern_ireland, "uk-b50-govuk-water-service-northern-ireland",
     "Water Service (Northern Ireland)", "Water Service (Northern Ireland)",
     "government", "government",
     "https://www.gov.uk/government/organisations/water-service-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Water Service (Northern Ireland)\",\"government\",\"batch:50\"]",
     604800,
     "Water Service (Northern Ireland) — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_waterways_ireland, "uk-b50-govuk-waterways-ireland",
     "Waterways Ireland", "Waterways Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/waterways-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Waterways Ireland\",\"government\",\"batch:50\"]",
     604800,
     "Waterways Ireland — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_wednesbury_education_action_zone, "uk-b50-govuk-wednesbury-education-action-zone",
     "Wednesbury Education Action Zone", "Wednesbury Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/wednesbury-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Wednesbury Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Wednesbury Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_welsh_development_agency, "uk-b50-govuk-welsh-development-agency",
     "Welsh Development Agency", "Welsh Development Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/welsh-development-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Welsh Development Agency\",\"government\",\"batch:50\"]",
     604800,
     "Welsh Development Agency — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_welsh_government, "uk-b50-govuk-welsh-government",
     "Welsh Government", "Welsh Government",
     "government", "government",
     "https://www.gov.uk/government/organisations/welsh-government.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Welsh Government\",\"government\",\"batch:50\"]",
     21600,
     "Welsh Government — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_welsh_language_commissioner, "uk-b50-govuk-welsh-language-commissioner",
     "Welsh Language Commissioner", "Welsh Language Commissioner",
     "government", "government",
     "https://www.gov.uk/government/organisations/welsh-language-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Welsh Language Commissioner\",\"government\",\"batch:50\"]",
     21600,
     "Welsh Language Commissioner — government feed (3 entries at probe)")

RSSX(uk_b50_govuk_welsh_office, "uk-b50-govuk-welsh-office",
     "Welsh Office", "Welsh Office",
     "government", "government",
     "https://www.gov.uk/government/organisations/welsh-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Welsh Office\",\"government\",\"batch:50\"]",
     604800,
     "Welsh Office — government feed (13 entries at probe)")

RSSX(uk_b50_govuk_western_education_and_library_board, "uk-b50-govuk-western-education-and-library-board",
     "Western Education and Library Board", "Western Education and Library Board",
     "government", "government",
     "https://www.gov.uk/government/organisations/western-education-and-library-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Western Education and Library Board\",\"government\",\"batch:50\"]",
     604800,
     "Western Education and Library Board — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_westminster_education_action_zone, "uk-b50-govuk-westminster-education-action-zone",
     "Westminster Education Action Zone", "Westminster Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/westminster-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Westminster Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Westminster Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_westminster_foundation_for_democracy, "uk-b50-govuk-westminster-foundation-for-democracy",
     "Westminster Foundation for Democracy", "Westminster Foundation for Democracy",
     "government", "government",
     "https://www.gov.uk/government/organisations/westminster-foundation-for-democracy.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Westminster Foundation for Democracy\",\"government\",\"batch:50\"]",
     21600,
     "Westminster Foundation for Democracy — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_wilton_park, "uk-b50-govuk-wilton-park",
     "UK Wilton Park", "UK Wilton Park",
     "government", "government",
     "https://www.gov.uk/government/organisations/wilton-park.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Wilton Park\",\"government\",\"batch:50\"]",
     21600,
     "UK Wilton Park — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_windrush_commemoration_committee, "uk-b50-govuk-windrush-commemoration-committee",
     "Windrush Commemoration Committee", "Windrush Commemoration Committee",
     "government", "government",
     "https://www.gov.uk/government/organisations/windrush-commemoration-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Windrush Commemoration Committee\",\"government\",\"batch:50\"]",
     3600,
     "Windrush Commemoration Committee — government feed (10 entries at probe)")

RSSX(uk_b50_govuk_withernsea_and_southern_holderness_education_action_zone, "uk-b50-govuk-withernsea-and-southern-holderness-education-action-zone",
     "Withernsea and Southern Holderness Education Action Zone", "Withernsea and Southern Holderness Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/withernsea-and-southern-holderness-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Withernsea and Southern Holderness Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Withernsea and Southern Holderness Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_wolverhampton_education_action_zone, "uk-b50-govuk-wolverhampton-education-action-zone",
     "Wolverhampton Education Action Zone", "Wolverhampton Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/wolverhampton-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Wolverhampton Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Wolverhampton Education Action Zone — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_women_and_equalities_unit, "uk-b50-govuk-women-and-equalities-unit",
     "Women and Equalities Unit", "Women and Equalities Unit",
     "government", "government",
     "https://www.gov.uk/government/organisations/women-and-equalities-unit.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Women and Equalities Unit\",\"government\",\"batch:50\"]",
     3600,
     "Women and Equalities Unit — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_working_ventures_uk, "uk-b50-govuk-working-ventures-uk",
     "Working Ventures UK", "Working Ventures UK",
     "government", "government",
     "https://www.gov.uk/government/organisations/working-ventures-uk.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Working Ventures UK\",\"government\",\"batch:50\"]",
     604800,
     "Working Ventures UK — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_wythenshawe_education_action_zone, "uk-b50-govuk-wythenshawe-education-action-zone",
     "Wythenshawe Education Action Zone", "Wythenshawe Education Action Zone",
     "government", "government",
     "https://www.gov.uk/government/organisations/wythenshawe-education-action-zone.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Wythenshawe Education Action Zone\",\"government\",\"batch:50\"]",
     604800,
     "Wythenshawe Education Action Zone — government feed (1 entries at probe)")

RSSX(uk_b50_govuk_yorkshire_dales_national_park_authority, "uk-b50-govuk-yorkshire-dales-national-park-authority",
     "Yorkshire Dales National Park Authority", "Yorkshire Dales National Park Authority",
     "government", "government",
     "https://www.gov.uk/government/organisations/yorkshire-dales-national-park-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Yorkshire Dales National Park Authority\",\"government\",\"batch:50\"]",
     21600,
     "Yorkshire Dales National Park Authority — government feed (5 entries at probe)")

RSSX(uk_b50_govuk_yorkshire_forward, "uk-b50-govuk-yorkshire-forward",
     "Yorkshire Forward", "Yorkshire Forward",
     "government", "government",
     "https://www.gov.uk/government/organisations/yorkshire-forward.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Yorkshire Forward\",\"government\",\"batch:50\"]",
     604800,
     "Yorkshire Forward — government feed (6 entries at probe)")

RSSX(uk_b50_govuk_young_peoples_learning_agency, "uk-b50-govuk-young-peoples-learning-agency",
     "Young People's Learning Agency", "Young People's Learning Agency",
     "government", "government",
     "https://www.gov.uk/government/organisations/young-peoples-learning-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Young People's Learning Agency\",\"government\",\"batch:50\"]",
     604800,
     "Young People's Learning Agency — government feed (2 entries at probe)")

RSSX(uk_b50_govuk_youth_custody_service, "uk-b50-govuk-youth-custody-service",
     "Youth Custody Service", "Youth Custody Service",
     "government", "government",
     "https://www.gov.uk/government/organisations/youth-custody-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Youth Custody Service\",\"government\",\"batch:50\"]",
     3600,
     "Youth Custody Service — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_youth_justice_agency_of_northern_ireland, "uk-b50-govuk-youth-justice-agency-of-northern-ireland",
     "Youth Justice Agency of Northern Ireland", "Youth Justice Agency of Northern Ireland",
     "government", "government",
     "https://www.gov.uk/government/organisations/youth-justice-agency-of-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Youth Justice Agency of Northern Ireland\",\"government\",\"batch:50\"]",
     21600,
     "Youth Justice Agency of Northern Ireland — government feed (17 entries at probe)")

RSSX(uk_b50_govuk_youth_justice_board_for_england_and_wales, "uk-b50-govuk-youth-justice-board-for-england-and-wales",
     "UK Youth Justice Board For England And Wales", "UK Youth Justice Board For England And Wales",
     "government", "government",
     "https://www.gov.uk/government/organisations/youth-justice-board-for-england-and-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Youth Justice Board For England And Wales\",\"government\",\"batch:50\"]",
     3600,
     "UK Youth Justice Board For England And Wales — government feed (20 entries at probe)")

RSSX(uk_b50_govuk_zahid_mubarek_inquiry, "uk-b50-govuk-zahid-mubarek-inquiry",
     "Zahid Mubarek Inquiry", "Zahid Mubarek Inquiry",
     "government", "government",
     "https://www.gov.uk/government/organisations/zahid-mubarek-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Zahid Mubarek Inquiry\",\"government\",\"batch:50\"]",
     604800,
     "Zahid Mubarek Inquiry — government feed (1 entries at probe)")

RSSX(uk_b50_gravesham_gov_uk_news, "uk-b50-gravesham-gov-uk-news",
     "Gravesham", "Gravesham",
     "government", "government",
     "https://www.gravesham.gov.uk/rss/news",
     "en", "[\"gov\",\"country:uk\",\"publisher:Gravesham\",\"government\",\"batch:50\"]",
     3600,
     "Gravesham — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(uk_b50_greaterlondonlieutenancy_com, "uk-b50-greaterlondonlieutenancy-com",
     "Greater London", "Greater London",
     "government", "government",
     "https://greaterlondonlieutenancy.com/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Greater London\",\"government\",\"batch:50\"]",
     3600,
     "Greater London — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_hartlepool_gov_uk_news, "uk-b50-hartlepool-gov-uk-news",
     "Hartlepool", "Hartlepool",
     "government", "government",
     "https://www.hartlepool.gov.uk/rss/news",
     "en", "[\"gov\",\"country:uk\",\"publisher:Hartlepool\",\"government\",\"batch:50\"]",
     3600,
     "Hartlepool — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(uk_b50_healthcareimprovementscotland_scot, "uk-b50-healthcareimprovementscotland-scot",
     "Healthcare Improvement Scotland", "Healthcare Improvement Scotland",
     "government", "government",
     "https://www.healthcareimprovementscotland.scot/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Healthcare Improvement Scotland\",\"government\",\"batch:50\"]",
     3600,
     "Healthcare Improvement Scotland — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_londonconsortium_com, "uk-b50-londonconsortium-com",
     "London Consortium", "London Consortium",
     "government", "government",
     "https://www.londonconsortium.com/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:London Consortium\",\"government\",\"batch:50\"]",
     3600,
     "London Consortium — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 25 entries at probe")

RSSX(uk_b50_lsclondon_co_uk, "uk-b50-lsclondon-co-uk",
     "London School of Commerce", "London School of Commerce",
     "government", "government",
     "https://www.lsclondon.co.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:London School of Commerce\",\"government\",\"batch:50\"]",
     3600,
     "London School of Commerce — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 4 entries at probe")

RSSX(uk_b50_newcastle_staffs_gov_uk_news, "uk-b50-newcastle-staffs-gov-uk-news",
     "Newcastle-under-Lyme", "Newcastle-under-Lyme",
     "government", "government",
     "https://www.newcastle-staffs.gov.uk:443/rss/news",
     "en", "[\"gov\",\"country:uk\",\"publisher:Newcastle-under-Lyme\",\"government\",\"batch:50\"]",
     3600,
     "Newcastle-under-Lyme — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(uk_b50_nuneatonandbedworth_gov_uk_news, "uk-b50-nuneatonandbedworth-gov-uk-news",
     "Nuneaton and Bedworth", "Nuneaton and Bedworth",
     "government", "government",
     "https://www.nuneatonandbedworth.gov.uk:443/rss/news",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nuneaton and Bedworth\",\"government\",\"batch:50\"]",
     3600,
     "Nuneaton and Bedworth — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(uk_b50_resolution_nhs_uk, "uk-b50-resolution-nhs-uk",
     "NHS Resolution", "NHS Resolution",
     "government", "government",
     "https://resolution.nhs.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Resolution\",\"government\",\"batch:50\"]",
     3600,
     "NHS Resolution — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_rossendale_gov_uk_news, "uk-b50-rossendale-gov-uk-news",
     "Rossendale", "Rossendale",
     "government", "government",
     "https://www.rossendale.gov.uk:443/rss/news",
     "en", "[\"gov\",\"country:uk\",\"publisher:Rossendale\",\"government\",\"batch:50\"]",
     3600,
     "Rossendale — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(uk_b50_scot_nhs_uk, "uk-b50-scot-nhs-uk",
     "NHS Scotland", "NHS Scotland",
     "government", "government",
     "https://www.scot.nhs.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Scotland\",\"government\",\"batch:50\"]",
     3600,
     "NHS Scotland — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_scotblood_co_uk, "uk-b50-scotblood-co-uk",
     "Scottish National Blood Transfusion Service", "Scottish National Blood Transfusion Service",
     "government", "government",
     "https://www.scotblood.co.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scottish National Blood Transfusion Service\",\"government\",\"batch:50\"]",
     3600,
     "Scottish National Blood Transfusion Service — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_scrp_scot, "uk-b50-scrp-scot",
     "School Closure Review Panels", "School Closure Review Panels",
     "government", "government",
     "https://scrp.scot/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:School Closure Review Panels\",\"government\",\"batch:50\"]",
     3600,
     "School Closure Review Panels — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_thomasyoungcentre_org, "uk-b50-thomasyoungcentre-org",
     "Thomas Young Centre", "Thomas Young Centre",
     "government", "government",
     "https://thomasyoungcentre.org/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Thomas Young Centre\",\"government\",\"batch:50\"]",
     3600,
     "Thomas Young Centre — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(uk_b50_vod_communitybroadcasting_uk_videos, "uk-b50-vod-communitybroadcasting-uk-videos",
     "Community Broadcasting UK — Community Broadcasting UK - Videos feed", "Community Broadcasting UK — Community Broadcasting UK - Videos feed",
     "government", "government",
     "https://vod.communitybroadcasting.uk/feeds/videos.xml",
     "en", "[\"gov\",\"country:uk\",\"publisher:Community Broadcasting UK — Community Broadcasting UK - Videos feed\",\"government\",\"batch:50\"]",
     3600,
     "Community Broadcasting UK — Community Broadcasting UK - Videos feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

/* ── health ──────────────────────────────────────────────────────────── */

RSSX(uk_b50_govuk_animal_health, "uk-b50-govuk-animal-health",
     "Animal Health", "Animal Health",
     "health", "health",
     "https://www.gov.uk/government/organisations/animal-health.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Animal Health\",\"health\",\"batch:50\"]",
     604800,
     "Animal Health — health feed (4 entries at probe)")

RSSX(uk_b50_govuk_animal_health_and_veterinary_laboratories_agency, "uk-b50-govuk-animal-health-and-veterinary-laboratories-agency",
     "Animal Health and Veterinary Laboratories Agency", "Animal Health and Veterinary Laboratories Agency",
     "health", "health",
     "https://www.gov.uk/government/organisations/animal-health-and-veterinary-laboratories-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Animal Health and Veterinary Laboratories Agency\",\"health\",\"batch:50\"]",
     604800,
     "Animal Health and Veterinary Laboratories Agency — health feed (12 entries at probe)")

RSSX(uk_b50_govuk_commission_for_health_improvement, "uk-b50-govuk-commission-for-health-improvement",
     "Commission for Health Improvement", "Commission for Health Improvement",
     "health", "health",
     "https://www.gov.uk/government/organisations/commission-for-health-improvement.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for Health Improvement\",\"health\",\"batch:50\"]",
     604800,
     "Commission for Health Improvement — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_commission_for_patient_and_public_involvement_in_health, "uk-b50-govuk-commission-for-patient-and-public-involvement-in-health",
     "Commission for Patient and Public Involvement in Health", "Commission for Patient and Public Involvement in Health",
     "health", "health",
     "https://www.gov.uk/government/organisations/commission-for-patient-and-public-involvement-in-health.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission for Patient and Public Involvement in Health\",\"health\",\"batch:50\"]",
     604800,
     "Commission for Patient and Public Involvement in Health — health feed (3 entries at probe)")

RSSX(uk_b50_govuk_commission_on_human_medicines, "uk-b50-govuk-commission-on-human-medicines",
     "Commission on Human Medicines", "Commission on Human Medicines",
     "health", "health",
     "https://www.gov.uk/government/organisations/commission-on-human-medicines.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Commission on Human Medicines\",\"health\",\"batch:50\"]",
     3600,
     "Commission on Human Medicines — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_council_for_healthcare_regulatory_excellence, "uk-b50-govuk-council-for-healthcare-regulatory-excellence",
     "Council for Healthcare Regulatory Excellence", "Council for Healthcare Regulatory Excellence",
     "health", "health",
     "https://www.gov.uk/government/organisations/council-for-healthcare-regulatory-excellence.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Council for Healthcare Regulatory Excellence\",\"health\",\"batch:50\"]",
     604800,
     "Council for Healthcare Regulatory Excellence — health feed (11 entries at probe)")

RSSX(uk_b50_govuk_defence_medical_education_and_training_agency, "uk-b50-govuk-defence-medical-education-and-training-agency",
     "Defence Medical Education and Training Agency", "Defence Medical Education and Training Agency",
     "health", "health",
     "https://www.gov.uk/government/organisations/defence-medical-education-and-training-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defence Medical Education and Training Agency\",\"health\",\"batch:50\"]",
     604800,
     "Defence Medical Education and Training Agency — health feed (3 entries at probe)")

RSSX(uk_b50_govuk_department_of_health_northern_ireland, "uk-b50-govuk-department-of-health-northern-ireland",
     "Department of Health (Northern Ireland)", "Department of Health (Northern Ireland)",
     "health", "health",
     "https://www.gov.uk/government/organisations/department-of-health-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Health (Northern Ireland)\",\"health\",\"batch:50\"]",
     21600,
     "Department of Health (Northern Ireland) — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_of_health_social_services_and_public_safety, "uk-b50-govuk-department-of-health-social-services-and-public-safety",
     "Department of Health, Social Services and Public Safety", "Department of Health, Social Services and Public Safety",
     "health", "health",
     "https://www.gov.uk/government/organisations/department-of-health-social-services-and-public-safety.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department of Health, Social Services and Public Safety\",\"health\",\"batch:50\"]",
     604800,
     "Department of Health, Social Services and Public Safety — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_eastern_health_and_social_services_board, "uk-b50-govuk-eastern-health-and-social-services-board",
     "Eastern Health and Social Services Board", "Eastern Health and Social Services Board",
     "health", "health",
     "https://www.gov.uk/government/organisations/eastern-health-and-social-services-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Eastern Health and Social Services Board\",\"health\",\"batch:50\"]",
     604800,
     "Eastern Health and Social Services Board — health feed (2 entries at probe)")

RSSX(uk_b50_govuk_family_health_services_appeal_authority, "uk-b50-govuk-family-health-services-appeal-authority",
     "Family Health Services Appeal Authority", "Family Health Services Appeal Authority",
     "health", "health",
     "https://www.gov.uk/government/organisations/family-health-services-appeal-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Family Health Services Appeal Authority\",\"health\",\"batch:50\"]",
     604800,
     "Family Health Services Appeal Authority — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_health_and_social_care_information_centre, "uk-b50-govuk-health-and-social-care-information-centre",
     "Health and Social Care Information Centre", "Health and Social Care Information Centre",
     "health", "health",
     "https://www.gov.uk/government/organisations/health-and-social-care-information-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Health and Social Care Information Centre\",\"health\",\"batch:50\"]",
     604800,
     "Health and Social Care Information Centre — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_health_protection_agency, "uk-b50-govuk-health-protection-agency",
     "Health Protection Agency", "Health Protection Agency",
     "health", "health",
     "https://www.gov.uk/government/organisations/health-protection-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Health Protection Agency\",\"health\",\"batch:50\"]",
     604800,
     "Health Protection Agency — health feed (8 entries at probe)")

RSSX(uk_b50_govuk_health_research_authority, "uk-b50-govuk-health-research-authority",
     "Health Research Authority", "Health Research Authority",
     "health", "health",
     "https://www.gov.uk/government/organisations/health-research-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Health Research Authority\",\"health\",\"batch:50\"]",
     21600,
     "Health Research Authority — health feed (17 entries at probe)")

RSSX(uk_b50_govuk_health_services_safety_investigations_body, "uk-b50-govuk-health-services-safety-investigations-body",
     "Health Services Safety Investigations Body", "Health Services Safety Investigations Body",
     "health", "health",
     "https://www.gov.uk/government/organisations/health-services-safety-investigations-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Health Services Safety Investigations Body\",\"health\",\"batch:50\"]",
     21600,
     "Health Services Safety Investigations Body — health feed (6 entries at probe)")

RSSX(uk_b50_govuk_healthcare_commission, "uk-b50-govuk-healthcare-commission",
     "Healthcare Commission", "Healthcare Commission",
     "health", "health",
     "https://www.gov.uk/government/organisations/healthcare-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Healthcare Commission\",\"health\",\"batch:50\"]",
     604800,
     "Healthcare Commission — health feed (6 entries at probe)")

RSSX(uk_b50_govuk_independent_medical_expert_group, "uk-b50-govuk-independent-medical-expert-group",
     "Independent Medical Expert Group", "Independent Medical Expert Group",
     "health", "health",
     "https://www.gov.uk/government/organisations/independent-medical-expert-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Independent Medical Expert Group\",\"health\",\"batch:50\"]",
     3600,
     "Independent Medical Expert Group — health feed (13 entries at probe)")

RSSX(uk_b50_govuk_medical_supplies_agency, "uk-b50-govuk-medical-supplies-agency",
     "Medical Supplies Agency", "Medical Supplies Agency",
     "health", "health",
     "https://www.gov.uk/government/organisations/medical-supplies-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Medical Supplies Agency\",\"health\",\"batch:50\"]",
     604800,
     "Medical Supplies Agency — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_mental_health_act_commission, "uk-b50-govuk-mental-health-act-commission",
     "Mental Health Act Commission", "Mental Health Act Commission",
     "health", "health",
     "https://www.gov.uk/government/organisations/mental-health-act-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Mental Health Act Commission\",\"health\",\"batch:50\"]",
     604800,
     "Mental Health Act Commission — health feed (5 entries at probe)")

RSSX(uk_b50_govuk_mid_staffordshire_nhs_foundation_trust_public_inquiry, "uk-b50-govuk-mid-staffordshire-nhs-foundation-trust-public-inquiry",
     "Mid Staffordshire NHS Foundation Trust Public Inquiry 2013", "Mid Staffordshire NHS Foundation Trust Public Inquiry 2013",
     "health", "health",
     "https://www.gov.uk/government/organisations/mid-staffordshire-nhs-foundation-trust-public-inquiry.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Mid Staffordshire NHS Foundation Trust Public Inquiry 2013\",\"health\",\"batch:50\"]",
     604800,
     "Mid Staffordshire NHS Foundation Trust Public Inquiry 2013 — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_mid_staffordshire_nhs_foundation_trust_public_inquiry_2010, "uk-b50-govuk-mid-staffordshire-nhs-foundation-trust-public-inquiry-2010",
     "Mid Staffordshire NHS Foundation Trust Public Inquiry 2010", "Mid Staffordshire NHS Foundation Trust Public Inquiry 2010",
     "health", "health",
     "https://www.gov.uk/government/organisations/mid-staffordshire-nhs-foundation-trust-public-inquiry-2010.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Mid Staffordshire NHS Foundation Trust Public Inquiry 2010\",\"health\",\"batch:50\"]",
     604800,
     "Mid Staffordshire NHS Foundation Trust Public Inquiry 2010 — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_national_institute_for_clinical_excellence, "uk-b50-govuk-national-institute-for-clinical-excellence",
     "National Institute for Health and Care Excellence", "National Institute for Health and Care Excellence",
     "health", "health",
     "https://www.gov.uk/government/organisations/national-institute-for-clinical-excellence.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Institute for Health and Care Excellence\",\"health\",\"batch:50\"]",
     21600,
     "National Institute for Health and Care Excellence — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_nhs_appointments_commission, "uk-b50-govuk-nhs-appointments-commission",
     "NHS Appointments Commission", "NHS Appointments Commission",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-appointments-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Appointments Commission\",\"health\",\"batch:50\"]",
     604800,
     "NHS Appointments Commission — health feed (7 entries at probe)")

RSSX(uk_b50_govuk_nhs_blood_and_transplant, "uk-b50-govuk-nhs-blood-and-transplant",
     "NHS Blood and Transplant", "NHS Blood and Transplant",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-blood-and-transplant.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Blood and Transplant\",\"health\",\"batch:50\"]",
     21600,
     "NHS Blood and Transplant — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_nhs_counter_fraud_authority, "uk-b50-govuk-nhs-counter-fraud-authority",
     "NHS Counter Fraud Authority", "NHS Counter Fraud Authority",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-counter-fraud-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Counter Fraud Authority\",\"health\",\"batch:50\"]",
     21600,
     "NHS Counter Fraud Authority — health feed (10 entries at probe)")

RSSX(uk_b50_govuk_nhs_digital, "uk-b50-govuk-nhs-digital",
     "NHS Digital", "NHS Digital",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-digital.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Digital\",\"health\",\"batch:50\"]",
     604800,
     "NHS Digital — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_nhs_direct_national_health_service_trust, "uk-b50-govuk-nhs-direct-national-health-service-trust",
     "NHS Direct National Health Service Trust", "NHS Direct National Health Service Trust",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-direct-national-health-service-trust.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Direct National Health Service Trust\",\"health\",\"batch:50\"]",
     604800,
     "NHS Direct National Health Service Trust — health feed (9 entries at probe)")

RSSX(uk_b50_govuk_nhs_estates, "uk-b50-govuk-nhs-estates",
     "NHS Estates", "NHS Estates",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-estates.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Estates\",\"health\",\"batch:50\"]",
     604800,
     "NHS Estates — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_nhs_improvement, "uk-b50-govuk-nhs-improvement",
     "NHS Improvement", "NHS Improvement",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-improvement.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Improvement\",\"health\",\"batch:50\"]",
     604800,
     "NHS Improvement — health feed (13 entries at probe)")

RSSX(uk_b50_govuk_nhs_institute_for_innovation_and_improvement, "uk-b50-govuk-nhs-institute-for-innovation-and-improvement",
     "NHS Institute for Innovation and Improvement", "NHS Institute for Innovation and Improvement",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-institute-for-innovation-and-improvement.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Institute for Innovation and Improvement\",\"health\",\"batch:50\"]",
     604800,
     "NHS Institute for Innovation and Improvement — health feed (8 entries at probe)")

RSSX(uk_b50_govuk_nhs_litigation_authority, "uk-b50-govuk-nhs-litigation-authority",
     "NHS Litigation Authority", "NHS Litigation Authority",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-litigation-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Litigation Authority\",\"health\",\"batch:50\"]",
     604800,
     "NHS Litigation Authority — health feed (11 entries at probe)")

RSSX(uk_b50_govuk_nhs_logistics_authority, "uk-b50-govuk-nhs-logistics-authority",
     "NHS Logistics Authority", "NHS Logistics Authority",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-logistics-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Logistics Authority\",\"health\",\"batch:50\"]",
     604800,
     "NHS Logistics Authority — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_nhs_pay_review_body, "uk-b50-govuk-nhs-pay-review-body",
     "NHS Pay Review Body", "NHS Pay Review Body",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-pay-review-body.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Pay Review Body\",\"health\",\"batch:50\"]",
     3600,
     "NHS Pay Review Body — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_nhs_pensions_agency, "uk-b50-govuk-nhs-pensions-agency",
     "NHS Pensions Agency", "NHS Pensions Agency",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-pensions-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Pensions Agency\",\"health\",\"batch:50\"]",
     604800,
     "NHS Pensions Agency — health feed (6 entries at probe)")

RSSX(uk_b50_govuk_nhs_professionals, "uk-b50-govuk-nhs-professionals",
     "NHS Professionals", "NHS Professionals",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-professionals.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Professionals\",\"health\",\"batch:50\"]",
     604800,
     "NHS Professionals — health feed (4 entries at probe)")

RSSX(uk_b50_govuk_nhs_purchasing_and_supply_agency, "uk-b50-govuk-nhs-purchasing-and-supply-agency",
     "NHS Purchasing and Supply Agency", "NHS Purchasing and Supply Agency",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-purchasing-and-supply-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Purchasing and Supply Agency\",\"health\",\"batch:50\"]",
     604800,
     "NHS Purchasing and Supply Agency — health feed (6 entries at probe)")

RSSX(uk_b50_govuk_nhs_resolution, "uk-b50-govuk-nhs-resolution",
     "NHS Resolution", "NHS Resolution",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-resolution.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Resolution\",\"health\",\"batch:50\"]",
     21600,
     "NHS Resolution — health feed (9 entries at probe)")

RSSX(uk_b50_govuk_nhs_trust_development_authority, "uk-b50-govuk-nhs-trust-development-authority",
     "NHS Trust Development Authority", "NHS Trust Development Authority",
     "health", "health",
     "https://www.gov.uk/government/organisations/nhs-trust-development-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS Trust Development Authority\",\"health\",\"batch:50\"]",
     604800,
     "NHS Trust Development Authority — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_northern_health_and_social_services_board, "uk-b50-govuk-northern-health-and-social-services-board",
     "Northern Health and Social Services Board", "Northern Health and Social Services Board",
     "health", "health",
     "https://www.gov.uk/government/organisations/northern-health-and-social-services-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Health and Social Services Board\",\"health\",\"batch:50\"]",
     604800,
     "Northern Health and Social Services Board — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_health_and_social_services_estates_agency, "uk-b50-govuk-northern-ireland-health-and-social-services-estates-agency",
     "Northern Ireland Health and Social Services Estates Agency", "Northern Ireland Health and Social Services Estates Agency",
     "health", "health",
     "https://www.gov.uk/government/organisations/northern-ireland-health-and-social-services-estates-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Health and Social Services Estates Agency\",\"health\",\"batch:50\"]",
     604800,
     "Northern Ireland Health and Social Services Estates Agency — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_office_for_health_improvement_and_disparities, "uk-b50-govuk-office-for-health-improvement-and-disparities",
     "Office for Health Improvement and Disparities", "Office for Health Improvement and Disparities",
     "health", "health",
     "https://www.gov.uk/government/organisations/office-for-health-improvement-and-disparities.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Health Improvement and Disparities\",\"health\",\"batch:50\"]",
     3600,
     "Office for Health Improvement and Disparities — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_health_professions_adjudicator, "uk-b50-govuk-office-of-the-health-professions-adjudicator",
     "Office of the Health Professions Adjudicator", "Office of the Health Professions Adjudicator",
     "health", "health",
     "https://www.gov.uk/government/organisations/office-of-the-health-professions-adjudicator.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Health Professions Adjudicator\",\"health\",\"batch:50\"]",
     604800,
     "Office of the Health Professions Adjudicator — health feed (4 entries at probe)")

RSSX(uk_b50_govuk_postgraduate_medical_education_and_training_board, "uk-b50-govuk-postgraduate-medical-education-and-training-board",
     "Postgraduate Medical Education and Training Board", "Postgraduate Medical Education and Training Board",
     "health", "health",
     "https://www.gov.uk/government/organisations/postgraduate-medical-education-and-training-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Postgraduate Medical Education and Training Board\",\"health\",\"batch:50\"]",
     604800,
     "Postgraduate Medical Education and Training Board — health feed (7 entries at probe)")

RSSX(uk_b50_govuk_professional_standards_authority_for_health_and_social_care, "uk-b50-govuk-professional-standards-authority-for-health-and-social-care",
     "Professional Standards Authority for Health and Social Care", "Professional Standards Authority for Health and Social Care",
     "health", "health",
     "https://www.gov.uk/government/organisations/professional-standards-authority-for-health-and-social-care.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Professional Standards Authority for Health and Social Care\",\"health\",\"batch:50\"]",
     21600,
     "Professional Standards Authority for Health and Social Care — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_public_health_agency_northern_ireland, "uk-b50-govuk-public-health-agency-northern-ireland",
     "Public Health Agency (Northern Ireland)", "Public Health Agency (Northern Ireland)",
     "health", "health",
     "https://www.gov.uk/government/organisations/public-health-agency-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Public Health Agency (Northern Ireland)\",\"health\",\"batch:50\"]",
     21600,
     "Public Health Agency (Northern Ireland) — health feed (16 entries at probe)")

RSSX(uk_b50_govuk_public_health_laboratory_service_board, "uk-b50-govuk-public-health-laboratory-service-board",
     "Public Health Laboratory Service Board", "Public Health Laboratory Service Board",
     "health", "health",
     "https://www.gov.uk/government/organisations/public-health-laboratory-service-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Public Health Laboratory Service Board\",\"health\",\"batch:50\"]",
     604800,
     "Public Health Laboratory Service Board — health feed (3 entries at probe)")

RSSX(uk_b50_govuk_public_health_wales, "uk-b50-govuk-public-health-wales",
     "Public Health Wales", "Public Health Wales",
     "health", "health",
     "https://www.gov.uk/government/organisations/public-health-wales.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Public Health Wales\",\"health\",\"batch:50\"]",
     21600,
     "Public Health Wales — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_review_body_for_nursing_and_other_health_professions, "uk-b50-govuk-review-body-for-nursing-and-other-health-professions",
     "Review Body for Nursing and Other Health Professions", "Review Body for Nursing and Other Health Professions",
     "health", "health",
     "https://www.gov.uk/government/organisations/review-body-for-nursing-and-other-health-professions.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Review Body for Nursing and Other Health Professions\",\"health\",\"batch:50\"]",
     604800,
     "Review Body for Nursing and Other Health Professions — health feed (2 entries at probe)")

RSSX(uk_b50_govuk_science_advisory_committee_on_the_medical_implications_of_less_lethal_weapons, "uk-b50-govuk-science-advisory-committee-on-the-medical-implications-of-less-lethal-weapons",
     "Scientific Advisory Committee on the Medical Implications of Less-Lethal Weapons", "Scientific Advisory Committee on the Medical Implications of Less-Lethal Weapons",
     "health", "health",
     "https://www.gov.uk/government/organisations/science-advisory-committee-on-the-medical-implications-of-less-lethal-weapons.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Scientific Advisory Committee on the Medical Implications of Less-Lethal Weapons\",\"health\",\"batch:50\"]",
     604800,
     "Scientific Advisory Committee on the Medical Implications of Less-Lethal Weapons — health feed (5 entries at probe)")

RSSX(uk_b50_govuk_southern_health_and_social_services_board, "uk-b50-govuk-southern-health-and-social-services-board",
     "Southern Health and Social Services Board", "Southern Health and Social Services Board",
     "health", "health",
     "https://www.gov.uk/government/organisations/southern-health-and-social-services-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Southern Health and Social Services Board\",\"health\",\"batch:50\"]",
     604800,
     "Southern Health and Social Services Board — health feed (1 entries at probe)")

RSSX(uk_b50_govuk_the_centre_for_climate_health_security, "uk-b50-govuk-the-centre-for-climate-health-security",
     "Centre for Climate and Health Security", "Centre for Climate and Health Security",
     "health", "health",
     "https://www.gov.uk/government/organisations/the-centre-for-climate-health-security.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Centre for Climate and Health Security\",\"health\",\"batch:50\"]",
     3600,
     "Centre for Climate and Health Security — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_the_parliamentary_and_health_service_ombudsman, "uk-b50-govuk-the-parliamentary-and-health-service-ombudsman",
     "Parliamentary and Health Service Ombudsman", "Parliamentary and Health Service Ombudsman",
     "health", "health",
     "https://www.gov.uk/government/organisations/the-parliamentary-and-health-service-ombudsman.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Parliamentary and Health Service Ombudsman\",\"health\",\"batch:50\"]",
     604800,
     "Parliamentary and Health Service Ombudsman — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_veterinary_medicines_directorate, "uk-b50-govuk-veterinary-medicines-directorate",
     "UK VMD", "UK VMD",
     "health", "health",
     "https://www.gov.uk/government/organisations/veterinary-medicines-directorate.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK VMD\",\"health\",\"batch:50\"]",
     3600,
     "UK VMD — health feed (20 entries at probe)")

RSSX(uk_b50_govuk_western_health_and_social_services_board, "uk-b50-govuk-western-health-and-social-services-board",
     "Western Health and Social Services Board", "Western Health and Social Services Board",
     "health", "health",
     "https://www.gov.uk/government/organisations/western-health-and-social-services-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Western Health and Social Services Board\",\"health\",\"batch:50\"]",
     604800,
     "Western Health and Social Services Board — health feed (1 entries at probe)")

/* ── intelligence ────────────────────────────────────────────────────── */

RSSX(uk_b50_govuk_civil_service_intelligence_analysis_profession, "uk-b50-govuk-civil-service-intelligence-analysis-profession",
     "Intelligence Analysis", "Intelligence Analysis",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/civil-service-intelligence-analysis-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Intelligence Analysis\",\"intelligence\",\"batch:50\"]",
     3600,
     "Intelligence Analysis — intelligence feed (3 entries at probe)")

RSSX(uk_b50_govuk_government_communications_headquarters, "uk-b50-govuk-government-communications-headquarters",
     "UK GCHQ", "UK GCHQ",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/government-communications-headquarters.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK GCHQ\",\"intelligence\",\"batch:50\"]",
     21600,
     "UK GCHQ — intelligence feed (20 entries at probe)")

RSSX(uk_b50_govuk_incubator_for_artificial_intelligence, "uk-b50-govuk-incubator-for-artificial-intelligence",
     "Incubator for Artificial Intelligence", "Incubator for Artificial Intelligence",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/incubator-for-artificial-intelligence.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Incubator for Artificial Intelligence\",\"intelligence\",\"batch:50\"]",
     604800,
     "Incubator for Artificial Intelligence — intelligence feed (1 entries at probe)")

RSSX(uk_b50_govuk_intelligence_services_commissioner, "uk-b50-govuk-intelligence-services-commissioner",
     "Intelligence Services Commissioner", "Intelligence Services Commissioner",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/intelligence-services-commissioner.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Intelligence Services Commissioner\",\"intelligence\",\"batch:50\"]",
     604800,
     "Intelligence Services Commissioner — intelligence feed (14 entries at probe)")

RSSX(uk_b50_govuk_national_criminal_intelligence_service, "uk-b50-govuk-national-criminal-intelligence-service",
     "National Criminal Intelligence Service", "National Criminal Intelligence Service",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/national-criminal-intelligence-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Criminal Intelligence Service\",\"intelligence\",\"batch:50\"]",
     604800,
     "National Criminal Intelligence Service — intelligence feed (2 entries at probe)")

RSSX(uk_b50_govuk_national_security, "uk-b50-govuk-national-security",
     "National Security and Intelligence", "National Security and Intelligence",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/national-security.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Security and Intelligence\",\"intelligence\",\"batch:50\"]",
     3600,
     "National Security and Intelligence — intelligence feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_artificial_intelligence, "uk-b50-govuk-office-for-artificial-intelligence",
     "Office for Artificial Intelligence", "Office for Artificial Intelligence",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/office-for-artificial-intelligence.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Artificial Intelligence\",\"intelligence\",\"batch:50\"]",
     604800,
     "Office for Artificial Intelligence — intelligence feed (20 entries at probe)")

RSSX(uk_b50_govuk_secret_intelligence_service, "uk-b50-govuk-secret-intelligence-service",
     "UK MI6 SIS", "UK MI6 SIS",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/secret-intelligence-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK MI6 SIS\",\"intelligence\",\"batch:50\"]",
     21600,
     "UK MI6 SIS — intelligence feed (15 entries at probe)")

RSSX(uk_b50_govuk_the_security_service_mi5, "uk-b50-govuk-the-security-service-mi5",
     "UK MI5", "UK MI5",
     "intelligence", "intelligence",
     "https://www.gov.uk/government/organisations/the-security-service-mi5.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK MI5\",\"intelligence\",\"batch:50\"]",
     21600,
     "UK MI5 — intelligence feed (14 entries at probe)")

/* ── legal ───────────────────────────────────────────────────────────── */

RSSX(uk_b50_services_parliament_uk_bills_allbills, "uk-b50-services-parliament-uk-bills-allbills",
     "UK Parliament Bills", "UK Parliament Bills",
     "legal", "legal",
     "https://services.parliament.uk/bills/AllBills.rss",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Parliament Bills\",\"legal\",\"batch:50\"]",
     3600,
     "UK Parliament Bills — legal feed (50 entries at probe)")

/* ── news ────────────────────────────────────────────────────────────── */

RSSX(uk_b50_feeds_bbci_co_uk_news, "uk-b50-feeds-bbci-co-uk-news",
     "BBC News", "BBC News",
     "news", "media",
     "https://feeds.bbci.co.uk/news/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC News\",\"media\",\"batch:50\"]",
     3600,
     "BBC News — news feed (36 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_business, "uk-b50-feeds-bbci-co-uk-news-business",
     "BBC Business", "BBC Business",
     "news", "media",
     "https://feeds.bbci.co.uk/news/business/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC Business\",\"media\",\"batch:50\"]",
     3600,
     "BBC Business — news feed (53 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_education, "uk-b50-feeds-bbci-co-uk-news-education",
     "BBC Education", "BBC Education",
     "news", "media",
     "https://feeds.bbci.co.uk/news/education/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC Education\",\"media\",\"batch:50\"]",
     3600,
     "BBC Education — news feed (23 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_entertainment_and_arts, "uk-b50-feeds-bbci-co-uk-news-entertainment-and-arts",
     "BBC Entertainment", "BBC Entertainment",
     "news", "media",
     "https://feeds.bbci.co.uk/news/entertainment_and_arts/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC Entertainment\",\"media\",\"batch:50\"]",
     3600,
     "BBC Entertainment — news feed (26 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_health, "uk-b50-feeds-bbci-co-uk-news-health",
     "BBC Health", "BBC Health",
     "news", "media",
     "https://feeds.bbci.co.uk/news/health/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC Health\",\"media\",\"batch:50\"]",
     3600,
     "BBC Health — news feed (56 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_politics, "uk-b50-feeds-bbci-co-uk-news-politics",
     "BBC Politics", "BBC Politics",
     "news", "media",
     "https://feeds.bbci.co.uk/news/politics/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC Politics\",\"media\",\"batch:50\"]",
     3600,
     "BBC Politics — news feed (55 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_science_and_environment, "uk-b50-feeds-bbci-co-uk-news-science-and-environment",
     "BBC Science", "BBC Science",
     "news", "media",
     "https://feeds.bbci.co.uk/news/science_and_environment/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC Science\",\"media\",\"batch:50\"]",
     3600,
     "BBC Science — news feed (41 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_technology, "uk-b50-feeds-bbci-co-uk-news-technology",
     "BBC Tech", "BBC Tech",
     "news", "media",
     "https://feeds.bbci.co.uk/news/technology/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC Tech\",\"media\",\"batch:50\"]",
     3600,
     "BBC Tech — news feed (21 entries at probe)")

RSSX(uk_b50_feeds_bbci_co_uk_news_uk, "uk-b50-feeds-bbci-co-uk-news-uk",
     "BBC UK", "BBC UK",
     "news", "media",
     "https://feeds.bbci.co.uk/news/uk/rss.xml",
     "en", "[\"news\",\"country:uk\",\"publisher:BBC UK\",\"media\",\"batch:50\"]",
     3600,
     "BBC UK — news feed (27 entries at probe)")

RSSX(uk_b50_theguardian_com_uk, "uk-b50-theguardian-com-uk",
     "Guardian UK", "Guardian UK",
     "news", "media",
     "https://www.theguardian.com/uk/rss",
     "en", "[\"news\",\"country:uk\",\"publisher:Guardian UK\",\"media\",\"batch:50\"]",
     3600,
     "Guardian UK — news feed (142 entries at probe)")

/* ── science ─────────────────────────────────────────────────────────── */

RSSX(uk_b50_chi_ac_uk, "uk-b50-chi-ac-uk",
     "University of Chichester", "University of Chichester",
     "science", "science",
     "https://www.chi.ac.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:University of Chichester\",\"science\",\"batch:50\"]",
     10800,
     "University of Chichester — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(uk_b50_govuk_advanced_research_and_invention_agency, "uk-b50-govuk-advanced-research-and-invention-agency",
     "Advanced Research and Invention Agency", "Advanced Research and Invention Agency",
     "science", "science",
     "https://www.gov.uk/government/organisations/advanced-research-and-invention-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advanced Research and Invention Agency\",\"science\",\"batch:50\"]",
     21600,
     "Advanced Research and Invention Agency — science feed (13 entries at probe)")

RSSX(uk_b50_govuk_advisory_committee_for_social_science, "uk-b50-govuk-advisory-committee-for-social-science",
     "Advisory Committee for Social Science", "Advisory Committee for Social Science",
     "science", "science",
     "https://www.gov.uk/government/organisations/advisory-committee-for-social-science.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Advisory Committee for Social Science\",\"science\",\"batch:50\"]",
     3600,
     "Advisory Committee for Social Science — science feed (12 entries at probe)")

RSSX(uk_b50_govuk_agri_food_and_biosciences_institute, "uk-b50-govuk-agri-food-and-biosciences-institute",
     "Agri-Food and Biosciences Institute (Northern Ireland)", "Agri-Food and Biosciences Institute (Northern Ireland)",
     "science", "science",
     "https://www.gov.uk/government/organisations/agri-food-and-biosciences-institute.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Agri-Food and Biosciences Institute (Northern Ireland)\",\"science\",\"batch:50\"]",
     21600,
     "Agri-Food and Biosciences Institute (Northern Ireland) — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_alcohol_education_and_research_council, "uk-b50-govuk-alcohol-education-and-research-council",
     "Alcohol Education and Research Council", "Alcohol Education and Research Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/alcohol-education-and-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Alcohol Education and Research Council\",\"science\",\"batch:50\"]",
     604800,
     "Alcohol Education and Research Council — science feed (7 entries at probe)")

RSSX(uk_b50_govuk_animal_and_plant_health_agency, "uk-b50-govuk-animal-and-plant-health-agency",
     "UK APHA", "UK APHA",
     "science", "science",
     "https://www.gov.uk/government/organisations/animal-and-plant-health-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK APHA\",\"science\",\"batch:50\"]",
     3600,
     "UK APHA — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_animals_in_science_committee, "uk-b50-govuk-animals-in-science-committee",
     "Animals in Science Committee", "Animals in Science Committee",
     "science", "science",
     "https://www.gov.uk/government/organisations/animals-in-science-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Animals in Science Committee\",\"science\",\"batch:50\"]",
     3600,
     "Animals in Science Committee — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_arts_and_humanities_research_council, "uk-b50-govuk-arts-and-humanities-research-council",
     "Arts and Humanities Research Council", "Arts and Humanities Research Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/arts-and-humanities-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Arts and Humanities Research Council\",\"science\",\"batch:50\"]",
     21600,
     "Arts and Humanities Research Council — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_awe_nuclear_security_technologies, "uk-b50-govuk-awe-nuclear-security-technologies",
     "AWE Nuclear Security Technologies", "AWE Nuclear Security Technologies",
     "science", "science",
     "https://www.gov.uk/government/organisations/awe-nuclear-security-technologies.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:AWE Nuclear Security Technologies\",\"science\",\"batch:50\"]",
     21600,
     "AWE Nuclear Security Technologies — science feed (2 entries at probe)")

RSSX(uk_b50_govuk_biotechnology_biological_sciences_research_council, "uk-b50-govuk-biotechnology-biological-sciences-research-council",
     "Biotechnology and Biological Sciences Research Council", "Biotechnology and Biological Sciences Research Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/biotechnology-biological-sciences-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Biotechnology and Biological Sciences Research Council\",\"science\",\"batch:50\"]",
     21600,
     "Biotechnology and Biological Sciences Research Council — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_building_research_establishment, "uk-b50-govuk-building-research-establishment",
     "Building Research Establishment", "Building Research Establishment",
     "science", "science",
     "https://www.gov.uk/government/organisations/building-research-establishment.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Building Research Establishment\",\"science\",\"batch:50\"]",
     604800,
     "Building Research Establishment — science feed (1 entries at probe)")

RSSX(uk_b50_govuk_central_science_laboratory, "uk-b50-govuk-central-science-laboratory",
     "Central Science Laboratory", "Central Science Laboratory",
     "science", "science",
     "https://www.gov.uk/government/organisations/central-science-laboratory.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Central Science Laboratory\",\"science\",\"batch:50\"]",
     604800,
     "Central Science Laboratory — science feed (4 entries at probe)")

RSSX(uk_b50_govuk_centre_for_environment_fisheries_and_aquaculture_science, "uk-b50-govuk-centre-for-environment-fisheries-and-aquaculture-science",
     "UK Cefas", "UK Cefas",
     "science", "science",
     "https://www.gov.uk/government/organisations/centre-for-environment-fisheries-and-aquaculture-science.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Cefas\",\"science\",\"batch:50\"]",
     3600,
     "UK Cefas — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_operational_research_service, "uk-b50-govuk-civil-service-government-operational-research-service",
     "Government Operational Research Service", "Government Operational Research Service",
     "science", "science",
     "https://www.gov.uk/government/organisations/civil-service-government-operational-research-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Operational Research Service\",\"science\",\"batch:50\"]",
     3600,
     "Government Operational Research Service — science feed (10 entries at probe)")

RSSX(uk_b50_govuk_civil_service_government_science_engineering, "uk-b50-govuk-civil-service-government-science-engineering",
     "Government Science & Engineering Profession", "Government Science & Engineering Profession",
     "science", "science",
     "https://www.gov.uk/government/organisations/civil-service-government-science-engineering.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Science & Engineering Profession\",\"science\",\"batch:50\"]",
     3600,
     "Government Science & Engineering Profession — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_council_for_science_and_technology, "uk-b50-govuk-council-for-science-and-technology",
     "Council for Science and Technology", "Council for Science and Technology",
     "science", "science",
     "https://www.gov.uk/government/organisations/council-for-science-and-technology.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Council for Science and Technology\",\"science\",\"batch:50\"]",
     3600,
     "Council for Science and Technology — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_council_for_the_central_laboratory_of_the_research_councils, "uk-b50-govuk-council-for-the-central-laboratory-of-the-research-councils",
     "Council for the Central Laboratory of the Research Councils", "Council for the Central Laboratory of the Research Councils",
     "science", "science",
     "https://www.gov.uk/government/organisations/council-for-the-central-laboratory-of-the-research-councils.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Council for the Central Laboratory of the Research Councils\",\"science\",\"batch:50\"]",
     604800,
     "Council for the Central Laboratory of the Research Councils — science feed (2 entries at probe)")

RSSX(uk_b50_govuk_department_for_business_innovation_science_and_trade, "uk-b50-govuk-department-for-business-innovation-science-and-trade",
     "Department for Business, Innovation, Science and Trade", "Department for Business, Innovation, Science and Trade",
     "science", "science",
     "https://www.gov.uk/government/organisations/department-for-business-innovation-science-and-trade.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Business, Innovation, Science and Trade\",\"science\",\"batch:50\"]",
     3600,
     "Department for Business, Innovation, Science and Trade — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_business_innovation_skills, "uk-b50-govuk-department-for-business-innovation-skills",
     "Department for Business, Innovation & Skills", "Department for Business, Innovation & Skills",
     "science", "science",
     "https://www.gov.uk/government/organisations/department-for-business-innovation-skills.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Business, Innovation & Skills\",\"science\",\"batch:50\"]",
     604800,
     "Department for Business, Innovation & Skills — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_department_for_innovation_universities_and_skills, "uk-b50-govuk-department-for-innovation-universities-and-skills",
     "Department for Innovation, Universities and Skills", "Department for Innovation, Universities and Skills",
     "science", "science",
     "https://www.gov.uk/government/organisations/department-for-innovation-universities-and-skills.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Department for Innovation, Universities and Skills\",\"science\",\"batch:50\"]",
     604800,
     "Department for Innovation, Universities and Skills — science feed (14 entries at probe)")

RSSX(uk_b50_govuk_economic_and_social_research_council, "uk-b50-govuk-economic-and-social-research-council",
     "Economic and Social Research Council", "Economic and Social Research Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/economic-and-social-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Economic and Social Research Council\",\"science\",\"batch:50\"]",
     21600,
     "Economic and Social Research Council — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_engineering_and_physical_sciences_research_council, "uk-b50-govuk-engineering-and-physical-sciences-research-council",
     "Engineering and Physical Sciences Research Council", "Engineering and Physical Sciences Research Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/engineering-and-physical-sciences-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Engineering and Physical Sciences Research Council\",\"science\",\"batch:50\"]",
     21600,
     "Engineering and Physical Sciences Research Council — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_flood_and_coastal_erosion_risk_management_research_and_development_programme, "uk-b50-govuk-flood-and-coastal-erosion-risk-management-research-and-development-programme",
     "Flood and Coastal Erosion Risk Management Research and Development Programme", "Flood and Coastal Erosion Risk Management Research and Development Programme",
     "science", "science",
     "https://www.gov.uk/government/organisations/flood-and-coastal-erosion-risk-management-research-and-development-programme.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Flood and Coastal Erosion Risk Management Research and Development Programme\",\"science\",\"batch:50\"]",
     3600,
     "Flood and Coastal Erosion Risk Management Research and Development Programme — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_forensic_science_northern_ireland, "uk-b50-govuk-forensic-science-northern-ireland",
     "Forensic Science Northern Ireland", "Forensic Science Northern Ireland",
     "science", "science",
     "https://www.gov.uk/government/organisations/forensic-science-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Forensic Science Northern Ireland\",\"science\",\"batch:50\"]",
     604800,
     "Forensic Science Northern Ireland — science feed (6 entries at probe)")

RSSX(uk_b50_govuk_forensic_science_service, "uk-b50-govuk-forensic-science-service",
     "Forensic Science Service", "Forensic Science Service",
     "science", "science",
     "https://www.gov.uk/government/organisations/forensic-science-service.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Forensic Science Service\",\"science\",\"batch:50\"]",
     604800,
     "Forensic Science Service — science feed (1 entries at probe)")

RSSX(uk_b50_govuk_forest_research, "uk-b50-govuk-forest-research",
     "Forest Research", "Forest Research",
     "science", "science",
     "https://www.gov.uk/government/organisations/forest-research.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Forest Research\",\"science\",\"batch:50\"]",
     21600,
     "Forest Research — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_government_office_for_science, "uk-b50-govuk-government-office-for-science",
     "Government Office for Science", "Government Office for Science",
     "science", "science",
     "https://www.gov.uk/government/organisations/government-office-for-science.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Office for Science\",\"science\",\"batch:50\"]",
     3600,
     "Government Office for Science — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_great_british_energy_nuclear, "uk-b50-govuk-great-british-energy-nuclear",
     "Great British Energy – Nuclear", "Great British Energy – Nuclear",
     "science", "science",
     "https://www.gov.uk/government/organisations/great-british-energy-nuclear.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Great British Energy – Nuclear\",\"science\",\"batch:50\"]",
     3600,
     "Great British Energy – Nuclear — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_great_british_nuclear, "uk-b50-govuk-great-british-nuclear",
     "Great British Nuclear", "Great British Nuclear",
     "science", "science",
     "https://www.gov.uk/government/organisations/great-british-nuclear.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Great British Nuclear\",\"science\",\"batch:50\"]",
     604800,
     "Great British Nuclear — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_life_sciences_organisation, "uk-b50-govuk-life-sciences-organisation",
     "Life Sciences Organisation", "Life Sciences Organisation",
     "science", "science",
     "https://www.gov.uk/government/organisations/life-sciences-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Life Sciences Organisation\",\"science\",\"batch:50\"]",
     3600,
     "Life Sciences Organisation — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_met_office, "uk-b50-govuk-met-office",
     "UK Met Office", "UK Met Office",
     "science", "science",
     "https://www.gov.uk/government/organisations/met-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Met Office\",\"science\",\"batch:50\"]",
     21600,
     "UK Met Office — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_endowment_for_science_technology_and_the_arts, "uk-b50-govuk-national-endowment-for-science-technology-and-the-arts",
     "National Endowment for Science, Technology and the Arts", "National Endowment for Science, Technology and the Arts",
     "science", "science",
     "https://www.gov.uk/government/organisations/national-endowment-for-science-technology-and-the-arts.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Endowment for Science, Technology and the Arts\",\"science\",\"batch:50\"]",
     604800,
     "National Endowment for Science, Technology and the Arts — science feed (9 entries at probe)")

RSSX(uk_b50_govuk_national_security_technology_and_innovation_exchange, "uk-b50-govuk-national-security-technology-and-innovation-exchange",
     "National Security Technology and Innovation Exchange", "National Security Technology and Innovation Exchange",
     "science", "science",
     "https://www.gov.uk/government/organisations/national-security-technology-and-innovation-exchange.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Security Technology and Innovation Exchange\",\"science\",\"batch:50\"]",
     604800,
     "National Security Technology and Innovation Exchange — science feed (4 entries at probe)")

RSSX(uk_b50_govuk_national_space_operations_centre, "uk-b50-govuk-national-space-operations-centre",
     "National Space Operations Centre", "National Space Operations Centre",
     "science", "science",
     "https://www.gov.uk/government/organisations/national-space-operations-centre.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Space Operations Centre\",\"science\",\"batch:50\"]",
     3600,
     "National Space Operations Centre — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_nuclear_liabilities_financing_assurance_board, "uk-b50-govuk-nuclear-liabilities-financing-assurance-board",
     "Nuclear Liabilities Financing Assurance Board", "Nuclear Liabilities Financing Assurance Board",
     "science", "science",
     "https://www.gov.uk/government/organisations/nuclear-liabilities-financing-assurance-board.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nuclear Liabilities Financing Assurance Board\",\"science\",\"batch:50\"]",
     3600,
     "Nuclear Liabilities Financing Assurance Board — science feed (6 entries at probe)")

RSSX(uk_b50_govuk_nuclear_research_advisory_council, "uk-b50-govuk-nuclear-research-advisory-council",
     "Nuclear Research Advisory Council", "Nuclear Research Advisory Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/nuclear-research-advisory-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nuclear Research Advisory Council\",\"science\",\"batch:50\"]",
     604800,
     "Nuclear Research Advisory Council — science feed (1 entries at probe)")

RSSX(uk_b50_govuk_nuclear_restoration_services, "uk-b50-govuk-nuclear-restoration-services",
     "Nuclear Restoration Services", "Nuclear Restoration Services",
     "science", "science",
     "https://www.gov.uk/government/organisations/nuclear-restoration-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nuclear Restoration Services\",\"science\",\"batch:50\"]",
     3600,
     "Nuclear Restoration Services — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_nuclear_waste_services, "uk-b50-govuk-nuclear-waste-services",
     "Nuclear Waste Services", "Nuclear Waste Services",
     "science", "science",
     "https://www.gov.uk/government/organisations/nuclear-waste-services.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nuclear Waste Services\",\"science\",\"batch:50\"]",
     3600,
     "Nuclear Waste Services — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_life_sciences, "uk-b50-govuk-office-for-life-sciences",
     "Office for Life Sciences", "Office for Life Sciences",
     "science", "science",
     "https://www.gov.uk/government/organisations/office-for-life-sciences.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office for Life Sciences\",\"science\",\"batch:50\"]",
     3600,
     "Office for Life Sciences — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_open_innovation_team, "uk-b50-govuk-open-innovation-team",
     "Open Innovation Team", "Open Innovation Team",
     "science", "science",
     "https://www.gov.uk/government/organisations/open-innovation-team.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Open Innovation Team\",\"science\",\"batch:50\"]",
     3600,
     "Open Innovation Team — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_particle_physics_and_astronomy_research_council, "uk-b50-govuk-particle-physics-and-astronomy-research-council",
     "Particle Physics and Astronomy Research Council", "Particle Physics and Astronomy Research Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/particle-physics-and-astronomy-research-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Particle Physics and Astronomy Research Council\",\"science\",\"batch:50\"]",
     604800,
     "Particle Physics and Astronomy Research Council — science feed (1 entries at probe)")

RSSX(uk_b50_govuk_research_collaboration_advice_team, "uk-b50-govuk-research-collaboration-advice-team",
     "Research Collaboration Advice Team", "Research Collaboration Advice Team",
     "science", "science",
     "https://www.gov.uk/government/organisations/research-collaboration-advice-team.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Research Collaboration Advice Team\",\"science\",\"batch:50\"]",
     3600,
     "Research Collaboration Advice Team — science feed (5 entries at probe)")

RSSX(uk_b50_govuk_science_advisory_council, "uk-b50-govuk-science-advisory-council",
     "Defra's Science Advisory Council", "Defra's Science Advisory Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/science-advisory-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Defra's Science Advisory Council\",\"science\",\"batch:50\"]",
     3600,
     "Defra's Science Advisory Council — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_science_and_technology_ethics_advisory_committee, "uk-b50-govuk-science-and-technology-ethics-advisory-committee",
     "Science and Technology Ethics Advisory Committee", "Science and Technology Ethics Advisory Committee",
     "science", "science",
     "https://www.gov.uk/government/organisations/science-and-technology-ethics-advisory-committee.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Science and Technology Ethics Advisory Committee\",\"science\",\"batch:50\"]",
     3600,
     "Science and Technology Ethics Advisory Committee — science feed (15 entries at probe)")

RSSX(uk_b50_govuk_science_and_technology_facilities_council, "uk-b50-govuk-science-and-technology-facilities-council",
     "UK STFC", "UK STFC",
     "science", "science",
     "https://www.gov.uk/government/organisations/science-and-technology-facilities-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK STFC\",\"science\",\"batch:50\"]",
     21600,
     "UK STFC — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_science_council, "uk-b50-govuk-science-council",
     "Science Council", "Science Council",
     "science", "science",
     "https://www.gov.uk/government/organisations/science-council.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Science Council\",\"science\",\"batch:50\"]",
     3600,
     "Science Council — science feed (13 entries at probe)")

RSSX(uk_b50_govuk_science_museum_group, "uk-b50-govuk-science-museum-group",
     "Science Museum Group", "Science Museum Group",
     "science", "science",
     "https://www.gov.uk/government/organisations/science-museum-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Science Museum Group\",\"science\",\"batch:50\"]",
     21600,
     "Science Museum Group — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_atomic_energy_authority, "uk-b50-govuk-uk-atomic-energy-authority",
     "UK Atomic Energy Authority", "UK Atomic Energy Authority",
     "science", "science",
     "https://www.gov.uk/government/organisations/uk-atomic-energy-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Atomic Energy Authority\",\"science\",\"batch:50\"]",
     3600,
     "UK Atomic Energy Authority — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_research_and_innovation, "uk-b50-govuk-uk-research-and-innovation",
     "UK Research Innovation", "UK Research Innovation",
     "science", "science",
     "https://www.gov.uk/government/organisations/uk-research-and-innovation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Research Innovation\",\"science\",\"batch:50\"]",
     21600,
     "UK Research Innovation — science feed (20 entries at probe)")

RSSX(uk_b50_govuk_ukti_life_sciences_organisation, "uk-b50-govuk-ukti-life-sciences-organisation",
     "UKTI Life Sciences Organisation", "UKTI Life Sciences Organisation",
     "science", "science",
     "https://www.gov.uk/government/organisations/ukti-life-sciences-organisation.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UKTI Life Sciences Organisation\",\"science\",\"batch:50\"]",
     604800,
     "UKTI Life Sciences Organisation — science feed (3 entries at probe)")

RSSX(uk_b50_govuk_united_kingdom_national_nuclear_laboratory, "uk-b50-govuk-united-kingdom-national-nuclear-laboratory",
     "United Kingdom National Nuclear Laboratory", "United Kingdom National Nuclear Laboratory",
     "science", "science",
     "https://www.gov.uk/government/organisations/united-kingdom-national-nuclear-laboratory.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:United Kingdom National Nuclear Laboratory\",\"science\",\"batch:50\"]",
     21600,
     "United Kingdom National Nuclear Laboratory — science feed (3 entries at probe)")

RSSX(uk_b50_mrclmb_ac_uk, "uk-b50-mrclmb-ac-uk",
     "Medical Research Council Centre Cambridge", "Medical Research Council Centre Cambridge",
     "science", "science",
     "https://mrclmb.ac.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Medical Research Council Centre Cambridge\",\"science\",\"batch:50\"]",
     10800,
     "Medical Research Council Centre Cambridge — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(uk_b50_ncc_eastlancslearning_ac_uk, "uk-b50-ncc-eastlancslearning-ac-uk",
     "Nelson and Colne College", "Nelson and Colne College",
     "science", "science",
     "https://ncc.eastlancslearning.ac.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Nelson and Colne College\",\"science\",\"batch:50\"]",
     10800,
     "Nelson and Colne College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(uk_b50_plymouth_ac_uk_events, "uk-b50-plymouth-ac-uk-events",
     "University of Plymouth", "University of Plymouth",
     "science", "science",
     "https://www.plymouth.ac.uk/feeds/events",
     "en", "[\"gov\",\"country:uk\",\"publisher:University of Plymouth\",\"science\",\"batch:50\"]",
     10800,
     "University of Plymouth — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(uk_b50_plymouth_ac_uk_news, "uk-b50-plymouth-ac-uk-news",
     "University of Plymouth", "University of Plymouth",
     "science", "science",
     "https://www.plymouth.ac.uk/feeds/news",
     "en", "[\"gov\",\"country:uk\",\"publisher:University of Plymouth\",\"science\",\"batch:50\"]",
     10800,
     "University of Plymouth — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 25 entries at probe")

RSSX(uk_b50_puno_ac_uk, "uk-b50-puno-ac-uk",
     "Polish University Abroad — PUNO » Kanał z wpisami", "Polish University Abroad — PUNO » Kanał z wpisami",
     "science", "science",
     "https://puno.ac.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Polish University Abroad — PUNO » Kanał z wpisami\",\"science\",\"batch:50\"]",
     10800,
     "Polish University Abroad — PUNO » Kanał z wpisami — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(uk_b50_sphr_nihr_ac_uk, "uk-b50-sphr-nihr-ac-uk",
     "NIHR School for Public Health Research — NIHR School for Public Health Research Feed", "NIHR School for Public Health Research — NIHR School for Public Health Research Feed",
     "science", "science",
     "https://sphr.nihr.ac.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:NIHR School for Public Health Research — NIHR School for Public Health Research Feed\",\"science\",\"batch:50\"]",
     10800,
     "NIHR School for Public Health Research — NIHR School for Public Health Research Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(uk_b50_usat_edu, "uk-b50-usat-edu",
     "University of Science, Arts and Technology", "University of Science, Arts and Technology",
     "science", "science",
     "https://usat.edu/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:University of Science, Arts and Technology\",\"science\",\"batch:50\"]",
     10800,
     "University of Science, Arts and Technology — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 22 entries at probe")

/* ── uk_opendata ─────────────────────────────────────────────────────── */

RSSX(uk_b50_govuk_central_digital_and_data_office, "uk-b50-govuk-central-digital-and-data-office",
     "Central Digital and Data Office", "Central Digital and Data Office",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/central-digital-and-data-office.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Central Digital and Data Office\",\"opendata\",\"batch:50\"]",
     604800,
     "Central Digital and Data Office — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_digital_data_and_technology_profession, "uk-b50-govuk-digital-data-and-technology-profession",
     "Digital, Data and Technology Profession", "Digital, Data and Technology Profession",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/digital-data-and-technology-profession.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Digital, Data and Technology Profession\",\"opendata\",\"batch:50\"]",
     604800,
     "Digital, Data and Technology Profession — opendata feed (13 entries at probe)")

RSSX(uk_b50_govuk_government_data_quality_hub, "uk-b50-govuk-government-data-quality-hub",
     "Government Data Quality Hub", "Government Data Quality Hub",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/government-data-quality-hub.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Data Quality Hub\",\"opendata\",\"batch:50\"]",
     3600,
     "Government Data Quality Hub — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_higher_education_statistical_agency, "uk-b50-govuk-higher-education-statistical-agency",
     "Higher Education Statistics Agency", "Higher Education Statistics Agency",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/higher-education-statistical-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Higher Education Statistics Agency\",\"opendata\",\"batch:50\"]",
     21600,
     "Higher Education Statistics Agency — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_data_guardian, "uk-b50-govuk-national-data-guardian",
     "National Data Guardian", "National Data Guardian",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/national-data-guardian.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National Data Guardian\",\"opendata\",\"batch:50\"]",
     3600,
     "National Data Guardian — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_national_dna_database_ethics_group, "uk-b50-govuk-national-dna-database-ethics-group",
     "National DNA Database Ethics Group", "National DNA Database Ethics Group",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/national-dna-database-ethics-group.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:National DNA Database Ethics Group\",\"opendata\",\"batch:50\"]",
     604800,
     "National DNA Database Ethics Group — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_northern_ireland_statistics_and_research_agency, "uk-b50-govuk-northern-ireland-statistics-and-research-agency",
     "Northern Ireland Statistics and Research Agency", "Northern Ireland Statistics and Research Agency",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/northern-ireland-statistics-and-research-agency.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Northern Ireland Statistics and Research Agency\",\"opendata\",\"batch:50\"]",
     21600,
     "Northern Ireland Statistics and Research Agency — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_for_budget_responsibility, "uk-b50-govuk-office-for-budget-responsibility",
     "UK OBR", "UK OBR",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/office-for-budget-responsibility.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK OBR\",\"opendata\",\"batch:50\"]",
     21600,
     "UK OBR — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_office_of_the_data_protection_registrar, "uk-b50-govuk-office-of-the-data-protection-registrar",
     "Office of the Data Protection Registrar", "Office of the Data Protection Registrar",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/office-of-the-data-protection-registrar.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Office of the Data Protection Registrar\",\"opendata\",\"batch:50\"]",
     604800,
     "Office of the Data Protection Registrar — opendata feed (2 entries at probe)")

RSSX(uk_b50_govuk_ordnance_survey, "uk-b50-govuk-ordnance-survey",
     "UK Ordnance Survey", "UK Ordnance Survey",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/ordnance-survey.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Ordnance Survey\",\"opendata\",\"batch:50\"]",
     21600,
     "UK Ordnance Survey — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_ordnance_survey_of_northern_ireland, "uk-b50-govuk-ordnance-survey-of-northern-ireland",
     "Ordnance Survey of Northern Ireland", "Ordnance Survey of Northern Ireland",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/ordnance-survey-of-northern-ireland.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Ordnance Survey of Northern Ireland\",\"opendata\",\"batch:50\"]",
     604800,
     "Ordnance Survey of Northern Ireland — opendata feed (1 entries at probe)")

RSSX(uk_b50_govuk_statistics_commission, "uk-b50-govuk-statistics-commission",
     "Statistics Commission", "Statistics Commission",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/statistics-commission.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:Statistics Commission\",\"opendata\",\"batch:50\"]",
     604800,
     "Statistics Commission — opendata feed (2 entries at probe)")

RSSX(uk_b50_govuk_the_national_archives, "uk-b50-govuk-the-national-archives",
     "UK National Archives", "UK National Archives",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/the-national-archives.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK National Archives\",\"opendata\",\"batch:50\"]",
     21600,
     "UK National Archives — opendata feed (20 entries at probe)")

RSSX(uk_b50_govuk_uk_statistics_authority, "uk-b50-govuk-uk-statistics-authority",
     "UK Statistics Authority", "UK Statistics Authority",
     "uk_opendata", "opendata",
     "https://www.gov.uk/government/organisations/uk-statistics-authority.atom",
     "en", "[\"gov\",\"country:uk\",\"publisher:UK Statistics Authority\",\"opendata\",\"batch:50\"]",
     21600,
     "UK Statistics Authority — opendata feed (20 entries at probe)")

/* ── browser-profile hosts (restored 2026-10-21) ─────────────────────
 * Rejected at authoring for refusing every self-identifying agent; fetched
 * now with the per-host browser profile in core/httpclient.c BROWSER_HOSTS. */

RSSX(uk_b50_isc_independent_gov_uk, "uk-b50-isc-independent-gov-uk",
     "Intelligence and Security Committee of Parliament", "Intelligence and Security Committee of Parliament",
     "intelligence", "intelligence",
     "https://isc.independent.gov.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Intelligence and Security Committee of Parliament\",\"intelligence\",\"batch:50\"]",
     3600,
     "Intelligence and Security Committee of Parliament — official feed; the host admits browser-profile requests only (core/httpclient.c BROWSER_HOSTS)")

RSSX(uk_b50_security_gov_uk, "uk-b50-security-gov-uk",
     "Government Cyber Coordination Centre (security.gov.uk)", "Government Cyber Coordination Centre (security.gov.uk)",
     "cyber_advisory", "cyber",
     "https://www.security.gov.uk/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:Government Cyber Coordination Centre (security.gov.uk)\",\"cyber\",\"batch:50\"]",
     3600,
     "Government Cyber Coordination Centre (security.gov.uk) — official feed; the host admits browser-profile requests only (core/httpclient.c BROWSER_HOSTS)")

RSSX(uk_b50_nhs24_scot, "uk-b50-nhs24-scot",
     "NHS 24", "NHS 24",
     "health", "health",
     "https://www.nhs24.scot/feed/",
     "en", "[\"gov\",\"country:uk\",\"publisher:NHS 24\",\"health\",\"batch:50\"]",
     3600,
     "NHS 24 — official feed; the host admits browser-profile requests only (core/httpclient.c BROWSER_HOSTS)")
