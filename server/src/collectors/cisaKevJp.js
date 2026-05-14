/**
 * CISA Known Exploited Vulnerabilities (KEV) — Japan-vendor intersection.
 *
 * CISA publishes the canonical list of CVEs *known to be exploited in the
 * wild*. We download the full JSON and keep entries whose vendorProject /
 * product matches a JP-vendor allowlist.
 *
 * Endpoint: https://www.cisa.gov/sites/default/files/feeds/known_exploited_vulnerabilities.json
 * Free, no auth.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const KEV_URL = 'https://www.cisa.gov/sites/default/files/feeds/known_exploited_vulnerabilities.json';
const TIMEOUT_MS = 15000;

const JP_VENDORS = [
  'trend micro', 'trendmicro',
  'fujitsu', 'nec', 'hitachi', 'mitsubishi', 'toshiba', 'panasonic',
  'sony', 'canon', 'ricoh', 'kyocera', 'sharp', 'olympus',
  'yokogawa', 'omron', 'denso', 'fanuc',
  'cybozu', 'rakuten', 'line', 'softbank', 'kddi', 'ntt',
  'buffalo', 'yamaha', 'i-o data', 'io-data', 'iodata', 'elecom',
  'corega', 'planex', 'logitec',
  'justsystem', 'justsystems', 'ichitaro',
  'baidu-jp', 'r-soft', 'sannet',
  'silex', 'allied telesis', 'atworks',
  'movabletype', 'sixapart',
  'a10 networks', 'a10networks',
];

// Coarse vendor → HQ-city mapping for nicer scatter on the map.
const VENDOR_GEO = {
  toshiba: [139.7595, 35.6627],
  fujitsu: [139.7460, 35.6810],
  nec: [139.7568, 35.6594],
  hitachi: [139.7648, 35.6824],
  mitsubishi: [139.7649, 35.6810],
  panasonic: [135.5023, 34.7250],
  sony: [139.7409, 35.6310],
  canon: [139.6203, 35.6713],
  ricoh: [139.7457, 35.6328],
  kyocera: [135.7681, 34.9850],
  sharp: [135.5161, 34.6515],
  yokogawa: [139.5953, 35.6796],
  omron: [135.7493, 34.9926],
  denso: [137.0167, 35.0467],
  fanuc: [138.7625, 35.4700],
  cybozu: [139.6961, 35.6907],
  rakuten: [139.6303, 35.6360],
  line: [139.6993, 35.6955],
  softbank: [139.7530, 35.6606],
  kddi: [139.7367, 35.6679],
  ntt: [139.7438, 35.6831],
  buffalo: [136.9066, 35.1815],
  yamaha: [137.7261, 34.7034],
  iodata: [136.6256, 36.5947],
  elecom: [135.5114, 34.6863],
  trendmicro: [139.7517, 35.6805],
  justsystems: [134.5538, 34.0666],
  movabletype: [139.7129, 35.6593],
};

function vendorGeo(vendorRaw) {
  const v = String(vendorRaw || '').toLowerCase().replace(/[^a-z0-9]/g, '');
  for (const [k, ll] of Object.entries(VENDOR_GEO)) {
    if (v.includes(k)) return ll;
  }
  return TOKYO;
}

function vendorMatches(v, p) {
  const blob = `${v} ${p}`.toLowerCase();
  return JP_VENDORS.some((needle) => blob.includes(needle));
}

export default createThreatIntelCollector({
  sourceId: 'cisa_kev',
  description: 'CISA KEV catalogue — Japan-vendor intersection (known-exploited CVEs)',
  // No auth required.
  run: async () => {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(KEV_URL, {
      signal: ctrl.signal,
      headers: { accept: 'application/json' },
    });
    clearTimeout(timer);
    let entries = [];
    let live = false;
    if (res.ok) {
      const json = await res.json();
      entries = Array.isArray(json?.vulnerabilities) ? json.vulnerabilities : [];
      live = entries.length > 0;
    }

    const filtered = entries.filter((v) => vendorMatches(v.vendorProject, v.product));
    const features = filtered.map((v, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: vendorGeo(v.vendorProject) },
      properties: {
        idx: i,
        cve_id: v.cveID || null,
        vendor: v.vendorProject || null,
        product: v.product || null,
        title: v.vulnerabilityName || null,
        added: v.dateAdded || null,
        due: v.dueDate || null,
        ransomware: v.knownRansomwareCampaignUse === 'Known' || false,
        description: (v.shortDescription || '').slice(0, 400),
        required_action: v.requiredAction || null,
        cwes: Array.isArray(v.cwes) ? v.cwes : [],
        notes: v.notes || null,
        source: live ? 'cisa_kev' : 'cisa_kev_seed',
      },
    }));

    return {
      features,
      source: live ? 'live' : 'seed',
      extraMeta: {
        total_kev: entries.length,
        jp_match_count: filtered.length,
      },
    };
  },
});
