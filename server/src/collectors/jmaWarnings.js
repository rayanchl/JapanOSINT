/**
 * JMA weather warnings / advisories — jma-warnings.
 *
 * Real: JMA bosai warning JSON, one file per prefecture area code:
 *   https://www.jma.go.jp/bosai/warning/data/warning/{areaCode}.json
 * Analogous to jmaForecastArea.js. We iterate the prefecture office codes,
 * emit one point per area that currently has at least one active warning
 * (status != type "解除"/empty). No fabrication: on failure → honest empty.
 */

// Prefecture forecast-office codes with a representative office coordinate.
const AREAS = [
  { code: '016000', name: '北海道（石狩・空知・後志）', lat: 43.06, lon: 141.35 },
  { code: '020000', name: '青森県', lat: 40.82, lon: 140.74 },
  { code: '030000', name: '岩手県', lat: 39.70, lon: 141.15 },
  { code: '040000', name: '宮城県', lat: 38.27, lon: 140.87 },
  { code: '050000', name: '秋田県', lat: 39.72, lon: 140.10 },
  { code: '060000', name: '山形県', lat: 38.24, lon: 140.36 },
  { code: '070000', name: '福島県', lat: 37.75, lon: 140.47 },
  { code: '080000', name: '茨城県', lat: 36.37, lon: 140.47 },
  { code: '090000', name: '栃木県', lat: 36.57, lon: 139.88 },
  { code: '100000', name: '群馬県', lat: 36.39, lon: 139.06 },
  { code: '110000', name: '埼玉県', lat: 35.86, lon: 139.65 },
  { code: '120000', name: '千葉県', lat: 35.61, lon: 140.12 },
  { code: '130000', name: '東京都', lat: 35.69, lon: 139.69 },
  { code: '140000', name: '神奈川県', lat: 35.45, lon: 139.64 },
  { code: '150000', name: '新潟県', lat: 37.90, lon: 139.02 },
  { code: '160000', name: '富山県', lat: 36.70, lon: 137.21 },
  { code: '170000', name: '石川県', lat: 36.59, lon: 136.63 },
  { code: '180000', name: '福井県', lat: 36.07, lon: 136.22 },
  { code: '190000', name: '山梨県', lat: 35.66, lon: 138.57 },
  { code: '200000', name: '長野県', lat: 36.65, lon: 138.18 },
  { code: '210000', name: '岐阜県', lat: 35.39, lon: 136.72 },
  { code: '220000', name: '静岡県', lat: 34.98, lon: 138.38 },
  { code: '230000', name: '愛知県', lat: 35.18, lon: 136.91 },
  { code: '240000', name: '三重県', lat: 34.73, lon: 136.51 },
  { code: '250000', name: '滋賀県', lat: 35.00, lon: 135.87 },
  { code: '260000', name: '京都府', lat: 35.02, lon: 135.76 },
  { code: '270000', name: '大阪府', lat: 34.69, lon: 135.52 },
  { code: '280000', name: '兵庫県', lat: 34.69, lon: 135.18 },
  { code: '290000', name: '奈良県', lat: 34.69, lon: 135.83 },
  { code: '300000', name: '和歌山県', lat: 34.23, lon: 135.17 },
  { code: '310000', name: '鳥取県', lat: 35.50, lon: 134.24 },
  { code: '320000', name: '島根県', lat: 35.47, lon: 133.05 },
  { code: '330000', name: '岡山県', lat: 34.66, lon: 133.93 },
  { code: '340000', name: '広島県', lat: 34.40, lon: 132.46 },
  { code: '350000', name: '山口県', lat: 34.19, lon: 131.47 },
  { code: '360000', name: '徳島県', lat: 34.07, lon: 134.56 },
  { code: '370000', name: '香川県', lat: 34.34, lon: 134.04 },
  { code: '380000', name: '愛媛県', lat: 33.84, lon: 132.77 },
  { code: '390000', name: '高知県', lat: 33.56, lon: 133.53 },
  { code: '400000', name: '福岡県', lat: 33.59, lon: 130.40 },
  { code: '410000', name: '佐賀県', lat: 33.25, lon: 130.30 },
  { code: '420000', name: '長崎県', lat: 32.74, lon: 129.87 },
  { code: '430000', name: '熊本県', lat: 32.80, lon: 130.73 },
  { code: '440000', name: '大分県', lat: 33.24, lon: 131.61 },
  { code: '450000', name: '宮崎県', lat: 31.91, lon: 131.42 },
  { code: '460100', name: '鹿児島県', lat: 31.56, lon: 130.56 },
  { code: '471000', name: '沖縄県（本島中南部）', lat: 26.21, lon: 127.68 },
];
const TIMEOUT_MS = 8000;

async function fetchOne(area) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(
      `https://www.jma.go.jp/bosai/warning/data/warning/${area.code}.json`,
      { signal: ctrl.signal },
    );
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    return { area, data };
  } catch {
    return null;
  }
}

// Collect active warning type names from a bosai warning document.
function extractActiveWarnings(doc) {
  const out = [];
  const areaTypes = doc?.areaTypes;
  if (!Array.isArray(areaTypes)) return out;
  for (const at of areaTypes) {
    for (const a of at.areas ?? []) {
      for (const w of a.warnings ?? []) {
        // status of "発表" / "継続" etc. means active; "解除" means cleared.
        if (w.status && w.status !== '解除' && w.code && w.code !== '00') {
          out.push(w.name || w.code);
        }
      }
    }
  }
  return Array.from(new Set(out));
}

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'JMA active weather warnings / advisories per prefecture',
    },
  };
}

export default async function collectJmaWarnings() {
  let results;
  try {
    results = await Promise.all(AREAS.map(fetchOne));
  } catch {
    return emptyResult('jma_warnings_unavailable');
  }

  const features = [];
  let anyFetched = false;
  for (const r of results) {
    if (!r || !r.data) continue;
    anyFetched = true;
    const warnings = extractActiveWarnings(r.data);
    if (warnings.length === 0) continue;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [r.area.lon, r.area.lat] },
      properties: {
        area_code: r.area.code,
        area_name: r.area.name,
        warnings,
        warning_count: warnings.length,
        report_at: r.data?.reportDatetime ?? null,
        source: 'jma_warning',
      },
    });
  }

  if (!anyFetched) return emptyResult('jma_warnings_unavailable');

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'jma_warning',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'JMA active weather warnings / advisories per prefecture',
    },
  };
}
