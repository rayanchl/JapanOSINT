/**
 * Wikipedia (ja) recent changes — track mainspace edits over the last hour.
 *
 * Free, no auth.
 *   GET https://ja.wikipedia.org/w/api.php?action=query&list=recentchanges&...
 */

import { fetchJson } from './_liveHelpers.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL = 'https://ja.wikipedia.org/w/api.php?action=query&list=recentchanges&rcprop=title|user|timestamp|comment|sizes|ids&rcnamespace=0&rclimit=200&format=json&origin=*';
const TIMEOUT_MS = 12000;

export default async function collectWikipediaJaRecent() {
  const json = await fetchJson(URL, {
    timeoutMs: TIMEOUT_MS,
    headers: { 'user-agent': 'japanosint-collector' },
  });
  if (!json) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'wp_ja_error',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        error: 'fetch_failed',
        description: 'Wikipedia (ja) recent changes — fetch failed',
      },
    };
  }

  const arr = json?.query?.recentchanges || [];
  const features = arr.slice(0, 200).map((rc, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: TOKYO },
    properties: {
      idx: i,
      title: rc.title,
      user: rc.user,
      timestamp: rc.timestamp,
      comment: rc.comment,
      pageid: rc.pageid,
      revid: rc.revid,
      old_revid: rc.old_revid,
      oldlen: rc.oldlen,
      newlen: rc.newlen,
      delta: (rc.newlen ?? 0) - (rc.oldlen ?? 0),
      url: rc.title ? `https://ja.wikipedia.org/wiki/${encodeURIComponent(rc.title.replace(/ /g, '_'))}` : null,
      source: 'wp_ja_recentchanges',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wp_ja_recentchanges',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Wikipedia (ja) recent mainspace edits — last 200',
    },
  };
}
