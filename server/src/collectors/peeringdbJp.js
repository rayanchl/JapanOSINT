/**
 * PeeringDB — JP networks, IXPs and facilities (org-level metadata).
 *
 * Free, no key. We pull `country=JP` slices from /api/net, /api/ix and
 * /api/fac to build a single org-aware feature collection.
 */

const BASE = 'https://www.peeringdb.com/api';
const TIMEOUT_MS = 20000;

async function fetchPdb(path) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(`${BASE}${path}`, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(t);
    if (!r.ok) return [];
    const j = await r.json();
    return Array.isArray(j?.data) ? j.data : [];
  } catch { return []; }
}

const TOKYO = [139.6917, 35.6895];

export default async function collectPeeringdbJp() {
  const [nets, ixs, facs] = await Promise.all([
    fetchPdb('/net?info_traffic__contains=&info_scope__contains=&info_type__contains=&policy_general__contains=&country=JP&depth=0&limit=1000'),
    fetchPdb('/ix?country=JP&depth=0&limit=500'),
    fetchPdb('/fac?country=JP&depth=0&limit=1000'),
  ]);

  const features = [];

  ixs.forEach((ix, i) => {
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        kind: 'ixp',
        pdb_id: ix.id,
        name: ix.name,
        name_long: ix.name_long,
        media: ix.media,
        country: ix.country,
        city: ix.city,
        net_count: ix.net_count,
        proto_unicast: ix.proto_unicast,
        proto_multicast: ix.proto_multicast,
        proto_ipv6: ix.proto_ipv6,
        url: `https://www.peeringdb.com/ix/${ix.id}`,
        source: 'peeringdb_ix',
      },
    });
  });

  facs.forEach((f, i) => {
    const lon = Number(f.longitude); const lat = Number(f.latitude);
    const geom = (Number.isFinite(lon) && Number.isFinite(lat))
      ? { type: 'Point', coordinates: [lon, lat] }
      : { type: 'Point', coordinates: TOKYO };
    features.push({
      type: 'Feature',
      geometry: geom,
      properties: {
        idx: i,
        kind: 'fac',
        pdb_id: f.id,
        name: f.name,
        org_id: f.org_id,
        country: f.country,
        city: f.city,
        clli: f.clli,
        rencode: f.rencode,
        npanxx: f.npanxx,
        net_count: f.net_count,
        url: `https://www.peeringdb.com/fac/${f.id}`,
        source: 'peeringdb_fac',
      },
    });
  });

  nets.forEach((n, i) => {
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        kind: 'net',
        pdb_id: n.id,
        asn: n.asn,
        name: n.name,
        aka: n.aka,
        info_type: n.info_type,
        info_traffic: n.info_traffic,
        info_scope: n.info_scope,
        info_ratio: n.info_ratio,
        irr_as_set: n.irr_as_set,
        info_prefixes4: n.info_prefixes4,
        info_prefixes6: n.info_prefixes6,
        url: `https://www.peeringdb.com/net/${n.id}`,
        source: 'peeringdb_net',
      },
    });
  });

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'peeringdb',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      ix_count: ixs.length,
      fac_count: facs.length,
      net_count: nets.length,
      description: 'PeeringDB — Japan IXPs, facilities, networks (org metadata)',
    },
  };
}
