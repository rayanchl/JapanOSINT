/**
 * Yahoo! JAPAN Realtime Search "Buzz" trending — JP X/Twitter trending
 * keywords without a paid X API. Free, no auth (HTML scrape).
 *
 * Endpoint (HTML):
 *   https://search.yahoo.co.jp/realtime/buzz   (アクセスランキング)
 */

const URL = 'https://search.yahoo.co.jp/realtime/buzz';
const TIMEOUT_MS = 15000;
const TOKYO = [139.6917, 35.6895];

function decodeEntities(s) {
  return String(s)
    .replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"').replace(/&#039;|&apos;/g, "'")
    .replace(/&#(\d+);/g, (_, d) => String.fromCharCode(Number(d)));
}

function stripTags(s) {
  return String(s).replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
}

function parseBuzz(html) {
  const out = [];
  // Yahoo wraps trending entries in <li> tagged with data-rapid-cl-msec or
  // similar. We grab any <a href="/realtime/search?p=…"> + text pair we can.
  const re = /<a[^>]+href="\/realtime\/search\?p=([^"&]+)[^"]*"[^>]*>([\s\S]*?)<\/a>/g;
  let m; const seen = new Set();
  while ((m = re.exec(html)) !== null) {
    const term = decodeURIComponent(m[1].replace(/\+/g, ' '));
    const label = decodeEntities(stripTags(m[2]));
    if (!term || term.length > 60 || /<svg|^http/.test(term)) continue;
    if (seen.has(term)) continue;
    seen.add(term);
    out.push({ term, label });
    if (out.length >= 60) break;
  }
  return out;
}

export default async function collectYahooRealtime() {
  let html = '';
  let live = false;
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL, {
      signal: ctrl.signal,
      headers: { 'user-agent': 'Mozilla/5.0 japanosint-collector', accept: 'text/html' },
    });
    clearTimeout(t);
    if (res.ok) { html = await res.text(); live = html.length > 0; }
  } catch { /* ignore */ }

  const items = parseBuzz(html);
  const features = items.map((it, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: TOKYO },
    properties: {
      idx: i,
      rank: i + 1,
      term: it.term,
      label: it.label || it.term,
      url: `https://search.yahoo.co.jp/realtime/search?p=${encodeURIComponent(it.term)}`,
      source: live ? 'yahoo_realtime_buzz' : 'yahoo_realtime_seed',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: live ? 'live' : 'seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Yahoo! JAPAN Realtime Search — buzz/trending keywords',
    },
  };
}
