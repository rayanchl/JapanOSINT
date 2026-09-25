import React, { useState } from 'react';
import { LuChevronDown, LuChevronUp } from 'react-icons/lu';
import EntityChips from './EntityChips.jsx';
import EntityHighlighter from './EntityHighlighter.jsx';
import PipelineView from './PipelineView.jsx';
import RunActions from './RunActions.jsx';
import { Spinner, cx } from '../ui/kit.jsx';
import { currentStageTitle, roundSuffix, isTerminal, serviceDisplayName } from './pipeline.js';

const STATUS_COLOR = {
  pending: 'text-osint-muted', running: 'text-neon-cyan', completed: 'text-neon-green',
  failed: 'text-neon-red', error: 'text-neon-red', skipped: 'text-osint-muted',
};

// How much of a raw service payload the card renders inline.
const RAW_CHARS = 1500;

// The server sends `data` as a PARSED JSON value — pipeline.c does
// `cJSON_Parse(r.data)` and only falls back to a string when the payload was
// not JSON. Rendering it with `String(r.data)` turned every structured value
// into "[object Object]". Stringify structured values, leave strings alone.
function renderPayload(data) {
  if (data == null) return '';
  if (typeof data === 'string') return data;
  try { return JSON.stringify(data, null, 2); }
  catch { return String(data); }
}

/**
 * One investigation: the collapsed run card from the iOS Search tab, with the
 * full `SearchRunDetailView` (pipeline diagram, service queue, entity
 * cross-links, reasoning) one click away inside the same card.
 */
export default function SearchCard({ snapshot, query, onPivot, run, defaultExpanded = false }) {
  const s = snapshot;
  const [expanded, setExpanded] = useState(defaultExpanded);
  const requestId = run?.request_id || s?.request_id;
  const streamError = run?.status === 'error' && !isTerminal(s) ? (run.error || 'live progress stream lost') : null;

  if (!s) {
    return (
      <div className="rounded-[10px] border border-osint-border bg-osint-surface p-3">
        <div className="flex items-center gap-2">
          <div className="text-sm text-osint-text truncate flex-1">{query}</div>
          {streamError ? <span className="text-[11px] text-neon-red">{streamError}</span> : <Spinner size={12} />}
        </div>
        <div className="text-xs text-osint-muted mt-1">{streamError ? 'The run never reported progress.' : 'Starting…'}</div>
      </div>
    );
  }

  const isError = s.phase === 'error';
  const finished = isTerminal(s);
  const working = !finished && !streamError;
  const pct = Math.max(0, Math.min(100, s.progress_percent || 0));
  const svc = s.results?.services || [];
  const runObj = run || { request_id: requestId, snapshot: s, status: finished ? (isError ? 'error' : 'completed') : 'running' };

  return (
    <div className={cx('rounded-[10px] border bg-osint-surface p-3 space-y-3', isError ? 'border-neon-red/40' : 'border-osint-border')}>
      <div className="flex items-center justify-between gap-3">
        <div className="min-w-0">
          <div className="text-sm text-osint-text truncate font-medium">{s.query || query}</div>
          <div className="text-xs text-osint-muted">
            {currentStageTitle(s)}{roundSuffix(s)}
          </div>
        </div>
        <div className="flex items-center gap-2 flex-shrink-0">
          {working && <Spinner size={12} />}
          <div className={cx('text-xs font-mono tabular-nums', isError ? 'text-neon-red' : 'text-accent')}>{Math.round(pct)}%</div>
        </div>
      </div>

      <div className="h-1 rounded bg-osint-border/60 overflow-hidden">
        <div
          className={cx('h-full transition-all duration-500', isError ? 'bg-neon-red' : 'bg-accent')}
          style={{ width: `${pct}%` }}
        />
      </div>

      {streamError && (
        <div className="rounded-md border border-neon-red/40 bg-neon-red/10 px-2 py-1.5 text-[11px] text-neon-red">
          Live progress lost — {streamError}. What is shown is the last snapshot the server gave us.
        </div>
      )}

      {/* A run whose ANALYSIS STAGE FAILED must not look like one that
          succeeded. When llama-server is unreachable the pipeline still walks
          the whole phase ladder to "completed" at 100%, so the progress bar
          says nothing is wrong. The server names every stage it could not
          complete (`stage_errors`, severity error|notice) and this is where
          the user sees it. `notice` rows are not failures: they are
          bounded-view disclosures, e.g. the service catalogue being trimmed
          to a prompt budget. */}
      {Array.isArray(s.stage_errors) && s.stage_errors.length > 0 && (
        <div className={cx('rounded-md border px-2 py-1.5 space-y-1', s.degraded ? 'border-neon-red/50 bg-neon-red/10' : 'border-osint-border bg-osint-bg/40')}>
          <div className={cx('text-[11px] font-medium uppercase tracking-wide', s.degraded ? 'text-neon-red' : 'text-osint-muted')}>
            {s.degraded ? 'Degraded investigation' : 'Run notices'}
          </div>
          {s.degraded && (
            <div className="text-[11px] text-osint-text">
              Part of this run did not happen. What is shown below is what was actually collected, not a complete investigation.
            </div>
          )}
          {s.stage_errors.map((e, i) => (
            <div key={i} className="text-[11px] leading-snug">
              <span className={e.severity === 'notice' ? 'text-osint-muted' : 'text-accent'}>{e.stage} · {e.code}</span>
              {e.detail && <span className="text-osint-muted"> — {e.detail}</span>}
            </div>
          ))}
        </div>
      )}

      {!expanded && s.gpt_thinking && (
        <div className="text-xs text-osint-muted italic line-clamp-2">{s.gpt_thinking}</div>
      )}

      {!expanded && (
        <EntityChips entities={s.entities} discovered={s.discovered_entities} onPivot={onPivot} title="Entities" />
      )}

      {!expanded && s.services?.length > 0 && (
        <div className="space-y-0.5">
          <div className="text-[11px] uppercase tracking-wide text-osint-muted">Services</div>
          <div className="flex flex-wrap gap-x-3 gap-y-0.5 text-xs font-mono">
            {/* `entities` is which entity this service was dispatched ON — the
                other half of the attribution. */}
            {s.services.map((sv) => (
              <span key={sv.name + (sv.entities || '')} className={STATUS_COLOR[sv.status] || 'text-osint-muted'} title={sv.status_message || sv.status}>
                {serviceDisplayName(sv.name)}
                {sv.entities && <span className="opacity-60"> ({sv.entities})</span>}
                <span className="opacity-60"> · {sv.status}</span>
                {sv.results_count > 0 && <span className="opacity-60"> · {sv.results_count}</span>}
              </span>
            ))}
          </div>
        </div>
      )}

      {!expanded && s.results?.synthesis && (
        <div className="text-sm text-osint-text border-t border-osint-border pt-2 whitespace-pre-wrap">
          <EntityHighlighter text={s.results.synthesis} entities={[...(s.entities || []), ...(s.discovered_entities || [])]} />
        </div>
      )}

      {expanded && (
        <div className="border-t border-osint-border pt-3">
          <PipelineView run={runObj} onPivot={onPivot} />
        </div>
      )}

      {svc.length > 0 && (
        <details className="text-xs">
          <summary className="cursor-pointer text-osint-muted hover:text-osint-text">Raw results ({svc.length})</summary>
          <div className="mt-2 space-y-2 max-h-72 overflow-auto">
            {svc.map((r, i) => (
              <div key={i} className="rounded bg-osint-bg/60 p-2">
                <div className="text-[11px] text-osint-muted font-mono">
                  {serviceDisplayName(r.name)} · {r.entity} · {r.success ? 'ok' : (r.error || 'no data')}
                </div>
                {/* Which upstream providers this service actually hit, and how
                    many records each returned (results.services[i].sources). */}
                {Array.isArray(r.sources) && r.sources.length > 0 && (
                  <div className="mt-1 flex flex-wrap gap-x-2 gap-y-0.5 text-[10px] font-mono">
                    <span className="text-osint-muted uppercase tracking-wide">Sources</span>
                    {r.sources.map((src, si) => (
                      <span key={si} className={src.status === 'ok' ? 'text-neon-green/80' : 'text-accent/80'} title={src.detail || src.status}>
                        {src.name}
                        <span className="opacity-60">
                          {' '}·{' '}
                          {/* `records` is null when the server could only
                              attribute by HTTP host. Rendering that as
                              "0 rec" would state a measurement nobody made. */}
                          {typeof src.records === 'number'
                            ? `${src.records} rec`
                            : typeof src.requests === 'number'
                              ? `${src.requests} req`
                              : 'rec n/a'}
                          {' '}· {src.status}
                        </span>
                      </span>
                    ))}
                  </div>
                )}
                {typeof r.record_count === 'number' && (
                  <div className="text-[10px] text-osint-muted">
                    {r.record_count.toLocaleString()} record{r.record_count === 1 ? '' : 's'} stored
                  </div>
                )}
                {r.data && (() => {
                  // A truncated payload that doesn't say it is truncated reads
                  // as the whole response the service gave us.
                  const full = renderPayload(r.data);
                  const clipped = full.length > RAW_CHARS;
                  return (
                    <>
                      <pre className="mt-1 whitespace-pre-wrap break-all text-[11px] text-osint-muted">{clipped ? full.slice(0, RAW_CHARS) : full}</pre>
                      {clipped && (
                        <div className="text-[10px] text-accent/80">
                          Showing the first {RAW_CHARS.toLocaleString()} of {full.length.toLocaleString()} characters.
                        </div>
                      )}
                    </>
                  );
                })()}
              </div>
            ))}
          </div>
        </details>
      )}

      <div className="flex flex-wrap items-center justify-between gap-2 pt-1">
        <button
          type="button"
          onClick={() => setExpanded((v) => !v)}
          className="inline-flex items-center gap-1 text-[11px] text-osint-muted hover:text-accent"
          aria-expanded={expanded}
        >
          {expanded ? <LuChevronUp size={12} /> : <LuChevronDown size={12} />}
          {expanded ? 'Hide pipeline' : 'Pipeline & details'}
        </button>
        {finished && requestId && (
          <RunActions requestId={requestId} query={s.query || query} link={`${typeof window !== 'undefined' ? window.location.origin : ''}/search?run=${encodeURIComponent(requestId)}`} />
        )}
      </div>
    </div>
  );
}
