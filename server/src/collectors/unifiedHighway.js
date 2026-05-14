/**
 * Unified Expressway / Road-traffic layer — fuses two upstreams that cover
 * the same road-network domain:
 *   - highwayTraffic.js  (NEXCO/Shuto/Hanshin IC/JCT/SA/PA point inventory)
 *   - jarticTraffic.js   (JARTIC live congestion hotspots)
 *
 * Each feature is tagged with `kind: 'node' | 'congestion'` so the renderer
 * can switch styling (interchange dot vs congestion segment).
 */

import highwayTraffic from './highwayTraffic.js';
import jarticTraffic from './jarticTraffic.js';
import { createUnifiedCollector } from '../utils/unifiedCollectorTemplate.js';

export default createUnifiedCollector({
  sourceId: 'unified_highway',
  description: 'Fused expressway nodes (IC/JCT/SA/PA) + JARTIC congestion hotspots',
  upstreams: [
    { name: 'highway-traffic', fn: highwayTraffic, kind: 'node' },
    { name: 'jartic-traffic',  fn: jarticTraffic,  kind: 'congestion' },
  ],
  // No dedupe — node and congestion features are disjoint geometries.
});
