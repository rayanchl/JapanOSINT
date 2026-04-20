/**
 * Hatena Bookmark — trending Japanese articles.
 *
 * Source: https://b.hatena.ne.jp/hotentry.rss (RDF/RSS 1.0)
 * No auth, no documented rate limit. Japan-only.
 *
 * Not geospatial. Returned as a FeatureCollection of zero-geometry features
 * keyed by article URL so the framework's logging/DB pipeline accepts it;
 * the client does not render a map layer for this source.
 */

const FEED_URL = 'https://b.hatena.ne.jp/hotentry.rss';
const TIMEOUT_MS = 15000;

function decodeXmlEntities(s) {
  if (!s) return s;
  return s
    .replace(/&#x([0-9a-fA-F]+);/g, (_, h) => String.fromCodePoint(parseInt(h, 16)))
    .replace(/&#(\d+);/g, (_, d) => String.fromCodePoint(parseInt(d, 10)))
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&apos;/g, "'");
}

function parseRdfXml(xml) {
  // Hatena returns RDF 1.0 with <item> elements bearing rdf:about + <title>,
  // <link>, <dc:date>, <hatena:bookmarkcount>. A tiny regex parse is enough
  // and avoids adding an xml dep for a single feed.
  const items = [];
  const itemRe = /<item\b[^>]*>([\s\S]*?)<\/item>/g;
  let m;
  while ((m = itemRe.exec(xml)) !== null) {
    const body = m[1];
    const get = (tag) => {
      const r = new RegExp(`<${tag}\\b[^>]*>([\\s\\S]*?)</${tag}>`);
      const mm = body.match(r);
      return mm ? mm[1].trim() : null;
    };
    const stripCdata = (s) => (s ? s.replace(/^<!\[CDATA\[|\]\]>$/g, '') : s);
    const clean = (tag) => decodeXmlEntities(stripCdata(get(tag)));
    items.push({
      title: clean('title'),
      link: clean('link'),
      date: clean('dc:date'),
      bookmarks: parseInt(clean('hatena:bookmarkcount') || '0', 10) || 0,
      subject: clean('dc:subject'),
    });
  }
  return items;
}

export default async function collectHatenaBookmark() {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  let items = [];
  let liveSource = 'hatena_live';

  try {
    const res = await fetch(FEED_URL, {
      signal: controller.signal,
      headers: { 'user-agent': 'JapanOSINT/1.0 (+https://github.com)' },
    });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const xml = await res.text();
    items = parseRdfXml(xml);
  } catch (err) {
    console.warn('[hatenaBookmark] fetch failed:', err?.message);
    liveSource = 'hatena_unavailable';
    items = [];
  }

  const features = items.map((it, i) => ({
    type: 'Feature',
    geometry: null,
    properties: {
      id: `HATENA_${i + 1}`,
      title: it.title,
      url: it.link,
      bookmarks: it.bookmarks,
      category: it.subject,
      published_at: it.date,
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
      feed_url: FEED_URL,
      description: 'Hatena Bookmark trending articles (Japanese web pulse)',
    },
    metadata: {},
  };
}
