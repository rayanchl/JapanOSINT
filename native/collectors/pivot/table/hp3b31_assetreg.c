/* collectors/pivot/table/hp3b31_assetreg.c — batch 31: assetreg — public registers of things.
 *
 * Companies are registered, but so are the assets they hold, and the asset
 * register is frequently the more honest record: a company can be dissolved and
 * re-formed, but the parcel, the hull, the airframe, the transmitter and the
 * mining tenement persist and carry their ownership history with them.
 *
 * The registers below are the ones that are genuinely public and genuinely
 * queryable — the Dutch national vehicle register as an open dataset, the EU
 * fishing fleet register, the US Coast Guard vessel database, the Australian
 * radiocommunications licence register, the Spanish and Czech cadastres, and
 * the two contract archives that publish the actual signed text of mining,
 * oil and land deals.
 *
 * PROVENANCE AND VERIFICATION STATUS — read before trusting a row here.
 *
 * Hand-authored in C, not scaffolded by tools/gen_hp_batch.py. This file is
 * the maintained copy. Since 2026-10-05 a manifest RECONSTRUCTED from it exists
 * at docs/candidate-sources-batch31.<beat>.txt, so the manifest-driven gates can
 * finally be pointed at batch 31. It is a record, not a source: never
 * regenerate this file from it. Its fidelity was proven rather than assumed —
 * gen_hp_batch.py run on the manifest reproduces every field of every row here
 * exactly, and a grep of field assignments agrees on both sides.
 *
 * These rows are NOT proof-of-life verified. No row was fetched over the wire,
 * so rules 4, 4b and 4d (fetching is not emitting; emitting is not storing;
 * answering is not answering THE QUESTION) are unmeasured. What HAS been
 * checked is offline and structural: clean build, zero audit-sources findings,
 * no duplicate id or endpoint, every row reachable (rule 3), and every paged
 * endpoint declares its walk (audit_batch_pagination, first run 2026-10-05).
 *
 * With network, run the MANIFEST-driven tools:
 *     python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter
 *     python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch31.*.txt \
 *             --bin ./bin/japanosint --jobs 6
 * NOT `audit_registry_emit.py --match hp3b31`, which this header used to
 * suggest: --match is a regex on the source ID, these ids carry no batch
 * prefix, so it selects zero rows and reports nothing wrong.
 * Rows with key_env need their key in the environment, and rows with
 * post_body are POSTs that probe_hp_batch does not send — judge those two
 * classes with audit_batch_emit. Retire whatever comes back EMITS_NOTHING,
 * DROPS_EVERYTHING, COLLISION or FILTER_IGNORED. The engine cannot fabricate:
 * an endpoint that moved yields an honest empty, never an invented record.
 */
#include "lib/hpengine.h"

static const hp_source HP3B31_ASSETREG[] = {
  /* ── Land and cadastre ────────────────────────────────────────────────── */
  { .id = "CZ_CUZK_CADASTRE", .name = "Czech Republic ČÚZK — cadastre of real estate",
    .name_ja = "チェコ 不動産地籍", .category = "government",
    .portal = "https://nahlizenidokn.cuzk.cz", .record_type = "cz-parcel",
    .tags = "\"cz\",\"cadastre\",\"property\",\"ownership\"",
    .mode = HP_JSON,
    /* RE-POINTED 2026-10-22 (open item B2). The scraped ASPX path is gone, and
     * the cadastre now publishes a documented REST API — found by asking the
     * host what it serves rather than guessing a path: its Swagger UI names
     * its own spec at /swagger/v1.0/swagger.json, which gives the base
     * (/api/v1), 41 paths, and the authentication scheme.
     *
     * IT IS A CREDENTIAL GATE, measured rather than assumed. One request each
     * on 2026-10-22, both of them refused identically:
     *     no ApiKey header at all   401, an RFC 7235 problem+json body
     *     ApiKey: <a bogus value>   401, the same body
     * while /api/v1/AplikacniSluzby/Health answers 200 without a key, so the
     * service is up and it is the DATA that is gated. So this row carries
     * key_env and stores a "needs credential" notice naming exactly what is
     * missing until a key exists — rule 1's honest degradation — instead of a
     * 404 that reads like a dead publisher.
     *
     * Its search endpoints take CODES, not free text (KodKatastralnihoUzemi is
     * a cadastral-area code, KmenoveCisloParcely a parcel number), so this is
     * an identifier pivot like the openFDA nine, not a name search. The entity
     * is the cadastral-area code.
     *
     * The API also documents the distinction rule 1 cares about, and it is
     * worth honouring when the key arrives: a MISSING object is 404, while a
     * search matching nothing is 200 with an empty data list and an
     * informational message. */
    .free_tier = 0, .key_env = "JO_CUZK_API_KEY",
    .url = "https://api-kn.cuzk.gov.cz/api/v1/Parcely/Vyhledani"
      "?KodKatastralnihoUzemi={qd}",
    .headers = { "ApiKey: {key}" },
    .array_path = "data",
    .description = "The Czech cadastre publishes the owner of each parcel and "
      "building by name, together with the title deed, encumbrances, easements "
      "and pending proceedings. One of the few European property registers "
      "where ownership is openly readable — through an API that needs a key "
      "registered at registrace.cuzk.gov.cz, which this row says in a notice "
      "until one is set in JO_CUZK_API_KEY" },

  { .id = "EE_LAND_BOARD_CADASTRE", .name = "Estonia Land Board — cadastre & address service",
    .name_ja = "エストニア 土地台帳", .category = "government",
    .portal = "https://xgis.maaamet.ee", .record_type = "ee-parcel",
    .tags = "\"ee\",\"cadastre\",\"property\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://xgis.maaamet.ee/ky/{q}",
    .base = "https://xgis.maaamet.ee", .filter_query = 1,
    .description = "Estonia's cadastral service — parcel identifier, area, "
      "intended use, restrictions and the linked address, in the most fully "
      "digitised land administration in Europe" },

  { .id = "GLOBAL_OPENLANDCONTRACTS", .name = "OpenLandContracts — published land & agriculture deals",
    .name_ja = "土地契約公開データベース", .category = "government",
    .portal = "https://www.openlandcontracts.org", .record_type = "land-contract",
    .tags = "\"land\",\"contracts\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.openlandcontracts.org/search?q={q}",
    .base = "https://www.openlandcontracts.org", .filter_query = 1,
    .description = "The actual signed text of large-scale land, agriculture and "
      "forestry concessions with governments, annotated — the concession area, "
      "the term, the fiscal terms, the community obligations and the "
      "termination provisions" },

  { .id = "GLOBAL_RESOURCE_CONTRACTS", .name = "ResourceContracts — mining, oil & gas contracts",
    .name_ja = "資源契約公開データベース", .category = "government",
    .portal = "https://www.resourcecontracts.org", .record_type = "extractive-contract",
    .tags = "\"extractives\",\"contracts\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.resourcecontracts.org/search?q={q}",
    .base = "https://www.resourcecontracts.org", .filter_query = 1,
    .description = "Published petroleum and mining contracts and licences — the "
      "signatory companies, the licence area, the royalty and production "
      "sharing terms, the stabilisation clauses and the state participation. "
      "The deal itself, not a summary of it" },

  /* ── Vehicles ─────────────────────────────────────────────────────────── */
  { .id = "NL_RDW_VEHICLE_REGISTER", .name = "Netherlands RDW — national vehicle register",
    .name_ja = "オランダ 車両登録簿", .category = "transport",
    .portal = "https://opendata.rdw.nl", .record_type = "nl-vehicle",
    .tags = "\"nl\",\"vehicle\",\"registry\"", .free_tier = 1,
    .url = "https://opendata.rdw.nl/resource/m9d7-ebf2.json?$q={q}&$limit=1000",
    .title_keys = "merk,handelsbenaming", .id_keys = "kenteken",
    .date_keys = "datum_eerste_toelating",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "The complete Dutch vehicle register as open data — plate, "
      "make and model, first registration date, technical specification, APK "
      "expiry, insurance status and whether the vehicle is exported or "
      "scrapped. An entire national vehicle register, queryable without a key" },

  { .id = "NL_RDW_VEHICLE_RECALLS", .name = "Netherlands RDW — vehicle recall & defect records",
    .name_ja = "オランダ 車両リコール記録", .category = "transport",
    .portal = "https://opendata.rdw.nl", .record_type = "nl-vehicle-recall",
    .tags = "\"nl\",\"vehicle\",\"safety\"", .free_tier = 1,
    .url = "https://opendata.rdw.nl/resource/j9yg-7rg9.json?$q={q}&$limit=1000",
    .title_keys = "merk,handelsbenaming", .id_keys = "kenteken",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Recall campaigns and open defect notices linked to "
      "individual Dutch registrations — the campaign, the defect and whether "
      "the vehicle has been remedied" },

  { .id = "UK_DVLA_VEHICLE_ENQUIRY", .name = "UK DVLA — vehicle enquiry service",
    .name_ja = "英国DVLA 車両照会", .category = "transport",
    .portal = "https://driver-vehicle-licensing.api.gov.uk",
    .record_type = "uk-vehicle",
    .tags = "\"uk\",\"vehicle\",\"registry\"", .key_env = "DVLA_API_KEY",
    .free_tier = 1,
    .url = "https://driver-vehicle-licensing.api.gov.uk/vehicle-enquiry/v1/vehicles",
    .post_body = "{\"registrationNumber\":\"{Q}\"}",
    .content_type = "application/json",
    .headers = { "x-api-key: {key}", NULL },
    .title_keys = "make,colour", .id_keys = "registrationNumber",
    .date_keys = "monthOfFirstRegistration",
    .description = "DVLA's official vehicle enquiry — make, colour, fuel type, "
      "engine capacity, CO2, tax and MOT status and the date of first "
      "registration for a UK plate" },

  /* ── Vessels and fisheries ────────────────────────────────────────────── */
  { .id = "EU_FISHING_FLEET_REGISTER", .name = "EU — Community fishing fleet register",
    .name_ja = "EU 漁船登録簿", .category = "maritime",
    .portal = "https://webgate.ec.europa.eu", .record_type = "eu-fishing-vessel",
    .tags = "\"eu\",\"fisheries\",\"vessel\",\"registry\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://webgate.ec.europa.eu/fleet-europa/search_en?vesselName={q}",
    .base = "https://webgate.ec.europa.eu", .filter_query = 1,
    .description = "Every fishing vessel in the EU fleet — CFR number, "
      "external marking, IMO where held, flag state, port, gear, tonnage, "
      "engine power and the licence and entry-exit events. The register that "
      "makes IUU fishing attribution possible" },

  /* ── Aircraft ─────────────────────────────────────────────────────────── */

  { .id = "OFFSHORE_AIRCRAFT_REGISTRIES", .name = "Offshore aircraft registries — Aruba, Bermuda, Cayman, San Marino",
    .name_ja = "オフショア 航空機登録", .category = "transport",
    .portal = "https://www.planelogger.com", .record_type = "aircraft-registration",
    .tags = "\"aviation\",\"offshore\",\"registry\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.planelogger.com/Aircraft/Search?searchterm={q}",
    .base = "https://www.planelogger.com", .filter_query = 1,
    .description = "The P4-, VP-B-, VP-C-, T7- and M- prefixed registries used "
      "to hold private jets outside the beneficial owner's home jurisdiction, "
      "with the registered owner-trustee, the type and the serial number that "
      "survives a re-registration" },

  /* ── Spectrum and transmission sites ──────────────────────────────────── */

  { .id = "CA_ISED_SPECTRUM_LICENCES", .name = "Canada ISED — spectrum licence & tower data",
    .name_ja = "カナダ 無線免許/鉄塔", .category = "infrastructure",
    .portal = "https://sms-sgs.ic.gc.ca", .record_type = "ca-spectrum-licence",
    .tags = "\"ca\",\"spectrum\",\"licence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://sms-sgs.ic.gc.ca/equipmentSearch/searchRadioEquipments?"
      "searchTerm={q}",
    .base = "https://sms-sgs.ic.gc.ca", .filter_query = 1,
    .description = "Canadian spectrum licences and certified radio equipment — "
      "the licensee, the service area, the frequency and the antenna structure "
      "locations registered with Innovation, Science and Economic Development" },

  /* ── Extraction, energy and environmental permits ─────────────────────── */

  { .id = "CLIMATE_TRACE_ASSETS", .name = "Climate TRACE — asset-level emissions & ownership",
    .name_ja = "Climate TRACE 施設別排出/所有", .category = "environment",
    .portal = "https://api.climatetrace.org", .record_type = "emitting-asset",
    .tags = "\"emissions\",\"assets\",\"ownership\"", .free_tier = 1,
    .url = "https://api.climatetrace.org/v6/assets?limit=1000",
    .post_body = "{\"name\":\"{Q}\"}", .content_type = "application/json",
    .array_path = "assets", .filter_query = 1,
    .title_keys = "Name,Sector", .id_keys = "Id",
    .lat_key = "Centroid.Geometry.1", .lon_key = "Centroid.Geometry.0",
    /* One response held at most limit=1000 assets and the row stopped there. The
     * assets endpoint takes offset, so walk it. */
    .page_param = "offset", .page_size = 1000, .page_max = 20,
    .description = "Individual emitting facilities worldwide — power stations, "
      "steel and cement plants, refineries, mines, ports and ships — with "
      "coordinates, sector, estimated emissions derived from satellite "
      "observation, and the corporate owner where attributed" },

  { .id = "GLOBAL_FOREST_CONCESSIONS", .name = "Global Forest Watch — concessions & land use",
    .name_ja = "森林監視 コンセッション", .category = "environment",
    .portal = "https://data.globalforestwatch.org", .record_type = "forest-concession",
    .tags = "\"forest\",\"concession\",\"environment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://data.globalforestwatch.org/search?q={q}",
    .base = "https://data.globalforestwatch.org", .filter_query = 1,
    .description = "Logging, palm oil, pulpwood and mining concession "
      "boundaries with the concession holder where published, joined to "
      "deforestation alerts inside those boundaries — a supply-chain claim "
      "checked against satellite fact" },

};

HP_REGISTER_TABLE(HP3B31_ASSETREG)
