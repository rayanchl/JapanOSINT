# Collector Catalog

Every collector in `server/src/collectors/` and the upstream source(s) it
fetches from. Fusion collectors (prefixed `unified*`) merge other in-repo
collectors rather than raw feeds.

| Collector (file) | Upstream source(s) | Notes |
|---|---|---|
| **Environment & Seismic** | | |
| jmaEarthquake | JMA JSON (jma.go.jp/bosai/quake) | Real-time earthquake events |
| jmaWeather | JMA Forecast API (jma.go.jp/bosai/forecast) | Regional weather forecasts |
| soramame | Ministry of Environment SORAMAME (soramame.env.go.jp), NIES backup | Hourly air quality monitoring stations |
| nraRadiation | NRA radioactivity feeds (radioactivity.nra.go.jp), JCAC, OSM | Real-time radiation dose monitoring |
| jmaOceanWave | JMA wave network (data.jma.go.jp/kaiyou/data/wave) | Significant wave heights from buoys |
| jmaOceanTemp | JMA SST feed (data.jma.go.jp/kaiyou/data/kaikyo) | Sea surface temperature observations |
| jmaTide | JMA tide stations (data.jma.go.jp/kaiyou/tide) | Tide observations |
| nowphasWave | PARI NOWPHAS network (nowphas.mlit.go.jp) | Deep-sea wave observations (GPS buoys + ultrasonic) |
| jshisSeismic | NIED J-SHIS seismic hazard API | 30-year seismic probability mesh |
| hiNet | NIED Hi-net (high-sensitivity seismograph stations) | Seismic observation stations (OSM pull) |
| kNet | NIED K-NET (strong motion seismograph stations) | Strong motion observation stations (OSM pull) |
| jmaIntensity | JMA earthquake intensity reports (jma.go.jp/bosai/quake) | Recent earthquake intensity events |
| hazardMapPortal | MLIT + OSM overlays | Tsunami, volcano, landslide, flood, liquefaction zones |
| p2pquakeJma | P2P Quake community mirror (api.p2pquake.net) | Community JMA earthquake mirror |
| wolfxEew | Wolfx JMA EEW (api.wolfx.jp/jma_eew.json) | Live earthquake early warning |
| wolfxEqlist | Wolfx earthquake list (api.wolfx.jp/jma_eqlist.json) | Last 50 earthquakes |
| jmaForecastArea | JMA regional forecast JSON (jma.go.jp/bosai/forecast) | Regional weather forecast areas |
| jmaTyphoonJson | JMA active typhoon tracks (jma.go.jp/bosai/typhoon) | Active typhoon tracks and forecasts |
| openmeteoJma | Open-Meteo JMA mirror (api.open-meteo.com/v1/jma) | Free JMA weather via Open-Meteo |
| nervFeed | NERV alert aggregator (unii-api.nerv.app/v1/lib/alerts.json) | Unified disaster prevention alerts |
| **Transport — Rail** | | |
| odptTransport | ODPT (odpt.org) + OSM fallback | Nationwide station coverage (JR, private, subway) |
| mlitN02Stations | MLIT N02 KSJ (railway) | Authoritative nationwide rail stations |
| mlitN05RailHistory | MLIT N05 KSJ (historical railway data) | Long-term rail network history |
| fullTransport | OSM Overpass (`railway=station`) nationwide | All rail/subway/monorail/tram stations |
| osmTransportTrains | OSM Overpass (`railway=station`, `railway=halt`) | Always-on OSM mainline rail stations |
| osmTransportSubways | OSM Overpass subway/metro/tram stops | Always-on OSM subway/metro/tram stations |
| overpassRailTracks | OSM Overpass (`railway=rail`, `light_rail`) | Rail track LineStrings |
| overpassSubwayTracks | OSM Overpass (subway/tram track ways) | Subway/metro track LineStrings |
| unifiedTrains | *Fusion:* mlitN02Stations + odptTransport + fullTransport + osmTransportTrains | Deduplicated rail stations |
| unifiedSubways | *Fusion:* mlitN02Stations + odptTransport + fullTransport + osmTransportSubways | Deduplicated subway/metro/tram stops |
| **Transport — Bus & Traffic** | | |
| mlitN07BusRoutes | MLIT N07 KSJ (bus route data) | Nationwide bus route geometry |
| mlitP11BusStops | MLIT P11 KSJ (bus stops, ~200k) | Authoritative bus stop point dataset |
| gtfsJp | GTFS-JP (gtfs-data.jp) + OSM fallback | Aggregated 400+ Japanese bus operator feeds |
| busRoutes | OSM Overpass (`amenity=bus_station`) | Bus station terminals |
| osmTransportBuses | OSM Overpass (bus stops, platforms, terminals) | Always-on OSM bus infrastructure |
| unifiedBuses | *Fusion:* mlitP11BusStops + gtfsJp + busRoutes + osmTransportBuses | Deduplicated bus stops & terminals |
| highwayTraffic | JARTIC + OSM (`highway=motorway_junction`) | Expressway IC/JCT/SA/PA + JARTIC congestion |
| jarticTraffic | JARTIC Open Traffic API (jartic.or.jp) | Road congestion observations |
| **Transport — Aviation** | | |
| flightAdsb | OpenSky Network (OAuth2 optional) + AeroDataBox (RapidAPI) | Live ADS-B positions + scheduled arrivals/departures |
| mlitP02Airports | MLIT P02 KSJ (airports) | All civilian and joint-use airfields |
| airportInfra | OSM (`aeroway=aerodrome`, navigation aids) | Airport runways, navigation aids, control towers |
| openskyJapan | OpenSky Network API (opensky-network.org) | ADS-B aircraft tracking (Japan bbox) |
| droneNofly | MLIT/JCAB (mlit.go.jp) + OSM overlays | Drone no-fly zones (airports, DID, key facilities) |
| **Transport — Maritime** | | |
| maritimeAis | MarineTraffic Exportvessels + VesselFinder + OSM + seed | Aggregated AIS vessel positions (Japan) |
| marineTraffic | MarineTraffic Exportvessels API (services.marinetraffic.com) | Dedicated MarineTraffic AIS feed (Japan bbox) |
| vesselFinder | VesselFinder Master API (api.vesselfinder.com) | Dedicated VesselFinder live AIS (Japan bbox) |
| mlitC02Ports | MLIT C02 KSJ (ports: international, important, local, fishing) | MLIT-designated port dataset |
| ferryRoutes | OSM (`amenity=ferry_terminal`) | Inter-island + inland sea + international terminals |
| osmTransportPorts | OSM Overpass (harbours, ferry terminals, marinas) | Always-on OSM maritime infrastructure |
| portInfra | OSM (`harbour=yes`, industrial ports) + curated strategic ports | Port and maritime infrastructure |
| lighthouseMap | OSM (`man_made=lighthouse`) + JCG register | Lighthouse locations (historic/strategic) |
| jcgPatrol | JCG patrol base ports (kaiho.mlit.go.jp) | Japan Coast Guard vessel bases and RCGH HQs |
| jcgNavarea | JCG NAVAREA XI (kaiho.mlit.go.jp/JAPANNAVAREA) | JCG maritime safety warning zones |
| msilUmishiru | MSIL Umishiru API (portal.msil.go.jp/apis) + port seed | JCG Maritime Domain Awareness (requires key) |
| damWaterLevel | MLIT Water System API (river.go.jp) + seed | Major dam water levels and capacity |
| unifiedAisShips | *Fusion:* maritimeAis + marineTraffic + vesselFinder | Deduplicated vessel positions (MMSI primary key) |
| unifiedPortInfra | *Fusion:* portInfra + osmTransportPorts + mlitC02Ports | Deduplicated port/harbour infrastructure |
| **Infrastructure — Energy & Utilities** | | |
| electricalGrid | OSM (`power=plant`, `power=substation`) | Power plants and substations |
| gasNetwork | OSM (`man_made=storage_tank` gas, industrial gas) | Gas terminals, plants, storage facilities |
| waterInfra | OSM (`man_made=water_works`, `landuse=reservoir`) | Water treatment, sewage, aqueducts |
| cellTowers | OSM + OpenCellID + MIC 5G registry | Mobile base station locations |
| coverage5g | MIC 5G base station registry (mic.go.jp) + OSM | 5G/LTE coverage zones |
| nuclearFacilities | OSM (`power=plant` nuclear) + JAXA/RIKEN registry | Nuclear power plants, research facilities, waste sites |
| evCharging | OpenChargeMap API + OSM (`amenity=charging_station`) | EV charging infrastructure (CHAdeMO, CCS, Tesla) |
| windTurbines | OSM (`power=generator` wind) + JWPA registry | Wind farms (onshore + offshore) |
| petroleumStockpile | METI strategic petroleum reserve sites + OSM | National oil stockpile bases |
| mlitRiver | MLIT/DIAS river-gauge endpoints (river.go.jp) | Major river monitoring stations |
| **Infrastructure — Telecom & Internet** | | |
| dataCenters | OSM + datacenter operator registries | Commercial data centers (Equinix, NTT, KDDI, etc.) |
| internetExchanges | PeeringDB API (peeringdb.com) + seed | Major IXPs (JPNAP, JPIX, etc.) |
| submarineCables | OSM (`telecom=connection_point`) + TeleGeography seed | Submarine cable landing stations |
| torExitNodes | Tor onionoo API (onionoo.torproject.org) | Live Tor exit nodes in Japan |
| satelliteGroundStations | OSM (`man_made=satellite_dish`, observatories) | JAXA, KDDI, commercial ground stations |
| amateurRadioRepeaters | RepeaterBook API (repeaterbook.com) | JARL VHF/UHF/D-STAR/DMR/HF repeaters |
| **Infrastructure — Physical** | | |
| bridgeTunnelInfra | OSM (`man_made=bridge`, tunnel ways) | Landmark bridges and tunnels |
| parkingFacilities | OSM (`amenity=parking`) tiled | Parking lots and parking entrances |
| waterTowers | OSM (`man_made=water_tower`, water_works) | Water towers and treatment infrastructure |
| transmissionTowers | OSM (`power=tower`, `power=pole`) tiled | Transmission towers and power poles |
| utilityPoles | OSM (`man_made=utility_pole`) tiled | Utility poles (telecom/low-voltage) |
| adminBoundaries | OSM (`boundary=administrative` admin_level 4/7) | Prefecture and municipality boundaries (centroids) |
| **Geospatial & Mapping** | | |
| plateauBuildings | PLATEAU (plateau.geospatial.jp) + OSM | MLIT 3D city model building footprints |
| sentinelHub | Copernicus Sentinel-2 imagery (multiple STAC sources) | Sentinel-2 L2A scene footprints (free-first chain) |
| googleMyMaps | Google My Maps KML export + OSM fallback | User-curated maps with tourism/historic/leisure POIs |
| famousPlaces | OSM Overpass (tourism, historic, amenity, leisure, natural) tiled | Unified OSM famous places/POIs |
| gsiGeocode | GSI address search (msearch.gsi.go.jp/address-search) | Address geocoding service |
| japanApiPrefectures | japan-api (japanapi.curtisbarnard.com) | Prefecture REST API |
| **Public Safety & Disaster** | | |
| hospitalMap | OSM (`amenity=hospital`) tiled + seed | Hospitals and clinics nationwide |
| aedMap | OSM (`emergency=defibrillator`) tiled + seed | AED (defibrillator) locations |
| kobanMap | OSM (`amenity=police`) tiled + seed | Police boxes and stations |
| fireStationMap | OSM (`amenity=fire_station`) tiled + seed | Fire stations |
| bosaiShelter | OSM (`amenity=shelter`) + curated disaster shelters | Evacuation shelters and assembly areas |
| **Health & Commerce** | | |
| pharmacyMap | OSM (`amenity=pharmacy`) tiled + seed | Pharmacies (major chains) |
| convenienceStores | OSM (`shop=convenience`) tiled + seed | Konbini (7-Eleven, FamilyMart, Lawson, MiniStop) |
| gasStations | OSM (`amenity=fuel`) tiled + seed | Fuel stations (ENEOS, Idemitsu, Cosmo, JA-SS) |
| tabelogRestaurants | HotPepper Gourmet API (recruit.co.jp) + seed | Restaurant listings with ratings |
| resasTourism | RESAS API (resas-portal.go.jp) + seed | Tourist destination visitor statistics |
| resasIndustry | RESAS API (resas-portal.go.jp) | Industry composition by city |
| mlitTransaction | MLIT Land Transaction API (land.mlit.go.jp) | Real estate transaction price data (quarterly) |
| mlitLandprice | MLIT land price data | Published land prices |
| estatPopulation | e-Stat API | Population statistics |
| **Government & Defense** | | |
| governmentBuildings | OSM (`office=government`) + seed | Cabinet, Diet, ministries, courts |
| cityHalls | OSM (`amenity=townhall`) + seed | City and ward halls (~80 major offices) |
| courtsPrisons | OSM + MOJ facility list | High courts, district courts, correctional facilities |
| embassies | OSM (`diplomatic=embassy`) + Tokyo embassy seed | Foreign diplomatic missions |
| jsdfBases | OSM (`landuse=military`) + JSDF seed | GSDF, MSDF, ASDF installations (70+ bases) |
| usfjBases | OSM + DoD published list | USAF, USMC, Army bases in Japan |
| radarSites | ASDF/USAF radar sites (JADGE, X-band) + MLIT registry | BMD/AEW radar installations |
| coastGuardStations | OSM + JCG registry | Japan Coast Guard regional and local offices |
| policeCrime | NPA crime statistics | Crime data by prefecture |
| **Industry & Manufacturing** | | |
| autoPlants | OSM + Toyota/Nissan/Honda/Mazda/Subaru IR data | Automotive assembly plants |
| steelMills | OSM + integrated steelworks registry | Nippon Steel, JFE, Kobelco mills |
| petrochemical | METI petrochemical complex registry | Major petrochemical plants (石油化学コンビナート) |
| refineries | METI oil refinery registry (~22 active) | ENEOS, Idemitsu, Cosmo, Showa Shell refineries |
| semiconductorFabs | OSM + METI semicon strategy | Semiconductor wafer fabs and packaging plants |
| shipyards | Shipbuilders Assoc Japan + OSM | Imabari, JMU, Japan Marine United yards |
| **Tourism & Culture** | | |
| nationalParks | OSM (`boundary=national_park`) + MOE registry | 34 national parks + quasi-national parks |
| unescoHeritage | UNESCO World Heritage Centre XML (whc.unesco.org) | 25 Japanese World Heritage sites |
| castles | OSM + JCCH registry | 100 Famous Castles (historic + reconstructed) |
| museums | OSM (`tourism=museum`) + curated majors | National, art, science, history museums |
| stadiums | OSM + NPB/J-League registry | Baseball, football, sumo, multi-purpose stadiums |
| racetracks | OSM (`leisure=track` sports) | JRA (horse), NAR, Keirin, Kyotei, auto |
| shrineTemple | OSM (`amenity=place_of_worship` Shinto/Buddhist) | Shinto shrines and Buddhist temples |
| onsenMap | OSM (`natural=hot_spring`, `amenity=public_bath` onsen) | Hot spring districts and onsen facilities |
| skiResorts | OSM (`landuse=winter_sports`) | Ski areas across Hokkaido, Tohoku, Nagano, etc. |
| animePilgrimage | OSM Wikidata-linked POIs + Animedia seed | Anime/manga pilgrimage sites (seichi junrei) |
| **Niche & Pop Culture** | | |
| vendingMachines | OSM (`amenity=vending_machine`) sampled | Vending machine density zones |
| karaokeChains | OSM (`amenity=karaoke_box`) + seed | Karaoke box chains (Big Echo, Karaoke-kan, Shidax) |
| mangaNetCafes | OSM (`shop=internet_cafe`, `amenity=internet_cafe`) | Manga/net cafes (24-hr facilities) |
| sentoPublicBaths | OSM (`amenity=public_bath` non-onsen) + Tokyo Sento Assoc | Traditional public bathhouses |
| themedCafes | OSM (`amenity=cafe` with themes) + seed | Themed cafes (cat, maid, owl, hedgehog, etc.) |
| **Food & Agriculture** | | |
| sakeBreweries | OSM (`craft=brewery` sake) + JSBA registry | ~1,200 registered sake breweries |
| wineriesCraftbeer | OSM (`craft=winery`, non-sake brewery) + NTA license registry | Wine and craft beer producers |
| fishMarkets | OSM (`amenity=marketplace` fish/seafood) + MAFF | Wholesale and famous morning fish markets |
| wagyuRanches | OSM (`landuse=farmyard` cattle/wagyu) + butcher shops | Certified wagyu production regions |
| teaZones | OSM (`landuse=farmland` crop=tea) + MAFF registry | Tea-growing regions (Shizuoka, Uji, Yame, etc.) |
| ricePaddies | OSM (`landuse=farmland` crop=rice) + MAFF mesh | Major rice-producing regions |
| **Social & Media** | | |
| socialMedia | Wikipedia GeoSearch API (en.wikipedia.org) | Geotagged Wikipedia articles (major cities) |
| twitterGeo | Twitter API v2 + Mastodon public timelines | Geotagged posts from Japan (auth required for Twitter) |
| facebookGeo | Facebook Graph API (graph.facebook.com) | Geotagged check-ins and posts (auth required) |
| snapchatHeatmap | Snap Map activity density (simulated) | Snapchat heatmap activity zones |
| publicCameras | OSM (`man_made=surveillance`) + JARTIC + JMA volcano cams | Public surveillance and traffic cameras |
| **Cyber & Security** | | |
| shodanIot | Shodan API (api.shodan.io) | Internet-connected IoT devices in Japan |
| insecamWebcams | Insecam.org + Shodan RTSP streams | Publicly accessible IP cameras |
| wifiNetworks | Wigle.net API (wigle.net) + OSM | Wireless network discovery |
| cameraDiscovery | *Fusion:* OSM + JMA + MLIT + Shodan + YouTube + Insecam + Windy | Unified camera discovery from all known channels |
| **Crime & Vice** | | |
| yakuzaHq | NPA designated organized crime (npa.go.jp) | Designated bouryokudan HQs |
| redLightZones | OSM (`amenity=stripclub`, `amenity=brothel`, `amenity=nightclub`) | Entertainment districts and venues |
| pachinkoDensity | OSM (`leisure=adult_gaming_centre`) | Pachinko parlor density |
| wantedPersons | NPA wanted persons list (npa.go.jp/sousa/shimeitehai) | 指名手配犯 (public wanted persons) |
| phoneScamHotspots | NPA 特殊詐欺 statistics by prefecture (npa.go.jp) | Phone scam incident density |
| **Classifieds & Real Estate** | | |
| classifieds | OSM (`office=employment_agency`) + Jmty/Mercari/Yahoo seed | Japanese classifieds |
| realEstate | Suumo (suumo.jp) + Homes.co.jp + AtHome (scraped) | Rental/sales listings with prices |
| jobBoards | OSM (`office=employment_agency`) + Town Work/Baitoru/Indeed/Coconala | Job and gig listings |
| **Data Catalogs & Reference** | | |
| dataGoJpCkan | data.go.jp CKAN API (data.go.jp/data/api) | Japan's national open data catalog |
| geospatialJpCkan | MLIT CKAN (geospatial.jp/ckan/api) | Geospatial/MLIT dataset catalog |
| egovLaws | e-Gov law search (laws.e-gov.go.jp/api) | Japan's national law database |
| bojStats | Bank of Japan statistics (stat-search.boj.or.jp) | Monetary aggregates and economic indicators |
| edinetFilings | FSA EDINET API (api.edinet-fsa.go.jp) | Corporate securities disclosures |
| **News & Information** | | |
| nhkNewsRss | NHK news RSS (nhk.or.jp/rss/news) | NHK top news feed |
| nhkWorldRss | NHK World English (nhk.or.jp/nhkworld/en/news/feeds) | NHK World English Atom feed |
| kyodoRss | Kyodo News English (english.kyodonews.net/rss) | Japanese news agency in English |
| jpcertAlertsRss | JPCERT/CC RDF/RSS (jpcert.or.jp/rss) | Cybersecurity vulnerability advisories |
| nictAtlas | NICT NICTER darknet sensor (nicter.jp/atlas) | Darknet attack visualization |

## Fusion collectors

These merge other in-repo collectors rather than raw upstream feeds:

- `unifiedTrains` — mlitN02Stations + odptTransport + fullTransport + osmTransportTrains
- `unifiedSubways` — mlitN02Stations + odptTransport + fullTransport + osmTransportSubways
- `unifiedBuses` — mlitP11BusStops + gtfsJp + busRoutes + osmTransportBuses
- `unifiedAisShips` — maritimeAis + marineTraffic + vesselFinder
- `unifiedPortInfra` — portInfra + osmTransportPorts + mlitC02Ports
- `cameraDiscovery` — fans out across OSM, JMA, MLIT, Shodan, YouTube, Insecam, Windy

## Internal helpers (not collectors)

Files prefixed `_` under `server/src/collectors/`:

- `_cameraSources.js` — channel registry used by `cameraDiscovery`
- `_dedupe.js` — merge / dedupe utilities for fusion collectors
- `_liveHelpers.js` — shared fetch/normalize helpers
- `_municipalityCentroids.js` — precomputed city centroids for geographic lookups
