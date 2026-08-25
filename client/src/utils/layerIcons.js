// Per-layer flat icon mapping, powered by react-icons (Material Design +
// Font Awesome where Material doesn't have a fit). Each entry is a React
// component; consumers render it inline (LayerPanel) or stringify it
// (MapView) to produce a raster for MapLibre's addImage.

import {
  MdPlace,
  MdPublic,
  MdWaves,
  MdAir,
  MdWarning,
  MdDangerous,
  MdLocalHospital,
  MdFavorite,
  MdLocalPharmacy,
  MdLocalPolice,
  MdLocalFireDepartment,
  MdHome,
  MdBusinessCenter,
  MdLocationCity,
  MdAccountBalance,
  MdGavel,
  MdFlag,
  MdSchool,
  MdRestaurant,
  MdLocalBar,
  MdLocalCafe,
  MdFastfood,
  MdLocalDrink,
  MdStore,
  MdLocalGroceryStore,
  MdLocalMall,
  MdShoppingBag,
  MdLocalGasStation,
  MdEvStation,
  MdElectricBolt,
  MdWaterDrop,
  MdWater,
  MdWhatshot,
  MdBolt,
  MdCellTower,
  MdSatelliteAlt,
  MdRouter,
  MdWifi,
  MdPhoneIphone,
  MdPhone,
  MdCameraAlt,
  MdPeople,
  MdChat,
  MdTrain,
  MdDirectionsBus,
  MdDirectionsBoat,
  MdFlight,
  MdDirectionsCar,
  MdTraffic,
  MdAnchor,
  MdLocalShipping,
  MdAttachMoney,
  MdTrendingUp,
  MdFactory,
  MdPrecisionManufacturing,
  MdComputer,
  MdMemory,
  MdStorage,
  MdRocketLaunch,
  MdShield,
  MdSecurity,
  MdRadar,
  MdPark,
  MdForest,
  MdCastle,
  MdMuseum,
  MdStadium,
  MdAttractions,
  MdHotTub,
  MdDownhillSkiing,
  MdStar,
  MdTempleBuddhist,
  MdTempleHindu,
  MdCasino,
  MdMic,
  MdBook,
  MdBathtub,
  MdEmojiFoodBeverage,
  MdAgriculture,
  MdSetMeal,
  MdLocalLibrary,
  MdDirectionsSubway,
  MdMap,
  MdPublicOff,
  MdTravelExplore,
  MdEmergency,
  MdWindPower,
  MdOilBarrel,
  MdLightbulb,
} from 'react-icons/md';

import { FaShip, FaBridge, FaTowerBroadcast, FaFish, FaPlaneUp } from 'react-icons/fa6';

export const LAYER_ICONS = {
  // Environment
  earthquakes: MdEmergency,
  weather: MdAir,
  airQuality: MdAir,
  radiation: MdDangerous,
  river: MdWater,
  hiNet: MdCellTower,
  kNet: MdCellTower,
  jmaIntensity: MdEmergency,
  jshisSeismic: MdEmergency,

  // Ocean
  jmaOceanWave: MdWaves,
  jmaOceanTemp: MdWaves,
  jmaTide: MdWaves,
  nowphasWave: MdWaves,
  lighthouseMap: MdLightbulb,

  // Transport
  transport: MdTrain,
  mlitN02Stations: MdDirectionsSubway,
  busRoutes: MdDirectionsBus,
  ferryRoutes: MdDirectionsBoat,
  highwayTraffic: MdDirectionsCar,
  unifiedHighway: MdDirectionsCar,
  maritimeAis: FaShip,
  flightAdsb: FaPlaneUp,
  jarticTraffic: MdTraffic,

  // Transport (unified)
  unifiedTrains: MdTrain,
  unifiedSubways: MdDirectionsSubway,
  unifiedBuses: MdDirectionsBus,
  unifiedAisShips: FaShip,
  unifiedPortInfra: MdAnchor,

  // Infrastructure
  plateauBuildings: MdLocationCity,
  electricalGrid: MdElectricBolt,
  gasNetwork: MdWhatshot,
  waterInfra: MdWaterDrop,
  cellTowers: MdCellTower,
  nuclearFacilities: MdDangerous,
  evCharging: MdEvStation,
  airportInfra: MdFlight,
  portInfra: MdAnchor,
  bridgeTunnelInfra: FaBridge,
  famousPlaces: MdAttractions,
  gasStations: MdLocalGasStation,
  damWaterLevel: MdWater,

  // Health
  hospitalMap: MdLocalHospital,
  aedMap: MdFavorite,
  pharmacyMap: MdLocalPharmacy,

  // Safety
  kobanMap: MdLocalPolice,
  fireStationMap: MdLocalFireDepartment,
  bosaiShelter: MdShield,
  hazardMapPortal: MdWarning,
  crime: MdLocalPolice,
  droneNofly: MdWarning,
  jcgPatrol: MdAnchor,

  // Cyber / OSINT
  cameras: MdCameraAlt,
  shodanIot: MdRouter,
  wifiNetworks: MdWifi,

  // Social
  population: MdPeople,
  twitterGeo: MdChat,
  facebookGeo: MdChat,

  // Economy / Statistics / Marketplace
  landPrice: MdAttachMoney,
  classifieds: MdStore,
  realEstate: MdHome,
  jobBoards: MdBusinessCenter,
  convenienceStores: MdLocalGroceryStore,
  tabelogRestaurants: MdRestaurant,
  resasTourism: MdAttractions,
  resasIndustry: MdFactory,
  mlitTransaction: MdHome,

  // Government / Defense
  governmentBuildings: MdAccountBalance,
  cityHalls: MdLocationCity,
  courtsPrisons: MdGavel,
  embassies: MdFlag,
  jsdfBases: MdShield,
  usfjBases: MdShield,
  radarSites: MdRadar,
  coastGuardStations: MdAnchor,

  // Industry
  autoPlants: MdPrecisionManufacturing,
  steelMills: MdFactory,
  petrochemical: MdFactory,
  refineries: MdOilBarrel,
  semiconductorFabs: MdMemory,
  shipyards: FaShip,
  petroleumStockpile: MdOilBarrel,
  windTurbines: MdWindPower,

  // Telecom
  dataCenters: MdStorage,
  internetExchanges: MdRouter,
  submarineCables: MdWaves,
  torExitNodes: MdPublicOff,
  coverage5g: MdCellTower,
  satelliteGroundStations: MdSatelliteAlt,
  amateurRadioRepeaters: FaTowerBroadcast,

  // Tourism / Culture
  nationalParks: MdPark,
  unescoHeritage: MdMuseum,
  castles: MdCastle,
  museums: MdMuseum,
  stadiums: MdStadium,
  racetracks: MdAttractions,
  shrineTemple: MdTempleBuddhist,
  onsenMap: MdHotTub,
  skiResorts: MdDownhillSkiing,
  animePilgrimage: MdStar,

  // Crime
  prefPoliceCrime: MdLocalPolice,
  npaMissingPersons: MdEmergency,
  npaTrafficAccidents: MdWarning,
  npaImportantWanted: MdEmergency,
  npaSpecialFraud: MdPhone,
  npaCyberThreatObs: MdSecurity,
  estatCrime: MdGavel,
  mojCrimeWhitepaper: MdGavel,
  redLightZones: MdLocalBar,
  pachinkoDensity: MdCasino,
  wantedPersons: MdEmergency,
  phoneScamHotspots: MdPhone,

  // Food / Agriculture
  sakeBreweries: MdLocalBar,
  wineriesCraftbeer: MdLocalBar,
  fishMarkets: FaFish,
  wagyuRanches: MdAgriculture,
  teaZones: MdEmojiFoodBeverage,
  ricePaddies: MdAgriculture,

  // Pop Culture
  vendingMachines: MdLocalDrink,
  karaokeChains: MdMic,
  mangaNetCafes: MdBook,
  sentoPublicBaths: MdBathtub,
  themedCafes: MdLocalCafe,

  // External Mapping
  marineTraffic: FaShip,
  vesselFinder: MdDirectionsBoat,
  googleMyMaps: MdMap,

  // Wave 15 — vuln intel
  myJvn: MdSecurity,
  cisaKevJp: MdDangerous,
  osvDev: MdShield,
  ghsaAdvisories: MdSecurity,
  pocInGithub: MdEmergency,
  trickestCve: MdEmergency,

  // Wave 15 — IOC / attacker activity
  shadowserverJp: MdShield,
  urlhausJp: MdWarning,
  threatfoxJp: MdWarning,
  feodoTrackerJp: MdDangerous,
  sslblJp: MdSecurity,
  spamhausDrop: MdDangerous,
  abuseipdbJp: MdWarning,
  alienvaultOtxJp: MdShield,
  phishingFeedsJp: MdWarning,
  sansIsc: MdShield,

  // Wave 15 — asset / breach
  leakixJp: MdRouter,
  netlasJp: MdRouter,
  hudsonRockJp: MdSecurity,
  virustotalJp: MdSecurity,
  chaosBugbountyJp: MdShield,

  // Wave 15 — network / BGP / DNS
  peeringdbJp: MdRouter,
  bgpToolsJp: MdPublic,
  crtshHistorical: MdSecurity,
  cloudflareRadarJp: MdRadar,
  ooniJp: MdPublic,
  iodaJp: MdPublic,

  // Wave 15 — SOCINT / news
  yahooRealtime: MdTrendingUp,
  mastodonJpInstances: MdChat,
  blueskyJetstreamJp: MdChat,
  niconicoRanking: MdMic,
  wikipediaJaRecent: MdBook,
  osmChangesetsJp: MdMap,
  yahooNewsJpRss: MdChat,
  jpNewsRss: MdChat,

  // Wave 15 — geo / disaster
  nasaFirmsJp: MdWhatshot,
};

export function getLayerIcon(layerId) {
  return LAYER_ICONS[layerId] || MdPlace;
}
