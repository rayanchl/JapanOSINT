/**
 * Amateur Radio Repeaters Collector
 * JARL repeater listings — VHF/UHF + D-STAR + DMR + HF beacons.
 */

const SEED_REPEATERS = [
  // VHF/UHF analog (selected major mountain-top repeaters)
  { call: 'JR1VK', name: 'JR1VK 大山 神奈川', lat: 35.4406, lon: 139.2436, freq_mhz: 145.78, mode: 'FM', kind: 'mountain' },
  { call: 'JR1VV', name: 'JR1VV 富士山', lat: 35.3606, lon: 138.7311, freq_mhz: 145.36, mode: 'FM', kind: 'mountain' },
  { call: 'JR1WL', name: 'JR1WL 筑波山', lat: 36.2256, lon: 140.0997, freq_mhz: 145.66, mode: 'FM', kind: 'mountain' },
  { call: 'JR1WV', name: 'JR1WV 高尾山', lat: 35.6253, lon: 139.2436, freq_mhz: 145.34, mode: 'FM', kind: 'mountain' },
  { call: 'JR1XS', name: 'JR1XS 愛宕山', lat: 35.3417, lon: 139.0889, freq_mhz: 433.90, mode: 'FM', kind: 'mountain' },
  { call: 'JR2VK', name: 'JR2VK 御嶽山', lat: 35.8919, lon: 137.4811, freq_mhz: 145.70, mode: 'FM', kind: 'mountain' },
  { call: 'JR2WD', name: 'JR2WD 鈴鹿山系', lat: 35.0683, lon: 136.4256, freq_mhz: 145.62, mode: 'FM', kind: 'mountain' },
  { call: 'JR3VC', name: 'JR3VC 六甲山', lat: 34.7800, lon: 135.2628, freq_mhz: 145.74, mode: 'FM', kind: 'mountain' },
  { call: 'JR3VK', name: 'JR3VK 生駒山', lat: 34.6781, lon: 135.6783, freq_mhz: 145.40, mode: 'FM', kind: 'mountain' },
  { call: 'JR3WO', name: 'JR3WO 比叡山', lat: 35.0683, lon: 135.8331, freq_mhz: 145.42, mode: 'FM', kind: 'mountain' },
  { call: 'JR4VC', name: 'JR4VC 大山 (鳥取)', lat: 35.3717, lon: 133.5364, freq_mhz: 145.50, mode: 'FM', kind: 'mountain' },
  { call: 'JR4VK', name: 'JR4VK 蒜山', lat: 35.2942, lon: 133.6533, freq_mhz: 145.38, mode: 'FM', kind: 'mountain' },
  { call: 'JR5VC', name: 'JR5VC 石鎚山', lat: 33.7681, lon: 133.1156, freq_mhz: 145.76, mode: 'FM', kind: 'mountain' },
  { call: 'JR5VG', name: 'JR5VG 剣山', lat: 33.8533, lon: 134.0942, freq_mhz: 145.42, mode: 'FM', kind: 'mountain' },
  { call: 'JR6VC', name: 'JR6VC 阿蘇山', lat: 32.8836, lon: 131.1042, freq_mhz: 145.74, mode: 'FM', kind: 'mountain' },
  { call: 'JR6VG', name: 'JR6VG 雲仙', lat: 32.7536, lon: 130.2942, freq_mhz: 145.78, mode: 'FM', kind: 'mountain' },
  { call: 'JR6VK', name: 'JR6VK 由布岳', lat: 33.2839, lon: 131.3892, freq_mhz: 145.62, mode: 'FM', kind: 'mountain' },
  { call: 'JR6VS', name: 'JR6VS 沖縄本島', lat: 26.4506, lon: 127.7847, freq_mhz: 145.70, mode: 'FM', kind: 'island' },
  { call: 'JR7VC', name: 'JR7VC 蔵王山', lat: 38.1442, lon: 140.4500, freq_mhz: 145.74, mode: 'FM', kind: 'mountain' },
  { call: 'JR7VS', name: 'JR7VS 月山', lat: 38.5478, lon: 140.0264, freq_mhz: 145.30, mode: 'FM', kind: 'mountain' },
  { call: 'JR8VC', name: 'JR8VC 暑寒別岳', lat: 43.7706, lon: 141.5500, freq_mhz: 145.74, mode: 'FM', kind: 'mountain' },
  { call: 'JR8VG', name: 'JR8VG 大雪山', lat: 43.6633, lon: 142.8519, freq_mhz: 145.76, mode: 'FM', kind: 'mountain' },
  { call: 'JR8VK', name: 'JR8VK 樽前山', lat: 42.6883, lon: 141.3719, freq_mhz: 145.62, mode: 'FM', kind: 'mountain' },
  { call: 'JR9VC', name: 'JR9VC 立山', lat: 36.5764, lon: 137.6206, freq_mhz: 145.74, mode: 'FM', kind: 'mountain' },
  // D-STAR
  { call: 'JP1YJV', name: 'JP1YJV 東京葛飾 D-STAR', lat: 35.7400, lon: 139.8500, freq_mhz: 439.10, mode: 'D-STAR', kind: 'urban' },
  { call: 'JP3YHL', name: 'JP3YHL 大阪 D-STAR', lat: 34.6900, lon: 135.5000, freq_mhz: 439.34, mode: 'D-STAR', kind: 'urban' },
  { call: 'JP6YJD', name: 'JP6YJD 福岡 D-STAR', lat: 33.5900, lon: 130.4017, freq_mhz: 439.54, mode: 'D-STAR', kind: 'urban' },
  { call: 'JP8YEH', name: 'JP8YEH 札幌 D-STAR', lat: 43.0640, lon: 141.3469, freq_mhz: 439.18, mode: 'D-STAR', kind: 'urban' },
  // DMR / C4FM
  { call: 'JR1VL', name: 'JR1VL 関東 C4FM', lat: 35.6800, lon: 139.7700, freq_mhz: 433.20, mode: 'C4FM', kind: 'urban' },
  { call: 'JR3VG', name: 'JR3VG 関西 C4FM', lat: 34.7000, lon: 135.5000, freq_mhz: 433.30, mode: 'C4FM', kind: 'urban' },
  // HF beacons
  { call: 'JA2IGY', name: 'JA2IGY 14MHz IBP beacon', lat: 34.8533, lon: 137.5633, freq_mhz: 14.100, mode: 'CW beacon', kind: 'hf_beacon' },
  { call: '8N1HQ', name: '8N1HQ HQ Tokyo special', lat: 35.6900, lon: 139.7600, freq_mhz: 7.050, mode: 'SSB/CW', kind: 'hq' },
];

function generateSeedData() {
  return SEED_REPEATERS.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      repeater_id: `RPT_${String(i + 1).padStart(5, '0')}`,
      callsign: r.call,
      name: r.name,
      freq_mhz: r.freq_mhz,
      mode: r.mode,
      kind: r.kind,
      country: 'JP',
      source: 'jarl_repeater_seed',
    },
  }));
}

export default async function collectAmateurRadioRepeaters() {
  const features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'amateur_radio_repeaters',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: false,
      description: 'JARL amateur radio repeaters: VHF/UHF FM, D-STAR, C4FM digital, HF beacons',
    },
    metadata: {},
  };
}
