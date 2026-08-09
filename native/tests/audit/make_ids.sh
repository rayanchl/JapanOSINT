#!/usr/bin/env bash
# Build the audit work list: "<id>[<TAB><entity>]" per line.
#
# Entity-pivot services (collector=osint) take their subject as a second
# argument and have nothing to search for without one. Hand each a type-plausible
# probe value so the audit exercises the REAL path, not only the
# missing-argument path. Everything else runs bare.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-/tmp/joaudit/ids.tsv}"
mkdir -p "$(dirname "$OUT")"

"$HERE/list_sources.sh" | awk -F'\t' '
  $2 == "osint" {
    id = tolower($1)
    e = "example.com"                                   # default: a domain
    if (id ~ /ip|asn|net|geoip|shodan|censys|cidr/)      e = "8.8.8.8"
    else if (id ~ /email|breach|leak|pwn|credential/)    e = "test@example.com"
    else if (id ~ /phone|carrier|hlr|msisdn/)            e = "+81312345678"
    else if (id ~ /vessel|imo|mmsi|ship|ais/)            e = "9074729"
    else if (id ~ /flight|icao24|tail|aircraft/)         e = "JA8089"
    else if (id ~ /person|name|user|handle|social/)      e = "john smith"
    else if (id ~ /company|corp|org|business|houjin/)    e = "Sony"
    else if (id ~ /wallet|btc|eth|crypto|address/)       e = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"
    else if (id ~ /hash|malware|sample|virus/)           e = "44d88612fea8a8f36de82e1278abb02f"
    print $1 "\t" e
    next
  }
  { print $1 }
'  > "$OUT"

printf 'work list: %s lines -> %s\n' "$(wc -l < "$OUT")" "$OUT" >&2
printf '  entity-pivot (osint): %s\n' "$(awk -F'\t' 'NF>1' "$OUT" | wc -l)" >&2
