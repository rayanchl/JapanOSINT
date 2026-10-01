import React from 'react';
import { describe, it, expect, afterEach } from 'vitest';
import { render, screen, cleanup } from '@testing-library/react';
import LayerPanel from '../LayerPanel.jsx';
import { buildCatalog } from '../../../hooks/layerCatalog.js';

// vitest runs with globals:false, so RTL cannot register its own afterEach.
afterEach(cleanup);

const SERVER = [
  { id: 'police-crime-points', name: 'Police Crime Points', category: 'crime', data_type: 'crime-report', modality: 'point', kind: 'curated', sources: [{ id: 's1' }], records_geocoded: 3 },
  { id: 'sakura-front', name: 'Sakura Front', category: 'wildlife', data_type: null, modality: null, kind: 'declared', sources: [], records_geocoded: null },
  { id: 'moon-bases', name: 'Moon Bases', category: 'selenology', data_type: null, modality: 'polygon', kind: 'declared', sources: [], records_geocoded: null },
];

const state = (visible = true) => ({ visible, opacity: 1, loading: false });

function setup(layerData, extra = {}) {
  const catalog = buildCatalog(SERVER, {});
  const layers = Object.fromEntries(Object.keys(catalog).map((id) => [id, state(true)]));
  render(
    <LayerPanel
      layers={layers}
      layerData={layerData}
      catalog={catalog}
      onToggleLayer={() => {}}
      onSetOpacity={() => {}}
      onSetTemporalWindow={() => {}}
      onSetAll={() => {}}
      {...extra}
    />,
  );
  return { catalog };
}

describe('LayerPanel', () => {
  it('groups by the server category and keeps a category it has never heard of', () => {
    setup({});
    expect(screen.getByTestId('category-crime')).toBeTruthy();
    expect(screen.getByTestId('category-selenology')).toBeTruthy();
    expect(screen.getByText('Selenology')).toBeTruthy();
    expect(screen.getByText('Moon Bases')).toBeTruthy();
  });

  it('shows the modality badge verbatim, geom for undeclared, and the data_type', () => {
    setup({});
    const badges = screen.getAllByTestId('modality-badge').map((b) => b.textContent);
    expect(badges.sort()).toEqual(['geom', 'poly', 'pt']);
    expect(screen.getByText('crime-report')).toBeTruthy();
  });

  it('distinguishes 404 / error / empty', () => {
    setup({
      'police-crime-points': { type: 'FeatureCollection', features: [], _error: 'HTTP 404', _status: 404, _notFound: true },
      'sakura-front': { type: 'FeatureCollection', features: [], _error: 'HTTP 502', _status: 502 },
      'moon-bases': { type: 'FeatureCollection', features: [], _meta: { records_available: 0, truncated: false } },
    });
    expect(screen.getByTestId('status-404').textContent).toBe('404');
    expect(screen.getByTestId('status-error').textContent).toBe('err');
    expect(screen.getByTestId('status-empty').textContent).toBe('0');
    expect(screen.queryByTestId('status-count')).toBeNull();
  });

  it('states "showing N of M" in band when the collection is truncated', () => {
    setup({
      'police-crime-points': {
        type: 'FeatureCollection',
        features: [{ type: 'Feature', geometry: null, properties: {} }, { type: 'Feature', geometry: null, properties: {} }],
        _meta: { records_available: 5000, records_used: 2, truncated: true, client_loading: true },
      },
    });
    expect(screen.getByTestId('truncation-notice').textContent).toMatch(/^Showing 2 of 5,000 records — loading the rest/);
    expect(screen.getByTestId('status-count').textContent).toBe('2');
  });

  it('is silent about truncation when the collection is complete', () => {
    setup({
      'police-crime-points': {
        type: 'FeatureCollection',
        features: [{ type: 'Feature', geometry: null, properties: {} }],
        _meta: { records_available: 1, truncated: false },
      },
    });
    expect(screen.queryByTestId('truncation-notice')).toBeNull();
  });

  it('surfaces renderer notices and the catalogue fetch state', () => {
    setup({}, {
      renderNotices: { 'moon-bases': [{ code: 'no_raster_url', message: 'no imagery URL' }] },
      catalogStatus: 'error',
      catalogError: 'HTTP 500',
    });
    expect(screen.getByTestId('render-notice-no_raster_url').textContent).toBe('no imagery URL');
    expect(screen.getByTestId('catalog-status').textContent).toMatch(/not obtained \(HTTP 500\)/);
  });
});
