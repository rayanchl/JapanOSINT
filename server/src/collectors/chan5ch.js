/**
 * 5ch (former 2ch) — JP textboard pulse.
 *
 * Pulls subject.txt for a curated set of boards. Each line is
 *   "<thread_id>.dat<TAB>title (replyCount)"
 * which is enough to surface trending threads (newsplus, livejupiter,
 * bizplus, sec) without scraping post bodies. For deeper drill-down,
 * fetch the dat file separately.
 *
 * Override boards with CHAN5CH_BOARDS=domain:board,domain:board,...
 *   e.g. greta.5ch.net:newsplus,medaka.5ch.net:livegalileo
 *
 * Polite: send a JP UA, reasonable delay between boards.
 *
 * Note: 5ch geo-blocks non-JP source IPs at the TCP layer. Run from a JP-egress
 * deployment or via a JP residential proxy; otherwise this collector will
 * return zero records (it fails gracefully — no exception raised).
 */

const TIMEOUT_MS = 12000;

// Default board set: news, breaking news, business, security, IT
const DEFAULT_BOARDS = (process.env.CHAN5CH_BOARDS || [
  'greta.5ch.net:newsplus',     // news+
  'medaka.5ch.net:liveplus',    // ニュース実況
  'fate.5ch.net:bizplus',       // ビジネス
  'mevius.5ch.net:sec',         // セキュリティ
  'mao.5ch.net:internet',       // インターネット
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

const PER_BOARD_LIMIT = parseInt(process.env.CHAN5CH_PER_BOARD_LIMIT || '40', 10);

async function fetchBoard(spec) {
  const [host, board] = spec.split(':');
  if (!host || !board) return [];
  const url = `https://${host}/${board}/subject.txt`;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(url, {
      signal: controller.signal,
      headers: {
        'user-agent': 'Monazilla/1.00 (JapanOSINT)',
        accept: 'text/plain, */*',
      },
    });
    clearTimeout(timer);
    if (!res.ok) return [];
    // 5ch responses are typically Shift_JIS — Node fetch returns text decoded
    // as UTF-8 by default which mangles JP. We re-decode from buffer.
    const buf = Buffer.from(await res.arrayBuffer());
    let text;
    try {
      text = new TextDecoder('shift_jis').decode(buf);
    } catch {
      text = buf.toString('utf8');
    }
    const lines = text.split(/\r?\n/).filter(Boolean).slice(0, PER_BOARD_LIMIT);
    return lines.map((line) => {
      // "1735456789.dat<TAB>thread title (123)"
      const m = /^(\d+)\.dat\s*[<\t> ]+(.*?)\s*\((\d+)\)\s*$/.exec(line)
        || /^(\d+)\.dat\s+(.*?)\s*\((\d+)\)\s*$/.exec(line);
      if (!m) return null;
      const [, tid, title, replies] = m;
      return {
        host, board, tid,
        title,
        replies: parseInt(replies, 10),
        url: `https://${host}/test/read.cgi/${board}/${tid}/`,
      };
    }).filter(Boolean);
  } catch {
    return [];
  }
}

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'chan-5ch';

export default async function collectChan5ch() {
  const out = [];
  for (const spec of DEFAULT_BOARDS) {
    out.push(...await fetchBoard(spec));
    await new Promise((r) => setTimeout(r, 800));
  }

  const items = out.map((t) => ({
    uid: intelUid(SOURCE_ID, `${t.host}_${t.board}_${t.tid}`),
    title: t.title,
    summary: `${t.replies} replies on /${t.board}`,
    link: t.url,
    language: 'ja',
    tags: ['5ch', `board:${t.board}`],
    properties: {
      board: t.board,
      host: t.host,
      thread_id: t.tid,
      replies: t.replies,
    },
  }));

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: '5ch trending threads — JP textboard pulse',
    extraMeta: {
      boards: DEFAULT_BOARDS,
      env_hint: 'Override via CHAN5CH_BOARDS=host:board,host:board; tune CHAN5CH_PER_BOARD_LIMIT',
    },
  });
}
