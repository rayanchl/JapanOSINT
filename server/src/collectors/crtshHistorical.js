/**
 * crt.sh — historical Certificate Transparency snapshot for .jp targets.
 *
 * Free, no auth. Complements the live `certstreamJp` ingest with
 * historical certs for a curated list of high-value JP targets.
 *
 * Endpoint: GET https://crt.sh/?q=<wildcard>&output=json
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';

const BASE = 'https://crt.sh/?output=json&q=';
const TIMEOUT_MS = 20000;

const DEFAULT_TARGETS = (process.env.CRTSH_TARGETS || [
  '%.go.jp', '%.gov.jp',
  '%.mod.go.jp', '%.kantei.go.jp',
  '%.mufg.jp', '%.smbc.co.jp', '%.mizuho-fg.co.jp', '%.japanpost.jp',
  '%.rakuten.co.jp', '%.line.me', '%.softbank.jp',
  '%.toyota.co.jp', '%.sony.co.jp',
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

const PER_TARGET = Number(process.env.CRTSH_PER_TARGET || 60);

async function fetchOne(target) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(`${BASE}${encodeURIComponent(target)}`, {
      signal: ctrl.signal,
      headers: { accept: 'application/json' },
    });
    clearTimeout(t);
    if (!r.ok) return [];
    const j = await r.json();
    return Array.isArray(j) ? j : [];
  } catch { return []; }
}

export default createThreatIntelCollector({
  sourceId: 'crt_sh_history',
  description: 'crt.sh historical certs for high-value JP targets',
  run: async () => {
    const concurrency = 3;
    const out = [];
    for (let i = 0; i < DEFAULT_TARGETS.length; i += concurrency) {
      const slice = DEFAULT_TARGETS.slice(i, i + concurrency);
      out.push(...(await Promise.all(slice.map(fetchOne))));
    }

    const features = [];
    out.forEach((rows, qi) => {
      const target = DEFAULT_TARGETS[qi];
      rows.slice(0, PER_TARGET).forEach((c) => {
        features.push({
          type: 'Feature',
          geometry: null,
          properties: {
            target,
            cert_id: c.id,
            common_name: c.common_name,
            name_value: c.name_value,
            issuer_name: c.issuer_name,
            not_before: c.not_before,
            not_after: c.not_after,
            entry_timestamp: c.entry_timestamp,
            serial_number: c.serial_number,
            source: 'crt_sh_history',
          },
        });
      });
    });

    return {
      features,
      extraMeta: {
        targets_polled: DEFAULT_TARGETS.length,
        env_hint: 'CRTSH_TARGETS comma list of wildcards; CRTSH_PER_TARGET to cap',
      },
    };
  },
});
