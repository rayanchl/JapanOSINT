import json, sys, urllib.request
def get(u, t=60):
    return json.load(urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'}), timeout=t))
host, rid = sys.argv[1], sys.argv[2]
r = get(f'https://{host}/api/3/action/resource_show?id={rid}')['result']
p = get(f'https://{host}/api/3/action/package_show?id={r["package_id"]}')['result']
print(p['title'], '|', p.get('name'), '| org', (p.get('organization') or {}).get('title'), '| mod', p.get('metadata_modified'))
for x in p['resources']:
    print(' ', x['id'], x.get('format'), x.get('datastore_active'), x.get('size'), x.get('last_modified'), x.get('name')[:50], x['url'][:160])
