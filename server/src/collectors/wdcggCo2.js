/**
 * WDCGG / CGER — CO2 & GHG monitoring stations.
 *
 * World Data Centre for Greenhouse Gases (WMO GAW) publishes station
 * metadata at https://gaw.kishou.go.jp/ (Tokyo-hosted). Some endpoints
 * require free account registration for bulk time-series; the station
 * registry CSV is publicly accessible.
 *
 * Because the exact CSV URL changes and is fetched via a JS-rendered
 * page, we carry a curated seed of the major Japan-region stations
 * so the collector returns meaningful data without registration.
 * When a machine-readable registry URL is found, drop it into
 * WDCGG_REGISTRY_URL to upgrade live.
 *
 * No auth for the seed path. The live pull is S-effort follow-up.
 */

const WDCGG_REGISTRY_URL = process.env.WDCGG_REGISTRY_URL || null;
const TIMEOUT_MS = 15000;

// Curated subset of GAW stations most relevant to Japan: all JP stations
// plus near-neighbour stations that measure air masses transiting Japan.
const SEED_STATIONS = [
  { code: 'MNM', name: 'Minamitorishima',    name_ja: '南鳥島',    country: 'JP', lat: 24.2883, lon: 153.9833, elev_m: 7,    species: ['CO2','CH4','N2O','SF6'], operator: 'JMA' },
  { code: 'RYO', name: 'Ryori',              name_ja: '綾里',       country: 'JP', lat: 39.0311, lon: 141.8228, elev_m: 260,  species: ['CO2','CH4','N2O','SF6'], operator: 'JMA' },
  { code: 'YON', name: 'Yonagunijima',       name_ja: '与那国島',   country: 'JP', lat: 24.4667, lon: 123.0111, elev_m: 30,   species: ['CO2','CH4','N2O','SF6'], operator: 'JMA' },
  { code: 'HAT', name: 'Hateruma',           name_ja: '波照間',     country: 'JP', lat: 24.0575, lon: 123.8075, elev_m: 10,   species: ['CO2','CH4','N2O','SF6','HFCs'], operator: 'NIES' },
  { code: 'COI', name: 'Cape Ochiishi',      name_ja: '落石岬',     country: 'JP', lat: 43.1556, lon: 145.5042, elev_m: 49,   species: ['CO2','CH4'], operator: 'NIES' },
  { code: 'RIS', name: 'Rishiri',            name_ja: '利尻',       country: 'JP', lat: 45.1211, lon: 141.2089, elev_m: 40,   species: ['CO2','CH4'], operator: 'NIES' },
  { code: 'TAP', name: 'Tae-ahn Peninsula',  name_ja: '泰安半島',   country: 'KR', lat: 36.7378, lon: 126.1325, elev_m: 20,   species: ['CO2','CH4'], operator: 'KMA' },
  { code: 'AMY', name: 'Anmyeon-do',         name_ja: '安眠島',     country: 'KR', lat: 36.5382, lon: 126.3302, elev_m: 46,   species: ['CO2','CH4','N2O','SF6'], operator: 'KMA' },
  { code: 'LLN', name: 'Lulin',              name_ja: '鹿林',       country: 'TW', lat: 23.4688, lon: 120.8736, elev_m: 2867, species: ['CO2','CH4'], operator: 'CWA' },
  { code: 'BKT', name: 'Bukit Kototabang',   name_ja: 'ブキットコトタバン', country: 'ID', lat: -0.2017, lon: 100.3183, elev_m: 864, species: ['CO2','CH4'], operator: 'BMKG' },
];

async function tryRegistryFetch() {
  if (!WDCGG_REGISTRY_URL) return null;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(WDCGG_REGISTRY_URL, {
      signal: controller.signal,
      headers: { 'user-agent': 'JapanOSINT/1.0' },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const text = await res.text();
    // A very loose CSV parser: first line is headers, subsequent lines
    // have code,name,country,lat,lon,elev. Unknown formats return null.
    const rows = text.trim().split(/\r?\n/);
    if (rows.length < 2) return null;
    const header = rows[0].toLowerCase().split(',');
    const idx = (k) => header.indexOf(k);
    const iCode = idx('code'), iName = idx('name'), iCountry = idx('country'),
          iLat = idx('latitude'), iLon = idx('longitude'), iElev = idx('elevation');
    if (iCode < 0 || iLat < 0 || iLon < 0) return null;
    const out = [];
    for (const r of rows.slice(1)) {
      const cols = r.split(',');
      const lat = parseFloat(cols[iLat]);
      const lon = parseFloat(cols[iLon]);
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      out.push({
        code: cols[iCode],
        name: cols[iName] || cols[iCode],
        country: cols[iCountry] || null,
        lat, lon,
        elev_m: Number.isFinite(parseFloat(cols[iElev])) ? parseFloat(cols[iElev]) : null,
        species: null,
        operator: null,
      });
    }
    return out.length ? out : null;
  } catch {
    return null;
  }
}

export default async function collectWdcggCo2() {
  let stations = await tryRegistryFetch();
  let liveSource = 'wdcgg_registry';
  if (!stations) {
    stations = SEED_STATIONS;
    liveSource = 'wdcgg_seed';
  }

  const features = stations.map((s) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      id: `WDCGG_${s.code}`,
      station_code: s.code,
      name: s.name,
      name_ja: s.name_ja || null,
      country: s.country,
      elevation_m: s.elev_m,
      species: s.species,
      operator: s.operator,
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
      live: liveSource === 'wdcgg_registry',
      env_hint: 'Set WDCGG_REGISTRY_URL to a CSV of GAW stations to enable full-coverage live mode',
      description: 'Greenhouse-gas monitoring stations from WMO GAW / WDCGG (Japan + neighbours)',
    },
    metadata: {},
  };
}
