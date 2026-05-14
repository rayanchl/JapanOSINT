/**
 * ProjectDiscovery Chaos — bug-bounty subdomain dataset, JP-program filter.
 *
 * Free, public bucket index at https://chaos-data.projectdiscovery.io/index.json
 * Each entry has program_name and a download URL with subdomain list. We
 * keep entries whose program_name matches a JP-headquartered company and
 * (if CHAOS_DOWNLOAD_FULL=1) download up to MAX subdomain lists.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const INDEX_URL = 'https://chaos-data.projectdiscovery.io/index.json';
const TIMEOUT_MS = 15000;

const JP_PROGRAMS = (process.env.CHAOS_JP_PROGRAMS || [
  'line', 'mercari', 'cybozu', 'sansan', 'freee', 'money forward', 'smarthr',
  'recruit', 'rakuten', 'cookpad', 'gmo', 'pixiv', 'dwango', 'klab', 'paypay',
  'kakaku', 'mufg', 'mizuho', 'smbc', 'sbi', 'softbank', 'kddi', 'ntt',
  'sony', 'nintendo', 'sega', 'square enix', 'capcom', 'bandai', 'konami',
  'nikkei', 'asahi', 'mainichi', 'yomiuri',
].join(',')).split(',').map((s) => s.trim().toLowerCase()).filter(Boolean);

const DOWNLOAD = String(process.env.CHAOS_DOWNLOAD_FULL || '0') === '1';
const MAX_DOWNLOAD = Number(process.env.CHAOS_MAX_PROGRAMS || 4);

function isJpProgram(name) {
  const n = String(name || '').toLowerCase();
  return JP_PROGRAMS.some((needle) => n.includes(needle));
}

async function fetchOne(url) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(url, { signal: ctrl.signal });
    clearTimeout(t);
    if (!r.ok) return null;
    return await r.text();
  } catch { return null; }
}

export default createThreatIntelCollector({
  sourceId: 'chaos_index',
  description: 'ProjectDiscovery Chaos — JP-headquartered bug-bounty program index',
  run: async () => {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(INDEX_URL, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(t);
    let index = [];
    let live = false;
    if (res.ok) {
      index = await res.json();
      live = Array.isArray(index);
    }

    const jp = (Array.isArray(index) ? index : []).filter((p) => isJpProgram(p?.name));
    const features = [];
    jp.forEach((p, i) => {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: i,
          kind: 'program',
          program_name: p.name,
          program_url: p.URL || null,
          bounty: p.bounty ?? null,
          platform: p.platform ?? null,
          download_url: p.URL,
          last_updated: p.last_updated || null,
          change: p.change ?? null,
          domains: Array.isArray(p.domains) ? p.domains : [],
          source: 'chaos_index',
        },
      });
    });

    if (DOWNLOAD) {
      const targets = jp.slice(0, MAX_DOWNLOAD);
      for (const p of targets) {
        const txt = p.URL ? await fetchOne(p.URL) : null;
        if (!txt) continue;
        const sub = txt.split(/\r?\n/).map((s) => s.trim()).filter(Boolean).slice(0, 200);
        sub.forEach((host, j) => {
          features.push({
            type: 'Feature',
            geometry: { type: 'Point', coordinates: TOKYO },
            properties: {
              idx: features.length,
              kind: 'subdomain',
              program_name: p.name,
              host,
              sub_idx: j,
              source: 'chaos_subdomains',
            },
          });
        });
      }
    }

    return {
      features,
      source: live ? 'chaos_index' : 'chaos_seed',
      extraMeta: {
        total_programs: Array.isArray(index) ? index.length : 0,
        jp_programs: jp.length,
        env_hint: 'CHAOS_DOWNLOAD_FULL=1 to expand top JP programs into subdomain features',
      },
    };
  },
});
