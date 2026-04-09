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

// ===========================================================================
// Social media expansions
// ===========================================================================

router.get('/twitter-geo', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'twitter-geo',
    layerType: 'twitter-geo',
    collectorKey: 'twitter-geo',
  });
});

router.get('/facebook-geo', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'facebook-geo',
    layerType: 'facebook-geo',
    collectorKey: 'facebook-geo',
  });
});

router.get('/snapchat-heatmap', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'snapchat-heatmap',
    layerType: 'snapchat-heatmap',
    collectorKey: 'snapchat-heatmap',
  });
});

// ===========================================================================
// Marketplace / classifieds
// ===========================================================================

router.get('/classifieds', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'classifieds',
    layerType: 'classifieds',
    collectorKey: 'classifieds',
  });
});

router.get('/real-estate', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'real-estate',
    layerType: 'real-estate',
    collectorKey: 'real-estate',
  });
});

router.get('/job-boards', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'job-boards',
    layerType: 'job-boards',
    collectorKey: 'job-boards',
  });
});

// ===========================================================================
// Cyber OSINT
// ===========================================================================

router.get('/google-dorking', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'google-dorking',
    layerType: 'google-dorking',
    collectorKey: 'google-dorking',
  });
});

router.get('/shodan-iot', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'shodan-iot',
    layerType: 'shodan-iot',
    collectorKey: 'shodan-iot',
  });
});

router.get('/insecam-webcams', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'insecam-webcams',
    layerType: 'insecam-webcams',
    collectorKey: 'insecam-webcams',
  });
});

router.get('/wifi-networks', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'wifi-networks',
    layerType: 'wifi-networks',
    collectorKey: 'wifi-networks',
  });
});

// ===========================================================================
// Transport (nationwide)
// ===========================================================================

router.get('/maritime-ais', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'maritime-ais',
    layerType: 'maritime-ais',
    collectorKey: 'maritime-ais',
  });
});

router.get('/flight-adsb', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'flight-adsb',
    layerType: 'flight-adsb',
    collectorKey: 'flight-adsb',
  });
});

router.get('/full-transport', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'full-transport',
    layerType: 'full-transport',
    collectorKey: 'full-transport',
  });
});

router.get('/bus-routes', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'bus-routes',
    layerType: 'bus-routes',
    collectorKey: 'bus-routes',
  });
});

router.get('/ferry-routes', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'ferry-routes',
    layerType: 'ferry-routes',
    collectorKey: 'ferry-routes',
  });
});

router.get('/highway-traffic', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'highway-traffic',
    layerType: 'highway-traffic',
    collectorKey: 'highway-traffic',
  });
});

// ===========================================================================
// Infrastructure
// ===========================================================================

router.get('/electrical-grid', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'electrical-grid',
    layerType: 'electrical-grid',
    collectorKey: 'electrical-grid',
  });
});

router.get('/gas-network', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'gas-network',
    layerType: 'gas-network',
    collectorKey: 'gas-network',
  });
});

router.get('/water-infra', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'water-infra',
    layerType: 'water-infra',
    collectorKey: 'water-infra',
  });
});

router.get('/cell-towers', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'cell-towers',
    layerType: 'cell-towers',
    collectorKey: 'cell-towers',
  });
});

router.get('/nuclear-facilities', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'nuclear-facilities',
    layerType: 'nuclear-facilities',
    collectorKey: 'nuclear-facilities',
  });
});

export default router;
