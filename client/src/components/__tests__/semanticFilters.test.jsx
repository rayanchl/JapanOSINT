// Semantic search filters (S1) and the vector-neighbour lists (S4/S5): the URL
// carries exactly the filters the user set, and a neighbour list says what it
// was built from — the server's counts, never a client guess.
import { describe, it, expect } from 'vitest';
import { semanticPath } from '../intel/IntelPage.jsx';
import { similarBasis } from '../intel/SimilarItems.jsx';

describe('semanticPath', () => {
  it('sends only the filters that are set', () => {
    const u = new URL(semanticPath({ q: '火災', semMode: 'vector' }), 'http://x');
    expect(u.pathname).toBe('/api/intel/semantic');
    expect(Object.fromEntries(u.searchParams)).toEqual({ q: '火災', mode: 'vector', limit: '50' });
  });
  it('normalises the source list and the bbox, passes since and record_type', () => {
    const u = new URL(semanticPath({
      q: 'fire', semMode: 'hybrid', source: ' a , ,b ', record_type: ' news ',
      since: '2026-10-01T00:00:00.000Z', bbox: '139, 35, 140, 36',
    }), 'http://x');
    expect(u.searchParams.get('source')).toBe('a,b');
    expect(u.searchParams.get('record_type')).toBe('news');
    expect(u.searchParams.get('since')).toBe('2026-10-01T00:00:00.000Z');
    expect(u.searchParams.get('bbox')).toBe('139,35,140,36');
  });
});

describe('similarBasis', () => {
  it('states how many pinned items had a vector, and the cap when it bit', () => {
    expect(similarBasis({ mode: 'case_related', case: { with_vector: 3, intel_items: 5, vectors_capped: 0, vector_cap: 1000 } }))
      .toBe('nearest to the mean of 3 of 5 pinned intel item(s) with a vector; pinned items excluded');
    expect(similarBasis({ mode: 'case_related', case: { with_vector: 1000, intel_items: 1200, vectors_capped: 200, vector_cap: 1000 } }))
      .toContain('200 past the 1000-vector cap');
  });
  it('names the item-similar basis and is silent without meta', () => {
    expect(similarBasis({ mode: 'similar' })).toMatch(/stored vector/);
    expect(similarBasis(null)).toBeNull();
  });
});
