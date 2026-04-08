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
