/**
 * FOFA — internet asset search engine (fofa.info), often catches JP IoT/ICS
 * that Shodan/Censys miss because of regional firmware naming and Asian-host
 * coverage. Returns JP-country hosts only.
 *
 * Auth: FOFA_API_KEY (and FOFA_EMAIL on legacy v1 keys) from
 * https://fofa.info/. Free tier is rate-limited; paid plans return more
 * fields and higher per_page caps.
 *
 * Endpoint reference:
 *   GET https://fofa.info/api/v1/search/all
 *   ?email=<email>&key=<key>&qbase64=<b64(query)>&size=100&fields=...
 *
 * Empty FeatureCollection when keys missing — no fallback.
 */

const BASE = 'https://fofa.info/api/v1/search/all';
const TIMEOUT_MS = 20000;

// Query FOFA filter expression — country=JP plus optional ICS/IoT focus.
// Override at deploy time with FOFA_QUERY=...
const DEFAULT_QUERY = process.env.FOFA_QUERY || 'country="JP"';

// Fields returned (order matters — FOFA returns positional arrays, not objects)
const FIELDS = [
  'ip', 'port', 'protocol', 'host', 'domain', 'title',
  'server', 'product', 'banner', 'country_name', 'city',
  'asn', 'org', 'lastupdatetime',
];

function decodeRow(arr) {
  const obj = {};
  FIELDS.forEach((f, i) => { obj[f] = arr[i] ?? null; });
  return obj;
}

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { getEnv } from '../utils/credentials.js';

const SOURCE_ID = 'fofa-jp';

export default async function collectFofaJp() {
  const key = getEnv(null, 'FOFA_API_KEY');
  const email = getEnv(null, 'FOFA_EMAIL') || '';
  if (!key) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'FOFA Japan-country internet assets — requires API key',
      extraMeta: { env_hint: 'Set FOFA_API_KEY (and FOFA_EMAIL for legacy v1 keys). Sign up: https://fofa.info/' },
    });
  }

  const qbase64 = Buffer.from(DEFAULT_QUERY).toString('base64');
  const params = new URLSearchParams({
    email,
    key,
    qbase64,
    size: '100',
    fields: FIELDS.join(','),
  });

  let items = [];
  let live = false;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?${params}`, {
      signal: controller.signal,
      headers: { accept: 'application/json' },
    });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    if (data.error) throw new Error(data.errmsg || 'FOFA error');
    const results = Array.isArray(data.results) ? data.results : [];

    items = results.map((row, i) => {
      const r = decodeRow(row);
      return {
        uid: intelUid(SOURCE_ID, `${r.ip}_${r.port}_${i}`),
        title: r.host || r.title || `${r.ip}:${r.port}`,
        body: r.banner ? String(r.banner).slice(0, 1000) : null,
        summary: [r.product, r.title].filter(Boolean).join(' — ') || null,
        link: r.host ? `https://fofa.info/result?qbase64=${Buffer.from(`host="${r.host}"`).toString('base64')}` : null,
        language: 'en',
        published_at: r.lastupdatetime ? safeIso(r.lastupdatetime) : null,
        tags: ['fofa', r.protocol ? `proto:${r.protocol}` : null, r.asn ? `asn:${r.asn}` : null].filter(Boolean),
        properties: {
          ip: r.ip,
          port: r.port,
          protocol: r.protocol,
          host: r.host,
          domain: r.domain,
          server: r.server,
          product: r.product,
          city: r.city,
          asn: r.asn,
          org: r.org,
        },
      };
    });
    live = items.length > 0;
  } catch (err) {
    console.warn('[fofaJp] fetch failed:', err?.message);
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'FOFA internet asset search — country="JP"',
    extraMeta: { query: DEFAULT_QUERY },
  });
}

function safeIso(s) {
  const d = new Date(s);
  return Number.isNaN(d.getTime()) ? null : d.toISOString();
}
