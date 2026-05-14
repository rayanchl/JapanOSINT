/**
 * NICTER darknet stats — daily packet counts observed at NICT's
 * darknet sensor network. https://www.nicter.jp/atlas/
 *
 * NICTER publishes aggregate stats on JS-rendered pages and via quarterly
 * PDF reports; there is no documented JSON/CSV endpoint. This collector
 * returns a single synthetic "stats" feature pinned at NICT headquarters
 * (Koganei, Tokyo) carrying whatever aggregates we can scrape from the
 * public page. When the HTML shape changes we degrade to an empty feed.
 *
 * No auth. Low value per call; worth running daily, not hourly.
 */

const NICTER_STATS_URL = 'https://www.nicter.jp/atlas/';
const TIMEOUT_MS = 15000;

// NICT HQ — anchor point so the feature has geometry for the map.
const NICT_HQ = [139.4878, 35.7100];

function extractInt(html, labelRegex) {
  if (!html) return null;
  const m = html.match(labelRegex);
  if (!m) return null;
  const num = parseInt(String(m[1]).replace(/[,，\s]/g, ''), 10);
  return Number.isFinite(num) ? num : null;
}

export default async function collectNicterStats() {
  let html = null;
  let ok = false;

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(NICTER_STATS_URL, {
      signal: controller.signal,
      headers: { 'user-agent': 'JapanOSINT/1.0' },
    });
    clearTimeout(timer);
    if (res.ok) {
      html = await res.text();
      ok = true;
    }
  } catch (err) {
    console.warn('[nicterStats] fetch failed:', err?.message);
  }

  // Heuristic scrape — the current page uses Japanese labels and
  // JS-rendered counters. These regexes are intentionally loose.
  const packetsToday   = extractInt(html, /本日[^0-9]{0,60}([0-9,，]+)/);
  const packetsTotal   = extractInt(html, /累計[^0-9]{0,60}([0-9,，]+)/);
  const sensorsOnline  = extractInt(html, /稼働センサ[^0-9]{0,60}([0-9,，]+)/);

  const features = ok ? [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: NICT_HQ },
    properties: {
      id: 'NICTER_STATS',
      packets_today: packetsToday,
      packets_total: packetsTotal,
      sensors_online: sensorsOnline,
      observed_at: new Date().toISOString(),
      html_length: html ? html.length : 0,
      source: 'nicter_atlas_scrape',
    },
  }] : [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: ok ? 'nicter_live' : 'nicter_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      scrape_caveat: 'Numbers are best-effort regex matches on a JS-rendered page; may be null',
      description: 'NICTER darknet observatory daily stats (scraped)',
    },
  };
}
