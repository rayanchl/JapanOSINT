// Smoke test for Phase 1 detection. Run with:
//   node test/phase1_detection.smoke.mjs
// Cleans up its own test source on exit.

import db from '../src/utils/database.js';
import {
  recordAnomaly,
  getOpenAnomalies,
  resolveAnomaly,
  hasOpenAnomalyOfVerdict,
  getRecordsBaseline,
  upsertSource,
  logFetch,
} from '../src/utils/database.js';
import { evaluateRun } from '../src/utils/collectorDetection.js';

const SRC_ID = '__phase1-smoke-source';

function cleanup() {
  db.prepare('DELETE FROM collector_anomaly WHERE source_id = ?').run(SRC_ID);
  db.prepare('DELETE FROM fetch_log WHERE source_id = ?').run(SRC_ID);
  db.prepare('DELETE FROM sources WHERE id = ?').run(SRC_ID);
}

function seedHealthyHistory(count, recordsPerRun) {
  for (let i = 0; i < count; i += 1) {
    logFetch({
      source_id: SRC_ID,
      status: 'online',
      records_fetched: recordsPerRun + Math.floor(Math.random() * 3),
      duration_ms: 100,
      error: null,
    });
  }
}

async function run() {
  cleanup();
  upsertSource({
    id: SRC_ID,
    name: 'Phase 1 Smoke Source',
    type: 'api',
    category: 'test',
    url: 'https://example.test/api',
    status: 'online',
  });

  // 1. Records baseline: seed 10 healthy runs at ~100 records
  seedHealthyHistory(10, 100);
  const baseline = getRecordsBaseline(SRC_ID);
  console.log('baseline:', baseline);

  // 2. Trigger records_drop: a run that returned 5 should fire
  await evaluateRun({
    source: { id: SRC_ID, name: 'Phase 1 Smoke Source', category: 'test', type: 'api' },
    statusOk: true,
    recordsCount: 5,
    rawBody: '{"items": [1,2,3,4,5]}',
    duration: 120,
  });
  logFetch({ source_id: SRC_ID, status: 'online', records_fetched: 5, duration_ms: 120, error: null });

  const openAfterDrop = getOpenAnomalies({ sourceId: SRC_ID });
  console.log('open after records_drop trigger:', openAfterDrop.map((a) => a.verdict));
  if (!openAfterDrop.some((a) => a.verdict === 'records_drop')) throw new Error('records_drop did not fire');

  // 3. Duplicate suppression: re-trigger same condition, count stays at 1
  await evaluateRun({
    source: { id: SRC_ID, name: 'Phase 1 Smoke Source', category: 'test', type: 'api' },
    statusOk: true,
    recordsCount: 5,
    rawBody: '{"items": [1,2,3,4,5]}',
    duration: 120,
  });
  const dropRows = getOpenAnomalies({ sourceId: SRC_ID }).filter((a) => a.verdict === 'records_drop');
  console.log('records_drop count after re-trigger (should be 1):', dropRows.length);
  if (dropRows.length !== 1) throw new Error('suppression failed');

  // 4. status_bad: two consecutive non-online runs
  logFetch({ source_id: SRC_ID, status: 'offline', records_fetched: 0, duration_ms: 50, error: 'first offline' });
  await evaluateRun({
    source: { id: SRC_ID, name: 'Phase 1 Smoke Source', category: 'test', type: 'api' },
    statusOk: false,
    recordsCount: 0,
    rawBody: null,
    duration: 50,
  });
  // Above evaluateRun ran with statusOk=false BUT it didn't write a fetch_log
  // — production scheduler logs first then calls evaluateRun. Emulate that:
  logFetch({ source_id: SRC_ID, status: 'offline', records_fetched: 0, duration_ms: 50, error: 'second offline' });
  await evaluateRun({
    source: { id: SRC_ID, name: 'Phase 1 Smoke Source', category: 'test', type: 'api' },
    statusOk: false,
    recordsCount: 0,
    rawBody: null,
    duration: 50,
  });
  const statusBad = hasOpenAnomalyOfVerdict(SRC_ID, 'status_bad');
  console.log('status_bad fired:', statusBad);
  if (!statusBad) throw new Error('status_bad did not fire');

  // 5. LLM sanity: without LM Studio running, the call returns null and
  // no sanity_failed anomaly is created (best-effort path, not an error).
  const hasSanity = hasOpenAnomalyOfVerdict(SRC_ID, 'sanity_failed');
  console.log('sanity_failed (should be false when LLM unreachable):', hasSanity);

  // 6. Resolve cleanup
  const all = getOpenAnomalies({ sourceId: SRC_ID });
  for (const a of all) resolveAnomaly(a.id, 'smoke-cleanup');
  const stillOpen = getOpenAnomalies({ sourceId: SRC_ID });
  console.log('open after resolve (should be 0):', stillOpen.length);
  if (stillOpen.length !== 0) throw new Error('resolveAnomaly failed');

  cleanup();
  console.log('\nPhase 1 smoke: OK');
}

run()
  .then(() => process.exit(0))
  .catch((err) => {
    console.error('SMOKE FAIL:', err);
    cleanup();
    process.exit(1);
  });
