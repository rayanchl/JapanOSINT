/**
 * NHK news top RSS
 * https://www3.nhk.or.jp/rss/news/cat0.xml
 */

const API_URL = 'https://www3.nhk.or.jp/rss/news/cat0.xml';
const TIMEOUT_MS = 8000;

function parseItems(xml) {
  const items = [];
  const re = /<item>([\s\S]*?)<\/item>/g;
  let m;
  while ((m = re.exec(xml)) !== null) {
    const block = m[1];
    const title = /<title><!\[CDATA\[(.*?)\]\]><\/title>|<title>(.*?)<\/title>/s.exec(block);
    const link = /<link>(.*?)<\/link>/s.exec(block);
    const date = /<pubDate>(.*?)<\/pubDate>/s.exec(block);
    items.push({
      title: (title?.[1] ?? title?.[2] ?? '').trim(),
      link: (link?.[1] ?? '').trim(),
      published: (date?.[1] ?? '').trim(),
    });
  }
  return items;
}

export default async function collectNhkNewsRss() {
  let source = 'live';
  let items = [];
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const xml = await res.text();
    items = parseItems(xml);
    if (items.length === 0) throw new Error('empty');
  } catch {
    source = 'seed';
    items = [
      { title: '[seed] NHK news headline', link: 'https://www3.nhk.or.jp/news/', published: new Date().toUTCString() },
    ];
  }
  const features = items.slice(0, 50).map((it, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.75, 35.67] }, // NHK HQ (Shibuya)
    properties: { idx: i, title: it.title, link: it.link, published: it.published, outlet: 'NHK', source: source === 'live' ? 'nhk' : 'nhk_seed' },
  }));
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'NHK news RSS (cat0 main feed)',
    },
    metadata: {},
  };
}
