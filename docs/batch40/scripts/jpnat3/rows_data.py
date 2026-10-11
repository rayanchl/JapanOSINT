# ---------------------------------------------------------------- NDL Imperial Diet minutes API
TK = "https://teikokugikai-i.ndl.go.jp/api/emp/meeting_list?nameOfHouse={h}&maximumRecords=100&recordPacking=json&startRecord=1"
row("JO40_JPNAT_TEIKOKU_GIKAI_PEERS", "json", "government", "diet-meeting-record",
    "japan,ndl,diet,parliament,imperial-diet,house-of-peers,history,minutes",
    "https://teikokugikai-i.ndl.go.jp/",
    "NDL Imperial Diet minutes - House of Peers meeting records (1890-1947)",
    "帝国議会会議録検索システム 貴族院 会議録一覧",
    TK.format(h="%E8%B2%B4%E6%97%8F%E9%99%A2"),
    "The NDL Imperial Diet minutes API (meeting_list, the keyless simple-output form) filtered to the House of Peers: numberOfRecords=8,915 meeting issues, 100 per page, 90 pages; page 1 starts at the 1st session (1891-03-07 plenary, issue 45), page 90 holds the last 15 (index volumes with no date). Each record carries issueID, session, house, meeting name, issue number, date, meetingURL and pdfURL into the page images, plus speechRecord - every speaker with a speechURL. issueID is distinct over every page fetched (100 of 100 on pages 1 and 2).",
    "interval=604800;array_path=meetingRecord;id_keys=issueID;title_keys=nameOfHouse+nameOfMeeting+issue;date_keys=date;link_keys=meetingURL;page_param=startRecord;page_start=1;page_size=100;page_max=92;timeout_ms=60000")
row("JO40_JPNAT_TEIKOKU_GIKAI_REPRESENTATIVES", "json", "government", "diet-meeting-record",
    "japan,ndl,diet,parliament,imperial-diet,house-of-representatives,history,minutes",
    "https://teikokugikai-i.ndl.go.jp/",
    "NDL Imperial Diet minutes - House of Representatives meeting records (1890-1947)",
    "帝国議会会議録検索システム 衆議院 会議録一覧",
    TK.format(h="%E8%A1%86%E8%AD%B0%E9%99%A2"),
    "The NDL Imperial Diet minutes API (meeting_list) filtered to the House of Representatives: numberOfRecords=17,774 meeting issues (plenaries and committees), 100 per page over 178 pages; startRecord=17701 returned the final 74, ending with the committee speaker indexes. Each record: issueID, imageKind, session, house, meeting name, issue, date, meetingURL, pdfURL and the speechRecord list (speaker, order, speechURL). issueID is unique per meeting issue. Page bodies run about 1.7 MB each.",
    "interval=604800;array_path=meetingRecord;id_keys=issueID;title_keys=nameOfHouse+nameOfMeeting+issue;date_keys=date;link_keys=meetingURL;page_param=startRecord;page_start=1;page_size=100;page_max=180;timeout_ms=60000")

# ---------------------------------------------------------------- DDBJ Search (ROIS-NIG)
row("JO40_JPNAT_DDBJ_JGA_GEA_METABOBANK", "json", "science", "genomic-archive-entry",
    "japan,ddbj,nig,genomics,jga,gea,metabobank,research-data",
    "https://ddbj.nig.ac.jp/search/",
    "DDBJ Search - DDBJ Center's own archives: JGA studies/datasets/policies, GEA and MetaboBank",
    "DDBJ Search JGA・GEA・MetaboBank エントリ一覧",
    "https://ddbj.nig.ac.jp/search/api/entries/?types=jga-study,jga-dataset,jga-policy,jga-dac,gea,metabobank&perPage=100&page=1&dbXrefsLimit=1000",
    "The DDBJ Search API cross-type entries endpoint restricted to the archives the DDBJ Center (National Institute of Genetics) runs itself: total=2,172 (jga-study 563, jga-dataset 710, jga-policy 14, jga-dac 1, gea 769, metabobank 115). All 22 pages of 100 were walked on 2026-10-02 (52.5 MB): 2,172 records, 2,169 distinct type+identifier - the upstream itself lists JGAD000683, JGAD000721 and JGAS000561 twice in a row on one page. Each entry: identifier, type, title, description, organism, accessibility, datePublished/Modified/Created, the full archive properties and dbXrefs with per-type totals in dbXrefsCount. dbXrefsLimit=1000 is the API maximum (default 100); with it one entry of the 2,172 still has more cross-references of one type (1,405) than the API will list, and its dbXrefsCount says so.",
    "interval=604800;id_keys=type+identifier;title_keys=title,identifier;date_keys=datePublished,dateModified;link_keys=url;array_path=items;page_param=page;page_max=24;timeout_ms=60000")

# ---------------------------------------------------------------- FIT/FIP certified generators (ANRE, METI)
FIT = "https://www.fit-portal.go.jp/servlet/servlet.FileDownload?file={f}"
for pid, pen, pja, fid, n in [
    ("IBARAKI", "Ibaraki", "茨城県", "00PhA0000013RxiUAE", "29,675"),
    ("CHIBA", "Chiba", "千葉県", "00PhA0000013PpsUAE", "21,263"),
    ("GUNMA", "Gunma", "群馬県", "00PhA0000013TzOUAU", "21,219"),
    ("TOCHIGI", "Tochigi", "栃木県", "00PhA0000013TWNUA2", "19,131"),
    ("MIE", "Mie", "三重県", "00PhA0000013R3MUAU", "16,499"),
    ("KAGOSHIMA", "Kagoshima", "鹿児島県", "00PhA0000013QXaUAM", "15,961"),
    ("AICHI", "Aichi", "愛知県", "00PhA0000013UIjUAM", "15,056"),
    ("SHIZUOKA", "Shizuoka", "静岡県", "00PhA0000013TEfUAM", "15,008"),
]:
    row("JO40_JPNAT_FIT_CERTIFIED_PLANTS_" + pid, "xlsx", "energy", "renewable-generator-certification",
        "japan,meti,anre,fit,fip,renewable-energy,solar,power-plant,registry," + pen.lower(),
        "https://www.fit-portal.go.jp/PublicInfo",
        "ANRE FIT/FIP certified renewable generating facilities - %s Prefecture" % pen,
        "再生可能エネルギー電気事業計画認定情報 公表（%s）" % pja,
        FIT.format(f=fid),
        ("The Agency for Natural Resources and Energy's published list of FIT/FIP business-plan certifications for %s, "
         "edition 202608 (served as %s_202608.xlsx under a Salesforce attachment id linked from the PublicInfo page): "
         "%s certified facilities, one per facility ID, with generator operator, representative, operator address and phone, "
         "source (solar, wind, hydro, biomass), output kW, facility site address, extra land-parcel count, certification date, "
         "planned and reported start of operation, decommissioning-reserve status and end of the purchase period. "
         "設備ID is unique over every line; a second sheet (すべての設備所在地) lists every parcel address of multi-site plants. "
         "The sheet has no row 1 (its first row is r=2, a note line), which lib/xlsx.c renders as a blank line and probe_hp_batch.py skips, so the two readers "
         "disagree by one line about where the header is: csv_skip_lines=2 is right for the engine (blank + note), and no key column is declared because the probe, "
         "reading one line lower, cannot see it. Column A is empty on every line, so each record keys on its first non-empty scalar, the facility ID.")
        % (pen, pja, n),
        "interval=604800;xlsx_sheet_index=0;csv_skip_lines=2;timeout_ms=90000;pagination_ok=one workbook holds the whole prefecture list (sheet 認定設備)")

# ---------------------------------------------------------------- MHLW long-term care provider open data (types not yet registered)
KG = "https://www.mhlw.go.jp/content/12300000/{f}.csv"
for sid, code, ja, en, f, n in [
    ("CARE_MANAGEMENT", "430", "居宅介護支援", "home care-management (care plan) offices", "jigyosho_430_all_20260709180740", "36,212"),
    ("COMMUNITY_DAY_CARE", "780", "地域密着型通所介護", "community-based day-care centres", "jigyosho_780_all_20260709180927", "18,023"),
    ("DEMENTIA_GROUP_HOMES", "320", "認知症対応型共同生活介護", "dementia group homes", "jigyosho_320_all_20260709180609", "14,236"),
    ("SHORT_STAY", "210", "短期入所生活介護", "short-stay daily-life care facilities", "jigyosho_210_all_20260709180538", "11,275"),
    ("SPECIAL_NURSING_HOMES", "510", "介護老人福祉施設", "special nursing homes for the elderly (tokuyo)", "jigyosho_510_all_20260709180754", "8,527"),
]:
    row("JO40_JPNAT_KAIGO_" + sid, "csv", "health", "care-service-provider",
        "japan,mhlw,long-term-care,kaigo,elderly-care,facility,registry,geo",
        "https://www.mhlw.go.jp/stf/kaigo-kouhyou_opendata.html",
        "MHLW care-service information system open data - %s (service %s)" % (en, code),
        "介護サービス情報公表システム オープンデータ %s_%s" % (code, ja),
        KG.format(f=f),
        ("Every %s provider (介護サービス %s) in the national care-service information publication system, output 2026-07-09: "
         "%s offices nationwide, each with municipality code, prefecture and city, office name and kana, service type, address and building, "
         "latitude and longitude (present on every row), phone, fax, corporate number and operator name, care-insurance office number, "
         "available days, capacity, URL and the joint elderly/disability service flags. UTF-8 with BOM. "
         "Office number+name+address is unique over all rows. Services 110-160 of this dataset are already registered; this one is not.")
        % (en, code, n),
        "interval=604800;title_keys=事業所名;id_keys=事業所番号+事業所名+住所;lat_key=緯度;lon_key=経度;timeout_ms=90000;pagination_ok=one CSV holds the whole national list for this service type (semiannual edition)")

# ---------------------------------------------------------------- MHLW workplace safety site: injury and fatality databases
row("JO40_JPNAT_ANZEN_INJURY_DB_R03", "xlsx", "labour", "occupational-injury-case",
    "japan,mhlw,occupational-safety,workplace-accident,labour,anzeninfo",
    "https://anzeninfo.mhlw.go.jp/anzen_pgm/SHISYO_FND.html",
    "MHLW workplace safety site - occupational injury database, 2021 (annual file)",
    "職場のあんぜんサイト 労働災害（死傷）データベース 令和3年 年間データ",
    "https://anzeninfo.mhlw.go.jp/anzen/shisyo_xls/sisyou_db_r03.xlsx",
    "The annual 2021 file of the MHLW occupational injury database (a random quarter sample of all lost-time injuries of 4+ days reported that year): 29,605 cases, each with a link ID, case ID, era/year/month, hour band, a free-text account of how the accident happened, industry (three levels, code and name), workplace size band, causal agent (three levels), accident type and the worker's age. 連結ID is unique 1..29,605. The annual file was added to the site on 2026-09-30.",
    "interval=604800;xlsx_sheet_index=0;csv_skip_lines=1;title_keys=災害状況;id_keys=連結ID;timeout_ms=90000;pagination_ok=one workbook holds the whole year (single sheet)")
row("JO40_JPNAT_ANZEN_INJURY_DB_R02", "xlsx", "labour", "occupational-injury-case",
    "japan,mhlw,occupational-safety,workplace-accident,labour,anzeninfo",
    "https://anzeninfo.mhlw.go.jp/anzen_pgm/SHISYO_FND.html",
    "MHLW workplace safety site - occupational injury database, 2020 (annual file)",
    "職場のあんぜんサイト 労働災害（死傷）データベース 令和2年 年間データ",
    "https://anzeninfo.mhlw.go.jp/anzen/shisyo_xls/sisyou_db_r02.xlsx",
    "The annual 2020 file of the same MHLW injury database: 29,956 sampled lost-time injury cases (link IDs 1..29,956) with month, hour band, narrative of the accident, industry codes and names, workplace size, causal agent, accident type and age. The header is the first sheet row (two cells carry line breaks); the second row is a code/name sub-header and reads as one record without a link ID. 連結ID is unique.",
    "interval=604800;xlsx_sheet_index=0;csv_skip_lines=0;title_keys=災害状況;id_keys=連結ID;timeout_ms=90000;pagination_ok=one workbook holds the whole year (single sheet)")
row("JO40_JPNAT_ANZEN_INJURY_DB_R01", "xlsx", "labour", "occupational-injury-case",
    "japan,mhlw,occupational-safety,workplace-accident,labour,anzeninfo",
    "https://anzeninfo.mhlw.go.jp/anzen_pgm/SHISYO_FND.html",
    "MHLW workplace safety site - occupational injury database, 2019 (annual file)",
    "職場のあんぜんサイト 労働災害（死傷）データベース 令和元年 年間データ",
    "https://anzeninfo.mhlw.go.jp/anzen/shisyo_xls/sisyou_db_r01.xlsx",
    "The annual 2019 file of the MHLW injury database: 34,795 sampled lost-time injury cases (link IDs 1..34,795) with era/year/month, hour band, accident narrative, industry, workplace size, causal agent, accident type and age. Header in the first sheet row; the second row is a code/name sub-header without a link ID. 連結ID is unique.",
    "interval=604800;xlsx_sheet_index=0;csv_skip_lines=0;title_keys=災害状況;id_keys=連結ID;timeout_ms=90000;pagination_ok=one workbook holds the whole year (single sheet)")
row("JO40_JPNAT_ANZEN_FATALITY_DB_R06", "xlsx", "labour", "occupational-fatality-case",
    "japan,mhlw,occupational-safety,workplace-fatality,labour,anzeninfo",
    "https://anzeninfo.mhlw.go.jp/anzen_pg/SIB_FND.html",
    "MHLW workplace safety site - occupational fatality database, 2024",
    "職場のあんぜんサイト 死亡災害データベース 令和6年",
    "https://anzeninfo.mhlw.go.jp/anzen/sib_xls/sibou_db_r06.xlsx",
    "Every 2024 workplace death in the MHLW fatal-accident database: 746 cases, each with ID, month, hour band, a detailed narrative of the fatal accident, industry (three levels), workplace size, causal agent (three levels) and accident type. Header is the first sheet row; the second is a code/name sub-header that reads as one extra record. ID is unique across the 746 cases.",
    "interval=604800;xlsx_sheet_index=0;csv_skip_lines=0;title_keys=災害状況;id_keys=ID;timeout_ms=60000;pagination_ok=one workbook holds the whole year (single sheet)")

# ---------------------------------------------------------------- Statistics Center: SSDSE-A (municipal indicators)
row("JO40_JPNAT_SSDSE_A_MUNICIPAL", "csv", "statistics", "municipal-indicator-set",
    "japan,statistics,nstac,ssdse,municipality,demographics,economy",
    "https://www.nstac.go.jp/use/literacy/ssdse/",
    "National Statistics Center - SSDSE-A standardized municipal dataset (2026 edition)",
    "教育用標準データセット SSDSE-A（市区町村）2026",
    "https://www.nstac.go.jp/files/SSDSE-A-2026.csv",
    "The National Statistics Center's standardized municipal dataset, 2026 edition: 1,741 municipalities (R01100 Sapporo .. R47382 Yonaguni) with 125 indicators each - population by sex and age band, foreigners, births, deaths, in- and out-migration, households, area, establishments and employees by industry, schools, hospitals, doctors, welfare facilities, finance and more, drawn from census, vital statistics and economic census tables. Shift_JIS; line 1 holds item codes, line 2 the reference year, line 3 the Japanese labels used as keys. 地域コード is unique over all 1,741.",
    "interval=604800;charset=shift_jis;csv_skip_lines=2;title_keys=市区町村;id_keys=地域コード;timeout_ms=60000;pagination_ok=one CSV holds every municipality")

# ---------------------------------------------------------------- MHLW: food poisoning cases 2025
row("JO40_JPNAT_FOOD_POISONING_CASES_2025", "xlsx", "health", "food-poisoning-incident",
    "japan,mhlw,food-safety,food-poisoning,outbreak,health",
    "https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/kenkou_iryou/shokuhin/syokuchu/04.html",
    "MHLW - food poisoning incidents reported in 2025 (case list)",
    "厚生労働省 令和7年（2025年）食中毒発生事例",
    "https://www.mhlw.go.jp/content/001681445.xlsx",
    "Every food-poisoning incident reported to MHLW for 2025: 1,176 incidents, each with the reporting prefecture or city, date of onset (Excel serial), place, causative food or meal, pathogen or agent (Campylobacter, norovirus, Anisakis, histamine...), facility type (restaurant, home, school), number who ate, number of patients and deaths. A banner row sits above the header. The full row is unique except two byte-identical duplicates.",
    "interval=604800;xlsx_sheet_index=0;csv_skip_lines=1;title_keys=原因施設+病因物質;id_keys=都道府県名等+発生月日+発生場所+原因食品+病因物質+原因施設+摂食者数+患者数+死者数;timeout_ms=60000;pagination_ok=one workbook holds the whole year (single sheet)")

# ---------------------------------------------------------------- MHLW: NHI drug price standard lists
for sid, n_ja, n_en, f, n in [
    ("ORAL", "内用薬", "oral drugs", "tp20260813-01_01", "7,064"),
    ("INJECTION", "注射薬", "injectable drugs", "tp20260813-01_02", "3,476"),
]:
    row("JO40_JPNAT_NHI_DRUG_PRICE_" + sid, "xlsx", "health", "listed-drug-price",
        "japan,mhlw,drug,pharmaceutical,nhi-price,registry",
        "https://www.mhlw.go.jp/topics/2026/04/tp20260401-01.html",
        "MHLW - National Health Insurance drug price standard list, %s (edition applied 2026-08-13)" % n_en,
        "薬価基準収載品目リスト（%s）令和8年8月13日適用" % n_ja,
        "https://www.mhlw.go.jp/topics/2026/04/xls/%s.xlsx" % f,
        ("Every %s listed in the NHI drug price standard as of the 2026-08-13 edition: %s products, each with the 12-character drug price code, "
         "ingredient name, strength and unit, pharmacopoeia and narcotic marks, product name, manufacturer, generic-drug flags for reimbursement add-ons, "
         "originator flags, the official price and any transitional expiry. The drug price code is unique over every row. Read by cell position, the header "
         "has three blank columns between strength and product name; records are titled by ingredient (成分名), because probe_hp_batch.py's xlsx reader shifts cells after a self-closing empty cell and does not see the 品名 header.") % (n_en, n),
        "interval=604800;xlsx_sheet_index=0;csv_skip_lines=0;title_keys=成分名;id_keys=薬価基準収載医薬品コード;timeout_ms=60000;pagination_ok=one workbook holds the whole list (sheet HP用)")

# ---------------------------------------------------------------- MLIT road traffic census 2021, per-section counts
for pid, pen, pja, code, n in [
    ("AICHI", "Aichi", "愛知県", "23", "4,684"),
    ("HOKKAIDO", "Hokkaido", "北海道", "01", "4,883"),
    ("FUKUOKA", "Fukuoka", "福岡県", "40", "4,373"),
    ("HYOGO", "Hyogo", "兵庫県", "28", "3,854"),
    ("NIIGATA", "Niigata", "新潟県", "15", "3,566"),
]:
    row("JO40_JPNAT_ROAD_TRAFFIC_CENSUS_" + pid, "csv", "infrastructure", "road-traffic-count-section",
        "japan,mlit,road,traffic,census,transport," + pen.lower(),
        "https://www.mlit.go.jp/road/census/r3/index.html",
        "MLIT 2021 road traffic census - per-section traffic volumes, %s" % pen,
        "令和3年度 全国道路・街路交通情勢調査 一般交通量調査 箇所別基本表（%s）" % pja,
        "https://www.mlit.go.jp/road/census/r3/data/csv/kasyo%s.csv" % code,
        ("The 2021 national road traffic census section table for %s: %s road sections, each with the survey section number, road class, route number and name, "
         "start and end connections, municipality code, length, expressway and managed-section flags, and per direction the observation point name, observation date, "
         "12-hour and 24-hour counts of small and large vehicles, day/night ratio, peak and heavy-vehicle ratios, congestion degree, peak and off-peak travel speeds, lane and footway widths, signal density and the 2015 comparison values - 159 columns, the last six repeating the section number and route name. Shift_JIS. The section number is unique over every row.")
        % (pen, n),
        "interval=604800;charset=shift_jis;title_keys=路線名+上り／交通量観測地点地名;id_keys=交通調査基本区間番号;timeout_ms=60000;pagination_ok=one CSV holds the prefecture's whole table")

# ---------------------------------------------------------------- NII / ROIS-DS CODH Geoshape historical-geography datasets
GS = "https://geoshape.ex.nii.ac.jp/"
row("JO40_JPNAT_GEOSHAPE_HISTORICAL_PLACE_NAMES", "csv", "geo", "historical-place-name",
    "japan,nii,codh,history,place-name,gazetteer,geo",
    GS + "nrct/",
    "NII/CODH Geoshape - Nihon Rekishi Chimei Taikei place-name entries dataset",
    "『日本歴史地名大系』地名項目データセット",
    GS + "nrct/dataset/nrct-20250719.csv",
    "The place-name entry dataset built by the NII and ROIS-DS CODH from Heibonsha's Nihon Rekishi Chimei Taikei, edition 2025-07-19: 80,502 historical place names (Edo-period villages, towns, post stations, districts) each with a 12-digit id, prefecture code, name and reading, the modern municipality above it, the source address, estimated latitude and longitude with the estimation method, a link to the historical place-name id and a GeoLOD id. UTF-8 with BOM. id is unique over all 80,502 rows.",
    "interval=604800;title_keys=名称;id_keys=id;lat_key=緯度;lon_key=経度;timeout_ms=90000;pagination_ok=one CSV holds the whole dataset")
row("JO40_JPNAT_GEOSHAPE_HISTORICAL_SITES", "csv", "geo", "historical-site",
    "japan,nii,codh,history,heritage,poi,geo",
    GS + "nrct-poi/",
    "NII/CODH Geoshape - Nihon Rekishi Chimei Taikei facility and site entries dataset",
    "『日本歴史地名大系』施設・地点項目データセット",
    GS + "nrct-poi/dataset/nrct-poi-20250515.csv",
    "The facility and site companion to the place-name dataset, edition 2025-05-15: 32,038 entries - castles, temples and shrines, barrier stations, historic offices, ruins, archaeological sites - each with id, prefecture code, name, reading, search string, address, latitude and longitude and a three-level classification (e.g. 施設 / 史跡・文化財・遺跡). UTF-8 with BOM. id is unique over all 32,038 rows.",
    "interval=604800;title_keys=名称;id_keys=id;lat_key=緯度;lon_key=経度;timeout_ms=90000;pagination_ok=one CSV holds the whole dataset")
row("JO40_JPNAT_GEOSHAPE_EDO_VILLAGES", "csv", "geo", "edo-period-village",
    "japan,nii,codh,history,edo,village,geo",
    GS + "edo/village/",
    "NII/CODH Geoshape - integrated Edo-period village dataset",
    "江戸近世村統合データセット",
    GS + "edo/village/dataset/edo-village-20250917.csv",
    "The integrated Edo-period village dataset (edition 2025-09-17) linking the Rekishi Chimei Taikei entries to the late-Edo village territory data: 56,589 villages, each with the JK_ID, the NRCT name, reading and coordinates and the matched early-modern village id, name, reading and coordinates. UTF-8 with BOM. JK_ID is unique over all 56,589 rows.",
    "interval=604800;title_keys=NRCT名称,近世村名称;id_keys=JK_ID;lat_key=NRCT緯度;lon_key=NRCT経度;timeout_ms=90000;pagination_ok=one CSV holds the whole dataset")
row("JO40_JPNAT_GEOSHAPE_HISTORICAL_MUNICIPALITIES", "csv", "geo", "historical-municipality",
    "japan,nii,codh,history,municipality,administrative-boundary",
    GS + "city/dataset/",
    "NII/CODH Geoshape - historical administrative areas, municipality ID list",
    "歴史的行政区域データセットβ版 市区町村ID一覧",
    GS + "city/dataset/geoshape_city_id.csv",
    "The municipality identifier list behind the historical administrative-area dataset (municipalities that existed from the 1889 municipal system to the present): 16,756 geoshape_city_id values such as 01100A1972, each with prefecture, county or designated-city name, municipality name, suffix and full address. UTF-8 with BOM. geoshape_city_id is unique over all rows.",
    "interval=604800;title_keys=住所;id_keys=geoshape_city_id;timeout_ms=60000;pagination_ok=one CSV holds the whole list")

# ---------------------------------------------------------------- MOE Biodiversity Center: Red List
row("JO40_JPNAT_MOE_REDLIST_2025_VASCULAR_PLANTS", "csv", "environment", "red-list-taxon",
    "japan,moe,biodiversity,red-list,endangered-species,plants",
    "https://ikilog.biodic.go.jp/Rdb/booklist",
    "Ministry of the Environment Red List 2025 - vascular plants",
    "環境省レッドリスト2025 維管束植物",
    "https://ikilog.biodic.go.jp/rdbdata/files/redlist2025/redlist2025_ikansoku.csv",
    "The Ministry of the Environment's 5th Red List for vascular plants (2025 edition): 2,222 taxa, each with its category (EX, EW, CR, EN, VU, NT, DD and the threatened local-population class), taxonomic group, Japanese name and scientific name. Shift_JIS. The scientific name is unique over all 2,222 rows.",
    "interval=604800;charset=shift_jis;title_keys=和名;id_keys=学名;timeout_ms=60000;pagination_ok=one CSV holds the whole list")
row("JO40_JPNAT_MOE_RDB_PLANT_DISTRIBUTION_MESH", "csv", "environment", "threatened-plant-occurrence",
    "japan,moe,biodiversity,red-data-book,plants,distribution,mesh",
    "https://ikilog.biodic.go.jp/Rdb/booklist",
    "Ministry of the Environment - threatened vascular plant distribution records by map mesh",
    "絶滅危惧種（維管束植物）分布情報 公開種全体",
    "https://ikilog.biodic.go.jp/rdbdata/files/va_2007.csv",
    "The published distribution records for threatened vascular plants from the Red Data Book survey (2000 RDB and 2007 Red List ranks): 13,498 records of species by prefecture, 1:25,000 map sheet and secondary mesh code, with species code, family, Japanese and scientific name, both ranks and the population status (extant, unknown, extinct). Shift_JIS. Species code+prefecture+mesh+map sheet+status+ranks is unique for 13,338 rows; the other 160 are byte-identical repeats.",
    "interval=604800;charset=shift_jis;title_keys=和名;id_keys=種コード+都道府県コード+メッシュコード+2万5千地形図名+生育状況+平成12年レッドデータブック刊行時のランク+平成19年レッドリスト選定時のランク;timeout_ms=60000;pagination_ok=one CSV holds the whole dataset")

# ---------------------------------------------------------------- Japan Link Center DOI prefixes
row("JO40_JPNAT_JALC_DOI_PREFIXES", "json", "science", "doi-prefix",
    "japan,jalc,jst,ndl,nii,doi,research-infrastructure,registry",
    "https://japanlinkcenter.org/top/",
    "Japan Link Center - every DOI prefix handled through JaLC",
    "ジャパンリンクセンター プレフィックス一覧",
    "https://api.japanlinkcenter.org/prefixes",
    "The JaLC REST API prefix list: 4,092 DOI prefixes (3,437 registered with JaLC as RA, 641 Crossref, 14 DataCite), each with the prefix, registration agency, the content site id it belongs to (107 sites, e.g. SI/JST.JSTAGE) and the last update date (newest 2026-09-09). The API returns everything in one page (totalPages=1). prefix is unique.",
    "interval=604800;array_path=data.items;id_keys=prefix;title_keys=prefix;date_keys=updated_date;timeout_ms=60000;pagination_ok=totalPages=1 - the whole list in one response")

# ---------------------------------------------------------------- MOFA overseas safety open data
row("JO40_JPNAT_MOFA_OVERSEAS_SAFETY_ALL", "xml", "government", "overseas-safety-notice",
    "japan,mofa,travel-advisory,consular,security,overseas-safety",
    "https://www.anzen.mofa.go.jp/",
    "MOFA overseas safety open data - every consular e-mail, spot, wide-area and risk notice (all regions)",
    "外務省 海外安全情報オープンデータ すべての地域（全情報）",
    "https://www.ezairyu.mofa.go.jp/opendata/area/00A.xml",
    "The Ministry of Foreign Affairs' overseas-safety open data for all regions in its full form (lastModified 2026-10-02, 18.9 MB): mailCount=4,221 notices dated 2012-03-21 to 2026-10-02 - 3,967 general and 45 emergency consular e-mails, 136 risk-level notices, 31+2 spot and 28+11 wide-area alerts (incl. infectious disease). Each mail carries keyCd, type code and name, issue time, area and country, title, lead, main text (3,932 have it), the anzen.mofa.go.jp detail URL, per-mail XML URLs and the issuing embassy or consulate (e.g. 在ドバイ日本国総領事館, 105 notices). keyCd is unique over all 4,221.",
    "interval=3600;array_path=mail;id_keys=keyCd;title_keys=title;date_keys=leaveDate;link_keys=infoUrl;timeout_ms=90000;pagination_ok=one XML document holds every current notice for all regions")

# ---------------------------------------------------------------- ROIS-DS CODH: Edo map place names
row("JO40_JPNAT_CODH_EDO_MAP_PLACES", "csv", "geo", "edo-map-place",
    "japan,codh,rois,history,edo,tokyo,place-name,geo",
    "https://codh.rois.ac.jp/edo-maps/dataset/",
    "ROIS-DS CODH - Edo map place-name dataset (Owariya-edition kiriezu)",
    "江戸マップ地名データセット（尾張屋版江戸切絵図）",
    "https://codh.rois.ac.jp/edo-maps/dataset/owariya.csv",
    "Every place name transcribed from the Owariya edition of the Edo kiriezu ward maps by ROIS-DS CODH: 8,788 entries (2,777 town and village names, 2,053 temples and shrines, 1,646 samurai residences, 1,325 facilities, 819 place names, 54 shops), each with entry id, name with prefix and suffix, class, georeferenced latitude and longitude (present on all 8,788), the source sheet (e.g. 御江戸大名小路絵図), variants and a link to the entry page. UTF-8 with BOM. entry_id is unique.",
    "interval=604800;title_keys=body;id_keys=entry_id;lat_key=latitude;lon_key=longitude;link_keys=source;timeout_ms=60000;pagination_ok=one CSV holds the whole dataset")

# ---------------------------------------------------------------- e-Gov public comment: result notices
row("JO40_JPNAT_EGOV_PUBLIC_COMMENT_RESULTS", "xml", "government", "public-comment-result",
    "japan,e-gov,public-comment,regulation,rulemaking,government",
    "https://public-comment.e-gov.go.jp/",
    "e-Gov public comment - published results of consultations (all ministries)",
    "e-Govパブリック・コメント 結果公示案件一覧",
    "https://public-comment.e-gov.go.jp/rss/pcm_result.xml",
    "The e-Gov RSS 1.0 feed of public-comment results published by every ministry and agency: 65 items covering 2026-09-28 to 2026-09-30 when fetched (about 20 a day), each with the case title (the draft rule or notice consulted on), the case link with its id, a description and dc:date. The case list feed (pcm_list.xml) is already registered; this is the separate result feed. link is unique over the 65 items.",
    "interval=21600;array_path=item;title_keys=title;id_keys=link;date_keys=dc:date;link_keys=link;timeout_ms=60000;pagination_ok=RSS window of the latest results - no paging offered")
