#!/bin/bash
# Stand up an isolated server with real data for a concurrency measurement.
# Never touches the 22 GB production DB on /mnt/c: works on a copy of the
# 129 MB WSL snapshot, on native fs, on a port nobody else holds.
set -u
W=$HOME/conctest
PORT=4055
mkdir -p "$W"
if [ ! -f "$W/japanmap.db" ]; then
  cp $HOME/jo_audit_2026_09_03/data/japanmap.db "$W/japanmap.db"
  echo "copied db: $(du -h "$W/japanmap.db" | cut -f1)"
fi

# Mint an HS256 token the server will accept (aud=authenticated, no iss check
# because SUPABASE_URL/JO_JWT_ISS are unset).
python3 - "$W" <<'PY'
import base64, hashlib, hmac, json, sys, time
W = sys.argv[1]
sec = b"conc-test-secret"
def b64(b): return base64.urlsafe_b64encode(b).rstrip(b"=")
h = b64(json.dumps({"alg":"HS256","typ":"JWT"},separators=(',',':')).encode())
now = int(time.time())
p = b64(json.dumps({"sub":"conc-test-user","aud":"authenticated","role":"authenticated",
                    "iat":now,"exp":now+86400,"email":"conc@test.local"},
                   separators=(',',':')).encode())
sig = b64(hmac.new(sec, h+b"."+p, hashlib.sha256).digest())
open(W+"/token", "wb").write(h+b"."+p+b"."+sig)
print("token minted")
PY

pkill -f "jobuild/jo --serve" 2>/dev/null
sleep 1
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native
JO_DB="$W/japanmap.db" \
SUPABASE_JWT_SECRET=conc-test-secret \
JO_BIND=127.0.0.1 PORT=$PORT \
JO_SCHED_STAGGER_SEC=9000 \
nohup $HOME/jobuild/jo --serve > "$W/server.log" 2>&1 &
disown
sleep 12
echo "--- health ---"
curl -sS --max-time 5 http://127.0.0.1:$PORT/api/health; echo
echo "--- authed probe ---"
curl -sS --max-time 20 -o /dev/null -w 'status=%{http_code} bytes=%{size_download} t=%{time_total}\n' \
  -H "Authorization: Bearer $(cat "$W/token")" \
  "http://127.0.0.1:$PORT/api/intel/items?limit=5"
echo "--- rows in the db ---"
sqlite3 "$W/japanmap.db" "select count(*) from intel_items" 2>/dev/null
tail -3 "$W/server.log"
