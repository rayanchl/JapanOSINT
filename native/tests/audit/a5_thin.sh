#!/usr/bin/env bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
for id in blitzortung-lightning food-poisoning jr-central-delay kakaku-prices \
          mic-broadcast-towers ndb-open pogo-raids-jp sumo-tournaments \
          yahoo-auctions ripestat-jp wantedly-bizreach cam-camstreamer; do
  echo "############## $id"
  python3 tests/audit/verify_one.py --slot a5 --rows 1 --timeout 120 "$id" 2>&1 | sed -n '2,30p'
done
