// Regression (finding 10): SQLite's `YYYY-MM-DD HH:MM:SS` is UTC with no zone
// designator; new Date() read it as local time, shifting every server
// timestamp by the viewer's UTC offset.
import { describe, it, expect, vi, afterEach } from 'vitest';
import { parseServerTime, relativeTime, fmtAbs } from '../time.js';

afterEach(() => { vi.useRealTimers(); });

describe('parseServerTime', () => {
  it('reads SQLite datetime() output as UTC', () => {
    expect(parseServerTime('2026-10-02 03:04:05').toISOString()).toBe('2026-10-02T03:04:05.000Z');
    expect(parseServerTime('2026-10-02 03:04:05.250').toISOString()).toBe('2026-10-02T03:04:05.250Z');
    expect(parseServerTime('2026-10-02 03:04').toISOString()).toBe('2026-10-02T03:04:00.000Z');
  });

  it('leaves zoned ISO strings, bare dates and epoch ms alone', () => {
    expect(parseServerTime('2026-10-02T03:04:05Z').toISOString()).toBe('2026-10-02T03:04:05.000Z');
    expect(parseServerTime('2026-10-02T12:04:05+09:00').toISOString()).toBe('2026-10-02T03:04:05.000Z');
    expect(parseServerTime('2026-10-02').toISOString()).toBe('2026-10-02T00:00:00.000Z');
    expect(parseServerTime(0).toISOString()).toBe('1970-01-01T00:00:00.000Z');
    expect(Number.isNaN(parseServerTime('garbage').getTime())).toBe(true);
    expect(Number.isNaN(parseServerTime(null).getTime())).toBe(true);
  });
});

describe('relativeTime / fmtAbs on server timestamps', () => {
  it('a row written 30 s ago reads as 30 s ago, whatever the viewer zone', () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-10-02T03:04:35Z'));
    expect(relativeTime('2026-10-02 03:04:05')).toBe('30s ago');
  });

  it('formats in JST and falls back to the raw string when unparseable', () => {
    expect(fmtAbs('2026-10-02 03:04:05')).toContain('12:04:05');
    expect(fmtAbs('not a date')).toBe('not a date');
  });
});
