import React from 'react';
import { useNavigate } from 'react-router-dom';
import { entityVisual } from '../../utils/entityVisuals.js';

/**
 * Entity chips (port of OSINTsaas EntityReviewPanel UX): type-coloured chips
 * for query + discovered entities. Click → open the entity profile; the
 * "pivot" action starts a fresh search on that entity (the multi-round
 * investigation flow). `discovered` chips get a subtle ring.
 */
export default function EntityChips({ entities = [], discovered = [], onPivot, title }) {
  const navigate = useNavigate();
  const all = [
    ...entities.map((e) => ({ ...e, _disc: false })),
    ...discovered.map((e) => ({ ...e, _disc: true })),
  ];
  if (all.length === 0) return null;
  const seen = new Set();
  return (
    <div className="space-y-1.5">
      {title && <div className="text-[11px] uppercase tracking-wide text-gray-500">{title}</div>}
      <div className="flex flex-wrap gap-1.5">
        {all.map((e, i) => {
          const key = `${e.type}|${e.value}`;
          if (seen.has(key)) return null;
          seen.add(key);
          const v = entityVisual(e.type);
          return (
            <span
              key={`${key}-${i}`}
              className={`group inline-flex items-center gap-1.5 pl-2 pr-1 py-0.5 rounded border text-xs ${v.color} ${e._disc ? 'ring-1 ring-white/10' : ''}`}
            >
              <button
                type="button"
                title={`Open ${v.label} profile`}
                onClick={() => navigate(`/entities/${encodeURIComponent(String(e.type).toLowerCase())}/lookup?q=${encodeURIComponent(e.value)}`)}
                className="font-medium hover:underline max-w-[220px] truncate"
              >
                <span className="opacity-60">{v.label}:</span> {e.value}
              </button>
              {onPivot && (
                <button
                  type="button"
                  title="Pivot — run a new investigation on this entity"
                  onClick={() => onPivot(e.value)}
                  className="opacity-0 group-hover:opacity-100 transition-opacity px-1 rounded hover:bg-white/10"
                >
                  ⤳
                </button>
              )}
            </span>
          );
        })}
      </div>
    </div>
  );
}
