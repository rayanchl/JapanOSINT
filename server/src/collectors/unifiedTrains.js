/**
 * Unified Train Stations — fuses and deduplicates every rail source:
 *   - MLIT N02 (authoritative station geometry)
 *   - ODPT (live, JR East + Tokyo Metro + Toei + challenge operators)
 *   - fullTransport (OSM railway=station + curated Shinkansen seed)
 *   - osmTransportTrains (always-on OSM transport layer for mainline rail)
 *
 * Excludes subway/tram/monorail — those feed unifiedSubways instead.
 */

import mlitN02Stations from './mlitN02Stations.js';
import odptTransport from './odptTransport.js';
import fullTransport from './fullTransport.js';
import osmTransportTrains from './osmTransportTrains.js';
import { mergeFeatureCollections, dedupeByKeys, countBySource } from './_dedupe.js';
import { computeLineColor } from './_lineColor.js';

// Backfill line_color on any feature whose upstream source didn't already
// stamp one — matches the hash used by track collectors so stations line
// up with their tracks.
function ensureLineColor(feature) {
  if (feature.properties?.line_color) return feature;
  const color = computeLineColor(feature.properties);
  if (!color) return feature;
  return { ...feature, properties: { ...feature.properties, line_color: color } };
}

const SUBWAY_TYPES = new Set(['subway', 'metro', 'underground']);
const EXCLUDE_TYPES = new Set(['tram_stop', 'tram', 'monorail', 'light_rail']);

function isTrainFeature(f) {
  const t = (f.properties?.type || f.properties?.classification || '').toLowerCase();
  if (SUBWAY_TYPES.has(t)) return false;
  if (EXCLUDE_TYPES.has(t)) return false;
  return true;
}

export default async function collectUnifiedTrains() {
  const [n02, odpt, full, osm] = await Promise.allSettled([
    mlitN02Stations(),
    odptTransport(),
    fullTransport(),
    osmTransportTrains(),
  ]);

  const raw = mergeFeatureCollections([
    n02.status === 'fulfilled' ? n02.value : null,
    odpt.status === 'fulfilled' ? odpt.value : null,
    full.status === 'fulfilled' ? full.value : null,
    osm.status === 'fulfilled' ? osm.value : null,
  ]).filter(isTrainFeature);

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
      source: 'unified_trains',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      upstream: {
        'mlit-n02-stations': n02.status === 'fulfilled' ? (n02.value.features?.length || 0) : 0,
        'odpt-transport': odpt.status === 'fulfilled' ? (odpt.value.features?.length || 0) : 0,
        'full-transport': full.status === 'fulfilled' ? (full.value.features?.length || 0) : 0,
        'osm-transport-trains': osm.status === 'fulfilled' ? (osm.value.features?.length || 0) : 0,
      },
      bySource: countBySource(features),
      description: 'Deduplicated nationwide train stations - merges MLIT N02 + ODPT + OSM transport layer',
    },
    metadata: {},
  };
}
