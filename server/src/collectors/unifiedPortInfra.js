/**
 * Unified Port Infrastructure - fuses and deduplicates port/harbour sources:
 *   - portInfra.js      (OSM harbours + curated strategic/important/fishing ports + ferry terminals)
 *   - osmTransportPorts (always-on OSM transport layer: harbours, ferry terminals, marinas)
 *   - mlitC02Ports      (MLIT KSJ C02 authoritative designated ports)
 *
 * Dedup key priority:
 *   1) port_id (if globally qualified, e.g. MLIT_C02_*, OSM_*, PORT_*)
 *   2) name + coord-grid (4 decimals ~= 11m)
 */

import portInfra from './portInfra.js';
import osmTransportPorts from './osmTransportPorts.js';
import mlitC02Ports from './mlitC02Ports.js';
import { createUnifiedCollector } from '../utils/unifiedCollectorTemplate.js';

export default createUnifiedCollector({
  sourceId: 'unified_port_infra',
  description: 'Deduplicated Japan port infrastructure - fuses curated PortInfra + OSM transport ports + MLIT C02 designated ports (ferry terminals, harbours, marinas, fishing ports)',
  upstreams: [
    { name: 'port-infra',          fn: portInfra },
    { name: 'osm-transport-ports', fn: osmTransportPorts },
    { name: 'mlit-c02-ports',      fn: mlitC02Ports },
  ],
  dedupeKeys: [
    (f) => {
      const id = f.properties?.port_id;
      if (!id) return null;
      const s = String(id);
      // Only qualified IDs are globally unique
      if (s.startsWith('MLIT_C02_') || s.startsWith('OSM_') || s.startsWith('PORT_')) return s;
      return null;
    },
  ],
  dedupeOpts: { coordPrecision: 4 },
});
