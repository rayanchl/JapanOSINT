#!/bin/bash
u="$1"
b=$(curl -sS --max-time 90 -A 'JapanOSINT/1.0' "$u/oai?verb=Identify" 2>/dev/null)
name=$(printf '%s' "$b" | grep -o '<repositoryName>[^<]*' | head -1 | sed 's/<repositoryName>//')
email=$(printf '%s' "$b" | grep -o '<earliestDatestamp>[^<]*' | head -1 | sed 's/<earliestDatestamp>//')
printf '%s\t%s\t%s\n' "$u" "$name" "$email"
