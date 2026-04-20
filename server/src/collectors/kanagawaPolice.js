/**
 * Kanagawa Prefectural Police — open data portal (crime / traffic stats).
 *
 * https://www.police.pref.kanagawa.jp/ publishes an open-data section
 * with CSVs of crime counts by ward and year. The exact CSV URL rotates
 * yearly and is not listed in a machine-readable index, so we ship a
 * seed of recent figures pinned at ward centroids. When a current CSV
 * URL is configured via KANAGAWA_POLICE_CSV_URL, the collector switches
 * to live.
 *
 * No auth. Annual cadence.
 */

const TIMEOUT_MS = 15000;

// Kanagawa ward centroids (rough lat/lon for rendering).
const KANAGAWA_WARDS = [
  { code: 'yokohama-tsurumi',   name: 'Yokohama · Tsurumi',   name_ja: '横浜市鶴見区', lat: 35.5085, lon: 139.6764 },
  { code: 'yokohama-kanagawa',  name: 'Yokohama · Kanagawa',  name_ja: '横浜市神奈川区', lat: 35.4768, lon: 139.6352 },
  { code: 'yokohama-nishi',     name: 'Yokohama · Nishi',     name_ja: '横浜市西区',   lat: 35.4672, lon: 139.6206 },
  { code: 'yokohama-naka',      name: 'Yokohama · Naka',      name_ja: '横浜市中区',   lat: 35.4425, lon: 139.6493 },
  { code: 'yokohama-minami',    name: 'Yokohama · Minami',    name_ja: '横浜市南区',   lat: 35.4271, lon: 139.6207 },
  { code: 'yokohama-hodogaya',  name: 'Yokohama · Hodogaya',  name_ja: '横浜市保土ケ谷区', lat: 35.4515, lon: 139.5972 },
  { code: 'kawasaki-kawasaki',  name: 'Kawasaki · Kawasaki',  name_ja: '川崎市川崎区', lat: 35.5321, lon: 139.7028 },
  { code: 'kawasaki-saiwai',    name: 'Kawasaki · Saiwai',    name_ja: '川崎市幸区',   lat: 35.5376, lon: 139.6788 },
  { code: 'kawasaki-nakahara',  name: 'Kawasaki · Nakahara',  name_ja: '川崎市中原区', lat: 35.5764, lon: 139.6604 },
  { code: 'kawasaki-miyamae',   name: 'Kawasaki · Miyamae',   name_ja: '川崎市宮前区', lat: 35.5920, lon: 139.5836 },
  { code: 'sagamihara-midori',  name: 'Sagamihara · Midori',  name_ja: '相模原市緑区', lat: 35.6122, lon: 139.1486 },
  { code: 'sagamihara-chuo',    name: 'Sagamihara · Chuo',    name_ja: '相模原市中央区', lat: 35.5714, lon: 139.3730 },
  { code: 'fujisawa',           name: 'Fujisawa',             name_ja: '藤沢市',       lat: 35.3382, lon: 139.4902 },
  { code: 'yokosuka',           name: 'Yokosuka',             name_ja: '横須賀市',     lat: 35.2815, lon: 139.6721 },
  { code: 'hiratsuka',          name: 'Hiratsuka',            name_ja: '平塚市',       lat: 35.3266, lon: 139.3500 },
  { code: 'odawara',            name: 'Odawara',              name_ja: '小田原市',     lat: 35.2562, lon: 139.1595 },
];

// Placeholder crime-count seed (illustrative, not authoritative).
// Replace via CSV import once the live URL is confirmed.
const SEED_CRIME_COUNTS = {
  'yokohama-tsurumi': 1440, 'yokohama-kanagawa': 1310, 'yokohama-nishi': 1050,
  'yokohama-naka': 1520, 'yokohama-minami': 1180, 'yokohama-hodogaya': 990,
  'kawasaki-kawasaki': 1830, 'kawasaki-saiwai': 820, 'kawasaki-nakahara': 980,
  'kawasaki-miyamae': 740, 'sagamihara-midori': 580, 'sagamihara-chuo': 1060,
  'fujisawa': 1420, 'yokosuka': 980, 'hiratsuka': 860, 'odawara': 640,
};

async function tryLiveCsv() {
  const url = process.env.KANAGAWA_POLICE_CSV_URL;
  if (!url) return null;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(url, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) return null;
    const text = await res.text();
    // Lightweight CSV: `ward_code,count`
    const out = {};
    for (const line of text.trim().split(/\r?\n/).slice(1)) {
      const [code, n] = line.split(',');
      const v = parseInt(n, 10);
      if (code && Number.isFinite(v)) out[code.trim()] = v;
    }
    return Object.keys(out).length ? out : null;
  } catch {
    return null;
  }
}

export default async function collectKanagawaPolice() {
  const live = await tryLiveCsv();
  const counts = live || SEED_CRIME_COUNTS;
  const liveSource = live ? 'kanagawa_police_csv' : 'kanagawa_police_seed';

  const features = KANAGAWA_WARDS.map((w) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [w.lon, w.lat] },
    properties: {
      id: `KANAGAWA_${w.code}`,
      ward_code: w.code,
      ward: w.name,
      ward_ja: w.name_ja,
      crime_count: counts[w.code] ?? null,
      period: 'annual',
      source: liveSource,
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSource,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      env_hint: 'Set KANAGAWA_POLICE_CSV_URL to a CSV of ward_code,count to enable live data',
      description: 'Kanagawa Prefectural Police ward-level crime counts (seed or live CSV)',
    },
    metadata: {},
  };
}
