/**
 * TDnet — Tokyo Stock Exchange Timely Disclosure Network.
 * https://www.release.tdnet.info/inbs/
 *
 * Public listed companies are required to file material disclosures (M&A,
 * breach notifications, recalls, leadership changes) within minutes of the
 * decision. The Idx pages are paginated by date stamp:
 *   I_list_001_YYYYMMDD.html        — first page of today's filings
 * Different feed from EDINET (which is the statutory 有報 archive).
 *
 * The listings are static HTML, no JS, so a single GET + minimal parse works.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText } from './_liveHelpers.js';

const SOURCE_ID = 'tdnet-disclosure';
const HOST = 'https://www.release.tdnet.info';

function todayYmd() {
  const d = new Date();
  const y = d.getUTCFullYear();
  const m = String(d.getUTCMonth() + 1).padStart(2, '0');
  const day = String(d.getUTCDate()).padStart(2, '0');
  return `${y}${m}${day}`;
}

// Quick-and-dirty row extractor. TDnet rows look like:
//   <td><a href="...pdf">[title]</a></td>
//   <td>[issuer code]</td><td>[issuer name]</td><td>[time]</td>
function extractRows(html) {
  const rows = [];
  const trRe = /<tr[^>]*>([\s\S]*?)<\/tr>/gi;
  let m;
  while ((m = trRe.exec(html)) !== null) {
    const block = m[1];
    const linkM = block.match(/<a\s+href="([^"]+\.pdf)"[^>]*>([^<]+)<\/a>/i);
    if (!linkM) continue;
    const cells = [];
    const tdRe = /<td[^>]*>([\s\S]*?)<\/td>/gi;
    let t;
    while ((t = tdRe.exec(block)) !== null) {
      cells.push(t[1].replace(/<[^>]+>/g, '').replace(/\s+/g, ' ').trim());
    }
    rows.push({
      pdfUrl: linkM[1].startsWith('http') ? linkM[1] : `${HOST}/inbs/${linkM[1]}`,
      title:  linkM[2].trim(),
      cells,
    });
  }
  return rows;
}

export default async function collectTdnetDisclosure() {
  const ymd = todayYmd();
  const indexUrl = `${HOST}/inbs/I_list_001_${ymd}.html`;
  let html = '';
  try {
    html = await fetchText(indexUrl, { timeoutMs: 12000 });
  } catch { /* keep empty */ }

  const rows = extractRows(html || '');
  const items = rows.slice(0, 100).map((r, i) => ({
    uid: intelUid(SOURCE_ID, r.pdfUrl, `${ymd}-${i}`),
    title: r.title,
    summary: r.cells.join(' · ').slice(0, 240),
    link: r.pdfUrl,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['disclosure', 'tdnet', 'tse', 'corporate'],
    properties: { date: ymd, cells: r.cells },
  }));

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    description: 'TDnet — TSE Timely Disclosure Network (today\'s filings)',
    extraMeta: { index_url: indexUrl },
  });
}
