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
            {s.services.map((sv) => (
              <span key={sv.name} className={STATUS_COLOR[sv.status] || 'text-gray-500'}>
                {sv.name}
                <span className="opacity-60"> · {sv.status}</span>
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
                {r.data && (
                  <pre className="mt-1 whitespace-pre-wrap break-all text-[11px] text-gray-400">
                    {String(r.data).slice(0, 1500)}
                  </pre>
                )}
              </div>
            ))}
          </div>
        </details>
      )}
    </div>
  );
}
