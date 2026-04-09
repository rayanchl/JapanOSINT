/**
 * Central collector registry
 * Imports all data collectors and exports them as a named map
 */

import jmaEarthquake from './jmaEarthquake.js';
import jmaWeather from './jmaWeather.js';
import soramame from './soramame.js';
import nraRadiation from './nraRadiation.js';
import odptTransport from './odptTransport.js';
import estatPopulation from './estatPopulation.js';
import mlitLandprice from './mlitLandprice.js';
import mlitRiver from './mlitRiver.js';
import publicCameras from './publicCameras.js';
import policeCrime from './policeCrime.js';
import plateauBuildings from './plateauBuildings.js';
import socialMedia from './socialMedia.js';

// Social media expansions
import twitterGeo from './twitterGeo.js';
import facebookGeo from './facebookGeo.js';
import snapchatHeatmap from './snapchatHeatmap.js';

// Marketplace / classifieds
import classifieds from './classifieds.js';
import realEstate from './realEstate.js';
import jobBoards from './jobBoards.js';

// Cyber OSINT
import googleDorking from './googleDorking.js';
import shodanIot from './shodanIot.js';
import insecamWebcams from './insecamWebcams.js';
import wifiNetworks from './wifiNetworks.js';

// Transport (nationwide expansion)
import maritimeAis from './maritimeAis.js';
import flightAdsb from './flightAdsb.js';
import fullTransport from './fullTransport.js';
import busRoutes from './busRoutes.js';
import ferryRoutes from './ferryRoutes.js';
import highwayTraffic from './highwayTraffic.js';

// Infrastructure
import electricalGrid from './electricalGrid.js';
import gasNetwork from './gasNetwork.js';
import waterInfra from './waterInfra.js';
import cellTowers from './cellTowers.js';
import nuclearFacilities from './nuclearFacilities.js';

export const collectors = {
  'jma-earthquake': jmaEarthquake,
  'jma-weather': jmaWeather,
  'soramame': soramame,
  'nra-radiation': nraRadiation,
  'odpt-transport': odptTransport,
  'estat-population': estatPopulation,
  'mlit-landprice': mlitLandprice,
  'mlit-river': mlitRiver,
  'public-cameras': publicCameras,
  'police-crime': policeCrime,
  'plateau-buildings': plateauBuildings,
  'social-media': socialMedia,

  // Social media expansions
  'twitter-geo': twitterGeo,
  'facebook-geo': facebookGeo,
  'snapchat-heatmap': snapchatHeatmap,

  // Marketplace / classifieds
  'classifieds': classifieds,
  'real-estate': realEstate,
  'job-boards': jobBoards,

  // Cyber OSINT
  'google-dorking': googleDorking,
  'shodan-iot': shodanIot,
  'insecam-webcams': insecamWebcams,
  'wifi-networks': wifiNetworks,

  // Transport (nationwide)
  'maritime-ais': maritimeAis,
  'flight-adsb': flightAdsb,
  'full-transport': fullTransport,
  'bus-routes': busRoutes,
  'ferry-routes': ferryRoutes,
  'highway-traffic': highwayTraffic,

  // Infrastructure
  'electrical-grid': electricalGrid,
  'gas-network': gasNetwork,
  'water-infra': waterInfra,
  'cell-towers': cellTowers,
  'nuclear-facilities': nuclearFacilities,
};

/**
 * Run a single collector by key name
 * @param {string} key - collector key from the collectors map
 * @returns {Promise<object>} GeoJSON FeatureCollection
 */
export async function runCollector(key) {
  const fn = collectors[key];
  if (!fn) throw new Error(`Unknown collector: ${key}`);
  return fn();
}

/**
 * Run all collectors in parallel
 * @returns {Promise<Record<string, object>>} Map of key -> GeoJSON FeatureCollection
 */
export async function runAllCollectors() {
  const entries = Object.entries(collectors);
  const results = await Promise.allSettled(
    entries.map(([key, fn]) => fn().then(result => [key, result]))
  );

  const output = {};
  for (const result of results) {
    if (result.status === 'fulfilled') {
      const [key, data] = result.value;
      output[key] = data;
    }
  }
  return output;
}

export default collectors;
