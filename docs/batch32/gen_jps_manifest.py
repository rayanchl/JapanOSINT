import json, math, re
PICK = """bibnl s_net dignl najda nij10 nij16 nme_trackInfo tokyomuseumcolection nme_mocat nmj01 nij01 fcm_db cria
arc_nishikie FishPix madb_animation cobas saitama_museum nij11 arc_photodb enpaku_engekijoen NagoyaCity_Museum arc_ban
tpada_kenmei nij13 exhib miemu02 bunka NarahakuBijutsuinDB madb_game syozo minamata Showa_magazine saitama_monjo_search
hpmm_magazine Showa_photo photo miemug rekibun aokenshida_doc u006 hpmm_abomb nda01 wkym_koubunsyo madb_mediaart
kaizuArchive wkym_kankoubutu enpk_eigaprogram rih02 nme_movcat tokei nfad aokenshida_pic NarahakuBijutsuinDB2 nij20
ojiya_dna jomon_archives musee_gsj shisan hpmm_abombdrawing xm_okinoshima kyotogyoen_archives ARC_maps michi cb1
Koubun_das Showa_video sf004 Showa_map hpmm_photo gyosei bunkazai_video kochizu_collection miebunkazai Showa_chart
hpmm_testimony tnricp_uritate""".split()
res = {r['id']: r for r in json.load(open('jps_probe.json'))}
UNF = 32846684
rows = []
for k in PICK:
    r = res[k]
    hit = r['hit']; n = r['n']
    reach = min(hit, 2000)
    pmax = math.ceil(reach / 500)
    geo = r['geo'] > 0
    en = (r['name'].get('en') or k).strip().replace('|', '/')
    ja = r['name']['ja'].strip().replace('|', '/')
    sid = 'JO32_JPS_' + re.sub(r'[^A-Z0-9]+', '_', k.upper()).strip('_')
    url = r['url']
    tags = 'japan,jpsearch,digital-archive' + (',geo' if geo else '')
    cats = ','.join(r.get('category') or [])
    fields = ['a top-level id (%s-...)' % k, 'common.title', 'common.database/provider/ownerOrg']
    if r['link']: fields.append('common.linkUrl into the source viewer (%d of %d)' % (r['link'], n))
    fields.append('common.lastUpdatedDate (epoch ms)')
    if r['desc']: fields.append('common.description (%d of %d)' % (r['desc'], n))
    if geo: fields.append('common.coordinates {lat,lon} (%d of %d)' % (r['geo'], n))
    first = (r['first_title'] or '').replace('|', '/').replace('\n', ' ')[:60]
    if hit <= 2000:
        bound = 'hit=%s fits inside the 2,000-record window the upstream allows (from+size<=2000, size<=500), so the walk of %d page(s) of 500 is the complete set.' % (format(hit, ','), pmax)
    else:
        bound = 'The upstream refuses from+size>2000 (from=1949&size=500 and size=1000 both answer hit=0, measured 2026-09-30), so the 4 pages of 500 declared here reach 2,000 of the %s records: a bounded view of an upstream ceiling, stated here.' % format(hit, ',')
    desc = ("Japan Search jps-cross item API filtered to database %s (%s, category %s) with an empty keyword. Fetched 2026-09-30 with UA JapanOSINT/1.0: hit=%s, the first page (size=500) returned %d records with %d distinct ids, first record '%s'. "
            "Each record carries %s. The filter is honoured: the unfiltered query answers hit=%s. %s") % (
            k, ja, cats or 'n/a', format(hit, ','), n, r['uniq'], first, ', '.join(fields), format(UNF, ','), bound)
    opts = ['interval=604800', 'array_path=list', 'id_keys=id', 'title_keys=common.title', 'date_keys=common.lastUpdatedDate']
    if r['link']: opts.append('link_keys=common.linkUrl')
    if r['desc']: opts.append('body_keys=common.description')
    if geo: opts += ['lat_key=common.coordinates.lat', 'lon_key=common.coordinates.lon']
    opts += ['page_param=from', 'page_size=500', 'page_zero_based=1', 'page_max=%d' % pmax, 'header1=User-Agent: JapanOSINT/1.0', 'timeout_ms=90000']
    rows.append('|'.join([sid, 'json', 'any', 'archive', 'digital-archive-record', tags, 'https://jpsearch.go.jp/database',
                          'Japan Search — %s (%s)' % (en, k), 'ジャパンサーチ — ' + ja, url, url, desc, ';'.join(opts)]))
hdr = """# Batch 32 — jpsearch2: Japan Search (jpsearch.go.jp) databases not yet registered. The tree held 63 of the 320
# connected databases; these are 77 of the 257 that were absent, picked for yield (most hold >1,000 records) and for
# analyst value (national/prefectural/municipal archives, geo-tagged photograph and survey collections, war and disaster
# archives, media databases). Every row was fetched 2026-09-30 through the filtered item API and its records inspected.
# id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts
"""
open('../candidate-sources-batch32.jpsearch2.txt', 'w').write(hdr + '\n'.join(rows) + '\n')
print(len(rows), 'rows;', sum(min(res[k]['hit'], 2000) for k in PICK), 'records per run;', sum(res[k]['hit'] for k in PICK), 'upstream total')
