#!/usr/bin/env python3
"""Split the big GIS-layer beats (and the mixed body-type beats) into collectors
by what each row IS: give every row an explicit `.collector`.

    python3 tools/split_beats_by_theme.py plan  collectors/pivot/table/hp3b61_us.c ... > plan.tsv
    python3 tools/split_beats_by_theme.py apply plan.tsv collectors/pivot/table/hp3b61_us.c ...
    python3 tools/split_beats_by_theme.py manifest plan.tsv ../docs/candidate-sources-batch61.us.txt ...
    python3 tools/split_beats_by_theme.py summary plan.tsv          # per-collector counts, `other` shares

Sibling of tools/split_beats_by_body_type.py and the same three-step shape
(plan -> apply -> manifest); read that file's header for the argument. This one
covers the beats the first split left alone (2026-11-14 collector
classification audit, section 5):

  * ArcGIS layer beats — hp3b61_us 3,908, hp3b62_eu 4,535, hp3b60_us 2,495,
    hp3b59_jpgis 2,543, hp3b60_eu 1,264, hp3b58_arcgis{,2,3} 2,947, hp3b61_jp
    761, hp3b53/46_arcgislayers 454. One collector each mixed parcels, wards,
    hydrants, flood zones, schools and transit, so every Collectors page showed
    them as "Geospatial" whatever the layer is. Key: `<region>_gis_<theme>`,
    region from the file (or, in the two mixed files, from the publisher's
    name), theme from the LAYER's name by ordered rules over a CLOSED set.
  * hp3b48_igo (intergovernmental bodies beside associations and company
    blogs), hp3b44_usgov (cities, counties, townships, tribes, districts,
    schools), hp3b47_wdgov (ministries, agencies, regulators, funders,
    universities) and the scheduled `government` collector.

What a plan row may change: `.collector`, and `.category` ONLY for the body-type
beats (a company blog is not "international"). The ArcGIS beats keep the
category they have — the theme is a property of the COLLECTOR. No id, URL, key
or paging field is touched, so no emit measurement is invalidated.

How a theme is chosen (ArcGIS), in order — first match wins:
  1. ordered regexes over the layer's own name (camelCase and `_` split first;
     English, Japanese, Dutch, French, German, Spanish, Portuguese, Nordic);
  2. the same regexes over the SERVICE name in the row's URL, for a layer
     called "Layer 0" or "Sheet1" whose service says what it holds;
  3. the row's existing category, ONLY where it is itself specific (transport,
     safety, environment, education, health, culture) — `geospatial`,
     `infrastructure` and `government` were the generator's defaults and carry
     no information;
  4. otherwise `<region>_gis_other`. Never a guessed theme.
The rules are data about these registers as read on 2026-11-16; a rule that no
longer matches anything is harmless and a new row is typed the same way.

Keys need a line in tools/gen_collectors_def.py (that file derives the group and
label of a `<region>_gis_<theme>` key from the same THEMES table, so nothing is
hand-listed); `make collectors-def` + `make collectors-def-check` verify it.
"""
import re, sys, collections, os, urllib.parse

R = lambda *p: re.compile('|'.join(p), re.I)

THEMES = [  # key, label, group (kept in sync with gen_collectors_def.py's GIS_THEMES)
    'boundaries', 'parcels_landuse', 'transport', 'utilities_infrastructure',
    'environment_hazards', 'safety_emergency', 'health_social', 'education',
    'planning_zoning', 'elections_admin', 'imagery_basemap', 'culture_recreation',
    'economy_business', 'other',
]

# (theme, regex) over the normalised layer name; first match wins.
def _sp(term):
    return term.replace(' ?', r'\s?').replace(' ', r'\s+')


def W(*words):
    """whole words, optional plural (s/es), case-insensitive; a space in a term matches any run of blanks."""
    return r'\b(?:' + '|'.join(_sp(w) for w in words) + r')(?:s|es)?\b'


def S(*stems):
    """word STEMS: the term may continue (flood -> flooding, floodplain). A term ending in \\b is exact."""
    return r'\b(?:' + '|'.join(_sp(s) for s in stems) + r')' + r'(?:\w*)'


def X(*cjk):
    """substrings (CJK terms and romanised Japanese, which have no word boundaries)."""
    return '|'.join(cjk)


# (theme, regex) over the normalised layer name; first match wins.  The ORDER is
# the argument: a hydrant is a utility before it is "fire", an evacuation route
# is safety before it is a road, a flood zone is a hazard before it is a zone.
THEME_RULES = [
 # Japanese planning terms, in kanji and in the romaji GIS authors use for layer
 # names (toshikeikakukuiki = 都市計画区域), ahead of everything: 都市計画道路 and
 # toshikeikakukasen are planning layers, not roads or rivers.
 ('planning_zoning', R(X('都市計画', 'toshikeikaku', 'toshi_?keikaku', '用途地域', 'youtochiiki', 'youto', '市街化', 'shigaika', '立地適正化', '都市機能誘導', '居住誘導', '地区計画', 'chikukeikaku', '土地区画整理', 'tochikukaku', '風致', 'huuchi', 'fuuchi', '高度地区', '防火地域', '準防火', '特別用途', '特別緑地', 'tokubetsuryokuchi', '第一種', '第二種', '低層住居', '中高層', '住居地域', '商業地域', '工業地域', '景観計画', 'keikankeikaku', '開発許可', '開発行為', 'toshisaikaihatsu', '再開発', '都市再生', '生産緑地', 'seisanryokuchi', '市街地', 'tokuteigaiku', '特定街区', 'tokubetsukougyou', '特別工業', 'junbouka', 'bouka', '白地', 'shirochi', 'kuiki_?kubun', 'kyoju', 'toshikino', 'ritchi'))),
 ('health_social', R(r'クーリング', r'\bcooling\b', X('熱中症', 'nettyuusyou', 'necchusho', '児童館', 'kodomo', '子ども', '子育て', 'kosodate', 'papamama', 'パパママ', '応援ショップ', '子ども食堂', '指定施設一覧', 'shiteishisetsu'))),
 ('imagery_basemap', R(S('imagery', 'orthophoto', 'ortho', 'aerial', 'basemap', 'base map', 'hillshade', 'lidar', 'raster', 'tile cache', 'mosaic', 'annotation', 'photo cent', 'photo point', 'photogram', 'cartograf', 'cartograph', 'topograph', 'gazetteer', 'geonames', 'place names?', 'toponym', 'control point', 'vertical control', 'geodetic', 'benchmark', 'survey control', 'index map', 'map index', 'sheet index', 'thomas brothers', 'quad\\b', 'quadrangle', 'grids?\\b', 'usng', 'mgrs', 'atlas grid', 'graticule'),
                       W('dem', 'dsm', 'dtm', 'elevation', 'contour', 'label', 'scale', 'tile', 'places?', 'geoid', 'gcp', 'bm', r'bathymetr\w+'),
                       X('標高', '等高線', 'オルソ', '空中写真', '背景地図', 'ラベル', 'メッシュ', '図郭', 'zukaku', '写真撮影', 'cityplan_?grid', '図郭', 'zukaku', 'tokei_?info', '時計'))),
 ('elections_admin', R(S('precinct', 'voting', 'voter', 'polling', 'election', 'electoral', 'ballot', 'senate', 'legislat', 'congress', 'redistrict', 'constituenc', 'parliamentar', 'council (district|ward|member)', 'commissioner', 'supervisor district', 'school board', 'trustee', 'alderm', 'ward\\d', 'wards\\b', 'wahlkreis', 'wahlbezirk', 'stembureau', 'kiesdistrict', 'kiesreg', 'circonscription', 'bureau de vote', 'valkrets', 'valdistrikt', 'valgkrets', 'valgsted', 'distrito electoral', 'colegio electoral', 'collegio elettorale', 'seccion', 'sección', 'secção', 'senator', 'mayor', 'political'),
                       X('投票区', '選挙', '議員', '議会', '投票所', '開票', 'kaihyo', '自治会', 'jichikai', 'kuchou', '区長'))),
 ('utilities_infrastructure', R(S('hydrant', 'fire hydrant', 'valve', 'sewer', 'manhole', 'storm ?water', 'stormdrain', 'storm drain', 'drainage', 'catch ?basin', 'culvert', 'water (main|line|pipe|service|meter|system|tower|tank|treatment|utility|supply|distribution|plant|well|pressure|zone|network|connection)', 'waterworks', 'wastewater', 'pump(ing)? station', 'lift station', 'utilit', 'electric', 'power ?(line|plant|station|pole|grid|generation)', 'nuclear', 'reactor', 'transmission line', 'substation', 'street ?light', 'light pole', 'lighting', 'luminair', 'gas (main|line|service)', 'pipeline', 'fiber', 'broadband', 'telecom', 'cell tower', 'cell site', 'wi-?fi', 'free ?spot', 'hotspot', 'ev charg', 'charging (station|point|pole)', 'oplaadpunt', 'laadpunt', 'laadpaal', 'laadpalen', 'laadinfra', 'laadbehoefte', 'rwzi', 'riool', 'gemaal', 'gemalen', 'stuw', 'duiker', 'sluis', 'legger', 'kering', 'watergang', 'sloot', 'sloten', 'afval', 'abfall', 'müll', 'muell', 'problemmuell', 'solar', 'solenergi', 'wind turbine', 'windpark', 'landfill', 'recycl', 'trash', 'garbage', 'refuse', 'transformer', 'gasleiding', 'kabel', 'leiding', 'elektri', 'strøm', 'strom', 'fjernvarme', 'district heating', 'warmte', 'heat network', 'alcantarill', 'tragante', 'pozo', 'saneamiento', 'abastecimiento', 'alumbrado', 'farola', 'acquedott', 'fognatur', 'illuminazione', 'rete (idrica|fognaria|gas|elettrica)', 'assainissement', 'eaux usées', 'éclairage', 'réseau', 'canalisation', 'infrastructure', 'telephone exchange', 'circuits?\\b', 'poles?\\b', 'billing', 'meter', 'dam\\b', 'weir', 'outfall', 'inlet', 'pole\\d'),
                                X('水道', '下水', '上水', '排水', '消火栓', '防火水槽', '街路灯', '防犯灯', '街灯', '電気', '電柱', '発電', 'ガス', '通信', '基地局', 'ごみ', 'ゴミ', '廃棄物', '浄水', 'ポンプ', 'マンホール', '汚物処理', '処理場', '処理施設', '配水', '管渠', 'ダム', 'suido', 'gesui', 'suidou', 'gesuidou', 'shoukasen', 'shokasen', 'bousuiso', 'boukasuiso', 'bokasuiso', 'boukasui', 'gaitou', 'gaitoh', 'denchu', 'jyoususi', 'jousui', 'hairyo', 'haisui', 'syorijo', 'shorijo', 'gomi', 'haikibutsu', 'streetlight', 'wifi', 'wi_fi', '管路', '取付管', '公衆電話', 'koushuudenwa', '水路', '無線', '汚水', '雨水', 'uosui', 'usui', 'kankyo_?gesui', '電話', 'シェル'))),
 ('safety_emergency', R(S('evacuat', 'shelter', 'refuge', 'fire (station|response|district|department|dept|run|protection|prevention|lane|rescue|box)', '\\bfd\\b', 'fhsz', 'firstaid', 'first aid', 'first responder', 'responder', 'police', 'sheriff', 'crime', 'accident', 'crash', 'collision', 'emergency', '\\bems\\b', 'ambulance', '\\baed\\b', 'defibrillat', '911', 'dispatch', 'bosai', 'hurricane', 'levee', 'dam breach', 'inundation', 'siren', 'hazmat', 'disaster', 'rescue', 'safety', 'fatal', 'incident', 'surveillance camera', 'cctv', 'speed camera', 'red light', 'alert', 'warning', 'damage', 'cwpp', 'burn (ban|permit)', 'mitigation', 'brandvæsen', 'brannstasjon', 'brandweer', 'politie', 'veiligheid', 'ongeval', 'ongeluk', 'ongevallen', 'hulpverlening', 'beredskap', 'brandstation', 'räddning', 'uheld', 'ulykke', 'trafikkulykke', 'feuerwehr', 'rettung', 'unfall', 'pompier', 'incendie', 'secours', 'sapeur', 'accidente', 'bomberos', 'emergencia', 'protección civil', 'proteccion civil', 'bombeiros', 'proteção civil', 'protecao civil', 'vigili', 'protezione civile', 'carabinieri', 'sinistr'),
                    X('避難', '防災', 'bousai', 'bosai', '消防', 'shoubou', 'shobo', '災害対策', '救急', '警察', '事故', 'jiko', '犯罪', '防犯', 'hanzai', 'bouhan', '交通安全', '火災', 'kasai', '耐震', 'ヘリポート', 'heliport', 'hinan', '被害', 'higai', '指定緊急', 'シェルター', '救助', '消防団', '災害', '罹災', 'risai', '倒壊', 'tousai', '危険箇所', 'kiken', '浸透', 'evac', '避難場所', '緊急', 'kinkyu', '防火', 'bouka', '家屋倒壊'))),
 ('environment_hazards', R(S('flood', 'fema', 'nfhl', 'sfha', 'hazard', 'landslide', 'liquefaction', 'tsunami', 'storm surge', 'surge', 'sea level', 'wildfire', 'wild fire', 'drought', 'earthquake', 'seismic', 'fault', 'erosion', 'avalanche', 'volcan', 'water quality', 'bathing', 'emission', 'air quality', 'noise', 'lden', 'lnight', 'pollut', 'contamina', 'watershed', 'huc\\d', 'wetland', 'stream', 'creek', 'river', 'lake', 'pond', 'reservoir', 'hydrograph', 'hydrolog', 'waterbod', 'water bod', 'groundwater', 'aquifer', 'spring', 'coast', 'shoreline', 'marine', 'aquatic', 'habitat', 'wildlife', 'species', 'eagle', 'nest', 'bird', 'fish', 'salmon', 'lamprey', 'sediment', 'substrate', 'forest', 'tree', 'canopy', 'vegetation', 'soil', 'geolog', 'corine', 'clc\\b', 'land cover', 'landcover', 'conservation', 'protected area', 'natura 2000', 'ramsar', 'environment', 'climate', 'rainfall', 'precipitation', 'snow', 'weather', 'temperature', 'norwest', 'owqi', 'shade', 'uwwtp', 'prtr', 'bwd\\b', 'rbd', 'eeadmz', 'ecolog', 'biodivers', 'invasive', 'spill', 'mosquito', 'steep slope', 'debris', 'slope', 'slope stab', 'brownfield', 'superfund', 'toxic', 'radon', 'green', 'groen', 'natuur', 'natur', 'naturtyp', 'våtmark', 'bomen', 'boom\\b', 'houtkant', 'eikenprocessie', 'processierups', 'overstromin', 'kwelgevoelig', 'oppervlaktewater', 'waterkering', 'beperkingengebied', 'waterstaatswerk', 'waterbeheer', 'waterschap', 'watersysteem', 'waterloop', 'beek', 'beken', 'cultuurhistorie', 'hochwasser', 'überschwemm', 'ueberschwemm', 'überflut', 'lärm', 'umwelt', 'gewässer', 'gewaesser', 'wald', 'biotop', 'schutzgebiet', 'naturschutz', 'medio ?ambiente', 'inundaci', 'agua', 'aigua', 'rio\\b', 'hidrograf', 'bacia', 'basins?\\b', 'zones? (humides|inondables)', 'inondation', 'crue', 'risque', 'risco', 'riesgo', 'rischio', 'faresone', 'fare\\b', 'rasfare', 'skred', 'flom', 'miljø', 'miljö', 'vattendrag', 'fallvilt', 'diffusion tube', 'no2', 'apicoll', 'permafrost', 'glacier', 'ice\\b', 'wave', 'tide', 'tidal', 'ocean', 'sea\\b', 'reef', 'mangrove', 'estuar', 'dune', 'beach', 'cliff', 'karst', 'cave', 'mineral', 'quarr', 'mining', 'mine\\b', 'mines\\b', 'borehole'),
                         X('土砂', '土石流', 'dosekiryu', 'doseki', '急傾斜', 'kyukeisha', '地すべり', 'jisuberi', '崩壊', '浸水', 'shinsui', '洪水', 'kozui', 'kouzui', '氾濫', '津波', 'tsunami', '高潮', 'takashio', 'ハザード', '液状化', '河川', 'kasen', '河岸', '砂防', 'sabo', '警戒区域', 'keikaikuiki', '雨量', '水位', '雪', '火山', 'kazan', '地震', '活断層', 'ため池', 'tameike', '湿地', '森林', 'shinrin', '植生', '自然', 'shizen', '環境', '景観', '大気', '騒音', '水質', '土壌', '海岸', 'kaigan', '護岸', '熊', '鳥獣', '野生', '有害鳥獣', '想定最大規模', '想定区域', '洪水浸水', '渓流', '特別警戒', '干潟', '泥流', '落石', '雪崩', '凍結', '融雪', '水害', 'suigai', '流域', 'ryuiki', '水系', '地盤', '地質', '断層', '噴火', '噴石', '降灰', '降雨', '浸食', '侵食', '林班', '小班', '治山', '林野', '森', '林', '保安林', 'hoanrin', '河道', 'kadou', '川', '沢', '用水', '堰', '岸', 'ashigawa', 'gawa', 'kagan', 'shinshoku', 'daisan', 'kiken_?shuryuro', 'shuryuro', '主流路', '大規模盛土', '盛土', 'morido', '造成'))),
 ('health_social', R(S('hospital', 'clinic', 'health', 'medical', 'medic\\b', 'pharmac', 'nursing', 'care ?(home|facilit)', 'elder', 'senior (center|centre|housing|service)', 'social service', 'welfare', 'day ?care', 'childcare', 'child care', 'kindergarten', 'preschool', 'covid', 'vaccin', 'dental', 'doctor', 'physician', 'mental', 'hospice', 'food (bank|pantry)', 'homeless', 'disabilit', 'accessib', 'accesibilidad', 'accessibilité', 'toilet', 'restroom', 'food business', 'food safety', 'sanit', 'urgent care', 'public health', 'diagnos', 'öppenvård', 'vård', 'omsorg', 'barnehage', 'förskola', 'sykehus', 'legevakt', 'helse', 'gezondheid', 'zorg', 'ziekenhuis', 'apotheek', 'huisarts', 'kinderopvang', 'gesundheit', 'krankenhaus', 'apotheke', 'pflege', 'santé', 'hôpital', 'hopital', 'pharmacie', 'ehpad', 'crèche', 'creche', 'salud', 'farmacia', 'centro de salud', 'saúde', 'sanità', 'ospedale', 'asilo nido'),
                    X('医療', '病院', '診療', '保健', '福祉', '介護', '高齢', '保育', '薬局', '歯科', '障害', '障がい', 'トイレ', 'toire', '食品', 'ワクチン', 'お年寄り', '児童', 'jidou', '助産', '救護'))),
 ('education', R(S('school', 'college', 'universit', 'campus', 'education', 'attendance (zone|area|boundar)', 'elementary', 'middle school', 'high school', 'student', 'academy', 'learning', 'librar', 'bibliotek', 'bibliothek', 'skole', 'skola', 'skolor', 'gymnas', 'onderwijs', 'schule', 'schul', 'école', 'ecole', 'collège', 'lycée', 'lycee', 'scolaire', 'escuela', 'colegio', 'instituto', 'escola', 'scuola', 'liceo', 'università'),
                  X('学校', '小学校', '中学校', '高校', '高等学校', '大学', '学区', '通学', '教育', '幼稚園', '学習', '図書館', 'gakkou', 'gakko', 'shougakkou', 'chuugakkou', 'shogakko', 'chugakko', 'youchien', 'tosyokan'))),
 ('transport', R(S('road', 'roadway', 'street', 'highway', 'motorway', 'freeway', 'interstate', 'route', 'bus\\b', 'buses', 'busstop', 'bstop', 'bus stop', 'stops?\\b', 'transit', 'tram', 'metro\\b', 'subway', 'rail', 'train', 'airport', 'airfield', 'airstrip', 'airspace', 'heliport', 'port\\b', 'ports\\b', 'harbo', 'ferr', 'marina', 'boat (ramp|launch)', 'bike', 'bicycle', 'cycl', 'greenway', 'sidewalk', 'crosswalk', 'crossing', 'pedestrian', 'bridge', 'tunnel', 'overpass', 'intersection', 'signal', 'traffic', 'parking', 'park and ride', 'park-and-ride', 'plow', 'snow route', 'winter maintenance', 'paving', 'pavement', 'centerline', 'center line', 'mile ?(post|marker)', 'milepost', 'corridor', 'transport', 'mobility', 'vehicle', 'taxi', 'scooter', 'divvy', 'ev\\b', 'adt\\b', 'aadt', 'curb', 'lane\\b', 'lanes\\b', 'detour', 'closure', 'fietspad', 'fietsroute', 'fietsnet', 'fietsen', 'wegen', 'weg\\b', 'straat', 'straten', 'parkeer', 'parkeren', 'bushalte', 'halte', 'spoor', 'trein', 'treinstation', 'station (bus|train|rail|metro)', 'vei\\b', 'veg\\b', 'veier', 'fylkesveg', 'riksveg', 'fotrute', 'sykkel', 'ferje', 'gang og sykkel', 'gangvei', 'trafikk', 'väg', 'gata', 'gator', 'cykel', 'trafik', 'straße', 'strasse', 'strassen', 'bahn', 'haltestelle', 'radweg', 'verkehr', 'rue\\b', 'rues\\b', 'voirie', 'chaussée', 'arrêt', 'transports', 'stationnement', 'horodateur', 'circulation', 'carretera', 'calle', 'parada', 'estrada', 'rua\\b', 'ruas', 'ciclovia', 'aeroporto', 'aeropuerto', 'ponte', 'puente', 'brug', 'viaduct', 'strada', 'strade', 'fermata', 'logradouro', 'tronçon', 'troncon', 'tramo', 'trecho', 'pdipr', 'randonnée', 'sentier'),
                    X('道路', '橋', 'トンネル', '歩道', '横断', '交通', 'バス', '鉄道', '駅', '空港', '港', '路線', '停留所', '自転車', '駐車', '駐輪', '踏切', '信号', '通学路', '中心線', '中央帯', '除雪', '防雪', '橋梁', '道路台帳', 'douro', 'kyouryou', 'kyoryo', 'tonneru', 'chushajo', 'chuushajou', 'kousaten', '平均車速', '車速', '渋滞', '立体交差', '交差点', 'fusyoujiko', 'ガソリンスタンド', 'gasorin', '給油', 'railway', 'tcd_points', 'tetsudo', 'tetudo', '高速', 'kousoku', '林道', 'rindo', '通行', '通れる', '区間', 'kukan', '測点', '路', 'アンダーパス', 'underpass', '構造化', '管理課', '国道', '県道', '市道', 'kokudo', 'kendo', 'shido'))),
 ('culture_recreation', R(S('park\\b', 'parks\\b', 'park and recreation', 'recreation', 'playground', 'playfield', 'trailhead', 'trail\\b', 'trails\\b', 'picnic', 'golf', 'sports?\\b', 'athletic', 'stadium', 'ball ?field', 'courts?\\b', 'basketball', 'tennis', 'pickleball', 'bbq', 'grill', 'pool\\b', 'pools\\b', 'swimming', 'splash pad', 'dog park', 'skate', 'camp(ground|site)?s?\\b', 'boat ramp', 'fishing', 'hunting', 'museum', 'theat', 'cemeter', 'graves?\\b', 'burial', 'historic', 'heritage', 'monument', 'landmark', 'cultural', 'culture', 'public art', 'tourism', 'tourist', 'attraction', 'visitor', 'points? of interest', 'poi\\b', 'amenit', 'community (center|centre)', 'rec ?center', 'civic center', 'worship', 'church', 'temple', 'shrine', 'religious', 'zoo\\b', 'botanical', 'garden', 'open space', 'green ?space', 'natural area', 'nature (reserve|preserve)', 'preserve', 'scenic', 'viewpoint', 'overlook', 'festival', 'kunstobject', 'kunst', 'speelplaats', 'speeltuin', 'sportveld', 'sportpark', 'begraafplaats', 'erfgoed', 'cultuur', 'kerk', 'klatring', 'turistcenter', 'turhytte', 'turist', 'friluft', 'stiar', 'stier\\b', 'sti\\b', 'kulturminne', 'fornminne', 'kultur', 'idrett', 'idrott', 'spielplatz', 'friedhof', 'denkmal', 'freizeit', 'sehenswürd', 'parc\\b', 'parcs\\b', 'jardin', 'cimeti', 'patrimoine', 'sport\\b', 'loisir', 'musée', 'randonn', 'parque', 'jard[ií]n', 'cementerio', 'patrim[oó]nio', 'cultura', 'mercado', 'mercati'),
                          X('公園', 'koen', 'kouen', '文化財', 'bunkazai', '文化', '史跡', '観光', 'kanko', '眺望', 'chobo', 'スポーツ', '運動', '体育', '広場', '遊具', '墓地', '寺', '神社', '名所', '景勝', 'キャンプ', 'ゴルフ', 'プール', '博物館', '美術館', 'イベント', '祭', 'ライブカメラ', 'パンマップ', '歴史', '遺跡', '遊歩道', 'ハイキング', '登山', '温泉', 'ベンチ', '休憩', '火葬', 'kasou', '葬', '眺望', 'チーズ', '動物', '飛行機', '風景', '写真', '投稿', '写真撮影', 'photo', '推奨', '食堂', 'ショップ'))),
 ('planning_zoning', R(S('zoning', 'overlay', 'future land use', 'land use (plan|designation|map|category)', 'general plan', 'comprehensive plan', 'comp plan', 'master plan', 'specific plan', 'plan(ning)? (area|zone|district|boundary|map|application|proposal|reserve)', 'planning', 'zonage', 'plu\\b', 'plui', 'bestemmingsplan', 'bebouwing', 'bebouwde', 'detaljplan', 'bebauungsplan', 'flächennutzung', 'kommuneplan', 'kplan', 'kpa', 'reguleringsplan', 'planreserve', 'development', 'redevelopment', 'rezoning', 'annex', 'variance', 'permit', 'permitted', 'building permit', 'application', 'entitlement', 'tif\\b', 'tax increment', 'opportunity zone', 'historic district', 'conservation area', 'design (review|district)', 'urban (growth|renewal|development)', 'growth (boundary|area)', 'setback', 'height (limit|district|overlay)', 'floor area', 'density', 'affordable', 'housing (element|site|opportunity|project)', 'site inventory', 'objective', 'strategic', 'settlement', 'infill', 'allocation', 'urbaniz', 'urbanismo', 'ordenaci', 'planeamento', 'pdm\\b', 'ordenamento', 'plan\\b', 'plans\\b', 'planned', 'proposed', 'future', 'ldp\\b', 'rmkrfo', 'consultation', 'concession', 'enforcement', 'licen[cs]'),
                        X('開発', '建築', '届出', '許可', '誘導', 'zoneamento'))),
 ('parcels_landuse', R(S('parcel', 'parcela', 'parcely', 'parcelle', 'lot\\b', 'lots\\b', 'lot lines?', 'tax (lot|map|parcel|district|roll)', 'taxlot', 'cadastr', 'kataster', 'property', 'properties', 'real estate', 'land ?use', 'landuse', 'land parcel', 'land ownership', 'ownership', 'owner', 'eigendom', 'panden', 'pand\\b', 'gebouw', 'building', 'footprint', 'structure', 'address', 'addr\\b', 'house ?number', 'huisnummer', 'adres', 'subdivision', 'condo', 'township', 'section lines?', 'plss', 'survey', 'bgt', 'kadaster', 'kadastr', 'kavel', 'bâtiment', 'batiment', 'bâti', 'occupation du sol', 'mos\\b', 'parzell', 'flurst', 'liegenschaft', 'grundst', 'gebäude', 'catastro', 'predio', 'predial', 'edific', 'manzana', 'bloktxt', 'blokken', 'plot\\b', 'tenure', 'acreage', 'lod1', 'lod2', 'housing', 'bygning', 'bygg', 'tomt', 'matrikel', 'fastighet', 'byggnad', 'lands\\b', 'land\\b', 'federal land', 'military', 'defense facility', 'terrain', 'terrenos', 'terreno', 'tierras', 'hoeve', 'perceel', 'percelen', 'kavels'),
                       X('地籍', 'chiseki', '筆', '土地', '建物', 'tatemono', '建築物', '家屋', '住所', 'jyusyo', 'juusho', '住居', '番地', '地番', 'chiban', '公図', '用地', '宅地', '所有', '居住'))),
 ('boundaries', R(S('boundar', 'border', 'limits?\\b', 'city limit', 'town limit', 'municipal', 'munic[ií]p', 'commune', 'gemeente', 'gemeinde', 'kommun', 'kommune', 'obec\\b', 'obce\\b', 'wijk', 'buurt', 'quartier', 'neighbou?rhood', 'district', 'jurisdiction', 'region', 'provinc', 'state\\b', 'states\\b', 'counties', 'county', 'countr', 'nuts', 'admin', 'extent', 'service area', 'catchment', 'census', 'tract', 'block ?group', 'zip ?code', 'postal', 'postcode', 'postleitzahl', 'código postal', 'code postal', 'département', 'freguesia', 'concelho', 'distrito', 'barrio', 'comarca', 'límite', 'limite', 'limites', 'grense', 'grenze', 'gemeentegrens', 'gebied', 'stadsdeel', 'ortsteil', 'stadtteil', 'stadtbezirk', 'bydel', 'kretsar', 'population', 'demograph', 'inwoners', 'unincorporated', 'incorporated', 'annexation', 'parish', 'borough', 'shire', 'territory', 'territorial', 'community', 'communit', 'comune', 'comuni', 'frazion', 'localit', 'ward\\b', 'zone\\b', 'zones\\b', 'sector', 'secteur', 'zonas', 'cuadricula', 'divisoes', 'divisões', 'mesoregi', 'meso região', 'subbacia', 'ibge', 'aglomerado', 'setor', 'bairro', 'faces de logradouro'),
                    X('境界', 'kyokai', '行政', 'gyosei', '市町村', 'sichouson', 'shichoson', 'sityouson', '町丁', '大字', 'ooaza', '字界', '地区', 'chiku', '区域', 'kuiki', '範囲', '管内', '管轄', '区画', '丁目', '年齢別人口', '人口', 'zinkou', 'jinkou', 'jinko', '世帯', '町界', '市界', '区界', '県境', 'shichousonbetsu', 'sichousonkai', 'shichousonkai', '市区町村', '町村', 'sichouson', 'c\\d{5}_', '字', '町内', 'コミュニティ', 'community'))),
 ('economy_business', R(S('business', 'commercial', 'retail', 'shop\\b', 'shops\\b', 'store\\b', 'stores\\b', 'restaurant', 'industr', 'employ', 'jobs?\\b', 'economic', 'economy', 'enterprise', 'incentive', 'commerce', 'chamber', 'market', 'vendor', 'licen[cs]e', 'liquor', 'cannabis', 'marijuana', 'tobacco', 'alcohol', 'hotel', 'lodging', 'freight', 'truck', 'cargo', 'trade\\b', 'agricultur', 'farm', 'crop', 'mining claim', 'oil (and|&) gas', 'energy', 'beauty', 'spa\\b', 'salon', 'tattoo', 'bakery', 'gas station', 'gaststätte', 'gaststaette', 'hotellerie', 'bedrijv', 'bedrijf', 'handel', 'gewerbe', 'empleo', 'empresa', 'comercio', 'negozi', 'mercados', 'auction', 'aoc\\b', 'igp\\b', 'vineyard', 'vigne', 'wine'),
                         X('事業所', 'jigyosho', 'コンビニ', '商業', '店舗', '工業', '産業', '農業', '農地', '経済', '雇用', '企業', '工場', 'ホテル', '宿泊', '飲食', '営業', '食品', '飲食店', 'gasorinsutando'))),
]

# Vocabulary added in the second and third reading of the registers (2026-11-16):
# the long tail of Dutch, Nordic, German, French, Iberian and romanised-Japanese
# layer names the first rules did not reach. Kept apart from THEME_RULES so the
# argument of the ORDER above stays readable; folded into the same regexes below.
def G(*stems):
    """Germanic compounds: the stem may sit INSIDE a word (Volksschule, Verzorgingshuis)."""
    return r'(?<![^\W\d_])(?:\w*?)(?:' + '|'.join(stems) + r')\w*'


EXTRA = {
 'planning_zoning': dict(s=['welstand', 'vergunning', 'bouwmelding', 'wohnungsbau', 'zonificacion', 'zonificación', 'alineacion', 'alineaci', 'pldp', 'plan\\d', 'planungs', 'bygge', 'byggeplan', 'regulering', 'kommunedelplan', 'utbyggnad', 'ubyggnad', 'investeringsbehov', 'bostadsbygg', 'projecten', 'project\\b', 'projects\\b', 'rural area', 'zpp', 'prug', 'actuacions', 'actuaciones', 'dmzones', 'plano', 'ordenanza'],
                         x=['tsu0414yoto']),
 'parcels_landuse': dict(s=['boligfelt', 'fritidsbolig', 'bostad', 'wohn', 'bouwblok', 'fabbricat', 'catast', 'catasto', 'cadastre', 'vellen', 'liander', 'brk\\d', 'celtic fields', 'perceel', 'woning', 'woon', 'verblijf', 'huizen', 'boende', 'tomter', 'eiendom'],
                         x=['tatemonohigai']),
 'boundaries': dict(s=['einzugs', 'ciq\\b', 'zonificaci', 'districte', 'barris', 'censal', 'comarcal', 'catchment', 'sektor', 'wijken', 'dorp', 'village', 'gemeinde', 'municip', 'concello', 'ajuntament', 'ayuntamiento'],
                    x=[]),
 'transport': dict(s=['camerapositie', 'wegvak', 'travaux', 'vinterrute', 'vinterdrift', 'stigning', 'wallring', 'carril', 'cyklo', 'cykel', 'arruamento', 'p-plads', 'p plads', 'parkering', 'parkplatz', 'parkplätze', 'arruament', 'cais\\b', 'dybdekurve', 'verkeer', 'verkehr', 'gc_', 'gnglänk', 'gng\\b', 'sorteerstraten', 'perturbation', 'omleiding', 'wegenwerk', 'wegwerk', 'werken', 'rv_', 'fylkesveg', 'riksveg', 'stikk', 'veinett', 'vegnett', 'rijks', 'oprit', 'afrit', 'rotonde', 'kruispunt', 'fietsstraat', 'fietsparkeer', 'nmbs', 'tgv'],
                x=['kousokutetsudo', 'kousokutetudo']),
 'utilities_infrastructure': dict(s=['kulvert', 'stikkrenne', 'dechet', 'déchet', 'ecopoint', 'eco point', 'recyclage', 'sluikstort', 'zwerfvuil', 'vuil', 'afvalcontainer', 'laadlocatie', 'laadlocaties', 'wi ?fi', 'wifi', 'wifi4eu', 'acque', 'acquedotto', 'ENEL', 'liander', 'enexis', 'stedin', 'kopplingar', 'ukpn', 'secondary sites', 'primary sites', 'infra\\b', 'abwasser', 'regenwasser', 'schacht', 'schächte', 'hausanschluss', 'vattenverk', 'avlopp', 'vatten och avlopp', 'va-', 'vannverk', 'avløp', 'rens'],
                              x=[]),
 'safety_emergency': dict(s=['brand', 'räddnings', 'toets', 'veiligheidsregio', 'schuilplaats', 'noodopvang', 'schutzraum', 'evakuier', 'sammelplatz', 'zivilschutz', 'refugi', 'protecci', 'sinistro', 'evac', 'lachgas', 'overlast', 'meldingen', 'handhaving', 'enforcement', 'wapen', 'combat', 'ww2', 'wwii', 'bomb', 'munition', 'explosive', 'mine action', 'minefield', 'unexploded'],
                          x=[]),
 'environment_hazards': dict(s=['piezómetro', 'piezometro', 'bioclimat', 'ens\\b', 'water\\b', 'waters\\b', 'vattenskydd', 'vatten', 'vergraven', 'bodemkaart', 'zepvn', 'enp\\b', 'faunapassasje', 'fauna', 'flora', 'arbre', 'bois\\b', 'haie', 'zones? naturelles', 'znieff', 'natura', 'særlig', 'sarskilt', 'värdefull', 'miljo', 'miljø', 'milj', 'archeolog', 'archaeolog', 'archeo', 'watertoets', 'pluviaal', 'fluviaal', 'waterrobuust', 'dijk', 'dijkring', 'keringen', 'polder', 'vloed', 'getij', 'kust', 'eau\\b', 'eaux', 'acuifer', 'embalse', 'lago', 'laghi', 'fiume', 'torrente', 'idro', 'hydro', 'wasser', 'bach', 'bäche', 'fluss', 'flüsse', 'see\\b', 'seen\\b', 'teich', 'moor', 'naturdenkmal', 'artenschutz', 'landschaft', 'landskap', 'landscape', 'nature\\b', 'naturreservat', 'fågel', 'vilt', 'jakt', 'skog', 'skogs', 'skov', 'træer', 'trær', 'kyst', 'strand', 'hav\\b', 'sjø', 'vann', 'innsjø', 'elv', 'bekk', 'myr\\b', 'våtmark', 'vådområde', 'ruimtelijk', 'zout', 'bodemverontreiniging', 'grondwater', 'drinkwater'],
                              x=['林班', '小班']),
 'health_social': dict(s=['verzorgingshuis', 'verzorging', 'woonzorg', 'zorg', 'toalett', 'toilette', 'wc\\b', 'sociale', 'sociaal', 'bostadsstatistik', 'kinderopvang', 'seniorenheim', 'altenheim', 'pflege', 'betreuung', 'betreuungseinrichtung', 'dagopvang', 'huisarts', 'ehpad', 'maison de retraite', 'résidence', 'centre social', 'asil', 'ambulatori', 'poliklin', 'praxis', 'arzt', 'ärzte', 'apotheke', 'pharmacy', 'drug', 'vaccin', 'coiffeur'],
                       x=[]),
 'education': dict(s=['volksschul', 'grundschul', 'hauptschul', 'realschul', 'mittelschul', 'berufsschul', 'schule', 'schulen', 'kinderkrippe', 'kindergart', 'universiteit', 'hogeschool', 'basisschool', 'middelbare', 'scholen', 'skolekrets', 'skoleomr', 'barnskola', 'grundskola', 'gymnasium', 'ecoles', 'écoles', 'collèges', 'lycées', 'crèches', 'maternelle', 'primaire', 'collegio', 'scuole', 'asili', 'escuelas', 'colegios', 'instituto', 'guardería', 'creche'],
                  x=[]),
 'culture_recreation': dict(s=['sporthal', 'sporthall', 'speelplek', 'speelplaats', 'play space', 'play area', 'playspace', 'konst', 'clocher', 'parcours', 'ens ouverts', 'celtic', 'archeologisch', 'terrasvlak', 'cultuurhistor', 'molen', 'kasteel', 'kerk', 'kapel', 'kapelle', 'kirche', 'schloss', 'burg\\b', 'monastery', 'abbaye', 'abbey', 'cathedral', 'church', 'chapel', 'mosque', 'synagogue', 'museu', 'museo', 'teatro', 'teatr', 'cinema', 'kino', 'bibliothe', 'zwembad', 'schwimmbad', 'badeplatz', 'badestelle', 'strandbad', 'sportanlage', 'sportplatz', 'sportplads', 'idrætsanlæg', 'idrottsplats', 'fotball', 'football', 'soccer', 'rugby', 'cricket', 'wandel', 'wander', 'vandr', 'turist', 'turstier', 'turløype', 'ski\\b', 'skiløype', 'langrenn', 'klatring', 'climb', 'boulder', 'rodel', 'bad\\b', 'sauna', 'terme', 'picnic', 'pique', 'campeggio', 'camping', 'caravan', 'wohnmobil', 'stellplatz', 'aire de', 'ludique', 'jeux', 'jeu\\b', 'aire de jeux', 'plaine de jeux', 'skatepark', 'pumptrack', 'bmx', 'recr', 'ocio', 'lazer', 'divertiment'],
                          x=['火葬場', '火葬']),
 'economy_business': dict(s=['terras', 'terrasvlak', 'horeca', 'winkel', 'markt', 'wochenmarkt', 'gaststätte', 'gastronom', 'restaurant', 'café', 'bar\\b', 'pub\\b', 'brasserie', 'boulangerie', 'commerce', 'commerçant', 'kommersiell', 'næring', 'næringsliv', 'verksamhet', 'verksamheter', 'ubyggnadsriktning', 'foretag', 'företag', 'arbeitsplatz', 'gewerbegebiet', 'industriegebiet', 'bedrijventerrein', 'zone artisanale', 'zone industrielle', 'zone d.activit', 'polígono', 'poligono industrial', 'agricol', 'landwirt', 'landbouw', 'boerderij', 'vineyard', 'viticol', 'vignoble', 'aoc\\b', 'igp\\b', 'mercados', 'app_mercados', 'mercato', 'mercati', 'tienda', 'comercial', 'turismo', 'alojamiento', 'hotel', 'hostel', 'airbnb', 'overnatting', 'overnattning', 'logi\\b'],
                        x=[]),
 'imagery_basemap': dict(s=['dybdekurve', 'depth contour', 'bathymetr', 'grid\\b', 'cuadricula', ],
                         x=[]),
}
# Third reading (2026-11-16): Finnish/Polish/Czech/German/Swiss-German vocabulary from the
# state and national WFS/ArcGIS beats (hp3b63_euwfs, hp3b64_derest), US census and
# insurance-map layers, and German tree species (a forest-risk atlas).
for _t, _s in {
 'boundaries': ['tilastointi', 'maakunta', 'kunta\\b', 'alueet', 'avi\\b', 'seutukunta', 'census', 'acs\\d*', 'percent', 'households', 'income', 'poverty', 'low income', 'children in', 'persons', 'ethnic', 'languages?', 'speak', 'designated place', 'incorporated place', 'civil', 'powiat', 'wojew', 'gmin', 'okres', 'kraj\\b', 'obvod', 'mestsk', 'neighbou?rhoods?', 'nbhd', 'pôle', 'quartiers', 'cartoquartiers', 'men\\b', 'ménages', 'menages', 'population', 'lau\\b', 'nuts'],
 'environment_hazards': ['tanne', 'erle', 'linde', 'eiche', 'buche', 'fichte', 'kiefer', 'ahorn', 'esche', 'birke', 'douglasie', 'lärche', 'baum', 'bäume', 'geothermie', 'tektonik', 'gestein', 'boden', 'grundwasser', 'firm panel', 'firm\\b', 'moss', 'daisy', 'drill hole', 'borehole', 'tilastoalue', 'natura', 'biosfera', 'biosphere', 'rcp\\d', 'fsk\\d', 'kuestentanne', 'luonto', 'lahti', 'järvi', 'joki', 'meri', 'merenkulku', 'kildesporing', 'fgg', 'fließgewässer', 'fliessgewaesser', 'neiße', 'neisse', 'elbe', 'rhein', 'mosel', 'donau', 'weser', 'lippe', 'ruhr', 'bafg', 'chemischer zustand', 'ecolog', 'zustand'],
 'safety_emergency': ['straz', 'strazy', 'pozar', 'pożar', 'komenda', 'policji', 'hasi', 'požár', 'palo', 'pelastus', 'poliisi', 'injury', 'high injury', 'vision zero', 'kildesporing', 'sinistre'],
 'utilities_infrastructure': ['trafo', 'trafos', 'lagetrafo', 'tower sites?', 'towers?\\b', 'coteq', 'ls verbinding', 'verbinding', 'mittelspannung', 'niederspannung', 'hochspannung', 'netz\\b', 'netzstation', 'umspann', 'fernwärme', 'kraftwerk', 'windrad', 'tanks?\\b', 'vav', 'swift site'],
 'imagery_basemap': ['datum', 'datum conversion', 'tile ?index', 'index_vt', 'topplus', 'topographi', 'dgm', 'dop\\b', 'luftbild', 'webatlas', 'basiskarte', 'mask\\b', 'perspective', 'names\\b', 'landforms?', 'gnis', 'shaded relief', 'relief'],
 'parcels_landuse': ['sales\\b', 'section corners?', 'plss', 'public row', 'right of way', 'row\\b', 'assessor', 'auditor', 'tax ?parcels?', 'vtparcels', 'cadastral', 'neighborhood ?parcels', 'lots?\\b', 'nbhdparcels', 'landforms'],
 'transport': ['public row', 'ada ?stops', 'adastops', 'exit', 'exits', 'autodeel', 'carshar', 'deelauto', 'tog\\b', 'bane\\b', 'jernbane', 'havn', 'lufthavn'],
 'health_social': ['senior', 'seniori', 'sluzby', 'pobyt', 'sociální', 'socialn', 'sote', 'terveys', 'lääk', 'children', 'low income'],
 'planning_zoning': ['lap\\b', 'kcdp', 'cdp\\b', 'variation', 'proposed variation', 'medborgardialog', 'utveckling', 'tiltak', 'prosjekt', 'pldp', 'ldp\\b', 'detailed plan', 'ppu', 'inventari'],
 'culture_recreation': ['upplevelse', 'rekreation', 'rekreationsvärde', 'recreational', 'calluna', 'friluftsliv', 'kulturmiljö', 'kulturmilj', 'tåg\\b', '17mai', 'tog\\b'],
}.items():
    EXTRA[_t]['s'] = EXTRA[_t].get('s', []) + _s

# Fourth reading: the next most frequent unread words of the European beats.
for _t, _s in {
 'boundaries': ['grenz', 'verwaltungsgrenz', 'gemarkung', 'staatsgrenze', 'bundesl', 'opzichtergebied', 'administratieve', 'deelgebied', 'parroquia', 'parr[oó]quia', 'mestsk', 'souhrn'],
 'planning_zoning': ['kp[a-z]', 'kommuneplan', 'arealplan', 'arealform', 'sikring', 'hensyn', 'sone\\b', 'soner\\b', 'restriktion', 'schutzzone', 'maatregel', 'geschiktheid', 'verklaring', 'msp\\b', 'reserved use', 'kompensation'],
 'imagery_basemap': ['ortofoto', 'orthofoto', 'ortho', 'cgr\\d', 'wgs84', 'nir\\b', 'hexagon'],
 'parcels_landuse': ['eigenaar', 'eigenaren', 'brk\\b', 'adresses', 'adresse', 'lieu-dit', 'lieu dit', 'pozemk', 'druhy', 'kataster', 'eigentum', 'immobilien', 'markytor', 'mark\\b', 'jordbruk', 'tehelna'],
 'utilities_infrastructure': ['avfall', 'restavfall', 'stortplaats', 'trinkwasser', 'papelera', 'sluikstort', 'brunnen', 'insamlat', 'debroussaillement'],
 'environment_hazards': ['bos\\b', 'bosvlak', 'bossen', 'zaplav', 'ardida', 'incêndio', 'incendio', 'fgc', 'pmdfci', 'zraniteln', 'hodnotenie', 'explosieven', 'waterkwaliteit', 'bodembelasting', 'debroussaill', 'zoldfelt'],
 'culture_recreation': ['puntos interes', 'puntos_interes', 'turismo', 'turist', 'aktivitet', 'venues?', 'informacion turismo', 'kevershäll'],
 'safety_emergency': ['hitzeschutz', 'ziold'],
}.items():
    EXTRA[_t]['s'] = EXTRA[_t].get('s', []) + _s

for _theme, _d in EXTRA.items():
    # THEME_RULES holds some themes twice (planning_zoning appears before and after
    # the specific ones): the extra terms go in the LAST rule of that theme, which is
    # where the general vocabulary lives.
    _idxs = [i for i, (t, _) in enumerate(THEME_RULES) if t == _theme]
    _i = _idxs[-1]
    _pat = THEME_RULES[_i][1].pattern
    _parts = []
    if _d.get('s'):
        _parts.append(S(*_d['s']))
    if _d.get('x'):
        _parts.append(X(*_d['x']))
    THEME_RULES[_i] = (_theme, re.compile(_pat + '|' + '|'.join(_parts), re.I))

# Rules over a service name only where the layer name is a placeholder.
PLACEHOLDER = re.compile(r'^(layer|sheet|table|feature|data|map|points?|polygons?|lines?|polylines?|gis|download|view|clip|test|copy|new|final|current|master|all|public|open ?data|tmp|temp|untitled|dbo|sde|wgs|etrs|shp|webmercator|web mercator|\d+|[a-z]\d*)( \d+)?$', re.I)

CAT_THEME = {'transport': 'transport', 'safety': 'safety_emergency', 'environment': 'environment_hazards',
             'education': 'education', 'health': 'health_social', 'culture': 'culture_recreation'}

FILE_REGION = {
    'hp3b61_us': 'us', 'hp3b60_us': 'us', 'hp3b58_arcgis2': 'us', 'hp3b58_arcgis3': 'us',
    'hp3b62_eu': 'eu', 'hp3b60_eu': 'eu', 'hp3b64_derest': 'eu', 'hp3b63_euwfs': 'eu',
    'hp3b59_jpgis': 'jp', 'hp3b61_jp': 'jp',
}
JP_MARK = re.compile(r'[぀-ヿ一-鿿]|\b(City|Town|Village|Ward|Prefecture|Prefectural|Japan|MLIT|Tokyo|Saitama|Hokkaido|Fukuoka|Nagano|Gunma|Toyama|Tochigi|Hyogo|Miyagi|Yamagata|Akita|Mie|Sado|Okaya|Saku|Shibuya|Setagaya|Hino|Hachioji|Kitakyushu|Kosai|Gyoda|Tsukuba|Itami|Urayasu|Ryugasaki|Muroran|Annaka|Omihachiman|Oiso|Kashiwazaki|Hamamatsu|Inzai|Aizuwakamatsu|Nagaizumi|Hidaka|River Foundation)\b', re.I)
US_MARK = re.compile(r'\b(City of [A-Z]|County|State of|Department of Transportation|DOT\b|NOAA|FAA|NIFC|NC OneMap|NC |Texas|Pennsylvania|California|Georgia|Florida|Massachusetts|Iowa|Idaho|Oregon|Oklahoma|Arizona|Kentucky|Illinois|Washington|Virginia|Ohio|Michigan|Colorado|Nevada|Utah|Montana|Wyoming|Minnesota|Wisconsin|Indiana|Missouri|Carolina|Maryland|Jersey|New York|NYS|DEC|EPA|USGS|U\.S\.|District of Columbia|Baton Rouge|Miami|Chicago|Seattle|Raleigh|Lexington|Houston|Kirkland|Corona|Rochester|New Orleans|OKI|Montgomery|Mid-Region|Great Basin|Railroad Commission|Governor|Emergency Management|carto\.nationalmap|nationalmap)\b')
EU_MARK = re.compile(r'\b(Reykjavik|Durham|Hub.?Eau|Rotterdam|Dunbartonshire|HELCOM|Ireland|Cabildo|La Palma|Council|Gemeente|Stad|Kommune|Region|Métropole|Metropole|Communauté|Waterschap|eea\.europa|discomap)\b', re.I)


def region_of(table, name, tags):
    if table in FILE_REGION:
        return FILE_REGION[table]
    if table == 'hp3b58_arcgis':
        return 'eu' if '\\"eu\\"' in tags or '"eu"' in tags else 'us'
    # the two mixed, un-tagged beats: the publisher's name decides, else `world`
    pub = name.split(' — ', 1)[0]
    if JP_MARK.search(pub) or JP_MARK.search(name):
        return 'jp'
    if US_MARK.search(pub):
        return 'us'
    if EU_MARK.search(pub):
        return 'eu'
    return 'world'


def norm(s):
    s = re.sub(r'\(ArcGIS[^)]*\)', ' ', s)
    s = re.sub(r'([a-z0-9])([A-Z])', r'\1 \2', s)      # camelCase
    s = re.sub(r'[_\-./()\[\]:,;|&+]+', ' ', s)
    return re.sub(r'\s+', ' ', s).strip().lower()


def service_of(url):
    m = re.search(r'/services/([^/?]+(?:/[^/?]+)*?)/(?:FeatureServer|MapServer)', url)
    if not m:
        m = re.search(r'/rest/services/(.+?)/(?:FeatureServer|MapServer)', url)
    return urllib.parse.unquote(m.group(1)) if m else ''


# Education after transport: a "School Bus Route" is a route, a "Schools" layer is
# schools. (Written as a move so the vocabulary lists above stay in reading order.)
_edu = [r for r in THEME_RULES if r[0] == 'education']
THEME_RULES = [r for r in THEME_RULES if r[0] != 'education']
_ti = max(i for i, r in enumerate(THEME_RULES) if r[0] == 'transport')
THEME_RULES[_ti + 1:_ti + 1] = _edu

# Themes the SERVICE name may decide when the layer's own name says nothing. A
# service called "Community" or "Planning_AGOL" holds every kind of layer, so the
# general themes are not read from it -- only the ones a service name names on
# purpose (a "Hydrants" or "Flood_Hazard" service).
SERVICE_THEMES = {'utilities_infrastructure', 'safety_emergency', 'environment_hazards', 'health_social',
                  'education', 'transport', 'culture_recreation', 'elections_admin'}


def theme_of(layer, service, cat):
    n = norm(layer)
    for theme, rx in THEME_RULES:
        if rx.search(n):
            return theme, 'layer'
    s = norm(service)
    if s:
        for theme, rx in THEME_RULES:
            if theme in SERVICE_THEMES and rx.search(s):
                return theme, 'service'
    if cat in CAT_THEME:
        return CAT_THEME[cat], 'category'
    return 'other', 'none'


# ------------------------------------------------------------------ body-type beats
# hp3b48_igo: the description records Wikidata's class ("Wikidata classes it as
# think-tank."), and 181 of the 495 rows carry the class `think-tank` for what
# are game studios, publishers and company blogs.
IGO_COMPANY = R(r'\b(studios?|games?|gaming|interactive|entertainment|software|soft\b|inc\.?|ltd\.?|llc|gmbh|s\.?r\.?l\.?|corp\.?|company|co\.|blog|developer\w*|publish\w*|productions?|pictures|press\b|records|music|toys?|figures?|anime|manga|rpg|fun\b|labs?|works|design|magic|pixel|digital|tech|apps?|shop|store|racing|sports? ?club|agency|marketing|advertising)\b',
                r'ゲーム|株式会社|スタジオ|ソフト')
IGO_IGO = R(r'\b(united nations|nations|intergovernmental|inter-?american|commission|secretariat|treaty|tribunal|international (court|criminal|seabed|monetary|labour|labor|maritime|energy|atomic|civil aviation|telecommunication|renewable|bank|organi[sz]ation for migration|criminal police)|world (bank|trade|health|food|meteorological|intellectual|customs)|organi[sz]ation of|organi[sz]ation for|organisation (of|for|internationale)|organizaci[oó]n|organisation internationale|bank for|development bank|european (union|commission|parliament|council|central bank|space|southern|patent|environment|investment|bank)|african union|asean|apec|oecd|nato|opec|mercosur|caricom|ecowas|sadc|comesa|g20|g7|\bimf\b|\bwto\b|\bwho\b|\bfao\b|\bilo\b|\bimo\b|\biaea\b|\bicc\b|\bunesco\b|\bunicef\b|\bunhcr\b|\bundp\b|\bunido\b|\bunep\b|\bitu\b|\bwipo\b|\bifad\b|control organi[sz]ation|permanent interstate|committee for|cooperation (council|treaty)|cooperation organi[sz]ation|community of|states? parties|conference of|secretariat|authority\b)')
IGO_ASSOC = R(r'\b(association|associations|federation|federations|alliance|network|coalition|forum|council|union|society|societies|institute|foundation|chamber|committee|confederation|congress|league|centre|center|club|organi[sz]ation|organisation|initiative|platform|partnership|consortium|summit|panel|observatory|academy|trust|fund|movement|campaign|international\b|world\b|global\b|european\b|african\b|asian\b|pan-?\w+|youth|workers|skeptic\w*)\b')


def igo_class(row):
    d = re.sub(r'\s+', ' ', row['desc'])
    m = re.search(r'classes it as ([^.]+?)\.', d)
    return m.group(1) if m else ''


def classify_igo(row):
    n = re.sub(r'^Q\d+ - ', '', row['name'])
    wc = igo_class(row)
    if wc == 'intergovernmental-organization' or (wc != 'think-tank' and IGO_IGO.search(n) and not IGO_COMPANY.search(n)):
        return 'igo_intergovernmental', row['cat']
    if IGO_IGO.search(n) and not IGO_COMPANY.search(n) and wc != 'think-tank':
        return 'igo_intergovernmental', row['cat']
    if IGO_COMPANY.search(n) and not IGO_ASSOC.search(n):
        return 'igo_companies_blogs', 'commercial'
    if wc == 'international-organization' or IGO_ASSOC.search(n):
        return 'igo_associations', row['cat']
    # a "think tank" by Wikidata that no rule can type by its name: the honest residue
    return 'igo_other', row['cat']


US_TYPE = {  # the .gov registry's own type, written into every name: "City of X (City, Absecon, NJ) - ..."
    'City': 'usgov_cities', 'County': 'usgov_counties', 'Special district': 'usgov_special_districts',
    'School district': 'usgov_special_districts', 'State or territory': 'usgov_states',
    'Interstate': 'usgov_states', 'Tribal': 'usgov_tribes',
    'Federal - Executive': 'usgov_federal', 'Federal - Legislative': 'usgov_federal', 'Federal - Judicial': 'usgov_federal',
}


def classify_usgov(row):
    m = re.search(r'\(([^,)]*),', row['name'])
    typ = m.group(1).strip() if m else ''
    if typ.endswith(' - Election'):
        return 'usgov_elections', row['cat']
    return US_TYPE.get(typ, 'usgov_other'), row['cat']


WD_PAT = [  # collector, regex over "<name> <site title>"; first match wins
 ('wdgov_universities_research', R(r'\b(universit\w+|college|polytechnic|research|institute|institut\b|academy|academia|observator\w+|laborator\w+|science|scientific|survey\b|museum|library|archives?|statistic\w+|census|knowledge|innovation|technology council)\b', r'大学|研究|試験|学院')),
 ('wdgov_police_security', R(r'\b(police|polic[ií]a|politie|gendarm\w*|security|defen[cs]e|army|armed forces|military|navy|air force|fire\b|fire (service|brigade|department)|rescue|civil protection|intelligence|prison|correctional|custody|border|immigration|customs)\b')),
 ('wdgov_health_education', R(r'\b(health|hospital|medical|medicine|pharma\w*|education|school\w*|teacher\w*|training|sports?|youth|culture|cultural|tourism|heritage|welfare|social|labou?r|employment|family|women|children)\b')),
 ('wdgov_economy_infrastructure', R(r'\b(agricultur\w+|fisher\w+|forest\w*|environment\w*|water|energy|electric\w*|power|transport\w*|road\w*|railway|aviation|airport|port\b|maritime|mining|mines|industry|industrial|trade|commerce|investment|housing|construction|building|telecommunications?|communications?|post\b|postal|revenue|tax\w*|finance|treasury|budget|planning|development)\b')),
]


def classify_wdgov(row):
    m = re.search(r'\(([^,)]*),\s*\w\w\)', row['name'])
    typ = m.group(1).strip() if m else ''
    text = re.sub(r'^Q\d+ ', '', row['name'])
    if typ == 'ministry':
        return 'wdgov_ministries', row['cat']
    if typ in ('legislature', 'court'):
        return 'wdgov_legislatures_courts', row['cat']
    if typ in ('central-bank', 'regulatory-agency'):
        return 'wdgov_regulators_central_banks', row['cat']
    if typ == 'police':
        return 'wdgov_police_security', row['cat']
    for key, rx in WD_PAT:
        if rx.search(text):
            return key, row['cat']
    if re.search(r'\b(ministr\w+|minist[eè]re|ministerio|ministero|department of|secretar\w+|cabinet|presidency)\b', text, re.I):
        return 'wdgov_ministries', row['cat']
    if re.search(r'\b(regulat\w+|authority|commission|ombuds\w*|audit\w*|comptroller|inspector\w*|competition|supervis\w+|central bank|bank of)\b', text, re.I):
        return 'wdgov_regulators_central_banks', row['cat']
    if re.search(r'\b(municipal\w*|city|county|province|provincial|state of|prefectur\w+|regional|region|council|district|governorate|commune|town|village|local)\b', text, re.I):
        return 'wdgov_local_regional', row['cat']
    return 'wdgov_agencies', row['cat']


# --- the scheduled `government` collector: macro-form rows (VRSS/VJSON/RSSX ...)
# whose COLL argument is the literal "government". Region from the id prefix,
# type from the body's name; a row no rule types keeps an honest `_other`.
UK_RULES = [
 ('uk_gov_justice_police', R(r'\b(police|policing|constabulary|fire\b|fire (and|&) rescue|court|courts|tribunal|prison|prisons|probation|crime|justice|judicial|sentencing|prosecut\w+|serious fraud|law commission|legal aid|coastguard|border force|immigration)\b')),
 ('uk_gov_health_social', R(r'\b(nhs|health|hospital|medicines|care quality|social care|screening|medical|public health|blood|nursing|midwifery|disabilit\w+|pensions?|benefits?|wellbeing)\b')),
 ('uk_gov_inquiries_reviews', R(r'\b(inquir\w+|review|reviews|task ?force|education action zone|royal commission|commissioner|commissioners|independent (panel|commission)|taskforce|working group|investigation|public inquiry|report)\b')),
 ('uk_gov_local', R(r'\b(council|borough|city|county|district|metropolitan|parish|town|combined authority|london|shire|combined|unitary|mayor|greater)\b', r'-gov-uk-', r'-council')),
 ('uk_gov_departments', R(r'^(department|ministry|cabinet|office of the|hm\b|her majesty|prime minister)|\b(department for|department of|ministry of|cabinet office|treasury|home office|foreign|commonwealth|secretary of state)\b')),
 ('uk_gov_regulators', R(r'\b(regulat\w+|ofcom|ofgem|ofwat|ofsted|ofqual|authority|commission|ombudsman|inspectorate|standards|watchdog|competition|oversight|supervis\w+|financial conduct|information commissioner|audit|comptroller)\b')),
 ('uk_gov_agencies_bodies', R(r'\b(board|council|committee|advisory|panel|agency|service|office|institute|trust|foundation|body|natural|environment|heritage|historic|museum|gallery|library|archives|arts|sport|bureau|centre|center|unit|group|forum|network|company|corporation|laboratory|survey|academy|college|university)\b')),
]


def classify_govrow(sid, name):
    if sid.startswith('jpx-city'):
        return 'jp_gov_cities'
    if sid.startswith('jpx-pref'):
        return 'jp_gov_prefectures'
    if sid.startswith('jpx-tokyo'):
        return 'jp_gov_tokyo'
    if sid.startswith('cn-b5'):
        if re.search(r'\.gov\.mo|澳門|澳门|\bMacao|\bMacau', name + ' ' + sid):
            return 'cn_gov_macao'
        if re.search(r'-gov-hk|-hk-|香港|\bHong Kong|\bHK\b|hongkong', name + ' ' + sid, re.I):
            return 'cn_gov_hongkong'
        return 'cn_gov_other'
    if sid.startswith('intl-b50'):
        return 'world_outlets'             # France 24 / SCMP: news outlets filed as government
    if sid.startswith('gov-opensanctions'):
        return 'sanctions'
    if sid.startswith('gov-openfda'):
        return 'us_health'
    if sid.startswith(('uk-', 'gov-govuk')):
        for key, rx in UK_RULES:
            if rx.search(name) or rx.search(sid):
                return key
        return 'uk_gov_other'
    if sid.startswith('us-'):
        return 'us_gov_agencies'
    if re.match(r'gov-(eu|europarl|eucommission|ecb|eurostat|eea|eib|edps)', sid):
        return 'world_gov_eu_bodies'
    if re.search(r'\b(bank|nations|united nations|world|international|imf|oecd|african|asian|inter-?american|development)\b', name, re.I):
        return 'world_gov_intergovernmental'
    if re.search(r'\b(open ?data|data portal|catalog\w*|ckan|api\b|datos|données|dati|portal)\b', name, re.I):
        return 'world_gov_data_portals'
    if re.search(r'\b(statistic\w*|census|stat\b)\b', name, re.I):
        return 'world_gov_statistics'
    return 'world_gov_agencies'


def rows_full(path):
    txt = open(path, encoding='utf-8').read()
    body = txt[txt.index('[] = {'):]
    for r in re.split(r'\n\s*\{\s*(?=\.id\s*=)', body)[1:]:
        def fld(n):
            m = re.search(r'\.' + n + r'\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+)', r)
            return ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))).replace('\\"', '"') if m else ''
        yield dict(id=fld('id'), name=fld('name'), cat=fld('category'), tags=fld('tags'),
                   url=fld('url'), portal=fld('portal'), collector=fld('collector'), desc=fld('description'))


def classify(table, row):
    if table == 'hp3b48_igo':
        return classify_igo(row)
    if table == 'hp3b44_usgov':
        return classify_usgov(row)
    if table == 'hp3b47_wdgov':
        return classify_wdgov(row)
    # ArcGIS / WFS layer beats
    reg = region_of(table, row['name'], row['tags'])
    layer = row['name'].split(' \u2014 ', 1)[-1]
    theme, how = theme_of(layer, service_of(row['url']), row['cat'])
    return f'{reg}_gis_{theme}', row['cat']


# --- macro-form files (VRSS / VJSON / RSSX ...): the collector is the literal COLL argument
MACRO_CALL = re.compile(r'^(\w+)\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+),', re.M)
COLL_LINE = re.compile(r'\n(\s+)"government"(, ")')


def macro_rows(path, only=None):
    t = open(path, encoding='utf-8', errors='replace').read()
    for m in MACRO_CALL.finditer(t):
        sid = m.group(3)
        if only is not None and sid not in only:
            continue
        mm = COLL_LINE.search(t, m.end(), m.end() + 400)
        if not mm:
            continue
        nxt = re.compile(r'\n\w+\(').search(t, m.end())
        if nxt and nxt.start() < mm.start():
            continue
        name = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(4)))
        yield sid, name, mm.start(1) - 0, mm


def cmd_gov_plan(files):
    tally = collections.Counter()
    for f in files:
        table = os.path.basename(f)[:-2]
        for sid, name, _, _ in macro_rows(f):
            key = classify_govrow(sid, name)
            tally[(table, key)] += 1
            print(f"{sid}\t{table}\t{key}\tgovernment")
    for (t, k), n in sorted(tally.items()):
        print(f'{t:28s} {k:32s} {n:6d}', file=sys.stderr)


def cmd_gov_apply(plan, files):
    for f in files:
        txt = open(f, encoding='utf-8', errors='replace').read()
        out = []; last = 0; n = 0
        for sid, name, _, mm in macro_rows(f, set(plan)):
            key = plan[sid][1]
            s = mm.start() + len(mm.group(0)) - len(mm.group(2)) - len('"government"')
            e = s + len('"government"')
            if txt[s:e] != '"government"':
                raise SystemExit(f'{f}: {sid}: COLL literal not where expected')
            out.append(txt[last:s]); out.append(f'"{key}"'); last = e; n += 1
        out.append(txt[last:])
        open(f, 'w', encoding='utf-8').write(''.join(out))
        print(f'{f}: {n} rows edited', file=sys.stderr)


def cmd_plan(files):
    tally = collections.Counter()
    for f in files:
        table = os.path.basename(f)[:-2]
        for row in rows_full(f):
            if row['collector']:
                print(f"{row['id']}\t{table}\t-\t{row['cat']}")     # already explicit: left alone
                tally[(table, '(already explicit: ' + row['collector'] + ')')] += 1
                continue
            key, cat = classify(table, row)
            tally[(table, key)] += 1
            print(f"{row['id']}\t{table}\t{key}\t{cat}")
    for (t, k), n in sorted(tally.items()):
        print(f'{t:22s} {k:36s} {n:6d}', file=sys.stderr)


def read_plan(path):
    plan = {}
    for l in open(path):
        sid, table, key, cat = l.rstrip('\n').split('\t')
        plan[sid] = (table, key, cat)
    return plan


def cmd_apply(plan, files):
    for f in files:
        txt = open(f, encoding='utf-8').read()
        head, body = txt.split('[] = {', 1)
        parts = re.split(r'(\n\s*\{\s*(?=\.id\s*=))', body)
        out = [parts[0]]; n = 0; miss = []
        for i in range(1, len(parts), 2):
            sep, row = parts[i], parts[i + 1]
            sid = re.match(r'\.id\s*=\s*"([^"]+)"', row).group(1)
            if sid not in plan:
                miss.append(sid); out.append(sep + row); continue
            _, key, cat = plan[sid]
            if key == '-':
                out.append(sep + row); continue
            if re.search(r'\.collector\s*=', row):
                raise SystemExit(f'{f}: {sid} already names a collector')
            row2, k = re.subn(r'\.category = "[^"]*",', f'.category = "{cat}", .collector = "{key}",', row, count=1)
            if k != 1:
                raise SystemExit(f'{f}: {sid}: category line not found')
            n += 1; out.append(sep + row2)
        if miss:
            raise SystemExit(f'{f}: {len(miss)} rows not in plan: {miss[:3]}')
        open(f, 'w', encoding='utf-8').write(head + '[] = {' + ''.join(out))
        print(f'{f}: {n} rows edited', file=sys.stderr)


def cmd_manifest(plan, files):
    for f in files:
        out = []; n = 0; miss = 0
        for line in open(f, encoding='utf-8'):
            if line.startswith('#') or '|' not in line:
                out.append(line); continue
            fld = line.rstrip('\n').split('|')
            if fld[0] not in plan or plan[fld[0]][1] == '-':
                miss += 1; out.append(line); continue
            _, key, cat = plan[fld[0]]
            while len(fld) < 13:
                fld.append('')
            fld[3] = cat
            fld[12] = (fld[12] + ';' if fld[12] else '') + f'collector={key}'
            out.append('|'.join(fld) + '\n'); n += 1
        open(f, 'w', encoding='utf-8').write(''.join(out))
        print(f'{f}: {n} rows mirrored, {miss} rows not in the tree (left as they are)', file=sys.stderr)


def cmd_summary(plan):
    per = collections.Counter(); reg = collections.Counter(); oth = collections.Counter()
    for sid, (t, k, c) in plan.items():
        per[k] += 1
        m = re.match(r'(us|eu|jp|world)_gis_(.*)$', k)
        if m:
            reg[m.group(1)] += 1
            if m.group(2) == 'other':
                oth[m.group(1)] += 1
    for k, n in sorted(per.items()):
        print(f'{k:40s} {n:6d}')
    for r in sorted(reg):
        print(f'region {r}: {reg[r]} rows, other {oth[r]} ({100*oth[r]/reg[r]:.1f}%)')


if __name__ == '__main__':
    if len(sys.argv) < 3 or sys.argv[1] not in ('plan', 'apply', 'manifest', 'summary', 'gov-plan', 'gov-apply'):
        raise SystemExit(__doc__)
    if sys.argv[1] == 'plan':
        cmd_plan(sys.argv[2:])
    elif sys.argv[1] == 'gov-plan':
        cmd_gov_plan(sys.argv[2:])
    elif sys.argv[1] == 'gov-apply':
        cmd_gov_apply(read_plan(sys.argv[2]), sys.argv[3:])
    elif sys.argv[1] == 'summary':
        cmd_summary(read_plan(sys.argv[2]))
    elif sys.argv[1] == 'apply':
        cmd_apply(read_plan(sys.argv[2]), sys.argv[3:])
    else:
        cmd_manifest(read_plan(sys.argv[2]), sys.argv[3:])
