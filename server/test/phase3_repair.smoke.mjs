// Phase 3 smoke. Stands up a mock LLM (serves both the repair proposal and
// the 4B sanity re-check) plus a mock "good content" upstream, then drives
// attemptRepair through the four routing outcomes:
//
//   1. url_swap happy path        -> verified + anomaly resolved
//   2. url_swap gate failure      -> rejected + anomaly re-escalated
//   3. needs_human class          -> needs_human + anomaly resolved
//   4. transient already recovered -> auto_dismiss + anomaly resolved
//
// Runs in dry mode (REPAIR_GIT unset) so the repo working tree is never
// touched — buildPatchedRegistry still lints a real registry entry in /tmp.
//
//   node test/phase3_repair.smoke.mjs

import { createServer } from 'http';
import sources from '../src/utils/sourceRegistry.js';
import db, {
  upsertSource,
  recordAnomaly,
  recordTriage,
  getAnomalyById,
  getRepairsByAnomaly,
} from '../src/utils/database.js';

// A real single-line registry entry so buildPatchedRegistry can find + swap
// its url line. We restore its canonical url at the end.
const SRC_ID = 'comiket-events';
const REG = sources.find((s) => s.id === SRC_ID);
if (!REG) throw new Error(`registry entry ${SRC_ID} vanished — pick another single-line id`);

let nextProposal = null;

const mockLlm = createServer((req, res) => {
  let body = '';
  req.on('data', (c) => (body += c));
  req.on('end', () => {
    const sys = (() => {
      try {
        return JSON.parse(body).messages?.[0]?.content || '';
      } catch {
        return '';
      }
    })();
    let content;
    if (sys.includes('You inspect a small sample')) {
      content = JSON.stringify({ looks_correct: true, reason: 'looks like comiket events' });
    } else {
      content = JSON.stringify(nextProposal);
    }
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ choices: [{ message: { content } }] }));
  });
});

// Healthy upstream: 10 records, within ±50% of the baseline mean (10).
const mockContent = createServer((_req, res) => {
  res.writeHead(200, { 'content-type': 'application/json' });
  res.end(JSON.stringify(Array.from({ length: 10 }, (_v, i) => ({ id: i, ts: Date.now() }))));
});

function cleanup() {
  const anomalies = db.prepare('SELECT id FROM collector_anomaly WHERE source_id = ?').all(SRC_ID);
  for (const a of anomalies) db.prepare('DELETE FROM collector_repair WHERE anomaly_id = ?').run(a.id);
  db.prepare('DELETE FROM collector_anomaly WHERE source_id = ?').run(SRC_ID);
  db.prepare('DELETE FROM fetch_log WHERE source_id = ?').run(SRC_ID);
}

function seedBaseline() {
  // 6 healthy runs @ 10 records -> getRecordsBaseline mean = 10.
  const ins = db.prepare(
    "INSERT INTO fetch_log (source_id, timestamp, status, records_fetched, duration_ms) VALUES (?, datetime('now'), 'online', 10, 50)",
  );
  for (let i = 0; i < 6; i += 1) ins.run(SRC_ID);
}

function freshAnomaly(triageClass, suggestedFix) {
  const a = recordAnomaly({
    source_id: SRC_ID,
    verdict: 'status_bad',
    reason: 'phase3 smoke seed',
    evidence: '{"seed":true}',
  });
  const id = a.lastInsertRowid;
  recordTriage({
    id,
    triage_class: triageClass,
    triage_confidence: 0.9,
    triage_evidence: `seeded ${triageClass}`,
    triage_suggested_fix: suggestedFix ? JSON.stringify(suggestedFix) : null,
    triage_model: 'mock',
  });
  return id;
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

async function run() {
  cleanup();

  upsertSource({
    id: SRC_ID,
    name: REG.name,
    type: REG.type,
    category: REG.category,
    url: 'http://127.0.0.1:1/broken', // unreachable "moved" url
    status: 'offline',
  });
  seedBaseline();

  await new Promise((r) => mockLlm.listen(0, '127.0.0.1', r));
  await new Promise((r) => mockContent.listen(0, '127.0.0.1', r));
  const llmPort = mockLlm.address().port;
  const goodUrl = `http://127.0.0.1:${mockContent.address().port}/feed.json`;

  process.env.LLM_BASE_URL = `http://127.0.0.1:${llmPort}`;
  process.env.LLM_MODEL = 'mock-model';
  delete process.env.REPAIR_GIT; // dry mode — never touch the repo

  const { attemptRepair } = await import('../src/utils/collectorRepair.js');

  // ---- 1. url_swap happy path (dry mode -> verified + PARKED, not resolved) ----
  {
    const id = freshAnomaly('url_move', { kind: 'url_swap', details: `try ${goodUrl}` });
    nextProposal = { action: 'url_swap', new_url: goodUrl, rationale: 'moved here', confidence: 0.95 };
    const out = await attemptRepair(id);
    console.log('1 url_swap ->', out);
    assert(out && out.status === 'verified', `expected verified, got ${JSON.stringify(out)}`);
    assert(out.parked === true, 'dry-mode verified must be parked (not applied)');
    const row = getAnomalyById(id);
    // Dry mode does NOT change the URL, so the fix isn't live: the anomaly
    // must stay OPEN (resolving it would churn fresh anomalies forever).
    assert(!row.resolved_at, 'parked anomaly must stay open in dry mode');
    const reps = getRepairsByAnomaly(id);
    assert(reps.length === 1 && reps[0].status === 'verified', 'one verified repair row expected');
    assert(reps[0].action === 'url_swap', 'action should be url_swap');
    assert(reps[0].triage_class === 'url_move', 'triage_class snapshot persisted on repair row');
    assert(JSON.parse(reps[0].patch).new_url === goodUrl, 'patch should record new_url');
    assert(JSON.parse(reps[0].gate).records === 10, 'gate should record 10 records');

    // Re-attempt must be a no-op: parked by hasStagedRepair, no LLM, no row.
    const again = await attemptRepair(id);
    assert(again === null, 'staged anomaly must be parked (null) on re-attempt');
    assert(getRepairsByAnomaly(id).length === 1, 'no duplicate repair row on re-attempt');
  }

  // ---- 2. url_swap gate failure (new url unreachable) ----
  {
    const id = freshAnomaly('url_move', null);
    nextProposal = {
      action: 'url_swap',
      new_url: 'http://127.0.0.1:2/dead',
      rationale: 'guess',
      confidence: 0.6,
    };
    const out = await attemptRepair(id);
    console.log('2 gate-fail ->', out);
    assert(out && out.status === 'rejected', `expected rejected, got ${JSON.stringify(out)}`);
    const row = getAnomalyById(id);
    assert(!row.resolved_at, 'rejected anomaly stays open');
    assert(row.escalation_level === 1, `escalation should bump to 1, got ${row.escalation_level}`);
    assert(row.triage_class === null, 'triage_class cleared for re-triage');
    assert(row.triaged_at === null, 'triaged_at cleared for re-triage');
    const reps = getRepairsByAnomaly(id);
    assert(reps.length === 1 && reps[0].status === 'rejected', 'one rejected repair row');
    assert(JSON.parse(reps[0].gate).failed?.includes('2xx'), 'gate failure reason recorded');
  }

  // ---- 3. needs_human class ----
  {
    const id = freshAnomaly('selector_drift', null);
    const out = await attemptRepair(id);
    console.log('3 needs_human ->', out);
    assert(out && out.status === 'needs_human', `expected needs_human, got ${JSON.stringify(out)}`);
    const row = getAnomalyById(id);
    assert(row.resolved_at, 'needs_human anomaly is resolved out of the queue');
    assert(row.resolution?.startsWith('needs_human: selector_drift'), `resolution: ${row.resolution}`);
    const reps = getRepairsByAnomaly(id);
    assert(reps.length === 1 && reps[0].status === 'needs_human', 'one needs_human repair row');
  }

  // ---- 4. transient already recovered ----
  {
    const id = freshAnomaly('transient', null);
    // a healthy run strictly after the anomaly's created_at
    db.prepare(
      "INSERT INTO fetch_log (source_id, timestamp, status, records_fetched, duration_ms) VALUES (?, datetime('now','+120 seconds'), 'online', 11, 40)",
    ).run(SRC_ID);
    const out = await attemptRepair(id);
    console.log('4 transient ->', out);
    assert(out && out.status === 'verified' && out.action === 'auto_dismiss', `expected auto_dismiss, got ${JSON.stringify(out)}`);
    const row = getAnomalyById(id);
    assert(row.resolved_at && row.resolution?.startsWith('auto: transient recovered'), `resolution: ${row.resolution}`);
  }

  // restore canonical registry url + drop smoke rows
  upsertSource({
    id: SRC_ID,
    name: REG.name,
    type: REG.type,
    category: REG.category,
    url: REG.url,
    status: REG.status || 'offline',
  });
  cleanup();
  mockLlm.close();
  mockContent.close();
  console.log('\nPhase 3 smoke: OK');
}

run()
  .then(() => process.exit(0))
  .catch((err) => {
    console.error('SMOKE FAIL:', err);
    try {
      cleanup();
    } catch {
      /* ignore */
    }
    mockLlm.close();
    mockContent.close();
    process.exit(1);
  });
