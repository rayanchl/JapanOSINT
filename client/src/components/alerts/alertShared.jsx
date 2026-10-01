import React from 'react';
import { LuPlus, LuTrash2 } from 'react-icons/lu';
import { Button, Input, Field, Select, Pill, cx } from '../ui/kit.jsx';

/**
 * Pieces shared by the alert-rule editor and "turn into alert" flows.
 *
 * Server contract (native/core/alertsapi.c validate_rule):
 *   channels: [{type:'email'|'webhook', target, secret?}]  — ≥1 required
 *   email target must be an address; webhook target is host-gated and its
 *   secret must be ≥16 chars. Reads mask webhook secrets as "••••", so a
 *   channel list echoed back unchanged is rejected — see `channelsDirty`.
 */
export const MASKED = '••••';

export function emptyChannel(type = 'email') {
  return type === 'email' ? { type: 'email', target: '' } : { type: 'webhook', target: '', secret: '' };
}

export function channelSummary(c) {
  if (!c) return '';
  return `${c.type === 'webhook' ? 'webhook' : 'email'} ${c.target || '(no target)'}`;
}

/** Client-side mirror of the server's checks so the form can say why before a round-trip. */
export function validateChannels(channels) {
  if (!Array.isArray(channels) || channels.length === 0) return 'At least one channel is required.';
  for (const c of channels) {
    if (c.type === 'email') {
      if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(c.target || '')) return `"${c.target || ''}" is not a valid email address.`;
    } else if (c.type === 'webhook') {
      if (!/^https?:\/\//i.test(c.target || '')) return 'Webhook target must be an http(s) URL.';
      if (!c.secret || c.secret === MASKED) return 'Webhook signing secret is required (the stored one is never echoed back — re-enter it to change channels).';
      if (c.secret.length < 16) return 'Webhook secret must be at least 16 characters.';
    } else {
      return `Unknown channel type "${c.type}".`;
    }
  }
  return null;
}

export function ChannelsEditor({ channels, onChange, className }) {
  const set = (i, patch) => onChange(channels.map((c, j) => (j === i ? { ...c, ...patch } : c)));
  const remove = (i) => onChange(channels.filter((_, j) => j !== i));
  return (
    <div className={cx('space-y-2', className)}>
      {channels.length === 0 && <div className="text-[11px] text-osint-muted">No channels — a rule with nowhere to deliver is rejected by the server.</div>}
      {channels.map((c, i) => (
        <div key={i} className="rounded-md border border-osint-border bg-osint-bg p-2 space-y-2">
          <div className="flex items-center gap-2">
            <Select value={c.type} onChange={(e) => set(i, emptyChannel(e.target.value))} className="text-xs">
              <option value="email">Email</option>
              <option value="webhook">Webhook</option>
            </Select>
            <Input
              mono
              className="flex-1"
              placeholder={c.type === 'email' ? 'analyst@example.com' : 'https://hooks.example.com/…'}
              value={c.target}
              onChange={(e) => set(i, { target: e.target.value })}
            />
            <Button size="sm" variant="ghost" onClick={() => remove(i)} title="Remove channel"><LuTrash2 size={13} /></Button>
          </div>
          {c.type === 'webhook' && (
            <Field label="Signing secret (≥16 chars)" hint={c.secret === MASKED ? 'A secret is stored. Leave the mask to keep the channel list untouched, or re-enter to change it.' : undefined}>
              <Input mono type="password" autoComplete="new-password" value={c.secret ?? ''} onChange={(e) => set(i, { secret: e.target.value })} placeholder="shared HMAC secret" />
            </Field>
          )}
        </div>
      ))}
      <div className="flex gap-2">
        <Button size="sm" onClick={() => onChange([...channels, emptyChannel('email')])}><LuPlus size={12} /> Email</Button>
        <Button size="sm" onClick={() => onChange([...channels, emptyChannel('webhook')])}><LuPlus size={12} /> Webhook</Button>
      </div>
    </div>
  );
}

export function ChannelPills({ channels }) {
  if (!Array.isArray(channels) || channels.length === 0) return <Pill tone="danger">no channel</Pill>;
  return (
    <>
      {channels.map((c, i) => (
        <Pill key={i} tone={c.type === 'webhook' ? 'cyan' : 'neutral'} title={c.target}>{c.type === 'webhook' ? 'hook' : 'mail'} {String(c.target || '').replace(/^https?:\/\//, '').slice(0, 28)}</Pill>
      ))}
    </>
  );
}

/** ISO string in the future → the rule is muted. */
export function isMuted(mutedUntil) {
  if (!mutedUntil) return false;
  const t = new Date(mutedUntil.includes('T') ? mutedUntil : mutedUntil.replace(' ', 'T') + 'Z').getTime();
  return Number.isFinite(t) && t > Date.now();
}

export function muteLabel(mutedUntil) {
  if (!isMuted(mutedUntil)) return null;
  const t = new Date(mutedUntil.includes('T') ? mutedUntil : mutedUntil.replace(' ', 'T') + 'Z');
  const years = (t.getTime() - Date.now()) / (365 * 864e5);
  if (years > 50) return 'muted forever';
  return `muted until ${t.toLocaleString('en-GB', { timeZone: 'Asia/Tokyo', dateStyle: 'short', timeStyle: 'short' })}`;
}

export const MUTE_OPTIONS = [
  { label: 'Mute 1h', value: 3600 },
  { label: 'Mute 24h', value: 86400 },
  { label: 'Mute 7d', value: 7 * 86400 },
  { label: 'Mute forever', value: 'forever' },
];

/** Comma-separated text ↔ array helpers for the predicate list fields. */
export function splitList(s) {
  return String(s || '').split(',').map((x) => x.trim()).filter(Boolean);
}
export function joinList(a) { return Array.isArray(a) ? a.join(', ') : ''; }

/** One-line human summary of a predicate, for rule rows. */
export function predicateSummary(p) {
  if (!p || typeof p !== 'object') return 'matches everything';
  const parts = [];
  if (p.mode === 'llm' && p.nl_query) parts.push(`LLM “${p.nl_query}”`);
  if (p.q) parts.push(`q=${p.q}`);
  if (p.source_ids?.length) parts.push(`${p.source_ids.length} source${p.source_ids.length === 1 ? '' : 's'}`);
  if (p.tags_any?.length) parts.push(`tags any [${p.tags_any.join(', ')}]`);
  if (p.tags_all?.length) parts.push(`tags all [${p.tags_all.join(', ')}]`);
  if (p.entity_ids?.length) parts.push(`${p.entity_ids.length} entit${p.entity_ids.length === 1 ? 'y' : 'ies'}`);
  if (p.entity_types?.length) parts.push(`types [${p.entity_types.join(', ')}]`);
  if (p.bbox) parts.push('bbox');
  if (p.polygon) parts.push('polygon');
  if (p.circle) parts.push(`circle ${Math.round(p.circle.radius_m)}m`);
  if (p.aoi_id) parts.push(`AOI ${p.aoi_id}`);
  return parts.length ? parts.join(' · ') : 'matches everything';
}
