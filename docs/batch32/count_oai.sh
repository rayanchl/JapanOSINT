#!/bin/bash
# $1 = base url; prints base<TAB>http<TAB>completeListSize<TAB>records_on_page1<TAB>seconds
u="$1"
t0=$(date +%s)
body=$(curl -sS --max-time 120 -A 'JapanOSINT/1.0' -w '\n__HTTP%{http_code}' "$u/oai?verb=ListIdentifiers&metadataPrefix=oai_dc" 2>/dev/null)
code=$(printf '%s' "$body" | grep -o '__HTTP[0-9]*' | tail -1 | sed 's/__HTTP//')
size=$(printf '%s' "$body" | grep -o 'completeListSize="[0-9]*"' | head -1 | grep -o '[0-9]*')
n=$(printf '%s' "$body" | grep -o '<header' | wc -l | tr -d ' ')
err=$(printf '%s' "$body" | grep -o '<error code="[^"]*"' | head -1)
printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$u" "$code" "${size:-}" "$n" "$(( $(date +%s)-t0 ))" "$err"
