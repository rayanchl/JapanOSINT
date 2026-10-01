/**
 * Pure helpers for the search pipeline UI — a port of the top of the iOS
 * `PipelineView.swift` (PipelineStage, pipelineStages(for:), SearchLinks).
 * Snapshot field names are the ones `native/core/progress.c` serialises.
 */

/** Ordered stages. `phaseKey` is progress.c's phase string, `failureAlias` the
 *  stage name pipeline.c uses in `stage_errors`, `percentFloor` the bar level
 *  the stage starts at (pipeline.c thresholds). */
export const STAGES = [
  { key: 'queued',              title: 'Queued',              alias: null,        floor: 5 },
  { key: 'gpt_analyzing',       title: 'Analyzing query',     alias: 'analysis',  floor: 15 },
  { key: 'services_assigned',   title: 'Services assigned',   alias: null,        floor: 20 },
  { key: 'agents_working',      title: 'Running services',    alias: null,        floor: 25 },
  { key: 'preliminary_results', title: 'Preliminary results', alias: null,        floor: 60 },
  { key: 'followup_analyzing',  title: 'Follow-up analysis',  alias: 'round_',    floor: 75 },
  { key: 'aggregating',         title: 'Aggregating',         alias: 'synthesis', floor: 90 },
  { key: 'completed',           title: 'Completed',           alias: null,        floor: 100 },
];
export const LAST_STAGE = STAGES.length - 1;

/** Phases progress.c can emit that are not their own row in the diagram. */
const PHASE_TO_STAGE = {
  image_analyzing: 'gpt_analyzing',
  services_launching: 'agents_working',
  awaiting_review: 'preliminary_results',
};

export const PHASE_LABEL = {
  queued: 'Queued', image_analyzing: 'Analyzing image', gpt_analyzing: 'Analyzing query',
  services_assigned: 'Services assigned', services_launching: 'Launching services',
  agents_working: 'Running services', preliminary_results: 'Preliminary results',
  awaiting_review: 'Awaiting review', followup_analyzing: 'Follow-up analysis',
  aggregating: 'Aggregating', completed: 'Completed', error: 'Error',
};

export function isTerminal(snap) {
  return Boolean(snap && (snap.done || snap.phase === 'completed' || snap.phase === 'error'));
}

export function stageFailures(snap) {
  return (snap?.stage_errors || []).filter((e) => (e?.severity || 'error') === 'error');
}
export function stageNotices(snap) {
  return (snap?.stage_errors || []).filter((e) => e?.severity === 'notice');
}
export function isDegraded(snap) {
  return Boolean(snap && (snap.degraded === true || stageFailures(snap).length > 0));
}

/** Fallback ordinal from progress_percent (pipeline.c thresholds). */
export function stageOrdinalForPercent(p) {
  const v = Number(p) || 0;
  if (v < 15) return 0;
  if (v < 20) return 1;
  if (v < 25) return 2;
  if (v < 60) return 3;
  if (v < 85) return 4;
  if (v < 90) return 5;
  if (v < 100) return 6;
  return 7;
}

/** [{stage, state}] with state ∈ done | current | pending | error. */
export function pipelineStages(snap) {
  if (!snap) return STAGES.map((s, i) => ({ stage: s, state: i === 0 ? 'current' : 'pending' }));
  if (snap.done || snap.phase === 'completed') {
    // A completed run is not a successful one: a stage that produced
    // nothing is painted as an error even though the run walked past it.
    const failed = stageFailures(snap).map((f) => f.stage);
    return STAGES.map((s) => {
      const hit = failed.some((f) => f === s.key || f === s.alias || (s.alias === 'round_' && String(f).startsWith('round_')));
      return { stage: s, state: hit ? 'error' : 'done' };
    });
  }
  if (snap.phase === 'error') {
    const k = stageOrdinalForPercent(snap.progress_percent);
    return STAGES.map((s, i) => ({ stage: s, state: i < k ? 'done' : i === k ? 'error' : 'pending' }));
  }
  const mapped = PHASE_TO_STAGE[snap.phase] || snap.phase;
  let k = STAGES.findIndex((s) => s.key === mapped);
  if (k < 0) k = stageOrdinalForPercent(snap.progress_percent);
  return STAGES.map((s, i) => ({ stage: s, state: i < k ? 'done' : i === k ? 'current' : 'pending' }));
}

/** Index of the current/error row, or the last row when everything is done. */
export function targetStageIndex(snap) {
  const rows = pipelineStages(snap);
  const i = rows.findIndex((r) => r.state === 'current' || r.state === 'error');
  return i >= 0 ? i : rows.length - 1;
}

export function currentStageTitle(snap) {
  if (snap?.phase === 'error') return 'Error';
  const cur = pipelineStages(snap).find((r) => r.state === 'current');
  return cur ? cur.stage.title : STAGES[LAST_STAGE].title;
}

export function roundSuffix(snap) {
  const r = Number(snap?.current_round) || 0;
  return r > 0 ? ` · round ${r}/${snap?.max_rounds ?? 5}` : '';
}

/** Human wording for the codes pipeline.c emits; unknown codes fall back to
 *  the raw code so a new one is never hidden behind a blank. */
export function stageErrorHeadline(code) {
  switch (code) {
    case 'llm_unreachable': return 'The analysis model was unreachable';
    case 'llm_timeout': return 'The analysis model timed out';
    case 'llm_bad_request': return 'The analysis model rejected the request';
    case 'llm_http_error': return 'The analysis model answered with an HTTP error';
    case 'llm_empty': return 'The analysis model returned nothing usable';
    case 'no_entities_extracted': return 'No entities could be extracted';
    case 'service_catalogue_bounded': return 'Service catalogue was trimmed to fit';
    default: return String(code || '').replace(/_/g, ' ');
  }
}

export function stageErrorStageLabel(stage) {
  switch (stage) {
    case 'analysis': return 'Analysis';
    case 'services_assigned': return 'Service assignment';
    case 'synthesis': return 'Synthesis';
    default:
      if (String(stage || '').startsWith('round_')) return `Follow-up ${String(stage).slice(6)}`;
      return String(stage || '').replace(/_/g, ' ').replace(/^./, (c) => c.toUpperCase());
  }
}

/* ---- SearchLinks: cross-references between services and entities ---- */

/** JP_CORPUS_LOOKUP surfaces as "DB SEARCH" everywhere. */
export function serviceDisplayName(raw) {
  return raw === 'JP_CORPUS_LOOKUP' ? 'DB SEARCH' : (raw || '');
}

function payloadText(data) {
  if (data == null) return '';
  if (typeof data === 'string') return data;
  try { return JSON.stringify(data); } catch { return String(data); }
}

export const SearchLinks = {
  discoveredBy(e, snap) {
    if (e?.discovered_by) return e.discovered_by;
    const hit = (snap?.discovered_entities || []).find(
      (d) => String(d.type).toLowerCase() === String(e?.type).toLowerCase() && d.value === e?.value,
    );
    return hit?.discovered_by || null;
  },
  /** Services that were *called for* this entity value. */
  servicesCalledFor(value, snap) {
    const v = String(value || '').toLowerCase();
    const names = [];
    for (const s of snap?.services || []) {
      if (String(s.entities || '').toLowerCase().includes(v)) names.push(serviceDisplayName(s.name));
    }
    for (const r of snap?.results?.services || []) {
      if (String(r.entity || '').toLowerCase() === v) {
        const n = serviceDisplayName(r.name);
        if (!names.includes(n)) names.push(n);
      }
    }
    return names;
  },
  /** Services whose result payload mentioned this entity again. */
  encounteredAgainBy(value, snap) {
    const v = String(value || '').toLowerCase();
    if (!v) return [];
    const names = [];
    for (const r of snap?.results?.services || []) {
      if (!payloadText(r.data).toLowerCase().includes(v)) continue;
      const n = serviceDisplayName(r.name);
      if (!names.includes(n)) names.push(n);
    }
    return names;
  },
  entitiesDiscoveredBy(service, snap) {
    return (snap?.discovered_entities || []).filter((d) => serviceDisplayName(d.discovered_by || '') === service);
  },
  callsOf(service, snap) {
    return (snap?.results?.services || []).filter((r) => serviceDisplayName(r.name) === service);
  },
  /** Underlying providers a service hit, aggregated across its per-entity calls
   *  (deduped by name; records summed ONLY when measured; any non-ok status wins). */
  sourcesOf(service, snap) {
    const order = [];
    const acc = new Map();
    for (const r of snap?.results?.services || []) {
      if (serviceDisplayName(r.name) !== service) continue;
      for (const s of r.sources || []) {
        const st = s.status || 'ok';
        const cur = acc.get(s.name);
        if (cur) {
          if (typeof s.records === 'number') cur.records = (cur.records ?? 0) + s.records;
          if (typeof s.requests === 'number') cur.requests = (cur.requests ?? 0) + s.requests;
          if (st !== 'ok') cur.status = st;
          if (!cur.detail && s.detail) cur.detail = s.detail;
        } else {
          order.push(s.name);
          acc.set(s.name, {
            name: s.name, status: st,
            records: typeof s.records === 'number' ? s.records : null,
            requests: typeof s.requests === 'number' ? s.requests : null,
            detail: s.detail || null,
          });
        }
      }
    }
    return order.map((n) => acc.get(n));
  },
};

/** Japanese script detection (JapaneseAware.swift `isJapanese`). */
export function isJapanese(s) {
  return /[぀-ゟ゠-ヿ一-鿿㐀-䶿ｦ-ﾟ]/.test(String(s || ''));
}
