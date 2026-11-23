import { useEffect, useMemo, useState } from 'react';
import { computeGroundTrack } from '../utils/groundTrack.js';
import { satelliteColor } from '../utils/satelliteColor.js';

const REFRESH_MS = 60 * 1000;

export function buildTracks(fc) {
  if (!fc || !Array.isArray(fc.features)) return [];
  const out = [];
  for (const feat of fc.features) {
    const p = feat?.properties || {};
    if (!p.tle_line1 || !p.tle_line2) continue;
    try {
      const geom = computeGroundTrack(p.tle_line1, p.tle_line2, { minutes: 90, stepSec: 30 });
      if (!geom) continue;
      out.push({
        type: 'Feature',
        geometry: geom,
        properties: {
          satellite_id: p.norad_id,
          satellite_name: p.name,
          color: satelliteColor(p.norad_id),
        },
      });
    } catch { /* skip broken TLE */ }
  }
  return out;
}

export function useSatelliteTracks(satelliteFc) {
  // `tick` only forces the tracks to be re-derived (they depend on the clock).
  const [tick, setTick] = useState(0);

  useEffect(() => {
    if (!satelliteFc?.features?.length) return undefined;
    const id = setInterval(() => setTick((n) => n + 1), REFRESH_MS);
    return () => clearInterval(id);
  }, [satelliteFc]);

  // eslint-disable-next-line react-hooks/exhaustive-deps -- `tick` is the refresh trigger, not a value read inside
  return useMemo(() => ({ type: 'FeatureCollection', features: buildTracks(satelliteFc) }), [satelliteFc, tick]);
}
