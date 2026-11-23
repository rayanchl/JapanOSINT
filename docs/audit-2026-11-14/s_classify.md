# Collector classification at row level (s_classify)

Branch worktree-agent-a549f8e0fba5d878c. Merged claude/peaceful-ritchie-i7e0hj twice (once for the base correction, once at the end); both clean. `core/collectors.def` regenerated with `make collectors-def` (745 collectors, 23 groups, 0 unassigned).

## Part A

Tool: `native/tools/split_beats_by_theme.py` (sibling of split_beats_by_body_type.py: plan, apply, manifest, summary, gov-plan, gov-apply). Only `.collector` changed (plus `.category` -> `commercial` for the 112 IGO company rows); ids, URLs, keys and paging untouched. 25,853 rows relabelled. `--list-sources` id set identical before/after (105,228), and a line diff that ignores the collector field is empty.

Included beyond the task list, same mixed ArcGIS/WFS kind: hp3b64_derest (851, German ArcGIS layers) and hp3b63_euwfs (591, EU WFS). hp3b63-67 CKAN/ODS/Socrata beats are dataset-portal beats, left alone.

### Rules
ArcGIS/WFS layers get `<region>_gis_<theme>`:
- region from the file (us/eu/jp); in the unattributed hp3b46/53 files from the publisher name (JP/US/EU markers) else `world`; hp3b58_arcgis by its eu/us tag.
- theme, a closed set of 14, by ordered regexes over the layer name (camelCase and underscore split; English, Japanese kanji and romaji, Dutch, French, German, Iberian, Italian, Nordic, Czech/Polish/Finnish), first match wins. Order: JP planning terms, cooling/health, imagery_basemap, elections_admin, utilities_infrastructure, safety_emergency, environment_hazards, health_social, transport, education, culture_recreation, planning_zoning, parcels_landuse, boundaries, economy_business. Second pass: the service name from the URL, only for specific themes (a "Community" service says nothing). Third: the row category, only where specific (transport, safety, environment, education, health, culture). Else `other`, never a guess.
- hp3b44_usgov: the .gov registry type written in each name (City, County, Special/School district, State/territory/Interstate, Tribal, Federal, "- Election").
- hp3b47_wdgov: the "(type, CC)" token (ministry, legislature, court, central-bank, regulatory-agency, police), then name rules (universities/research, police/security, health/education, economy/infrastructure, ministries, regulators, local/regional); residue `wdgov_agencies`.
- hp3b48_igo: the Wikidata class in the description plus name rules: intergovernmental, associations, companies/blogs, residue other.
- `government` collector (macro-form VRSS/VJSON/RSSX rows; the COLL argument is rewritten, 1,379 of 1,505 rows): by id prefix and name: uk_gov_* (local, departments, regulators, agencies_bodies, inquiries_reviews, justice_police, health_social, other), jp_gov_cities/prefectures/tokyo, cn_gov_hongkong/macao/other, world_gov_*, us_gov_agencies. OpenSanctions rows go to the existing `sanctions`, openFDA to `us_health`, France24/SCMP feeds (filed as government) to `world_outlets`.

### Per-collector counts (rows relabelled)
- eu_gis_environment_hazards: 2223
- us_gis_environment_hazards: 1770
- eu_gis_other: 1636
- eu_gis_boundaries: 1291
- usgov_cities: 1197
- us_gis_transport: 1002
- us_gis_utilities_infrastructure: 885
- us_gis_elections_admin: 783
- jp_gis_safety_emergency: 751
- us_gis_other: 742
- us_gis_parcels_landuse: 717
- us_gis_boundaries: 707
- eu_gis_transport: 685
- jp_gis_environment_hazards: 618
- us_gis_culture_recreation: 547
- wdgov_agencies: 501
- us_gis_safety_emergency: 466
- jp_gis_transport: 449
- us_gis_planning_zoning: 426
- eu_gis_utilities_infrastructure: 396
- eu_gis_planning_zoning: 381
- eu_gis_culture_recreation: 379
- jp_gis_other: 368
- eu_gis_parcels_landuse: 360
- jp_gis_planning_zoning: 321
- us_gis_imagery_basemap: 312
- usgov_counties: 294
- uk_gov_agencies_bodies: 276
- eu_gis_imagery_basemap: 264
- jp_gis_parcels_landuse: 252
- igo_associations: 226
- wdgov_universities_research: 205
- wdgov_health_education: 178
- jp_gis_boundaries: 174
- usgov_special_districts: 173
- wdgov_economy_infrastructure: 172
- uk_gov_other: 171
- wdgov_regulators_central_banks: 170
- jp_gis_health_social: 160
- eu_gis_safety_emergency: 155
- jp_gis_utilities_infrastructure: 149
- eu_gis_elections_admin: 148
- wdgov_ministries: 140
- uk_gov_regulators: 140
- uk_gov_inquiries_reviews: 133
- us_gis_education: 129
- usgov_states: 122
- usgov_federal: 121
- jp_gis_culture_recreation: 116
- igo_companies_blogs: 112
- eu_gis_health_social: 107
- usgov_elections: 106
- jp_gov_tokyo: 88
- jp_gov_cities: 87
- igo_intergovernmental: 86
- world_gov_agencies: 85
- wdgov_legislatures_courts: 75
- igo_other: 70
- eu_gis_education: 64
- uk_gov_justice_police: 63
- eu_gis_economy_business: 63
- wdgov_local_regional: 61
- usgov_tribes: 58
- uk_gov_local: 58
- wdgov_police_security: 57
- us_gis_health_social: 50
- uk_gov_departments: 46
- world_gis_other: 44
- uk_gov_health_social: 40
- jp_gov_prefectures: 40
- jp_gis_education: 39
- cn_gov_hongkong: 39
- jp_gis_imagery_basemap: 35
- world_gov_data_portals: 29
- world_gis_environment_hazards: 24
- world_gis_transport: 23
- us_gis_economy_business: 23
- world_gis_boundaries: 22
- jp_gis_economy_business: 22
- world_outlets: 16
- jp_gis_elections_admin: 15
- world_gov_eu_bodies: 14
- world_gov_intergovernmental: 12
- world_gis_utilities_infrastructure: 12
- sanctions: 12
- cn_gov_other: 12
- world_gis_planning_zoning: 11
- world_gis_safety_emergency: 7
- world_gis_economy_business: 6
- us_health: 6
- world_gov_statistics: 5
- world_gis_parcels_landuse: 5
- world_gis_imagery_basemap: 5
- world_gis_culture_recreation: 5
- cn_gov_macao: 5
- world_gis_health_social: 2
- world_gis_education: 2
- us_gov_agencies: 2
- world_gis_elections_admin: 1
- usgov_other: 1
- -: 1

### `other` buckets (GIS)
| region | rows | other | share |
|---|---|---|---|
| us | 8,559 | 742 | 8.7% |
| jp | 3,469 | 368 | 10.6% |
| eu | 8,152 | 1,636 | 20.1% |
| world | 169 | 44 | 26.0% |

US and JP are under 15%. EU (20.1%) and world (26%) are not. The sampled remainder is opaque short municipal layer codes and ID-like names in Swedish, Norwegian, Dutch, Slovak and Galician ("Kevershall", "Tehelna_vrstva", "BOZIT_AppBO", "CE_CAT1") and anonymous services.arcgis.com hosts with names such as "2012_FIRS". The rules were iterated four times and each pass moved a few percent; this is the honest residue, not a gap I can close by name. Precision was spot-checked on about 20 rows per large theme: most hits are right, with a minority of misfiles (a "Zonas_Inundables" layer filed as boundaries). Named residues elsewhere: `wdgov_agencies` 501 (32% of wdgov), `uk_gov_other` 171, `igo_other` 70.

### A4 / A5
`gen_collectors_def.py` derives label and group for every `<region>_gis_<theme>` key from one theme table (transport->grp_transport, utilities->grp_infrastructure, environment_hazards->grp_environment, safety->grp_safety, education->grp_education, health->grp_health, culture->grp_culture, economy->grp_corporate, elections->grp_government, boundaries/parcels/planning/imagery/other->grp_geo) plus a BODY_COLLECTORS table for the body-type keys. `gen_hp_batch.py`: `collector` was already in STR_OPTS; it is now documented in the docstring and `--auto-collector gis --region us|eu|jp|world` derives the key from the layer name (tested on the batch-46 manifest, 164 rows). Shipped tables were not regenerated. No separate ArcGIS manifest builder exists in tools/ (grep).

### Group table (before = audit-branch generator on the pre-split census; after = final; 105,228 sources)
| group | label | collectors before | sources before | % before | collectors after | sources after | % after |
|---|---|---|---|---|---|---|---|
| grp_opendata | Open data portals & statistics | 86 | 25862 | 24.6 | 88 | 25896 | 24.6 |
| grp_government | Government & public administration | 127 | 24638 | 23.4 | 157 | 24219 | 23.0 |
| grp_geo | Geospatial & mapping | 30 | 20652 | 19.6 | 37 | 8376 | 8.0 |
| grp_media | News & media | 28 | 7573 | 7.2 | 28 | 7589 | 7.2 |
| grp_environment | Environment, climate & hazards | 36 | 1398 | 1.3 | 40 | 6033 | 5.7 |
| grp_corporate | Companies, finance & markets | 76 | 5032 | 4.8 | 82 | 5428 | 5.2 |
| grp_health | Health, food & agriculture | 30 | 3921 | 3.7 | 36 | 4464 | 4.2 |
| grp_science | Science, research & academia | 27 | 3697 | 3.5 | 28 | 3902 | 3.7 |
| grp_education | Education & schools | 3 | 2733 | 2.6 | 7 | 2967 | 2.8 |
| grp_transport | Transport, aviation & maritime | 17 | 593 | 0.6 | 21 | 2752 | 2.6 |
| grp_infrastructure | Infrastructure, energy & utilities | 26 | 1117 | 1.1 | 31 | 2731 | 2.6 |
| grp_culture | Culture, heritage, libraries & archives | 11 | 1543 | 1.5 | 15 | 2590 | 2.5 |
| grp_safety | Crime, policing, defense & conflict | 18 | 901 | 0.9 | 24 | 2400 | 2.3 |
| grp_legal | Law, courts, legislation & IP | 33 | 2032 | 1.9 | 34 | 2107 | 2.0 |
| grp_cyber | Cyber threat intelligence & hacking | 20 | 1230 | 1.2 | 20 | 1230 | 1.2 |
| grp_social | Social, web & communities | 10 | 926 | 0.9 | 11 | 1152 | 1.1 |
| grp_osint | Investigation pivots & reference | 10 | 322 | 0.3 | 10 | 322 | 0.3 |
| grp_surveillance | Cameras, sensors & exposure | 8 | 313 | 0.3 | 8 | 313 | 0.3 |
| grp_telecom | Telecom, networks & signals | 9 | 270 | 0.3 | 9 | 270 | 0.3 |
| grp_space | Space & satellites | 4 | 208 | 0.2 | 4 | 208 | 0.2 |
| grp_procurement | Procurement, aid & public spending | 48 | 187 | 0.2 | 48 | 187 | 0.2 |
| grp_sanctions | Sanctions & compliance | 4 | 61 | 0.1 | 4 | 73 | 0.1 |
| grp_internal | Internal pods & tools | 3 | 19 | 0.0 | 3 | 19 | 0.0 |

Geospatial 19.6% -> 8.0% (20,652 -> 8,376 sources); Environment 1.3% -> 5.7%, Transport 0.6% -> 2.6%, Infrastructure 1.1% -> 2.6%, Safety 0.9% -> 2.3%, Health 3.7% -> 4.2%, Education 2.6% -> 2.8%. Government 23.4% -> 23.0%: the rest is genuinely government feeds (.gov, municipal, UK, JP, FR, IT).

## Part B

### SRC-02 (law.go.kr OC=test)
The 12 `OC=test` occurrences were 9 rows: hp3b19_apac (5: admrul, licbyl, lstrm, oldAndNew, admbyl), hp17_kr_legal_1 (3: decc, elaw, expc) and the VJSON `gov-kr-law-search` (vsrc_legal_2.c). All are `OC={key}` with `key_env = "KR_LAW_GO_KR_OC"` (detail_url too). The VJSON row has no credential gate, so it moved into hp17_kr_legal_1.c as an hpengine row with the same id, url, array_path and interval (collector `legal`, `page_walk=1` to keep VJSON's paging); the VJSON block was removed. Not verifiable without a registered OC; without the variable each row files the GATED notice.

### SRC-05 slow-source re-run
`python3 tools/audit_registry_emit.py --bin ./bin/japanosint --ids-file slow_ids.txt --jobs 4 --timeout 400 --out slow_rerun.tsv` over the 92 SLOW ids of docs/audit-2026-11-14/sweep.tsv. Full table: /tmp/claude-0/-home-user-JapanOSINT/c20195ab-72c3-5bbf-bc8d-73ece67936f3/scratchpad/slow_rerun.tsv.

| verdict | rows | reading |
|---|---|---|
| OK (just slow) | 45 | 6.7 to 338.5 s, median 127 s |
| COLLISION | 20 | all tiny: 1 to 14 of 411 to 26,805 records (dportal-iati-*, af-dportal-act-*, af-pmg-*), the upstream's repeats; slow, not broken |
| still SLOW | 19 | 14 died with "no [sched] line, killed before the run returned" at 44 to 328 s, below the 400 s line (af-dportal-trans-bw/et/gh/ke/na/ng/rw/tz/ug/za/zm, JO32_ARC_HYOGO_ACC, PACKAGIST_ALL_PACKAGE_NAMES, socrata-sf-business-registrations; a process kill, not the timeout); 5 hit the 400 s line with rows already stored: CAIDA_ASRANK_ORG 36,000, JP25_JPARCH_KURENAI_OAI 54,000, JO34_FDSNEVT_SERVICE_SCEDC_CALTECH_EDU 32,000, JO40_CNMKT_CSRC_LICENSING_RESULTS 8,040, hk-legco-hansard-speeches 9,400 |
| FETCH_FAILED | 8 | il-knesset-committee-session (HTTP 481); five Overpass rows (data-centers, fish-markets, museums, public-cameras, wineries-craftbeer: 0 of 4 hosts answered, transport timeout from this container); google-my-maps (TLS handshake refused); hazard-map-portal (hosts silent) |

Unmeasured, not failed: the still-SLOW rows walk long collections. Nothing was changed on these readings; the page-heavy rows with no page_resume are listed here, not edited.

### ARC-03 geojson-file
`collectors/pod/geojson_file.c` is wrapped in `#ifdef JO_TEST_PODS`. Nothing in tests/ or tools/ references it, so no define was added. `tools/lint_sources.py` `strip_comments` drops `#ifdef JO_TEST_PODS ... #endif` blocks so `make source-count` agrees with the binary. Floors re-recorded 105,229 -> 105,228 (exactly the one fixture), binary `--list-sources` agreeing.

## Verification (from native/, JO_DB on scratch files)
- `make -j8`: 0 warnings, after both merges.
- `--list-sources` id set before/after Part A identical (105,228; the only difference from the audit-branch tip is geojson-file gone); collector differs on exactly 25,853 rows.
- `make collectors-def` then rebuild; `make collectors-def-check`: every registered collector has a def line (745 collectors, 0 unassigned).
- `make lint-sources`: OK. `make source-floor`: 105228 >= 105228. `make registry-floor`: 105228 >= 105228, 0 duplicates, 0 dropped, 0 never-runs.
- `make audit-sources`: strict set 0 findings; whole tree 1 finding, `collectors/sources/bird_flu_outbreaks.c` record-cap, a file I did not touch.
- test_sourceapi alone (`JO_UNIT_ONLY='^test_sourceapi$' bash tests/unit/run.sh`): "test_sourceapi: all passed" (run before the second merge).
- End-to-end `--run` of 6 relabelled rows, all rc=0 with stored == records: JO61_USGIS_722F1DA656 (us_gis_transport, 112), JO59_JPGIS_6DE743A426 (jp_gis_other, 2), JO62_EUGIS_F8C5A6526C (eu_gis_utilities_infrastructure, 2,988), JO44_US_ABSECONNJ_GOV (usgov_cities, 10), JO48_IGO_Q677691 (igo_associations, 10), uk-b50-braintree-gov-uk-news (uk_gov_local, 20).

## Not done
- Manifests (docs/candidate-sources-batch*.txt) not mirrored; the C is the maintained copy.
- 126 `government` rows whose collector comes from a non-macro form (CKAN, ODS, Socrata, hand-written) keep `government`.
- EU and world GIS `other` stay above 15% (see above).
- test_sourceapi and `make unit` not re-run after the final merge.
