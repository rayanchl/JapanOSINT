/**
 * JMA Tsunami information collector (jma-tsunami).
 *
 * Source: https://www.jma.go.jp/bosai/tsunami/data/list.json — the key-free
 * public JMA bosai feed, structurally identical to the quake list.json used
 * by jmaEarthquake.js (ISO 6709 `cod` hypocenter string). Each entry is a
 * tsunami advisory/observation bulletin tied to a source earthquake.
 *
 * Returns an honest empty FeatureCollection when JMA is unreachable or no
 * bulletins are active — never fabricated points.
 */

import { fetchJson } from './_liveHelpers.js';

const API_URL = 'https://www.jma.go.jp/bosai/tsunami/data/list.json';

// Parse JMA ISO 6709 hypocenter string ("+39.8+143.2-20000/" → lat/lon/depth).
function parseIso6709(cod) {
  if (typeof cod !== 'string') return null;
  const m = cod.match(/^([+-]\d+(?:\.\d+)?)([+-]\d+(?:\.\d+)?)(?:([+-]\d+(?:\.\d+)?))?/);
  if (!m) return null;
  const lat = parseFloat(m[1]);
  const lon = parseFloat(m[2]);
  const depthM = m[3] != null ? parseFloat(m[3]) : null;
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
  return {
    lat,
    lon,
    depthKm: depthM != null && Number.isFinite(depthM) ? Math.abs(depthM) / 1000 : null,
  };
}

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'JMA tsunami advisories / observations (bosai tsunami feed)',
    },
  };
}

export default async function collectJmaTsunami() {
  const data = await fetchJson(API_URL, { timeoutMs: 8000 }).catch(() => null);
  if (!Array.isArray(data)) return result([], 'jma_tsunami_unavailable');

  const features = [];
  for (const t of data) {
    const parsed = parseIso6709(t.cod);
    if (!parsed) continue;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [parsed.lon, parsed.lat] },
      properties: {
        bulletin_id: t.eid ?? null,
        title: t.ttl ?? t.en_ttl ?? null,
        information_type: t.ift ?? null,
        area_name: t.anm ?? null,
        area_name_en: t.en_anm ?? null,
        magnitude: t.mag != null && t.mag !== '' ? parseFloat(t.mag) : null,
        depth_km: parsed.depthKm,
        origin_time: t.at ?? null,
        report_time: t.rdt ?? null,
        serial: t.ser ?? null,
        kind: Array.isArray(t.kind) ? t.kind : [],
        json: t.json ?? null,
        source: 'jma_tsunami',
      },
    });
  }
  return result(features, features.length ? 'jma_tsunami' : 'jma_tsunami_empty');
}
