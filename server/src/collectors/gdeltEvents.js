/**
 * GDELT Global Events Collector
 *
 * GDELT 2.0 publishes a raw events CSV every 15 minutes at
 * http://data.gdeltproject.org/gdeltv2/<YYYYMMDDHHMMSS>.export.CSV.zip
 * and an index file at http://data.gdeltproject.org/gdeltv2/lastupdate.txt
 * listing the most recent export.
 *
 * This collector fetches the latest export, filters events whose action
 * location is in Japan (FIPS 10-4 country code "JA"), and returns them as
 * a GeoJSON FeatureCollection. No client-side filtering beyond the country
 * match — filters on CAMEO event codes, NumSources, or tone can be added
 * later if the noise level is unmanageable.
 *
 * Columns (GDELT 2.0 Event Database codebook, 0-indexed, tab-separated):
 *   0  GlobalEventID
 *   1  Day (YYYYMMDD)
 *   26 EventCode (CAMEO)
 *   27 EventBaseCode
 *   28 EventRootCode
 *   30 GoldsteinScale
 *   31 NumMentions
 *   32 NumSources
 *   33 NumArticles
 *   34 AvgTone
 *   52 ActionGeo_FullName
 *   53 ActionGeo_CountryCode  (FIPS 10-4 — "JA" for Japan)
 *   56 ActionGeo_Lat
 *   57 ActionGeo_Long
 *   59 DATEADDED (YYYYMMDDHHMMSS)
 *   60 SOURCEURL
 */

import AdmZip from 'adm-zip';

const INDEX_URL = 'http://data.gdeltproject.org/gdeltv2/lastupdate.txt';
const TIMEOUT_MS = 20000;
const JAPAN_FIPS = 'JA';

async function fetchLatestExportUrl() {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  try {
    const res = await fetch(INDEX_URL, { signal: controller.signal });
    if (!res.ok) throw new Error(`index HTTP ${res.status}`);
    const text = await res.text();
    // First line is the events export. Format: "<size> <md5> <url>"
    const firstLine = text.split('\n').find(Boolean);
    if (!firstLine) throw new Error('empty index');
    const parts = firstLine.trim().split(/\s+/);
    const url = parts[2];
    if (!url || !url.endsWith('.export.CSV.zip')) {
      throw new Error(`unexpected index entry: ${firstLine}`);
    }
    return url;
  } finally {
    clearTimeout(timer);
  }
}

async function fetchZipBuffer(url) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  try {
    const res = await fetch(url, { signal: controller.signal });
    if (!res.ok) throw new Error(`export HTTP ${res.status}`);
    const arrayBuf = await res.arrayBuffer();
    return Buffer.from(arrayBuf);
  } finally {
    clearTimeout(timer);
  }
}

function extractCsv(zipBuf) {
  const zip = new AdmZip(zipBuf);
  const entry = zip.getEntries().find((e) => e.entryName.endsWith('.export.CSV'));
  if (!entry) throw new Error('no .export.CSV entry in zip');
  return entry.getData().toString('utf8');
}

function parseDateAdded(s) {
  // YYYYMMDDHHMMSS -> ISO
  if (!s || s.length < 14) return null;
  const y = s.slice(0, 4), mo = s.slice(4, 6), d = s.slice(6, 8);
  const h = s.slice(8, 10), mi = s.slice(10, 12), se = s.slice(12, 14);
  return `${y}-${mo}-${d}T${h}:${mi}:${se}Z`;
}

function rowsToFeatures(csv) {
  const features = [];
  const lines = csv.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (!line) continue;
    const f = line.split('\t');
    if (f.length < 61) continue;
    if (f[53] !== JAPAN_FIPS) continue;

    const lat = parseFloat(f[56]);
    const lon = parseFloat(f[57]);
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;

    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: {
        event_id: f[0] || null,
        event_code: f[26] || null,
        event_base_code: f[27] || null,
        event_root_code: f[28] || null,
        goldstein_scale: f[30] ? parseFloat(f[30]) : null,
        num_mentions: f[31] ? parseInt(f[31], 10) : null,
        num_sources: f[32] ? parseInt(f[32], 10) : null,
        num_articles: f[33] ? parseInt(f[33], 10) : null,
        avg_tone: f[34] ? parseFloat(f[34]) : null,
        location_name: f[52] || null,
        country_code: f[53] || null,
        date_added: parseDateAdded(f[59]),
        timestamp: parseDateAdded(f[59]),
        url: f[60] || null,
        source: 'gdelt',
      },
    });
  }
  return features;
}

export default async function collectGdeltEvents() {
  let features = [];
  let source = 'gdelt_live';
  let exportUrl = null;
  let error = null;

  try {
    exportUrl = await fetchLatestExportUrl();
    const zipBuf = await fetchZipBuffer(exportUrl);
    const csv = extractCsv(zipBuf);
    features = rowsToFeatures(csv);
  } catch (err) {
    source = 'gdelt_error';
    error = err?.message ?? String(err);
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      exportUrl,
      error,
      description: 'GDELT 2.0 events (latest 15-min export, ActionGeo.CountryCode=JA)',
    },
    metadata: {},
  };
}
