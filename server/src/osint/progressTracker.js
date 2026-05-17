// server/src/osint/progressTracker.js
//
// JS port of progress_tracker.c. In-memory per-request progress with the
// EXACT snapshot JSON shape progress_to_json() emitted, so the ported
// OSINTsaas frontend searchStore consumes it unchanged. Each request is an
// EventEmitter the SSE route subscribes to; every mutation emits 'update'
// with a fresh snapshot (the C backend pushed the whole blob every 500ms —
// we push on change, which is strictly better and equivalent for the client).

import { EventEmitter } from 'node:events';

const requests = new Map(); // request_id -> RequestProgress

const PHASES = [
  'queued', 'image_analyzing', 'gpt_analyzing', 'services_assigned',
  'services_launching', 'agents_working', 'preliminary_results',
  'awaiting_review', 'followup_analyzing', 'aggregating', 'completed', 'error',
];

class RequestProgress extends EventEmitter {
  constructor(requestId, query, maxRounds = 5) {
    super();
    this.setMaxListeners(0);
    this.request_id = requestId;
    this.query = query;
    this.phase = 'queued';
    this.progress_percent = 0;
    this.gpt_thinking = '';
    this.total_results = 0;
    this.preliminary_findings = '';
    this.created_at = Math.floor(Date.now() / 1000);
    this.updated_at = this.created_at;
    this.entities = [];                 // [{value,type}]
    this.services_assigned = [];        // [name]
    this.services = [];                 // [{name,status,results_count,status_message,is_followup,entities}]
    this.stats = { total_services: 0, active_services: 0, completed_services: 0, failed_services: 0, skipped_services: 0 };
    this.phase_history = [];
    this.current_round = 0;
    this.max_rounds = maxRounds;
    this.awaiting_user_action = false;
    this.discovered_entities = [];      // [{value,type,discovered_by}]
    this.confirmed_entities = [];
    this.all_entities = [];             // unified dedup [{value,type}]
    this.results_json = null;
    this.done = false;
  }

  _touch() { this.updated_at = Math.floor(Date.now() / 1000); this.emit('update', this.toJSON()); }

  setPhase(phase, percent) {
    if (!PHASES.includes(phase)) phase = 'unknown';
    this.phase = phase;
    if (Number.isFinite(percent)) this.progress_percent = percent;
    this.phase_history.push({ phase, timestamp: Math.floor(Date.now() / 1000), progress: this.progress_percent });
    this._touch();
  }
  setThinking(t) { this.gpt_thinking = String(t || ''); this._touch(); }
  setEntities(list) {
    this.entities = (list || []).map((e) => ({ value: e.value, type: e.type }));
    this._touch();
  }
  assignServices(names) {
    this.services_assigned = [...names];
    for (const name of names) {
      if (!this.services.find((s) => s.name === name)) {
        this.services.push({ name, status: 'pending', results_count: 0, status_message: '', is_followup: this.current_round > 0, entities: '' });
      }
    }
    this.stats.total_services = this.services.length;
    this._touch();
  }
  serviceStatus(name, status, message, resultsCount, entities) {
    let s = this.services.find((x) => x.name === name);
    if (!s) { s = { name, status: 'pending', results_count: 0, status_message: '', is_followup: this.current_round > 0, entities: '' }; this.services.push(s); }
    s.status = status;
    if (message != null) s.status_message = message;
    if (Number.isFinite(resultsCount)) s.results_count = resultsCount;
    if (entities != null) s.entities = entities;
    this.stats.active_services = this.services.filter((x) => x.status === 'running').length;
    this.stats.completed_services = this.services.filter((x) => x.status === 'completed').length;
    this.stats.failed_services = this.services.filter((x) => x.status === 'failed').length;
    this.stats.skipped_services = this.services.filter((x) => x.status === 'skipped').length;
    this._touch();
  }
  addDiscovered(value, type, discoveredBy) {
    if (this.discovered_entities.find((e) => e.value === value && e.type === type)) return;
    this.discovered_entities.push({ value, type, discovered_by: discoveredBy || '' });
    this._rebuildUnified();
    this._touch();
  }
  _rebuildUnified() {
    const seen = new Set();
    const all = [];
    for (const e of [...this.entities, ...this.discovered_entities]) {
      const k = `${e.type}|${String(e.value).toLowerCase()}`;
      if (seen.has(k)) continue;
      seen.add(k);
      all.push({ value: e.value, type: e.type });
    }
    this.all_entities = all;
  }
  setRound(r) { this.current_round = r; this._touch(); }
  setResults(obj) {
    this.results_json = JSON.stringify(obj);
    this.total_results = Array.isArray(obj?.services) ? obj.services.length : 0;
    this._touch();
  }
  finish(phase = 'completed') {
    this.setPhase(phase, phase === 'completed' ? 100 : this.progress_percent);
    this.done = true;
    this.emit('done', this.toJSON());
  }

  toJSON() {
    let results = null;
    if (this.results_json) { try { results = JSON.parse(this.results_json); } catch { results = null; } }
    return {
      request_id: this.request_id,
      query: this.query,
      phase: this.phase,
      progress_percent: this.progress_percent,
      gpt_thinking: this.gpt_thinking,
      total_results: this.total_results,
      preliminary_findings: this.preliminary_findings,
      created_at: this.created_at,
      updated_at: this.updated_at,
      entities: this.entities,
      services_assigned: this.services_assigned,
      services: this.services,
      stats: this.stats,
      phase_history: this.phase_history,
      current_round: this.current_round,
      max_rounds: this.max_rounds,
      awaiting_user_action: this.awaiting_user_action,
      discovered_entities: this.discovered_entities,
      confirmed_entities: this.confirmed_entities,
      all_entities: this.all_entities,
      ...(results ? { results } : {}),
    };
  }
}

export function createRequest(requestId, query, maxRounds) {
  const rp = new RequestProgress(requestId, query, maxRounds);
  requests.set(requestId, rp);
  // Bounded retention: drop oldest finished requests beyond 200.
  if (requests.size > 200) {
    const oldest = [...requests.values()].filter((r) => r.done).sort((a, b) => a.updated_at - b.updated_at)[0];
    if (oldest) requests.delete(oldest.request_id);
  }
  return rp;
}
export function getRequest(requestId) { return requests.get(requestId) || null; }
