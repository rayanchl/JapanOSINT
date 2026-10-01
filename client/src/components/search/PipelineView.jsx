import React, { useEffect, useRef, useState } from 'react';
import { LuCheck, LuX, LuChevronDown, LuChevronUp, LuZap, LuArrowRightToLine } from 'react-icons/lu';
import EntityChips from './EntityChips.jsx';
import EntityHighlighter from './EntityHighlighter.jsx';
import { cx, Spinner, SectionLabel, Pill } from '../ui/kit.jsx';
import {
  STAGES, LAST_STAGE, pipelineStages, targetStageIndex, roundSuffix, isTerminal,
  stageFailures, stageErrorHeadline, stageErrorStageLabel, SearchLinks, serviceDisplayName,
} from './pipeline.js';

/* ------------------------------------------------------------------------
 * Port of the iOS PipelineView: the walked stage diagram, the degraded
 * banner, the synthesis, the service queue with per-service attribution, the
 * entity cross-link rows and the LLM reasoning block.
 * ---------------------------------------------------------------------- */

/** StageAnimator: walks the DISPLAYED stage forward one step per second so a
 *  backend that jumps several phases between frames still reads. Forward-only;
 *  an already-finished run seeds straight to done. */
export function useStageAnimator(snap, finished) {
  const errored = snap?.phase === 'error';
  const target = (finished && !errored) ? LAST_STAGE : targetStageIndex(snap);
  const seeded = useRef(false);
  const [shown, setShown] = useState(() => (finished ? LAST_STAGE : targetStageIndex(snap)));
  const targetRef = useRef(target);
  useEffect(() => {
    if (!seeded.current) { seeded.current = true; return; }
    targetRef.current = Math.max(targetRef.current, target);
  }, [target]);
  useEffect(() => {
    if (shown >= targetRef.current) return undefined;
    const t = setTimeout(() => setShown((s) => Math.min(s + 1, targetRef.current)), 1000);
    return () => clearTimeout(t);
  }, [shown, target]);
  return { shown: Math.min(shown, LAST_STAGE), finished, errored };
}

function StageGlyph({ state }) {
  if (state === 'done') return <span className="w-5 h-5 rounded-full bg-neon-green/15 text-neon-green flex items-center justify-center"><LuCheck size={12} /></span>;
  if (state === 'current') return <span className="w-5 h-5 flex items-center justify-center"><Spinner size={14} className="border-t-neon-cyan" /></span>;
  if (state === 'error') return <span className="w-5 h-5 rounded-full bg-neon-red/15 text-neon-red flex items-center justify-center"><LuX size={12} /></span>;
  return <span className="w-5 h-5 flex items-center justify-center"><span className="w-2.5 h-2.5 rounded-full border border-osint-border-bright" /></span>;
}

function StageRow({ stage, state, isLast, connectorDone, suffix }) {
  const tone = state === 'done' ? 'text-osint-text' : state === 'current' ? 'text-neon-cyan' : state === 'error' ? 'text-neon-red' : 'text-osint-muted';
  return (
    <div className="flex items-stretch gap-3">
      <div className="flex flex-col items-center">
        <StageGlyph state={state} />
        {!isLast && <span className={cx('w-px flex-1 my-0.5', connectorDone ? 'bg-neon-green/40' : 'bg-osint-border')} />}
      </div>
      <div className={cx('text-sm pb-3 flex items-baseline gap-2 min-w-0', tone)}>
        <span className="truncate">{stage.title}{suffix}</span>
        <span className="text-[10px] font-mono text-osint-muted">{state === 'done' ? 'done' : state === 'current' ? 'in progress' : state === 'error' ? 'failed' : 'pending'}</span>
      </div>
    </div>
  );
}

export function StageDiagram({ snap, finished, animator }) {
  const { shown, errored } = animator;
  const staticRows = pipelineStages(snap);
  const rows = STAGES.map((s, i) => {
    let state;
    if (i < shown) state = 'done';
    else if (i === shown) state = errored ? 'error' : (finished && i >= LAST_STAGE ? 'done' : 'current');
    else state = 'pending';
    // Once the walk has caught up with a finished run, paint the real
    // per-stage verdict (a stage that produced nothing is an error).
    if (finished && !errored && shown >= LAST_STAGE) state = staticRows[i].state;
    return { stage: s, state };
  });
  return (
    <div className="pt-1">
      {rows.map((r, i) => (
        <StageRow
          key={r.stage.key}
          stage={r.stage}
          state={r.state}
          isLast={i === rows.length - 1}
          connectorDone={r.state === 'done'}
          suffix={r.stage.key === 'followup_analyzing' ? roundSuffix(snap) : ''}
        />
      ))}
    </div>
  );
}

export function DegradedSection({ snap }) {
  const failures = stageFailures(snap);
  if (!failures.length) return null;
  return (
    <section className="rounded-[10px] border border-neon-red/40 bg-neon-red/10 p-3 space-y-2">
      <div className="font-mono text-[10px] font-semibold uppercase tracking-[0.12em] text-neon-red">Degraded run</div>
      <p className="text-xs text-osint-text">
        One or more pipeline stages produced nothing and the run continued on a fallback. What follows is what was actually collected, not a complete investigation.
      </p>
      <ul className="space-y-1">
        {failures.map((f, i) => (
          <li key={i} className="text-xs">
            <span className="text-osint-text">{stageErrorStageLabel(f.stage)}</span>
            <span className="text-osint-muted"> · </span>
            <span className="text-neon-red">{stageErrorHeadline(f.code)}</span>
            {f.detail && <div className="text-[11px] text-osint-muted font-mono break-words">{f.detail}</div>}
          </li>
        ))}
      </ul>
    </section>
  );
}

export function SynthesisSection({ snap }) {
  const text = snap?.results?.synthesis;
  if (!text) return null;
  const degraded = stageFailures(snap).some((f) => f.stage === 'synthesis');
  return (
    <section className="space-y-1.5">
      <SectionLabel right={degraded ? <Pill tone="danger">fallback synthesis</Pill> : null}>Synthesis</SectionLabel>
      <div className="text-sm text-osint-text leading-relaxed whitespace-pre-wrap">
        <EntityHighlighter text={text} entities={[...(snap.entities || []), ...(snap.discovered_entities || [])]} />
      </div>
    </section>
  );
}

const SVC_TONE = { running: 'cyan', completed: 'success', failed: 'danger', error: 'danger', skipped: 'neutral', pending: 'neutral' };

function StatusIcon({ status }) {
  if (status === 'running') return <LuZap size={11} />;
  if (status === 'completed') return <LuCheck size={11} />;
  if (status === 'failed' || status === 'error') return <LuX size={11} />;
  if (status === 'skipped') return <LuArrowRightToLine size={11} />;
  return null;
}

function prettyPayload(data) {
  if (data == null) return null;
  if (typeof data === 'string') {
    const t = data.trim();
    if (!t || t === 'null') return null;
    try { return JSON.stringify(JSON.parse(t), null, 2); } catch { return t; }
  }
  try { return JSON.stringify(data, null, 2); } catch { return String(data); }
}

function SourceLine({ src }) {
  const ok = (src.status || 'ok') === 'ok';
  return (
    <div className="flex items-baseline gap-2 text-[11px] font-mono" title={src.detail || src.status}>
      <span className={ok ? 'text-neon-green' : 'text-accent'}>{src.name}</span>
      <span className="text-osint-muted">
        {/* `records` is null when the server could only attribute by HTTP
            host; printing 0 there would state a measurement nobody made. */}
        {typeof src.records === 'number' ? `${src.records} rec` : typeof src.requests === 'number' ? `${src.requests} req` : 'rec n/a'}
        {' · '}{src.status || 'ok'}
      </span>
      {src.detail && <span className="text-osint-muted truncate">{src.detail}</span>}
    </div>
  );
}

const RAW_CHARS = 1500;

function ServiceRow({ svc, snap, onPivot }) {
  const [open, setOpen] = useState(false);
  const name = serviceDisplayName(svc.name);
  const sources = SearchLinks.sourcesOf(name, snap);
  const calls = SearchLinks.callsOf(name, snap);
  const found = SearchLinks.entitiesDiscoveredBy(name, snap);
  const canExpand = sources.length > 0 || calls.length > 0 || found.length > 0;
  return (
    <li className="rounded-md border border-osint-border bg-osint-bg/40">
      <button type="button" disabled={!canExpand} onClick={() => setOpen((v) => !v)} className="w-full flex items-center gap-2 px-2.5 py-1.5 text-left disabled:cursor-default">
        <Pill tone={SVC_TONE[svc.status] || 'neutral'}><StatusIcon status={svc.status} />{svc.status}</Pill>
        <span className="text-xs font-mono text-osint-text truncate">{name}</span>
        {svc.entities && <span className="text-[11px] text-osint-muted truncate">({svc.entities})</span>}
        {svc.is_followup && <Pill tone="purple">follow-up</Pill>}
        <span className="ml-auto text-[11px] font-mono text-osint-muted whitespace-nowrap">
          {svc.results_count > 0 ? `${svc.results_count} results` : svc.status_message || ''}
        </span>
        {canExpand && (open ? <LuChevronUp size={12} className="text-osint-muted" /> : <LuChevronDown size={12} className="text-osint-muted" />)}
      </button>
      {open && (
        <div className="px-2.5 pb-2.5 space-y-2 border-t border-osint-border pt-2">
          {sources.length > 0 ? (
            <div className="space-y-0.5">
              <div className="text-[10px] uppercase tracking-wide text-osint-muted">Sources</div>
              {sources.map((s) => <SourceLine key={s.name} src={s} />)}
            </div>
          ) : (
            <div className="text-[11px] text-osint-muted">No source attribution (populates when the run completes).</div>
          )}
          {calls.length > 0 && (
            <div className="space-y-1">
              <div className="text-[10px] uppercase tracking-wide text-osint-muted">Calls</div>
              {calls.map((c, i) => {
                const full = prettyPayload(c.data);
                const clipped = full && full.length > RAW_CHARS;
                return (
                  <div key={i} className="text-[11px] space-y-0.5">
                    <div className="font-mono">
                      <span className="text-osint-text">{c.entity || '—'}</span>
                      <span className={c.success ? 'text-neon-green' : 'text-neon-red'}> · {c.success ? 'ok' : (c.error || 'no data')}</span>
                      {typeof c.confidence === 'number' && c.confidence > 0 && <span className="text-osint-muted"> · {Math.round(c.confidence)}%</span>}
                      {typeof c.record_count === 'number' && <span className="text-osint-muted"> · {c.record_count.toLocaleString()} record{c.record_count === 1 ? '' : 's'} stored</span>}
                    </div>
                    {full && (
                      <details>
                        <summary className="cursor-pointer text-osint-muted hover:text-osint-text">payload</summary>
                        <pre className="mt-1 whitespace-pre-wrap break-all text-[11px] text-osint-muted max-h-64 overflow-auto">{clipped ? full.slice(0, RAW_CHARS) : full}</pre>
                        {clipped && <div className="text-[10px] text-accent">Showing the first {RAW_CHARS.toLocaleString()} of {full.length.toLocaleString()} characters.</div>}
                      </details>
                    )}
                  </div>
                );
              })}
            </div>
          )}
          {found.length > 0 && (
            <div className="space-y-1">
              <div className="text-[10px] uppercase tracking-wide text-osint-muted">Discovered</div>
              <EntityChips discovered={found} onPivot={onPivot} />
            </div>
          )}
        </div>
      )}
    </li>
  );
}

export function ServiceQueueSection({ snap, onPivot }) {
  const svcs = snap?.services || [];
  if (!svcs.length) return null;
  const st = snap.stats || {};
  return (
    <section className="space-y-1.5">
      <SectionLabel right={(
        <span className="font-mono text-[10px] text-osint-muted">
          {typeof st.completed_services === 'number' ? `${st.completed_services}/${st.total_services ?? svcs.length} done` : `${svcs.length}`}
          {st.failed_services > 0 ? ` · ${st.failed_services} failed` : ''}
        </span>
      )}>Service queue</SectionLabel>
      <ul className="space-y-1">
        {svcs.map((s) => <ServiceRow key={s.name + (s.entities || '')} svc={s} snap={snap} onPivot={onPivot} />)}
      </ul>
    </section>
  );
}

function EntityCrossLinkRow({ entity, snap, onPivot, navigateTo }) {
  const [open, setOpen] = useState(false);
  const discoveredBy = SearchLinks.discoveredBy(entity, snap);
  const calledFor = SearchLinks.servicesCalledFor(entity.value, snap);
  const againBy = SearchLinks.encounteredAgainBy(entity.value, snap);
  const canExpand = Boolean(discoveredBy) || calledFor.length > 0 || againBy.length > 0;
  return (
    <li className="rounded-md border border-osint-border bg-osint-bg/40">
      <div className="flex items-center gap-2 px-2.5 py-1.5">
        <EntityChips entities={entity._disc ? [] : [entity]} discovered={entity._disc ? [entity] : []} onPivot={onPivot} />
        <span className="ml-auto" />
        {navigateTo && <button type="button" onClick={() => navigateTo(entity)} className="text-[11px] text-osint-muted hover:text-accent">profile</button>}
        {canExpand && (
          <button type="button" onClick={() => setOpen((v) => !v)} className="text-osint-muted hover:text-osint-text" aria-label="Cross-links">
            {open ? <LuChevronUp size={12} /> : <LuChevronDown size={12} />}
          </button>
        )}
      </div>
      {open && (
        <div className="px-2.5 pb-2 text-[11px] space-y-0.5 border-t border-osint-border pt-1.5">
          {discoveredBy && <div><span className="text-osint-muted">discovered by </span><span className="font-mono text-osint-text">{serviceDisplayName(discoveredBy)}</span></div>}
          {calledFor.length > 0 && <div><span className="text-osint-muted">queried by </span><span className="font-mono text-osint-text">{calledFor.join(', ')}</span></div>}
          {againBy.length > 0 && <div><span className="text-osint-muted">seen again in </span><span className="font-mono text-osint-text">{againBy.join(', ')}</span></div>}
        </div>
      )}
    </li>
  );
}

export function EntitySection({ snap, onPivot }) {
  const all = [
    ...(snap?.entities || []).map((e) => ({ ...e, _disc: false })),
    ...(snap?.discovered_entities || []).map((e) => ({ ...e, _disc: true })),
  ];
  const seen = new Set();
  const uniq = all.filter((e) => { const k = `${e.type}|${e.value}`; if (seen.has(k)) return false; seen.add(k); return true; });
  if (!uniq.length) return null;
  return (
    <section className="space-y-1.5">
      <SectionLabel right={<span className="font-mono text-[10px] text-osint-muted">{uniq.length}</span>}>Entities</SectionLabel>
      <ul className="space-y-1">
        {uniq.map((e) => <EntityCrossLinkRow key={`${e.type}|${e.value}`} entity={e} snap={snap} onPivot={onPivot} />)}
      </ul>
    </section>
  );
}

export function ThinkingSection({ snap }) {
  const text = snap?.gpt_thinking;
  const [open, setOpen] = useState(false);
  if (!text) return null;
  return (
    <section className="space-y-1.5">
      <button type="button" onClick={() => setOpen((v) => !v)} className="w-full">
        <SectionLabel right={open ? <LuChevronUp size={12} className="text-osint-muted" /> : <LuChevronDown size={12} className="text-osint-muted" />}>LLM reasoning</SectionLabel>
      </button>
      {open && <div className="text-xs text-osint-muted italic whitespace-pre-wrap leading-relaxed">{text}</div>}
    </section>
  );
}

/** The full detail view (SearchRunDetailView), used inside an expanded card. */
export default function PipelineView({ run, onPivot }) {
  const snap = run?.snapshot;
  const finished = Boolean(run?.status === 'completed' || run?.status === 'error' || isTerminal(snap));
  const animator = useStageAnimator(snap, finished);
  const [collapsed, setCollapsed] = useState(false);
  const cur = STAGES[Math.min(animator.shown, LAST_STAGE)];
  return (
    <div className="space-y-5">
      <section className="space-y-1.5">
        <button type="button" onClick={() => setCollapsed((v) => !v)} className="w-full" aria-expanded={!collapsed}>
          <SectionLabel right={collapsed ? <LuChevronDown size={12} className="text-osint-muted" /> : <LuChevronUp size={12} className="text-osint-muted" />}>Pipeline</SectionLabel>
        </button>
        {collapsed ? (
          <StageRow stage={cur} state={animator.errored ? 'error' : (finished && animator.shown >= LAST_STAGE ? 'done' : 'current')} isLast connectorDone={false} suffix={cur.key === 'followup_analyzing' ? roundSuffix(snap) : ''} />
        ) : (
          <StageDiagram snap={snap} finished={finished} animator={animator} />
        )}
      </section>
      <DegradedSection snap={snap} />
      <SynthesisSection snap={snap} />
      <ServiceQueueSection snap={snap} onPivot={onPivot} />
      <EntitySection snap={snap} onPivot={onPivot} />
      <ThinkingSection snap={snap} />
    </div>
  );
}
