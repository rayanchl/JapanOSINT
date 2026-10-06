#!/usr/bin/env python3
"""Scratch generator for docs/candidate-sources-batch38.jpgeo3.txt.

Every number in a description below was observed by a fetch made on
2026-10-02 (see the scratch scripts arcfull.py, arccheck.py, dsfull.py,
uniq.py in this directory). This script only formats the rows.
"""
import math, os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'candidate-sources-batch38.jpgeo3.txt')

rows = []

def arc(id_, cat, rtype, tags, name, name_ja, layer, oid, count, title, desc, ps=1000,
        latlon=True, date=None, interval=604800, portal=None):
    url = (f"{layer}/query?where=1%3D1&outFields=*&outSR=4326&f=json&orderByFields={oid}"
           f"&resultRecordCount={ps}&resultOffset=0")
    pm = math.ceil(count / ps) + 2
    opts = [f"array_path=features", f"id_keys=attributes.{oid}", f"title_keys={title}"]
    if date:
        opts.append(f"date_keys={date}")
    if latlon:
        opts += ["lat_key=geometry.y", "lon_key=geometry.x"]
    opts += ["page_param=resultOffset", f"page_size={ps}", "page_zero_based=1", f"page_max={pm}",
             "timeout_ms=60000", f"interval={interval}"]
    rows.append([id_, 'json', 'any', cat, rtype, tags, portal or layer, name, name_ja, url, url,
                 desc, ';'.join(opts)])

def ckan(id_, cat, rtype, tags, portal, name, name_ja, host, rid, total, idk, title, desc,
         date=None, interval=604800, extra=None):
    url = f"https://{host}/api/3/action/datastore_search?resource_id={rid}&limit=1000&offset=0&sort=_id"
    pm = math.ceil(total / 1000) + 2
    opts = ["array_path=result.records", f"id_keys={idk}", f"title_keys={title}"]
    if date:
        opts.append(f"date_keys={date}")
    if extra:
        opts += extra
    opts += ["page_param=offset", "page_size=1000", "page_zero_based=1", f"page_max={pm}",
             "timeout_ms=60000", f"interval={interval}"]
    rows.append([id_, 'json', 'any', cat, rtype, tags, portal, name, name_ja, url, url, desc,
                 ';'.join(opts)])

def csvrow(id_, cat, rtype, tags, portal, name, name_ja, url, idk, title, desc, date=None,
           extra=None, interval=604800, why=''):
    opts = [f"id_keys={idk}", f"title_keys={title}"]
    if date:
        opts.append(f"date_keys={date}")
    if extra:
        opts += extra
    opts += [f"pagination_ok={why}", "timeout_ms=120000", f"interval={interval}"]
    rows.append([id_, 'csv', 'any', cat, rtype, tags, portal, name, name_ja, url, url, desc,
                 ';'.join(opts)])

# ---------------------------------------------------------------- MLIT DPF (8)
DPF = 'https://services8.arcgis.com/7WU6MyYwq0ewt100/arcgis/rest/services'
arc('JO38_JPGEO_MLIT_CORINS_CONTRACTS', 'government', 'public-works-contract',
    'japan,mlit,public-works,construction,procurement,contract',
    'MLIT Data Platform - CORINS public works contract registrations',
    '国土交通DPF 工事実績情報（コリンズ）',
    f'{DPF}/crtc_corins/FeatureServer/0', 'ObjectId', 11565, 'attributes.DPF_title',
    'Public works contracts registered in JACIC CORINS as published by the 国土交通DPF organisation: one point per '
    'contract with project title, ordering authority (477 distinct, led by NEXCO West Kansai with 283 and NEXCO East '
    'Kanto with 276), road or river route, free-text construction outline, contract start and end dates (starts from '
    '2014-04-04 to 2026-12-01), registration type (5,806 completion, 3,680 change, 2,079 award) and CORINS registration '
    'number. Fetched 2026-10-02: 11,565 features by returnCountOnly (maxRecordCount 2000); a full walk at 1000 per page '
    'returned 11,565 records with 11,565 distinct ObjectId and 11,565 distinct CRTC_reg_number; layer last edited 2026-09-22.',
    date='attributes.CRTC_contract_period_start_date', interval=86400)
arc('JO38_JPGEO_MLIT_MYCITY_CONSTRUCTION', 'government', 'public-works-contract',
    'japan,mlit,public-works,construction,procurement,municipal,contractor',
    'MLIT Data Platform - My City Construction local-government works and contractors',
    '国土交通DPF 地方公共団体の工事データ（My City Construction）',
    f'{DPF}/mcc/FeatureServer/0', 'ObjectId', 13554, 'attributes.MCC_construction_name',
    'Works and consultancy contracts of prefectures and municipalities from the My City Construction service, served '
    'by the 国土交通DPF organisation: construction name, client body (209 distinct; Yamagata 5,938 and Shizuoka 4,671 '
    'records lead 19 prefectures), contractor organisation (2,359 distinct firms), work description, start and end '
    'dates and a link to the mycityconstruction.jp product page. Fetched 2026-10-02: 13,554 features (maxRecordCount '
    '2000); a full walk returned 13,554 records with 13,554 distinct ObjectId and 13,554 distinct MCC_ID; layer last '
    'edited 2026-02-13.',
    date='attributes.MCC_construction_end_date')
arc('JO38_JPGEO_MLIT_RIVER_SLUICE_GATES', 'infrastructure', 'river-sluice-gate',
    'japan,mlit,river,flood-control,infrastructure,sluice',
    'MLIT Data Platform - river sluice gates and culverts (樋門・樋管)',
    '国土交通DPF 河川 樋門・樋管',
    f'{DPF}/ipf_river_sluice_gate/FeatureServer/0', 'ObjectId', 8102, 'attributes.DPF_title',
    'Sluice gates and drainage culverts on nationally managed rivers from MLIT infrastructure data: facility name, river '
    'system and river, managing body down to the river office (6,663 MLIT regional bureaus, 1,439 Hokkaido Development '
    'Bureau), prefecture (46 distinct), year and coordinates. Fetched 2026-10-02: 8,102 features (maxRecordCount 2000); '
    'a full walk returned 8,102 records with 8,102 distinct ObjectId and 8,102 distinct DPF_id; layer last edited 2026-03-06.')
arc('JO38_JPGEO_MLIT_SABO_DAMS', 'infrastructure', 'sabo-dam',
    'japan,mlit,sabo,erosion-control,landslide,infrastructure',
    'MLIT Data Platform - sabo (erosion-control) dams on national rivers',
    '国土交通DPF 河川 砂防施設（堰堤）',
    f'{DPF}/ipf_river_sand_barrier/FeatureServer/0', 'ObjectId', 6201, 'attributes.DPF_title',
    'Erosion-control dams (砂防堰堤) under MLIT management: facility name, river system and river, administering '
    'regional bureau and office (6,062 MLIT, 139 Hokkaido Development Bureau), prefecture (30 distinct), address, '
    'completion year, crest height and length, contractor and coordinates. Fetched 2026-10-02: 6,201 features (maxRecordCount 2000); a full walk returned '
    '6,201 records with 6,201 distinct ObjectId and 6,201 distinct DPF_id; layer last edited 2026-03-06.')
arc('JO38_JPGEO_MLIT_RSDB_SHEDS', 'infrastructure', 'road-shed',
    'japan,mlit,road,rockfall,avalanche,inspection,infrastructure',
    'MLIT Data Platform - road sheds (rock and snow sheds) from the road inspection database',
    '国土交通DPF シェッド（全国道路施設点検データベース）',
    f'{DPF}/rsdb_shed/FeatureServer/0', 'ObjectId', 3669, 'attributes.DPF_title',
    'Rock-fall and avalanche sheds (洞門・ロックシェッド) from the national road facility inspection database: name and '
    'kana, route, managing body (163 distinct; Niigata 403, Toyama 350, Chubu Regional Bureau 215), office, length, '
    'width, completion year, latest inspection year, soundness judgement and repair status. Fetched 2026-10-02: 3,669 features (maxRecordCount 2000); a full '
    'walk returned 3,669 records with 3,669 distinct ObjectId; layer last edited 2026-09-07. Complements the bridge, '
    'tunnel and culvert RSDB layers already registered.')
arc('JO38_JPGEO_MLIT_LIGHTHOUSES', 'infrastructure', 'lighthouse',
    'japan,maritime,coast-guard,navigation,lighthouse,msil',
    'MLIT Data Platform - lighthouses and navigation lights (MSIL, Japan Coast Guard)',
    '国土交通DPF 灯台（海洋状況表示システム 海しる）',
    f'{DPF}/msil_lighthouse/FeatureServer/0', 'ObjectId', 3207, 'attributes.DPF_title',
    'Every lighthouse and harbour light in the MSIL (海しる) dataset sourced from the Japan Coast Guard, R7 (2025) '
    'edition: name, romanised name, navigation-aid number (MSIL_water_course_sign_number) and coordinates. Fetched 2026-10-02: 3,207 '
    'features (maxRecordCount 2000); a full walk returned 3,207 records with 3,207 distinct ObjectId and 3,207 distinct '
    'titles; layer last edited 2026-01-24.')
arc('JO38_JPGEO_MLIT_FISHING_PORTS', 'infrastructure', 'fishing-port',
    'japan,maritime,fishing-port,fisheries,msil',
    'MLIT Data Platform - fishing ports (MSIL)',
    '国土交通DPF 漁港（海洋状況表示システム 海しる）',
    f'{DPF}/msil_fishing_port/FeatureServer/0', 'ObjectId', 3374, 'attributes.DPF_title',
    'Fishing ports from the MSIL (海しる) dataset: port name and reading, location, administrator municipality, data '
    'source, Coast Guard region, harbour-regulation field and fishing-port category, data years H22 (2010, 1,725 ports) to H28 (2016). Fetched '
    '2026-10-02: 3,374 features (maxRecordCount 2000); a full walk returned 3,374 records with 3,374 distinct ObjectId '
    '(port names repeat: 2,784 distinct); layer last edited 2026-01-24.')
arc('JO38_JPGEO_MLIT_DAM_HANDBOOK', 'infrastructure', 'dam',
    'japan,dam,reservoir,water,infrastructure',
    'MLIT Data Platform - Japan Dam Handbook dam register (ダム便覧)',
    '国土交通DPF ダム便覧',
    f'{DPF}/dhb/FeatureServer/0', 'ObjectId', 2476, 'attributes.DPF_title',
    'Dams from the ダム便覧 dam handbook: dam number, name and reading, water system and river, left-bank prefecture and '
    'location, operator, builder, dam type, purpose code, height (2.75 m to 186 m), crest length, volume, catchment, '
    'total and effective storage, submerged area, construction start and completion and status '
    '(2,417 in operation, 50 under construction, 9 in planning survey). Fetched 2026-10-02: 2,476 features '
    '(maxRecordCount 2000); a full walk returned 2,476 records with 2,476 distinct ObjectId; layer last edited 2026-02-23.')

# ---------------------------------------------------------------- NIES (6)
NIES = 'https://services9.arcgis.com/wlu6GWBNU4aPwEwv/arcgis/rest/services'
def nies(id_, svc, lyr, count, gas_en, gas_ja, years, edited, perpg):
    arc(id_, 'environment', 'air-quality-annual-mean',
        f'japan,nies,air-quality,pollution,{gas_en.lower().replace(".", "")},monitoring-station',
        f'NIES - annual {gas_en} at every Japanese air-monitoring station',
        f'国立環境研究所 日本の大気環境 {gas_ja} 年平均値',
        f'{NIES}/{svc}/FeatureServer/{lyr}', 'ESRI_OID', count,
        'attributes.measure_point_name+attributes.measure_year',
        f'National Institute for Environmental Studies layer behind 日本の大気環境 Light: one record per monitoring station '
        f'per year with station code and name, station class (in the NO2 layer 60,180 general and 15,830 roadside-exhaust '
        f'station-years), prefecture, address, {gas_ja} annual mean value and unit, and an environmental-standard status code. Fetched 2026-10-02: {count:,} features '
        f'covering {years} (maxRecordCount 2000); a full walk returned {count:,} records with {count:,} distinct ESRI_OID '
        f'and {count:,} distinct station-code+year pairs ({perpg}); layer last edited {edited}.',
        date='attributes.measure_year')
nies('JO38_JPGEO_NIES_AIR_NO2', 'no2_allyear', 10, 76077, 'NO2', '二酸化窒素', '1981 to 2024', '2026-09-08', '1,518 stations reported for 2024')
nies('JO38_JPGEO_NIES_AIR_SPM', 'spm_allyear', 15, 68854, 'SPM', '浮遊粒子状物質', '1981 to 2024', '2026-09-07', '44 measurement years')
nies('JO38_JPGEO_NIES_AIR_SO2', 'so2_allyear', 5, 62026, 'SO2', '二酸化硫黄', '1981 to 2024', '2026-09-07', '44 measurement years')
nies('JO38_JPGEO_NIES_AIR_OX', 'ox_allyear', 13, 46548, 'Ox', '光化学オキシダント', '1985 to 2024', '2026-09-25', '40 measurement years')
nies('JO38_JPGEO_NIES_AIR_PM25', 'pm25_allyear', 16, 13720, 'PM2.5', '微小粒子状物質', '2010 to 2024', '2026-09-14', '15 measurement years')
nies('JO38_JPGEO_NIES_AIR_NMHC', 'nmhc_allyear', 14, 21734, 'NMHC', '非メタン炭化水素', '1981 to 2024', '2026-09-07', '44 measurement years')
nies('JO38_JPGEO_NIES_AIR_CO', 'co_allyear', 12, 18306, 'CO', '一酸化炭素', '1981 to 2024', '2026-09-07', '44 measurement years')
arc('JO38_JPGEO_NIES_DIOXIN_2023', 'environment', 'dioxin-measurement',
    'japan,nies,dioxin,pollution,monitoring',
    'NIES - dioxin monitoring results by measurement point (FY2023)',
    '国立環境研究所 ダイオキシン類 測定結果（地点別）',
    f'{NIES}/dxn_lg_gdb/FeatureServer/0', 'OBJECTID', 4158,
    'attributes.measure_point_name+attributes.jis_code_name',
    'Dioxin survey results for FY2023 from the National Institute for Environmental Studies: measurement point code and '
    'name, two medium codes, municipality code and name, prefecture, address, mean value and a criterion code. Fetched 2026-10-02: 4,158 features, all measure_year 2023 '
    '(maxRecordCount 2000); page 1 and page 2 returned disjoint OBJECTID ranges; layer last edited 2025-06-10.')

# ---------------------------------------------------------------- Tokyo Metropolitan Government files (6)
TPORT = 'https://catalog.data.metro.tokyo.lg.jp/dataset/t000013d2000000034'
csvrow('JO38_JPGEO_TOKYO_MARKET_PRICE_PRODUCE', 'economy', 'wholesale-price',
       'japan,tokyo,wholesale-market,food,price,produce,toyosu',
       TPORT, 'Tokyo Metropolitan Central Wholesale Market - daily produce prices FY2026',
       '東京都卸売市場日報（販売結果・価格・青果）2026年度',
       'https://www.opendata.metro.tokyo.lg.jp/shijyou/2026/result_price_fresh.csv',
       '市場+日付+品名+販売方法+品種+産地+単位', '品名+産地+市場',
       'Daily auction and negotiated-sale prices for vegetables and fruit at the Tokyo metropolitan wholesale markets '
       '(Toyosu, Ota, Kasai and others): market, date, category, item, sale method, variety, origin, unit and high, '
       'middle and low price in yen. Fetched 2026-10-02: 7.7 MB UTF-8 CSV with 94,626 rows from 2026-04-02 to 2026-06-30, '
       'the file is rewritten in place through the fiscal year (resource modified 2026-07-27); the 7-field key is '
       'distinct on all 94,626 rows. The 2026/ path segment is the fiscal year.',
       date='日付', interval=86400, why='one CSV file holds the whole fiscal year, 94,626 rows in a single response')
csvrow('JO38_JPGEO_TOKYO_MARKET_PRICE_FISH', 'economy', 'wholesale-price',
       'japan,tokyo,wholesale-market,food,price,seafood,toyosu',
       TPORT, 'Tokyo Metropolitan Central Wholesale Market - daily seafood prices FY2026',
       '東京都卸売市場日報（販売結果・価格・水産）2026年度',
       'https://www.opendata.metro.tokyo.lg.jp/shijyou/2026/result_price_fish.csv',
       '市場+日付+品名+販売方法+産地+銘柄', '品名+産地+市場',
       'Daily seafood prices at the Tokyo metropolitan wholesale markets: market, date, category (大物 tuna, 鮮魚, '
       '塩干加工 ...), item, sale method, origin, brand and high, middle and low price. Fetched 2026-10-02: UTF-8 CSV '
       'with 16,697 rows from 2026-04-02 to 2026-06-30 (first row: Toyosu fresh tuna from Miyagi, high 10,800 yen); '
       'the 6-field key is distinct on all 16,697 rows. Rewritten in place through the fiscal year.',
       date='日付', interval=86400, why='one CSV file holds the whole fiscal year, 16,697 rows in a single response')
csvrow('JO38_JPGEO_TOKYO_MARKET_VOLUME_PRODUCE', 'economy', 'wholesale-volume',
       'japan,tokyo,wholesale-market,food,volume,produce',
       TPORT, 'Tokyo Metropolitan Central Wholesale Market - daily produce volumes FY2026',
       '東京都卸売市場日報（販売結果・数量・青果）2026年度',
       'https://www.opendata.metro.tokyo.lg.jp/shijyou/2026/result_quantity_fresh.csv',
       '市場+日付+品名+販売方法', '品名+市場+日付',
       'Daily wholesale quantities in kg for each vegetable and fruit item, market and sale method at the Tokyo '
       'metropolitan wholesale markets. Fetched 2026-10-02: UTF-8 CSV with 32,522 rows from 2026-04-02 to 2026-06-30; '
       'market+date+item+method is distinct on all 32,522 rows. Pairs with the price file of the same fiscal year.',
       date='日付', interval=86400, why='one CSV file holds the whole fiscal year, 32,522 rows in a single response')
csvrow('JO38_JPGEO_TOKYO_MARKET_PRICE_MEAT', 'economy', 'wholesale-price',
       'japan,tokyo,wholesale-market,food,price,meat,beef,pork',
       TPORT, 'Tokyo Metropolitan Central Wholesale Market - daily meat carcass prices by grade FY2026',
       '東京都卸売市場日報（販売結果・価格・食肉（規格））2026年度',
       'https://www.opendata.metro.tokyo.lg.jp/shijyou/2026/result_price_kikaku_meat.csv',
       '日付+種別+区分+販売方法+種類1+種類2+規格', '種別+種類1+規格+日付',
       'Daily beef and pork prices at the Tokyo meat market by grade: date, species, lot class (live animal, carcass '
       'brought in ...), sale method, breed and sex (e.g. 和牛 おす), grade (A-5 ... 等外), beef weighted average and pork '
       'high, low and weighted average prices. Fetched 2026-10-02: UTF-8 CSV with 17,980 rows from 2026-04-01 to '
       '2026-06-30; the 7-field key is distinct on all 17,980 rows. Rewritten in place through the fiscal year.',
       date='日付', interval=86400, why='one CSV file holds the whole fiscal year, 17,980 rows in a single response')
TREES = 'https://catalog.data.metro.tokyo.lg.jp/dataset/t000014d2000000029'
csvrow('JO38_JPGEO_TOKYO_STREET_TREES_23KU', 'environment', 'street-tree',
       'japan,tokyo,street-tree,urban-forest,road,geo',
       TREES, 'Tokyo Metropolitan Government - every street tree on metropolitan roads in the 23 wards',
       '都道の街路樹（23区）',
       'https://www.opendata.metro.tokyo.lg.jp/kensetsu/tokyo_gairoju.csv',
       '経度+緯度+樹種+樹高(m)+幹周(cm）+枝張(m)', '樹種+路線名+行政区',
       'Individual street trees on Tokyo metropolitan roads in the 23 wards: species, class (tall or medium), height, '
       'crown spread, trunk girth, ward, road class, route number and name, common road name and longitude/latitude. '
       'Fetched 2026-10-02: 13 MB Shift_JIS CSV with 144,183 rows; the 6-field key gives 142,229 distinct values, '
       'exactly the number of distinct rows (1,954 rows are byte-identical repeats). Resource dated 2025-02-20.',
       extra=['charset=shift_jis', 'lat_key=緯度', 'lon_key=経度'],
       why='one CSV file is the whole register, 144,183 rows in a single response')
csvrow('JO38_JPGEO_TOKYO_STREET_TREES_TAMA', 'environment', 'street-tree',
       'japan,tokyo,tama,street-tree,urban-forest,road,geo',
       TREES, 'Tokyo Metropolitan Government - every street tree on metropolitan roads in the Tama area',
       '都道の街路樹（多摩地域）',
       'https://www.opendata.metro.tokyo.lg.jp/kensetsu/tokyo_tama_gairoju.csv',
       'longitude+latitude', 'name+route_name',
       'Individual street trees on Tokyo metropolitan roads in the Tama area: species name, class, height, girth, '
       'crown width, route number, route name and nickname, route type and coordinates. Fetched 2026-10-02: 8.6 MB '
       'UTF-8 CSV with 83,875 rows, longitude+latitude distinct on all 83,875; resource modified 2026-04-26.',
       extra=['lat_key=latitude', 'lon_key=longitude'],
       why='one CSV file is the whole register, 83,875 rows in a single response')
csvrow('JO38_JPGEO_TOKYO_IZU_WATER_TEMP', 'environment', 'sea-temperature',
       'japan,tokyo,izu-islands,ocean,sea-temperature,fisheries,time-series',
       'https://catalog.data.metro.tokyo.lg.jp/dataset/t000012d0000000069',
       'Tokyo Metropolitan Government - daily coastal water temperature at the Izu islands since 1926',
       '伊豆諸島各島定地水温',
       'https://www.opendata.metro.tokyo.lg.jp/sangyouroudou/2026fixed_water_temperature_at_Izu-islands.csv',
       '日付', '日付',
       'Daily fixed-point coastal water temperatures for the Izu islands published by the Tokyo Bureau of Industrial and '
       'Labor Affairs, one column per site: 大島, 新島, 式根島, 神津島, 坪田, 阿古, 御蔵島, 大賀郷, 三根, 青灯台下 at 0 m and '
       '4 m, and 脇浜. Fetched 2026-10-02: UTF-8 CSV with 36,706 dated rows from 1926-01-01 to 2026-06-30 plus 154 '
       'blank trailing rows; 日付 is distinct on every dated row. The file name carries the year it was refreshed.',
       date='日付', interval=604800, why='one CSV file is the whole series, 36,706 rows in a single response')

# ---------------------------------------------------------------- Kanazawa (3)
KZ = 'catalog-data.city.kanazawa.ishikawa.jp'
ckan('JO38_JPGEO_KANAZAWA_FIRE_TARGET_BUILDINGS', 'public-safety', 'fire-code-building',
     'japan,ishikawa,kanazawa,fire,building,registry,geo',
     f'https://{KZ}/dataset/172014-boukataisyobutu',
     'Kanazawa City - register of buildings subject to the Fire Service Act (防火対象物)',
     '金沢市 防火対象物情報一覧', KZ, '575f5855-f9f3-49b3-9ce4-f5ea925b3cf8', 19095,
     '施設ID', '建物名称',
     'Buildings in Kanazawa listed as 防火対象物 (fire-prevention objects under the Fire Service Act): facility ID, '
     'responsible fire station (3 distinct, 中央消防署 8,706), site number, building name (apartment blocks, factories, halls, '
     'schools, shops), street address and lat/lon. Fetched 2026-10-02: datastore total 19,095, a full walk at 1000 per '
     'page returned 19,095 rows with 19,095 distinct 施設ID (18,759 distinct building names). Resource dated 2023-10-25.',
     extra=['lat_key=緯度', 'lon_key=経度'])
ckan('JO38_JPGEO_KANAZAWA_MARKET_VEG_ORIGIN', 'economy', 'wholesale-volume',
     'japan,ishikawa,kanazawa,wholesale-market,food,produce,origin',
     f'https://{KZ}/dataset/kanazawa_market_products_by_origin_annual',
     'Kanazawa Central Wholesale Market - vegetables handled by item, origin and year since 2000',
     '金沢市中央卸売市場 品目別産地別年別取扱高（青果部 野菜）', KZ, '8dbcb7a8-9f19-422a-a773-e765e9fccce6', 50772,
     '販売年+品目+産地', '品目+産地+販売年',
     'Annual vegetable volumes, values and average unit prices at Kanazawa Central Wholesale Market by item (528 '
     'distinct) and origin prefecture or country (74 distinct), 2000 to 2025. Fetched 2026-10-02: datastore total '
     '50,772, a full walk returned 50,772 rows with 50,772 distinct year+item+origin keys; resource modified 2026-04-28.',
     date='販売年')
ckan('JO38_JPGEO_KANAZAWA_MARKET_FISH_ORIGIN', 'economy', 'wholesale-volume',
     'japan,ishikawa,kanazawa,wholesale-market,food,seafood,origin',
     f'https://{KZ}/dataset/kanazawa_market_products_by_origin_annual',
     'Kanazawa Central Wholesale Market - fresh fish handled by species, origin and year since 2000',
     '金沢市中央卸売市場 品目別産地別年別取扱高（水産物部 鮮魚）', KZ, '590e1ed3-5350-45aa-8916-818df1503400', 40202,
     '販売年+品名+産地', '品名+産地+販売年',
     'Annual fresh-fish volumes, values and average unit prices at Kanazawa Central Wholesale Market by species and '
     'landing origin, 2000 to 2025 (first row: 2000 tuna from Hokkaido, 2,613 kg). Fetched 2026-10-02: datastore total '
     '40,202, a full walk returned 40,202 rows with 40,202 distinct year+species+origin keys; resource modified 2026-04-28.',
     date='販売年')

ckan('JO38_JPGEO_KANAZAWA_MARKET_FRUIT_ORIGIN', 'economy', 'wholesale-volume',
     'japan,ishikawa,kanazawa,wholesale-market,food,fruit,origin',
     f'https://{KZ}/dataset/kanazawa_market_products_by_origin_annual',
     'Kanazawa Central Wholesale Market - fruit handled by item, origin and year since 2000',
     '金沢市中央卸売市場 品目別産地別年別取扱高（青果部 果実）', KZ, 'e470140c-4a6a-4338-b6ae-43f4017a9d86', 24656,
     '販売日+品名+産地', '品名+産地+販売日',
     'Annual fruit volumes, values and average unit prices at Kanazawa Central Wholesale Market by item (462 distinct) '
     'and origin (74 distinct), 2000 to 2025; the year column is named 販売日 in this resource. Fetched 2026-10-02: '
     'datastore total 24,656, a full walk returned 24,656 rows with 24,656 distinct year+item+origin keys; resource '
     'modified 2026-04-28.',
     date='販売日')

# ---------------------------------------------------------------- Kobe (2)
KB = 'catalog.city.kobe.lg.jp'
ckan('JO38_JPGEO_KOBE_PORT_CARGO_BY_COUNTRY', 'trade', 'port-cargo-flow',
     'japan,hyogo,kobe,port,trade,cargo,shipping',
     f'https://{KB}/dataset/58805-2-2-10-10-33e8acb558b859a5-0886e8fcaa46c413',
     'Port of Kobe - cargo tonnage by commodity and partner country or prefecture, 2015-2024',
     '神戸港 輸移出入別品種別国別県別貨物量', KB, '0a33e799-9035-4039-82f7-292722974775', 30408,
     '外貿・内貿+輸移出入+使用年+品種+仕出向国・県', '品種+仕出向国・県+使用年',
     'Freight tonnage through the Port of Kobe split by foreign or domestic trade, import/export/coastal in-out, year, '
     'commodity and partner country (foreign) or prefecture (domestic): e.g. 2024 wheat imports 450,085 t from Canada '
     'and 206,991 t from Australia. Fetched 2026-10-02: datastore total 30,408 covering 10 years 2015-2024, a full walk '
     'returned 30,408 rows with 30,408 distinct 5-field keys; resource modified 2025-12-16.',
     date='使用年')
ckan('JO38_JPGEO_KOBE_SANITATION_PERMITS', 'registry', 'business-permit',
     'japan,hyogo,kobe,food,permit,business,registry',
     f'https://{KB}/dataset/6359-7-2-0-3-1d5b2e5dd4d08682-c4eba86e486e886e',
     'Kobe City - all food and sanitation business permits (snapshot end of May 2021)',
     '神戸市 生活衛生関係許可施設（令和3年5月末現在の全ての許可施設）', KB, 'fc07e197-5dc3-4183-b7d8-cd4012c0f2c2', 31449,
     '許可番号', '屋号+業種情報公開名称',
     'Every food and sanitation business permit Kobe held at the end of May 2021: permit number, business type '
     '(restaurant, cafe, confectionery, mobile food vehicle ...), premises address and building, trade name, '
     'operator (company or sole proprietor), premises phone, permit and expiry dates and application type. Fetched '
     '2026-10-02: datastore total 31,449, a full walk returned 31,449 rows with 31,449 distinct 許可番号. The dataset '
     'page was modified 2026-09-14 but this is still its only full-register resource.',
     date='許可決定日')

# ---------------------------------------------------------------- other CKAN municipalities (5)
ckan('JO38_JPGEO_SAPPORO_FOOD_PERMITS', 'registry', 'business-permit',
     'japan,hokkaido,sapporo,food,permit,business,registry',
     'https://ckan.pf-sapporo.jp/dataset/sapporo_food_business_licences',
     'Sapporo City - food business permit register (as of 2026-03-31)',
     '札幌市内の食品営業許可施設一覧（2026年3月31日時点）', 'ckan.pf-sapporo.jp', '54618ac4-da90-493c-8a20-8df29be9d435', 25163,
     '許可番号+許可年月日', '屋号+業種名',
     'Every food business permit in force in Sapporo on 2026-03-31: ward, permit number, permit date, business type, '
     'applicant (company or individual), trade name and premises address. Fetched 2026-10-02: datastore total 25,163, a '
     'full walk returned 25,163 rows; 許可番号 alone repeats (24,550 distinct, ward offices reuse numbers) while '
     '許可番号+許可年月日 is distinct on all 25,163. Newest permit 2026-03-31.',
     date='許可年月日')
ckan('JO38_JPGEO_MINATO_FOOD_PERMITS', 'registry', 'business-permit',
     'japan,tokyo,minato,food,permit,business,registry',
     'https://opendata.city.minato.tokyo.jp/dataset/food_business_all',
     'Minato City (Tokyo) - food business permit register, updated monthly',
     '港区 食品営業許可一覧', 'opendata.city.minato.tokyo.jp', 'c9d0299e-8e05-4317-877f-83055709e41f', 5791,
     '許可番号', '施設名称',
     'Food business permits for Minato ward in the national standard-dataset layout: facility name, business type, '
     'full address and building, operating company (法人名), permit number, first-permit, permit, start and expiry '
     'dates. Fetched 2026-10-02: datastore total 5,791 (5,790 permits plus a trailer row stating 令和8年8月31日現在), a '
     'full walk returned 5,791 rows with 5,791 distinct 許可番号; the resource is replaced in place (modified 2026-09-30).',
     date='許可年月日', interval=86400)
ckan('JO38_JPGEO_FUKUYAMA_FOOD_PERMITS', 'registry', 'business-permit',
     'japan,hiroshima,fukuyama,food,permit,business,registry',
     'https://data.city.fukuyama.hiroshima.jp/dataset/licensed_food',
     'Fukuyama City - food sanitation business permits (as of end of March 2026)',
     '福山市 営業許認可等施設一覧（食品衛生関係）', 'data.city.fukuyama.hiroshima.jp', '21fec913-dc08-4344-94d1-ffb0068ab147', 5880,
     '施設番号', '施設＿名称（屋号・商号）１+業種',
     'Every food sanitation permit held in Fukuyama at the end of March 2026: applicant name, title and representative, '
     'applicant address, premises postcode and address, trade name, business type and sub-type, facility number, '
     'permit start and end dates and first-permit date. Fetched 2026-10-02: datastore total 5,880, a full walk '
     'returned 5,880 rows with 5,880 distinct 施設番号; resource modified 2026-04-28.')
ckan('JO38_JPGEO_UTSUNOMIYA_INFECTIOUS_DISEASE', 'health', 'disease-surveillance',
     'japan,tochigi,utsunomiya,infectious-disease,influenza,surveillance,time-series',
     'https://catalog.city.utsunomiya.tochigi.jp/dataset/kansenshoudoukouchousa',
     'Utsunomiya City - weekly sentinel infectious disease reports since 2012',
     '宇都宮市 感染症動向調査（定点当たり患者数）', 'catalog.city.utsunomiya.tochigi.jp', '77fbcad3-b468-4368-83e0-bd9f43fb7f28', 14215,
     '病名+年+週', '病名+年+週',
     'Weekly patients per sentinel site in Utsunomiya for each sentinel-reported disease (インフルエンザ, 急性呼吸器感染症 '
     'and others) by year and epidemiological week, 2012 to 2026. Fetched 2026-10-02: datastore '
     'total 14,215, a full walk returned 14,215 rows with 14,215 distinct disease+year+week keys; 1,055 rows already '
     'for 2026; resource modified 2026-04-20.',
     date='年', interval=86400)
ckan('JO38_JPGEO_AKITA_POPULATION_FLOW', 'demographics', 'population-flow',
     'japan,akita,population,migration,demographics,time-series',
     'https://ckan.pref.akita.lg.jp/dataset/050008_toukeika_033',
     'Akita Prefecture - population and migration by municipality and single year of age, 2006-2025',
     '【秋田県】年齢別人口流動調査（年報時系列データ）', 'ckan.pref.akita.lg.jp', 'd9999c29-93ea-4268-b07a-7fa342bad173', 64350,
     '年月日コード+県・市町村コード+県・市町村+年齢・年齢階級（歳）', '県・市町村+年齢・年齢階級（歳）+年月日',
     'Population on 1 October of each year for the prefecture and its 25 municipalities by single year and 5-year age '
     'band, with male/female in-prefecture and out-of-prefecture moves in and out, deaths and net change, 2006 to 2025. '
     'Fetched 2026-10-02: datastore total 64,350, a full walk returned 64,350 rows; date+code+age alone repeats 24 times '
     'because the 2007 rows reuse code 5464 for two towns, so the key adds the municipality name. Modified 2026-07-02.',
     date='年月日')

# ---------------------------------------------------------------- G-spatial / MLIT operator data (3)
GSP = 'https://www.geospatial.jp/ckan/dataset'
csvrow('JO38_JPGEO_MLIT_MOTOR_CARRIER_ACCIDENTS', 'transport', 'commercial-vehicle-accident',
       'japan,mlit,road-safety,accident,truck,bus,taxi,geo',
       f'{GSP}/34f1fd46-04d0-4a6a-9136-f4deb259b246',
       'MLIT - accident reports filed by bus, truck and taxi operators, 2003-2023',
       '自動車運送事業事故データ（自動車事故報告書）',
       f'{GSP}/34f1fd46-04d0-4a6a-9136-f4deb259b246/resource/d009e4d4-8062-4c00-b176-3efa480a7d44/download/02_jikohoukokusyo.csv',
       'OCCURRENCE_DATE_AND_TIME+LOCATION+VEHICLE_ID+ACCIDENT_TYPE_(CATEGORY)+ORDER_OF_ACCIDENT_OCCURRENCE+COLLISION_CONDITION',
       'ACCIDENT_TYPE_(CATEGORY)+LOCATION',
       'Accident reports (自動車事故報告書) that commercial bus, truck and taxi operators must file with MLIT: date and '
       'time, weather, location with lat/lon, vehicle ID, accident type and sequence, collision type, vehicle make, model '
       'code and body, load and hazardous cargo, road and surface, speeds, driver working hours and days off, injuries '
       'and deaths. Fetched 2026-10-02 (redirects to S3): 35 MB UTF-8 CSV with 108,668 reports dated 2003 to 2023; the '
       '6-field key is distinct on 108,633 and the remaining 35 differ only in other columns.',
       date='OCCURRENCE_DATE_AND_TIME', extra=['lat_key=LATITUDE', 'lon_key=LONGITUDE'],
       why='one CSV file is the whole dataset, 108,668 rows in a single 35 MB response')
csvrow('JO38_JPGEO_MLIT_WAREHOUSE_REGISTRATIONS', 'registry', 'warehouse-registration',
       'japan,mlit,logistics,warehouse,registry,geo',
       f'{GSP}/00c1adf2-35ec-40c5-9596-95c6229743ad',
       'MLIT - warehouse business registration applications (warehouse-level), FY2024 data',
       '倉庫業データ 倉庫業登録申請書（2024年度）',
       f'{GSP}/00c1adf2-35ec-40c5-9596-95c6229743ad/resource/a2ae846b-9055-44b1-8802-32c338cad6d4/download/01_soukogyoutourokushinseisyo.csv',
       'WAREHOUSE_ID', 'WAREHOUSE_TYPE+CITY',
       'One row per registered warehouse from warehouse-business registration filings: operator, office and warehouse '
       'IDs, lat/lon, warehouse class (e.g. 1類倉庫, 貯蔵槽倉庫), goods stored, structure, completion date, land and '
       'building tenure, floor-by-floor area, eaves height and volume, prefecture and municipality. Fetched 2026-10-02: '
       'UTF-8 CSV with 4,876 rows and 85 columns, WAREHOUSE_ID distinct on all 4,876; resource modified 2025-09-07.',
       extra=['lat_key=LATITUDE', 'lon_key=LONGITUDE'],
       why='one CSV file is the whole dataset, 4,876 rows in a single response')

# ---------------------------------------------------------------- municipal / prefectural ArcGIS (7)
arc('JO38_JPGEO_OKINAWA_FARMLAND_LEDGER', 'agriculture', 'farmland-parcel',
    'japan,okinawa,farmland,agriculture,land,parcel,registry',
    'Okinawa - farmland ledger pins for 37 agricultural committees (水土里情報)',
    '沖縄県 農地台帳ピン（筆ポリゴン検索用）',
    'https://services1.arcgis.com/t2DiVWqSr8z6f9DT/arcgis/rest/services/staffgis_okinawa_farmland_pins_agol/FeatureServer/0',
    'OBJECTID', 361493, 'attributes.Address',
    'Farmland ledger (農地台帳) points for Okinawa published by the 水土里情報 ArcGIS organisation: agricultural '
    'committee (37; Miyakojima 49,313, Uruma 39,676), oaza code, full address and lot number, land category (畑 '
    '318,755, 田 13,379), registered area, agricultural-promotion and city-planning zoning, kind of right and lease '
    'dates, idle-land survey results, ledger ID and a hashed farmer number. Fetched 2026-10-02: 361,493 features '
    '(maxRecordCount 4000); pages 1 and 2 at 1000 returned OBJECTID 1-1000 and 1001-2000; layer last edited '
    '2026-08-03. The service describes itself as a verification layer for a staff parcel-lookup app.')
arc('JO38_JPGEO_OMIHACHIMAN_PARCELS', 'land', 'land-parcel',
    'japan,shiga,omihachiman,land,parcel,cadastre',
    'Omihachiman City - lot-number (地番) reference points',
    '近江八幡市 地番参考図レイヤ 地番',
    'https://services5.arcgis.com/yza4EfymdySQuOhS/arcgis/rest/services/d010_09_chiban_gdb/FeatureServer/3',
    'OBJECTID_1', 133745, 'attributes.全所在',
    'One point per land parcel in Omihachiman from the city public map: oaza code and name (188 oaza), lot number '
    'and the combined location string (e.g. 安養寺町1208-1) with coordinates. Fetched 2026-10-02: 133,745 features '
    '(maxRecordCount 2000); pages 1 and 2 returned OBJECTID_1 1-1000 and 1001-2000; layer last edited 2026-04-10.')
arc('JO38_JPGEO_KOSAI_PARCELS', 'land', 'land-parcel',
    'japan,shizuoka,kosai,land,parcel,cadastre',
    'Kosai City - lot-number (地番) points',
    '湖西市 地番',
    'https://services5.arcgis.com/KCMGoYYcTgZdlker/arcgis/rest/services/chiban/FeatureServer/0',
    'FID', 129237, 'attributes.f000000001',
    'Land parcel label points for Kosai (Shizuoka), each carrying the town name and lot number as one string (e.g. '
    'ときわ三丁目1198-17) at the parcel position. Fetched 2026-10-02: 129,237 features (maxRecordCount 2000); pages 1 and '
    '2 returned FID 1-1000 and 1001-2000; layer last edited 2026-04-13.')
arc('JO38_JPGEO_HIDAKA_BUILDING_CONFIRMATIONS', 'land', 'building-confirmation',
    'japan,saitama,hidaka,building,construction,permit',
    'Hidaka City (Saitama) - building confirmation certificates issued, located on the map',
    '日高市 建築確認情報（公開用）',
    'https://services5.arcgis.com/32q7k7mjKAH9oXmb/arcgis/rest/services/kentikukeikakugaiyousyo/FeatureServer/0',
    'FID', 9235, 'attributes.確認済', latlon=False,
    desc='Building confirmation certificates (建築確認済証) issued for sites in Hidaka, each a multipoint at the site with '
    'the certificate number (e.g. 第SJK-KX225436550号) and issue date, dates seen from 平成10 (1998) to 令和6 (2024). '
    'Fetched 2026-10-02: 9,235 features (maxRecordCount 2000, multipoint geometry so no x/y keys); pages 1 and 2 '
    'returned FID 1-1000 and 1001-2000; layer last edited 2026-02-02.')
arc('JO38_JPGEO_SAITAMA_RACCOON_CAPTURES', 'environment', 'wildlife-capture',
    'japan,saitama,wildlife,invasive-species,raccoon,capture',
    'Saitama Prefecture - raccoon captures FY2024 (date, place, method, sex, age)',
    '埼玉県 アライグマ捕獲情報（令和6年度）',
    'https://services9.arcgis.com/n65w8AXGaYPTqFYI/arcgis/rest/services/R6OnlyRacoonkoukai_/FeatureServer/0',
    'ObjectId', 11881, 'attributes.市町村+attributes.捕獲方法', date='attributes.捕獲日',
    desc='Every invasive raccoon capture in Saitama recorded for FY2024 (R6): municipality (71 '
    'distinct), lat/lon, capture method (box trap ...), capture date, species, sex and adult or juvenile. Fetched '
    '2026-10-02: 11,881 features (maxRecordCount 1000); pages 1 and 2 returned ObjectId 1-1000 and 1001-2000; layer '
    'last edited 2025-09-19.')
arc('JO38_JPGEO_SAITAMA_BOREHOLES', 'geology', 'borehole',
    'japan,saitama,geology,borehole,ground,groundwater',
    'Saitama Prefecture - geotechnical borehole index with groundwater level',
    '埼玉県 地盤情報（ボーリング柱状図）位置',
    'https://services9.arcgis.com/n65w8AXGaYPTqFYI/arcgis/rest/services/c20250108WEBGIS_XYTableToPoint/FeatureServer/0',
    'OBJECTID', 6415, 'attributes.Station',
    'Boring logs published by the Saitama Prefecture GIS organisation: station ID, lat/lon, ground elevation, '
    'boring length in metres, water level and the source log file name. Fetched 2026-10-02: 6,415 features '
    '(maxRecordCount 2000); pages 1 and 2 returned OBJECTID 1-1000 and 1001-2000; layer last edited 2025-03-06.')
arc('JO38_JPGEO_TSUKUBA_SEWER_MANHOLES', 'infrastructure', 'sewer-manhole',
    'japan,ibaraki,tsukuba,sewer,infrastructure,utility,geo',
    'Tsukuba City - foul-sewer manhole register',
    'つくば市 下水道汚水（人孔）',
    'https://services6.arcgis.com/yogEhrEiATcZmbcU/arcgis/rest/services/osui_jinkou/FeatureServer/0',
    'FID', 46684, 'attributes.分類+attributes.処理分区',
    desc='Every foul-sewer manhole in the Tsukuba sewer ledger: manhole class, separate or combined system, connection, '
    'inner diameter, ground level and depth, pump flag, project type, construction fiscal year, treatment district and '
    'sub-district, drawing number, asset numbers and last-update timestamp, with coordinates. Fetched 2026-10-02: '
    '46,684 features (maxRecordCount 2000); pages 1 and 2 returned FID 1-1000 and 1001-2000; layer last edited 2025-10-14.')
arc('JO38_JPGEO_ITAMI_WATCH_CAMERAS', 'public-safety', 'surveillance-camera',
    'japan,hyogo,itami,cctv,surveillance,camera,public-safety',
    'Itami City - locations of the city watch cameras (見守りカメラ)',
    '伊丹市 見守りカメラ 設置位置',
    'https://services1.arcgis.com/FcIbUx0Qx9iofzTs/arcgis/rest/services/03_04_mimamorume/FeatureServer/0',
    'OBJECTID', 1247, 'attributes.ミマモルメ',
    'Positions of the street cameras of Itami city-wide watch network: management number (伊丹-001 ...), elementary '
    'school district, site description, street address, lat/lon and class. Fetched 2026-10-02: 1,247 features '
    '(maxRecordCount 1000); page 1 returned 1000 and page 2 the remaining 247 with disjoint OBJECTID; layer last edited '
    '2026-03-31.')

HEADER = [
    '# Batch 38 beat jpgeo3: Japanese public-sector GIS layers and municipal/prefectural open-data tables not yet registered.',
    '# Sources: ArcGIS Online FeatureServers of MLIT (国土交通DPF), NIES, Okinawa, Saitama, Tsukuba, Omihachiman, Kosai, Hidaka and Itami;',
    '# CKAN datastore_search resources of Kanazawa, Kobe, Sapporo, Minato, Fukuyama, Utsunomiya and Akita; Tokyo metropolitan',
    '# CSV files; MLIT operator datasets on G空間情報センター. Every row fetched with curl/urllib on 2026-10-02 (counts, page 2, key uniqueness).',
    '# id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts',
]

seen = set()
with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    for h in HEADER:
        f.write(h + '\n')
    for r in rows:
        assert len(r) == 13, r[0]
        assert r[0] not in seen, r[0]
        seen.add(r[0])
        for x in r:
            assert '|' not in x, (r[0], x)
            assert '\n' not in x, r[0]
        f.write('|'.join(r) + '\n')
print(len(rows), 'rows ->', os.path.abspath(OUT))
