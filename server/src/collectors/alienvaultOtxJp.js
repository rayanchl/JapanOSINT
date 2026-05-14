/**
 * AlienVault OTX (LevelBlue) — threat-intel pulses targeting Japan.
 *
 * Auth: OTX_API_KEY (free, https://otx.alienvault.com/api). The DirectConnect
 * search endpoint accepts free-text queries; we run "japan" and surface the
 * 50 most recent pulses with their indicator counts and adversary tags.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL = 'https://otx.alienvault.com/api/v1/search/pulses?q=japan&limit=50&sort=-modified';
const TIMEOUT_MS = 15000;

export default createThreatIntelCollector({
  sourceId: 'otx_search_pulses',
  description: 'AlienVault OTX — recent pulses matching "japan"',
  envKey: 'OTX_API_KEY',
  envHint: 'Set OTX_API_KEY (free at https://otx.alienvault.com/api)',
  run: async (key) => {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL, {
      signal: ctrl.signal,
      headers: { 'X-OTX-API-KEY': key, accept: 'application/json' },
    });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const pulses = Array.isArray(json?.results) ? json.results : [];
    const features = pulses.map((p, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        pulse_id: p.id,
        title: p.name,
        author: p?.author?.username,
        modified: p.modified,
        created: p.created,
        tlp: p.tlp,
        adversary: p.adversary,
        industries: p.industries,
        targeted_countries: p.targeted_countries,
        indicator_count: p.indicator_count,
        tags: p.tags,
        url: p.id ? `https://otx.alienvault.com/pulse/${p.id}` : null,
        source: 'otx_search_pulses',
      },
    }));
    return { features, extraMeta: { query: 'japan' } };
  },
});
