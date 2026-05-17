/**
 * Shared helper for the 10 regional "でんき予報" (denki-yoho) power-demand
 * collectors. Each regional transmission/distribution utility publishes a
 * public daily 5-minute supply/demand CSV (juyo*.csv). We fetch the real
 * per-utility CSV, parse the last numeric load row, and emit a single
 * GeoJSON point at the utility HQ with the live load as properties.
 *
 * Endpoints verified against the existing `grid-usage-realtime` collector
 * (server/src/collectors/gridUsageRealtime.js) which already consumes the
 * same real public feeds. No fabricated data: a parse/fetch failure yields
 * an honest empty FeatureCollection with `_meta.source = '<x>_unavailable'`.
 */

import { fetchText } from './_liveHelpers.js';

function parseLastLoad(csv) {
  if (!csv) return null;
  const lines = csv.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  for (let i = lines.length - 1; i >= 0; i--) {
    const cols = lines[i].split(',');
    if (cols.length < 2) continue;
    const last = parseFloat(cols[cols.length - 1]);
    if (Number.isFinite(last) && last > 0) {
      return { load_mw: last, raw: lines[i] };
    }
  }
  return null;
}

/**
 * Build a denki-yoho collector.
 * @param {object} cfg
 * @param {string} cfg.sourceId   - source id (e.g. 'tepco-power')
 * @param {string} cfg.metaSource - base for _meta.source on success (e.g. 'tepco_power')
 * @param {string} cfg.operator   - operator display name (Japanese)
 * @param {string} cfg.csvUrl     - real public juyo CSV URL
 * @param {number} cfg.lat        - utility HQ latitude
 * @param {number} cfg.lon        - utility HQ longitude
 */
export function makeDenkiYohoCollector(cfg) {
  return async function collect() {
    let csv = null;
    try {
      csv = await fetchText(cfg.csvUrl, { timeoutMs: 10000 });
    } catch {
      csv = null;
    }
    const parsed = parseLastLoad(csv);
    const fetchedAt = new Date().toISOString();

    if (!parsed) {
      return {
        type: 'FeatureCollection',
        features: [],
        _meta: {
          source: `${cfg.metaSource}_unavailable`,
          fetchedAt,
          recordCount: 0,
          description: `${cfg.operator} でんき予報 demand feed (unreachable)`,
        },
      };
    }

    const features = [{
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [cfg.lon, cfg.lat] },
      properties: {
        source_id: cfg.sourceId,
        operator: cfg.operator,
        load_mw: parsed.load_mw,
        raw: parsed.raw,
        feed_url: cfg.csvUrl,
        country: 'JP',
        updated_at: fetchedAt,
        source: cfg.metaSource,
      },
    }];

    return {
      type: 'FeatureCollection',
      features,
      _meta: {
        source: cfg.metaSource,
        fetchedAt,
        recordCount: features.length,
        description: `${cfg.operator} でんき予報 latest 5-minute power demand`,
      },
    };
  };
}
