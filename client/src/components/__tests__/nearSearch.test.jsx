// Nearby mode sends the text filter and asks for translations: the server's
// ?near= route now applies both (nearapi.c q/qAlt, httpd.c lang_view), so the
// client no longer hides the text box and the 原文/EN toggle in that mode.
import { describe, it, expect } from 'vitest';
import { nearParams } from '../intel/IntelPage.jsx';

describe('nearParams', () => {
  it('carries q and lang_view alongside the point and radius', () => {
    expect(nearParams({ near: '35.68,139.76', radius: '1000', q: 'fire' }))
      .toEqual({ near: '35.68,139.76', radius_m: '1000', q: 'fire', limit: 200, lang_view: 'both' });
  });
  it('omits an empty text filter rather than sending q=', () => {
    const p = nearParams({ near: '35.68,139.76', radius: '', q: '' });
    expect(p.q).toBeUndefined();
    expect(p.radius_m).toBeUndefined();
    expect(p.lang_view).toBe('both');
  });
});
