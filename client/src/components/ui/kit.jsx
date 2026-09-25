import React, { useEffect, useRef, useState } from 'react';
import { errorMessage } from '../../api/client.js';

/* ------------------------------------------------------------------------
 * Shared UI kit — the web counterpart of the iOS design tokens
 * (`Theme.swift`): one card radius, one section-label style, one pill, one
 * honest error notice. Pages compose these instead of re-inventing them.
 * ---------------------------------------------------------------------- */

export function cx(...parts) { return parts.filter(Boolean).join(' '); }

/** Page scaffold: scrollable body with a max width and a header. */
export function Page({ title, subtitle, actions, children, wide = false, className }) {
  return (
    <div className={cx('h-full overflow-auto', className)}>
      <div className={cx('mx-auto p-4 md:p-6 space-y-4', wide ? 'max-w-7xl' : 'max-w-4xl')}>
        {(title || actions) && (
          <header className="flex items-start justify-between gap-4 flex-wrap">
            <div className="min-w-0">
              {title && <h1 className="font-mono text-xl font-bold tracking-tight text-osint-text">{title}</h1>}
              {subtitle && <p className="text-xs text-osint-muted mt-0.5 max-w-2xl">{subtitle}</p>}
            </div>
            {actions && <div className="flex items-center gap-2 flex-shrink-0">{actions}</div>}
          </header>
        )}
        {children}
      </div>
    </div>
  );
}

/** Uppercase tracked section label (`sectionLabel` on iOS). */
export function SectionLabel({ children, right, className }) {
  return (
    <div className={cx('flex items-center justify-between gap-2', className)}>
      <div className="font-mono text-[10px] font-semibold uppercase tracking-[0.12em] text-osint-muted">{children}</div>
      {right}
    </div>
  );
}

export function Card({ children, className, padded = true, onClick, as: Tag = 'div', ...rest }) {
  return (
    <Tag
      onClick={onClick}
      className={cx(
        'rounded-[10px] border border-osint-border bg-osint-surface',
        padded && 'p-3 md:p-4',
        onClick && 'cursor-pointer hover:border-accent/50 transition-colors text-left w-full',
        className,
      )}
      {...rest}
    >
      {children}
    </Tag>
  );
}

/** Section = label + card, the inset-grouped-list idiom from iOS. */
export function Section({ label, right, children, className, padded = true }) {
  return (
    <section className={cx('space-y-1.5', className)}>
      {label && <SectionLabel right={right}>{label}</SectionLabel>}
      <Card padded={padded}>{children}</Card>
    </section>
  );
}

/** A labelled row inside a Section (`LabeledContent`). */
export function Row({ label, children, hint, className, onClick }) {
  const Tag = onClick ? 'button' : 'div';
  return (
    <Tag
      type={onClick ? 'button' : undefined}
      onClick={onClick}
      className={cx(
        'flex items-center justify-between gap-3 py-2 first:pt-0 last:pb-0 border-b border-osint-border last:border-0 w-full text-left',
        onClick && 'hover:text-accent',
        className,
      )}
    >
      <div className="min-w-0">
        <div className="text-sm text-osint-text">{label}</div>
        {hint && <div className="text-[11px] text-osint-muted">{hint}</div>}
      </div>
      <div className="text-sm text-osint-muted font-mono text-right flex-shrink-0 flex items-center gap-2">{children}</div>
    </Tag>
  );
}

const TONES = {
  neutral: 'border-osint-border text-osint-muted bg-osint-panel',
  accent: 'border-accent/40 text-accent bg-accent/10',
  cyan: 'border-neon-cyan/40 text-neon-cyan bg-neon-cyan/10',
  success: 'border-neon-green/40 text-neon-green bg-neon-green/10',
  warning: 'border-accent/40 text-accent bg-accent/10',
  danger: 'border-neon-red/40 text-neon-red bg-neon-red/10',
  purple: 'border-neon-purple/40 text-neon-purple bg-neon-purple/10',
};

export function Pill({ tone = 'neutral', children, className, title, mono = true }) {
  return (
    <span
      title={title}
      className={cx('inline-flex items-center gap-1 px-1.5 py-0.5 rounded-full border text-[10px] leading-4 whitespace-nowrap',
        mono && 'font-mono', TONES[tone] || TONES.neutral, className)}
    >
      {children}
    </span>
  );
}

const BTN = {
  primary: 'bg-accent/15 text-accent border-accent/40 hover:bg-accent/25',
  secondary: 'bg-transparent text-osint-text border-osint-border hover:border-accent/40 hover:text-accent',
  ghost: 'bg-transparent text-osint-muted border-transparent hover:text-osint-text hover:bg-white/5',
  danger: 'bg-neon-red/10 text-neon-red border-neon-red/40 hover:bg-neon-red/20',
  cyan: 'bg-neon-cyan/10 text-neon-cyan border-neon-cyan/40 hover:bg-neon-cyan/20',
};

export function Button({ variant = 'secondary', size = 'md', busy = false, disabled, className, children, type = 'button', ...rest }) {
  const sz = size === 'sm' ? 'px-2 py-1 text-[11px]' : size === 'lg' ? 'px-4 py-2.5 text-sm' : 'px-3 py-1.5 text-xs';
  return (
    <button
      type={type}
      disabled={disabled || busy}
      className={cx('inline-flex items-center justify-center gap-1.5 rounded-md border font-medium transition-colors disabled:opacity-40 disabled:cursor-not-allowed',
        sz, BTN[variant] || BTN.secondary, className)}
      {...rest}
    >
      {busy && <Spinner size={12} />}
      {children}
    </button>
  );
}

export function Spinner({ size = 16, className }) {
  return (
    <span
      className={cx('inline-block rounded-full border-2 border-osint-border border-t-accent animate-spin', className)}
      style={{ width: size, height: size }}
      aria-label="loading"
    />
  );
}

export function Input({ className, mono, ...rest }) {
  return (
    <input
      className={cx('w-full px-3 py-2 rounded-md bg-osint-bg border border-osint-border text-sm text-osint-text placeholder:text-osint-muted/70 focus:border-accent/60 outline-none',
        mono && 'font-mono', className)}
      {...rest}
    />
  );
}

export function TextArea({ className, mono, ...rest }) {
  return (
    <textarea
      className={cx('w-full px-3 py-2 rounded-md bg-osint-bg border border-osint-border text-sm text-osint-text placeholder:text-osint-muted/70 focus:border-accent/60 outline-none min-h-[80px]',
        mono && 'font-mono', className)}
      {...rest}
    />
  );
}

export function Select({ className, children, ...rest }) {
  return (
    <select
      className={cx('px-2 py-1.5 rounded-md bg-osint-bg border border-osint-border text-sm text-osint-text focus:border-accent/60 outline-none', className)}
      {...rest}
    >
      {children}
    </select>
  );
}

export function Field({ label, hint, children, className }) {
  return (
    <label className={cx('block space-y-1', className)}>
      {label && <div className="text-[11px] font-medium text-osint-muted">{label}</div>}
      {children}
      {hint && <div className="text-[11px] text-osint-muted/80">{hint}</div>}
    </label>
  );
}

export function Toggle({ on, onChange, label, disabled }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={on}
      disabled={disabled}
      onClick={() => onChange?.(!on)}
      className={cx('flex items-center gap-2 disabled:opacity-40', label && 'w-full justify-between')}
    >
      {label && <span className="text-sm text-osint-text">{label}</span>}
      <span className={cx('toggle-switch', on && 'on')} />
    </button>
  );
}

/** Segmented control (`FilterControl` on iOS). options: [{value,label,count?}] */
export function Segmented({ value, onChange, options, className, size = 'sm' }) {
  return (
    <div className={cx('inline-flex rounded-md border border-osint-border bg-osint-bg p-0.5 gap-0.5', className)}>
      {options.map((o) => (
        <button
          key={String(o.value)}
          type="button"
          onClick={() => onChange(o.value)}
          className={cx('rounded px-2 font-medium transition-colors whitespace-nowrap',
            size === 'sm' ? 'py-0.5 text-[11px]' : 'py-1 text-xs',
            value === o.value ? 'bg-accent/15 text-accent' : 'text-osint-muted hover:text-osint-text')}
        >
          {o.label}
          {o.count != null && <span className="ml-1 font-mono opacity-70">{o.count}</span>}
        </button>
      ))}
    </div>
  );
}

/**
 * A failed request, rendered as such. Never render an empty state on an
 * error — that is a claim about the data made out of a 500.
 */
export function ErrorNotice({ error, title = 'Request failed', onRetry, className, children }) {
  if (!error && !children) return null;
  return (
    <div className={cx('rounded-md border border-neon-red/40 bg-neon-red/10 px-3 py-2 text-xs text-neon-red', className)} role="alert">
      <div className="flex items-start justify-between gap-2">
        <div className="min-w-0">
          <span className="font-semibold">{title}</span>
          {error && <span className="font-mono"> — {errorMessage(error)}</span>}
          {children && <div className="mt-1 text-neon-red/80">{children}</div>}
          <div className="mt-0.5 text-neon-red/70">This is not a statement that nothing exists.</div>
        </div>
        {onRetry && <Button size="sm" variant="danger" onClick={onRetry}>Retry</Button>}
      </div>
    </div>
  );
}

export function EmptyState({ icon, title, children, action, className }) {
  return (
    <div className={cx('text-center py-10 px-4', className)}>
      {icon && <div className="text-2xl mb-2 text-osint-muted">{icon}</div>}
      <div className="text-sm text-osint-text font-medium">{title}</div>
      {children && <div className="text-xs text-osint-muted mt-1 max-w-md mx-auto">{children}</div>}
      {action && <div className="mt-3">{action}</div>}
    </div>
  );
}

export function LoadingState({ label = 'Loading…', className }) {
  return (
    <div className={cx('flex items-center gap-2 text-xs text-osint-muted py-6 justify-center', className)}>
      <Spinner size={14} /> {label}
    </div>
  );
}

/** Data value in mono accent (`Typography.mono`). */
export function Mono({ children, className, tone }) {
  return <span className={cx('font-mono', tone === 'accent' ? 'text-accent' : tone === 'cyan' ? 'text-neon-cyan' : '', className)}>{children}</span>;
}

/** "Showing N of M" — a bounded view must state the bound in-band. */
export function BoundNote({ shown, total, noun = 'records', className }) {
  if (total == null || shown == null) return null;
  if (shown >= total) return <div className={cx('text-[11px] text-osint-muted font-mono', className)}>{total} {noun}</div>;
  return (
    <div className={cx('text-[11px] text-accent font-mono', className)}>
      showing {shown} of {total} {noun}
    </div>
  );
}

/** Modal sheet. Closes on backdrop click / Escape. */
export function Sheet({ open, onClose, title, children, footer, width = 'max-w-lg' }) {
  const ref = useRef(null);
  useEffect(() => {
    if (!open) return undefined;
    const onKey = (e) => { if (e.key === 'Escape') onClose?.(); };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [open, onClose]);
  if (!open) return null;
  return (
    <div
      className="fixed inset-0 z-[100] flex items-end md:items-center justify-center bg-black/60 backdrop-blur-sm p-0 md:p-6"
      onMouseDown={(e) => { if (e.target === e.currentTarget) onClose?.(); }}
    >
      <div
        ref={ref}
        role="dialog"
        aria-modal="true"
        className={cx('w-full rounded-t-2xl md:rounded-[14px] border border-osint-border bg-osint-surface shadow-2xl max-h-[92vh] flex flex-col', width)}
      >
        {title && (
          <div className="flex items-center justify-between px-4 py-3 border-b border-osint-border">
            <h2 className="font-mono text-sm font-semibold text-osint-text">{title}</h2>
            <button type="button" onClick={onClose} className="text-osint-muted hover:text-osint-text text-lg leading-none px-1" aria-label="Close">×</button>
          </div>
        )}
        <div className="p-4 overflow-auto flex-1">{children}</div>
        {footer && <div className="px-4 py-3 border-t border-osint-border flex justify-end gap-2">{footer}</div>}
      </div>
    </div>
  );
}

/** Destructive confirmation (`confirmationDialog`). */
export function ConfirmDialog({ open, onClose, onConfirm, title, message, confirmLabel = 'Confirm', danger = true, busy }) {
  return (
    <Sheet open={open} onClose={onClose} title={title} width="max-w-sm"
      footer={(
        <>
          <Button onClick={onClose}>Cancel</Button>
          <Button variant={danger ? 'danger' : 'primary'} busy={busy} onClick={onConfirm}>{confirmLabel}</Button>
        </>
      )}
    >
      <div className="text-sm text-osint-muted">{message}</div>
    </Sheet>
  );
}

/** Lightweight toast bus. `toast('saved')` from anywhere; <Toaster/> in shell. */
const toastListeners = new Set();
export function toast(message, { tone = 'neutral', ttl = 3500 } = {}) {
  const t = { id: Math.random().toString(36).slice(2), message, tone, ttl };
  toastListeners.forEach((l) => l(t));
}
export function Toaster() {
  const [items, setItems] = useState([]);
  useEffect(() => {
    const l = (t) => {
      setItems((xs) => [...xs, t]);
      setTimeout(() => setItems((xs) => xs.filter((x) => x.id !== t.id)), t.ttl);
    };
    toastListeners.add(l);
    return () => toastListeners.delete(l);
  }, []);
  if (!items.length) return null;
  return (
    <div className="fixed bottom-16 md:bottom-4 left-1/2 -translate-x-1/2 z-[120] space-y-1.5 pointer-events-none">
      {items.map((t) => (
        <div key={t.id} className={cx('px-3 py-1.5 rounded-md border text-xs font-mono shadow-lg backdrop-blur', TONES[t.tone] || TONES.neutral)}>
          {t.message}
        </div>
      ))}
    </div>
  );
}

/** Copy-to-clipboard button with feedback. */
export function CopyButton({ text, label = 'Copy', size = 'sm' }) {
  const [done, setDone] = useState(false);
  return (
    <Button size={size} onClick={async () => {
      try { await navigator.clipboard.writeText(text); setDone(true); setTimeout(() => setDone(false), 1200); }
      catch { toast('Clipboard unavailable', { tone: 'danger' }); }
    }}>
      {done ? 'Copied' : label}
    </Button>
  );
}

/** Key/value grid for detail views. `pairs`: [[k, v], …]; nullish v skipped. */
export function KV({ pairs, className }) {
  const rows = (pairs || []).filter(([, v]) => v !== null && v !== undefined && v !== '');
  if (!rows.length) return null;
  return (
    <dl className={cx('grid grid-cols-[auto,1fr] gap-x-3 gap-y-1 text-xs', className)}>
      {rows.map(([k, v]) => (
        <React.Fragment key={k}>
          <dt className="text-osint-muted whitespace-nowrap">{k}</dt>
          <dd className="font-mono text-osint-text break-all min-w-0">{typeof v === 'object' && !React.isValidElement(v) ? JSON.stringify(v) : v}</dd>
        </React.Fragment>
      ))}
    </dl>
  );
}
