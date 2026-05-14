/**
 * MyJVN — IPA/JPCERT Japan Vulnerability Notes (jvndb.jvn.jp).
 *
 * Pulls the most recent JVN iPedia entries via the public getVulnOverviewList
 * REST endpoint. JVN catalogues both global CVEs picked up by JP CERTs and
 * Japan-only advisories that never get a CVE (Hitachi, Yokogawa, Mitsubishi,
 * Cybozu, Trend Micro JP, etc) — pairs with jpcertAlertsRss + ipaAlertsRss
 * for full coverage.
 *
 * Endpoint:
 *   GET https://jvndb.jvn.jp/myjvn?method=getVulnOverviewList&feed=hnd&lang=en
 *      &rangeDatePublic=n&rangeDatePublished=n&maxCountItem=50
 * Response is XML (atom-ish). No auth, no key. Free.
 */

// Drop lang=en — the en subset is tiny (often empty). Default JP feed
// returns ~50 entries per call.
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'my-jvn';
const FEED_URL = 'https://jvndb.jvn.jp/myjvn?method=getVulnOverviewList&feed=hnd&maxCountItem=50';
const TIMEOUT_MS = 12000;

function parseItems(xml) {
  const out = [];
  const re = /<item[^>]*>([\s\S]*?)<\/item>/g;
  let m;
  while ((m = re.exec(xml)) !== null) {
    const block = m[1];
    const grab = (tag) => {
      const r = new RegExp(`<${tag}[^>]*>(?:<!\\[CDATA\\[(.*?)\\]\\]>|([\\s\\S]*?))<\\/${tag}>`, 's').exec(block);
      return ((r?.[1] || r?.[2]) ?? '').trim();
    };
    out.push({
      id: grab('sec:identifier') || grab('dc:identifier'),
      title: grab('title'),
      link: grab('link'),
      published: grab('dcterms:issued') || grab('dc:date'),
      modified: grab('dcterms:modified'),
      description: grab('description').slice(0, 500),
    });
  }
  return out;
}

export default async function collectMyJvn() {
  let items = [];
  let live = false;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(FEED_URL, {
      signal: ctrl.signal,
      headers: { accept: 'application/xml,text/xml,*/*' },
    });
    clearTimeout(timer);
    if (res.ok) {
      items = parseItems(await res.text());
      live = items.length > 0;
    }
  } catch { /* fallthrough */ }

  const intelItems = items.map((it) => ({
    uid: intelUid(SOURCE_ID, it.id, it.link),
    title: it.title,
    body: it.description || null,
    summary: (it.description || '').slice(0, 240) || null,
    link: it.link || null,
    language: 'ja',
    published_at: it.published || null,
    tags: ['advisory', 'jvn', 'cyber'],
    properties: {
      jvn_id: it.id || null,
      modified: it.modified || null,
    },
  }));

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: intelItems,
    live,
    description: 'JVN iPedia / MyJVN — Japan vulnerability database advisories',
  });
}
