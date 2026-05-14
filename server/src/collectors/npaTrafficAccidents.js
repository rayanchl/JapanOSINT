/**
 * NPA Traffic Accidents — open-data CSV with per-incident lat/lon.
 *
 * Source: Annual `honhyo_{year}.csv` published by the National Police Agency
 * at `/publications/statistics/koutsuu/opendata/{year}/`. Each row is a
 * single recorded accident. Latitude is column `地点　緯度（北緯）` and
 * longitude is `地点　経度（東経）`, both encoded as packed DMS:
 *   - latitude  9 digits → DDMMSSsss → DD°MM'SS.sss"
 *   - longitude 10 digits → DDDMMSSsss → DDD°MM'SS.sss"
 * The file is Shift_JIS, ~60MB, ~280–400k rows depending on year.
 *
 * Strategy:
 *   1. Probe the latest available year's CSV (current year, then previous).
 *   2. Stream-parse all rows; convert DMS → decimal, drop sentinels (9999).
 *   3. Aggregate to a 3-decimal lat/lon grid (~110m) keyed by (lat, lon,
 *      year_month) so the heatmap layer is tractable (~30k features instead
 *      of ~400k). Each bucket sums incidents and fatalities.
 *   4. Tag each feature `year_month` so the temporal slider in the layer
 *      panel can scrub through months within the year.
 */

import { fetchArrayBuffer, fetchText, decodeShiftJis, namedCache } from './_liveHelpers.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'npa-traffic-accidents';
const INDEX_URL_TEMPLATE = (year) =>
  `https://www.npa.go.jp/publications/statistics/koutsuu/opendata/${year}/opendata_${year}.html`;
const ONE_DAY_MS = 24 * 60 * 60 * 1000;

function packedDmsToDecimal(s, latOrLon) {
  if (s == null) return null;
  const str = String(s).trim();
  if (!str || str.startsWith('9999')) return null;
  // Latitude is 9 chars (DDMMSSsss); longitude is 10 (DDDMMSSsss).
  const ddLen = latOrLon === 'lat' ? 2 : 3;
  if (str.length < ddLen + 6) return null;
  const dd = parseInt(str.slice(0, ddLen), 10);
  const mm = parseInt(str.slice(ddLen, ddLen + 2), 10);
  const ss = parseInt(str.slice(ddLen + 2, ddLen + 4), 10);
  const ms = parseInt(str.slice(ddLen + 4, ddLen + 7) || '0', 10);
  if (!Number.isFinite(dd) || !Number.isFinite(mm) || !Number.isFinite(ss)) return null;
  const dec = dd + mm / 60 + (ss + ms / 1000) / 3600;
  return Number.isFinite(dec) ? dec : null;
}

async function discoverCsvUrl(year) {
  const html = await fetchText(INDEX_URL_TEMPLATE(year), { timeoutMs: 8000 });
  if (!html) return null;
  const m = html.match(/href="([^"]*honhyo[^"]*\.csv)"/i);
  if (!m) return null;
  const href = m[1];
  if (href.startsWith('http')) return href;
  if (href.startsWith('/')) return `https://www.npa.go.jp${href}`;
  return `https://www.npa.go.jp/publications/statistics/koutsuu/opendata/${year}/${href}`;
}

/**
 * Streaming aggregator: walks the Shift_JIS CSV text once, extracts only
 * the four columns we need (severity / fatalities / month / lat / lon) by
 * header index, and accumulates per-bucket counts as it goes. Avoids the
 * 400k × 70-field object materialisation that OOMs on default heap.
 *
 * The CSV's quoted-field handling is a non-issue here — the NPA file uses
 * comma-separated unquoted ints, so a fast `indexOf` split is safe and
 * about 5× faster than the general-purpose state machine.
 */
function streamAggregate(text, year) {
  const buckets = new Map();
  let badCoords = 0;
  let rowCount = 0;
  // Header line ends at first '\n'.
  const firstNl = text.indexOf('\n');
  if (firstNl < 0) return { buckets, badCoords, rowCount };
  const headerCols = text.slice(0, firstNl).replace(/\r$/, '').split(',');
  const idx = (name) => headerCols.indexOf(name);
  const iSev = idx('事故内容');
  const iFatal = idx('死者数');
  const iMonth = idx('発生日時　　月');
  const iLat = idx('地点　緯度（北緯）');
  const iLon = idx('地点　経度（東経）');
  if (iLat < 0 || iLon < 0 || iMonth < 0) return { buckets, badCoords, rowCount };
  const maxIdx = Math.max(iSev, iFatal, iMonth, iLat, iLon);

  let cursor = firstNl + 1;
  const len = text.length;
  while (cursor < len) {
    const eol = text.indexOf('\n', cursor);
    const lineEnd = eol < 0 ? len : eol;
    const line = text[lineEnd - 1] === '\r' ? text.slice(cursor, lineEnd - 1) : text.slice(cursor, lineEnd);
    cursor = lineEnd + 1;
    if (!line) continue;
    rowCount++;

    // Manual split — only walk to the highest column we need.
    const fields = [];
    let p = 0;
    for (let i = 0; i <= maxIdx; i++) {
      const next = line.indexOf(',', p);
      if (next < 0) { fields.push(line.slice(p)); break; }
      fields.push(line.slice(p, next));
      p = next + 1;
    }

    const lat = packedDmsToDecimal(fields[iLat], 'lat');
    const lon = packedDmsToDecimal(fields[iLon], 'lon');
    if (lat == null || lon == null) { badCoords++; continue; }
    if (lat < 20 || lat > 50 || lon < 120 || lon > 155) { badCoords++; continue; }
    const month = String(fields[iMonth] || '').padStart(2, '0');
    if (!month || month === '00') continue;
    const ym = `${year}-${month}`;
    // 2-decimal grid ≈ 1.1km cells — coarse enough that the nationwide
    // heatmap GeoJSON stays under a few MB while retaining the visual
    // density signal a heatmap renders from.
    const lat2 = Math.round(lat * 100) / 100;
    const lon2 = Math.round(lon * 100) / 100;
    const key = `${lat2},${lon2},${ym}`;
    let b = buckets.get(key);
    if (!b) {
      b = { lat: lat2, lon: lon2, year_month: ym, count: 0, fatalities: 0, severity_max: 0 };
      buckets.set(key, b);
    }
    b.count += 1;
    if (iFatal >= 0) {
      const fatal = parseInt(fields[iFatal] || '', 10);
      if (Number.isFinite(fatal)) b.fatalities += fatal;
    }
    if (iSev >= 0) {
      const sev = parseInt(fields[iSev] || '', 10);
      if (Number.isFinite(sev) && sev > b.severity_max) b.severity_max = sev;
    }
  }
  return { buckets, badCoords, rowCount };
}

async function loadYear(year) {
  const url = await discoverCsvUrl(year);
  if (!url) return null;
  const buf = await fetchArrayBuffer(url, { timeoutMs: 60000 });
  if (!buf) return null;
  const text = decodeShiftJis(buf);
  return { url, text };
}

export default async function collectNpaTrafficAccidents() {
  const fetchedAt = new Date().toISOString();
  const thisYear = new Date().getFullYear();

  // Cache the heavy parse for 24h — re-fetching on every TTL refresh would
  // re-download 60MB for the same data.
  const parsed = await namedCache(`${SOURCE_ID}:latest`, ONE_DAY_MS, async () => {
    for (const y of [thisYear - 1, thisYear - 2, thisYear - 3]) {
      const got = await loadYear(y);
      if (got && got.text && got.text.length > 100000) {
        const aggResult = streamAggregate(got.text, y);
        return { year: y, url: got.url, ...aggResult };
      }
    }
    return null;
  });

  if (!parsed) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: SOURCE_ID,
        fetchedAt,
        recordCount: 0,
        live: false,
        live_source: null,
        description: 'NPA traffic accidents — upstream CSV unavailable.',
      },
      intel: { items: [] },
    };
  }

  const { year, url, buckets, badCoords, rowCount } = parsed;

  const features = [];
  for (const b of buckets.values()) {
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [b.lon, b.lat] },
      properties: {
        id: `ACC_${b.year_month}_${b.lat}_${b.lon}`,
        year_month: b.year_month,
        count: b.count,
        fatalities: b.fatalities,
        severity_max: b.severity_max,
        source: SOURCE_ID,
      },
    });
  }

  const intelItems = [{
    uid: intelUid(SOURCE_ID, 'index'),
    title: `NPA Traffic Accidents (${year})`,
    summary: `${rowCount.toLocaleString()} accidents reported nationwide in ${year}; ${features.length.toLocaleString()} grid buckets after 110m quantisation.`,
    link: INDEX_URL_TEMPLATE(year),
    language: 'ja',
    published_at: fetchedAt,
    tags: ['safety', 'traffic', 'statistics', 'nationwide'],
    properties: {
      year,
      csv_url: url,
      raw_row_count: rowCount,
      bucket_count: features.length,
      dropped_for_bad_coords: badCoords,
    },
  }];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: SOURCE_ID,
      fetchedAt,
      recordCount: features.length,
      live: features.length > 0,
      live_source: 'npa_honhyo_csv',
      upstream_url: INDEX_URL_TEMPLATE(year),
      description: `NPA traffic accidents ${year}, quantised to a 3-decimal lat/lon grid (~110m). Each feature aggregates one month at one grid cell; year_month drives the temporal slider.`,
    },
    intel: { items: intelItems },
  };
}
