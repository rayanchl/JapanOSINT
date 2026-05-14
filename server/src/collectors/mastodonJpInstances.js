/**
 * Public local timelines from major JP Mastodon instances.
 *
 * Free, no auth required for the public API. We poll each instance's
 * /api/v1/timelines/public?local=true and merge the results.
 *
 * Override MASTODON_JP_INSTANCES=mstdn.jp,pawoo.net,fedibird.com,mastodon-japan.net
 * Override MASTODON_JP_LIMIT=20 (per instance).
 */

const TIMEOUT_MS = 12000;
const TOKYO = [139.6917, 35.6895];

const INSTANCES = (process.env.MASTODON_JP_INSTANCES || [
  'mstdn.jp', 'pawoo.net', 'fedibird.com', 'mastodon-japan.net',
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

const LIMIT = Number(process.env.MASTODON_JP_LIMIT || 25);

async function pollOne(host) {
  const url = `https://${host}/api/v1/timelines/public?local=true&limit=${LIMIT}`;
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: { accept: 'application/json', 'user-agent': 'japanosint-collector' },
    });
    clearTimeout(t);
    if (!res.ok) return { host, err: `HTTP ${res.status}` };
    const arr = await res.json();
    return { host, posts: Array.isArray(arr) ? arr : [] };
  } catch (err) { return { host, err: err?.message || 'fetch_failed' }; }
}

export default async function collectMastodonJpInstances() {
  const results = await Promise.all(INSTANCES.map(pollOne));

  const features = [];
  for (const r of results) {
    if (r.err) {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: { kind: 'instance_error', host: r.host, err: r.err, source: 'mastodon_jp' },
      });
      continue;
    }
    for (const p of (r.posts || [])) {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          host: r.host,
          id: p.id,
          uri: p.uri,
          url: p.url,
          created_at: p.created_at,
          content: String(p.content || '').replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').slice(0, 400),
          spoiler_text: p.spoiler_text,
          language: p.language,
          replies_count: p.replies_count,
          reblogs_count: p.reblogs_count,
          favourites_count: p.favourites_count,
          author_acct: p?.account?.acct,
          author_url: p?.account?.url,
          author_followers: p?.account?.followers_count,
          source: `mastodon_${r.host}`,
        },
      });
    }
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'mastodon_jp',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      instances_polled: INSTANCES.length,
      env_hint: 'MASTODON_JP_INSTANCES csv to override; MASTODON_JP_LIMIT per-instance',
      description: 'Public local timelines from JP Mastodon instances',
    },
  };
}
