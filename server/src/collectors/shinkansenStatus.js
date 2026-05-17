/**
 * Shinkansen operating status (shinkansen-status).
 *
 * Aggregates the official Shinkansen operating-status portals across
 * operators (JR East / JR Central / JR West / JR Kyushu / JR Hokkaido).
 * Each is best-effort fetched; per-operator items carry reachability + a
 * stripped text snippet. No fabricated status — honest unreachable flag.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText, fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'shinkansen-status';

const LINES = [
  { op: 'JR East',     line: 'Tohoku/Joetsu/Hokuriku Shinkansen', url: 'https://traininfo.jreast.co.jp/shinkansen/' },
  { op: 'JR Central',  line: 'Tokaido Shinkansen',                url: 'https://global.jr-central.co.jp/en/info/status/' },
  { op: 'JR West',     line: 'Sanyo/Hokuriku Shinkansen',         url: 'https://www.westjr.co.jp/global/en/traffic/' },
  { op: 'JR Kyushu',   line: 'Kyushu Shinkansen',                 url: 'https://www.jrkyushu.co.jp/unkou/' },
  { op: 'JR Hokkaido', line: 'Hokkaido Shinkansen',               url: 'https://www3.jrhokkaido.co.jp/traininfo/' },
];

export default async function collectShinkansenStatus() {
  const items = [];
  let anyLive = false;

  for (const l of LINES) {
    let live = false;
    try { live = await fetchHead(l.url); } catch { live = false; }
    if (live) anyLive = true;

    let snippet = null;
    try {
      const html = await fetchText(l.url, { timeoutMs: 10000 });
      snippet = (html || '').replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 240);
    } catch { snippet = null; }

    items.push({
      uid: intelUid(SOURCE_ID, l.op),
      title: `${l.line} status (${l.op})`,
      summary: snippet || `${l.op} ${l.line} operating-status portal`,
      body: snippet || null,
      link: l.url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['transit', 'shinkansen', 'rail', l.op.toLowerCase().replace(/\s+/g, '-'), live ? 'reachable' : 'unreachable'],
      properties: { operator: l.op, line: l.line, reachable: live },
    });
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Shinkansen operating-status portals across all JR operators',
  });
}
