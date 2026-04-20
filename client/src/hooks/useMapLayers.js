import { useState, useCallback, useRef } from 'react';

const LAYER_DEFINITIONS = {
  earthquakes: {
    name: 'Earthquakes',
    icon: '\u{1F534}',
    color: '#ff4444',
    endpoint: '/api/data/earthquake',
    category: 'Environment',
  },
  weather: {
    name: 'Weather',
    icon: '\u{1F324}',
    color: '#4fc3f7',
    endpoint: '/api/data/weather',
    category: 'Environment',
  },
  transport: {
    name: 'Transit Stations',
    icon: '\u{1F683}',
    color: '#66bb6a',
    endpoint: '/api/data/transport',
    category: 'Transport',
    hidden: true,
  },
  airQuality: {
    name: 'Air Quality',
    icon: '\u{1F4A8}',
    color: '#ffb74d',
    endpoint: '/api/data/air-quality',
    category: 'Environment',
  },
  radiation: {
    name: 'Radiation',
    icon: '\u{2622}',
    color: '#ffd600',
    endpoint: '/api/data/radiation',
    category: 'Safety',
  },
  cameras: {
    name: 'Cameras',
    icon: '\u{1F4F7}',
    color: '#ce93d8',
    endpoint: '/api/data/cameras',
    category: 'Cyber',
  },
  population: {
    name: 'Population',
    icon: '\u{1F465}',
    color: '#4dd0e1',
    endpoint: '/api/data/population',
    category: 'Social',
  },
  landPrice: {
    name: 'Land Prices',
    icon: '\u{1F4B0}',
    color: '#aed581',
    endpoint: '/api/data/landprice',
    category: 'Economy',
  },
  river: {
    name: 'River Levels',
    icon: '\u{1F30A}',
    color: '#42a5f5',
    endpoint: '/api/data/river',
    category: 'Environment',
  },
  crime: {
    name: 'Crime/Police',
    icon: '\u{1F694}',
    color: '#ef5350',
    endpoint: '/api/data/crime',
    category: 'Safety',
  },
  buildings: {
    name: 'Buildings',
    icon: '\u{1F3E2}',
    color: '#78909c',
    endpoint: '/api/data/buildings',
    category: 'Infrastructure',
  },
  social: {
    name: 'Social Media',
    icon: '\u{1F4AC}',
    color: '#f06292',
    endpoint: '/api/data/social',
    category: 'Social',
  },

  // ── Social Media Geo (expanded) ─────────────────────────────────
  twitterGeo: {
    name: 'Twitter/X Geo',
    icon: '\u{1F426}',
    color: '#1da1f2',
    endpoint: '/api/data/twitter-geo',
    category: 'Social',
  },
  facebookGeo: {
    name: 'Facebook Check-ins',
    icon: '\u{1F4D8}',
    color: '#4267b2',
    endpoint: '/api/data/facebook-geo',
    category: 'Social',
  },
  // ── Marketplace / Classifieds ───────────────────────────────────
  classifieds: {
    name: 'Classifieds (Jmty)',
    icon: '\u{1F4DD}',
    color: '#ff9800',
    endpoint: '/api/data/classifieds',
    category: 'Marketplace',
  },
  realEstate: {
    name: 'Real Estate (Suumo)',
    icon: '\u{1F3D8}',
    color: '#8bc34a',
    endpoint: '/api/data/real-estate',
    category: 'Marketplace',
  },
  jobBoards: {
    name: 'Job Boards',
    icon: '\u{1F4BC}',
    color: '#a1887f',
    endpoint: '/api/data/job-boards',
    category: 'Marketplace',
  },

  // ── Cyber OSINT ─────────────────────────────────────────────────
  shodanIot: {
    name: 'Shodan IoT',
    icon: '\u{1F4E1}',
    color: '#d62d20',
    endpoint: '/api/data/shodan-iot',
    category: 'Cyber',
  },
  wifiNetworks: {
    name: 'WiFi Networks',
    icon: '\u{1F4F6}',
    color: '#00bcd4',
    endpoint: '/api/data/wifi-networks',
    category: 'Cyber',
  },

  // ── Transport (nationwide) ──────────────────────────────────────
  fullTransport: {
    name: 'Japan Rail Network',
    icon: '\u{1F684}',
    color: '#43a047',
    endpoint: '/api/data/full-transport',
    category: 'Transport',
    hidden: true,
  },
  mlitN02Stations: {
    name: 'MLIT N02 Rail Stations',
    icon: '\u{1F68A}',
    color: '#2e7d32',
    endpoint: '/api/data/mlit-n02-stations',
    category: 'Transport',
    hidden: true,
  },
  busRoutes: {
    name: 'Bus Terminals',
    icon: '\u{1F68C}',
    color: '#fb8c00',
    endpoint: '/api/data/bus-routes',
    category: 'Transport',
    hidden: true,
  },
  ferryRoutes: {
    name: 'Ferries',
    icon: '\u{26F4}',
    color: '#039be5',
    endpoint: '/api/data/ferry-routes',
    category: 'Transport',
  },
  highwayTraffic: {
    name: 'Expressway IC/JCT',
    icon: '\u{1F6E3}',
    color: '#9e9e9e',
    endpoint: '/api/data/highway-traffic',
    category: 'Infrastructure',
  },
  maritimeAis: {
    name: 'AIS Ship Tracking',
    icon: '\u{1F6A2}',
    color: '#0277bd',
    endpoint: '/api/data/maritime-ais',
    category: 'Transport',
    hidden: true,
  },
  flightAdsb: {
    name: 'Planes',
    icon: '\u{2708}',
    color: '#7c4dff',
    endpoint: '/api/data/flight-adsb',
    category: 'Transport',
  },

  // ── Infrastructure ──────────────────────────────────────────────
  electricalGrid: {
    name: 'Electrical Grid',
    icon: '\u{26A1}',
    color: '#ffeb3b',
    endpoint: '/api/data/electrical-grid',
    category: 'Infrastructure',
  },
  gasNetwork: {
    name: 'Gas Network',
    icon: '\u{1F525}',
    color: '#ff5722',
    endpoint: '/api/data/gas-network',
    category: 'Infrastructure',
  },
  waterInfra: {
    name: 'Water Infrastructure',
    icon: '\u{1F4A7}',
    color: '#29b6f6',
    endpoint: '/api/data/water-infra',
    category: 'Infrastructure',
  },
  cellTowers: {
    name: 'Cell Towers',
    icon: '\u{1F4F1}',
    color: '#9c27b0',
    endpoint: '/api/data/cell-towers',
    category: 'Infrastructure',
  },
  nuclearFacilities: {
    name: 'Nuclear Facilities',
    icon: '\u{2622}',
    color: '#76ff03',
    endpoint: '/api/data/nuclear-facilities',
    category: 'Infrastructure',
  },
  evCharging: {
    name: 'EV Charging',
    icon: '\u{1F50C}',
    color: '#4caf50',
    endpoint: '/api/data/ev-charging',
    category: 'Infrastructure',
  },
  airportInfra: {
    name: 'Airports',
    icon: '\u{2708}',
    color: '#546e7a',
    endpoint: '/api/data/airport-infra',
    category: 'Transport',
  },
  portInfra: {
    name: 'Port Infrastructure',
    icon: '\u{2693}',
    color: '#1565c0',
    endpoint: '/api/data/port-infra',
    category: 'Transport',
    hidden: true,
  },
  bridgeTunnelInfra: {
    name: 'Bridges & Tunnels',
    icon: '\u{1F309}',
    color: '#795548',
    endpoint: '/api/data/bridge-tunnel-infra',
    category: 'Infrastructure',
  },
  famousPlaces: {
    name: 'Famous Places',
    icon: '\u{1F3EF}',
    color: '#d81b60',
    endpoint: '/api/data/famous-places',
    category: 'Infrastructure',
  },

  // ── Wave 1: Public Safety + Disaster ────────────────────────────
  hospitalMap: {
    name: 'Hospitals',
    icon: '\u{1F3E5}',
    color: '#e53935',
    endpoint: '/api/data/hospital-map',
    category: 'Health',
  },
  aedMap: {
    name: 'AED Locations',
    icon: '\u{2764}',
    color: '#ff5252',
    endpoint: '/api/data/aed-map',
    category: 'Health',
  },
  kobanMap: {
    name: 'Police Boxes (Koban)',
    icon: '\u{1F46E}',
    color: '#1565c0',
    endpoint: '/api/data/koban-map',
    category: 'Safety',
  },
  fireStationMap: {
    name: 'Fire Stations',
    icon: '\u{1F692}',
    color: '#d84315',
    endpoint: '/api/data/fire-station-map',
    category: 'Safety',
  },
  bosaiShelter: {
    name: 'Disaster Shelters',
    icon: '\u{1F6E1}',
    color: '#00897b',
    endpoint: '/api/data/bosai-shelter',
    category: 'Safety',
  },
  hazardMapPortal: {
    name: 'Hazard Zones',
    icon: '\u{26A0}',
    color: '#ff6f00',
    endpoint: '/api/data/hazard-map-portal',
    category: 'Safety',
  },
  jshisSeismic: {
    name: 'Seismic Hazard (J-SHIS)',
    icon: '\u{1F30B}',
    color: '#bf360c',
    endpoint: '/api/data/jshis-seismic',
    category: 'Safety',
  },
  hiNet: {
    name: 'Hi-net Stations',
    icon: '\u{1F4DF}',
    color: '#7e57c2',
    endpoint: '/api/data/hi-net',
    category: 'Environment',
  },
  kNet: {
    name: 'K-NET Stations',
    icon: '\u{1F4DF}',
    color: '#9575cd',
    endpoint: '/api/data/k-net',
    category: 'Environment',
  },
  jmaIntensity: {
    name: 'JMA Intensity',
    icon: '\u{1F4CA}',
    color: '#c62828',
    endpoint: '/api/data/jma-intensity',
    category: 'Environment',
  },

  // ── Wave 2: Health + Statistics + Commerce ──────────────────────
  pharmacyMap: {
    name: 'Pharmacies',
    icon: '\u{1F48A}',
    color: '#26a69a',
    endpoint: '/api/data/pharmacy-map',
    category: 'Health',
  },
  convenienceStores: {
    name: 'Konbini',
    icon: '\u{1F3EA}',
    color: '#43a047',
    endpoint: '/api/data/convenience-stores',
    category: 'Marketplace',
  },
  gasStations: {
    name: 'Gas Stations',
    icon: '\u{26FD}',
    color: '#e64a19',
    endpoint: '/api/data/gas-stations',
    category: 'Infrastructure',
  },
  tabelogRestaurants: {
    name: 'Restaurants',
    icon: '\u{1F371}',
    color: '#fb8c00',
    endpoint: '/api/data/tabelog-restaurants',
    category: 'Marketplace',
  },
  resasTourism: {
    name: 'Tourism Sites',
    icon: '\u{1F5FE}',
    color: '#00acc1',
    endpoint: '/api/data/resas-tourism',
    category: 'Statistics',
  },
  resasIndustry: {
    name: 'Industry Hubs',
    icon: '\u{1F3ED}',
    color: '#6d4c41',
    endpoint: '/api/data/resas-industry',
    category: 'Statistics',
  },
  mlitTransaction: {
    name: 'Land Transactions',
    icon: '\u{1F3E0}',
    color: '#558b2f',
    endpoint: '/api/data/mlit-transaction',
    category: 'Economy',
  },
  damWaterLevel: {
    name: 'Dam Water Levels',
    icon: '\u{1F3DE}',
    color: '#0288d1',
    endpoint: '/api/data/dam-water-level',
    category: 'Infrastructure',
  },

  // ── Wave 3: Maritime + Ocean + Aviation ─────────────────────────
  jmaOceanWave: {
    name: 'Ocean Waves',
    icon: '\u{1F30A}',
    color: '#0288d1',
    endpoint: '/api/data/jma-ocean-wave',
    category: 'Ocean',
  },
  jmaOceanTemp: {
    name: 'Sea Surface Temp',
    icon: '\u{1F321}',
    color: '#ff6f00',
    endpoint: '/api/data/jma-ocean-temp',
    category: 'Ocean',
  },
  jmaTide: {
    name: 'Tide Levels',
    icon: '\u{1F30A}',
    color: '#0277bd',
    endpoint: '/api/data/jma-tide',
    category: 'Ocean',
  },
  nowphasWave: {
    name: 'NOWPHAS Buoys',
    icon: '\u{1F535}',
    color: '#1565c0',
    endpoint: '/api/data/nowphas-wave',
    category: 'Ocean',
  },
  lighthouseMap: {
    name: 'Lighthouses',
    icon: '\u{1F3EE}',
    color: '#fdd835',
    endpoint: '/api/data/lighthouse-map',
    category: 'Ocean',
  },
  jarticTraffic: {
    name: 'Traffic Congestion',
    icon: '\u{1F6A6}',
    color: '#e53935',
    endpoint: '/api/data/jartic-traffic',
    category: 'Transport',
  },
  droneNofly: {
    name: 'Drone No-Fly',
    icon: '\u{1F6F8}',
    color: '#c62828',
    endpoint: '/api/data/drone-nofly',
    category: 'Safety',
  },
  jcgPatrol: {
    name: 'JCG Patrol Bases',
    icon: '\u{1F6E5}',
    color: '#00838f',
    endpoint: '/api/data/jcg-patrol',
    category: 'Safety',
  },

  // Wave 4: Government + Defense
  governmentBuildings: {
    name: 'Government Buildings',
    icon: '\u{1F3DB}',
    color: '#6a1b9a',
    endpoint: '/api/data/government-buildings',
    category: 'Government',
  },
  cityHalls: {
    name: 'City Halls',
    icon: '\u{1F3E2}',
    color: '#7b1fa2',
    endpoint: '/api/data/city-halls',
    category: 'Government',
  },
  courtsPrisons: {
    name: 'Courts & Prisons',
    icon: '\u{2696}',
    color: '#4527a0',
    endpoint: '/api/data/courts-prisons',
    category: 'Government',
  },
  embassies: {
    name: 'Embassies',
    icon: '\u{1F3F3}',
    color: '#1565c0',
    endpoint: '/api/data/embassies',
    category: 'Government',
  },
  jsdfBases: {
    name: 'JSDF Bases',
    icon: '\u{1F396}',
    color: '#33691e',
    endpoint: '/api/data/jsdf-bases',
    category: 'Defense',
  },
  usfjBases: {
    name: 'USFJ Bases',
    icon: '\u{1F1FA}',
    color: '#1a237e',
    endpoint: '/api/data/usfj-bases',
    category: 'Defense',
  },
  radarSites: {
    name: 'Radar Sites',
    icon: '\u{1F4E1}',
    color: '#bf360c',
    endpoint: '/api/data/radar-sites',
    category: 'Defense',
  },
  coastGuardStations: {
    name: 'Coast Guard Stations',
    icon: '\u{1F6F0}',
    color: '#0277bd',
    endpoint: '/api/data/coast-guard-stations',
    category: 'Defense',
  },

  // Wave 5: Industry + Energy Deep
  autoPlants: {
    name: 'Auto Plants',
    icon: '\u{1F697}',
    color: '#d84315',
    endpoint: '/api/data/auto-plants',
    category: 'Industry',
  },
  steelMills: {
    name: 'Steel Mills',
    icon: '\u{1F3ED}',
    color: '#5d4037',
    endpoint: '/api/data/steel-mills',
    category: 'Industry',
  },
  petrochemical: {
    name: 'Petrochemical',
    icon: '\u{2697}',
    color: '#6a1b9a',
    endpoint: '/api/data/petrochemical',
    category: 'Industry',
  },
  refineries: {
    name: 'Oil Refineries',
    icon: '\u{26FD}',
    color: '#e65100',
    endpoint: '/api/data/refineries',
    category: 'Industry',
  },
  semiconductorFabs: {
    name: 'Semiconductor Fabs',
    icon: '\u{1F4BB}',
    color: '#1565c0',
    endpoint: '/api/data/semiconductor-fabs',
    category: 'Industry',
  },
  shipyards: {
    name: 'Shipyards',
    icon: '\u{1F6A2}',
    color: '#00695c',
    endpoint: '/api/data/shipyards',
    category: 'Industry',
  },
  petroleumStockpile: {
    name: 'Petroleum Stockpile',
    icon: '\u{1F6E2}',
    color: '#bf360c',
    endpoint: '/api/data/petroleum-stockpile',
    category: 'Industry',
  },
  windTurbines: {
    name: 'Wind Turbines',
    icon: '\u{1F300}',
    color: '#43a047',
    endpoint: '/api/data/wind-turbines',
    category: 'Industry',
  },

  // Wave 6: Telecom + Internet Infrastructure
  dataCenters: {
    name: 'Data Centers',
    icon: '\u{1F5A5}',
    color: '#00838f',
    endpoint: '/api/data/data-centers',
    category: 'Telecom',
  },
  internetExchanges: {
    name: 'Internet Exchanges (IXP)',
    icon: '\u{1F310}',
    color: '#00acc1',
    endpoint: '/api/data/internet-exchanges',
    category: 'Telecom',
  },
  submarineCables: {
    name: 'Submarine Cable Landings',
    icon: '\u{1F30A}',
    color: '#01579b',
    endpoint: '/api/data/submarine-cables',
    category: 'Telecom',
  },
  torExitNodes: {
    name: 'Tor Exit Nodes',
    icon: '\u{1F9C5}',
    color: '#7b1fa2',
    endpoint: '/api/data/tor-exit-nodes',
    category: 'Telecom',
  },
  coverage5g: {
    name: '5G Coverage',
    icon: '\u{1F4F6}',
    color: '#e91e63',
    endpoint: '/api/data/5g-coverage',
    category: 'Telecom',
  },
  satelliteGroundStations: {
    name: 'Satellite Ground Stations',
    icon: '\u{1F6F0}',
    color: '#ffa726',
    endpoint: '/api/data/satellite-ground-stations',
    category: 'Satellite',
  },
  satelliteImagery: {
    name: 'Satellite Imagery',
    icon: '\u{1F30D}',
    color: '#64b5f6',
    endpoint: '/api/data/satellite-imagery',
    category: 'Satellite',
  },
  satelliteTracking: {
    name: 'Live Satellite Positions',
    icon: '\u{1F6F0}',
    color: '#ba68c8',
    endpoint: '/api/data/satellite-tracking',
    category: 'Satellite',
  },
  amateurRadioRepeaters: {
    name: 'Amateur Radio Repeaters',
    icon: '\u{1F4FB}',
    color: '#8d6e63',
    endpoint: '/api/data/amateur-radio-repeaters',
    category: 'Telecom',
  },

  // ── Wave 7: Tourism + Culture ───────────────────────────────────
  nationalParks: {
    name: 'National Parks',
    icon: '\u{1F3DE}',
    color: '#2e7d32',
    endpoint: '/api/data/national-parks',
    category: 'Tourism',
  },
  unescoHeritage: {
    name: 'UNESCO Heritage',
    icon: '\u{1F3DB}',
    color: '#ffa000',
    endpoint: '/api/data/unesco-heritage',
    category: 'Tourism',
  },
  castles: {
    name: 'Castles',
    icon: '\u{1F3F0}',
    color: '#8d6e63',
    endpoint: '/api/data/castles',
    category: 'Tourism',
  },
  museums: {
    name: 'Museums',
    icon: '\u{1F3DB}',
    color: '#5e35b1',
    endpoint: '/api/data/museums',
    category: 'Tourism',
  },
  stadiums: {
    name: 'Stadiums',
    icon: '\u{1F3DF}',
    color: '#00897b',
    endpoint: '/api/data/stadiums',
    category: 'Tourism',
  },
  racetracks: {
    name: 'Racetracks',
    icon: '\u{1F3C7}',
    color: '#c2185b',
    endpoint: '/api/data/racetracks',
    category: 'Tourism',
  },
  shrineTemple: {
    name: 'Shrines & Temples',
    icon: '\u{26E9}',
    color: '#b71c1c',
    endpoint: '/api/data/shrine-temple',
    category: 'Culture',
  },
  onsenMap: {
    name: 'Onsen (Hot Springs)',
    icon: '\u{2668}',
    color: '#ef6c00',
    endpoint: '/api/data/onsen-map',
    category: 'Culture',
  },
  skiResorts: {
    name: 'Ski Resorts',
    icon: '\u{1F3BF}',
    color: '#0288d1',
    endpoint: '/api/data/ski-resorts',
    category: 'Tourism',
  },
  animePilgrimage: {
    name: 'Anime Pilgrimage',
    icon: '\u{1F38C}',
    color: '#e91e63',
    endpoint: '/api/data/anime-pilgrimage',
    category: 'Culture',
  },

  // ── Wave 8: Crime + Vice + Wildlife ─────────────────────────────
  yakuzaHq: {
    name: 'Yakuza HQs',
    icon: '\u{1F5E1}',
    color: '#6a1b9a',
    endpoint: '/api/data/yakuza-hq',
    category: 'Crime',
  },
  redLightZones: {
    name: 'Red Light Districts',
    icon: '\u{1F4A1}',
    color: '#d81b60',
    endpoint: '/api/data/red-light-zones',
    category: 'Crime',
  },
  pachinkoDensity: {
    name: 'Pachinko Density',
    icon: '\u{1F3B0}',
    color: '#fdd835',
    endpoint: '/api/data/pachinko-density',
    category: 'Crime',
  },
  wantedPersons: {
    name: 'Wanted Persons',
    icon: '\u{1F6A8}',
    color: '#d32f2f',
    endpoint: '/api/data/wanted-persons',
    category: 'Crime',
  },
  phoneScamHotspots: {
    name: 'Phone Scam Hotspots',
    icon: '\u{1F4DE}',
    color: '#ff7043',
    endpoint: '/api/data/phone-scam-hotspots',
    category: 'Crime',
  },

  // ── Wave 9: Food + Agriculture ──────────────────────────────────
  sakeBreweries: {
    name: 'Sake Breweries',
    icon: '\u{1F376}',
    color: '#7986cb',
    endpoint: '/api/data/sake-breweries',
    category: 'Food',
  },
  wineriesCraftbeer: {
    name: 'Wineries & Craft Beer',
    icon: '\u{1F377}',
    color: '#ad1457',
    endpoint: '/api/data/wineries-craftbeer',
    category: 'Food',
  },
  fishMarkets: {
    name: 'Fish Markets',
    icon: '\u{1F41F}',
    color: '#0288d1',
    endpoint: '/api/data/fish-markets',
    category: 'Food',
  },
  wagyuRanches: {
    name: 'Wagyu Ranches',
    icon: '\u{1F404}',
    color: '#6d4c41',
    endpoint: '/api/data/wagyu-ranches',
    category: 'Food',
  },
  teaZones: {
    name: 'Tea Zones',
    icon: '\u{1F375}',
    color: '#43a047',
    endpoint: '/api/data/tea-zones',
    category: 'Agriculture',
  },
  ricePaddies: {
    name: 'Rice Paddies',
    icon: '\u{1F33E}',
    color: '#9ccc65',
    endpoint: '/api/data/rice-paddies',
    category: 'Agriculture',
  },

  // ── Wave 10: Niche + Pop Culture ────────────────────────────────
  vendingMachines: {
    name: 'Vending Machines',
    icon: '\u{1F964}',
    color: '#e91e63',
    endpoint: '/api/data/vending-machines',
    category: 'Culture',
  },
  karaokeChains: {
    name: 'Karaoke Chains',
    icon: '\u{1F3A4}',
    color: '#d81b60',
    endpoint: '/api/data/karaoke-chains',
    category: 'Culture',
  },
  mangaNetCafes: {
    name: 'Manga / Net Cafes',
    icon: '\u{1F4D6}',
    color: '#8e24aa',
    endpoint: '/api/data/manga-net-cafes',
    category: 'Culture',
  },
  sentoPublicBaths: {
    name: 'Sento Public Baths',
    icon: '\u{1F6C0}',
    color: '#0097a7',
    endpoint: '/api/data/sento-public-baths',
    category: 'Culture',
  },
  themedCafes: {
    name: 'Themed Cafes',
    icon: '\u{1F431}',
    color: '#f06292',
    endpoint: '/api/data/themed-cafes',
    category: 'Culture',
  },

  // ── Wave 11: External Mapping Platforms ─────────────────────────
  marineTraffic: {
    name: 'MarineTraffic AIS',
    icon: '\u{1F6A2}',
    color: '#01579b',
    endpoint: '/api/data/marine-traffic',
    category: 'Transport',
    hidden: true,
  },
  vesselFinder: {
    name: 'VesselFinder AIS',
    icon: '\u{26F4}',
    color: '#0288d1',
    endpoint: '/api/data/vessel-finder',
    category: 'Transport',
    hidden: true,
  },
  googleMyMaps: {
    name: 'Google My Maps',
    icon: '\u{1F5FA}',
    color: '#ea4335',
    endpoint: '/api/data/google-my-maps',
    category: 'Mapping',
  },
  // -- Unified transport collectors (fused + deduped) --
  unifiedTrains: {
    name: 'Trains',
    icon: '\u{1F686}',
    color: '#2e7d32',
    endpoint: '/api/data/unified-trains',
    category: 'Transport',
  },
  unifiedSubways: {
    name: 'Subways & Trams',
    icon: '\u{1F687}',
    color: '#ff7043',
    endpoint: '/api/data/unified-subways',
    category: 'Transport',
  },
  unifiedBuses: {
    name: 'Buses',
    icon: '\u{1F68C}',
    color: '#fb8c00',
    endpoint: '/api/data/unified-buses',
    category: 'Transport',
  },
  unifiedAisShips: {
    name: 'Ships',
    icon: '\u{1F6A2}',
    color: '#0277bd',
    endpoint: '/api/data/unified-ais-ships',
    category: 'Transport',
  },
  unifiedPortInfra: {
    name: 'Ports',
    icon: '\u{2693}',
    color: '#1565c0',
    endpoint: '/api/data/unified-port-infra',
    category: 'Transport',
  },
  // -- Live-transit vehicles (client-side simulator, no server endpoint) --
  // Data comes from useLiveVehicles() fetching /api/transit/routes; the
  // hook emits a GeoJSON FeatureCollection at 1 Hz which MapView pushes
  // into a dedicated source via setData.
  liveTransitTrains: {
    name: 'Live trains (sim)',
    icon: '\u{1F686}',
    color: '#2e7d32',
    category: 'Transport',
  },
  liveTransitSubways: {
    name: 'Live subways (sim)',
    icon: '\u{1F687}',
    color: '#ff7043',
    category: 'Transport',
  },
  liveTransitBuses: {
    name: 'Live buses (sim)',
    icon: '\u{1F68C}',
    color: '#fb8c00',
    category: 'Transport',
  },

  // ── Wave 11: broadened pulse ────────────────────────────────────
  japanPostOffices: {
    name: 'Post Offices',
    icon: '\u{1F4EE}',
    color: '#ef6c00',
    endpoint: '/api/data/japan-post-offices',
    category: 'Infrastructure',
  },
  odptTrainDelay: {
    name: 'Train Delays (ODPT)',
    icon: '\u{26A0}',
    color: '#ffb300',
    endpoint: '/api/data/odpt-train-delay',
    category: 'Transport',
  },
  certstreamJp: {
    name: '.jp CT Monitor',
    icon: '\u{1F512}',
    color: '#26a69a',
    endpoint: '/api/data/certstream-jp',
    category: 'Cyber',
  },
};

export const LAYER_CATEGORIES = [
  'Environment',
  'Ocean',
  'Transport',
  'Safety',
  'Health',
  'Economy',
  'Statistics',
  'Government',
  'Defense',
  'Industry',
  'Telecom',
  'Satellite',
  'Tourism',
  'Culture',
  'Food',
  'Agriculture',
  'Crime',
  'Wildlife',
  'Social',
  'Marketplace',
  'Cyber',
  'Infrastructure',
  'Mapping',
];

export { LAYER_DEFINITIONS };

export default function useMapLayers() {
  const [layers, setLayers] = useState(() => {
    const initial = {};
    for (const key of Object.keys(LAYER_DEFINITIONS)) {
      initial[key] = { visible: false, opacity: 1, loading: false };
    }
    return initial;
  });

  const [layerData, setLayerData] = useState({});
  const [isLoading, setIsLoading] = useState(false);
  const cacheRef = useRef({});

  const fetchLayerData = useCallback(async (layerId) => {
    const def = LAYER_DEFINITIONS[layerId];
    if (!def) return;
    if (!def.endpoint) return; // client-side-only layers (e.g. live-transit)

    if (cacheRef.current[layerId]) {
      setLayerData((prev) => ({ ...prev, [layerId]: cacheRef.current[layerId] }));
      return;
    }

    setLayers((prev) => ({
      ...prev,
      [layerId]: { ...prev[layerId], loading: true },
    }));

    try {
      const res = await fetch(def.endpoint);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();

      const geojson = data.type === 'FeatureCollection' ? data : {
        type: 'FeatureCollection',
        features: Array.isArray(data) ? data : (data.features || []),
      };

      cacheRef.current[layerId] = geojson;
      setLayerData((prev) => ({ ...prev, [layerId]: geojson }));
    } catch (err) {
      console.warn(`[useMapLayers] Failed to fetch ${layerId}:`, err.message);
      setLayerData((prev) => ({
        ...prev,
        [layerId]: { type: 'FeatureCollection', features: [] },
      }));
    } finally {
      setLayers((prev) => ({
        ...prev,
        [layerId]: { ...prev[layerId], loading: false },
      }));
    }
  }, []);

  const toggleLayer = useCallback((layerId) => {
    setLayers((prev) => {
      const current = prev[layerId];
      if (!current) return prev;
      const newVisible = !current.visible;

      if (newVisible && !cacheRef.current[layerId]) {
        fetchLayerData(layerId);
      }

      return {
        ...prev,
        [layerId]: { ...current, visible: newVisible },
      };
    });
  }, [fetchLayerData]);

  const setLayerOpacity = useCallback((layerId, opacity) => {
    setLayers((prev) => ({
      ...prev,
      [layerId]: { ...prev[layerId], opacity },
    }));
  }, []);

  const setAllLayers = useCallback((visible) => {
    setLayers((prev) => {
      const updated = {};
      for (const key of Object.keys(prev)) {
        updated[key] = { ...prev[key], visible };
        if (visible && !cacheRef.current[key]) {
          fetchLayerData(key);
        }
      }
      return updated;
    });
  }, [fetchLayerData]);

  const refreshLayer = useCallback((layerId) => {
    delete cacheRef.current[layerId];
    fetchLayerData(layerId);
  }, [fetchLayerData]);

  const activeCount = Object.values(layers).filter((l) => l.visible).length;

  return {
    layers,
    layerDefinitions: LAYER_DEFINITIONS,
    toggleLayer,
    setLayerOpacity,
    setAllLayers,
    refreshLayer,
    layerData,
    isLoading,
    activeCount,
  };
}
