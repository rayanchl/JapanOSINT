/**
 * NASA FIRMS — VIIRS/MODIS active-fire pixels over Japan.
 *
 * Auth: NASA_FIRMS_MAP_KEY (free; https://firms.modaps.eosdis.nasa.gov/api/area/).
 *
 * Endpoint:
 *   https://firms.modaps.eosdis.nasa.gov/api/area/csv/<MAP_KEY>/VIIRS_NOAA20_NRT/JPN/1
 *
 * Source: 'firms_active_fire' — geometry from per-row latitude/longitude.
 */

const TIMEOUT_MS = 20000;

const SENSOR = process.env.FIRMS_SENSOR || 'VIIRS_NOAA20_NRT';
const DAYS = Math.min(Number(process.env.FIRMS_DAYS || 1), 10);

export default async function collectNasaFirmsJp() {
  const key = process.env.NASA_FIRMS_MAP_KEY || process.env.FIRMS_MAP_KEY || '';
  if (!key) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'firms_no_key',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        env_hint: 'Set NASA_FIRMS_MAP_KEY (free https://firms.modaps.eosdis.nasa.gov/api/area/)',
        description: 'NASA FIRMS active fires over JP — no key',
      },
    };
  }
  const url = `https://firms.modaps.eosdis.nasa.gov/api/area/csv/${key}/${SENSOR}/JPN/${DAYS}`;

  let csv = '';
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(url, { signal: ctrl.signal });
    clearTimeout(t);
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    csv = await r.text();
  } catch (err) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'firms_error',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        error: err?.message || 'fetch_failed',
        description: 'NASA FIRMS — fetch failed',
      },
    };
  }

  const lines = csv.split(/\r?\n/);
  if (lines.length < 2) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'firms_empty',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        description: 'NASA FIRMS — no fire pixels in window',
      },
    };
  }
  const header = lines[0].split(',');
  const idx = (name) => header.indexOf(name);
  const iLat = idx('latitude');
  const iLon = idx('longitude');
  const iBright = idx('bright_ti4') !== -1 ? idx('bright_ti4') : idx('brightness');
  const iAcq = idx('acq_date');
  const iAcqT = idx('acq_time');
  const iConf = idx('confidence');
  const iFrp = idx('frp');
  const iSat = idx('satellite');

  const features = [];
  for (let li = 1; li < lines.length; li += 1) {
    const line = lines[li].trim();
    if (!line) continue;
    const cols = line.split(',');
    const lon = Number(cols[iLon]); const lat = Number(cols[iLat]);
    const geocoded = Number.isFinite(lon) && Number.isFinite(lat);
    features.push({
      type: 'Feature',
      geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
      properties: {
        bright: Number(cols[iBright]),
        acq_date: cols[iAcq],
        acq_time: cols[iAcqT],
        confidence: cols[iConf],
        frp: Number(cols[iFrp]),
        satellite: cols[iSat],
        source: 'firms_active_fire',
      },
    });
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'firms_active_fire',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      sensor: SENSOR,
      window_days: DAYS,
      env_hint: 'FIRMS_SENSOR (default VIIRS_NOAA20_NRT); FIRMS_DAYS (1-10)',
      description: 'NASA FIRMS — active-fire pixels over JP last N days',
    },
  };
}
