import { Router } from 'express';
import sources from '../utils/sourceRegistry.js';

const router = Router();

// Build layer metadata from the source registry
function getLayerDefinitions() {
  const layerMap = new Map();

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

  return Array.from(layerMap.values());
}

// GET /api/layers - list available map layers
router.get('/', (_req, res) => {
  try {
    const layers = getLayerDefinitions();
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
