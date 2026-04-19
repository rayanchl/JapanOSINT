/**
 * Unified Subway Stations - fuses and deduplicates subway / metro / monorail / tram / light_rail stops:
 *   - MLIT N02 (authoritative station geometry, filtered by type/operator)
 *   - ODPT subway operators (Tokyo Metro, Toei, Osaka Metro, etc.)
 *   - fullTransport (OSM railway/station/monorail/tram via tiled Overpass)
 *   - osmTransportSubways (always-on dedicated OSM transport layer)
 */

import mlitN02Stations from './mlitN02Stations.js';
import odptTransport from './odptTransport.js';
import fullTransport from './fullTransport.js';
import osmTransportSubways from './osmTransportSubways.js';
import { mergeFeatureCollections, dedupeByKeys, countBySource } from './_dedupe.js';
import { computeLineColor } from './_lineColor.js';

// Always recompute line_color so stale values from older hash algorithms
// get overwritten. No identity → null → layer default color.
function ensureLineColor(feature) {
  const color = computeLineColor(feature.properties) || null;
  return { ...feature, properties: { ...feature.properties, line_color: color } };
}

const SUBWAY_LIKE = new Set(['subway', 'metro', 'underground', 'light_rail', 'monorail', 'tram_stop', 'tram']);
const SUBWAY_LINE_HINTS = [
  'Tokyo Metro', 'Toei', 'Osaka Metro', 'Nagoya', 'Sapporo', 'Sendai',
  'Fukuoka', 'Kyoto', 'Kobe', 'Yokohama', 'Monorail',
];
const SUBWAY_JA_HINTS = ['メトロ', '都営', '市営', 'モノレール'];

function isSubwayFeature(f) {
  const t = (f.properties?.type || f.properties?.classification || '').toLowerCase();
  if (SUBWAY_LIKE.has(t)) return true;
  const line = (f.properties?.line || f.properties?.line_name || '').toString();
  if (SUBWAY_LINE_HINTS.some(h => line.includes(h))) return true;
  if (SUBWAY_JA_HINTS.some(h => line.includes(h))) return true;
  const op = (f.properties?.operator || '').toString();
  if (SUBWAY_LINE_HINTS.some(h => op.includes(h))) return true;
  if (SUBWAY_JA_HINTS.some(h => op.includes(h))) return true;
  return false;
}

export default async function collectUnifiedSubways() {
  const [n02, odpt, full, osm] = await Promise.allSettled([
    mlitN02Stations(),
    odptTransport(),
    fullTransport(),
    osmTransportSubways(),
  ]);

  const raw = mergeFeatureCollections([
    n02.status === 'fulfilled' ? n02.value : null,
    odpt.status === 'fulfilled' ? odpt.value : null,
    full.status === 'fulfilled' ? full.value : null,
    osm.status === 'fulfilled' ? osm.value : null,
  ]).filter(isSubwayFeature);

  const features = dedupeByKeys(raw, [
    (f) => f.properties?.station_code || null,
    (f) => {
      const sid = f.properties?.station_id;
      if (!sid) return null;
      return String(sid).includes(':') ? sid : null;
    },
  ]).map(ensureLineColor);

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'unified_subways',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      upstream: {
        'mlit-n02-stations': n02.status === 'fulfilled' ? (n02.value.features?.length || 0) : 0,
        'odpt-transport': odpt.status === 'fulfilled' ? (odpt.value.features?.length || 0) : 0,
        'full-transport': full.status === 'fulfilled' ? (full.value.features?.length || 0) : 0,
        'osm-transport-subways': osm.status === 'fulfilled' ? (osm.value.features?.length || 0) : 0,
      },
      bySource: countBySource(features),
      description: 'Deduplicated subway / metro / monorail / tram stops - fused ODPT + OSM transport + MLIT N02',
    },
    metadata: {},
  };
}
