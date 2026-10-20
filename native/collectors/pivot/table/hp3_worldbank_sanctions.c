/* collectors/pivot/table/hp3_worldbank_sanctions.c -- the World Bank sanctions list.
 *
 * Moved here on 2026-10-19 from collectors/feed/generated/vsrc17_int_sanctions_1.c,
 * where it had lived since batch 17 as a VJSON row with the apikey written into
 * the URL. The key is the one the World Bank's own sanctions page sends from the
 * browser (visible in that page's network requests), not a secret issued to us,
 * but a credential inside a source file is still a credential inside a source
 * file: push protection flags it, rotation breaks the row silently, and the
 * tree's rule is key_env. The VJSON fleet has no key_env, hpengine does. Set
 * WORLDBANK_SANCTIONS_APIKEY (see .env.example); unset, the row stores one
 * "needs credential" notice rather than nothing (rule 1).
 *
 * Same id as before so the row's history in fetch_log and intel_items continues.
 * Verified 2026-10-19 with the key set: 1,546 records emitted and stored, keyed
 * on SUPP_ID (the World Bank's own supplier id), titled SUPP_NAME. */
#include "lib/hpengine.h"

static const hp_source HP3_WORLDBANK_SANCTIONS[] = {
  { .id = "worldbank-debarred-firms",
    .name = "World Bank — debarred and cross-debarred firms and individuals",
    .name_ja = "世界銀行 — 制裁対象企業・個人リスト",
    .category = "sanctions", .portal = "https://www.worldbank.org/en/projects-operations/procurement/debarred-firms",
    .record_type = "sanctions", .tags = "\"int\",\"sanctions\",\"batch17\",\"high-penetrancy\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .key_env = "WORLDBANK_SANCTIONS_APIKEY",
    .url = "https://apigwext.worldbank.org/dvsvc/v1.0/json/APPLICATION/ADOBE_EXPRNCE_MGR/FIRM/SANCTIONED_FIRM?apikey={key}",
    .array_path = "response.zprocsupp",
    .id_keys = "SUPP_ID", .title_keys = "SUPP_NAME",
    .interval = 86400, .timeout_ms = 60000,
    .description = "THE World Bank sanctions list, 1,530 entries: SUPP_NAME (firm or named individual), SUPP_ID, entity type (I=individual/F=firm), full street address, city, country, ineligibility from/to dates, the grounds for debarment and cross-debarment status under the MDB agreement. The label lands on COUNTRY_NAME because country_name outranks the _NAME suffix fallback in the parser; SUPP_NAME is still in every record's props. Key is published in clear in the public debarred-firms page source, not private." },
};

HP_REGISTER_TABLE(HP3_WORLDBANK_SANCTIONS);
