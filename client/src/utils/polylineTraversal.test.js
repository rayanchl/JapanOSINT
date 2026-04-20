import { describe, it, expect } from 'vitest';
import { segmentLengthsMeters, advanceAlongLine } from './polylineTraversal.js';

describe('polylineTraversal', () => {
  // 2-segment line: ~90 km east, then ~110 km north.
  const coords = [[139.0, 35.0], [140.0, 35.0], [140.0, 36.0]];

  it('segmentLengthsMeters returns one length per segment', () => {
    const lens = segmentLengthsMeters(coords);
    expect(lens.length).toBe(2);
    expect(lens[0]).toBeGreaterThan(80_000);
    expect(lens[0]).toBeLessThan(120_000);
    expect(lens[1]).toBeGreaterThan(100_000);
  });

  it('advanceAlongLine moves a point forward and returns new state', () => {
    const lens = segmentLengthsMeters(coords);
    const start = { segIdx: 0, segOffset: 0 };
    const next = advanceAlongLine(coords, lens, start, 50_000);
    expect(next.segIdx).toBe(0);
    expect(next.segOffset).toBeCloseTo(50_000, -2);
    expect(next.lng).toBeGreaterThan(139.3);
    expect(next.lng).toBeLessThan(139.8);
    expect(next.lat).toBeCloseTo(35.0, 2);
    expect(typeof next.bearing).toBe('number');
  });

  it('advanceAlongLine crosses a segment boundary', () => {
    const lens = segmentLengthsMeters(coords);
    const start = { segIdx: 0, segOffset: 0 };
    const distance = lens[0] + 10_000; // cross into segment 1
    const next = advanceAlongLine(coords, lens, start, distance);
    expect(next.segIdx).toBe(1);
    expect(next.segOffset).toBeCloseTo(10_000, -2);
  });

  it('advanceAlongLine wraps at the end of the polyline', () => {
    const lens = segmentLengthsMeters(coords);
    const start = { segIdx: 1, segOffset: lens[1] - 100 };
    const next = advanceAlongLine(coords, lens, start, 500);
    expect(next.segIdx).toBe(0);
    expect(next.segOffset).toBeCloseTo(400, -1);
  });
});
