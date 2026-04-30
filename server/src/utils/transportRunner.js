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
import { snapStationsToNearestLine } from './transportSpatialSnap.js';
import { withCollectorRun } from './collectorTap.js';
import { runStationClusterer } from './stationClusterer.js';
import collectOsmTransportStationBoundaries from '../collectors/osmTransportStationBoundaries.js';
import {
  upsertFootprintsTx,
  linkClustersToFootprintsTx,
  footprintCount,
} from './stationFootprintsStore.js';
import db from './database.js';

// Transport runs are cold-start-heavy: _liveHelpers' in-memory Overpass
// cache is lost on server restart, so the first run after boot does
// nationwide Overpass pulls through a 2-per-host / 500ms-gap queue. Give
// each collector up to 10 min and the whole run up to 45 min.
const RUN_CEILING_MS = 45 * 60 * 1000;
const STATION_COLLECTOR_TIMEOUT_MS = 10 * 60 * 1000;
const LINE_COLLECTOR_TIMEOUT_MS = 10 * 60 * 1000;

let _inflightRun = null;
let _lastRunAt = null; // ISO timestamp of the most recent completed run

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

async function runMode({
  mode, source,
  stationCollector, stationCollectorId,
  lineCollectors = [], lineCollectorIds = [],
}) {
  // Each leaf collector gets its OWN withCollectorRun scope so the Follow
  // panel attributes fetches to the specific source (unified-trains,
  // overpass-rail-tracks, …) instead of all bleeding into the
  // transportDiscovery umbrella. AsyncLocalStorage nests; the leaf `ctx`
  // wins at fetch-tap emission time.
  const stationThunk = stationCollectorId
    ? () => withCollectorRun(stationCollectorId, stationCollector, { trigger: 'cron' })
    : stationCollector;
  const stations = await settleFc(
    stationThunk,
    `${mode}:stations`,
    STATION_COLLECTOR_TIMEOUT_MS,
  );
  const lineFeatures = [];
  for (let i = 0; i < lineCollectors.length; i++) {
    const leafId = lineCollectorIds[i];
    const lineThunk = leafId
      ? () => withCollectorRun(leafId, lineCollectors[i], { trigger: 'cron' })
      : lineCollectors[i];
    const items = await settleFc(
      lineThunk,
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
    {
      mode: 'train',  source: 'unified_trains',
      stationCollector: unifiedTrains,     stationCollectorId: 'unified-trains',
      lineCollectors: [overpassRailTracks], lineCollectorIds: ['overpass-rail-tracks'],
    },
    {
      mode: 'subway', source: 'unified_subways',
      stationCollector: unifiedSubways,       stationCollectorId: 'unified-subways',
      lineCollectors: [overpassSubwayTracks], lineCollectorIds: ['overpass-subway-tracks'],
    },
    {
      mode: 'bus',    source: 'unified_buses',
      stationCollector: unifiedBuses,      stationCollectorId: 'unified-buses',
      lineCollectors: [mlitN07BusRoutes], lineCollectorIds: ['mlit-n07-bus-routes'],
    },
    {
      mode: 'ship',   source: 'unified_ais_ships',
      stationCollector: unifiedAisShips,   stationCollectorId: 'unified-ais-ships',
      lineCollectors: [],
    },
    {
      mode: 'port',   source: 'unified_port_infra',
      stationCollector: unifiedPortInfra,  stationCollectorId: 'unified-port-infra',
      lineCollectors: [],
    },
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

    // Spatial snap: after all upserts, force each station's line_color to
    // the nearest track's color so stations always match their line even
    // when upstream sources disagree on operator/line text.
    for (const mode of ['train', 'subway']) {
      try {
        const snap = snapStationsToNearestLine(mode);
        console.log(
          `[transportRunner] ${mode} snap — ${JSON.stringify(snap)}`,
        );
      } catch (err) {
        console.error(`[transportRunner] ${mode} snap failed:`, err?.message);
      }
    }

    // Station clustering: build one canonical cross-mode station per
    // physical place (Shinjuku = one row, spanning JR / Tokyo Metro /
    // Toei / Keio / Odakyu). The clusterer rebuilds the table from scratch
    // each run so member UIDs stay in sync with transport_stations.
    try {
      const cluster = runStationClusterer();
      console.log(`[transportRunner] cluster — ${JSON.stringify(cluster)}`);
    } catch (err) {
      console.error('[transportRunner] cluster failed:', err?.message);
    }

    // Station footprints: nationwide OSM station-building polygons. Pull
    // fresh and upsert; then stamp each footprint with the cluster whose
    // centroid falls inside its bbox (for popup back-linking).
    try {
      const fps = await withTimeout(
        collectOsmTransportStationBoundaries(),
        LINE_COLLECTOR_TIMEOUT_MS,
        'station-footprints',
      );
      const list = Array.isArray(fps?.features) ? fps.features : [];
      if (list.length) upsertFootprintsTx(list);
      const allClusters = db.prepare(
        'SELECT cluster_uid, lat, lon FROM station_clusters',
      ).all();
      const linked = linkClustersToFootprintsTx(allClusters);
      console.log(
        `[transportRunner] footprints — fetched=${list.length}, total=${footprintCount()}, linked=${linked}`,
      );
    } catch (err) {
      console.error('[transportRunner] footprints failed:', err?.message);
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
    _lastRunAt = new Date().toISOString();
  }
}

export function isTransportRunInFlight() {
  return _inflightRun !== null;
}

export function getLastRunAt() {
  return _lastRunAt;
}
