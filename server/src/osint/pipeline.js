// server/src/osint/pipeline.js
//
// JS port of osint_pipeline.c — the recursive multi-round OSINT pipeline.
// Node owns orchestration; llama.cpp (via llmClient) runs the verbatim
// prompts/grammars; the dispatcher runs Node service impls + the always-on
// JP corpus adapter so every pivot reaches Japan sources.
//
// Phases mirror progress_tracker.c: queued → gpt_analyzing → services_assigned
// → agents_working → preliminary_results → (followup_analyzing × ≤max_rounds)
// → aggregating → completed. Handler-based dedup + executed-combo dedup match
// the C pipeline.

import { randomUUID } from 'node:crypto';
import { complete, completeJSON } from '../utils/llmClient.js';
import { loadGrammar } from '../utils/grammars.js';
import { createAnalysisPrompt, createPhase2Prompt, createSuggestionsPrompt } from './prompts.js';
import { dispatchService, getServicesList, serviceFromName, handlerKey, isImplemented } from './dispatcher.js';
import { createRequest, getRequest } from './progressTracker.js';

const MAX_ROUNDS = Number(process.env.OSINT_MAX_ROUNDS || 5);

// Tolerant JSON extractor — pull the first balanced {...} (the grammar makes
// analysis output clean, but phase-2 is ungrammared like the C backend).
function extractJson(raw) {
  if (raw == null) return null;
  if (typeof raw === 'object') return raw;
  const s = String(raw);
  const start = s.indexOf('{');
  if (start < 0) return null;
  let depth = 0;
  for (let i = start; i < s.length; i += 1) {
    if (s[i] === '{') depth += 1;
    else if (s[i] === '}') { depth -= 1; if (depth === 0) { try { return JSON.parse(s.slice(start, i + 1)); } catch { return null; } } }
  }
  return null;
}

// Tolerant JSON-array extractor — pull the first balanced [...]. Mirrors
// extractJson() for the suggestions path, whose grammar emits an array.
function extractJsonArray(raw) {
  if (raw == null) return null;
  if (Array.isArray(raw)) return raw;
  const s = String(raw);
  const start = s.indexOf('[');
  if (start < 0) return null;
  let depth = 0;
  for (let i = start; i < s.length; i += 1) {
    if (s[i] === '[') depth += 1;
    else if (s[i] === ']') { depth -= 1; if (depth === 0) { try { return JSON.parse(s.slice(start, i + 1)); } catch { return null; } } }
  }
  return null;
}

/**
 * Suggestions endpoint. Uses the native /completion path (raw prompt + GBNF)
 * like every other ported pipeline step — NOT chat()/jinja. createSuggestions
 * Prompt() is a few-shot completion prompt; routing it through the harmony
 * chat template masked the channel tokens gpt-oss is primed to emit and
 * collapsed generation into dot-spam. Raw /completion has no harmony
 * scaffolding, so the grammar agrees with the model's natural continuation.
 */
export async function getSuggestions(query) {
  const raw = await complete({
    prompt: createSuggestionsPrompt(query),
    grammar: loadGrammar('suggestions') || undefined,
    maxTokens: 1024,
    temperature: 0.3,
    timeoutMs: 20_000,
  });
  const arr = extractJsonArray(raw);
  if (!Array.isArray(arr)) return [];
  return arr.filter((s) => typeof s === 'string').slice(0, 9);
}

/**
 * Run the full pipeline. Returns the RequestProgress immediately; the run
 * proceeds async and emits 'update'/'done'. Caller (route) wires SSE +
 * searchIngest to 'done'.
 *
 * @param {object} opts { requestId?, maxRounds?, deps? } — deps is a test seam
 *        ({ complete, completeJSON } overrides).
 */
export function runPipeline(query, opts = {}) {
  const requestId = opts.requestId || randomUUID();
  const maxRounds = opts.maxRounds ?? MAX_ROUNDS;
  const rp = createRequest(requestId, query, maxRounds);
  const deps = {
    complete: opts.deps?.complete || complete,
    completeJSON: opts.deps?.completeJSON || completeJSON,
  };
  // Fire-and-forget; SSE/ingest hang off rp's events.
  _execute(rp, query, maxRounds, deps).catch((err) => {
    rp.setThinking(`pipeline error: ${err?.message || err}`);
    rp.finish('error');
  });
  return rp;
}

async function _execute(rp, query, maxRounds, deps) {
  rp.setPhase('queued', 5);

  // ── Phase 1: analysis ────────────────────────────────────────────────
  rp.setPhase('gpt_analyzing', 15);
  const analysisRaw = await deps.complete({
    prompt: createAnalysisPrompt(query, getServicesList()),
    grammar: loadGrammar('osint_analysis') || undefined,
    maxTokens: 2048,
    temperature: 0.2,
    timeoutMs: 60_000,
  });
  const analysis = extractJson(analysisRaw) || { entities: [], recommended_services: [], chain_reason: false, analysis: '' };
  const queryEntities = Array.isArray(analysis.entities) ? analysis.entities : [];
  rp.setEntities(queryEntities);
  rp.setThinking(typeof analysis.analysis === 'string' ? analysis.analysis : '');

  // ── Phase 2: assign services ─────────────────────────────────────────
  rp.setPhase('services_assigned', 20);
  // (service, entity, entityType, sourceService) tasks. Always append the
  // JP corpus adapter per entity so Japan sources are inherent.
  const executed = new Set();           // `${handlerKey}|${entityLower}`
  const tasks = [];
  const addTask = (svc, entity, type, source) => {
    const canon = serviceFromName(svc);
    if (!canon || !entity) return;
    const key = `${handlerKey(canon)}|${String(entity).toLowerCase()}`;
    if (executed.has(key)) return;
    executed.add(key);
    tasks.push({ service: canon, entity, type: type || 'unknown', source: source || 'analysis' });
  };
  for (const e of queryEntities) {
    const svcs = Array.isArray(e.services) && e.services.length ? e.services : (analysis.recommended_services || []);
    for (const s of svcs) addTask(s, e.value, e.type, 'analysis');
    addTask('JP_CORPUS_LOOKUP', e.value, e.type, 'analysis');
  }
  if (tasks.length === 0 && query) {
    // No entities extracted — still run the corpus adapter on the raw query.
    addTask('JP_CORPUS_LOOKUP', query, 'keyword', 'query');
  }
  rp.assignServices([...new Set(tasks.map((t) => t.service))]);

  // ── Phase 3: dispatch (round 0) ──────────────────────────────────────
  rp.setPhase('agents_working', 25);
  const serviceResults = []; // [{name, entity, success, data, confidence}]
  await runTasks(rp, deps, tasks, serviceResults, 25, 60);
  rp.setPhase('preliminary_results', 60);

  // ── Phases 4+: recursive follow-up rounds ────────────────────────────
  let needMore = analysis.chain_reason === true;
  let round = 0;
  while (needMore && round < maxRounds) {
    round += 1;
    rp.setRound(round);
    rp.setPhase('followup_analyzing', Math.min(60 + round * 6, 85));
    const phase2Raw = await deps.completeJSON({
      prompt: createPhase2Prompt(query, JSON.stringify({ services: serviceResults }), getServicesList()),
      maxTokens: 2048,
      temperature: 0.2,
      timeoutMs: 60_000,
    });
    const phase2 = extractJson(phase2Raw) || { needs_newphase: false, chain_services: [] };
    const chain = Array.isArray(phase2.chain_services) ? phase2.chain_services : [];
    const roundTasks = [];
    for (const c of chain) {
      rp.addDiscovered(c.entity, c.entity_type || 'unknown', c.source_service || 'phase2');
      const canon = serviceFromName(c.service);
      if (!canon || !c.entity) continue;
      const key = `${handlerKey(canon)}|${String(c.entity).toLowerCase()}`;
      if (executed.has(key)) continue;
      executed.add(key);
      roundTasks.push({ service: canon, entity: c.entity, type: c.entity_type || 'unknown', source: c.source_service || 'phase2' });
      // Pivot every discovered entity through the JP corpus too.
      const jpKey = `JP_CORPUS_LOOKUP|${String(c.entity).toLowerCase()}`;
      if (!executed.has(jpKey)) { executed.add(jpKey); roundTasks.push({ service: 'JP_CORPUS_LOOKUP', entity: c.entity, type: c.entity_type || 'unknown', source: c.source_service || 'phase2' }); }
    }
    if (roundTasks.length === 0) break;
    rp.assignServices([...new Set([...rp.services_assigned, ...roundTasks.map((t) => t.service)])]);
    await runTasks(rp, deps, roundTasks, serviceResults, Math.min(60 + round * 6, 85), Math.min(66 + round * 6, 88));
    needMore = phase2.needs_newphase === true;
  }

  // ── Aggregate + complete ─────────────────────────────────────────────
  rp.setPhase('aggregating', 90);
  const okCount = serviceResults.filter((r) => r.success).length;
  const synthesis =
    `Investigated "${query}". Ran ${serviceResults.length} service call(s) across ` +
    `${rp.current_round + 1} round(s); ${okCount} returned data. ` +
    `${rp.all_entities.length} unique entit${rp.all_entities.length === 1 ? 'y' : 'ies'} in scope.`;
  rp.setResults({
    query,
    services: serviceResults.map((r) => ({
      name: r.name, entity: r.entity, success: r.success,
      confidence: r.confidence, data: r.data, error: r.error || null,
    })),
    synthesis,
    stats: rp.stats,
  });
  rp.preliminary_findings = synthesis;
  rp.finish('completed');
}

async function runTasks(rp, deps, tasks, sink, fromPct, toPct) {
  const total = tasks.length || 1;
  let done = 0;
  await Promise.all(tasks.map(async (t) => {
    rp.serviceStatus(t.service, 'running', `querying ${t.entity}`, undefined, t.entity);
    const res = await dispatchService(t.service, t.entity, t.type);
    sink.push({ name: t.service, entity: t.entity, success: res.success, data: res.data, confidence: res.confidence, error: res.error });
    done += 1;
    rp.progress_percent = Math.round(fromPct + (toPct - fromPct) * (done / total));
    if (!isImplemented(t.service)) rp.serviceStatus(t.service, 'skipped', 'not_implemented', 0, t.entity);
    else rp.serviceStatus(t.service, res.success ? 'completed' : 'failed', res.error || 'done', res.success ? 1 : 0, t.entity);
  }));
}

export { getRequest };
