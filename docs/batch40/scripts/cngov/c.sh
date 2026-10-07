#!/bin/bash
# usage: c.sh URL [outfile] [extra curl args]
UA='JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)'
u="$1"; o="${2:-/dev/stdout}"; shift; shift
curl -sS -L --compressed --max-time 60 -A "$UA" -o "$o" -w '%{http_code} %{size_download}B %{time_total}s %{content_type}\n' "$@" "$u"
