/**
 * Unified Train Stations — fuses and deduplicates every rail source:
 *   - MLIT N02 (authoritative station geometry)
 *   - ODPT (live, JR East + Tokyo Metro + Toei + challenge operators)
 *   - osmTransportTrains (always-on OSM transport layer for mainline rail)
 *
 * Excludes subway/tram/monorail — those feed unifiedSubways instead.
 */

import mlitN02Stations from './mlitN02Stations.js';
import odptTransport from './odptTransport.js';
import osmTransportTrains from './osmTransportTrains.js';
import { computeLineColor } from './_lineColor.js';
import { createUnifiedCollector } from '../utils/unifiedCollectorTemplate.js';

const SUBWAY_TYPES = new Set(['subway', 'metro', 'underground']);
const EXCLUDE_TYPES = new Set(['tram_stop', 'tram', 'monorail', 'light_rail']);

function isTrainFeature(f) {
  const t = (f.properties?.type || f.properties?.classification || '').toLowerCase();
  if (SUBWAY_TYPES.has(t)) return false;
  if (EXCLUDE_TYPES.has(t)) return false;
  return true;
}

// Always recompute line_color from the current canonical identity so stale
// values from older hash algorithms get overwritten. A feature with no line
// identity ends up with line_color: null and renders in the layer default.
function ensureLineColor(feature) {
  const color = computeLineColor(feature.properties) || null;
  return { ...feature, properties: { ...feature.properties, line_color: color } };
}

export default createUnifiedCollector({
  sourceId: 'unified_trains',
  description: 'Deduplicated nationwide train stations - merges MLIT N02 + ODPT + OSM transport layer',
  upstreams: [
    { name: 'mlit-n02-stations',     fn: mlitN02Stations },
    { name: 'odpt-transport',        fn: odptTransport },
    { name: 'osm-transport-trains',  fn: osmTransportTrains },
  ],
  filter: isTrainFeature,
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
