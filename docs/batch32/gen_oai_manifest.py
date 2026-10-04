import csv, html, math, re
counts = {}
for l in open('oai_ranked.tsv'):
    f = l.rstrip('\n').split('\t'); counts[f[1]] = (int(f[0]), f[5])
irdb = {}
for l in open('irdb_rows.tsv'):
    f = l.rstrip('\n').split('\t'); irdb[f[3].strip().rstrip('/')] = (f[1].strip(), f[2].strip())
ident = {}
for l in open('oai_identify.tsv'):
    f = l.rstrip('\n').split('\t'); ident[f[0]] = (html.unescape(f[1]).strip(), f[2] if len(f) > 2 else '')
rows = []
for base in [l.strip() for l in open('oai_top100.txt')]:
    n, secs = counts[base]
    inst, repo_ja = irdb.get(base, ('', ''))
    rname = ident.get(base, ('', ''))[0] or repo_ja
    earliest = ident.get(base, ('', ''))[1]
    host = base.split('//')[1]
    short = host.split('.')[0].upper().replace('-', '_')
    sid = 'JO32_REPO_%s_OAI' % short
    pmax = math.ceil(n / 100) + 5
    url = base + '/oai?verb=ListRecords&metadataPrefix=oai_dc'
    en = rname.replace('|', '/')
    ja = ('%s %s' % (inst, repo_ja)).strip().replace('|', '/')
    desc = ("The complete deposited output of %s (%s), read through the repository's own JAIRO Cloud OAI-PMH interface. "
            "Fetched 2026-09-30: ListIdentifiers reported completeListSize=%s and ListRecords page 1 returned 100 oai_dc records, each with a header identifier (oai:%s:NNNNNNNN), datestamp, dc:title, every dc:creator, dc:publisher, dc:type (departmental bulletin paper, journal article, doctoral thesis ...), dc:identifier (handle and /records/ URL) and dc:rights%s. "
            "Pages are 100 records joined by resumptionToken (about 50 s each from this host), so page_max=%d reaches the end of the %s records with headroom; the first full walk is long and runs on the weekly clock. "
            "It links named Japanese researchers to bulletin papers, theses and reports that often reach no commercial index.") % (
            en or host, inst or host, format(n, ','), host,
            ('; earliest datestamp %s' % earliest[:10]) if earliest else '', pmax, format(n, ','))
    opts = ('interval=604800;timeout_ms=150000;page_max=%d;array_path=record;title_keys=dc:title;id_keys=identifier,dc:identifier;'
            'date_keys=datestamp,dc:date;next_path=resumptionToken;next_tmpl=%s/oai?verb=ListRecords&resumptionToken={v}') % (pmax, base)
    rows.append('|'.join([sid, 'xml', 'any', 'research', 'repository-record',
                          'japan,academic,repository,oai-pmh,thesis,university', base,
                          '%s — OAI-PMH repository' % (en or host), '%s 機関リポジトリ' % (inst or repo_ja or host),
                          url, url, desc, opts]))
hdr = """# Batch 32 — jprepo2: Japanese institutional repositories on JAIRO Cloud (repo.nii.ac.jp) that were not yet registered.
# Enumerated from IRDB's own repository list (irdb.nii.ac.jp/repositorylist, 849 repositories, 17 pages) on 2026-09-30:
# 751 are on JAIRO Cloud, 95 were already in the tree, and each of the other 600 was asked for ListIdentifiers to read
# its completeListSize. These are the 100 largest (62,484 down to ~2,700 records). Opts mirror batch 25 jpacademic,
# except page_max, which is sized from each repository's own completeListSize so the walk reaches the end.
# id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts
"""
open('../candidate-sources-batch32.jprepo2.txt', 'w').write(hdr + '\n'.join(rows) + '\n')
print(len(rows), 'rows', sum(counts[b][0] for b in [l.strip() for l in open('oai_top100.txt')]), 'records')
