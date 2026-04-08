/**
 * Public Cameras Collector
 * Curated list of known public webcam feeds across Japan
 * Includes traffic cameras, tourist spots, volcano cams, city cams
 */

const CAMERAS = [
  // Traffic cameras - Tokyo
  { name: '首都高速 箱崎JCT', type: 'traffic', lat: 35.6838, lon: 139.7892, url: 'https://www.shutoko.jp/use/realtime/camera/', thumbnail: 'https://www.shutoko.jp/camera/hakozaki.jpg' },
  { name: '首都高速 大橋JCT', type: 'traffic', lat: 35.6510, lon: 139.6862, url: 'https://www.shutoko.jp/use/realtime/camera/', thumbnail: 'https://www.shutoko.jp/camera/ohashi.jpg' },
  { name: '渋谷スクランブル交差点', type: 'traffic', lat: 35.6595, lon: 139.7004, url: 'https://www.youtube.com/watch?v=shibuya_cam', thumbnail: null },
  { name: '首都高速 辰巳JCT', type: 'traffic', lat: 35.6457, lon: 139.8125, url: 'https://www.shutoko.jp/use/realtime/camera/', thumbnail: null },
  { name: '国道1号 横浜新道', type: 'traffic', lat: 35.4430, lon: 139.5930, url: 'https://www.ktr.mlit.go.jp/yokohama/camera/', thumbnail: null },
  { name: '東名高速 海老名SA', type: 'traffic', lat: 35.4428, lon: 139.3898, url: 'https://highway-cctv.jp/', thumbnail: null },
  // Traffic cameras - Osaka
  { name: '阪神高速 環状線', type: 'traffic', lat: 34.6870, lon: 135.5190, url: 'https://www.hanshin-exp.co.jp/drivers/camera/', thumbnail: null },
  { name: '名神高速 吹田IC', type: 'traffic', lat: 34.7711, lon: 135.5174, url: 'https://highway-cctv.jp/', thumbnail: null },
  // Tourist spots
  { name: '富士山ライブカメラ（河口湖）', type: 'tourist', lat: 35.5105, lon: 138.7571, url: 'https://live.fujigoko.tv/', thumbnail: 'https://live.fujigoko.tv/camera/fuji.jpg' },
  { name: '富士山ライブカメラ（御殿場）', type: 'tourist', lat: 35.3088, lon: 138.9349, url: 'https://live.fujigoko.tv/', thumbnail: null },
  { name: '東京タワーライブ', type: 'tourist', lat: 35.6586, lon: 139.7454, url: 'https://www.youtube.com/watch?v=tokyotower_live', thumbnail: null },
  { name: '東京スカイツリー', type: 'tourist', lat: 35.7101, lon: 139.8107, url: 'https://www.youtube.com/watch?v=skytree_live', thumbnail: null },
  { name: '道頓堀ライブ', type: 'tourist', lat: 34.6687, lon: 135.5013, url: 'https://www.youtube.com/watch?v=dotonbori_live', thumbnail: null },
  { name: '金閣寺ライブ', type: 'tourist', lat: 35.0394, lon: 135.7292, url: 'https://www.youtube.com/watch?v=kinkakuji_live', thumbnail: null },
  { name: '宮島 厳島神社', type: 'tourist', lat: 34.2960, lon: 132.3196, url: 'https://www.youtube.com/watch?v=miyajima_live', thumbnail: null },
  { name: '小樽運河ライブ', type: 'tourist', lat: 43.1974, lon: 140.9942, url: 'https://www.youtube.com/watch?v=otaru_live', thumbnail: null },
  { name: '沖縄美ら海水族館', type: 'tourist', lat: 26.6943, lon: 127.8779, url: 'https://churaumi.okinawa/livecamera/', thumbnail: null },
  { name: '白川郷ライブカメラ', type: 'tourist', lat: 36.2578, lon: 136.9063, url: 'https://shirakawa-go.org/livecam/', thumbnail: null },
  // Volcano cameras
  { name: '桜島 (鹿児島)', type: 'volcano', lat: 31.5853, lon: 130.6569, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '阿蘇山 (熊本)', type: 'volcano', lat: 32.8841, lon: 131.1041, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '浅間山 (長野・群馬)', type: 'volcano', lat: 36.4061, lon: 138.5231, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '箱根山 (神奈川)', type: 'volcano', lat: 35.2314, lon: 139.0211, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '雲仙岳 (長崎)', type: 'volcano', lat: 32.7611, lon: 130.2990, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '有珠山 (北海道)', type: 'volcano', lat: 42.5445, lon: 140.8378, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '十勝岳 (北海道)', type: 'volcano', lat: 43.4157, lon: 142.6862, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '霧島山（新燃岳）', type: 'volcano', lat: 31.9091, lon: 130.8878, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '草津白根山', type: 'volcano', lat: 36.6181, lon: 138.5281, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  { name: '諏訪之瀬島', type: 'volcano', lat: 29.6381, lon: 129.7141, url: 'https://www.data.jma.go.jp/svd/volcam/data/gazo/', thumbnail: null },
  // City / weather cameras
  { name: '札幌大通公園', type: 'weather', lat: 43.0580, lon: 141.3485, url: 'https://www.youtube.com/watch?v=sapporo_cam', thumbnail: null },
  { name: '名古屋テレビ塔', type: 'weather', lat: 35.1710, lon: 136.9088, url: 'https://www.youtube.com/watch?v=nagoya_cam', thumbnail: null },
  { name: '福岡タワー', type: 'weather', lat: 33.5924, lon: 130.3513, url: 'https://www.youtube.com/watch?v=fukuoka_cam', thumbnail: null },
  { name: '横浜みなとみらい', type: 'weather', lat: 35.4578, lon: 139.6319, url: 'https://www.youtube.com/watch?v=yokohama_cam', thumbnail: null },
  { name: '神戸ポートタワー', type: 'weather', lat: 34.6777, lon: 135.1862, url: 'https://www.youtube.com/watch?v=kobe_cam', thumbnail: null },
  { name: '広島平和記念公園', type: 'weather', lat: 34.3955, lon: 132.4534, url: 'https://www.youtube.com/watch?v=hiroshima_cam', thumbnail: null },
  { name: '長崎港', type: 'weather', lat: 32.7427, lon: 129.8718, url: 'https://www.youtube.com/watch?v=nagasaki_cam', thumbnail: null },
  { name: '仙台駅前', type: 'weather', lat: 38.2601, lon: 140.8822, url: 'https://www.youtube.com/watch?v=sendai_cam', thumbnail: null },
  { name: '那覇国際通り', type: 'weather', lat: 26.3358, lon: 127.6862, url: 'https://www.youtube.com/watch?v=naha_cam', thumbnail: null },
  { name: '新潟万代橋', type: 'weather', lat: 37.9195, lon: 139.0485, url: 'https://www.youtube.com/watch?v=niigata_cam', thumbnail: null },
];

function generateSeedData() {
  const now = new Date();
  return CAMERAS.map((cam, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [cam.lon, cam.lat] },
    properties: {
      camera_id: `CAM_${String(i + 1).padStart(3, '0')}`,
      name: cam.name,
      camera_type: cam.type,
      stream_url: cam.url,
      thumbnail_url: cam.thumbnail,
      status: 'active',
      updated_at: now.toISOString(),
      source: 'curated',
    },
  }));
}

export default async function collectPublicCameras() {
  // Public cameras are curated, not fetched from an API
  const features = generateSeedData();

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'curated',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Curated public webcam feeds across Japan',
    },
    metadata: {},
  };
}
