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

// ===========================================================================
// Wave 1: Public Safety + Disaster
// ===========================================================================

router.get('/hospital-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'hospital-map',
    layerType: 'hospital-map',
    collectorKey: 'hospital-map',
  });
});

router.get('/aed-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'aed-map',
    layerType: 'aed-map',
    collectorKey: 'aed-map',
  });
});

router.get('/koban-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'koban-map',
    layerType: 'koban-map',
    collectorKey: 'koban-map',
  });
});

router.get('/fire-station-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'fire-station-map',
    layerType: 'fire-station-map',
    collectorKey: 'fire-station-map',
  });
});

router.get('/bosai-shelter', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'bosai-shelter',
    layerType: 'bosai-shelter',
    collectorKey: 'bosai-shelter',
  });
});

router.get('/hazard-map-portal', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'hazard-map-portal',
    layerType: 'hazard-map-portal',
    collectorKey: 'hazard-map-portal',
  });
});

router.get('/jshis-seismic', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jshis-seismic',
    layerType: 'jshis-seismic',
    collectorKey: 'jshis-seismic',
  });
});

router.get('/hi-net', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'hi-net',
    layerType: 'hi-net',
    collectorKey: 'hi-net',
  });
});

router.get('/k-net', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'k-net',
    layerType: 'k-net',
    collectorKey: 'k-net',
  });
});

router.get('/jma-intensity', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-intensity',
    layerType: 'jma-intensity',
    collectorKey: 'jma-intensity',
  });
});

// ===========================================================================
// Wave 2: Health + Statistics + Commerce
// ===========================================================================

router.get('/pharmacy-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'pharmacy-map',
    layerType: 'pharmacy-map',
    collectorKey: 'pharmacy-map',
  });
});

router.get('/convenience-stores', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'convenience-stores',
    layerType: 'convenience-stores',
    collectorKey: 'convenience-stores',
  });
});

router.get('/gas-stations', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'gas-stations',
    layerType: 'gas-stations',
    collectorKey: 'gas-stations',
  });
});

router.get('/tabelog-restaurants', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'tabelog-restaurants',
    layerType: 'tabelog-restaurants',
    collectorKey: 'tabelog-restaurants',
  });
});

router.get('/estat-census', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'estat-census',
    layerType: 'estat-census',
    collectorKey: 'estat-census',
  });
});

router.get('/resas-population', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'resas-population',
    layerType: 'resas-population',
    collectorKey: 'resas-population',
  });
});

router.get('/resas-tourism', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'resas-tourism',
    layerType: 'resas-tourism',
    collectorKey: 'resas-tourism',
  });
});

router.get('/resas-industry', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'resas-industry',
    layerType: 'resas-industry',
    collectorKey: 'resas-industry',
  });
});

router.get('/mlit-transaction', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'mlit-transaction',
    layerType: 'mlit-transaction',
    collectorKey: 'mlit-transaction',
  });
});

router.get('/dam-water-level', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'dam-water-level',
    layerType: 'dam-water-level',
    collectorKey: 'dam-water-level',
  });
});

// ===========================================================================
// Wave 3: Maritime + Ocean + Aviation
// ===========================================================================

router.get('/jma-ocean-wave', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-ocean-wave',
    layerType: 'jma-ocean-wave',
    collectorKey: 'jma-ocean-wave',
  });
});

router.get('/jma-ocean-temp', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-ocean-temp',
    layerType: 'jma-ocean-temp',
    collectorKey: 'jma-ocean-temp',
  });
});

router.get('/jma-tide', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jma-tide',
    layerType: 'jma-tide',
    collectorKey: 'jma-tide',
  });
});

router.get('/nowphas-wave', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'nowphas-wave',
    layerType: 'nowphas-wave',
    collectorKey: 'nowphas-wave',
  });
});

router.get('/lighthouse-map', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'lighthouse-map',
    layerType: 'lighthouse-map',
    collectorKey: 'lighthouse-map',
  });
});

router.get('/jartic-traffic', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jartic-traffic',
    layerType: 'jartic-traffic',
    collectorKey: 'jartic-traffic',
  });
});

router.get('/narita-flights', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'narita-flights',
    layerType: 'narita-flights',
    collectorKey: 'narita-flights',
  });
});

router.get('/haneda-flights', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'haneda-flights',
    layerType: 'haneda-flights',
    collectorKey: 'haneda-flights',
  });
});

router.get('/drone-nofly', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'drone-nofly',
    layerType: 'drone-nofly',
    collectorKey: 'drone-nofly',
  });
});

router.get('/jcg-patrol', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'jcg-patrol',
    layerType: 'jcg-patrol',
    collectorKey: 'jcg-patrol',
  });
});

// ===========================================================================
// Wave 4: Government + Defense
// ===========================================================================
router.get('/government-buildings', async (_req, res) => {
  await respondWithData(res, { sourceId: 'government-buildings', layerType: 'government-buildings', collectorKey: 'government-buildings' });
});

router.get('/city-halls', async (_req, res) => {
  await respondWithData(res, { sourceId: 'city-halls', layerType: 'city-halls', collectorKey: 'city-halls' });
});

router.get('/courts-prisons', async (_req, res) => {
  await respondWithData(res, { sourceId: 'courts-prisons', layerType: 'courts-prisons', collectorKey: 'courts-prisons' });
});

router.get('/embassies', async (_req, res) => {
  await respondWithData(res, { sourceId: 'embassies', layerType: 'embassies', collectorKey: 'embassies' });
});

router.get('/jsdf-bases', async (_req, res) => {
  await respondWithData(res, { sourceId: 'jsdf-bases', layerType: 'jsdf-bases', collectorKey: 'jsdf-bases' });
});

router.get('/usfj-bases', async (_req, res) => {
  await respondWithData(res, { sourceId: 'usfj-bases', layerType: 'usfj-bases', collectorKey: 'usfj-bases' });
});

router.get('/radar-sites', async (_req, res) => {
  await respondWithData(res, { sourceId: 'radar-sites', layerType: 'radar-sites', collectorKey: 'radar-sites' });
});

router.get('/coast-guard-stations', async (_req, res) => {
  await respondWithData(res, { sourceId: 'coast-guard-stations', layerType: 'coast-guard-stations', collectorKey: 'coast-guard-stations' });
});

// ===========================================================================
// Wave 5: Industry + Energy Deep
// ===========================================================================
router.get('/auto-plants', async (_req, res) => {
  await respondWithData(res, { sourceId: 'auto-plants', layerType: 'auto-plants', collectorKey: 'auto-plants' });
});

router.get('/steel-mills', async (_req, res) => {
  await respondWithData(res, { sourceId: 'steel-mills', layerType: 'steel-mills', collectorKey: 'steel-mills' });
});

router.get('/petrochemical', async (_req, res) => {
  await respondWithData(res, { sourceId: 'petrochemical', layerType: 'petrochemical', collectorKey: 'petrochemical' });
});

router.get('/refineries', async (_req, res) => {
  await respondWithData(res, { sourceId: 'refineries', layerType: 'refineries', collectorKey: 'refineries' });
});

router.get('/semiconductor-fabs', async (_req, res) => {
  await respondWithData(res, { sourceId: 'semiconductor-fabs', layerType: 'semiconductor-fabs', collectorKey: 'semiconductor-fabs' });
});

router.get('/shipyards', async (_req, res) => {
  await respondWithData(res, { sourceId: 'shipyards', layerType: 'shipyards', collectorKey: 'shipyards' });
});

router.get('/petroleum-stockpile', async (_req, res) => {
  await respondWithData(res, { sourceId: 'petroleum-stockpile', layerType: 'petroleum-stockpile', collectorKey: 'petroleum-stockpile' });
});

router.get('/wind-turbines', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wind-turbines', layerType: 'wind-turbines', collectorKey: 'wind-turbines' });
});

// ===========================================================================
// Wave 6: Telecom + Internet Infrastructure
// ===========================================================================
router.get('/data-centers', async (_req, res) => {
  await respondWithData(res, { sourceId: 'data-centers', layerType: 'data-centers', collectorKey: 'data-centers' });
});

router.get('/internet-exchanges', async (_req, res) => {
  await respondWithData(res, { sourceId: 'internet-exchanges', layerType: 'internet-exchanges', collectorKey: 'internet-exchanges' });
});

router.get('/submarine-cables', async (_req, res) => {
  await respondWithData(res, { sourceId: 'submarine-cables', layerType: 'submarine-cables', collectorKey: 'submarine-cables' });
});

router.get('/tor-exit-nodes', async (_req, res) => {
  await respondWithData(res, { sourceId: 'tor-exit-nodes', layerType: 'tor-exit-nodes', collectorKey: 'tor-exit-nodes' });
});

router.get('/5g-coverage', async (_req, res) => {
  await respondWithData(res, { sourceId: '5g-coverage', layerType: '5g-coverage', collectorKey: '5g-coverage' });
});

router.get('/satellite-ground-stations', async (_req, res) => {
  await respondWithData(res, { sourceId: 'satellite-ground-stations', layerType: 'satellite-ground-stations', collectorKey: 'satellite-ground-stations' });
});

router.get('/amateur-radio-repeaters', async (_req, res) => {
  await respondWithData(res, { sourceId: 'amateur-radio-repeaters', layerType: 'amateur-radio-repeaters', collectorKey: 'amateur-radio-repeaters' });
});

// ===========================================================================
// Wave 7: Tourism + Culture
// ===========================================================================
router.get('/national-parks', async (_req, res) => {
  await respondWithData(res, { sourceId: 'national-parks', layerType: 'national-parks', collectorKey: 'national-parks' });
});

router.get('/unesco-heritage', async (_req, res) => {
  await respondWithData(res, { sourceId: 'unesco-heritage', layerType: 'unesco-heritage', collectorKey: 'unesco-heritage' });
});

router.get('/castles', async (_req, res) => {
  await respondWithData(res, { sourceId: 'castles', layerType: 'castles', collectorKey: 'castles' });
});

router.get('/museums', async (_req, res) => {
  await respondWithData(res, { sourceId: 'museums', layerType: 'museums', collectorKey: 'museums' });
});

router.get('/stadiums', async (_req, res) => {
  await respondWithData(res, { sourceId: 'stadiums', layerType: 'stadiums', collectorKey: 'stadiums' });
});

router.get('/racetracks', async (_req, res) => {
  await respondWithData(res, { sourceId: 'racetracks', layerType: 'racetracks', collectorKey: 'racetracks' });
});

router.get('/shrine-temple', async (_req, res) => {
  await respondWithData(res, { sourceId: 'shrine-temple', layerType: 'shrine-temple', collectorKey: 'shrine-temple' });
});

router.get('/onsen-map', async (_req, res) => {
  await respondWithData(res, { sourceId: 'onsen-map', layerType: 'onsen-map', collectorKey: 'onsen-map' });
});

router.get('/ski-resorts', async (_req, res) => {
  await respondWithData(res, { sourceId: 'ski-resorts', layerType: 'ski-resorts', collectorKey: 'ski-resorts' });
});

router.get('/anime-pilgrimage', async (_req, res) => {
  await respondWithData(res, { sourceId: 'anime-pilgrimage', layerType: 'anime-pilgrimage', collectorKey: 'anime-pilgrimage' });
});

// ===========================================================================
// Wave 8: Crime + Vice + Wildlife
// ===========================================================================
router.get('/yakuza-hq', async (_req, res) => {
  await respondWithData(res, { sourceId: 'yakuza-hq', layerType: 'yakuza-hq', collectorKey: 'yakuza-hq' });
});

router.get('/red-light-zones', async (_req, res) => {
  await respondWithData(res, { sourceId: 'red-light-zones', layerType: 'red-light-zones', collectorKey: 'red-light-zones' });
});

router.get('/pachinko-density', async (_req, res) => {
  await respondWithData(res, { sourceId: 'pachinko-density', layerType: 'pachinko-density', collectorKey: 'pachinko-density' });
});

router.get('/bear-encounters', async (_req, res) => {
  await respondWithData(res, { sourceId: 'bear-encounters', layerType: 'bear-encounters', collectorKey: 'bear-encounters' });
});

router.get('/bird-flu-outbreaks', async (_req, res) => {
  await respondWithData(res, { sourceId: 'bird-flu-outbreaks', layerType: 'bird-flu-outbreaks', collectorKey: 'bird-flu-outbreaks' });
});

router.get('/sakura-front', async (_req, res) => {
  await respondWithData(res, { sourceId: 'sakura-front', layerType: 'sakura-front', collectorKey: 'sakura-front' });
});

router.get('/wanted-persons', async (_req, res) => {
  await respondWithData(res, { sourceId: 'wanted-persons', layerType: 'wanted-persons', collectorKey: 'wanted-persons' });
});

router.get('/phone-scam-hotspots', async (_req, res) => {
  await respondWithData(res, { sourceId: 'phone-scam-hotspots', layerType: 'phone-scam-hotspots', collectorKey: 'phone-scam-hotspots' });
});

export default router;
