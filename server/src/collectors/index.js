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

// Wave 1: Public Safety + Disaster
import hospitalMap from './hospitalMap.js';
import aedMap from './aedMap.js';
import kobanMap from './kobanMap.js';
import fireStationMap from './fireStationMap.js';
import bosaiShelter from './bosaiShelter.js';
import hazardMapPortal from './hazardMapPortal.js';
import jshisSeismic from './jshisSeismic.js';
import hiNet from './hiNet.js';
import kNet from './kNet.js';
import jmaIntensity from './jmaIntensity.js';

// Wave 2: Health + Statistics + Commerce
import pharmacyMap from './pharmacyMap.js';
import convenienceStores from './convenienceStores.js';
import gasStations from './gasStations.js';
import tabelogRestaurants from './tabelogRestaurants.js';
import estatCensus from './estatCensus.js';
import resasPopulation from './resasPopulation.js';
import resasTourism from './resasTourism.js';
import resasIndustry from './resasIndustry.js';
import mlitTransaction from './mlitTransaction.js';
import damWaterLevel from './damWaterLevel.js';

// Wave 3: Maritime + Ocean + Aviation
import jmaOceanWave from './jmaOceanWave.js';
import jmaOceanTemp from './jmaOceanTemp.js';
import jmaTide from './jmaTide.js';
import nowphasWave from './nowphasWave.js';
import lighthouseMap from './lighthouseMap.js';
import jarticTraffic from './jarticTraffic.js';
import naritaFlights from './naritaFlights.js';
import hanedaFlights from './hanedaFlights.js';
import droneNofly from './droneNofly.js';
import jcgPatrol from './jcgPatrol.js';

// Wave 4: Government + Defense
import governmentBuildings from './governmentBuildings.js';
import cityHalls from './cityHalls.js';
import courtsPrisons from './courtsPrisons.js';
import embassies from './embassies.js';
import jsdfBases from './jsdfBases.js';
import usfjBases from './usfjBases.js';
import radarSites from './radarSites.js';
import coastGuardStations from './coastGuardStations.js';

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

  // Wave 1: Public Safety + Disaster
  'hospital-map': hospitalMap,
  'aed-map': aedMap,
  'koban-map': kobanMap,
  'fire-station-map': fireStationMap,
  'bosai-shelter': bosaiShelter,
  'hazard-map-portal': hazardMapPortal,
  'jshis-seismic': jshisSeismic,
  'hi-net': hiNet,
  'k-net': kNet,
  'jma-intensity': jmaIntensity,

  // Wave 2: Health + Statistics + Commerce
  'pharmacy-map': pharmacyMap,
  'convenience-stores': convenienceStores,
  'gas-stations': gasStations,
  'tabelog-restaurants': tabelogRestaurants,
  'estat-census': estatCensus,
  'resas-population': resasPopulation,
  'resas-tourism': resasTourism,
  'resas-industry': resasIndustry,
  'mlit-transaction': mlitTransaction,
  'dam-water-level': damWaterLevel,

  // Wave 3: Maritime + Ocean + Aviation
  'jma-ocean-wave': jmaOceanWave,
  'jma-ocean-temp': jmaOceanTemp,
  'jma-tide': jmaTide,
  'nowphas-wave': nowphasWave,
  'lighthouse-map': lighthouseMap,
  'jartic-traffic': jarticTraffic,
  'narita-flights': naritaFlights,
  'haneda-flights': hanedaFlights,
  'drone-nofly': droneNofly,
  'jcg-patrol': jcgPatrol,

  // Wave 4: Government + Defense
  'government-buildings': governmentBuildings,
  'city-halls': cityHalls,
  'courts-prisons': courtsPrisons,
  'embassies': embassies,
  'jsdf-bases': jsdfBases,
  'usfj-bases': usfjBases,
  'radar-sites': radarSites,
  'coast-guard-stations': coastGuardStations,
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
