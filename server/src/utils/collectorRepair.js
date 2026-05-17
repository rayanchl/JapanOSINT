// Phase 3 — repair sandbox.
//
// The triage worker (Phase 2) classifies an open anomaly. This worker acts
// on that classification:
//
//   transient   -> auto-dismiss if the source has since recovered
//   url_move     -> propose a single-URL swap on the source's registry entry
//   site_dead    -> same (a moved/replacement endpoint, if the model finds one)
//   everything   -> needs_human (selector/auth/rate-limit/structural fixes
//   else          touch parsing logic or credentials — out of this phase)
//
// A proposed URL swap is applied to an *ephemeral copy* of sourceRegistry.js
// and pushed through a deterministic, non-negotiable verification gate:
//
//   a. parse/lint clean        (node --check on the patched file)
//   b. diff scope              (exactly one line — this source's entry — moved)
//   c. records within ±50%     (re-fetch the new URL, count like the scheduler)
//   d. 4B sanity re-check      (the new payload still looks like this collector)
//
// Gate pass -> verified (REPAIR_GIT opens a branch/PR; REPAIR_AUTO_MERGE may
// fast-path the safe class). Gate fail -> the anomaly is re-escalated so the
// next triage runs at a higher tier. Every attempt is audited in
// collector_repair.
//
// Deliberate gaps (same spirit as Phase 2):
//   - Default mode is dry: the verified patch + gate result are recorded but
//     the repo working tree is NOT mutated. Set REPAIR_GIT=true to apply the
//     change in a real `git worktree`, REPAIR_PR=true to open a PR, and
//     REPAIR_AUTO_MERGE=true to fast-path the safe class. With no FTE the
//     conservative default (record, don't touch) is intentional — the
//     alternative is silent corruption.
//   - Only registry-level URL swaps are machine-applied. Selector/structural
//     fixes need collector-file edits and stay needs_human by design.
//   - The gate's re-fetch is a plain GET (no WAF fallback), inherited from
//     the triage module's choice — a challenge body is itself informative.

import { execFileSync } from 'child_process';
import { mkdtempSync, writeFileSync, readFileSync, rmSync } from 'fs';
import { tmpdir } from 'os';
import { resolve, dirname, join } from 'path';
import { fileURLToPath } from 'url';
import {
  getAnomalyById,
  getSourceById,
  getLogsBySourceId,
  getRecordsBaseline,
  getRepairCandidates,
  getRepairAttemptCount,
  getLatestHealthyRunAfter,
  recordRepair,
  reopenForRetriage,
  resolveAnomaly,
  countRecentFailedRepairs,
  quarantineSource,
  getActiveQuarantine,
  hasStagedRepair,
} from './database.js';
import { getFixtureMeta, loadFixture, captureFixture } from './collectorFixtures.js';
import { chat as llmChat } from './llmClient.js';
import { withLlmSlot } from './llmConcurrency.js';

const __dirname = dirname(fileURLToPath(import.meta.url));
const REGISTRY_PATH = resolve(__dirname, 'sourceRegistry.js');
const REPO_ROOT = resolve(__dirname, '../../..');
const REGISTRY_REL = 'server/src/utils/sourceRegistry.js';

const FAST_MODEL = process.env.LLM_REPAIR_MODEL || undefined;
const HARD_MODEL = process.env.LLM_REPAIR_MODEL_HARD || FAST_MODEL;
const ESCALATE_AT = Number(process.env.REPAIR_ESCALATE_AT || 1);
const MAX_ESCALATION = Number(process.env.REPAIR_MAX_ESCALATION || 3);
const MAX_ATTEMPTS = Number(process.env.REPAIR_MAX_ATTEMPTS || 4);
const REPAIR_TIMEOUT_MS = Number(process.env.LLM_REPAIR_TIMEOUT_MS || 60_000);
const SANITY_TIMEOUT_MS = Number(process.env.LLM_REPAIR_SANITY_TIMEOUT_MS || 8_000);
const POLL_MS = Number(process.env.REPAIR_POLL_MS || 120_000);
const BATCH = Number(process.env.REPAIR_BATCH || 3);
const REFETCH_TIMEOUT_MS = Number(process.env.REPAIR_REFETCH_TIMEOUT_MS || 15_000);
const RECORDS_TOLERANCE = Number(process.env.REPAIR_RECORDS_TOLERANCE || 0.5);
const GIT_ENABLED = process.env.REPAIR_GIT === 'true';
const PR_ENABLED = process.env.REPAIR_PR === 'true';
const AUTO_MERGE = process.env.REPAIR_AUTO_MERGE === 'true';
// Phase 4 circuit breaker: this many failed repair cycles within the window
// quarantines the source for the cooldown so the pod stops hammering it.
const BREAKER_THRESHOLD = Number(process.env.REPAIR_BREAKER_THRESHOLD || 3);
const BREAKER_WINDOW_H = Number(process.env.REPAIR_BREAKER_WINDOW_HOURS || 24);
const QUARANTINE_H = Number(process.env.REPAIR_QUARANTINE_HOURS || 24);
const BODY_CAP = 2000;

const ACTIONABLE = new Set(['url_move', 'site_dead']);
const NEEDS_HUMAN = new Set([
  'selector_drift',
  'structural',
  'auth_break',
  'rate_limit',
  'unknown',
]);

const PROPOSAL_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['action', 'rationale', 'confidence'],
  properties: {
    action: { type: 'string', enum: ['url_swap', 'no_fix'] },
    new_url: { type: ['string', 'null'], maxLength: 2048 },
    rationale: { type: 'string', maxLength: 500 },
    confidence: { type: 'number', minimum: 0, maximum: 1 },
  },
};

const SANITY_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['looks_correct', 'reason'],
  properties: {
    looks_correct: { type: 'boolean' },
    reason: { type: 'string', maxLength: 240 },
  },
};

const SYSTEM_PROMPT = `You repair a Japan OSINT data-collector fleet. One collector has an anomaly that triage classified as a moved or dead endpoint. Your ONLY available action is to replace the single source URL in that collector's registry entry. You cannot edit parsing code, headers, or credentials here.

Propose action "url_swap" with a concrete new_url ONLY when you have strong evidence of a specific replacement endpoint that will serve the same data (e.g. the fresh fetch shows a permanent redirect Location, the triage suggested_fix names a versioned path, an obvious http->https or host-rename, a documented API base change). The new_url must be a fully-qualified absolute http(s) URL.

If you cannot identify a concrete, same-data replacement with confidence, return action "no_fix" — guessing corrupts the collector silently, which is worse than leaving it broken. Output JSON only.`;

function truncate(s, n) {
  if (s == null) return null;
  const str = String(s);
  return str.length <= n ? str : `${str.slice(0, n)}...[truncated]`;
}

// Mirror scheduler.fetchSource's record-counting so the gate's notion of
// "how many records" matches what the live probe would log.
function countRecords(rawBody) {
  try {
    const json = JSON.parse(rawBody);
    if (Array.isArray(json)) return json.length;
    if (json && typeof json === 'object') {
      const data = json.data ?? json.results ?? json.features ?? json.items;
      return Array.isArray(data) ? data.length : 1;
    }
  } catch {
    /* not JSON */
  }
  return 0;
}

async function refetch(url) {
  if (!url) return { ok: false, error: 'no url' };
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), REFETCH_TIMEOUT_MS);
  const start = Date.now();
  try {
    const res = await fetch(url, {
      signal: controller.signal,
      redirect: 'follow',
      headers: { accept: '*/*', 'user-agent': 'JapanOSINT-repair/1.0' },
    });
    clearTimeout(timer);
    const body = await res.text().catch(() => '');
    return {
      ok: res.ok,
      status: res.status,
      duration_ms: Date.now() - start,
      content_type: res.headers.get('content-type'),
      final_url: res.url,
      body,
      records: countRecords(body),
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

function isCleanUrl(u) {
  if (typeof u !== 'string' || u.length === 0 || u.length > 2048) return false;
  // Must survive embedding in a single-quoted JS string literal untouched.
  if (/['\\\n\r]/.test(u)) return false;
  try {
    const parsed = new URL(u);
    return parsed.protocol === 'http:' || parsed.protocol === 'https:';
  } catch {
    return false;
  }
}

function escapeRe(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

/**
 * Build the patched registry source by swapping the `url:` value on the
 * single line that defines `sourceId`. Returns { content, oldUrl, lineNo }
 * or throws with a gate-friendly reason. Deterministic: the model only
 * supplies a string, never a diff — so diff scope is provable here, not
 * inferred after the fact.
 */
function buildPatchedRegistry(registrySrc, sourceId, newUrl) {
  const lines = registrySrc.split('\n');
  const idRe = new RegExp(`(?:^|[^\\w])id:\\s*'${escapeRe(sourceId)}'`);
  const urlRe = /(url:\s*')([^'\\]*)(')/;
  const hits = [];
  for (let i = 0; i < lines.length; i += 1) {
    if (idRe.test(lines[i])) hits.push(i);
  }
  if (hits.length === 0) throw new Error(`registry entry not found for "${sourceId}"`);
  if (hits.length > 1) throw new Error(`ambiguous: ${hits.length} entries match "${sourceId}"`);
  const idStart = hits[0];

  // Entries are object literals — single-line (`{ id: ..., url: ... },`) or
  // multi-line. Find the one `url:` line inside this entry's span. The span
  // runs from the id line to the object's closing brace (cap the scan so a
  // malformed file can't make us swallow a neighbouring entry).
  let urlIdx = -1;
  const MAX_SPAN = 40;
  for (let i = idStart; i < lines.length && i < idStart + MAX_SPAN; i += 1) {
    if (urlRe.test(lines[i])) {
      if (urlIdx !== -1) throw new Error(`multiple url fields in entry for "${sourceId}"`);
      urlIdx = i;
    }
    if (i > idStart && /^\s*\},?\s*$/.test(lines[i])) break; // end of multi-line entry
    if (/\}\s*,?\s*$/.test(lines[i]) && i >= idStart) break; // end of single-line entry
  }
  if (urlIdx === -1) throw new Error(`no url field on registry entry for "${sourceId}"`);

  const m = lines[urlIdx].match(urlRe);
  const oldUrl = m[2];
  if (oldUrl === newUrl) throw new Error('proposed url equals current url');
  lines[urlIdx] = lines[urlIdx].replace(urlRe, `$1${newUrl}$3`);
  const idx = urlIdx;
  const content = lines.join('\n');

  // Independent diff-scope proof: exactly one line differs, and it is idx.
  const before = registrySrc.split('\n');
  let changed = 0;
  let changedAt = -1;
  for (let i = 0; i < before.length; i += 1) {
    if (before[i] !== lines[i]) {
      changed += 1;
      changedAt = i;
    }
  }
  if (changed !== 1 || changedAt !== idx) {
    throw new Error(`diff scope violation: ${changed} line(s) changed`);
  }
  return { content, oldUrl, lineNo: idx + 1 };
}

// Syntax-check the patched registry. Written as .mjs so a bare `node
// --check` parses it as ESM (`export default` would be a CJS SyntaxError).
function lintRegistry(patchedContent) {
  const dir = mkdtempSync(join(tmpdir(), 'repair-lint-'));
  const file = join(dir, 'sourceRegistry.mjs');
  try {
    writeFileSync(file, patchedContent);
    execFileSync(process.execPath, ['--check', file], { stdio: 'pipe' });
    return { ok: true };
  } catch (err) {
    return { ok: false, error: (err?.stderr?.toString() || err?.message || 'lint failed').slice(0, 500) };
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

async function sanityCheck(source, rawBody, recordsCount) {
  if (typeof rawBody !== 'string' || rawBody.length === 0) {
    return { ok: false, reason: 'empty body' };
  }
  let sample;
  try {
    const parsed = JSON.parse(rawBody);
    let arr = null;
    if (Array.isArray(parsed)) arr = parsed;
    else if (parsed && typeof parsed === 'object') {
      arr = parsed.data ?? parsed.results ?? parsed.features ?? parsed.items ?? null;
    }
    sample = Array.isArray(arr) && arr.length
      ? { kind: 'json_sample', text: JSON.stringify(arr.slice(0, 3)).slice(0, 1500) }
      : { kind: 'raw_prefix', text: rawBody.slice(0, 1500) };
  } catch {
    sample = { kind: 'raw_prefix', text: rawBody.slice(0, 1500) };
  }
  const verdict = await llmChat({
    messages: [
      {
        role: 'system',
        content:
          "You inspect a small sample of the raw response from a Japan OSINT data collector and decide whether it looks like a legitimate, useful payload for that collector. A bad sample looks like: an error page, a Cloudflare challenge, an empty/placeholder page, content in a totally unrelated domain, or obviously corrupted output. Be permissive: legitimate-but-unusual content is fine. Output JSON only.",
      },
      {
        role: 'user',
        content: `Collector: ${source.name}\nCategory: ${source.category}\nType: ${source.type}\nRecords reported: ${recordsCount ?? 'unknown'}\nSample (${sample.kind}):\n${sample.text}\n\nDoes this look like a healthy payload for this collector? JSON: { "looks_correct": boolean, "reason": short string }`,
      },
    ],
    jsonSchema: SANITY_SCHEMA,
    timeoutMs: SANITY_TIMEOUT_MS,
    model: process.env.LLM_DETECT_MODEL || undefined,
  });
  if (!verdict) return { ok: false, reason: 'sanity classifier unavailable' };
  return { ok: !!verdict.looks_correct, reason: verdict.reason?.slice(0, 240) || null };
}

function buildBundle({ anomaly, source, logs, fixtureMeta, fixture, current, candidate }) {
  return {
    anomaly: {
      verdict: anomaly.verdict,
      reason: anomaly.reason,
      evidence: anomaly.evidence,
      escalation_level: anomaly.escalation_level,
    },
    triage: {
      class: anomaly.triage_class,
      confidence: anomaly.triage_confidence,
      evidence: anomaly.triage_evidence,
      suggested_fix: anomaly.triage_suggested_fix
        ? safeParse(anomaly.triage_suggested_fix)
        : null,
    },
    source: {
      id: source.id,
      name: source.name,
      type: source.type,
      category: source.category,
      url: source.url,
      status: source.status,
      error_message: source.error_message,
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
          raw_head: fixture ? truncate(String(fixture.raw), BODY_CAP) : null,
        }
      : null,
    current_fetch: current
      ? {
          ok: current.ok,
          status: current.status,
          error: current.error,
          content_type: current.content_type,
          final_url: current.final_url,
          body_head: truncate(current.body, BODY_CAP),
        }
      : null,
    candidate_fetch: candidate
      ? {
          url: candidate.url,
          ok: candidate.ok,
          status: candidate.status,
          error: candidate.error,
          content_type: candidate.content_type,
          records: candidate.records,
          body_head: truncate(candidate.body, BODY_CAP),
        }
      : null,
  };
}

function safeParse(s) {
  try {
    return JSON.parse(s);
  } catch {
    return s;
  }
}

// Pull a candidate URL out of the triage suggested_fix, if it named one,
// so the model can see what that endpoint actually returns before deciding.
function candidateUrlFromTriage(anomaly) {
  if (!anomaly.triage_suggested_fix) return null;
  const fix = safeParse(anomaly.triage_suggested_fix);
  const blob = typeof fix === 'string' ? fix : JSON.stringify(fix || {});
  const m = blob.match(/https?:\/\/[^\s"'\\]+/);
  return m ? m[0] : null;
}

function sh(cwd, args) {
  return execFileSync('git', args, { cwd, stdio: 'pipe' }).toString().trim();
}

/**
 * Apply the verified patch in a throwaway git worktree, commit it, and
 * optionally open a PR / fast-path merge the safe class. Returns
 * { status, pr_url }. Only invoked after the gate has already passed.
 */
function applyViaGit({ source, anomaly, patched, oldUrl, newUrl }) {
  const branch = `repair/${source.id}-a${anomaly.id}`.replace(/[^a-zA-Z0-9/_-]/g, '-');
  const wt = mkdtempSync(join(tmpdir(), 'repair-wt-'));
  let prUrl = null;
  try {
    sh(REPO_ROOT, ['worktree', 'add', '--detach', wt, 'HEAD']);
    sh(wt, ['checkout', '-b', branch]);
    writeFileSync(join(wt, REGISTRY_REL), patched);
    // Defence in depth: git's own view must agree the change is one file.
    const touched = sh(wt, ['diff', '--name-only']).split('\n').filter(Boolean);
    if (touched.length !== 1 || touched[0] !== REGISTRY_REL) {
      throw new Error(`worktree diff touched ${touched.join(',') || 'nothing'}`);
    }
    execFileSync(process.execPath, ['--check', join(wt, REGISTRY_REL)], { stdio: 'pipe' });
    sh(wt, ['add', REGISTRY_REL]);
    sh(wt, [
      'commit',
      '-m',
      `fix(${source.id}): url swap ${oldUrl} -> ${newUrl}\n\nAuto-repair for anomaly #${anomaly.id} (triage: ${anomaly.triage_class}). Verified by the Phase 3 gate.`,
    ]);

    let status = 'verified';
    if (PR_ENABLED && hasGh()) {
      try {
        sh(wt, ['push', '-u', 'origin', branch]);
        prUrl = execFileSync(
          'gh',
          ['pr', 'create', '--fill', '--head', branch],
          { cwd: wt, stdio: 'pipe' },
        ).toString().trim();
      } catch (err) {
        console.warn(`[repair] PR creation failed for ${source.id}: ${err?.message}`);
      }
    }
    // Safe class = single-line URL swap in one file. Auto-merge stays
    // opt-in: with no FTE a PR just sits, which is the acceptable default.
    // Only merge into a CLEAN primary worktree — merging on top of
    // uncommitted work (e.g. an in-progress collector sweep) would entangle
    // the developer's tree or fail mid-merge. If it's dirty, leave the fix
    // as a branch/PR (status stays 'verified') and say why.
    if (AUTO_MERGE) {
      const dirty = sh(REPO_ROOT, ['status', '--porcelain']);
      if (dirty) {
        console.warn(
          `[repair] auto-merge skipped for ${source.id}: primary worktree dirty — fix left on branch ${branch}`,
        );
      } else {
        sh(REPO_ROOT, ['merge', '--no-ff', '-m', `auto-merge ${branch}`, branch]);
        status = 'merged';
      }
    }
    return { status, pr_url: prUrl };
  } finally {
    try {
      sh(REPO_ROOT, ['worktree', 'remove', '--force', wt]);
    } catch {
      rmSync(wt, { recursive: true, force: true });
    }
  }
}

function hasGh() {
  try {
    execFileSync('gh', ['--version'], { stdio: 'pipe' });
    return true;
  } catch {
    return false;
  }
}

/**
 * Trip the circuit breaker if this source has failed too many repair cycles
 * in the window. Quarantine makes the scheduler stop probing and this worker
 * skip the source until the cooldown lapses (half-open), at which point a
 * healthy probe clears it. Returns true if it just quarantined.
 */
function maybeQuarantine(source) {
  if (getActiveQuarantine(source.id)) return false; // already down
  const fails = countRecentFailedRepairs(source.id, BREAKER_WINDOW_H);
  if (fails < BREAKER_THRESHOLD) return false;
  const reason = `breaker: ${fails} failed repair cycles in ${BREAKER_WINDOW_H}h`;
  quarantineSource(source.id, QUARANTINE_H, reason);
  console.warn(`[repair] quarantined ${source.id} for ${QUARANTINE_H}h — ${reason}`);
  return true;
}

/**
 * Run one repair cycle for an anomaly. Returns a small result object
 * describing what happened, or null when the LLM was unreachable (no row
 * written — the next worker tick retries, same contract as triageOne).
 */
export async function attemptRepair(anomalyId) {
  const anomaly = getAnomalyById(anomalyId);
  if (!anomaly) return null;
  if (anomaly.resolved_at) return null;
  if (!anomaly.triaged_at) return null; // triage hasn't run yet

  const source = getSourceById(anomaly.source_id);
  if (!source) return null;

  // Circuit breaker: if this source is actively quarantined, the scheduler
  // has already stopped probing it — don't burn an LLM call re-attempting a
  // page we've agreed to back off from. The anomaly waits (still open) until
  // the cooldown lapses.
  const q = getActiveQuarantine(source.id);
  if (q) return null;

  // A verified fix is already staged for this anomaly but not yet live
  // (dry mode, or a branch/PR awaiting merge). Re-running the model would
  // just re-derive the same patch and pile up duplicate rows + LLM cost.
  // Park it: it stays open (so detection suppresses dupes) until the fix
  // merges (which resolves it) or a human acts.
  if (hasStagedRepair(anomalyId)) return null;

  const cls = anomaly.triage_class;

  // Give-up rails. Either too many escalation rounds or too many real
  // model attempts: stop hammering, hand to a human, leave the audit.
  const attempts = getRepairAttemptCount(anomalyId);
  if (anomaly.escalation_level >= MAX_ESCALATION || attempts >= MAX_ATTEMPTS) {
    const gate = JSON.stringify({
      reason: 'repair exhausted',
      escalation_level: anomaly.escalation_level,
      attempts,
    });
    recordRepair({
      anomaly_id: anomalyId,
      source_id: source.id,
      status: 'needs_human',
      action: 'exhausted',
      triage_class: cls,
      gate,
    });
    resolveAnomaly(anomalyId, `needs_human: repair exhausted (esc ${anomaly.escalation_level}, ${attempts} attempts)`);
    return { status: 'needs_human', source_id: source.id, reason: 'exhausted' };
  }

  // transient: only resolve if the source actually recovered on its own.
  if (cls === 'transient') {
    const healthy = getLatestHealthyRunAfter(source.id, anomaly.created_at);
    if (healthy) {
      recordRepair({
        anomaly_id: anomalyId,
        source_id: source.id,
        status: 'verified',
        action: 'auto_dismiss',
        triage_class: cls,
        gate: JSON.stringify({ recovered_run: healthy.timestamp, records: healthy.records_fetched }),
      });
      resolveAnomaly(anomalyId, `auto: transient recovered at ${healthy.timestamp}`);
      return { status: 'verified', source_id: source.id, action: 'auto_dismiss' };
    }
    // Still not healthy — maybe it isn't transient. Bump tier and re-triage.
    recordRepair({
      anomaly_id: anomalyId,
      source_id: source.id,
      status: 'rejected',
      action: 'await_recovery',
      triage_class: cls,
      gate: JSON.stringify({ reason: 'no healthy run since anomaly' }),
    });
    reopenForRetriage(anomalyId);
    return { status: 'rejected', source_id: source.id, action: 'await_recovery' };
  }

  // Classes whose fix lives in parsing logic / credentials / quota — not a
  // registry URL swap. Out of Phase 3 scope by design.
  if (NEEDS_HUMAN.has(cls)) {
    recordRepair({
      anomaly_id: anomalyId,
      source_id: source.id,
      status: 'needs_human',
      action: cls,
      triage_class: cls,
      gate: JSON.stringify({ reason: `class ${cls} not machine-fixable`, evidence: anomaly.triage_evidence }),
    });
    resolveAnomaly(anomalyId, `needs_human: ${cls} — ${truncate(anomaly.triage_evidence, 200) || 'see triage'}`);
    return { status: 'needs_human', source_id: source.id, action: cls };
  }

  if (!ACTIONABLE.has(cls)) {
    // Unknown/unhandled triage class — escalate rather than guess.
    reopenForRetriage(anomalyId);
    return { status: 'rejected', source_id: source.id, reason: `unhandled class ${cls}` };
  }

  // ---- url_swap flow ----
  const escalated = anomaly.escalation_level >= ESCALATE_AT;
  const model = escalated ? HARD_MODEL : FAST_MODEL;
  // 27B hard path -> 'heavy' (1 at a time), 12B fast path -> 'mid'.
  const tier = escalated ? 'heavy' : 'mid';
  const logs = getLogsBySourceId(source.id, 10);
  const fixtureMeta = getFixtureMeta(source.id);
  const fixture = fixtureMeta ? loadFixture(source.id) : null;
  const current = await refetch(source.url);

  const candUrl = candidateUrlFromTriage(anomaly);
  let candidate = null;
  if (candUrl && isCleanUrl(candUrl) && candUrl !== source.url) {
    const c = await refetch(candUrl);
    candidate = { url: candUrl, ...c };
  }

  const bundle = buildBundle({ anomaly, source, logs, fixtureMeta, fixture, current, candidate });

  const proposal = await withLlmSlot(tier, () =>
    llmChat({
      messages: [
        { role: 'system', content: SYSTEM_PROMPT },
        { role: 'user', content: JSON.stringify(bundle) },
      ],
      jsonSchema: PROPOSAL_SCHEMA,
      timeoutMs: REPAIR_TIMEOUT_MS,
      model,
    }),
  );

  if (!proposal) {
    console.warn(`[repair] LLM returned null for anomaly #${anomalyId} (source ${source.id})`);
    return null; // retry next tick, no row — same contract as triage
  }

  const reject = (reason, extraGate = {}) => {
    recordRepair({
      anomaly_id: anomalyId,
      source_id: source.id,
      status: 'rejected',
      action: 'url_swap',
      triage_class: cls,
      patch: JSON.stringify({ proposal }),
      gate: JSON.stringify({ failed: reason, ...extraGate }),
      model: model || 'default',
    });
    reopenForRetriage(anomalyId);
    maybeQuarantine(source);
    return { status: 'rejected', source_id: source.id, reason };
  };

  if (proposal.action !== 'url_swap' || !proposal.new_url) {
    return reject('model returned no_fix');
  }
  if (!isCleanUrl(proposal.new_url)) {
    return reject('proposed url failed validation', { new_url: truncate(proposal.new_url, 120) });
  }

  let patched;
  let oldUrl;
  let lineNo;
  try {
    ({ content: patched, oldUrl, lineNo } = buildPatchedRegistry(
      readFileSync(REGISTRY_PATH, 'utf8'),
      source.id,
      proposal.new_url,
    ));
  } catch (err) {
    return reject(`patch build: ${err.message}`);
  }

  // Gate (a): parse/lint clean.
  const lint = lintRegistry(patched);
  if (!lint.ok) return reject('lint failed', { lint: truncate(lint.error, 300) });

  // Gate (c): the new URL must actually serve data, within ±tolerance of
  // the rolling baseline (or just non-empty when there's no baseline yet).
  const verifyFetch = await refetch(proposal.new_url);
  if (!verifyFetch.ok) {
    return reject('new url did not return 2xx', {
      status: verifyFetch.status ?? null,
      error: verifyFetch.error ?? null,
    });
  }
  const baseline = getRecordsBaseline(source.id);
  const got = verifyFetch.records;
  if (baseline.mean != null) {
    const lo = baseline.mean * (1 - RECORDS_TOLERANCE);
    const hi = baseline.mean * (1 + RECORDS_TOLERANCE);
    if (got < lo || got > hi) {
      return reject('records outside ±tolerance of baseline', {
        records: got,
        baseline_mean: baseline.mean,
        tolerance: RECORDS_TOLERANCE,
      });
    }
  } else if (got <= 0) {
    return reject('new url returned zero records and no baseline to compare', { records: got });
  }

  // Gate (d): 4B sanity re-check on the new payload.
  const sanity = await sanityCheck(source, verifyFetch.body, got);
  if (!sanity.ok) return reject('sanity re-check failed', { sanity: sanity.reason });

  const gatePass = {
    lint: 'ok',
    diff_scope: `1 line (registry L${lineNo})`,
    records: got,
    baseline_mean: baseline.mean,
    sanity: sanity.reason || 'ok',
    new_url: proposal.new_url,
    final_url: verifyFetch.final_url,
  };

  // Gate passed. Apply (git path) or just record (dry default).
  let status = 'verified';
  let prUrl = null;
  if (GIT_ENABLED) {
    try {
      const applied = applyViaGit({
        source,
        anomaly,
        patched,
        oldUrl,
        newUrl: proposal.new_url,
      });
      status = applied.status;
      prUrl = applied.pr_url;
    } catch (err) {
      // The change verified but couldn't be applied — record it as an
      // error (not rejected: the fix is sound, the plumbing failed) and
      // leave the anomaly open for a retry / human.
      recordRepair({
        anomaly_id: anomalyId,
        source_id: source.id,
        status: 'error',
        action: 'url_swap',
        triage_class: cls,
        patch: JSON.stringify({ file: REGISTRY_REL, old_url: oldUrl, new_url: proposal.new_url }),
        gate: JSON.stringify({ ...gatePass, apply_error: err?.message }),
        model: model || 'default',
      });
      return { status: 'error', source_id: source.id, reason: `apply: ${err?.message}` };
    }
  }

  recordRepair({
    anomaly_id: anomalyId,
    source_id: source.id,
    status,
    action: 'url_swap',
    triage_class: cls,
    patch: JSON.stringify({ file: REGISTRY_REL, line: lineNo, old_url: oldUrl, new_url: proposal.new_url }),
    gate: JSON.stringify(gatePass),
    model: model || 'default',
    pr_url: prUrl,
  });

  // Re-capture the golden fixture only when the change is actually live
  // (merged). In dry/PR-pending mode the collector still uses the old URL,
  // so overwriting the fixture would desync it from reality.
  if (status === 'merged') {
    try {
      captureFixture({
        sourceId: source.id,
        rawBody: verifyFetch.body,
        contentType: verifyFetch.content_type,
        statusCode: verifyFetch.status,
        recordsCount: got,
        sourceUrl: proposal.new_url,
      });
    } catch (err) {
      console.warn(`[repair] fixture re-capture failed for ${source.id}: ${err?.message}`);
    }
  }

  // Resolve ONLY when the fix is actually live. `merged` means the URL was
  // applied to the registry. `verified` means the patch passed the gate but
  // isn't live yet (dry mode, or a branch/PR awaiting merge) — resolving
  // here would let the still-broken source churn fresh anomalies every
  // probe. Instead leave it open: the hasStagedRepair guard parks it, the
  // open anomaly suppresses duplicate detections, and the digest's
  // awaiting_review bucket surfaces it for a human / the merge to finish.
  const parked = status !== 'merged';
  if (!parked) {
    resolveAnomaly(
      anomalyId,
      `auto-repair url_swap → ${proposal.new_url} (${status}${prUrl ? `, ${prUrl}` : ''})`,
    );
  }
  return {
    status,
    source_id: source.id,
    action: 'url_swap',
    new_url: proposal.new_url,
    pr_url: prUrl,
    parked,
  };
}

let workerRunning = false;
let workerTimer = null;

async function workerTick() {
  if (workerRunning) return; // re-entrancy guard for the polling timer
  workerRunning = true;
  try {
    const candidates = getRepairCandidates(BATCH);
    for (const a of candidates) {
      try {
        await attemptRepair(a.id);
      } catch (err) {
        console.warn(`[repair] worker crashed on anomaly #${a.id}: ${err?.message}`);
        try {
          recordRepair({
            anomaly_id: a.id,
            source_id: a.source_id,
            status: 'error',
            action: 'crash',
            triage_class: a.triage_class || null,
            gate: JSON.stringify({ error: err?.message }),
          });
        } catch {
          /* best-effort audit */
        }
      }
    }
  } finally {
    workerRunning = false;
  }
}

/**
 * Boot the repair polling loop. No-op unless LLM_ENABLED is 'true', same as
 * the triage worker. Returns a stop() handle for tests / clean shutdown.
 */
export function startRepairWorker() {
  if (workerTimer) return { stop: stopRepairWorker };
  if (process.env.LLM_ENABLED !== 'true') {
    console.log('[repair] worker disabled (LLM_ENABLED!=true)');
    return { stop: () => {} };
  }
  console.log(
    `[repair] worker started, poll=${POLL_MS}ms batch=${BATCH} git=${GIT_ENABLED} pr=${PR_ENABLED} automerge=${AUTO_MERGE}`,
  );
  workerTimer = setInterval(() => {
    workerTick().catch((err) => console.warn(`[repair] tick error: ${err?.message}`));
  }, POLL_MS);
  setTimeout(() => workerTick().catch(() => {}), 1500);
  return { stop: stopRepairWorker };
}

export function stopRepairWorker() {
  if (workerTimer) {
    clearInterval(workerTimer);
    workerTimer = null;
  }
}

// Every maintenance-pod env knob, grouped, with its default. `npm start`
// takes no flags, so this is how an operator discovers what they *can*
// set — printed once at boot whether or not the pod is enabled.
const POD_OPTIONS = [
  ['global', [
    ['LLM_ENABLED', 'false', 'master switch — detection/triage/repair all no-op unless "true"'],
    ['LLM_BASE_URL', 'http://localhost:1234', 'LM Studio (OpenAI-compatible) base URL'],
    ['LLM_MODEL', 'local-model', 'fallback model id when a stage model isn\'t set'],
  ]],
  ['detection (Phase 1, 4B)', [
    ['LLM_DETECT_MODEL', '<LLM_MODEL>', 'small/fast sanity classifier'],
    ['LLM_DETECT_TIMEOUT_MS', '8000', 'sanity classifier timeout'],
    ['LLM_DETECT_CONCURRENCY', '2', 'max parallel sanity calls'],
    ['DETECT_DURATION_OUTLIER_MS', '10000', 'slow-but-ok probe threshold'],
  ]],
  ['triage (Phase 2, 12B)', [
    ['LLM_TRIAGE_MODEL', '<LLM_MODEL>', 'classifier model'],
    ['LLM_TRIAGE_TIMEOUT_MS', '30000', 'triage LLM timeout'],
    ['TRIAGE_POLL_MS', '60000', 'queue poll interval'],
    ['TRIAGE_BATCH', '5', 'anomalies per tick'],
    ['TRIAGE_REFETCH_TIMEOUT_MS', '10000', 're-fetch timeout'],
  ]],
  ['repair (Phase 3, 12B/27B)', [
    ['LLM_REPAIR_MODEL', '<LLM_MODEL>', 'fast-path repair model (12B)'],
    ['LLM_REPAIR_MODEL_HARD', '<LLM_REPAIR_MODEL>', 'escalated repair model (27B)'],
    ['REPAIR_ESCALATE_AT', '1', 'escalation_level at which the hard model takes over'],
    ['REPAIR_MAX_ESCALATION', '3', 'give up -> needs_human past this'],
    ['REPAIR_MAX_ATTEMPTS', '4', 'per-anomaly model attempts before needs_human'],
    ['LLM_REPAIR_TIMEOUT_MS', '60000', 'repair proposal LLM timeout'],
    ['LLM_REPAIR_SANITY_TIMEOUT_MS', '8000', 'post-fix 4B sanity re-check timeout'],
    ['REPAIR_POLL_MS', '120000', 'queue poll interval'],
    ['REPAIR_BATCH', '3', 'anomalies per tick'],
    ['REPAIR_REFETCH_TIMEOUT_MS', '15000', 'gate re-fetch timeout'],
    ['REPAIR_RECORDS_TOLERANCE', '0.5', 'allowed ± fraction vs baseline mean'],
    ['REPAIR_GIT', 'false', 'apply verified fix on a throwaway git worktree branch'],
    ['REPAIR_PR', 'false', 'also open a GitHub PR (needs gh + REPAIR_GIT)'],
    ['REPAIR_AUTO_MERGE', 'false', 'fast-path-merge the safe class (needs REPAIR_GIT)'],
  ]],
];

/**
 * Print the full maintenance-pod option matrix at boot. `current` shows the
 * effective value (env override or default); the value column makes it
 * obvious what's actually in force this run.
 */
export function logMaintenancePodConfig() {
  const enabled = process.env.LLM_ENABLED === 'true';
  const mode = !enabled
    ? 'OFF (LLM_ENABLED!=true)'
    : AUTO_MERGE
      ? 'LIVE auto-merge'
      : PR_ENABLED
        ? 'PR (review-gated)'
        : GIT_ENABLED
          ? 'git branch (no PR)'
          : 'DRY (record only — repo untouched)';
  const lines = [
    '────────────────────────────────────────────────────────────────',
    ` Maintenance pod — mode: ${mode}`,
    ' Env knobs (NAME = current value  [default])  •  npm start takes no flags',
    '────────────────────────────────────────────────────────────────',
  ];
  for (const [group, knobs] of POD_OPTIONS) {
    lines.push(` ${group}`);
    for (const [name, def, desc] of knobs) {
      const cur = process.env[name];
      const shown = cur != null && cur !== '' ? cur : `(${def})`;
      lines.push(`   ${name} = ${shown}  — ${desc}`);
    }
  }
  lines.push('────────────────────────────────────────────────────────────────');
  console.log(lines.join('\n'));
}
