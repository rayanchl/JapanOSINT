// Phase 4 smoke. Three independent checks:
//
//   A. circuit breaker — N failed repair cycles quarantine the source, the
//      repair worker then skips it, a healthy probe (clearQuarantine) lifts
//      it, and half-open vs active is reported correctly.
//   B. shared LLM concurrency — the 'heavy' tier never runs >1 at once.
//   C. digest — buildMaintenanceDigest has the stable shape and reflects
//      the quarantined source + recent repair rows.
//
//   node test/phase4_governance.smoke.mjs

import { createServer } from 'http';
import sources from '../src/utils/sourceRegistry.js';
import db, {
  upsertSource,
  recordAnomaly,
  recordTriage,
  getActiveQuarantine,
  getQuarantinedSources,
  quarantineSource,
  clearQuarantine,
  countRecentFailedRepairs,
} from '../src/utils/database.js';
import { withLlmSlot, llmConcurrencySnapshot } from '../src/utils/llmConcurrency.js';
import { buildMaintenanceDigest } from '../src/utils/maintenanceReport.js';

const SRC_ID = 'comiket-events';
const REG = sources.find((s) => s.id === SRC_ID);

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
    const content = sys.includes('You inspect a small sample')
      ? JSON.stringify({ looks_correct: true, reason: 'ok' })
      : JSON.stringify({
          action: 'url_swap',
          new_url: 'http://127.0.0.1:2/still-dead',
          rationale: 'guess',
          confidence: 0.5,
        });
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ choices: [{ message: { content } }] }));
  });
});

function cleanup() {
  const anomalies = db.prepare('SELECT id FROM collector_anomaly WHERE source_id = ?').all(SRC_ID);
  for (const a of anomalies) db.prepare('DELETE FROM collector_repair WHERE anomaly_id = ?').run(a.id);
  db.prepare('DELETE FROM collector_anomaly WHERE source_id = ?').run(SRC_ID);
  db.prepare('DELETE FROM fetch_log WHERE source_id = ?').run(SRC_ID);
  clearQuarantine(SRC_ID);
}

function assert(c, m) {
  if (!c) throw new Error(m);
}

function freshTriagedAnomaly() {
  const a = recordAnomaly({ source_id: SRC_ID, verdict: 'status_bad', reason: 'p4 seed' });
  const id = a.lastInsertRowid;
  recordTriage({
    id,
    triage_class: 'url_move',
    triage_confidence: 0.9,
    triage_evidence: 'seed',
    triage_model: 'mock',
  });
  return id;
}

async function run() {
  cleanup();
  upsertSource({
    id: SRC_ID,
    name: REG.name,
    type: REG.type,
    category: REG.category,
    url: 'http://127.0.0.1:1/broken',
    status: 'offline',
  });

  await new Promise((r) => mockLlm.listen(0, '127.0.0.1', r));
  process.env.LLM_BASE_URL = `http://127.0.0.1:${mockLlm.address().port}`;
  process.env.LLM_MODEL = 'mock-model';
  delete process.env.REPAIR_GIT;
  const { attemptRepair } = await import('../src/utils/collectorRepair.js');

  // ── A. circuit breaker (threshold default = 3) ──
  // Two failed cycles already on the clock, then drive the third.
  db.prepare(
    "INSERT INTO collector_repair (anomaly_id, source_id, status, action) VALUES (?,?,?,?)",
  ).run(freshTriagedAnomaly(), SRC_ID, 'rejected', 'url_swap');
  db.prepare(
    "INSERT INTO collector_repair (anomaly_id, source_id, status, action) VALUES (?,?,?,?)",
  ).run(freshTriagedAnomaly(), SRC_ID, 'error', 'url_swap');
  assert(countRecentFailedRepairs(SRC_ID, 24) === 2, 'expected 2 prior failures');
  assert(!getActiveQuarantine(SRC_ID), 'not quarantined yet');

  const id3 = freshTriagedAnomaly();
  const out3 = await attemptRepair(id3); // gate fails (new url unreachable) -> reject -> breaker trips
  console.log('A third attempt ->', out3);
  assert(out3 && out3.status === 'rejected', `expected rejected, got ${JSON.stringify(out3)}`);
  const q = getActiveQuarantine(SRC_ID);
  assert(q, 'source should be quarantined after 3 failed cycles');
  assert(q.quarantine_reason?.includes('breaker'), `reason: ${q.quarantine_reason}`);
  console.log('A quarantined:', q.quarantine_reason, 'until', q.quarantined_until);

  // While quarantined the worker skips it entirely (no LLM call, null).
  const id4 = freshTriagedAnomaly();
  const skipped = await attemptRepair(id4);
  assert(skipped === null, 'quarantined source must be skipped (null)');
  assert(
    db.prepare('SELECT COUNT(*) n FROM collector_repair WHERE anomaly_id = ?').get(id4).n === 0,
    'no repair row while quarantined',
  );
  console.log('A skipped while quarantined: OK');

  // Half-open: simulate the cooldown elapsing — flag stays, active=false.
  db.prepare(
    "UPDATE sources SET quarantined_until = datetime('now','-1 hours') WHERE id = ?",
  ).run(SRC_ID);
  assert(!getActiveQuarantine(SRC_ID), 'past cooldown -> not active (half-open)');
  const listed = getQuarantinedSources().find((s) => s.id === SRC_ID);
  assert(listed && listed.active === 0, 'half-open still listed with active=0');

  // A healthy probe clears it.
  clearQuarantine(SRC_ID);
  assert(!getQuarantinedSources().some((s) => s.id === SRC_ID), 'cleared after healthy run');
  console.log('A half-open + clear: OK');

  // ── B. shared concurrency: heavy tier limit is 1 ──
  let cur = 0;
  let peak = 0;
  const job = () =>
    withLlmSlot('heavy', async () => {
      cur += 1;
      peak = Math.max(peak, cur);
      await new Promise((r) => setTimeout(r, 30));
      cur -= 1;
    });
  await Promise.all([job(), job(), job(), job()]);
  console.log('B heavy peak concurrency =', peak);
  assert(peak === 1, `heavy tier must serialize, peak was ${peak}`);
  const snap = llmConcurrencySnapshot();
  assert(snap.heavy.limit === 1 && snap.mid.limit >= 1, 'snapshot shape');
  assert(snap.heavy.inflight === 0 && snap.heavy.waiting === 0, 'slots released');

  // ── C. digest shape ──
  quarantineSource(SRC_ID, 24, 'breaker: digest check');
  const d = buildMaintenanceDigest({ hours: 24 });
  for (const k of [
    'generated_at',
    'window_hours',
    'totals',
    'success_by_class',
    'worst_sources',
    'quarantined',
    'auto_fixed',
    'awaiting_review',
    'needs_human',
    'concurrency',
  ]) {
    assert(k in d, `digest missing key ${k}`);
  }
  assert(d.totals.rejected >= 1, `expected rejected>=1, got ${d.totals.rejected}`);
  assert(
    d.quarantined.some((x) => x.source_id === SRC_ID && x.active === true),
    'digest should list the quarantined source as active',
  );
  const cls = d.success_by_class.find((c) => c.class === 'url_move');
  assert(cls && cls.fail >= 1, 'url_move class should show a failure');
  console.log('C digest:', JSON.stringify({ totals: d.totals, quarantined: d.quarantined.length }));

  cleanup();
  upsertSource({
    id: SRC_ID,
    name: REG.name,
    type: REG.type,
    category: REG.category,
    url: REG.url,
    status: REG.status || 'offline',
  });
  mockLlm.close();
  console.log('\nPhase 4 smoke: OK');
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
    process.exit(1);
  });
