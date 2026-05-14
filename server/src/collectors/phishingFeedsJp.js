/**
 * PhishTank + OpenPhish — JP-targeting phishing URLs.
 *
 * Two open feeds, lightly filtered:
 *   - OpenPhish community: https://openphish.com/feed.txt  (auth-free, plain list)
 *   - PhishTank:           https://data.phishtank.com/data/online-valid.csv
 *                          (PHISHTANK_APP_KEY raises rate limit)
 *
 * Filter: keep entries whose host ends in .jp OR whose URL contains a major
 * JP brand keyword (rakuten, jcb, mizuho, mufg, smbc, jp-bank, japanpost,
 * yamato, sagawa, ana, jal, aeon, family-mart, yodobashi, mercari, paypay).
 */

import { intelEnvelope, intelUid, intelHashKey } from '../utils/intelHelpers.js';

const SOURCE_ID = 'phishing-feeds-jp';
const URL_OPENPHISH = 'https://openphish.com/feed.txt';
const URL_PHISHTANK = 'https://data.phishtank.com/data/online-valid.csv';
const TIMEOUT_MS = 15000;

const JP_KEYWORDS = [
  'rakuten', 'jcb', 'mizuho', 'mufg', 'smbc', 'japanpost', 'jp-bank',
  'yamato', 'sagawa', 'kuroneko', 'ana', 'jal', 'aeon', 'familymart',
  'family-mart', 'yodobashi', 'mercari', 'paypay', 'docomo', 'au', 'softbank',
  'jcom', 'nicovideo', 'amazon\\.co\\.jp', 'apple\\.com.*jp', 'line\\.me',
];

const JP_RE = new RegExp(`(?:${JP_KEYWORDS.join('|')})`, 'i');

function isJp(url) {
  const u = String(url || '').toLowerCase();
  if (!u) return false;
  return /\.jp(\b|\/|:|$)/.test(u) || JP_RE.test(u);
}

async function fetchText(url, headers) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(url, { signal: ctrl.signal, headers });
    clearTimeout(t);
    if (!r.ok) return '';
    return await r.text();
  } catch { return ''; }
}

function parsePhishtankCsv(text) {
  // header: phish_id,url,phish_detail_url,submission_time,verified,verification_time,online,target
  const out = [];
  const lines = text.split(/\r?\n/);
  let firstSkipped = false;
  for (const raw of lines) {
    const line = raw.trim();
    if (!line) continue;
    if (!firstSkipped) { firstSkipped = true; continue; }
    const cols = [];
    let cur = ''; let inQuote = false;
    for (const ch of line) {
      if (ch === '"') inQuote = !inQuote;
      else if (ch === ',' && !inQuote) { cols.push(cur); cur = ''; }
      else cur += ch;
    }
    cols.push(cur);
    out.push({
      phish_id: cols[0],
      url: cols[1],
      detail: cols[2],
      submitted: cols[3],
      target: cols[7],
    });
  }
  return out;
}

export default async function collectPhishingFeedsJp() {
  const ptKey = process.env.PHISHTANK_APP_KEY || '';
  const [opText, ptText] = await Promise.all([
    fetchText(URL_OPENPHISH, { 'user-agent': 'japanosint-collector' }),
    fetchText(`${URL_PHISHTANK}${ptKey ? `?app_key=${encodeURIComponent(ptKey)}` : ''}`, {
      'user-agent': ptKey ? `phishtank/${ptKey}` : 'japanosint-collector',
    }),
  ]);

  const opUrls = opText.split(/\r?\n/).map((s) => s.trim()).filter(Boolean);
  const ptRows = parsePhishtankCsv(ptText);

  const items = [];

  opUrls.filter(isJp).slice(0, 250).forEach((u) => {
    const host = (() => { try { return new URL(u).hostname; } catch { return null; } })();
    items.push({
      uid: intelUid(SOURCE_ID, intelHashKey('op', u)),
      title: host || u,
      summary: 'OpenPhish feed match',
      link: u,
      language: 'en',
      tags: ['phishing', 'openphish', host ? `host:${host}` : null].filter(Boolean),
      properties: { feed: 'openphish', host, full_url: u },
    });
  });

  ptRows.filter((r) => isJp(r.url)).slice(0, 250).forEach((r) => {
    const host = (() => { try { return new URL(r.url).hostname; } catch { return null; } })();
    items.push({
      uid: intelUid(SOURCE_ID, r.phish_id, intelHashKey('pt', r.url)),
      title: r.target ? `${r.target} — ${host || r.url}` : (host || r.url),
      summary: r.detail || null,
      link: r.url,
      language: 'en',
      published_at: r.submitted || null,
      tags: ['phishing', 'phishtank', r.target ? `target:${r.target}` : null].filter(Boolean),
      properties: {
        feed: 'phishtank',
        host,
        full_url: r.url,
        phish_id: r.phish_id,
        target: r.target,
        detail: r.detail,
      },
    });
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'PhishTank + OpenPhish — JP-targeting phishing URLs',
    extraMeta: {
      openphish_total: opUrls.length,
      phishtank_total: ptRows.length,
      env_hint: 'Optional PHISHTANK_APP_KEY raises PhishTank rate limit',
    },
  });
}
