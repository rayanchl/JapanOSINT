/**
 * Bear sighting / encounter reports — bear-encounters.
 *
 * Bear sighting data (クマ出没情報) is published per-prefecture as open data
 * (e.g. Ishikawa, Akita, Niigata, Nagano), each in its own CSV/GeoJSON shape
 * with no national feed. The Ministry of the Environment aggregates annual
 * counts only (no point geometry). This collector does a real best-effort
 * fetch of a point dataset supplied via the optional env var
 * BEAR_ENCOUNTERS_GEOJSON_URL (GeoJSON FeatureCollection of sighting points)
 * and returns an HONEST EMPTY FeatureCollection when none is configured or
 * the fetch fails. No fabricated sighting points, ever.
 *
 * NOTE: there is no canonical national endpoint; the env-supplied source is
 * prefecture-specific and its property schema is handled defensively.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelHashKey } from '../utils/intelHelpers.js';

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'Bear sighting / encounter reports (prefectural open data, requires BEAR_ENCOUNTERS_GEOJSON_URL)',
    },
  };
}

function prop(p, keys) {
  for (const k of keys) {
    if (p[k] != null && String(p[k]).trim() !== '') return p[k];
  }
  return null;
}

export default async function collectBearEncounters() {
  const url = envFor('BEAR_ENCOUNTERS_GEOJSON_URL');
  if (!url) return emptyResult('bear_encounters_no_source');

  let data = null;
  try {
    data = await fetchJson(url, { timeoutMs: 20000 });
  } catch {
    return emptyResult('bear_encounters_unavailable');
  }
  const src = Array.isArray(data?.features) ? data.features : null;
  if (!src) return emptyResult('bear_encounters_unavailable');

  const features = [];
  for (const f of src) {
    let lon, lat;
    if (f?.geometry?.type === 'Point' && Array.isArray(f.geometry.coordinates)) {
      lon = Number(f.geometry.coordinates[0]);
      lat = Number(f.geometry.coordinates[1]);
    } else {
      const p = f?.properties || {};
      lat = Number(prop(p, ['lat', 'latitude', '緯度']));
      lon = Number(prop(p, ['lon', 'lng', 'longitude', '経度']));
    }
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
    if (lat < 20 || lat > 46 || lon < 122 || lon > 154) continue;
    const p = f?.properties || {};
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: {
        sighting_id:
          prop(p, ['id', 'ID', '番号']) ?? `BEAR_${intelHashKey(lon, lat, prop(p, ['date', '日付']))}`,
        date: prop(p, ['date', '日付', '発生日時', '目撃日時']),
        place: prop(p, ['place', '場所', '市町村', 'address', '住所']),
        species: prop(p, ['species', '種別', 'クマの種類']),
        note: prop(p, ['note', '備考', '状況']),
        source: 'bear_encounters',
      },
    });
  }

  if (features.length === 0) return emptyResult('bear_encounters_unavailable');

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'bear_encounters',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Bear sighting / encounter reports (prefectural open data, requires BEAR_ENCOUNTERS_GEOJSON_URL)',
    },
  };
}
