/**
 * GDELT 2.0 Global Events — Japan slice, raw CSV path (no API key, no record cap).
 *
 * GDELT 2.0 publishes a raw events CSV every 15 minutes at
 *   http://data.gdeltproject.org/gdeltv2/<YYYYMMDDHHMMSS>.export.CSV.zip
 * and an index file at
 *   http://data.gdeltproject.org/gdeltv2/lastupdate.txt
 * whose first line is "<size> <md5> <url>" pointing at the most recent export.
 *
 * Default behaviour: fetch the latest 15-min slice, filter to Japan (FIPS 10-4
 * country code "JA"), and emit every event — geocoded as Point features for
 * the map, ungeocoded as `geometry: null` features so the ingest pipeline
 * (mirrorCollectorOutput → upsertItems) routes them into the intel store
 * (visible in DatabaseExplorerTab + counted in SourcesPanel).
 *
 * Set GDELT_SLICES=N (1..96) to walk back N consecutive 15-min slices and
 * union the results — N=4 ≈ last hour, N=96 ≈ last 24h.
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

const BUCKET = 'http://data.gdeltproject.org/gdeltv2';
const INDEX_URL = `${BUCKET}/lastupdate.txt`;
const TIMEOUT_MS = 20000;
const JAPAN_FIPS = 'JA';
const SLICE_MS = 15 * 60 * 1000;
const MAX_SLICES = 96;

function clampSlices(raw) {
  const n = parseInt(raw, 10);
  if (!Number.isFinite(n) || n < 1) return 1;
  return Math.min(n, MAX_SLICES);
}

async function fetchLatestExportUrl() {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  try {
    const res = await fetch(INDEX_URL, { signal: controller.signal });
    if (!res.ok) throw new Error(`index HTTP ${res.status}`);
    const text = await res.text();
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

function timestampFromUrl(url) {
  const m = url.match(/\/(\d{14})\.export\.CSV\.zip$/);
  return m ? m[1] : null;
}

function parseStamp(s) {
  // YYYYMMDDHHMMSS → Date (UTC)
  if (!s || s.length < 14) return null;
  const iso = `${s.slice(0, 4)}-${s.slice(4, 6)}-${s.slice(6, 8)}T${s.slice(8, 10)}:${s.slice(10, 12)}:${s.slice(12, 14)}Z`;
  const d = new Date(iso);
  return Number.isNaN(d.getTime()) ? null : d;
}

function formatStamp(date) {
  const pad = (n, w = 2) => String(n).padStart(w, '0');
  return (
    pad(date.getUTCFullYear(), 4) +
    pad(date.getUTCMonth() + 1) +
    pad(date.getUTCDate()) +
    pad(date.getUTCHours()) +
    pad(date.getUTCMinutes()) +
    pad(date.getUTCSeconds())
  );
}

function priorSliceUrls(latestUrl, count) {
  if (count <= 1) return [latestUrl];
  const stamp = timestampFromUrl(latestUrl);
  const start = parseStamp(stamp);
  if (!start) return [latestUrl];
  const urls = [latestUrl];
  for (let i = 1; i < count; i++) {
    const t = new Date(start.getTime() - i * SLICE_MS);
    urls.push(`${BUCKET}/${formatStamp(t)}.export.CSV.zip`);
  }
  return urls;
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
  if (!s || s.length < 14) return null;
  return `${s.slice(0, 4)}-${s.slice(4, 6)}-${s.slice(6, 8)}T${s.slice(8, 10)}:${s.slice(10, 12)}:${s.slice(12, 14)}Z`;
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
    const geocoded = Number.isFinite(lat) && Number.isFinite(lon);
    const avgTone = f[34] ? parseFloat(f[34]) : null;
    const locationName = f[52] || null;
    const url = f[60] || null;
    const dateAdded = parseDateAdded(f[59]);

    features.push({
      type: 'Feature',
      geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
      properties: {
        event_id: f[0] || null,
        name: locationName,
        location_name: locationName,
        event_code: f[26] || null,
        event_base_code: f[27] || null,
        event_root_code: f[28] || null,
        goldstein_scale: f[30] ? parseFloat(f[30]) : null,
        num_mentions: f[31] ? parseInt(f[31], 10) : null,
        num_sources: f[32] ? parseInt(f[32], 10) : null,
        num_articles: f[33] ? parseInt(f[33], 10) : null,
        avg_tone: avgTone,
        tone: avgTone,
        country: 'JP',
        country_code: f[53] || null,
        date_added: dateAdded,
        timestamp: dateAdded,
        url,
        source: 'gdelt',
      },
    });
  }
  return features;
}

export default async function collectGdelt() {
  const slices = clampSlices(process.env.GDELT_SLICES);
  const fetchedUrls = [];
  const sliceErrors = [];
  let features = [];
  let live = false;
  let topLevelError = null;

  try {
    const latestUrl = await fetchLatestExportUrl();
    const urls = priorSliceUrls(latestUrl, slices);
    for (const url of urls) {
      try {
        const zipBuf = await fetchZipBuffer(url);
        const csv = extractCsv(zipBuf);
        features = features.concat(rowsToFeatures(csv));
        fetchedUrls.push(url);
      } catch (err) {
        sliceErrors.push({ url, error: err?.message ?? String(err) });
      }
    }
    live = fetchedUrls.length > 0;
  } catch (err) {
    topLevelError = err?.message ?? String(err);
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'gdelt',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      slices,
      exportUrls: fetchedUrls,
      sliceErrors: sliceErrors.length ? sliceErrors : undefined,
      error: topLevelError,
      env_hint: 'Override GDELT_SLICES (1..96, default 1; 4 ≈ last hour, 96 ≈ last 24h)',
      description:
        'GDELT 2.0 events (raw 15-min export, ActionGeo.CountryCode=JA); geocoded → map, ungeocoded → intel store',
    },
  };
}
