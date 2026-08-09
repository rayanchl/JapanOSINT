#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native
FILES=$(cat tests/audit/a4_files.txt)
echo "=== probe.h users in slice 5"
grep -l 'lib/probe.h' $FILES
echo
echo "=== all probe.h users repo-wide (count)"
grep -rl 'lib/probe.h' collectors/sources/ | wc -l
