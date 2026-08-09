#!/usr/bin/env bash
B=https://dashboard.shadowserver.org
try() { echo "=== $1"; shift; curl -sS -m 25 -w '\n[HTTP %{http_code} %{content_type}]\n' "$@" | head -c 400; echo; }
try "GET api/data time-series" "$B/api/data/?endpoint=stats/time-series&dataset=compromised_website&geo=JP&days=14"
try "POST api/ json" -X POST -H 'Content-Type: application/json' -d '{"endpoint":"stats/time-series","dataset":"compromised_website","geo":"JP","days":14}' "$B/api/"
try "GET statistics combined" "$B/statistics/combined/timeseries/?dataset=compromised_website&geo=JP&days=14&format=json"
try "GET api/ bare" "$B/api/"
try "GET api/1/ " "$B/api/1/?endpoint=stats/time-series&dataset=compromised_website&geo=JP&days=14"
