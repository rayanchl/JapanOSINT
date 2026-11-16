# fix_src report (2026-11-14). All fixes verified by curl + reasoning only (no rebuild). Every edited file passes `cc -fsyntax-only`.

## SRC-01
- bird-flu-outbreaks (collectors/sources/bird_flu_outbreaks.c): index 200 58,957 B links r8_hpai_kokunai.html (newest). r8 (200, 40,255 B) is a brand-new 令和8 season page: "更新日 令和8年11月13日", cases only as PDFs, zero "例目". r7 page has 24 "例目". Collector stopped at the highest season. FIX: walk down seasons (max 3) until one yields cases; if bodies fetched and none parse -> return -1 (disclosed).
- real-estate (collectors/sources/real_estate.c): `ta=tokyo` slug now answered 200, 12,438 B, `<title>エラー｜SUUMO`. `ta=13&ar=030` returns 463 KB / 30 cassetteitem_content-title per page; title/addr/rent/madori/href selectors still match. FIX: slugs -> (ar,ta) pairs measured per prefecture (tokyo 030/13, kanagawa 030/14, osaka 060/27, aichi 050/23, hokkaido 010/01, fukuoka 090/40, hyogo 060/28, chiba 030/12, saitama 030/11, kyoto 060/26, hiroshima 080/34 (070 is empty), miyagi 020/04, shizuoka 050/22); all 30/page. Returns -1 if bodies fetched and zero listings. Note: now up to 65 real requests/run (~30 MB) vs 13 before.
- phone-scam-hotspots (collectors/sources/phone_scam_hotspots.c): 9 of 10 hard-coded prefectural paths are 404 (Osaka redirects to site root, Aichi redirect loop); feed_get_text returned error bodies, so 227 KB/12 requests parsed to nothing. Data still exists on the NPA SOS47 page (200, 72 KB): "期間：2025/1～2025/12 認知件数合計 43,000 件 合計被害額 約3,257 億 4,258 万 7,000 円 被害件数上位… 1位 東京 5,879 件 …". REWRITTEN to parse that block: 1 national record (uid phone-scam:npa-sos47:national) + up to 10 ranked prefecture records (uid ...:pref:<JIS>) at prefecture centroids, geo_precision stated. -1 if page fetched and block unparsed. NOTE: record identity/shape changes (old rows never stored anything). Parser traced against real html_strip output (built a tiny harness on lib/htmlparse.c).

## SRC-02 (law.go.kr, 215 B, HTTP 200)
Body: {"result":"사용자 정보 검증에 실패하였습니다.","msg":"OPEN API 호출 시 사용자 검증을 위하여 정확한 서버장비의 IP주소 및 도메인주소를 등록해 주세요."} -> OC=test is not a registered user id; needs a real OC and registered server IP. FIX: key_env="KR_LAW_GO_KR_OC", URLs use OC={key} (also detail_url) in hp17_kr_courts_1.c (kr-constitutional-court-decisions, kr-supreme-court-precedents) and hp17_kr_legal_1.c (kr-local-ordinances). kr-treaties was a VJSON row (no gate possible): moved into hp17_kr_legal_1.c as an hpengine row with same url/array_path/default keys, and the VJSON block removed from vsrc17_kr_legal_1.c (now comment-only). Without the env var all four now file a needs-credential (GATED) notice. Cannot be verified without a registered OC.
OTHER COPIES (not touched, outside scope): 14 more rows carry OC=test: hp3b19_apac.c (admrul, licbyl, lstrm, oldAndNew, admbyl), hp17_kr_legal_1.c (decc, elaw, expc), vsrc_legal_2.c (law). Same fix applies.

## SRC-03 ifrc-go-appeal-document (no code change; comment updated in vsrc17_int_disaster_1.c)
Walked all 8 pages: count=7,708; 5,811 distinct ids; 5,811 distinct whole records; 0 ids with differing bodies. 7,708/5,811 is the upstream's byte-identical repeats. Leave.

## SRC-04
- arxiv-cs-ro, arxiv-cond-mat-mtrl-sci: valid RSS, today's (Sat 14 Nov) lastBuildDate, skipDays Sat/Sun, zero items. Honest empty. Leave.
- us-usaspending-federal-account-detail: 200, 499 B, account 020-0550 has "children":[] (FY2027). Honest empty; a frozen single account. Leave.
- es-opendata-aragon-...-zar: package_search count 0; organization_show package_count=0 (org exists, no public datasets). Honest empty. Leave.
- JO47_FR_D507DF73...: archives.meurthe-et-moselle.fr/rss.xml answers 200, 229 B JS+cookie redirect page ("requires JS enabled and cookies"; redirect target 403 with curl). A bot wall, undisclosed on the run (rc=0 records=0, no notice) -> hpengine HP_XML gap (lib), not row-fixable. Left; flag to lib owners.
- JO53_AGS_SERVICES_FREIGHT_ROUTES_CFEC: 200 with {"error":{"code":499,"Item does not exist or is inaccessible"}}; engine logs it but stores nothing at rc=0 (lib gap: error document should file fetch-failure). Service 05282025_ODOT_CFEC_MM_Ashland_Freight no longer listed in org uUvqNMGPm7axC2dD; a different `ODOT_CFEC_MM_Ashland_Freight_FinalData` exists but re-pointing to a different dataset is unmeasured; not changed.
- JO57_Q772103 (Takahata): feed has empty rdf:Seq (1,149 B); run already files a shape notice (stored=1 notices=1). Honest empty, disclosed. Leave.

## COLLISIONS
- JO63_EUODS_D151D660E1: REAL LOSS, fixed. No id_keys, uid fell to first scalar (doc_titre); full export 23,488 records, 21,840 distinct titles; no 1-3 field key unique. Added 9-field composite id_keys (doc_titre+doc_cote+doc_source+doc_dee+doc_per_dpar+doc_sup+doc_an_edit+doc_de+doc_dl): 2,000/2,000 distinct on windows at offsets 0/2000/10000/21000, 23,487/23,488 whole. Offset walk is unordered (no unique sort key), reads arbitrary 2,000 of 23,488.
- WD_TELECOM_OPERATORS: 1,238 rows, 1,081 item ids, 1,136 distinct whole rows (stored 1,141). The 97 are byte-identical SPARQL cross-product repeats (OPTIONAL joins); distinct-inception rows are kept. Leave.
- JO32_JPS_DIGNL: Japan Search jps-cross, documented reshuffling/2000-cap family; id is correct. Leave.
- JO39_CN_CZCE_DAILY: row description already documents 4 byte-identical zero-activity subtotal lines. Leave.
- JO47_FR_783FC934: feed has 143 items, no guid, 137 distinct links, 137 distinct whole items: 6 byte-identical repeats. Leave.
- chiriin-place, gr-diavgeia-positions: skipped as instructed.

## RE-RUN AFTER REBUILD
bird-flu-outbreaks real-estate phone-scam-hotspots kr-constitutional-court-decisions kr-supreme-court-precedents kr-local-ordinances kr-treaties JO63_EUODS_D151D660E1
(expect: first three store records; kr-* four report GATED notice without KR_LAW_GO_KR_OC; JO63 stored ~=emitted.)
