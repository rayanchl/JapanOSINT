/**
 * Shared time formatters. Kept in one place so the panels don't drift apart
 * (SourcesPanel and DatabaseSchedulerTab used to carry divergent copies).
 */

// SQLite's datetime('now') / CURRENT_TIMESTAMP: `YYYY-MM-DD HH:MM:SS[.fff]`,
// always UTC, with no zone designator. `new Date()` reads a zone-less
// date-time as LOCAL time, so every such server timestamp was shifted by the
// viewer's UTC offset (9 h in Japan: "fetched 9h ago" for a fetch just now).
// Only the space-separated form is rewritten: that is SQLite's own spelling,
// whereas a `T`-separated zone-less string can come from an upstream feed and
// its zone is not ours to guess.
const SQLITE_UTC = /^(\d{4}-\d{2}-\d{2}) (\d{2}:\d{2}(?::\d{2}(?:\.\d+)?)?)$/;

/**
 * Parse a timestamp the SERVER produced. SQLite's zone-less
 * `YYYY-MM-DD HH:MM:SS` is UTC; anything else (ISO with `Z` or an offset, a
 * bare date, epoch ms) goes to the platform parser. Returns a Date (possibly
 * an Invalid Date — callers check).
 */
export function parseServerTime(v) {
  if (v instanceof Date) return v;
  if (typeof v === 'number') return new Date(v);
  if (typeof v !== 'string') return new Date(NaN);
  const s = v.trim();
  const m = SQLITE_UTC.exec(s);
  return new Date(m ? `${m[1]}T${m[2]}Z` : s);
}

/**
 * Compact relative time — `42s ago` / `3h ago`, and `in 42s` for timestamps
 * in the future (scheduler "next run" values).
 */
export function relativeTime(iso) {
  if (!iso) return 'never';
  const ts = parseServerTime(iso).getTime();
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
    const d = parseServerTime(iso);
    if (Number.isNaN(d.getTime())) return String(iso);
    return d.toLocaleString('en-GB', {
      timeZone: 'Asia/Tokyo',
      dateStyle: 'short',
      timeStyle: 'medium',
    });
  } catch { return iso; }
}
