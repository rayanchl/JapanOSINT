/**
 * JPCERT/CC security advisories RSS/RDF
 * https://www.jpcert.or.jp/rss/jpcert.rdf
 */

const API_URL = 'https://www.jpcert.or.jp/rss/jpcert.rdf';
const TIMEOUT_MS = 8000;

function parseItems(xml) {
  const items = [];
  const re = /<item[^>]*>([\s\S]*?)<\/item>/g;
  let m;
  while ((m = re.exec(xml)) !== null) {
    const block = m[1];
    const title = /<title>(.*?)<\/title>/s.exec(block);
    const link = /<link>(.*?)<\/link>/s.exec(block);
    const date = /<dc:date>(.*?)<\/dc:date>|<pubDate>(.*?)<\/pubDate>/s.exec(block);
    items.push({
      title: (title?.[1] ?? '').trim(),
      link: (link?.[1] ?? '').trim(),
      published: (date?.[1] ?? date?.[2] ?? '').trim(),
    });
  }
  return items;
}

export default async function collectJpcertAlertsRss() {
  let items = [];
  let source = 'live';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    items = parseItems(await res.text());
    if (items.length === 0) throw new Error('empty');
  } catch {
    source = 'seed';
    items = [{ title: '[seed] JPCERT advisory', link: 'https://www.jpcert.or.jp/', published: new Date().toUTCString() }];
  }
  const features = items.slice(0, 50).map((it, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.7515, 35.6761] }, // JPCERT Ochanomizu
    properties: { idx: i, title: it.title, link: it.link, published: it.published, issuer: 'JPCERT/CC', source: source === 'live' ? 'jpcert' : 'jpcert_seed' },
  }));
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'JPCERT/CC security advisories RSS/RDF',
    },
    metadata: {},
  };
}
