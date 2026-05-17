/**
 * Typosquat / phishing-domain discovery for JP corp & gov domains
 * (dnstwist-jp-targets).
 *
 * Key-free. We generate a deterministic set of dnstwist-style domain
 * permutations for a fixed list of high-value Japanese corporate and
 * government domains, then DNS-resolve each candidate via public DNS-over-HTTPS
 * (Google `dns.google/resolve`, key-free). Only permutations that actually
 * resolve to an A record are emitted — these are real, observed registrations,
 * not fabricated. On failure we return honest empty.
 *
 * DoH endpoint (verified, key-free): https://dns.google/resolve?name=<d>&type=A
 */

import { fetchJson } from './_liveHelpers.js';

const SOURCE_ID = 'dnstwist-jp-targets';

// High-value JP targets (corporate + government). Public, well-known domains.
const TARGETS = [
  'mufg.jp', 'smbc.co.jp', 'mizuhobank.co.jp', 'ntt.co.jp', 'softbank.jp',
  'rakuten.co.jp', 'toyota.jp', 'sony.co.jp', 'jp.gov', 'mhlw.go.jp',
  'mod.go.jp', 'npa.go.jp', 'soumu.go.jp', 'jreast.co.jp',
];

const HOMOGLYPHS = { o: '0', l: '1', i: '1', e: '3', a: '4', s: '5' };
const TLD_SWAPS = ['com', 'net', 'org', 'co', 'jp.com'];

function permutations(domain) {
  const dot = domain.indexOf('.');
  const sld = domain.slice(0, dot);
  const tld = domain.slice(dot + 1);
  const set = new Set();

  // Bitsquatting-ish: character omission
  for (let i = 0; i < sld.length; i++) {
    set.add(sld.slice(0, i) + sld.slice(i + 1) + '.' + tld);
  }
  // Adjacent transposition
  for (let i = 0; i < sld.length - 1; i++) {
    const a = sld.split('');
    [a[i], a[i + 1]] = [a[i + 1], a[i]];
    set.add(a.join('') + '.' + tld);
  }
  // Repetition
  for (let i = 0; i < sld.length; i++) {
    set.add(sld.slice(0, i + 1) + sld[i] + sld.slice(i + 1) + '.' + tld);
  }
  // Homoglyph substitution
  for (let i = 0; i < sld.length; i++) {
    const sub = HOMOGLYPHS[sld[i]];
    if (sub) set.add(sld.slice(0, i) + sub + sld.slice(i + 1) + '.' + tld);
  }
  // Hyphenation
  for (let i = 1; i < sld.length; i++) {
    set.add(sld.slice(0, i) + '-' + sld.slice(i) + '.' + tld);
  }
  // TLD swap
  for (const t of TLD_SWAPS) set.add(sld + '.' + t);

  set.delete(domain);
  return [...set];
}

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'Resolving typosquat permutations of high-value JP corp/gov domains (DoH-verified, key-free)',
    },
  };
}

async function resolves(domain) {
  const url = `https://dns.google/resolve?name=${encodeURIComponent(domain)}&type=A`;
  const data = await fetchJson(url, { timeoutMs: 8000 });
  if (!data || data.Status !== 0 || !Array.isArray(data.Answer)) return null;
  const a = data.Answer.filter((r) => r.type === 1).map((r) => r.data);
  return a.length ? a : null;
}

export default async function collectDnstwistJpTargets() {
  const items = [];
  let probed = 0;

  for (const target of TARGETS) {
    const perms = permutations(target);
    for (const cand of perms) {
      probed++;
      let ips = null;
      try {
        ips = await resolves(cand);
      } catch { ips = null; }
      if (!ips) continue;
      items.push({
        uid: `${SOURCE_ID}|${cand}`,
        title: `${cand} (typosquat of ${target})`,
        summary: `Resolving permutation of ${target} → ${ips.join(', ')}`,
        body: `Candidate phishing/typosquat domain "${cand}" resolves to A record(s): ${ips.join(', ')}. Original target: ${target}.`,
        link: `http://${cand}`,
        author: null,
        language: 'en',
        published_at: null,
        tags: ['typosquat', 'phishing', 'dnstwist', `target:${target}`],
        properties: { candidate: cand, target, a_records: ips, source: 'doh_google' },
      });
    }
  }

  if (probed === 0) return envelope([], 'dnstwist_jp_targets_unavailable');
  // Zero resolving permutations is a valid, honest result (empty is fine).
  return envelope(items, items.length ? 'doh_google' : 'dnstwist_jp_targets_empty');
}
