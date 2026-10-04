import json, sys, urllib.request, urllib.parse, concurrent.futures as cf, time
un = json.load(open('jps_uncovered.json'))
cand = [x for x in un if int(x.get('recordCount') or 0) >= 500 and x.get('apiType') == 'ok']
def probe(x):
    u = 'https://jpsearch.go.jp/api/item/search/jps-cross?keyword=&f-db=%s&size=500&from=0' % urllib.parse.quote(x['id'])
    for attempt in range(3):
        try:
            req = urllib.request.Request(u, headers={'User-Agent': 'JapanOSINT/1.0'})
            d = json.load(urllib.request.urlopen(req, timeout=90))
            L = d.get('list', [])
            ids = [r.get('id') for r in L]
            titles = [((r.get('common') or {}).get('title')) for r in L]
            geo = sum(1 for r in L if (r.get('common') or {}).get('coordinates'))
            dates = sum(1 for r in L if (r.get('common') or {}).get('lastUpdatedDate'))
            desc = sum(1 for r in L if (r.get('common') or {}).get('description'))
            link = sum(1 for r in L if (r.get('common') or {}).get('linkUrl'))
            first = L[0] if L else {}
            return dict(id=x['id'], url=u, hit=d.get('hit'), n=len(L), uniq=len(set(ids)), titled=sum(1 for t in titles if t),
                        geo=geo, dates=dates, desc=desc, link=link, first_title=(titles[0] if titles else None),
                        first_id=first.get('id'), db=first.get('common', {}).get('database') if first else None,
                        provider=(first.get('common') or {}).get('provider') if first else None,
                        owner=(first.get('common') or {}).get('ownerOrg') if first else None,
                        rc=x.get('recordCount'), name=x['name'], category=x.get('category'), ctype=x.get('contentsType'),
                        durl=x.get('url'), description=x.get('description'), last=x.get('lastDataUpdated'))
        except Exception as e:
            err = str(e); time.sleep(3)
    return dict(id=x['id'], url=u, error=err)
with cf.ThreadPoolExecutor(6) as ex:
    res = list(ex.map(probe, cand))
json.dump(res, open('jps_probe.json', 'w'), ensure_ascii=False)
ok = [r for r in res if r.get('n')]
print(len(cand), 'candidates;', len(ok), 'returned records;', sum(1 for r in res if 'error' in r), 'errors')
