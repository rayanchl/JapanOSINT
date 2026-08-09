#!/usr/bin/env bash
# Dry-run disable+backfill on a throwaway DB seeded from the real schema, and
# prove both the quarantine and the history annotation actually landed.
set -eu
cd "$(dirname "$0")/../.." || exit 1
T=$(mktemp -d)
DB="$T/t.db"
JO_DB="$DB" JO_SCHEMA=core/schema.sql ./bin/japanosint --list-sources >/dev/null 2>&1 || true

# seed one intel_items row for a source we are about to retire
sqlite3 "$DB" "INSERT OR IGNORE INTO intel_items (uid,source_id,title,properties)
               VALUES ('gnews-dk-world|x','gnews-dk-world','probe row','{\"guid\":\"x\"}');" 2>/dev/null || true

python3 tests/audit/gnews_retire.py --db "$DB"

echo "--- quarantined sample ---"
sqlite3 "$DB" "SELECT id,substr(quarantine_reason,1,60),quarantined_until
               FROM sources WHERE quarantined_until IS NOT NULL LIMIT 3;"
echo "--- count quarantined ---"
sqlite3 "$DB" "SELECT COUNT(*) FROM sources WHERE quarantined_until>datetime('now');"
echo "--- annotated history row ---"
sqlite3 "$DB" "SELECT source_id,properties FROM intel_items WHERE source_id='gnews-dk-world';"
rm -rf "$T"
