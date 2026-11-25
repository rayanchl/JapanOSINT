import React, { useEffect, useRef, useState } from 'react';
import { api } from '../../api/client.js';

/* A server-backed typeahead: the options are never preloaded. Each keystroke
 * (debounced) asks `pathFor(text)` for one small page, and `pick(response)`
 * turns the body into [{value, label, sub?}]. Used wherever a <select> would
 * otherwise need every registered source or collector in the browser
 * (~61k sources, ~640 collectors) to render one choice.
 *
 * `value` is the chosen key; `valueLabel` is what to show for it while the
 * user is not typing (the key itself is the honest fallback). */
export default function Typeahead({ value, valueLabel, onChange, pathFor, pick, label, placeholder = 'type to search…', clearLabel = 'any', className = '', debounceMs = 250 }) {
  const [text, setText] = useState('');
  const [open, setOpen] = useState(false);
  const [opts, setOpts] = useState([]);
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState(null);
  const seq = useRef(0);
  const pickRef = useRef(pick); pickRef.current = pick;

  useEffect(() => {
    if (!open) return undefined;
    const my = ++seq.current;
    const t = setTimeout(async () => {
      setBusy(true);
      try {
        const j = await api.get(pathFor(text.trim()));
        if (my !== seq.current) return;
        setOpts(pickRef.current(j) || []); setErr(null);
      } catch (e) { if (my === seq.current && e?.name !== 'AbortError') setErr(e); }
      finally { if (my === seq.current) setBusy(false); }
    }, text ? debounceMs : 0);
    return () => clearTimeout(t);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [text, open]);

  const choose = (v, opt) => { onChange(v, opt); setText(''); setOpen(false); };
  return (
    <div className={`relative ${className}`}>
      <input
        type="text" role="combobox" aria-label={label} aria-expanded={open} aria-autocomplete="list"
        className="px-2 py-1 rounded-md bg-osint-bg border border-osint-border text-xs text-osint-text w-full min-w-[160px]"
        placeholder={value ? (valueLabel || value) : placeholder}
        value={open ? text : (value ? (valueLabel || value) : '')}
        onFocus={() => setOpen(true)}
        onChange={(e) => { setText(e.target.value); setOpen(true); }}
        onBlur={() => setTimeout(() => setOpen(false), 150)}
      />
      {open && (
        <ul role="listbox" aria-label={`${label} options`} className="absolute z-30 mt-1 max-h-64 overflow-auto w-full min-w-[240px] rounded-md border border-osint-border bg-osint-surface shadow-lg text-xs">
          <li role="option" aria-selected={!value} className="px-2 py-1 cursor-pointer text-osint-muted hover:bg-white/5" onMouseDown={(e) => { e.preventDefault(); choose(''); }}>{clearLabel}</li>
          {busy && <li className="px-2 py-1 text-osint-muted">searching…</li>}
          {err && <li className="px-2 py-1 text-neon-red">search failed: {err.message}</li>}
          {!busy && !err && opts.length === 0 && <li className="px-2 py-1 text-osint-muted">no match</li>}
          {opts.map((o) => (
            <li key={o.value} role="option" aria-selected={o.value === value} className="px-2 py-1 cursor-pointer hover:bg-white/5" onMouseDown={(e) => { e.preventDefault(); choose(o.value, o); }}>
              <div className="text-osint-text truncate">{o.label}</div>
              {o.sub && <div className="font-mono text-[10px] text-osint-muted truncate">{o.sub}</div>}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
