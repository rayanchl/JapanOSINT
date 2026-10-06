# Builds docs/candidate-sources-batch38.jpnat3.txt from the verified rows below.
# Every row here was fetched with curl on 2026-10-02 and its counts are the ones observed.
import sys

OUT = "/Users/rayan/OSINTsaas/docs/candidate-sources-batch38.jpnat3.txt"
HEAD = [
    "# Batch 38 - jpnat3: Japanese NATIONAL bodies and institutions - ministry/agency registers and enforcement data,",
    "# science and research archives (NDL Imperial Diet API, DDBJ), national statistics and licence registers published as CSV/XLSX/JSON.",
    "# Every row fetched with curl and counted 2026-10-02 (JST); id uniqueness checked over the fetched records.",
    "# id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts",
]
ROWS = []


def row(id, mode, category, record_type, tags, portal, name, name_ja, url, desc, opts, probe=None, want="any"):
    for f in (id, mode, category, record_type, tags, portal, name, name_ja, url, desc, opts):
        assert "|" not in f, (id, f)
        assert "\n" not in f, (id, f)
    assert "interval=" in opts, id
    ROWS.append([id, mode, want, category, record_type, tags, portal, name, name_ja, url, probe or url, desc, opts])


exec(open("/Users/rayan/OSINTsaas/docs/batch38/jpnat3_scratch/rows_data.py", encoding="utf-8").read())

ids = [r[0] for r in ROWS]
assert len(ids) == len(set(ids)), "dup id"
urls = [r[9] for r in ROWS]
assert len(urls) == len(set(urls)), "dup url"
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(HEAD) + "\n")
    for r in ROWS:
        f.write("|".join(r) + "\n")
print("wrote", len(ROWS), "rows")
