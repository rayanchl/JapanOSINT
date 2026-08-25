/**
 * Shared time formatters. Kept in one place so the panels don't drift apart
 * (SourcesPanel and DatabaseSchedulerTab used to carry divergent copies).
 */

/**
 * Compact relative time — `42s ago` / `3h ago`, and `in 42s` for timestamps
 * in the future (scheduler "next run" values).
 */
export function relativeTime(iso) {
  if (!iso) return 'never';
  const ts = new Date(iso).getTime();
  if (Number.isNaN(ts)) return 'never';
  const now = Date.now();
  const diff = Math.abs(now - ts);
  const future = ts > now;
  const s = Math.floor(diff / 1000);
  const suffix = (v) => future ? `in ${v}` : `${v} ago`;
  if (s < 60) return suffix(`${s}s`);
  const m = Math.floor(s / 60);
  if (m < 60) return suffix(`${m}m`);
  const h = Math.floor(m / 60);
  if (h < 24) return suffix(`${h}h`);
  return suffix(`${Math.floor(h / 24)}d`);
}

/** Absolute JST timestamp, falling back to the raw string on bad input. */
export function fmtAbs(iso) {
  if (!iso) return '—';
  try {
    return new Date(iso).toLocaleString('en-GB', {
      timeZone: 'Asia/Tokyo',
      dateStyle: 'short',
      timeStyle: 'medium',
    });
  } catch { return iso; }
}
