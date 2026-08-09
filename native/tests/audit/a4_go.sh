#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native
nohup bash tests/audit/a4_batch.sh tests/audit/a4_all.txt > tests/audit/a4_all.log 2>&1 < /dev/null &
disown
echo "pid $!"
