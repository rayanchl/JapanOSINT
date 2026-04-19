/**
 * Run the unified-transport fan-out end-to-end:
 *   1. Call each mode's unified collector to get stations/stops/ports/ships.
 *   2. For train/subway/bus modes, also call the LineString collectors that
 *      provide track / route geometry (rail tracks, subway tracks, MLIT N07
 *      bus routes).
 *   3. Upsert every emitted feature into `transport_stations` or
 *      `transport_lines` tables.
 *   4. Broadcast WebSocket events so the UI can react live.
 *
 * Mirrors `cameraRunner.js` — same `_inflightRun` flag, same hourly cadence,
 * same WS broadcast shape.
 */

import unifiedTrains from '../collectors/unifiedTrains.js';
import unifiedSubways from '../collectors/unifiedSubways.js';
import unifiedBuses from '../collectors/unifiedBuses.js';
import unifiedAisShips from '../collectors/unifiedAisShips.js';
import unifiedPortInfra from '../collectors/unifiedPortInfra.js';
import overpassRailTracks from '../collectors/overpassRailTracks.js';
import overpassSubwayTracks from '../collectors/overpassSubwayTracks.js';
import mlitN07BusRoutes from '../collectors/mlitN07BusRoutes.js';
import {
  upsertStationsTx,
  upsertLinesTx,
  transportStats,
} from './transportStore.js';

// Transport runs are cold-start-heavy: _liveHelpers' in-memory Overpass
// cache is lost on server restart, so the first run after boot does
// nationwide Overpass pulls through a 2-per-host / 500ms-gap queue. Give
// each collector up to 10 min and the whole run up to 45 min.
const RUN_CEILING_MS = 45 * 60 * 1000;
const STATION_COLLECTOR_TIMEOUT_MS = 10 * 60 * 1000;
const LINE_COLLECTOR_TIMEOUT_MS = 10 * 60 * 1000;

let _inflightRun = null;

function broadcast(wsServer, payload) {
  if (!wsServer) return;
  const msg = JSON.stringify(payload);
  for (const client of wsServer.clients) {
    if (client.readyState === 1) {
      try { client.send(msg); } catch {}
    }
  }
}

function isLineFeature(f) {
  const t = f?.geometry?.type;
  return t === 'LineString' || t === 'MultiLineString';
}

function withTimeout(promise, ms, label) {
  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      console.warn(`[transportRunner] ${label} timed out after ${ms}ms`);
      resolve(null);
    }, ms);
    Promise.resolve(promise)
      .then((v) => { clearTimeout(timer); resolve(v); })
      .catch((e) => {
        clearTimeout(timer);
        console.warn(`[transportRunner] ${label} failed: ${e?.message || e}`);
        resolve(null);
      });
  });
}

async function settleFc(fn, label, timeoutMs) {
  const fc = await withTimeout(
    Promise.resolve().then(() => fn()),
    timeoutMs,
    label,
  );
  return Array.isArray(fc?.features) ? fc.features : [];
}

async function runMode({ mode, source, stationCollector, lineCollectors = [] }) {
  const stations = await settleFc(
    stationCollector,
    `${mode}:stations`,
    STATION_COLLECTOR_TIMEOUT_MS,
  );
  const lineFeatures = [];
  for (let i = 0; i < lineCollectors.length; i++) {
    const items = await settleFc(
      lineCollectors[i],
      `${mode}:lines[${i}]`,
      LINE_COLLECTOR_TIMEOUT_MS,
    );
    for (const f of items) if (isLineFeature(f)) lineFeatures.push(f);
  }

  let stationResults = [];
  let lineResults = [];
  try {
    stationResults = stations.length ? upsertStationsTx(stations, mode, source) : [];
  } catch (e) {
    console.error(`[transportRunner] ${mode} station upsert failed:`, e?.message);
  }
  try {
    lineResults = lineFeatures.length ? upsertLinesTx(lineFeatures, mode, source) : [];
  } catch (e) {
    console.error(`[transportRunner] ${mode} line upsert failed:`, e?.message);
  }

  const summary = {
    mode,
    stations_in: stations.length,
    lines_in: lineFeatures.length,
    stations_new: stationResults.filter((r) => r.kind === 'new').length,
    stations_updated: stationResults.filter((r) => r.kind === 'updated').length,
    lines_new: lineResults.filter((r) => r.kind === 'new').length,
    lines_updated: lineResults.filter((r) => r.kind === 'updated').length,
  };
  console.log(`[transportRunner] ${mode} done — ${JSON.stringify(summary)}`);
  return summary;
}

export async function runTransportDiscovery(wsServer) {
  if (_inflightRun) return _inflightRun;

  const run_id = new Date().toISOString();
  const started = Date.now();
  const deadline = started + RUN_CEILING_MS;
  console.log(`[transportRunner] starting run ${run_id}`);
  broadcast(wsServer, { type: 'transport_run_start', run_id });

  const modes = [
    { mode: 'train',  source: 'unified_trains',     stationCollector: unifiedTrains,     lineCollectors: [overpassRailTracks] },
    { mode: 'subway', source: 'unified_subways',    stationCollector: unifiedSubways,    lineCollectors: [overpassSubwayTracks] },
    { mode: 'bus',    source: 'unified_buses',      stationCollector: unifiedBuses,      lineCollectors: [mlitN07BusRoutes] },
    { mode: 'ship',   source: 'unified_ais_ships',  stationCollector: unifiedAisShips,   lineCollectors: [] },
    { mode: 'port',   source: 'unified_port_infra', stationCollector: unifiedPortInfra,  lineCollectors: [] },
  ];

  const task = (async () => {
    const summaries = [];
    for (const cfg of modes) {
      if (Date.now() >= deadline) {
        console.warn(`[transportRunner] skipping ${cfg.mode} — run ceiling reached`);
        continue;
      }
      try {
        summaries.push(await runMode(cfg));
      } catch (err) {
        console.error(`[transportRunner] ${cfg.mode} crashed:`, err?.message);
      }
    }
    return summaries;
  })();

  _inflightRun = task;

  try {
    const summaries = await task;
    const elapsed = Date.now() - started;
    const dbTotals = {};
    for (const m of ['train', 'subway', 'bus', 'ship', 'port']) {
      dbTotals[m] = transportStats(m);
    }
    console.log(
      `[transportRunner] run ${run_id} done in ${(elapsed / 1000).toFixed(1)}s — ` +
      JSON.stringify(dbTotals),
    );
    broadcast(wsServer, {
      type: 'transport_run_end',
      run_id,
      elapsed_ms: elapsed,
      summaries: summaries || [],
      db_totals: dbTotals,
    });
    return summaries;
  } finally {
    _inflightRun = null;
  }
}

export function isTransportRunInFlight() {
  return _inflightRun !== null;
}
