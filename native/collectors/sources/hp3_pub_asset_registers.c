/* collectors/sources/hp3_pub_asset_registers.c — public registers of things.
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
 * oil and land deals. */
#include "../../lib/hpengine.h"

static const hp_source HP3_PUB_ASSETS[] = {
  /* ── Land and cadastre ────────────────────────────────────────────────── */
  { .id = "ES_CATASTRO_PARCELS", .name = "Spain Catastro — cadastral parcel search",
    .name_ja = "スペイン 地籍検索", .category = "government",
    .portal = "https://ovc.catastro.meh.es", .record_type = "es-parcel",
    .tags = "\"es\",\"cadastre\",\"property\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www1.sedecatastro.gob.es/Cartografia/BuscarParcela.aspx?"
      "busqueda={q}",
    .base = "https://www1.sedecatastro.gob.es", .filter_query = 1,
    .description = "Spain's cadastre — the cadastral reference, surface area, "
      "built area, construction year, use classification and cadastral value "
      "for any parcel, plus the graphical boundary. Free and without "
      "registration, which is rare for a European cadastre" },

  { .id = "CZ_CUZK_CADASTRE", .name = "Czech Republic ČÚZK — cadastre of real estate",
    .name_ja = "チェコ 不動産地籍", .category = "government",
    .portal = "https://nahlizenidokn.cuzk.cz", .record_type = "cz-parcel",
    .tags = "\"cz\",\"cadastre\",\"property\",\"ownership\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://nahlizenidokn.cuzk.cz/VyberBudovu.aspx?typ=Parcela&hledat={q}",
    .base = "https://nahlizenidokn.cuzk.cz", .filter_query = 1,
    .description = "The Czech cadastre publishes the owner of each parcel and "
      "building by name, together with the title deed, encumbrances, easements "
      "and pending proceedings. One of the few European property registers "
      "where ownership is openly readable" },

  { .id = "FR_DVF_LAND_SALES", .name = "France DVF — declared property transaction values",
    .name_ja = "フランス 不動産取引価格", .category = "government",
    .portal = "https://app.dvf.etalab.gouv.fr", .record_type = "fr-property-sale",
    .tags = "\"fr\",\"property\",\"prices\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://app.dvf.etalab.gouv.fr/?q={q}",
    .base = "https://app.dvf.etalab.gouv.fr", .filter_query = 1,
    .description = "Demandes de valeurs foncières — every property sale "
      "registered in France with the price, the date, the cadastral parcel, the "
      "surface and the type. The transaction record behind the notarial deed" },

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

  { .id = "INDIAN_OCEAN_MOU_PSC", .name = "Indian Ocean MoU — port state control inspections",
    .name_ja = "インド洋MOU 港湾国監督検査", .category = "maritime",
    .portal = "https://www.iomou.org", .record_type = "psc-inspection",
    .tags = "\"maritime\",\"inspection\",\"indian-ocean\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.iomou.org/ipcqsearch.php?vessel={q}",
    .base = "https://www.iomou.org", .filter_query = 1,
    .description = "Inspections in Indian Ocean ports — India, South Africa, "
      "Iran, Kenya, Australia and the Gulf approaches — with the deficiencies, "
      "detentions and the flag and class of the vessel. The regional regime "
      "covering the routes the European and Asian MoUs do not" },

  { .id = "CA_VESSEL_REGISTRY", .name = "Canada — vessel registration query system",
    .name_ja = "カナダ 船舶登録", .category = "maritime",
    .portal = "https://wwwapps.tc.gc.ca", .record_type = "ca-vessel",
    .tags = "\"ca\",\"maritime\",\"vessel\",\"registry\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://wwwapps.tc.gc.ca/Saf-Sec-Sur/4/vrqs-srib/eng/"
      "vessel-registrations/vessel-registration-results?name={q}",
    .base = "https://wwwapps.tc.gc.ca", .filter_query = 1,
    .description = "Transport Canada's vessel registry — official number, "
      "hull identification, tonnage, the registered owner and mortgagee, and "
      "the port of registry, covering commercial and pleasure craft alike" },

  { .id = "TOKYO_MOU_INSPECTIONS", .name = "Tokyo MoU — Asia-Pacific port state control",
    .name_ja = "東京MOU 港湾国監督検査", .category = "maritime",
    .portal = "https://www.tokyo-mou.org", .record_type = "psc-inspection",
    .tags = "\"maritime\",\"inspection\",\"asia-pacific\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.tokyo-mou.org/inspections_detentions/psc_database.php"
      "?vessel={q}",
    .base = "https://www.tokyo-mou.org", .filter_query = 1,
    .description = "Port state control inspections across the Asia-Pacific "
      "region including Japan, with the deficiency detail, detention record and "
      "the flag and recognised organisation performance tables" },

  /* ── Aircraft ─────────────────────────────────────────────────────────── */
  { .id = "UK_CAA_GINFO_REGISTER", .name = "UK CAA G-INFO — aircraft register",
    .name_ja = "英国民間航空局 航空機登録", .category = "transport",
    .portal = "https://siteapps.caa.co.uk", .record_type = "uk-aircraft",
    .tags = "\"uk\",\"aviation\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://siteapps.caa.co.uk/g-info/?search={q}",
    .base = "https://siteapps.caa.co.uk", .filter_query = 1,
    .description = "The UK civil aircraft register — registration mark, type, "
      "serial number, registered owner and its address, and the "
      "certificate-of-airworthiness status" },

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
  { .id = "AU_ACMA_SPECTRUM_LICENCES", .name = "Australia ACMA — radiocommunications licence register",
    .name_ja = "豪州ACMA 無線免許登録", .category = "infrastructure",
    .portal = "https://web.acma.gov.au", .record_type = "au-spectrum-licence",
    .tags = "\"au\",\"spectrum\",\"licence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://web.acma.gov.au/rrl/register_search.main_page?search={q}",
    .base = "https://web.acma.gov.au", .filter_query = 1,
    .description = "Australia's register of radiocommunications licences — the "
      "licensee, the frequency assignment, the transmitter site coordinates, "
      "the antenna details and the emission designator. Site coordinates make "
      "this a physical-infrastructure map as well as a licence list" },

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

  { .id = "ITU_SPACE_NETWORKS", .name = "ITU — satellite network filings & frequency assignments",
    .name_ja = "ITU 衛星ネットワーク登録", .category = "infrastructure",
    .portal = "https://www.itu.int", .record_type = "satellite-filing",
    .tags = "\"space\",\"spectrum\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.itu.int/net/search/index.aspx?cx=&q={q}",
    .base = "https://www.itu.int", .filter_query = 1,
    .description = "Satellite network filings — the notifying administration, "
      "the orbital position, the frequency bands and the bringing-into-use "
      "date. Orbital slots are held by states on behalf of named operators, and "
      "the filing is the only public record of that arrangement" },

  /* ── Extraction, energy and environmental permits ─────────────────────── */
  { .id = "AU_MINING_TENEMENTS", .name = "Australia — state mining tenement registers",
    .name_ja = "豪州 鉱区登録", .category = "energy",
    .portal = "https://geoview.dmp.wa.gov.au", .record_type = "au-mining-tenement",
    .tags = "\"au\",\"mining\",\"licence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://minedexext.dmirs.wa.gov.au/web/guest/minedex-search?q={q}",
    .base = "https://minedexext.dmirs.wa.gov.au", .filter_query = 1,
    .description = "Western Australian mineral tenements and MINEDEX project "
      "records — the tenement holder, the commodity, the status, the "
      "expenditure commitment and the operating status of each deposit" },

  { .id = "CLIMATE_TRACE_ASSETS", .name = "Climate TRACE — asset-level emissions & ownership",
    .name_ja = "Climate TRACE 施設別排出/所有", .category = "environment",
    .portal = "https://api.climatetrace.org", .record_type = "emitting-asset",
    .tags = "\"emissions\",\"assets\",\"ownership\"", .free_tier = 1,
    .url = "https://api.climatetrace.org/v6/assets?limit=1000",
    .post_body = "{\"name\":\"{Q}\"}", .content_type = "application/json",
    .array_path = "assets", .filter_query = 1,
    .title_keys = "Name,Sector", .id_keys = "Id",
    .lat_key = "Centroid.Geometry.1", .lon_key = "Centroid.Geometry.0",
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

  { .id = "US_BLM_MINERAL_LEASES", .name = "US BLM — federal mineral & land leases",
    .name_ja = "米国土地管理局 鉱区リース", .category = "energy",
    .portal = "https://reports.blm.gov", .record_type = "us-mineral-lease",
    .tags = "\"us\",\"mining\",\"oil-gas\",\"lease\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://reports.blm.gov/reports/LR2000?q={q}",
    .base = "https://reports.blm.gov", .filter_query = 1,
    .description = "Bureau of Land Management case records — oil, gas, coal "
      "and hardrock leases on federal land with the lessee of record, the "
      "serial number, the acreage, the legal land description and the lease "
      "status. Federal minerals are held by name and the name is public" },

  { .id = "EU_INDUSTRIAL_EMISSIONS_PORTAL", .name = "EU Industrial Emissions Portal — permitted installations",
    .name_ja = "EU 産業排出施設登録", .category = "environment",
    .portal = "https://industry.eea.europa.eu", .record_type = "eu-industrial-facility",
    .tags = "\"eu\",\"environment\",\"industry\",\"permit\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://industry.eea.europa.eu/explore/explore-data-map/results?q={q}",
    .base = "https://industry.eea.europa.eu", .filter_query = 1,
    .description = "Every permitted industrial installation in the EEA — the "
      "operator and parent company, the site coordinates, the activities "
      "permitted, the pollutant releases and transfers, and the inspection and "
      "non-compliance record held by the national regulator" },
};

HP_REGISTER_TABLE(HP3_PUB_ASSETS)
