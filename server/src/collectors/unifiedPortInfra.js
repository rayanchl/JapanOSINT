/**
 * Unified Port Infrastructure - fuses and deduplicates port/harbour sources:
 *   - portInfra.js      (OSM harbours + curated strategic/important/fishing ports + ferry terminals)
 *   - osmTransportPorts (always-on OSM transport layer: harbours, ferry terminals, marinas)
 *   - mlitC02Ports      (MLIT KSJ C02 authoritative designated ports)
 *
 * Dedup key priority:
 *   1) port_id (if globally qualified, e.g. MLIT_C02_*, OSM_*, PORT_*)
 *   2) name + coord-grid (4 decimals ~= 11m)
 *
 * Fused output preserves classification, cargo/catch tonnage, ferry routes,
 * and prefecture metadata from whichever source provided them.
 */

import portInfra from './portInfra.js';
import osmTransportPorts from './osmTransportPorts.js';
import mlitC02Ports from './mlitC02Ports.js';
import { mergeFeatureCollections, dedupeByKeys, countBySource } from './_dedupe.js';

export default async function collectUnifiedPortInfra() {
  const [pInfra, osm, c02] = await Promise.allSettled([
    portInfra(),
    osmTransportPorts(),
    mlitC02Ports(),
  ]);

  const raw = mergeFeatureCollections([
    pInfra.status === 'fulfilled' ? pInfra.value : null,
    osm.status === 'fulfilled' ? osm.value : null,
    c02.status === 'fulfilled' ? c02.value : null,
  ]);

  const features = dedupeByKeys(raw, [
    (f) => {
      const id = f.properties?.port_id;
      if (!id) return null;
      const s = String(id);
      // Only qualified IDs are globally unique
      if (s.startsWith('MLIT_C02_') || s.startsWith('OSM_') || s.startsWith('PORT_')) return s;
      return null;
    },
  ], { coordPrecision: 4 });

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'unified_port_infra',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      upstream: {
        'port-infra': pInfra.status === 'fulfilled' ? (pInfra.value.features?.length || 0) : 0,
        'osm-transport-ports': osm.status === 'fulfilled' ? (osm.value.features?.length || 0) : 0,
        'mlit-c02-ports': c02.status === 'fulfilled' ? (c02.value.features?.length || 0) : 0,
      },
      bySource: countBySource(features),
      description: 'Deduplicated Japan port infrastructure - fuses curated PortInfra + OSM transport ports + MLIT C02 designated ports (ferry terminals, harbours, marinas, fishing ports)',
    },
    metadata: {},
  };
}
