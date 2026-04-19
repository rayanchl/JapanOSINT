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

// Marketplace / classifieds
import classifieds from './classifieds.js';
import realEstate from './realEstate.js';
import jobBoards from './jobBoards.js';

// Cyber OSINT
import shodanIot from './shodanIot.js';
import insecamWebcams from './insecamWebcams.js';
import wifiNetworks from './wifiNetworks.js';

// Transport (nationwide expansion)
import maritimeAis from './maritimeAis.js';
import flightAdsb from './flightAdsb.js';
import fullTransport from './fullTransport.js';
import mlitN02Stations from './mlitN02Stations.js';
import mlitN05RailHistory from './mlitN05RailHistory.js';
import mlitN07BusRoutes from './mlitN07BusRoutes.js';
import mlitP02Airports from './mlitP02Airports.js';
import mlitP11BusStops from './mlitP11BusStops.js';
import mlitC02Ports from './mlitC02Ports.js';
import gtfsJp from './gtfsJp.js';
import busRoutes from './busRoutes.js';
import ferryRoutes from './ferryRoutes.js';
import highwayTraffic from './highwayTraffic.js';

// Transport (OSM always-on layer per category)
import osmTransportTrains from './osmTransportTrains.js';
import osmTransportSubways from './osmTransportSubways.js';
import osmTransportBuses from './osmTransportBuses.js';
import osmTransportPorts from './osmTransportPorts.js';
import overpassRailTracks from './overpassRailTracks.js';
import overpassSubwayTracks from './overpassSubwayTracks.js';

// Transport (unified, deduplicated)
import unifiedTrains from './unifiedTrains.js';
import unifiedSubways from './unifiedSubways.js';
import unifiedBuses from './unifiedBuses.js';
import unifiedAisShips from './unifiedAisShips.js';
import unifiedPortInfra from './unifiedPortInfra.js';

// Infrastructure
import electricalGrid from './electricalGrid.js';
import gasNetwork from './gasNetwork.js';
import waterInfra from './waterInfra.js';
import cellTowers from './cellTowers.js';
import nuclearFacilities from './nuclearFacilities.js';
import evCharging from './evCharging.js';
import airportInfra from './airportInfra.js';
import portInfra from './portInfra.js';
import bridgeTunnelInfra from './bridgeTunnelInfra.js';
import famousPlaces from './famousPlaces.js';

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

// Wave 5: Industry + Energy Deep
import autoPlants from './autoPlants.js';
import steelMills from './steelMills.js';
import petrochemical from './petrochemical.js';
import refineries from './refineries.js';
import semiconductorFabs from './semiconductorFabs.js';
import shipyards from './shipyards.js';
import petroleumStockpile from './petroleumStockpile.js';
import windTurbines from './windTurbines.js';

// Wave 6: Telecom + Internet Infrastructure
import dataCenters from './dataCenters.js';
import internetExchanges from './internetExchanges.js';
import submarineCables from './submarineCables.js';
import torExitNodes from './torExitNodes.js';
import coverage5g from './coverage5g.js';
import satelliteGroundStations from './satelliteGroundStations.js';
import satelliteImagery from './satelliteImagery.js';
import satelliteTracking from './satelliteTracking.js';
import amateurRadioRepeaters from './amateurRadioRepeaters.js';

// Wave 7: Tourism + Culture
import nationalParks from './nationalParks.js';
import unescoHeritage from './unescoHeritage.js';
import castles from './castles.js';
import museums from './museums.js';
import stadiums from './stadiums.js';
import racetracks from './racetracks.js';
import shrineTemple from './shrineTemple.js';
import onsenMap from './onsenMap.js';
import skiResorts from './skiResorts.js';
import animePilgrimage from './animePilgrimage.js';

// Wave 8: Crime + Vice + Wildlife
import yakuzaHq from './yakuzaHq.js';
import redLightZones from './redLightZones.js';
import pachinkoDensity from './pachinkoDensity.js';
import wantedPersons from './wantedPersons.js';
import phoneScamHotspots from './phoneScamHotspots.js';

// Wave 9: Food + Agriculture
import sakeBreweries from './sakeBreweries.js';
import wineriesCraftbeer from './wineriesCraftbeer.js';
import fishMarkets from './fishMarkets.js';
import wagyuRanches from './wagyuRanches.js';
import teaZones from './teaZones.js';
import ricePaddies from './ricePaddies.js';

// Wave 11: External Mapping Platforms (MarineTraffic, VesselFinder, Sentinel Hub, My Maps)
import marineTraffic from './marineTraffic.js';
import vesselFinder from './vesselFinder.js';
import sentinelHub from './sentinelHub.js';
import googleMyMaps from './googleMyMaps.js';

// Wave 12: Untapped OSM infrastructure tags
import parkingFacilities from './parkingFacilities.js';
import waterTowers from './waterTowers.js';
import transmissionTowers from './transmissionTowers.js';
import utilityPoles from './utilityPoles.js';
import adminBoundaries from './adminBoundaries.js';
// Wave 12: Unified camera discovery
import cameraDiscovery from './cameraDiscovery.js';

// Wave 13: Net-new live OSINT endpoints (2026 sweep)
import p2pquakeJma from './p2pquakeJma.js';
import wolfxEew from './wolfxEew.js';
import wolfxEqlist from './wolfxEqlist.js';
import jmaForecastArea from './jmaForecastArea.js';
import jmaTyphoonJson from './jmaTyphoonJson.js';
import openmeteoJma from './openmeteoJma.js';
import nervFeed from './nervFeed.js';
import msilUmishiru from './msilUmishiru.js';
import jcgNavarea from './jcgNavarea.js';
import edinetFilings from './edinetFilings.js';
import bojStats from './bojStats.js';
import egovLaws from './egovLaws.js';
import dataGoJpCkan from './dataGoJpCkan.js';
import geospatialJpCkan from './geospatialJpCkan.js';
import nhkNewsRss from './nhkNewsRss.js';
import nhkWorldRss from './nhkWorldRss.js';
import kyodoRss from './kyodoRss.js';
import openskyJapan from './openskyJapan.js';
import jpcertAlertsRss from './jpcertAlertsRss.js';
import nictAtlas from './nictAtlas.js';
import gsiGeocode from './gsiGeocode.js';
import japanApiPrefectures from './japanApiPrefectures.js';

// Wave 10: Niche + Pop Culture
import vendingMachines from './vendingMachines.js';
import karaokeChains from './karaokeChains.js';
import mangaNetCafes from './mangaNetCafes.js';
import sentoPublicBaths from './sentoPublicBaths.js';
import themedCafes from './themedCafes.js';

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

  // Marketplace / classifieds
  'classifieds': classifieds,
  'real-estate': realEstate,
  'job-boards': jobBoards,

  // Cyber OSINT
  'shodan-iot': shodanIot,
  'insecam-webcams': insecamWebcams,
  'wifi-networks': wifiNetworks,

  // Transport (nationwide)
  'maritime-ais': maritimeAis,
  'flight-adsb': flightAdsb,
  'full-transport': fullTransport,
  'mlit-n02-stations': mlitN02Stations,
  'mlit-n05-rail-history': mlitN05RailHistory,
  'mlit-n07-bus-routes': mlitN07BusRoutes,
  'mlit-p02-airports': mlitP02Airports,
  'mlit-p11-bus-stops': mlitP11BusStops,
  'mlit-c02-ports': mlitC02Ports,
  'gtfs-jp': gtfsJp,
  'bus-routes': busRoutes,
  'ferry-routes': ferryRoutes,
  'highway-traffic': highwayTraffic,

  // Transport (OSM always-on layer per category)
  'osm-transport-trains': osmTransportTrains,
  'osm-transport-subways': osmTransportSubways,
  'osm-transport-buses': osmTransportBuses,
  'osm-transport-ports': osmTransportPorts,
  'overpass-rail-tracks': overpassRailTracks,
  'overpass-subway-tracks': overpassSubwayTracks,

  // Transport (unified, deduplicated)
  'unified-trains': unifiedTrains,
  'unified-subways': unifiedSubways,
  'unified-buses': unifiedBuses,
  'unified-ais-ships': unifiedAisShips,
  'unified-port-infra': unifiedPortInfra,

  // Infrastructure
  'electrical-grid': electricalGrid,
  'gas-network': gasNetwork,
  'water-infra': waterInfra,
  'cell-towers': cellTowers,
  'nuclear-facilities': nuclearFacilities,
  'ev-charging': evCharging,
  'airport-infra': airportInfra,
  'port-infra': portInfra,
  'bridge-tunnel-infra': bridgeTunnelInfra,
  'famous-places': famousPlaces,

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

  // Wave 5: Industry + Energy Deep
  'auto-plants': autoPlants,
  'steel-mills': steelMills,
  'petrochemical': petrochemical,
  'refineries': refineries,
  'semiconductor-fabs': semiconductorFabs,
  'shipyards': shipyards,
  'petroleum-stockpile': petroleumStockpile,
  'wind-turbines': windTurbines,

  // Wave 6: Telecom + Internet Infrastructure
  'data-centers': dataCenters,
  'internet-exchanges': internetExchanges,
  'submarine-cables': submarineCables,
  'tor-exit-nodes': torExitNodes,
  '5g-coverage': coverage5g,
  'satellite-ground-stations': satelliteGroundStations,
  'satellite-imagery':  satelliteImagery,
  'satellite-tracking': satelliteTracking,
  'amateur-radio-repeaters': amateurRadioRepeaters,

  // Wave 7: Tourism + Culture
  'national-parks': nationalParks,
  'unesco-heritage': unescoHeritage,
  'castles': castles,
  'museums': museums,
  'stadiums': stadiums,
  'racetracks': racetracks,
  'shrine-temple': shrineTemple,
  'onsen-map': onsenMap,
  'ski-resorts': skiResorts,
  'anime-pilgrimage': animePilgrimage,

  // Wave 8: Crime + Vice + Wildlife
  'yakuza-hq': yakuzaHq,
  'red-light-zones': redLightZones,
  'pachinko-density': pachinkoDensity,
  'wanted-persons': wantedPersons,
  'phone-scam-hotspots': phoneScamHotspots,

  // Wave 9: Food + Agriculture
  'sake-breweries': sakeBreweries,
  'wineries-craftbeer': wineriesCraftbeer,
  'fish-markets': fishMarkets,
  'wagyu-ranches': wagyuRanches,
  'tea-zones': teaZones,
  'rice-paddies': ricePaddies,

  // Wave 10: Niche + Pop Culture
  'vending-machines': vendingMachines,
  'karaoke-chains': karaokeChains,
  'manga-net-cafes': mangaNetCafes,
  'sento-public-baths': sentoPublicBaths,
  'themed-cafes': themedCafes,

  // Wave 11: External Mapping Platforms
  'marine-traffic': marineTraffic,
  'vessel-finder': vesselFinder,
  'sentinel-hub': sentinelHub,
  'google-my-maps': googleMyMaps,

  // Wave 12: Untapped OSM infrastructure tags
  'parking-facilities': parkingFacilities,
  'water-towers': waterTowers,
  'transmission-towers': transmissionTowers,
  'utility-poles': utilityPoles,
  'admin-boundaries': adminBoundaries,
  // Wave 12: Unified camera discovery
  'camera-discovery': cameraDiscovery,

  // Wave 13: Net-new live OSINT endpoints (2026 sweep)
  'p2pquake-jma': p2pquakeJma,
  'wolfx-eew': wolfxEew,
  'wolfx-eqlist': wolfxEqlist,
  'jma-forecast-area': jmaForecastArea,
  'jma-typhoon-json': jmaTyphoonJson,
  'openmeteo-jma': openmeteoJma,
  'nerv-feed': nervFeed,
  'msil-umishiru': msilUmishiru,
  'jcg-navarea': jcgNavarea,
  'edinet-filings': edinetFilings,
  'boj-stats': bojStats,
  'egov-laws': egovLaws,
  'data-go-jp-ckan': dataGoJpCkan,
  'geospatial-jp-ckan': geospatialJpCkan,
  'nhk-news-rss': nhkNewsRss,
  'nhk-world-rss': nhkWorldRss,
  'kyodo-rss': kyodoRss,
  'opensky-japan': openskyJapan,
  'jpcert-alerts-rss': jpcertAlertsRss,
  'nict-atlas': nictAtlas,
  'gsi-geocode': gsiGeocode,
  'japan-api-prefectures': japanApiPrefectures,
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
