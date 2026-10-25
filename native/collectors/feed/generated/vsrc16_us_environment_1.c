/* Verified-live us_environment sources (14), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(us_epa_echo_detailed_facility_report, "us-epa-echo-detailed-facility-report", "EPA ECHO — Detailed Facility Report (DFR)", "EPA ECHO — Detailed Facility Report (DFR)",
  "us_environment", "environment",
  "https://echodata.epa.gov/echo/dfr_rest_services.get_dfr?output=JSON&p_id=110009441979",
  "results.permits",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "THE deep hop: one FRS RegistryID expands into the full compliance dossier — MultipleFRSFacilities, Reports flags (HasPollRpt/HasAimRpt/HasBiosolidAnnualRpt/HasBflmRpt), and a Permits array carrying, per EPA system (FRS/ICIS-AIR/NPDES/RCRAInfo/SDWIS/TRI), the Statute, EPASystem, EPARegion, SourceID, facility name/address/FIPS/county, Universe, permit ExpDate, lat/lon, IndianCountry flag, SIC/NAICS and geocoding provenance. Downstream sections carry inspections, violations, formal/informal enforcement actions and penalties for that facility.");

VCSV(us_epa_echo_download_csv, "us-epa-echo-download-csv", "EPA ECHO — bulk CSV export of a query", "EPA ECHO — bulk CSV export of a query",
  "us_environment", "environment",
  "https://echodata.epa.gov/echo/echo_rest_services.get_download?output=CSV&qid=263&qcolumns=1,2,3",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Dumps an entire ECHO query result set as CSV instead of paging get_qid. Columns are selected by index via qcolumns (1,2,3 = FacName, FacStreet, FacCity; RegistryID always included). 30,577 data rows in the observed response. Same qid-recycling caveat: take qid from the paired search call's QueryID.");

VJSON(us_epa_envirofacts_frs_facility_site, "us-epa-envirofacts-frs-facility-site", "EPA Envirofacts — FRS facility site master", "EPA Envirofacts — FRS facility site master",
  "us_environment", "environment",
  "https://data.epa.gov/efservice/frs_facility_site/state_code/RI/rows/0:1/JSON",
  "",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "The Facility Registry Service master record — the canonical EPA identity for a regulated site. state_name, location_address, std_full_address and standardised address components (std_base_name, std_stype_before/after, std_city_name), country_name, tribal_land_name, federal_agency_code, parent linkage, user_id/create_date/update_date provenance. Registry ID here is the key used by ECHO's DFR.");

VJSON(us_epa_envirofacts_frs_program_facility, "us-epa-envirofacts-frs-program-facility", "EPA Envirofacts — FRS program-system facility crosswalk", "EPA Envirofacts — FRS program-system facility crosswalk",
  "us_environment", "environment",
  "https://data.epa.gov/efservice/frs_program_facility/pgm_sys_acrnm/RCRAINFO/state_code/RI/rows/0:2/JSON",
  "",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Maps a facility to every EPA programme system it appears in — filter by pgm_sys_acrnm (RCRAINFO, ICIS-AIR, NPDES, TRIS, SDWIS, AIRS/AFS, CERCLIS, EIS, GHG) and by state_code. Carries location_address, std_full_address, std_city_name, country_name, tribal_land_name, federal_agency_code, standardised street components, create/update/refresh dates. This is the join table that lets one company name reach all its permits.");

VJSON(us_epa_envirofacts_ghg_facility, "us-epa-envirofacts-ghg-facility", "EPA GHGRP — greenhouse gas reporting facility dimension", "EPA GHGRP — greenhouse gas reporting facility dimension",
  "us_environment", "environment",
  "https://data.epa.gov/efservice/pub_dim_facility/state/TX/rows/0:1/JSON",
  "",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Mandatory greenhouse-gas reporters: facility_id, facility_name, latitude/longitude, address1/address2, city, state, state_name, zip, county and county_fips, naics_code, reporting year, bamm_used_desc (best-available-monitoring-method), emission classification. facility_id joins to sector emissions.");

VJSON(us_epa_envirofacts_icis_facility_interest, "us-epa-envirofacts-icis-facility-interest", "EPA ICIS — facility interest records", "EPA ICIS — facility interest records",
  "us_environment", "environment",
  "https://data.epa.gov/efservice/icis_facility_interest/rows/0:0/JSON",
  "",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Facility-side of ICIS-NPDES: location_address, city, state, zip, geocoding coordinates and format code, facility UIN, federal/state flag. Joins ICIS permits to physical sites and to FRS.");

VJSON(us_epa_envirofacts_tri_chem_info, "us-epa-envirofacts-tri-chem-info", "EPA Envirofacts — TRI chemical dictionary", "EPA Envirofacts — TRI chemical dictionary",
  "us_environment", "environment",
  "https://data.epa.gov/efservice/tri_chem_info/rows/0:1/JSON",
  "",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Resolves tri_chem_id to a substance: chem_name, active_date/inactive_date, caac_ind (Clean Air Act chemical), carc_ind (carcinogen), r3350_ind, metal_ind, feds_ind, classification, pbt_start_year (persistent bioaccumulative toxic).");

VJSON(us_epa_envirofacts_tri_reporting_form, "us-epa-envirofacts-tri-reporting-form", "EPA Envirofacts — TRI Form R submissions", "EPA Envirofacts — TRI Form R submissions",
  "us_environment", "environment",
  "https://data.epa.gov/efservice/tri_reporting_form/rows/0:1/JSON",
  "",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "One row per chemical per facility per reporting year. doc_ctrl_num (the join key to release/transfer quantities), tri_facility_id, tri_chem_id, form_type_ind, reporting_year, certif_name and certif_official_title (the NAMED EXECUTIVE who signed the toxics report), certif_signature, certif_date_signed, postmark_date, partial_fac/entire_fac, max_amount_of_chem, active_status.");

VJSON(us_ny_dec_remediation_spills, "us-ny-dec-remediation-spills", "New York State DEC - remediation sites and spills", "New York State DEC - remediation sites and spills",
  "us_environment", "environment",
  /* THIS ROW WAS STORING THE LAYER'S SCHEMA AS SPILL RECORDS. Two defects,
   * measured 2026-10-25, and the second one made the first invisible:
   *
   *   `array_path` was "fields" — the `fields` array of an ArcGIS query reply
   *   is its COLUMN DEFINITIONS, so every run emitted ~20 field descriptors
   *   titled and keyed as remediation sites. That is the "once counted a
   *   response's own schema block as records" trap in CLAUDE.md, in a row that
   *   shipped. It is "features" now.
   *
   *   `resultRecordCount=1` — one record every three hours, of 6,072 the layer
   *   holds (its own returnCountOnly). Raising that alone would have changed
   *   NOTHING while array_path still named `fields`, which is why the record
   *   count looked plausible for as long as it did.
   *
   * `f=geojson` rather than `f=json` so each feature carries a top-level `id`
   * (the OID): VJSON keys on a fixed top-level precedence list, and an
   * `f=json` feature is {attributes, geometry} with no scalar at the top, so
   * every record would have fallen back to a hash of title/link/date. The
   * size is the layer's own declared maxRecordCount — above it ArcGIS clamps
   * silently, so pages would come back short and a walk reads a short page as
   * the end of the data. `orderByFields=OBJECTID` because the walk advances
   * `resultOffset` and an offset walk with no stable sort skips records; this
   * layer publishes no `objectIdField` key at all, so OBJECTID is the field it
   * declares with type esriFieldTypeOID — read, not guessed. 6,072 records is
   * 4 pages, well inside the 20-page ceiling.
   *
   * The uids this row has written are field names and are now orphaned;
   * tools/cleanup_stale_uids.py is the tool for that, and it has NOT been run
   * here because the live database is mid-integrity-check. */
  "https://gisservices.dec.ny.gov/arcgis/rest/services/der/RemediationSpills/MapServer/0/query?where=1%3D1&outFields=*&orderByFields=OBJECTID&resultRecordCount=2000&f=geojson",
  "features",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "NY Division of Environmental Remediation site register: SITECODE, SITENAME, PROGRAM, SITECLASS, ADDRESS1/2, LOCALITY, ZIPCODE, COUNTY, TOWN, REGION, UTMX/UTMY, METHOD, and per-record DETAIL_URL, DOC_URL, BROWN_URL, CLASS_URL fields that link straight to the DEC case file and documents. Rate-sensitive: it reset the connection on repeated rapid hits.");

VJSON(us_sdwis_water_system_facility, "us-sdwis-water-system-facility", "EPA SDWIS — physical facilities within a water system", "EPA SDWIS — physical facilities within a water system",
  "us_environment", "environment",
  "https://data.epa.gov/efservice/water_system_facility/pwsid/010106001/rows/0:1/JSON",
  "",
  "en", "[\"us\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Wells, intakes, treatment plants and storage inside a PWS: pwsid, facility_id, facility_name, state_facility_id, facility_activity_code, facility_deactivation_date, facility_type_code, submission_status_code, is_source_ind, water_type_code, availability_code, seller/treatment linkage. Lets a violation be pinned to a specific well or plant.");
