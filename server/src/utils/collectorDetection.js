import {
  getRecordsBaseline,
  getLogsBySourceId,
  recordAnomaly,
  hasOpenAnomalyOfVerdict,
} from './database.js';
import { chat as llmChat } from './llmClient.js';

// 4B classifier model. Defaults to the same LLM_MODEL the rest of the
// codebase uses; in production set LLM_DETECT_MODEL to a small fast model
// (Gemma 3 4B) so the bulk path doesn't tie up the bigger triage model.
const DETECT_MODEL = process.env.LLM_DETECT_MODEL || undefined;
const DETECT_TIMEOUT_MS = Number(process.env.LLM_DETECT_TIMEOUT_MS || 8_000);

// Hard cap on parallel sanity-classifier calls. Inference on a 128GB DDR5
// CPU box is memory-bandwidth bound; running 308 probes' worth of LLM calls
// at once would crater throughput. Two slots keeps the box responsive while
// letting the scheduler fan out other work.
const MAX_DETECT_CONCURRENCY = Number(process.env.LLM_DETECT_CONCURRENCY || 2);
let detectInflight = 0;

async function withDetectSlot(fn) {
  while (detectInflight >= MAX_DETECT_CONCURRENCY) {
    await new Promise((r) => setTimeout(r, 100));
  }
  detectInflight += 1;
  try {
    return await fn();
  } finally {
    detectInflight -= 1;
  }
}

// Drop sigma threshold. records_fetched below (mean - K*stddev) trips
// records_drop. K=2 catches the ~2.5% lower tail under a normal assumption,
// which for real collector traffic is a comfortable "something genuinely
// changed" signal.
const RECORDS_DROP_SIGMA = 2;

// Fixed duration threshold. The probe timeout in scheduler.js is 15s, so
// 10s is "succeeded but the upstream is sluggish enough that next time it
// might time out." No history needed — same threshold for every collector.
const DURATION_OUTLIER_MS = Number(process.env.DETECT_DURATION_OUTLIER_MS || 10_000);

// Per-detector evidence is JSON-stringified into the anomaly row. Keep it
// small so the queue UI stays readable.
function evidenceJson(obj) {
  try {
    return JSON.stringify(obj);
  } catch {
    return null;
  }
}

function detectRecordsDrop({ source, recordsCount }) {
  if (recordsCount == null) return null;
  const baseline = getRecordsBaseline(source.id);
  if (baseline.mean == null || baseline.stddev == null) return null;
  // A pristine zero-variance baseline (every run returns the same count)
  // would flag any deviation as anomalous. Require a minimum absolute drop
  // so single-record additions to a tiny feed don't fire.
  const threshold = baseline.mean - RECORDS_DROP_SIGMA * baseline.stddev;
  if (recordsCount >= threshold) return null;
  if (Math.abs(recordsCount - baseline.mean) < 2) return null;
  return {
    verdict: 'records_drop',
    reason: `records_fetched=${recordsCount} below baseline mean ${baseline.mean.toFixed(1)} − ${RECORDS_DROP_SIGMA}σ (${baseline.stddev.toFixed(1)})`,
    evidence: evidenceJson({
      records_count: recordsCount,
      baseline_mean: baseline.mean,
      baseline_stddev: baseline.stddev,
      baseline_n: baseline.count,
    }),
  };
}

function detectDurationOutlier({ statusOk, duration }) {
  if (!statusOk) return null;
  if (typeof duration !== 'number') return null;
  if (duration <= DURATION_OUTLIER_MS) return null;
  return {
    verdict: 'duration_outlier',
    reason: `probe took ${duration}ms (threshold ${DURATION_OUTLIER_MS}ms)`,
    evidence: evidenceJson({ duration_ms: duration, threshold_ms: DURATION_OUTLIER_MS }),
  };
}

function detectStatusBad({ source, statusOk }) {
  if (statusOk) return null;
  // logFetch for this run has already been called, so the most recent two
  // rows are this run + the previous one.
  const recent = getLogsBySourceId(source.id, 2);
  if (recent.length < 2) return null;
  if (recent.every((r) => r.status !== 'online')) {
    return {
      verdict: 'status_bad',
      reason: `2 consecutive non-online runs: ${recent.map((r) => r.status).join(', ')}`,
      evidence: evidenceJson({
        recent_statuses: recent.map((r) => ({ status: r.status, ts: r.timestamp, error: r.error })),
      }),
    };
  }
  return null;
}

const SANITY_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['looks_correct', 'reason'],
  properties: {
    looks_correct: { type: 'boolean' },
    reason: { type: 'string', maxLength: 240 },
  },
};

function buildSanitySample(rawBody, recordsCount) {
  if (!rawBody || typeof rawBody !== 'string') return null;
  // Try JSON first — if the response parses as an array or array-bearing
  // envelope, grab a small head sample. Otherwise fall back to a raw text
  // prefix (works for HTML scrapers, XML, plain text).
  try {
    const parsed = JSON.parse(rawBody);
    let arr = null;
    if (Array.isArray(parsed)) arr = parsed;
    else if (parsed && typeof parsed === 'object') {
      arr = parsed.data ?? parsed.results ?? parsed.features ?? parsed.items ?? null;
    }
    if (Array.isArray(arr) && arr.length > 0) {
      const head = arr.slice(0, 3);
      return { kind: 'json_sample', text: JSON.stringify(head).slice(0, 1500), records_total: recordsCount };
    }
  } catch {
    // not JSON
  }
  return { kind: 'raw_prefix', text: rawBody.slice(0, 1500), records_total: recordsCount };
}

async function detectSanity({ source, rawBody, recordsCount, statusOk }) {
  if (!statusOk) return null;
  if (process.env.LLM_ENABLED !== 'true') return null;
  const sample = buildSanitySample(rawBody, recordsCount);
  if (!sample) return null;

  const messages = [
    {
      role: 'system',
      content:
        'You inspect a small sample of the raw response from a Japan OSINT data collector and decide whether it looks like a legitimate, useful payload for that collector. A bad sample looks like: an error page, a Cloudflare challenge, an empty/placeholder page, content in a totally unrelated domain, or obviously corrupted output. A good sample contains records that plausibly match the collector\'s stated purpose and category. Be permissive: legitimate-but-unusual content is fine. Output JSON only.',
    },
    {
      role: 'user',
      content: `Collector: ${source.name}\nCategory: ${source.category}\nType: ${source.type}\nRecords reported: ${recordsCount ?? 'unknown'}\nSample (${sample.kind}):\n${sample.text}\n\nDoes this look like a healthy payload for this collector? Respond with JSON: { "looks_correct": boolean, "reason": short string }`,
    },
  ];

  const verdict = await withDetectSlot(() =>
    llmChat({
      messages,
      jsonSchema: SANITY_SCHEMA,
      timeoutMs: DETECT_TIMEOUT_MS,
      model: DETECT_MODEL,
    }),
  );
  if (!verdict) return null;
  if (verdict.looks_correct) return null;
  return {
    verdict: 'sanity_failed',
    reason: verdict.reason?.slice(0, 240) || 'classifier flagged payload',
    evidence: evidenceJson({ sample_kind: sample.kind, sample_chars: sample.text.length }),
  };
}

/**
 * Run all Phase 1 detectors for a completed fetch run. Anomalies are
 * persisted via recordAnomaly with per-verdict suppression so a
 * chronically-broken source generates one row per failure mode, not one
 * per cron tick. Returns the list of newly-recorded anomalies (empty when
 * the run looks clean).
 *
 * Best-effort: any single detector failure is logged and skipped — never
 * propagates back to the scheduler.
 */
export async function evaluateRun({ source, statusOk, recordsCount, rawBody, duration }) {
  const findings = [];
  try {
    const drop = detectRecordsDrop({ source, recordsCount });
    if (drop) findings.push(drop);
  } catch (err) {
    console.warn(`[detect] records_drop failed for ${source.id}: ${err?.message}`);
  }
  try {
    const bad = detectStatusBad({ source, statusOk });
    if (bad) findings.push(bad);
  } catch (err) {
    console.warn(`[detect] status_bad failed for ${source.id}: ${err?.message}`);
  }
  try {
    const slow = detectDurationOutlier({ statusOk, duration });
    if (slow) findings.push(slow);
  } catch (err) {
    console.warn(`[detect] duration_outlier failed for ${source.id}: ${err?.message}`);
  }
  try {
    const sanity = await detectSanity({ source, rawBody, recordsCount, statusOk });
    if (sanity) findings.push(sanity);
  } catch (err) {
    console.warn(`[detect] sanity failed for ${source.id}: ${err?.message}`);
  }

  const recorded = [];
  for (const finding of findings) {
    if (hasOpenAnomalyOfVerdict(source.id, finding.verdict)) continue;
    try {
      const r = recordAnomaly({
        source_id: source.id,
        verdict: finding.verdict,
        reason: finding.reason,
        evidence: finding.evidence,
      });
      recorded.push({ id: r.lastInsertRowid, ...finding });
    } catch (err) {
      console.warn(`[detect] recordAnomaly failed for ${source.id}/${finding.verdict}: ${err?.message}`);
    }
  }
  return recorded;
}
