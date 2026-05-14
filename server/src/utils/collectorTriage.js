import {
  getUntriagedAnomalies,
  getAnomalyById,
  getSourceById,
  getLogsBySourceId,
  recordTriage,
} from './database.js';
import { getFixtureMeta, loadFixture } from './collectorFixtures.js';
import { chat as llmChat } from './llmClient.js';

const TRIAGE_MODEL = process.env.LLM_TRIAGE_MODEL || undefined;
const TRIAGE_TIMEOUT_MS = Number(process.env.LLM_TRIAGE_TIMEOUT_MS || 30_000);
const TRIAGE_POLL_MS = Number(process.env.TRIAGE_POLL_MS || 60_000);
const TRIAGE_BATCH = Number(process.env.TRIAGE_BATCH || 5);
const REFETCH_TIMEOUT_MS = Number(process.env.TRIAGE_REFETCH_TIMEOUT_MS || 10_000);
const REFETCH_BODY_CAP = 2000;

const TRIAGE_CLASSES = [
  'transient',
  'url_move',
  'selector_drift',
  'auth_break',
  'rate_limit',
  'site_dead',
  'structural',
  'unknown',
];

const TRIAGE_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['class', 'confidence', 'evidence'],
  properties: {
    class: { type: 'string', enum: TRIAGE_CLASSES },
    confidence: { type: 'number', minimum: 0, maximum: 1 },
    evidence: { type: 'string', maxLength: 500 },
    suggested_fix: {
      type: ['object', 'null'],
      additionalProperties: false,
      required: ['kind', 'details'],
      properties: {
        kind: { type: 'string', maxLength: 40 },
        details: { type: 'string', maxLength: 500 },
      },
    },
  },
};

const SYSTEM_PROMPT = `You triage failures in a Japan OSINT data-collector fleet. Each input describes one anomaly on one collector and includes its registry metadata, recent fetch history, a snapshot of the last-known-good payload, and a fresh re-fetch of the source URL.

Pick exactly one class:
- transient: random network blip, slow upstream, will likely recover
- url_move: URL or domain redirected / 404s / moved permanently
- selector_drift: scraper-only — HTML structure changed, selectors no longer match
- auth_break: API key rejected, 401/403, credential rotation needed
- rate_limit: 429, quota exceeded, throttle response
- site_dead: domain gone, host unreachable, certificate dead, project shut down
- structural: site or API restructured — pagination changed, schema changed, response envelope changed
- unknown: insufficient evidence to classify

Output JSON only. Be conservative with confidence: 0.9+ only when the evidence in the bundle is unambiguous. Provide a one-sentence evidence string citing the specific signal that drove the verdict. If you can sketch a fix (URL swap, header change, selector update), include it in suggested_fix; otherwise null.`;

function truncate(s, n) {
  if (!s) return null;
  return s.length <= n ? s : `${s.slice(0, n)}...[truncated]`;
}

async function refetchUrl(url) {
  if (!url) return { ok: false, error: 'no url in registry' };
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), REFETCH_TIMEOUT_MS);
  const start = Date.now();
  try {
    const res = await fetch(url, {
      signal: controller.signal,
      redirect: 'follow',
      headers: {
        accept: '*/*',
        'user-agent': 'JapanOSINT-triage/1.0',
      },
    });
    clearTimeout(timer);
    const body = await res.text().catch(() => '');
    return {
      ok: res.ok,
      status: res.status,
      duration_ms: Date.now() - start,
      content_type: res.headers.get('content-type'),
      final_url: res.url,
      body_head: truncate(body, REFETCH_BODY_CAP),
      body_bytes: Buffer.byteLength(body),
    };
  } catch (err) {
    clearTimeout(timer);
    return {
      ok: false,
      duration_ms: Date.now() - start,
      error: err?.name === 'AbortError' ? `timeout ${REFETCH_TIMEOUT_MS}ms` : err?.message,
    };
  }
}

function buildBundle({ anomaly, source, logs, fixture, fixtureMeta, refetch }) {
  return {
    anomaly: {
      verdict: anomaly.verdict,
      reason: anomaly.reason,
      evidence: anomaly.evidence,
      created_at: anomaly.created_at,
      escalation_level: anomaly.escalation_level,
    },
    source: {
      id: source.id,
      name: source.name,
      type: source.type,
      category: source.category,
      url: source.url,
      status: source.status,
      last_success: source.last_success,
      error_message: source.error_message,
      response_time_ms: source.response_time_ms,
    },
    recent_runs: logs.map((r) => ({
      ts: r.timestamp,
      status: r.status,
      records: r.records_fetched,
      duration_ms: r.duration_ms,
      error: r.error,
    })),
    fixture: fixtureMeta
      ? {
          captured_at: fixtureMeta.captured_at,
          content_type: fixtureMeta.content_type,
          records_count: fixtureMeta.records_count,
          raw_bytes: fixtureMeta.raw_bytes,
          raw_head: fixture ? truncate(String(fixture.raw), REFETCH_BODY_CAP) : null,
        }
      : null,
    current_fetch: refetch,
  };
}

/**
 * Triage one anomaly: build context, prompt 12B, persist verdict. Returns
 * the recorded triage row, or null if the LLM call failed (anomaly stays
 * untriaged — next worker tick will retry).
 */
export async function triageOne(anomalyId) {
  const anomaly = getAnomalyById(anomalyId);
  if (!anomaly) return null;
  if (anomaly.resolved_at) return null;
  if (anomaly.triaged_at) return null;

  const source = getSourceById(anomaly.source_id);
  if (!source) return null;

  const logs = getLogsBySourceId(source.id, 10);
  const fixtureMeta = getFixtureMeta(source.id);
  const fixture = fixtureMeta ? loadFixture(source.id) : null;
  const refetch = await refetchUrl(source.url);

  const bundle = buildBundle({ anomaly, source, logs, fixture, fixtureMeta, refetch });

  const messages = [
    { role: 'system', content: SYSTEM_PROMPT },
    { role: 'user', content: JSON.stringify(bundle) },
  ];

  const verdict = await llmChat({
    messages,
    jsonSchema: TRIAGE_SCHEMA,
    timeoutMs: TRIAGE_TIMEOUT_MS,
    model: TRIAGE_MODEL,
  });

  if (!verdict) {
    console.warn(`[triage] LLM returned null for anomaly #${anomalyId} (source ${source.id})`);
    return null;
  }

  recordTriage({
    id: anomalyId,
    triage_class: verdict.class,
    triage_confidence: verdict.confidence,
    triage_evidence: verdict.evidence,
    triage_suggested_fix: verdict.suggested_fix ? JSON.stringify(verdict.suggested_fix) : null,
    triage_model: TRIAGE_MODEL || 'default',
  });

  return {
    id: anomalyId,
    source_id: source.id,
    ...verdict,
  };
}

let workerRunning = false;
let workerTimer = null;

async function workerTick() {
  if (workerRunning) return; // re-entrancy guard for the polling timer
  workerRunning = true;
  try {
    const pending = getUntriagedAnomalies(TRIAGE_BATCH);
    for (const a of pending) {
      try {
        await triageOne(a.id);
      } catch (err) {
        console.warn(`[triage] worker crashed on anomaly #${a.id}: ${err?.message}`);
      }
    }
  } finally {
    workerRunning = false;
  }
}

/**
 * Boot the triage polling loop. No-op when LLM_ENABLED isn't 'true' so
 * the worker only runs in environments wired up to an LM Studio server.
 * Returns a stop() handle for tests / clean shutdown.
 */
export function startTriageWorker() {
  if (workerTimer) return { stop: stopTriageWorker };
  if (process.env.LLM_ENABLED !== 'true') {
    console.log('[triage] worker disabled (LLM_ENABLED!=true)');
    return { stop: () => {} };
  }
  console.log(`[triage] worker started, poll=${TRIAGE_POLL_MS}ms batch=${TRIAGE_BATCH}`);
  workerTimer = setInterval(() => {
    workerTick().catch((err) => console.warn(`[triage] tick error: ${err?.message}`));
  }, TRIAGE_POLL_MS);
  // First tick promptly so a hot anomaly doesn't wait a full poll interval.
  setTimeout(() => workerTick().catch(() => {}), 1000);
  return { stop: stopTriageWorker };
}

export function stopTriageWorker() {
  if (workerTimer) {
    clearInterval(workerTimer);
    workerTimer = null;
  }
}
