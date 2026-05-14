/**
 * Unified Subway Stations - fuses and deduplicates subway / metro / monorail / tram / light_rail stops:
 *   - MLIT N02 (authoritative station geometry, filtered by type/operator)
 *   - ODPT subway operators (Tokyo Metro, Toei, Osaka Metro, etc.)
 *   - osmTransportSubways (always-on dedicated OSM transport layer)
 */

import mlitN02Stations from './mlitN02Stations.js';
import odptTransport from './odptTransport.js';
import osmTransportSubways from './osmTransportSubways.js';
import { computeLineColor } from './_lineColor.js';
import { createUnifiedCollector } from '../utils/unifiedCollectorTemplate.js';

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

// Always recompute line_color so stale values from older hash algorithms get
// overwritten. No identity → null → layer default color.
function ensureLineColor(feature) {
  const color = computeLineColor(feature.properties) || null;
  return { ...feature, properties: { ...feature.properties, line_color: color } };
}

export default createUnifiedCollector({
  sourceId: 'unified_subways',
  description: 'Deduplicated subway / metro / monorail / tram stops - fused ODPT + OSM transport + MLIT N02',
  upstreams: [
    { name: 'mlit-n02-stations',     fn: mlitN02Stations },
    { name: 'odpt-transport',        fn: odptTransport },
    { name: 'osm-transport-subways', fn: osmTransportSubways },
  ],
  filter: isSubwayFeature,
  dedupeKeys: [
    (f) => f.properties?.station_code || null,
    (f) => {
      const sid = f.properties?.station_id;
      if (!sid) return null;
      return String(sid).includes(':') ? sid : null;
    },
  ],
  postProcess: ensureLineColor,
});
