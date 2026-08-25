import React from 'react';
import EntityChips from './EntityChips.jsx';
import EntityHighlighter from './EntityHighlighter.jsx';

const PHASE_LABEL = {
  queued: 'Queued', gpt_analyzing: 'Analyzing query', services_assigned: 'Routing services',
  services_launching: 'Launching', agents_working: 'Running services',
  preliminary_results: 'Preliminary results', awaiting_review: 'Awaiting review',
  followup_analyzing: 'Follow-up analysis', aggregating: 'Aggregating',
  completed: 'Completed', error: 'Error',
};

const STATUS_COLOR = {
  pending: 'text-gray-500', running: 'text-neon-cyan', completed: 'text-neon-green',
  failed: 'text-status-offline', skipped: 'text-gray-600',
};

// How much of a raw service payload the card renders inline.
const RAW_CHARS = 1500;

// The server sends `data` as a PARSED JSON value — pipeline.c does
// `cJSON_Parse(r.data)` and only falls back to a string when the payload was
// not JSON. This card rendered it with `String(r.data)`, which turns the
// object every real service returns into the literal seven characters
// "[object Object]" — and then, because that is 15 characters long, cheerfully
// reported it as under the truncation limit and showed no warning. Every
// service result in the raw panel was that string. Stringify structured values
// and leave genuine strings alone.
function renderPayload(data) {
  if (data == null) return '';
  if (typeof data === 'string') return data;
  try { return JSON.stringify(data, null, 2); }
  catch { return String(data); }
}

/** Live progress card — port of the OSINTsaas SearchCard live subtree. */
export default function SearchCard({ snapshot, query, onPivot }) {
  const s = snapshot;
  if (!s) {
    return (
      <div className="rounded border border-osint-border bg-osint-surface p-3">
        <div className="text-sm text-gray-300">{query}</div>
        <div className="text-xs text-gray-500 mt-1">Starting…</div>
      </div>
    );
  }
  const pct = Math.max(0, Math.min(100, s.progress_percent || 0));
  const svc = s.results?.services || [];
  return (
    <div className="rounded border border-osint-border bg-osint-surface p-3 space-y-3">
      <div className="flex items-center justify-between gap-3">
        <div className="min-w-0">
          <div className="text-sm text-gray-200 truncate">{s.query}</div>
          <div className="text-xs text-gray-500">
            {PHASE_LABEL[s.phase] || s.phase}
            {s.current_round > 0 && ` · round ${s.current_round}/${s.max_rounds}`}
          </div>
        </div>
        <div className="text-xs font-mono text-neon-cyan">{pct}%</div>
      </div>

      <div className="h-1 rounded bg-white/5 overflow-hidden">
        <div
          className={`h-full transition-all duration-500 ${s.phase === 'error' ? 'bg-status-offline' : 'bg-neon-cyan'}`}
          style={{ width: `${pct}%` }}
        />
      </div>

      {/* A run whose ANALYSIS STAGE FAILED must not look like one that
          succeeded. When llama-server is unreachable the pipeline still walks
          the whole phase ladder to "completed" at 100% — because the corpus
          lookup it falls back to is real work worth keeping — so the progress
          bar above says nothing is wrong. The server now names every stage it
          could not complete (`stage_errors`, severity error|notice) and this is
          where the user sees it. `notice` rows are not failures: they are
          bounded-view disclosures, e.g. the service catalogue the model was
          shown being trimmed to a prompt budget. */}
      {Array.isArray(s.stage_errors) && s.stage_errors.length > 0 && (
        <div
          className={`rounded border px-2 py-1.5 space-y-1 ${
            s.degraded
              ? 'border-status-offline/50 bg-status-offline/10'
              : 'border-osint-border bg-black/20'
          }`}
        >
          <div
            className={`text-[11px] font-medium uppercase tracking-wide ${
              s.degraded ? 'text-status-offline' : 'text-gray-500'
            }`}
          >
            {s.degraded ? 'Degraded investigation' : 'Run notices'}
          </div>
          {s.degraded && (
            <div className="text-[11px] text-gray-300">
              Part of this run did not happen. What is shown below is what was
              actually collected, not a complete investigation.
            </div>
          )}
          {s.stage_errors.map((e, i) => (
            <div key={i} className="text-[11px] leading-snug">
              <span
                className={
                  e.severity === 'notice' ? 'text-gray-500' : 'text-amber-400'
                }
              >
                {e.stage} · {e.code}
              </span>
              {e.detail && <span className="text-gray-500"> — {e.detail}</span>}
            </div>
          ))}
        </div>
      )}

      {s.gpt_thinking && (
        <div className="text-xs text-gray-400 italic line-clamp-2">{s.gpt_thinking}</div>
      )}

      <EntityChips
        entities={s.entities}
        discovered={s.discovered_entities}
        onPivot={onPivot}
        title="Entities"
      />

      {s.services?.length > 0 && (
        <div className="space-y-0.5">
          <div className="text-[11px] uppercase tracking-wide text-gray-500">Services</div>
          <div className="flex flex-wrap gap-x-3 gap-y-0.5 text-xs">
            {/* `entities` is which entity this service was dispatched ON — the
                other half of the attribution, and the field that makes a
                multi-service run readable as "these services, against these
                entities" instead of a bare list of names. */}
            {s.services.map((sv) => (
              <span
                key={sv.name}
                className={STATUS_COLOR[sv.status] || 'text-gray-500'}
                title={sv.status_message || sv.status}
              >
                {sv.name}
                {sv.entities && <span className="opacity-60"> ({sv.entities})</span>}
                <span className="opacity-60"> · {sv.status}</span>
                {sv.results_count > 0 && (
                  <span className="opacity-60"> · {sv.results_count}</span>
                )}
              </span>
            ))}
          </div>
        </div>
      )}

      {s.results?.synthesis && (
        <div className="text-sm text-gray-300 border-t border-osint-border pt-2">
          <EntityHighlighter
            text={s.results.synthesis}
            entities={[...(s.entities || []), ...(s.discovered_entities || [])]}
          />
        </div>
      )}

      {svc.length > 0 && (
        <details className="text-xs">
          <summary className="cursor-pointer text-gray-500 hover:text-gray-300">
            Raw results ({svc.length})
          </summary>
          <div className="mt-2 space-y-2 max-h-72 overflow-auto">
            {svc.map((r, i) => (
              <div key={i} className="rounded bg-black/20 p-2">
                <div className="text-[11px] text-gray-400">
                  {r.name} · {r.entity} · {r.success ? 'ok' : (r.error || 'no data')}
                </div>
                {/* Which upstream providers this service actually hit, and how
                    many records each returned. osint_dispatch.c has always
                    built this (results.services[i].sources) and the card never
                    rendered it, so a result carried no attribution on screen
                    even when the server knew exactly where every row came
                    from. */}
                {Array.isArray(r.sources) && r.sources.length > 0 && (
                  <div className="mt-1 flex flex-wrap gap-x-2 gap-y-0.5 text-[10px]">
                    <span className="text-gray-600 uppercase tracking-wide">Sources</span>
                    {r.sources.map((src, si) => (
                      <span
                        key={si}
                        className={src.status === 'ok' ? 'text-neon-green/80' : 'text-amber-400/80'}
                        title={src.detail || src.status}
                      >
                        {src.name}
                        <span className="opacity-60">
                          {' '}·{' '}
                          {/* `records` is null when the server could only
                              attribute by HTTP host and therefore does not
                              know how many rows came from this one. Rendering
                              that as "0 rec" would state a measurement nobody
                              made — and `?? 0` did exactly that. Show the
                              requests it really counted instead. */}
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
                  <div className="text-[10px] text-gray-600">
                    {r.record_count.toLocaleString()} record
                    {r.record_count === 1 ? '' : 's'} stored
                  </div>
                )}
                {r.data && (() => {
                  // A truncated payload that doesn't say it is truncated reads
                  // as the whole response the service gave us.
                  const full = renderPayload(r.data);
                  const clipped = full.length > RAW_CHARS;
                  return (
                    <>
                      <pre className="mt-1 whitespace-pre-wrap break-all text-[11px] text-gray-400">
                        {clipped ? full.slice(0, RAW_CHARS) : full}
                      </pre>
                      {clipped && (
                        <div className="text-[10px] text-amber-400/80">
                          Showing the first {RAW_CHARS.toLocaleString()} of{' '}
                          {full.length.toLocaleString()} characters.
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
    </div>
  );
}
