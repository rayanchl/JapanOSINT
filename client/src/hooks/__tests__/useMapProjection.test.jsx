// Regression (finding 3): MapPage passed `[reverse.lon, reverse.lat]` built
// fresh every render; the hook depended on the array identity and called
// setPos synchronously in its effect, so every render scheduled another one —
// ~130k renders per 500 ms behind the reverse-geocode card.
import React, { useRef } from 'react';
import { createRoot } from 'react-dom/client';
import { describe, it, expect } from 'vitest';
import useMapProjection from '../useMapProjection.js';

function countRenders(lngLatFor, ms = 300) {
  return new Promise((resolve) => {
    let renders = 0;
    const map = { project: () => ({ x: 1, y: 2 }), on() {}, off() {} };
    function C() {
      renders += 1;
      const ref = useRef(map);
      useMapProjection(ref, lngLatFor());
      return null;
    }
    const prev = globalThis.IS_REACT_ACT_ENVIRONMENT;
    globalThis.IS_REACT_ACT_ENVIRONMENT = false;
    const root = createRoot(document.createElement('div'));
    root.render(<C />);
    setTimeout(() => { const n = renders; root.unmount(); globalThis.IS_REACT_ACT_ENVIRONMENT = prev; resolve(n); }, ms);
  });
}

describe('useMapProjection', () => {
  it('does not loop when the caller builds a fresh [lng, lat] array every render', async () => {
    const stable = [139, 35];
    const stableRenders = await countRenders(() => stable);
    const freshRenders = await countRenders(() => [139, 35]);
    expect(freshRenders).toBeLessThanOrEqual(stableRenders + 1);
    expect(freshRenders).toBeLessThan(10);
  });

  it('re-projects when the coordinates actually change', async () => {
    const seen = [];
    const map = { project: ([lng, lat]) => ({ x: lng, y: lat }), on() {}, off() {} };
    let setPoint;
    function C() {
      const [pt, sp] = React.useState([139, 35]);
      setPoint = sp;
      const ref = useRef(map);
      const pos = useMapProjection(ref, [pt[0], pt[1]]);
      if (pos) seen.push(`${pos.x},${pos.y}`);
      return null;
    }
    globalThis.IS_REACT_ACT_ENVIRONMENT = false;
    const root = createRoot(document.createElement('div'));
    root.render(<C />);
    await new Promise((r) => setTimeout(r, 50));
    setPoint([140, 36]);
    await new Promise((r) => setTimeout(r, 50));
    root.unmount();
    expect(seen).toContain('139,35');
    expect(seen[seen.length - 1]).toBe('140,36');
  });
});
