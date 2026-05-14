/**
 * Shared seed data for satellite-family collectors.
 */

export const JAPAN_BBOX = [122, 24, 154, 46]; // [W, S, E, N]

// Tokyo centroid — used as a fallback geometry by threat-intel collectors
// whose rows have no per-record lat/lon (most do not).
export const TOKYO = [139.6917, 35.6895];

// Imagery fallback grid — 5x3 centroids over Japan, used when all live
// imagery providers fail.
export const IMAGERY_SEED_CENTROIDS = [
  { lon: 130, lat: 33, region: 'Kyushu' },
  { lon: 132, lat: 34, region: 'Chugoku' },
  { lon: 134, lat: 34, region: 'Shikoku' },
  { lon: 136, lat: 35, region: 'Kansai/Tokai' },
  { lon: 139, lat: 36, region: 'Kanto' },
  { lon: 141, lat: 38, region: 'Tohoku' },
  { lon: 142, lat: 41, region: 'Hokkaido south' },
  { lon: 143, lat: 43, region: 'Hokkaido east' },
  { lon: 127, lat: 26, region: 'Okinawa' },
  { lon: 124, lat: 24, region: 'Yaeyama' },
];

// Additional commercial + university ground-station sites.
export const EXTRA_GROUND_STATIONS = [
  { name: 'Intelsat Ibaraki', lat: 36.2050, lon: 140.6300, operator: 'Intelsat', kind: 'commercial_satcom', bands: 'C,Ku', category: 'satcom' },
  { name: 'Inmarsat Yamaguchi', lat: 34.0500, lon: 131.5600, operator: 'Inmarsat', kind: 'commercial_satcom', bands: 'L,Ku', category: 'satcom' },
  { name: 'NTT Yokohama Teleport', lat: 35.4400, lon: 139.6400, operator: 'NTT', kind: 'commercial_satcom', bands: 'C,Ku', category: 'satcom' },
  { name: 'SoftBank Chiba Gateway', lat: 35.3300, lon: 140.3800, operator: 'SoftBank', kind: 'commercial_satcom', bands: 'Ka', category: 'satcom' },
  { name: 'Rakuten Mobile Satellite Gateway', lat: 35.6800, lon: 139.7600, operator: 'Rakuten', kind: 'commercial_satcom', bands: 'Ka', category: 'satcom' },
  { name: 'UTokyo Kashiwa Ground Station', lat: 35.9000, lon: 139.9400, operator: 'U. of Tokyo', kind: 'university', bands: 'S,X', category: 'university' },
  { name: 'Kyushu University Ground Station', lat: 33.5900, lon: 130.2200, operator: 'Kyushu Univ.', kind: 'university', bands: 'S', category: 'university' },
  { name: 'Hokkaido University Uchinada GS', lat: 43.0700, lon: 141.3500, operator: 'Hokkaido Univ.', kind: 'university', bands: 'S', category: 'university' },
  { name: 'Tohoku Univ. CubeSat GS', lat: 38.2500, lon: 140.8400, operator: 'Tohoku Univ.', kind: 'university', bands: 'UHF,S', category: 'university' },
];

// VLBI radio telescopes (VERA array + Nobeyama + Kashima).
export const VLBI_STATIONS = [
  { name: 'VERA Mizusawa', lat: 39.1336, lon: 141.1328, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'VERA Iriki', lat: 31.7475, lon: 130.4397, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'VERA Ishigakijima', lat: 24.4122, lon: 124.1711, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'VERA Ogasawara', lat: 27.0919, lon: 142.2167, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'Nobeyama 45m Radio Telescope', lat: 35.9417, lon: 138.4722, operator: 'NAOJ', bands: 'mm', category: 'vlbi' },
  { name: 'Kashima 34m Antenna', lat: 35.9536, lon: 140.6597, operator: 'NICT', bands: 'S,X,K', category: 'vlbi' },
];

// Satellite Laser Ranging stations.
export const SLR_STATIONS = [
  { name: 'Koganei SLR', lat: 35.7100, lon: 139.4900, operator: 'NICT', bands: 'laser', category: 'slr' },
  { name: 'Simosato Hydrographic Observatory', lat: 33.5772, lon: 135.9369, operator: 'JHA', bands: 'laser', category: 'slr' },
];

// Optical satellite tracking observatories.
export const OPTICAL_TRACKING_STATIONS = [
  { name: 'Bisei Spaceguard Center', lat: 34.6717, lon: 133.5444, operator: 'JSGA', bands: 'optical', category: 'optical_tracking' },
  { name: 'JAXA Mt. Nyukasa Observatory', lat: 35.9750, lon: 138.1917, operator: 'JAXA', bands: 'optical', category: 'optical_tracking' },
];

// Curated fallback subset of GEONET stations (used when GSI live list fetch fails).
// Full station list (~1,300) is fetched live in the collector.
export const GEONET_FALLBACK = [
  { name: 'GEONET 940001 Wakkanai', station_code: '940001', lat: 45.4040, lon: 141.6897 },
  { name: 'GEONET 940058 Sapporo', station_code: '940058', lat: 43.0700, lon: 141.3350 },
  { name: 'GEONET 950211 Sendai', station_code: '950211', lat: 38.2680, lon: 140.8710 },
  { name: 'GEONET 960603 Tsukuba', station_code: '960603', lat: 36.1060, lon: 140.0870 },
  { name: 'GEONET 93010 Tokyo', station_code: '93010', lat: 35.7100, lon: 139.4880 },
  { name: 'GEONET 950265 Nagoya', station_code: '950265', lat: 35.1700, lon: 136.9600 },
  { name: 'GEONET 960647 Osaka', station_code: '960647', lat: 34.6860, lon: 135.5200 },
  { name: 'GEONET 970791 Hiroshima', station_code: '970791', lat: 34.3900, lon: 132.4600 },
  { name: 'GEONET 950460 Fukuoka', station_code: '950460', lat: 33.5900, lon: 130.4000 },
  { name: 'GEONET 940089 Naha', station_code: '940089', lat: 26.2120, lon: 127.6800 },
];
