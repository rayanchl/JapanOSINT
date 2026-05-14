/**
 * NPA Cyber Threat Observation (不審なアクセスの観測状況).
 *
 * Source: `https://www.npa.go.jp/bureau/cyber/koho/observation.html` — the
 * NPA Cyber Police Bureau's live dashboard of darknet/honeypot scan
 * traffic into Japan. The page itself renders four hourly-updated graphs
 * (signature detections, dest port, source country, JP-origin dest port)
 * as PNGs; per-data-point values aren't exposed in HTML.
 *
 * We surface the dashboard as a single map pin at NPA HQ (the observation
 * locus) plus an intel item linking out for users to view the live
 * graphs. When the page emits any HTML-visible summary text, we capture
 * it into the feature/intel `summary`.
 */

import { fetchText } from './_liveHelpers.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'npa-cyber-threat-obs';
const URL_PAGE = 'https://www.npa.go.jp/bureau/cyber/koho/observation.html';
const NPA_HQ = { lat: 35.6749, lon: 139.7531 };

function extractSummary(html) {
  if (!html) return null;
  // Common pattern: leading <p> describing the observation method.
  const m = html.match(/<p[^>]*>([\s\S]*?)<\/p>/);
  if (!m) return null;
  const text = m[1].replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
  return text.slice(0, 500);
}

function extractGraphImages(html) {
  const out = [];
  const re = /<img[^>]+src="([^"]+\.(?:png|jpg|svg))"/gi;
  let m;
  while ((m = re.exec(html))) {
    const src = m[1];
    if (/observation|access|honey|sensor|detect|port|country/i.test(src)) {
      out.push(src.startsWith('http') ? src : `https://www.npa.go.jp${src.startsWith('/') ? '' : '/bureau/cyber/koho/'}${src}`);
    }
  }
  return out;
}

export default async function collectNpaCyberThreatObs() {
  const fetchedAt = new Date().toISOString();
  const html = await fetchText(URL_PAGE, { timeoutMs: 8000, retries: 1 });
  const summary = extractSummary(html);
  const graphs = extractGraphImages(html || '');

  const features = html
    ? [{
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [NPA_HQ.lon, NPA_HQ.lat] },
        properties: {
          id: 'CYBER_OBS_NPA',
          name: 'NPA Cyber Threat Observation',
          summary: summary || '警察庁サイバー警察局 不審アクセス観測ダッシュボード (live)',
          dashboard_url: URL_PAGE,
          graph_count: graphs.length,
          source: SOURCE_ID,
        },
      }]
    : [];

  const intelItems = html
    ? [{
        uid: intelUid(SOURCE_ID, 'dashboard'),
        title: 'NPA Cyber Threat Observation Dashboard',
        summary: summary || 'Live darknet / honeypot scan traffic into Japan, refreshed hourly.',
        link: URL_PAGE,
        language: 'ja',
        published_at: fetchedAt,
        tags: ['cyber', 'observation', 'npa', 'live'],
        properties: {
          dashboard_url: URL_PAGE,
          graph_urls: graphs,
        },
      }]
    : [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: SOURCE_ID,
      fetchedAt,
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? 'npa_observation_html' : null,
      upstream_url: URL_PAGE,
      description: 'NPA cyber threat-observation dashboard (single map pin at NPA HQ; full graph data only available on the live page).',
    },
    intel: { items: intelItems },
  };
}
