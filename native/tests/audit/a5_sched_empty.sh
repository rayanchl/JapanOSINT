#!/usr/bin/env bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
for id in atlas-jp classifieds gdelt-events jartic-traffic mlit-n02-stations \
          mofa-travel-advisory sslbl-jp hurriyet-en greynoise-jp; do
  echo "############## $id"
  python3 tests/audit/verify_one.py --slot a5 --stderr --rows 0 --timeout 150 "$id" 2>&1 \
    | grep -vE '^\[db\]' | sed -n '2,18p'
done
