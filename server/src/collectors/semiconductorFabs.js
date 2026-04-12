/**
 * Semiconductor Fabs Collector
 * Major Japanese semiconductor wafer fabs and packaging plants.
 * METI semicon strategy + IR public data.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["industrial"="semiconductor"](area.jp);way["industrial"="semiconductor"](area.jp);node["industrial"="electronics"](area.jp);way["industrial"="electronics"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        fab_id: `FAB_LIVE_${String(i + 1).padStart(5, '0')}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Fab ${el.id}`,
        company: el.tags?.operator || el.tags?.brand || 'unknown',
        tech: el.tags?.industrial || 'semiconductor',
        country: 'JP',
        source: 'semicon_live',
      },
    })
  );
}

const SEED_FABS = [
  // Kioxia (formerly Toshiba Memory) — NAND flash
  { name: 'キオクシア 四日市工場', lat: 34.9311, lon: 136.6286, company: 'Kioxia', tech: 'NAND', wafer_size_mm: 300, fab_count: 6 },
  { name: 'キオクシア 北上工場', lat: 39.2950, lon: 141.0858, company: 'Kioxia', tech: 'NAND', wafer_size_mm: 300, fab_count: 1 },
  // Sony Semiconductor (CMOS image sensors)
  { name: 'ソニーセミコンダクタ 熊本TEC (Fab1/2)', lat: 32.8772, lon: 130.7556, company: 'Sony', tech: 'CIS', wafer_size_mm: 300, fab_count: 2 },
  { name: 'ソニーセミコンダクタ 長崎TEC (諫早)', lat: 32.8553, lon: 130.0381, company: 'Sony', tech: 'CIS', wafer_size_mm: 300, fab_count: 1 },
  { name: 'ソニーセミコンダクタ 鹿児島TEC (国分)', lat: 31.7414, lon: 130.7736, company: 'Sony', tech: 'CIS', wafer_size_mm: 300, fab_count: 1 },
  { name: 'ソニーセミコンダクタ 山形TEC', lat: 38.4189, lon: 140.4350, company: 'Sony', tech: 'CIS', wafer_size_mm: 300, fab_count: 1 },
  { name: 'ソニーセミコンダクタ 大分TEC', lat: 33.2725, lon: 131.7344, company: 'Sony', tech: 'CIS', wafer_size_mm: 200, fab_count: 1 },
  // Renesas Electronics (MCU, automotive)
  { name: 'ルネサス 那珂工場', lat: 36.4639, lon: 140.5358, company: 'Renesas', tech: 'MCU', wafer_size_mm: 300, fab_count: 2 },
  { name: 'ルネサス 川尻工場 (熊本)', lat: 32.7350, lon: 130.7283, company: 'Renesas', tech: 'MCU', wafer_size_mm: 200, fab_count: 1 },
  { name: 'ルネサス 高崎工場', lat: 36.3225, lon: 138.9778, company: 'Renesas', tech: 'MCU', wafer_size_mm: 200, fab_count: 1 },
  { name: 'ルネサス 山口工場 (柳井)', lat: 33.9633, lon: 132.0883, company: 'Renesas', tech: 'analog', wafer_size_mm: 150, fab_count: 1 },
  { name: 'ルネサス 滋賀工場', lat: 35.0211, lon: 135.8594, company: 'Renesas', tech: 'analog', wafer_size_mm: 200, fab_count: 1 },
  // Rohm
  { name: 'ローム 京都本社工場', lat: 34.9583, lon: 135.7561, company: 'Rohm', tech: 'analog', wafer_size_mm: 200, fab_count: 1 },
  { name: 'ローム 筑後工場', lat: 33.2261, lon: 130.5300, company: 'Rohm', tech: 'analog', wafer_size_mm: 200, fab_count: 1 },
  { name: 'ローム 浜松工場', lat: 34.7100, lon: 137.7261, company: 'Rohm', tech: 'discrete', wafer_size_mm: 150, fab_count: 1 },
  { name: 'ラピスセミコンダクタ 宮崎工場 (旧OKI)', lat: 31.8403, lon: 131.4297, company: 'Rohm/Lapis', tech: 'analog', wafer_size_mm: 200, fab_count: 1 },
  // Sumitomo Electric / SEDI
  { name: '住友電気 大阪工場 (光半導体)', lat: 34.7269, lon: 135.5036, company: 'SEDI', tech: 'compound', wafer_size_mm: 100, fab_count: 1 },
  // Mitsubishi Electric (power)
  { name: '三菱電機 福岡工場 (パワー半導体)', lat: 33.5764, lon: 130.4081, company: 'Mitsubishi Electric', tech: 'power', wafer_size_mm: 200, fab_count: 1 },
  { name: '三菱電機 熊本工場', lat: 32.8033, lon: 130.7081, company: 'Mitsubishi Electric', tech: 'power', wafer_size_mm: 150, fab_count: 1 },
  { name: '三菱電機 群馬工場', lat: 36.4006, lon: 139.0581, company: 'Mitsubishi Electric', tech: 'power', wafer_size_mm: 200, fab_count: 1 },
  // Fuji Electric
  { name: '富士電機 松本工場', lat: 36.2275, lon: 137.9683, company: 'Fuji Electric', tech: 'power', wafer_size_mm: 200, fab_count: 1 },
  { name: '富士電機 山梨工場', lat: 35.6389, lon: 138.6464, company: 'Fuji Electric', tech: 'power', wafer_size_mm: 150, fab_count: 1 },
  // Toshiba / Toshiba Electronic Devices
  { name: '東芝デバイス 大分工場', lat: 33.2725, lon: 131.7322, company: 'Toshiba', tech: 'discrete', wafer_size_mm: 200, fab_count: 1 },
  { name: '東芝デバイス 姫路工場', lat: 34.8056, lon: 134.6925, company: 'Toshiba', tech: 'analog', wafer_size_mm: 200, fab_count: 1 },
  // Tower Semiconductor (Japan JV at Nuvoton)
  { name: 'タワー パートナーズ 魚津 (旧Panasonic)', lat: 36.8211, lon: 137.4131, company: 'Tower JP', tech: 'analog', wafer_size_mm: 200, fab_count: 1 },
  // TSMC JASM (new)
  { name: 'TSMC JASM 熊本菊陽工場 Fab1', lat: 32.8911, lon: 130.7717, company: 'TSMC JASM', tech: 'logic_28nm', wafer_size_mm: 300, fab_count: 1 },
  { name: 'TSMC JASM 熊本菊陽工場 Fab2', lat: 32.8950, lon: 130.7750, company: 'TSMC JASM', tech: 'logic_6nm', wafer_size_mm: 300, fab_count: 1 },
  // Rapidus (new)
  { name: 'Rapidus 千歳工場 (建設中)', lat: 42.7833, lon: 141.6878, company: 'Rapidus', tech: 'logic_2nm', wafer_size_mm: 300, fab_count: 1 },
  // Western Digital (JV with Kioxia)
  { name: 'ウェスタンデジタル 四日市拠点', lat: 34.9322, lon: 136.6300, company: 'WD/Kioxia', tech: 'NAND', wafer_size_mm: 300, fab_count: 1 },
  { name: 'ウェスタンデジタル 北上拠点', lat: 39.2961, lon: 141.0867, company: 'WD/Kioxia', tech: 'NAND', wafer_size_mm: 300, fab_count: 1 },
  // Micron Memory Japan (formerly Elpida)
  { name: 'マイクロン 広島工場 (DRAM)', lat: 34.5500, lon: 132.7889, company: 'Micron Japan', tech: 'DRAM', wafer_size_mm: 300, fab_count: 1 },
  // Other
  { name: '京セラ 八日市工場', lat: 35.1167, lon: 136.2167, company: 'Kyocera', tech: 'package', wafer_size_mm: 0, fab_count: 1 },
  { name: 'NXP / Nexperia 日本拠点', lat: 35.6900, lon: 139.7000, company: 'Nexperia', tech: 'discrete', wafer_size_mm: 0, fab_count: 1 },
];

function generateSeedData() {
  return SEED_FABS.map((f, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [f.lon, f.lat] },
    properties: {
      fab_id: `FAB_${String(i + 1).padStart(5, '0')}`,
      name: f.name,
      company: f.company,
      tech: f.tech,
      wafer_size_mm: f.wafer_size_mm,
      fab_count: f.fab_count,
      country: 'JP',
      source: 'semicon_seed',
    },
  }));
}

export default async function collectSemiconductorFabs() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'semiconductor_fabs',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese semiconductor fabs: Kioxia, Sony, Renesas, Rohm, Mitsubishi, Fuji, TSMC JASM, Rapidus, Micron',
    },
    metadata: {},
  };
}
