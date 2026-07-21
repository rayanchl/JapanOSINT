import React from 'react';
import { useNavigate } from 'react-router-dom';
import { entityVisual } from '../../utils/entityVisuals.js';

// Escape a literal value for use in a RegExp (no \b — CJK has no word
// boundaries; OSINTsaas used value-literal matching for the same reason).
function esc(s) {
  return String(s).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

/**
 * Port of OSINTsaas EntityHighlighter: render `text` with discovered entity
 * values wrapped in type-coloured, clickable spans (→ entity profile).
 */
export default function EntityHighlighter({ text, entities = [] }) {
  const navigate = useNavigate();
  if (!text) return null;
  const uniq = [];
  const seen = new Set();
  for (const e of entities) {
    const val = String(e?.value || '').trim();
    if (val.length < 2 || seen.has(val)) continue;
    seen.add(val);
    uniq.push(e);
  }
  if (uniq.length === 0) return <>{text}</>;

  const re = new RegExp(`(${uniq.map((e) => esc(e.value)).join('|')})`, 'g');
  const byVal = new Map(uniq.map((e) => [e.value, e]));
  const parts = String(text).split(re);

  return (
    <>
      {parts.map((part, i) => {
        const e = byVal.get(part);
        if (!e) return <React.Fragment key={i}>{part}</React.Fragment>;
        const v = entityVisual(e.type);
        return (
          <button
            key={i}
            type="button"
            title={`${v.label} — open profile`}
            onClick={() => navigate(`/entities/${encodeURIComponent(String(e.type).toLowerCase())}/lookup?q=${encodeURIComponent(e.value)}`)}
            className={`inline px-1 rounded border ${v.color} hover:brightness-125`}
          >
            {part}
          </button>
        );
      })}
    </>
  );
}
