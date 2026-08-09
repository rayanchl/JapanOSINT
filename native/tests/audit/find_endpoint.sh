#!/usr/bin/env bash
# Pull a JS asset and list every URL-ish literal in it, to find the data endpoint
# a page loads its content from.
set -u
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126 Safari/537.36'
curl -s -A "$UA" -L --max-time 25 "$1" -o /tmp/asset.js
echo "size=$(wc -c </tmp/asset.js)"
echo "--- url-ish literals ---"
grep -oE "[\"'][^\"']*\.(json|php|xml|csv)[^\"']*[\"']" /tmp/asset.js | sort -u | head -20
echo "--- fetch/ajax targets ---"
grep -oE "(fetch|ajax|open|url)\s*[:(]\s*[\"'][^\"']+[\"']" /tmp/asset.js | head -20
echo "--- path fragments ---"
grep -oE "[\"']/[a-zA-Z0-9_./-]{6,}[\"']" /tmp/asset.js | sort -u | head -20
