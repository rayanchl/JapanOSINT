import { describe, it, expect } from 'vitest';
import { satelliteColor } from '../satelliteColor.js';

describe('satelliteColor', () => {
  it('returns a hex color for a numeric NORAD id', () => {
    const c = satelliteColor(25544);
    expect(c).toMatch(/^#[0-9a-f]{6}$/i);
  });

  it('is deterministic — same id returns same color', () => {
    expect(satelliteColor(25544)).toBe(satelliteColor(25544));
    expect(satelliteColor('25544')).toBe(satelliteColor(25544));
  });

  it('distributes across the palette (different ids often get different colors)', () => {
    const colors = new Set();
    for (let i = 0; i < 100; i += 1) colors.add(satelliteColor(10000 + i));
    expect(colors.size).toBeGreaterThan(10);
  });

  it('falls back to a default color when id is null/undefined', () => {
    expect(satelliteColor(null)).toMatch(/^#[0-9a-f]{6}$/i);
    expect(satelliteColor(undefined)).toMatch(/^#[0-9a-f]{6}$/i);
  });
});
