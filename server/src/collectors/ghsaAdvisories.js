/**
 * GitHub Security Advisories (GHSA) — global advisories REST endpoint.
 *
 * Free with optional GITHUB_TOKEN for the higher rate limit. Pulls the
 * 100 most recent global advisories. Useful as a high-velocity overlay
 * to JVN — many CVEs surface here days before JVN ingests them.
 *
 * Endpoint: GET /advisories on api.github.com (REST v2022-11-28).
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL = 'https://api.github.com/advisories?per_page=100&sort=published&direction=desc';
const TIMEOUT_MS = 15000;

export default createThreatIntelCollector({
  sourceId: 'github_ghsa_rest',
  description: 'GitHub Security Advisories — most recent 100 global entries',
  run: async () => {
    const token = process.env.GITHUB_TOKEN || '';
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL, {
      signal: ctrl.signal,
      headers: {
        accept: 'application/vnd.github+json',
        'X-GitHub-Api-Version': '2022-11-28',
        'user-agent': 'japanosint-collector',
        ...(token ? { authorization: `Bearer ${token}` } : {}),
      },
    });
    clearTimeout(timer);
    let entries = [];
    let live = false;
    if (res.ok) {
      entries = await res.json();
      live = Array.isArray(entries) && entries.length > 0;
    }
    const features = (Array.isArray(entries) ? entries : []).map((a, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        ghsa_id: a.ghsa_id || null,
        cve_id: a.cve_id || null,
        title: a.summary || null,
        severity: a.severity || null,
        cvss: a.cvss?.score ?? null,
        published: a.published_at || null,
        updated: a.updated_at || null,
        withdrawn: a.withdrawn_at || null,
        url: a.html_url || null,
        ecosystems: Array.isArray(a.vulnerabilities)
          ? Array.from(new Set(a.vulnerabilities.map((v) => v?.package?.ecosystem).filter(Boolean)))
          : [],
        source: live ? 'github_ghsa_rest' : 'github_ghsa_seed',
      },
    }));
    return {
      features,
      source: live ? 'live' : 'seed',
      extraMeta: { env_hint: 'Optional GITHUB_TOKEN raises rate limit from 60/hr to 5000/hr' },
    };
  },
});
