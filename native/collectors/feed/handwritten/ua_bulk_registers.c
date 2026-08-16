/* ua_bulk_registers.c — Ukraine Interior Ministry / National Police bulk
 * registers, streamed.
 *
 * HAND-WRITTEN, and living outside feed/generated on purpose: everything under
 * feed/generated is rendered from a manifest and is deleted and re-rendered on
 * every regeneration. An earlier version of this file sat there and was wiped
 * by the generator's cleanup glob.
 *
 * These are among the highest-value public records in the batch and every one
 * was previously unreachable, for reasons that had nothing to do with the data:
 * collectors/verify_feeds.py rejects a body over 8 MB as TOO_BIG, and VJSON
 * buffers the whole array before parsing it. So a size limit, not a coverage
 * decision, was keeping ~2.4M person, vehicle and device records out of the
 * platform.
 *
 * VJSONBIG streams them — each top-level element is parsed and emitted as the
 * bytes arrive, so peak memory is one record whatever the file weighs. Verified
 * against carswanted.json: 79,360 of 79,360 elements, matching an independent
 * Python count of the same array exactly, from 37 MB at 114 MB peak RSS. See
 * lib/jsonstream.h.
 *
 * All five are keyless JSON arrays on data.gov.ua resource URLs, fetched and
 * read before being written down here.
 *
 * Daily: these are national registers that change on filing, not live feeds,
 * and each run re-reads a large file. The firearms register is weekly — it is
 * ~714 MB. */
#include "_verified_macros.inc"

#define UA_DG "https://data.gov.ua/dataset"
#define UA_TAGS "[\"ua\",\"legal\",\"batch17\",\"high-penetrancy\",\"streamed\"]"

VJSONBIG(ua_mvs_wanted_persons, "ua-mvs-wanted-persons",
  "Ukraine MVS wanted persons register",
  "Ukraine MVS wanted persons register",
  "ua_legal", "legal",
  UA_DG "/59ecf2ab-47a1-4fae-a63c-fe5007d68130/resource/"
  "9694e34c-92a5-4839-91df-c32850db7ba9/download/mvswantedperson_1.json",
  "", "uk", UA_TAGS, 86400,
  "~73,000 persons wanted by Ukrainian Interior Ministry bodies. Per record: ID, "
  "OVD (the exact police or SBU unit that opened the search), CATEGORY (hiding "
  "from prosecution / from pre-trial investigation), surname, first name and "
  "patronymic in Ukrainian, Russian AND Latin transliteration, BIRTH_DATE, SEX, "
  "LOST_DATE, LOST_PLACE, ARTICLE_CRIM (criminal code article), RESTRAINT (the "
  "court detention order and its date), CONTACT phone and PHOTOID.");

VJSONBIG(ua_police_missing_persons, "ua-police-missing-persons",
  "Ukraine missing persons register",
  "Ukraine missing persons register",
  "ua_legal", "legal",
  UA_DG "/206741b9-2dc1-4aff-bbfa-c0a3fd254447/resource/"
  "af13068e-6aa2-4c4f-a9b3-e76328ba4a01/download/bezvistiwanted.json",
  "", "uk", UA_TAGS, 86400,
  "~122,000 people registered missing in Ukraine: id, ovd (police unit), "
  "category, names in Ukrainian and Russian, birth_date, sex, lost_date, "
  "lost_place down to settlement, plus a structured appearance block describing "
  "face shape, hair, nose, brows and distinguishing marks.");

VJSONBIG(ua_police_cars_wanted, "ua-police-cars-wanted",
  "Ukraine stolen and seized vehicles register",
  "Ukraine stolen and seized vehicles register",
  "ua_legal", "legal",
  UA_DG "/2cd12755-834b-4c43-9026-fe356ce93af5/resource/"
  "2d69d196-02f1-49b9-97fa-0fb69077e05f/download/carswanted.json",
  "", "uk", UA_TAGS, 86400,
  "79,360 vehicles under search, pivotable directly from a plate or VIN: id, "
  "brand, model, cartype, color, vehiclenumber (plate), bodynumber (VIN), "
  "chassisnumber, enginenumber, illegalseizuredate, the registering police unit "
  "and insertdate.");

VJSONBIG(ua_police_stolen_phones, "ua-police-stolen-phones",
  "Ukraine stolen mobile devices register (IMEI)",
  "Ukraine stolen mobile devices register (IMEI)",
  "ua_legal", "legal",
  UA_DG "/b80e556b-08c1-4ba9-a63d-cd6fb827a058/resource/"
  "2059a788-fa1e-4003-b0f4-1a8c342b3b48/download/wantedmt.json",
  "", "uk", UA_TAGS, 86400,
  "~1,060,000 stolen handsets searchable by IMEI: id, ovd (police unit), "
  "insert_date, nz (make and model), imei, nk criminal-proceeding number and dk "
  "its date.");

/* ~714 MB, ~1.1M records — the largest file in the fleet and the reason
 * lib/jsonstream.c exists. Weekly, not daily. */
VJSONBIG(ua_police_weapons_wanted, "ua-police-weapons-wanted",
  "Ukraine wanted firearms register",
  "Ukraine wanted firearms register",
  "ua_legal", "legal",
  UA_DG "/7bf715e8-b355-4db5-8f6b-c8c5e4087d3b/resource/"
  "a68e054b-91a2-486e-9351-7b97408d660f/download/weaponswanted.json",
  "", "uk", UA_TAGS, 604800,
  "~1,100,000 firearms lost or stolen in Ukraine: id, brandmodel, producer "
  "country, weapontype, weaponkind, weaponseries, weaponnumber, weaponcaliber, "
  "trunks, graduationyear, the registering police unit, reasonsearch (loss or "
  "theft), insertdate and theftdate. ~714 MB, streamed element by element.");
