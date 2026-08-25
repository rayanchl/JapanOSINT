/**
 * Formatting helpers shared by transit popup components.
 */

/**
 * Format an absolute wall-clock-time-of-arrival (epoch seconds) relative to
 * `nowSec` (also epoch seconds). Returns "now" / "Nm" / "ShN m".
 */
export function fmtMinutes(wallSec, nowSec) {
  const raw = wallSec - nowSec;
  const mins = Math.round(raw / 60);
  if (mins < 1) return 'now';
  if (mins < 60) return `${mins}m`;
  return `${Math.floor(mins / 60)}h${mins % 60}m`;
}

/**
 * Format a delay in seconds as `+Nm` / `-Nm`. Returns null for delays under
 * 30 seconds (treated as on-time).
 */
export function fmtDelay(sec) {
  if (sec == null) return null;
  if (Math.abs(sec) < 30) return null;
  const mins = Math.round(sec / 60);
  return mins > 0 ? `+${mins}m` : `${mins}m`;
}
