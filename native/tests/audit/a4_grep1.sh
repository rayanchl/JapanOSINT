#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native
FILES=$(cat tests/audit/a4_files.txt)
echo "=== run() returning a positive count (rc!=0 => scheduler marks error)"
grep -nE 'return +(n|emitted|count|total|out|c|k|i)( *> *0 *\? *0 *: *-1)? *;' $FILES | grep -vE 'return +[a-z]+ *> *0' | head -60
