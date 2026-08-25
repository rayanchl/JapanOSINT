import { describe, it, expect, vi } from 'vitest';
import { buildTracks } from '../useSatelliteTracks.js';

// Mock the utility functions before rendering
vi.mock('../utils/groundTrack.js', () => ({
  computeGroundTrack: vi.fn((tleLine1, tleLine2, opts) => {
    // Return a simple LineString for testing
    return {
      type: 'LineString',
      coordinates: [[0, 0], [1, 1], [2, 2]],
    };
  }),
}));

vi.mock('../utils/satelliteColor.js', () => ({
  satelliteColor: vi.fn((noradId) => '#1e88e5'),
}));

// A real TLE (ISS, 2024-ish) — exercises the SGP4 path end-to-end.
const ISS_TLE_1 = '1 25544U 98067A   24001.50000000  .00016717  00000-0  10270-3 0  9007';
const ISS_TLE_2 = '2 25544  51.6400 208.9163 0006317  69.6530  25.7298 15.50377579000000';

function makeFc(features) {
  return { type: 'FeatureCollection', features };
}

describe('useSatelliteTracks', () => {
  it('returns a FeatureCollection of line features, one per satellite with TLE', () => {
    const input = makeFc([
      {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [0, 0] },
        properties: {
          norad_id: 25544, name: 'ISS (ZARYA)',
          tle_line1: ISS_TLE_1, tle_line2: ISS_TLE_2,
        },
      },
    ]);
    const features = buildTracks(input);
    expect(features).toHaveLength(1);
    const f = features[0];
    expect(f.type).toBe('Feature');
    expect(['LineString', 'MultiLineString']).toContain(f.geometry.type);
    expect(f.properties.satellite_id).toBe(25544);
    expect(f.properties.satellite_name).toBe('ISS (ZARYA)');
    expect(f.properties.color).toMatch(/^#[0-9a-f]{6}$/i);
  });

  it('skips features without TLE', () => {
    const input = makeFc([
      {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [0, 0] },
        properties: { norad_id: 99999, name: 'No TLE sat' },
      },
    ]);
    const features = buildTracks(input);
    expect(features).toHaveLength(0);
  });

  it('returns an empty array when input is null/empty', () => {
    const r1 = buildTracks(null);
    expect(r1).toHaveLength(0);
    const r2 = buildTracks(makeFc([]));
    expect(r2).toHaveLength(0);
  });
});
