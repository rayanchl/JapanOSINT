/**
 * OpenStreetMap — recent changesets within Japan's bbox.
 *
 * Free, no auth.
 *   GET https://api.openstreetmap.org/api/0.6/changesets?bbox=…&closed=true
 *   .json or .xml — we use .json for ease.
 */

const URL = 'https://api.openstreetmap.org/api/0.6/changesets.json?bbox=122.0,24.0,153.0,46.0&closed=true&limit=100';
const TIMEOUT_MS = 15000;

export default async function collectOsmChangesetsJp() {
  let json;
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL, { signal: ctrl.signal, headers: { accept: 'application/json', 'user-agent': 'japanosint-collector' } });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    json = await res.json();
  } catch (err) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'osm_changesets_error',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        error: err?.message || 'fetch_failed',
        description: 'OSM JP changesets — fetch failed',
      },
    };
  }

  const arr = Array.isArray(json?.changesets) ? json.changesets : [];
  const features = arr.map((cs, i) => {
    const lon = (Number(cs.min_lon) + Number(cs.max_lon)) / 2;
    const lat = (Number(cs.min_lat) + Number(cs.max_lat)) / 2;
    const geom = (Number.isFinite(lon) && Number.isFinite(lat))
      ? { type: 'Point', coordinates: [lon, lat] }
      : { type: 'Point', coordinates: [139.6917, 35.6895] };
    return {
      type: 'Feature',
      geometry: geom,
      properties: {
        idx: i,
        cs_id: cs.id,
        user: cs.user,
        uid: cs.uid,
        created_at: cs.created_at,
        closed_at: cs.closed_at,
        comment: cs?.tags?.comment || null,
        created_by: cs?.tags?.created_by || null,
        source_tag: cs?.tags?.source || null,
        changes_count: cs.changes_count,
        bbox: [cs.min_lon, cs.min_lat, cs.max_lon, cs.max_lat],
        url: cs.id ? `https://www.openstreetmap.org/changeset/${cs.id}` : null,
        source: 'osm_changesets_jp',
      },
    };
  });

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'osm_changesets_jp',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'OSM changesets within JP bbox — recent 100',
    },
  };
}
