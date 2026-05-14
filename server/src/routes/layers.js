import { Router } from 'express';
import sources from '../utils/sourceRegistry.js';
import { INTEL_SOURCE_IDS } from '../utils/intelCatalog.js';
import { describeTemporal } from '../utils/layerTemporal.js';

const router = Router();

// Layer ids that should NOT appear in /api/layers nor /api/status. Three
// reasons a layer ends up here:
//   1. ingredient of a unified-* fuser (data flows via the parent)
//   2. rollup category with no real collector (just an aggregator)
//   3. zombie stub with no implementation OR sweep-only ingestor
//   4. UX merge absorbed into a parent toggle on iOS — its /api/data/:id
//      endpoint stays reachable so the hiddenFollowers fold mechanism
//      can still fetch it programmatically.
export const STRIP_LAYER_IDS = new Set([
  // Transport ingredients
  'osm-transport-trains', 'osm-transport-subways', 'osm-transport-buses',
  'osm-transport-ports', 'mlit-n02-stations', 'mlit-n07-bus-routes',
  'mlit-p11-bus-stops', 'mlit-c02-ports', 'mlit-p02-airports',
  'gtfs-jp',
  // AIS ingredients
  'maritime', 'maritime-ais', 'marine-traffic', 'vessel-finder',
  // Flight ingredients
  'aviation', 'narita-flights', 'haneda-flights', 'flight-adsb',
  // Camera sweep ingestor
  'camera-discovery',
  // Rollups without their own collector
  'transport', 'cyber', 'social', 'satellite', 'infrastructure',
  'radar', 'river', 'telecom', 'energy', 'crime', 'economy',
  'health', 'population', 'hazard', 'basemap', 'elevation', 'geocode',
  'landuse', 'poi', 'admin-boundaries', 'news-feed', 'ocean',
  'emergency', 'warnings', 'classifieds',
  // (formerly cyber zombie stubs — now real collectors:
  //  fofa-jp, quake360-jp, urlscan-jp, wayback-jp, github-leaks-jp,
  //  grayhat-buckets, strava-heatmap-bases, gdelt, chan-5ch, houjin-bangou)
  // UX merges (iOS picker hides; /api/data/:id stays callable)
  'unified-station-footprints',
  // Cross-mode clustered stations — overlaps unified-trains/unified-subways
  // visually. Web client already loads it as a hidden auto-follow; iOS now
  // hides it from the picker too to avoid the misleading "Subway" duplicate.
  'unified-stations',
  'bus-routes',
  'highway-traffic',
  'jartic-traffic',
  // Migrated to /api/intel — non-spatial sources whose collectors return
  // kind:'intel' instead of GeoJSON. Stripped from /api/layers so they don't
  // appear as toggleable map layers; their /api/data/:id endpoints stay
  // mounted and return an empty FC (the runner upserts the items into
  // intel_items as a side-effect). Source of truth is INTEL_SOURCE_IDS in
  // utils/intelCatalog.js — keep adding new intel sources there, not here.
  ...INTEL_SOURCE_IDS,
]);

// Layer-id → list of source-ids that *contribute* to a unified fan-out
// collector. Each contributing source already exists as its own first-class
// layer in the registry; this map additionally surfaces them as members of
// the parent unified layer so `/api/layers` reports the real underlying-
// provider count (the iOS LayersTab UI reads `layer.sources.count`).
//
// Mapping is derived from the imports inside each `unified*.js` collector;
// keep in sync if a unified collector grows / drops a provider.
const UNIFIED_LAYER_PROVIDERS = {
  'unified-trains':     ['mlit-n02-stations', 'osm-transport-trains'],
  'unified-subways':    ['mlit-n02-stations', 'osm-transport-subways'],
  'unified-buses':      ['mlit-p11-bus-stops', 'gtfs-jp', 'bus-routes', 'osm-transport-buses'],
  'unified-flights':    ['flight-adsb'],
  'unified-airports':   ['mlit-p02-airports'],
  'unified-highway':    ['highway-traffic', 'jartic-traffic'],
  'unified-port-infra': ['mlit-c02-ports', 'osm-transport-ports'],
  'unified-ais-ships':  ['maritime-ais', 'marine-traffic', 'vessel-finder'],
};

// Build layer metadata from the source registry
function getLayerDefinitions() {
  const layerMap = new Map();
  const byId = new Map(sources.map((s) => [s.id, s]));

  for (const src of sources) {
    if (!layerMap.has(src.layer)) {
      layerMap.set(src.layer, {
        id: src.layer,
        name: src.layer.replace(/-/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase()),
        category: src.category,
        sources: [],
      });
    }
    layerMap.get(src.layer).sources.push({
      id: src.id,
      name: src.name,
      type: src.type,
      free: src.free,
    });
  }

  // Fold underlying providers into each unified layer's source list. The
  // contributing rows still appear under their own layers — this just adds
  // them as additional members here, dedup'd by id.
  for (const [layerId, providerIds] of Object.entries(UNIFIED_LAYER_PROVIDERS)) {
    const bucket = layerMap.get(layerId);
    if (!bucket) continue;
    const seen = new Set(bucket.sources.map((s) => s.id));
    for (const pid of providerIds) {
      if (seen.has(pid)) continue;
      const src = byId.get(pid);
      if (!src) continue;
      bucket.sources.push({ id: src.id, name: src.name, type: src.type, free: src.free });
      seen.add(pid);
    }
  }

  return Array.from(layerMap.values());
}

// GET /api/layers - list available map layers
router.get('/', (_req, res) => {
  try {
    const layers = getLayerDefinitions()
      .filter((l) => !STRIP_LAYER_IDS.has(l.id))
      .map((l) => ({ ...l, ...(describeTemporal(l.id) || {}) }));
    res.json(layers);
  } catch (err) {
    console.error('[layers] Error listing layers:', err.message);
    res.status(500).json({ error: 'Failed to list layers' });
  }
});

// GET /api/layers/:layerId/geojson - get GeoJSON for a layer
router.get('/:layerId/geojson', (req, res) => {
  try {
    const { layerId } = req.params;
    const layers = getLayerDefinitions();
    const layer = layers.find((l) => l.id === layerId);

    if (!layer) {
      return res.status(404).json({ error: 'Layer not found' });
    }

    // data_cache has been removed. Callers that want live data should hit
    // /api/data/:layerId, which runs the collector on demand.
    res.json({
      type: 'FeatureCollection',
      features: [],
      _meta: {
        layer: layerId,
        message: 'Use /api/data/:layerId for live collector output.',
        sources: layer.sources.map((s) => s.id),
      },
    });
  } catch (err) {
    console.error('[layers] Error fetching layer GeoJSON:', err.message);
    res.status(500).json({ error: 'Failed to fetch layer data' });
  }
});

export default router;
