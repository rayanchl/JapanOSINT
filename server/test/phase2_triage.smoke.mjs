// Phase 2 smoke. Stands up a mock LLM HTTP server, points LLM_BASE_URL at
// it, drives triageOne against a fake anomaly, then cleans up.
//
//   node test/phase2_triage.smoke.mjs

import { createServer } from 'http';
import db, {
  upsertSource,
  recordAnomaly,
  getAnomalyById,
  resolveAnomaly,
} from '../src/utils/database.js';

const SRC_ID = '__phase2-smoke-source';

const mockLlm = createServer((req, res) => {
  let body = '';
  req.on('data', (c) => (body += c));
  req.on('end', () => {
    const payload = {
      choices: [
        {
          message: {
            content: JSON.stringify({
              class: 'url_move',
              confidence: 0.85,
              evidence: 'fresh re-fetch returned 404 while fixture captured a 200',
              suggested_fix: { kind: 'url_swap', details: 'try the /v2/ endpoint' },
            }),
          },
        },
      ],
    };
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify(payload));
  });
});

function cleanup() {
  db.prepare('DELETE FROM collector_anomaly WHERE source_id = ?').run(SRC_ID);
  db.prepare('DELETE FROM fetch_log WHERE source_id = ?').run(SRC_ID);
  db.prepare('DELETE FROM sources WHERE id = ?').run(SRC_ID);
}

async function run() {
  cleanup();

  upsertSource({
    id: SRC_ID,
    name: 'Phase 2 Smoke',
    type: 'api',
    category: 'test',
    url: 'http://127.0.0.1:1', // unreachable on purpose so refetch errors cleanly
    status: 'degraded',
  });

  const anom = recordAnomaly({
    source_id: SRC_ID,
    verdict: 'records_drop',
    reason: 'smoke-test seed',
    evidence: '{"baseline_mean":100,"records_count":2}',
  });
  const anomalyId = anom.lastInsertRowid;

  await new Promise((r) => mockLlm.listen(0, '127.0.0.1', r));
  const { port } = mockLlm.address();
  process.env.LLM_BASE_URL = `http://127.0.0.1:${port}`;
  process.env.LLM_MODEL = 'mock-model';

  const { triageOne } = await import('../src/utils/collectorTriage.js');
  const out = await triageOne(anomalyId);
  console.log('triage output:', out);
  if (!out) throw new Error('triageOne returned null');
  if (out.class !== 'url_move') throw new Error(`expected url_move, got ${out.class}`);

  const row = getAnomalyById(anomalyId);
  console.log('row after triage:', {
    class: row.triage_class,
    conf: row.triage_confidence,
    evidence: row.triage_evidence?.slice(0, 60),
    fix: row.triage_suggested_fix?.slice(0, 60),
    triaged_at: row.triaged_at,
    model: row.triage_model,
  });
  if (row.triage_class !== 'url_move') throw new Error('triage_class not persisted');
  if (!row.triaged_at) throw new Error('triaged_at not set');
  if (!row.triage_suggested_fix?.includes('url_swap')) throw new Error('suggested_fix not persisted');

  // Re-triage skip: already-triaged anomaly returns null without calling LLM.
  const second = await triageOne(anomalyId);
  console.log('re-triage (should be null):', second);
  if (second !== null) throw new Error('re-triage should skip already-triaged anomalies');

  // Resolved anomaly: never triage.
  const anom2 = recordAnomaly({ source_id: SRC_ID, verdict: 'manual', reason: 'resolved test' });
  resolveAnomaly(anom2.lastInsertRowid, 'pre-resolved');
  const skipResolved = await triageOne(anom2.lastInsertRowid);
  console.log('triage on resolved (should be null):', skipResolved);
  if (skipResolved !== null) throw new Error('should not triage resolved anomalies');

  cleanup();
  mockLlm.close();
  console.log('\nPhase 2 smoke: OK');
}

run()
  .then(() => process.exit(0))
  .catch((err) => {
    console.error('SMOKE FAIL:', err);
    cleanup();
    mockLlm.close();
    process.exit(1);
  });
