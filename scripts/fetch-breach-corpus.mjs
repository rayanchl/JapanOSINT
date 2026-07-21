#!/usr/bin/env node
// Build the breach-corpus METADATA MANIFEST used by the breach-check pipeline.
//
//   node scripts/fetch-breach-corpus.mjs         # OFFLINE (default): build JSON from the committed seed TSV
//   node scripts/fetch-breach-corpus.mjs --live  # opt-in: refresh manifest metadata from HIBP /breaches
//
// The pipeline is fully self-hosted (see docs/breach-check-pipeline.md): datasets are downloaded and
// ingested ahead of time, and lookups make ZERO network calls. This script only produces the
// breach *metadata* manifest (names/dates/prevalence — never credentials) that seeds the breach_meta
// table so a hit can report *which* breach. Default is offline from docs/breach-corpus.seed.tsv.
// --live is a convenience for refreshing that metadata against HIBP's public keyless /api/v3/breaches;
// it touches metadata only and is never on the lookup path.

import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const SEED = join(ROOT, 'docs', 'breach-corpus.seed.tsv');
const OUT = join(ROOT, 'docs', 'breach-corpus.json');
const HIBP = 'https://haveibeenpwned.com/api/v3/breaches';
const live = process.argv.includes('--live');

// "821.1k" | "6.6M" | "2B" | "126" -> integer
function parseCount(s) {
  const m = String(s).trim().match(/^([\d.]+)\s*([kMB]?)$/);
  if (!m) return null;
  const n = parseFloat(m[1]);
  const mult = { '': 1, k: 1e3, M: 1e6, B: 1e9 }[m[2]];
  return Math.round(n * mult);
}

// Deterministic id from the breach name (stable across refreshes).
function slug(name) {
  return name.toLowerCase().normalize('NFKD').replace(/[^\w]+/g, '-').replace(/^-|-$/g, '');
}

async function fromSeed() {
  const text = await readFile(SEED, 'utf8');
  const rows = [];
  for (const line of text.split(/\r?\n/)) {
    if (!line || line.startsWith('#')) continue;
    const [name, records, added, breach] = line.split('\t');
    if (!name || name === 'name') continue;
    rows.push({
      id: slug(name), name, title: name, domain: null,
      pwn_count: parseCount(records), added_date: added || null, breach_date: breach || null,
      data_classes: null, sensitive: false, verified: null, source_id: 'hibp-corpus-seed',
    });
  }
  return rows;
}

async function fromHibp() {
  const res = await fetch(HIBP, { headers: { 'User-Agent': 'OSINTsaas-breach-corpus/1.0' } });
  if (!res.ok) throw new Error(`HIBP ${res.status}`);
  const data = await res.json();
  return data.map((b) => ({
    id: slug(b.Name), name: b.Name, title: b.Title, domain: b.Domain || null,
    pwn_count: b.PwnCount ?? null,
    added_date: (b.AddedDate || '').slice(0, 7) || null,
    breach_date: (b.BreachDate || '').slice(0, 7) || null,
    data_classes: b.DataClasses || null,
    sensitive: !!b.IsSensitive, verified: !!b.IsVerified, source_id: 'hibp',
  }));
}

let rows, origin;
if (live) {
  try {
    rows = await fromHibp();
    origin = 'hibp-live';
  } catch (e) {
    console.error(`--live fetch failed (${e.message}); falling back to committed seed TSV`);
    rows = await fromSeed();
    origin = 'seed-fallback';
  }
} else {
  rows = await fromSeed();
  origin = 'seed';
}

rows.sort((a, b) => (b.added_date || '').localeCompare(a.added_date || ''));
await writeFile(OUT, JSON.stringify({ origin, count: rows.length, breaches: rows }, null, 2));
console.log(`wrote ${rows.length} breaches -> ${OUT} (origin: ${origin})`);
