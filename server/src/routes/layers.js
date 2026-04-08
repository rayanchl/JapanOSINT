import { Router } from 'express';
import sources from '../utils/sourceRegistry.js';
import { getCachedData } from '../utils/database.js';

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

    // Try to find cached data from any source that feeds this layer
    for (const src of layer.sources) {
      const cached = getCachedData(src.id, layerId);
      if (cached) {
        try {
          const geojson = JSON.parse(cached.geojson);
          return res.json({
            ...geojson,
            _meta: {
              layer: layerId,
              source_id: src.id,
              fetched_at: cached.fetched_at,
              expires_at: cached.expires_at,
              fromCache: true,
            },
          });
        } catch {
          // corrupted cache entry, continue to next source
        }
      }
    }

    // No cached data available
    res.json({
      type: 'FeatureCollection',
      features: [],
      _meta: {
        layer: layerId,
        message: 'No cached data available. Data will be populated on next fetch cycle.',
        sources: layer.sources.map((s) => s.id),
      },
    });
  } catch (err) {
    console.error('[layers] Error fetching layer GeoJSON:', err.message);
    res.status(500).json({ error: 'Failed to fetch layer data' });
  }
});

export default router;
