#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit || exit 1
python3 a4b_table.py
echo "=========== rows==0 OR rc!=0 ==========="
awk -F'\t' 'NR>1 && ($2!="0" || $3=="0")' a4b_table.tsv | cut -c1-200
echo "=========== rows>0 and geo==0 (all) ==========="
awk -F'\t' 'NR>1 && $3>0 && $4=="0" {print $1"\t"$3"\t"$8}' a4b_table.tsv | cut -c1-160
