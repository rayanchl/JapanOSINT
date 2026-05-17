/**
 * Police incident locations — police-incidents.
 *
 * Prefectural police "犯罪情報マップ" / 事件発生情報 are published per-force
 * with no national feed, and most are ArcGIS/Flash/JS map viewers that do not
 * expose a stable open endpoint. Where a force publishes a machine-readable
 * incident export we accept its URL via the optional env var
 * POLICE_INCIDENTS_CSV_URL (CSV with lat/lon columns) — a real best-effort
 * fetch. With no configured source (or on failure) we return an HONEST EMPTY
 * FeatureCollection. No fabricated incident points, ever.
 *
 * NOTE: endpoint shape is force-specific and unverified for any given
 * prefecture; the CSV adapter below assumes columns lat/lon/date/type and is
 * defensive about header names. Empty-on-miss keeps the contract honest.
 */

import { fetchText, parseCsv } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

function pickNum(row, keys) {
  for (const k of keys) {
    if (row[k] != null && row[k] !== '') {
      const n = Number(row[k]);
      if (Number.isFinite(n)) return n;
    }
  }
  return null;
}
function pickStr(row, keys) {
  for (const k of keys) {
    if (row[k] != null && String(row[k]).trim() !== '') return String(row[k]).trim();
  }
  return null;
}

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'Prefectural police reported incident locations (best-effort, requires POLICE_INCIDENTS_CSV_URL)',
    },
  };
}

export default async function collectPoliceIncidents() {
  const url = envFor('POLICE_INCIDENTS_CSV_URL');
  if (!url) return emptyResult('police_incidents_no_source');

  let text;
  try {
    text = await fetchText(url, { timeoutMs: 20000 });
  } catch {
    return emptyResult('police_incidents_unavailable');
  }
  if (!text) return emptyResult('police_incidents_unavailable');

  let rows;
  try {
    rows = parseCsv(text, { headers: true });
  } catch {
    return emptyResult('police_incidents_unavailable');
  }

  const features = [];
  for (const r of rows) {
    const lat = pickNum(r, ['lat', 'latitude', '緯度', 'Y', 'y']);
    const lon = pickNum(r, ['lon', 'lng', 'longitude', '経度', 'X', 'x']);
    if (lat == null || lon == null) continue;
    if (lat < 20 || lat > 46 || lon < 122 || lon > 154) continue;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: {
        incident_type: pickStr(r, ['type', '罪種', '手口', 'category']),
        date: pickStr(r, ['date', '発生年月日', '日付', 'datetime']),
        place: pickStr(r, ['place', '発生場所', '市区町村', 'address']),
        source: 'police_incidents',
      },
    });
  }

  if (features.length === 0) return emptyResult('police_incidents_unavailable');

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'police_incidents',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Prefectural police reported incident locations (best-effort, requires POLICE_INCIDENTS_CSV_URL)',
    },
  };
}
