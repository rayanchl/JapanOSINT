/* openFDA identifier pivots — resolve a regulatory number to its full record.
 *
 * WHERE THESE CAME FROM. Batch 16 discovered all nine of these endpoints and
 * described them, correctly, as detail hops: "DETAIL HOP on a device clearance",
 * "Resolves an NDC seen on a recall or adverse event back to its labeler". They
 * were then generated as SCHEDULED VJSON collectors with the probe's own example
 * id frozen into the URL and `limit=1` left where the probe had put it:
 *
 *     https://api.fda.gov/device/510k.json?search=k_number:%22K092877%22&limit=1
 *
 * So every twelve hours, forever, that row re-fetched one 2009 device clearance.
 * `make audit-sources` flags this as limit-one; the deeper problem is that a row
 * authored as a lookup shipped as a feed, which makes it useless in both roles —
 * it answers no question about any OTHER k-number, and it is not news.
 *
 * These are the same endpoints as pivots, which is what their own descriptions
 * always said they were: the example id becomes {q}, so the row answers for any
 * identifier, and limit=1 becomes limit=100 so an id that legitimately matches
 * several records (an application number with many products, a recall number
 * spanning several product lines) returns them all instead of the first.
 *
 * The ids are unchanged, so nothing vanishes from the registry.
 *
 * NO PAGING DECLARED, deliberately. A keyed lookup returns one short page, and
 * lib/pager.c only advances a cursor when the page came back exactly full — so
 * these rows make one request and stop, rather than spending a second request
 * on a `skip=100` that openFDA would answer "no matches found". The firm-name
 * searches in hp2_northam_us_reg.c do declare skip paging, because a firm really
 * can have more than 100 recalls; an identifier cannot.
 *
 * limit=100 and the skip/limit contract are not guesses: hp2_northam_us_reg.c
 * has shipped four openFDA rows on exactly those parameters. */
#include "lib/hpengine.h"

static const hp_source HP18_US_OPENFDA[] = {
  { .id = "us-openfda-device-510k-detail",
    .name = "openFDA — 510(k) clearance detail by K-number",
    .name_ja = "openFDA — 510(k)クリアランス詳細（K番号）",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-device-510k",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/device/510k.json?search=k_number:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "device_name,applicant",
    .id_keys = "k_number", .date_keys = "decision_date",
    .description = "DETAIL HOP on a device clearance: k_number, applicant, "
      "contact, address_1/2, city/state/postal_code/country_code, date_received, "
      "decision_date, decision_code/description, clearance_type, product_code, "
      "advisory_committee and description, review_advisory_committee, "
      "statement_or_summary, third_party_flag, expedited_review_flag, "
      "device_name, plus the openfda block (device_class, regulation_number, "
      "medical_specialty_description). Links a marketed device back to the firm "
      "that cleared it." },

  { .id = "us-openfda-device-classification-detail",
    .name = "openFDA — device product-code classification detail",
    .name_ja = "openFDA — 医療機器プロダクトコード分類詳細",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-device-classification",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/device/classification.json?search=product_code:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "device_name",
    .id_keys = "product_code",
    .description = "DETAIL HOP that resolves the product_code appearing on every "
      "MAUDE report, recall and 510(k): device_name, "
      "medical_specialty_description, device_class, regulation_number, "
      "definition, physical_state, technical_method, target_area, "
      "gmp_exempt_flag, implant_flag, life_sustain_support_flag, "
      "third_party_flag, review_panel, submission_type_id, "
      "unclassified_reason." },

  { .id = "us-openfda-device-enforcement-by-recall-number",
    .name = "openFDA — device enforcement report detail by recall number",
    .name_ja = "openFDA — 医療機器行政措置報告詳細（リコール番号）",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-device-enforcement",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/device/enforcement.json?search=recall_number:%22{q}%22&limit=100",
    .array_path = "results",
    .title_keys = "product_description,reason_for_recall",
    .id_keys = "recall_number", .date_keys = "recall_initiation_date",
    .description = "DETAIL HOP on the formal FDA enforcement report: "
      "recall_number, classification (Class I/II/III), status, recalling_firm "
      "and full address, event_id, product_type, product_description, "
      "product_quantity, code_info, reason_for_recall, recall_initiation_date, "
      "center_classification_date, termination_date, report_date, "
      "distribution_pattern, initial_firm_notification, voluntary_mandated, and "
      "openfda linkage." },

  { .id = "us-openfda-device-pma-detail",
    .name = "openFDA — PMA premarket approval detail by PMA number",
    .name_ja = "openFDA — PMA市販前承認詳細（PMA番号）",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-device-pma",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/device/pma.json?search=pma_number:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "trade_name,generic_name",
    .id_keys = "pma_number", .date_keys = "decision_date",
    .description = "DETAIL HOP: pma_number, supplement_number and "
      "supplement_type/reason, applicant, street/city/state/zip, generic_name, "
      "trade_name, product_code, advisory_committee, date_received, "
      "decision_date, decision_code, expedited_review_flag, ao_statement (the "
      "approval-order text), docket_number, and openfda "
      "device_class/regulation_number. The highest-scrutiny device approval "
      "record." },

  { .id = "us-openfda-device-udi-detail",
    .name = "openFDA — Unique Device Identifier record detail",
    .name_ja = "openFDA — UDI（機器固有識別子）レコード詳細",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-device-udi",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/device/udi.json?search=public_device_record_key:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "brand_name,device_description",
    .id_keys = "public_device_record_key",
    .description = "DETAIL HOP into the GUDID: public_device_record_key, "
      "public_version_number/date/status, device_description, brand_name, "
      "version_or_model_number, catalog_number, company_name, labeler DUNS, "
      "identifiers[] (GTIN/DI values and issuing agency), device_sizes, "
      "sterilization info, mri_safety, is_rx/is_otc/is_kit/"
      "is_combination_product, has_lot_or_batch_number/serial_number/"
      "expiration_date, product_codes[], customer_contacts[], "
      "device_publish/commercial_distribution dates." },

  { .id = "us-openfda-drug-enforcement-by-recall-number",
    .name = "openFDA — drug enforcement report detail by recall number",
    .name_ja = "openFDA — 医薬品行政措置報告詳細（リコール番号）",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-drug-enforcement",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/drug/enforcement.json?search=recall_number:%22{q}%22&limit=100",
    .array_path = "results",
    .title_keys = "product_description,reason_for_recall",
    .id_keys = "recall_number", .date_keys = "recall_initiation_date",
    .description = "DETAIL HOP: full FDA drug recall enforcement record — "
      "recall_number, classification, status, recalling_firm, "
      "city/state/country/postal_code, event_id, product_description, "
      "product_quantity, code_info, reason_for_recall, recall_initiation_date, "
      "center_classification_date, termination_date, report_date, "
      "distribution_pattern, initial_firm_notification, voluntary_mandated, plus "
      "openfda (application_number, brand_name, generic_name, manufacturer_name, "
      "product_ndc, substance_name, unii, spl_id)." },

  { .id = "us-openfda-drug-ndc-by-product-ndc",
    .name = "openFDA — National Drug Code directory detail",
    .name_ja = "openFDA — 全米医薬品コード（NDC）詳細",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-drug-ndc",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/drug/ndc.json?search=product_ndc:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "brand_name,generic_name",
    .id_keys = "product_ndc", .date_keys = "marketing_start_date",
    .description = "DETAIL HOP: product_ndc, generic_name, labeler_name, "
      "brand_name and suffix, active_ingredients[], finished flag, packaging[] "
      "(package_ndc, description, marketing_start_date, sample flag), "
      "listing_expiration_date, marketing_category, dosage_form, route[], "
      "product_type, marketing_start/end_date, application_number, "
      "pharm_class[], dea_schedule. Resolves an NDC seen on a recall or adverse "
      "event back to its labeler." },

  { .id = "us-openfda-drugsfda-by-application",
    .name = "openFDA — Drugs@FDA application detail by NDA/ANDA/BLA number",
    .name_ja = "openFDA — Drugs@FDA申請詳細（NDA/ANDA/BLA番号）",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-drug-application",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/drug/drugsfda.json?search=application_number:%22{q}%22&limit=100",
    .array_path = "results", .title_keys = "sponsor_name,application_number",
    .id_keys = "application_number",
    .description = "DETAIL HOP: application_number, sponsor_name, products[] "
      "(product_number, reference_drug, brand_name, active_ingredients[] with "
      "strength, dosage_form, route, marketing_status, te_code), submissions[] — "
      "every supplement with submission_type, submission_number, "
      "submission_status and date, review_priority, "
      "submission_class_code_description, and application_docs[] with URLs to "
      "the approval letters, labels and review packages." },

  { .id = "us-openfda-food-enforcement-by-recall-number",
    .name = "openFDA — food recall enforcement detail by recall number",
    .name_ja = "openFDA — 食品リコール行政措置詳細（リコール番号）",
    .collector = "us_health", .category = "health",
    .portal = "https://open.fda.gov", .record_type = "us-food-enforcement",
    .tags = "\"us\",\"health\",\"batch16\"", .free_tier = 1,
    .url = "https://api.fda.gov/food/enforcement.json?search=recall_number:%22{q}%22&limit=100",
    .array_path = "results",
    .title_keys = "product_description,reason_for_recall",
    .id_keys = "recall_number", .date_keys = "recall_initiation_date",
    .description = "DETAIL HOP: recall_number, classification, status, "
      "recalling_firm with full address, event_id, product_type, "
      "product_description, product_quantity, code_info, reason_for_recall, "
      "recall_initiation_date, center_classification_date, termination_date, "
      "report_date, distribution_pattern, initial_firm_notification, "
      "voluntary_mandated, more_code_info." },
};
HP_REGISTER_TABLE(HP18_US_OPENFDA)
