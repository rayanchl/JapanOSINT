/**
 * Wayback Machine CDX API — historical snapshots of JP gov/corp sites.
 *
 * Surfaces: deleted advisories, removed personnel pages, old API endpoints,
 * and historical reorgs. Useful baseline for diffing what JP orgs *used to*
 * publish vs what they hide today.
 *
 * Endpoint:
 *   GET https://web.archive.org/cdx/search/cdx
 *     ?url=<host/prefix>&matchType=prefix&limit=100&output=json
 *
 * Free, no auth. Override targets with WAYBACK_TARGETS=host1,host2,...
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'wayback-jp';
const BASE = 'https://web.archive.org/cdx/search/cdx';
const TIMEOUT_MS = 35000;

const DEFAULT_TARGETS = (process.env.WAYBACK_TARGETS || [
  'kantei.go.jp',
  'mod.go.jp',
  'mofa.go.jp',
  'meti.go.jp',
  'soumu.go.jp',
  'jpcert.or.jp',
  'ipa.go.jp',
  'nisc.go.jp',
  'pmda.go.jp',
  'mhlw.go.jp',
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

const PER_TARGET_LIMIT = parseInt(process.env.WAYBACK_PER_TARGET_LIMIT || '20', 10);

async function fetchOne(host) {
  const params = new URLSearchParams({
    url: host,
    matchType: 'prefix',
    limit: String(PER_TARGET_LIMIT),
    output: 'json',
    fl: 'timestamp,original,statuscode,mimetype,digest',
    filter: 'statuscode:200',
    collapse: 'digest',
  });

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?${params}`, {
      signal: controller.signal,
      headers: { accept: 'application/json' },
    });
    clearTimeout(timer);
    if (!res.ok) return [];
    const rows = await res.json();
    if (!Array.isArray(rows) || rows.length < 2) return [];
    const [header, ...data] = rows;
    return data.map((row) => {
      const obj = {};
      header.forEach((k, i) => { obj[k] = row[i]; });
      obj._target = host;
      return obj;
    });
  } catch {
    return [];
  }
}

export default async function collectWaybackJp() {
  const all = await Promise.all(DEFAULT_TARGETS.map(fetchOne));
  const flat = all.flat();

  const items = flat.map((row, i) => {
    const replay = row.timestamp && row.original
      ? `https://web.archive.org/web/${row.timestamp}/${row.original}`
      : null;
    let publishedIso = null;
    if (row.timestamp && /^\d{14}$/.test(row.timestamp)) {
      const t = row.timestamp;
      publishedIso = `${t.slice(0,4)}-${t.slice(4,6)}-${t.slice(6,8)}T${t.slice(8,10)}:${t.slice(10,12)}:${t.slice(12,14)}Z`;
    }
    return {
      uid: intelUid(SOURCE_ID, row.digest, `${row._target}|${row.timestamp}|${i}`),
      title: row.original || row._target,
      summary: `${row._target} · ${row.mimetype || 'unknown mime'} · ${row.statuscode || '?'}`,
      link: replay,
      language: 'ja',
      published_at: publishedIso,
      tags: ['wayback', `host:${row._target}`],
      properties: {
        target: row._target,
        timestamp: row.timestamp || null,
        original: row.original || null,
        mimetype: row.mimetype || null,
        statuscode: row.statuscode || null,
        digest: row.digest || null,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'Wayback Machine CDX snapshots for JP government / cyber agency hosts',
    extraMeta: {
      targets: DEFAULT_TARGETS,
      env_hint: 'Override targets via WAYBACK_TARGETS=host1,host2,...; tune WAYBACK_PER_TARGET_LIMIT',
    },
  });
}
