/**
 * Unified AIS Ship Tracking - fuses live vessel positions from:
 *   - maritimeAis.js  (aggregated MT + VF + OSM + seed)
 *   - marineTraffic.js (dedicated MarineTraffic feed)
 *   - vesselFinder.js (dedicated VesselFinder feed)
 *
 * Dedup key priority:
 *   1) MMSI (authoritative AIS identifier)
 *   2) IMO number
 *   3) vessel_name + coord-grid (seed/fallback data)
 *
 * Note: static port/harbour geometry lives in the port infrastructure layer
 * (unifiedPortInfra) - do not merge it into the vessel feed.
 */

import maritimeAis from './maritimeAis.js';
import marineTraffic from './marineTraffic.js';
import vesselFinder from './vesselFinder.js';
import { mergeFeatureCollections, countBySource, normName } from './_dedupe.js';

function parseTs(v) {
  if (!v) return 0;
  const t = Date.parse(v);
  return Number.isFinite(t) ? t : 0;
}

function fuseVessels(features) {
  const seen = new Map();
  const orderedKeys = [];

  for (const f of features) {
    const p = f.properties || {};
    const mmsi = p.mmsi || p.MMSI || null;
    const imo = p.imo || p.IMO || null;
    let key = null;
    if (mmsi) key = `mmsi:${String(mmsi).trim()}`;
    else if (imo) key = `imo:${String(imo).trim()}`;
    else {
      const [lon, lat] = f.geometry?.coordinates || [];
      if (!Number.isFinite(lon) || !Number.isFinite(lat)) continue;
      const name = normName(p.vessel_name || p.name);
      key = `nc:${name}:${lon.toFixed(3)},${lat.toFixed(3)}`;
    }

    if (!seen.has(key)) {
      seen.set(key, {
        type: 'Feature',
        geometry: f.geometry,
        properties: {
          ...p,
          sources: [p.source].filter(Boolean),
          _freshness: parseTs(p.last_position_update),
        },
      });
      orderedKeys.push(key);
      continue;
    }

    const existing = seen.get(key);
    const existingTs = existing.properties._freshness;
    const candTs = parseTs(p.last_position_update);

    let winner, loser;
    if (candTs >= existingTs) {
      winner = { type: 'Feature', geometry: f.geometry, properties: { ...p } };
      loser = existing;
    } else {
      winner = existing;
      loser = { type: 'Feature', geometry: f.geometry, properties: { ...p } };
    }

    const merged = { ...winner.properties };
    for (const [k, v] of Object.entries(loser.properties || {})) {
      if (merged[k] == null && v != null) merged[k] = v;
    }
    const sources = new Set(existing.properties.sources || []);
    if (p.source) sources.add(p.source);
    if (winner.properties.source) sources.add(winner.properties.source);
    merged.sources = Array.from(sources);
    merged._freshness = Math.max(existingTs, candTs);

    seen.set(key, {
      type: 'Feature',
      geometry: winner.geometry,
      properties: merged,
    });
  }

  return orderedKeys.map(k => {
    const f = seen.get(k);
    const { _freshness, ...rest } = f.properties;
    return { ...f, properties: rest };
  });
}

export default async function collectUnifiedAisShips() {
  const [a, b, c] = await Promise.allSettled([
    marineTraffic(),
    vesselFinder(),
    maritimeAis(),
  ]);

  const raw = mergeFeatureCollections([
    a.status === 'fulfilled' ? a.value : null,
    b.status === 'fulfilled' ? b.value : null,
    c.status === 'fulfilled' ? c.value : null,
  ]);

  const features = fuseVessels(raw);

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'unified_ais_ships',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      upstream: {
        'marine-traffic': a.status === 'fulfilled' ? (a.value.features?.length || 0) : 0,
        'vessel-finder': b.status === 'fulfilled' ? (b.value.features?.length || 0) : 0,
        'maritime-ais': c.status === 'fulfilled' ? (c.value.features?.length || 0) : 0,
      },
      bySource: countBySource(features),
      dedup_strategy: 'mmsi > imo > name+coord; freshest last_position_update wins, fields merged',
      description: 'Deduplicated AIS vessel positions - fuses MarineTraffic + VesselFinder + maritimeAis (vessels only; ports live in unifiedPortInfra)',
    },
    metadata: {},
  };
}
