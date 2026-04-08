import { Router } from 'express';
import { getCachedData, getSourceById } from '../utils/database.js';
import { collectors } from '../collectors/index.js';

const router = Router();

/**
 * Helper: try cache first, then fall back to a collector function, or return
 * an empty FeatureCollection with status info.
 */
async function respondWithData(res, { sourceId, layerType, collectorKey }) {
  try {
    // 1. Try cache
    const cached = getCachedData(sourceId, layerType);
    if (cached) {
      try {
        const geojson = JSON.parse(cached.geojson);
        return res.json({
          ...geojson,
          _meta: {
            ...geojson._meta,
            fromCache: true,
            fetched_at: cached.fetched_at,
            expires_at: cached.expires_at,
          },
        });
      } catch {
        // corrupted cache, continue
      }
    }

    // 2. Try collector from the registry
    const collector = collectorKey ? collectors[collectorKey] : null;
    if (collector) {
      const data = await collector();
      return res.json(data);
    }

    // 3. No collector available, return source info + empty collection
    const source = getSourceById(sourceId);
    res.json({
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source_id: sourceId,
        status: source?.status ?? 'unknown',
        message: 'No data currently available. Collector not yet implemented or source offline.',
      },
    });
  } catch (err) {
    console.error(`[data] Error fetching ${sourceId}:`, err.message);
    res.status(500).json({ error: `Failed to fetch ${layerType} data` });
  }
}

// GET /api/data/earthquake
router.get('/earthquake', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-earthquake',
    layerType: 'earthquake',
    collectorKey: 'jma-earthquake',
  });
});

// GET /api/data/weather
router.get('/weather', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-weather',
    layerType: 'weather',
    collectorKey: 'jma-weather',
  });
});

// GET /api/data/transport
router.get('/transport', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'odpt-train',
    layerType: 'transport',
    collectorKey: 'odpt-transport',
  });
});

// GET /api/data/air-quality
router.get('/air-quality', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'soramame',
    layerType: 'air-quality',
    collectorKey: 'soramame',
  });
});

// GET /api/data/radiation
router.get('/radiation', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'nra-radiation',
    layerType: 'radiation',
    collectorKey: 'nra-radiation',
  });
});

// GET /api/data/cameras
router.get('/cameras', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'traffic-cameras',
    layerType: 'cameras',
    collectorKey: 'public-cameras',
  });
});

// GET /api/data/population
router.get('/population', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'estat-population',
    layerType: 'population',
    collectorKey: 'estat-population',
  });
});

// GET /api/data/landprice
router.get('/landprice', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'mlit-landprice',
    layerType: 'landprice',
    collectorKey: 'mlit-landprice',
  });
});

// GET /api/data/river
router.get('/river', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'mlit-river',
    layerType: 'river',
    collectorKey: 'mlit-river',
  });
});

// GET /api/data/crime
router.get('/crime', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'police-incidents',
    layerType: 'crime',
    collectorKey: 'police-crime',
  });
});

// GET /api/data/buildings
router.get('/buildings', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'plateau-buildings',
    layerType: 'buildings',
    collectorKey: 'plateau-buildings',
  });
});

// GET /api/data/social
router.get('/social', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'twitter-geo',
    layerType: 'social',
    collectorKey: 'social-media',
  });
});

export default router;
