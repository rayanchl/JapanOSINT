#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native
grep -oE '^## `native/collectors/sources/[a-z0-9_]+\.c`' tests/audit/slice_05.md | sed 's/^## `//; s/`$//' | sed 's#^native/##' > tests/audit/a4_files.txt
wc -l tests/audit/a4_files.txt
