/**
 * GitHub code search — surface JP-domain leaks (internal hostnames, SMTP
 * credentials, hardcoded API keys, config files mentioning *.co.jp / *.go.jp).
 *
 * This is *defensive* recon: results are public code on GitHub. Treat any
 * surfaced credential as already-burned and used for blue-team triage only.
 *
 * Auth: GITHUB_TOKEN (classic PAT or fine-grained, read-only public scope is
 * enough). Required — code search API rejects anonymous requests.
 *
 * Endpoint:
 *   GET https://api.github.com/search/code?q=<query>&per_page=50
 */

const BASE = 'https://api.github.com/search/code';
const TIMEOUT_MS = 20000;

const DEFAULT_QUERIES = (process.env.GITHUB_LEAK_QUERIES || [
  '"smtp.co.jp" password',
  '".go.jp" api_key',
  '".co.jp" client_secret',
  'extension:env ".jp"',
  'extension:yml ".co.jp" password',
].join('|')).split('|').map((s) => s.trim()).filter(Boolean);

const PER_QUERY = parseInt(process.env.GITHUB_LEAK_PER_QUERY || '20', 10);

async function searchOne(token, q) {
  const params = new URLSearchParams({ q, per_page: String(PER_QUERY) });
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?${params}`, {
      signal: controller.signal,
      headers: {
        accept: 'application/vnd.github+json',
        authorization: `Bearer ${token}`,
        'x-github-api-version': '2022-11-28',
        'user-agent': 'JapanOSINT-leakcheck',
      },
    });
    clearTimeout(timer);
    if (!res.ok) return { q, error: `HTTP ${res.status}`, items: [] };
    const json = await res.json();
    return { q, items: Array.isArray(json.items) ? json.items : [] };
  } catch (err) {
    return { q, error: err?.message || 'fetch failed', items: [] };
  }
}

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'github-leaks-jp';

export default async function collectGithubLeaksJp() {
  const token = process.env.GITHUB_TOKEN;
  if (!token) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'GitHub code search for JP-domain credential leaks',
      extraMeta: { env_hint: 'Set GITHUB_TOKEN (PAT with public_repo / read scope) — code search rejects anonymous requests' },
    });
  }

  const out = [];
  for (const q of DEFAULT_QUERIES) {
    out.push(await searchOne(token, q));
    await new Promise((r) => setTimeout(r, 2200));
  }

  const items = [];
  for (const { q, items: hits } of out) {
    for (const it of hits) {
      items.push({
        uid: intelUid(SOURCE_ID, it.sha, `${it.repository?.full_name}_${it.path}`),
        title: `${it.repository?.full_name || '?'} — ${it.path || '?'}`,
        summary: `Match for: ${q}`,
        link: it.html_url || null,
        author: it.repository?.owner?.login || null,
        language: 'en',
        tags: ['github-leak', 'defensive', `query:${q.slice(0, 32)}`],
        properties: {
          query: q,
          repo: it.repository?.full_name || null,
          path: it.path || null,
          score: it.score ?? null,
          repo_visibility: it.repository?.visibility || 'public',
          repo_stars: it.repository?.stargazers_count ?? null,
          sha: it.sha || null,
        },
      });
    }
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'GitHub code search — JP-domain credential / config leaks (defensive)',
    extraMeta: {
      queries: DEFAULT_QUERIES,
      env_hint: 'Override queries via GITHUB_LEAK_QUERIES (pipe-delimited); tune GITHUB_LEAK_PER_QUERY',
    },
  });
}
