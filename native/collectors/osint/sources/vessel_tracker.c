/* collectors/osint/sources/vessel_tracker.c
 * OSINT service — port of OSINTsaas osint_tools/vessel_tracker.c (vessel_track
 * → handle_vessel_tracker). Canonical SERVICE = VESSEL_TRACKER (dispatcher row
 * {SERVICE_VESSEL_TRACKER, handle_vessel_tracker, "VESSEL_TRACKER", true};
 * MARITIME_DATABASE aliases same handler — VESSEL_TRACKER is first/dedup name).
 * On-demand (interval 0); ctx->entity = an IMO / MMSI / vessel name. Pure
 * compute (no network, no key): faithfully reproduces analyze_imo (7-digit IMO
 * + check-digit), analyze_mmsi (9-digit MMSI: type from first digit, flag
 * country via MID ranges, MID) and the vessel-name resource fallback. The C
 * builder produced items {"IMO Analysis"|"MMSI Analysis"|"MarineTraffic"};
 * here the single item's data object is the envelope payload. confidence 75.
 * Emits ONE osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct { int s, e; const char *c; } mid_range_t;
static const mid_range_t MID_CODES[] = {
  {201,201,"Albania"},{211,211,"Germany"},{212,212,"Cyprus"},{213,213,"Georgia"},
  {214,214,"Moldova"},{215,215,"Malta"},{216,216,"Armenia"},{218,218,"Germany"},
  {219,219,"Denmark"},{220,220,"Denmark"},{224,224,"Spain"},{225,225,"Spain"},
  {226,226,"France"},{227,227,"France"},{228,228,"France"},{230,230,"Finland"},
  {231,231,"Faroe Islands"},{232,234,"United Kingdom"},{235,235,"United Kingdom"},
  {236,236,"Gibraltar"},{237,237,"Greece"},{238,238,"Croatia"},{239,239,"Greece"},
  {240,240,"Greece"},{241,241,"Greece"},{242,242,"Morocco"},{243,243,"Hungary"},
  {244,245,"Netherlands"},{246,246,"Netherlands"},{247,247,"Italy"},{248,249,"Malta"},
  {250,250,"Ireland"},{251,251,"Iceland"},{256,256,"Monaco"},{257,257,"Norway"},
  {258,258,"Norway"},{259,259,"Norway"},{261,261,"Poland"},{263,263,"Portugal"},
  {265,265,"Sweden"},{266,266,"Sweden"},{269,269,"Switzerland"},{271,271,"Turkey"},
  {272,272,"Ukraine"},{273,273,"Russia"},{303,303,"Alaska (USA)"},{304,304,"Antigua"},
  {305,305,"Antigua"},{306,307,"Netherlands Antilles"},{308,308,"Bahamas"},
  {309,310,"Bahamas"},{311,311,"Bermuda"},{312,312,"Belize"},{314,314,"Barbados"},
  {316,316,"Canada"},{319,319,"Cayman Islands"},{338,339,"USA"},{351,351,"Jamaica"},
  {352,352,"Jamaica"},{353,353,"Jamaica"},{354,355,"Jamaica"},{356,356,"Jamaica"},
  {357,357,"Jamaica"},{358,359,"Puerto Rico"},{361,361,"Saint Lucia"},{366,369,"USA"},
  {370,370,"Panama"},{371,372,"Panama"},{373,373,"Panama"},{374,374,"Panama"},
  {375,376,"Saint Vincent"},{377,377,"Saint Vincent"},{378,378,"British Virgin Islands"},
  {379,379,"US Virgin Islands"},{401,401,"Afghanistan"},{403,403,"Saudi Arabia"},
  {405,405,"Bangladesh"},{408,408,"Bahrain"},{410,410,"Bhutan"},{412,412,"China"},
  {413,413,"China"},{414,414,"China"},{416,416,"Taiwan"},{417,417,"Sri Lanka"},
  {419,419,"India"},{422,422,"Iran"},{423,423,"Azerbaijan"},{425,425,"Iraq"},
  {428,428,"Israel"},{431,431,"Japan"},{432,432,"Japan"},{440,441,"South Korea"},
  {443,443,"Palestine"},{445,445,"North Korea"},{447,447,"Kuwait"},{450,450,"Lebanon"},
  {451,451,"Kyrgyzstan"},{453,453,"Macao"},{455,455,"Maldives"},{457,457,"Mongolia"},
  {459,459,"Nepal"},{461,461,"Oman"},{463,463,"Pakistan"},{466,466,"Qatar"},
  {468,468,"Syria"},{470,470,"UAE"},{473,473,"Yemen"},{477,477,"Hong Kong"},
  {501,501,"Adelie Land"},{503,503,"Australia"},{506,506,"Myanmar"},{508,508,"Brunei"},
  {510,510,"Micronesia"},{511,511,"Palau"},{512,512,"New Zealand"},{514,514,"Cambodia"},
  {515,515,"Cambodia"},{516,516,"Christmas Island"},{518,518,"Cook Islands"},
  {520,520,"Fiji"},{523,523,"Cocos Islands"},{525,525,"Indonesia"},{529,529,"Kiribati"},
  {531,531,"Laos"},{533,533,"Malaysia"},{536,536,"Northern Mariana Islands"},
  {538,538,"Marshall Islands"},{540,540,"New Caledonia"},{542,542,"Niue"},
  {544,544,"Nauru"},{546,546,"French Polynesia"},{548,548,"Philippines"},
  {553,553,"Papua New Guinea"},{555,555,"Pitcairn Island"},{557,557,"Solomon Islands"},
  {559,559,"American Samoa"},{561,561,"Samoa"},{563,564,"Singapore"},{565,566,"Singapore"},
  {567,567,"Thailand"},{570,570,"Tonga"},{572,572,"Tuvalu"},{574,574,"Vietnam"},
  {576,576,"Vanuatu"},{578,578,"Wallis and Futuna"},{601,601,"South Africa"},
  {603,603,"Angola"},{605,605,"Algeria"},{607,607,"Saint Paul"},{608,608,"Ascension Island"},
  {609,609,"Burundi"},{610,610,"Benin"},{611,611,"Botswana"},{612,612,"Central African Republic"},
  {613,613,"Cameroon"},{615,615,"Congo"},{616,616,"Comoros"},{617,617,"Cabo Verde"},
  {618,618,"Crozet Archipelago"},{619,619,"Ivory Coast"},{621,621,"Djibouti"},
  {622,622,"Egypt"},{624,624,"Ethiopia"},{625,625,"Eritrea"},{626,626,"Gabon"},
  {627,627,"Ghana"},{629,629,"Gambia"},{630,630,"Guinea-Bissau"},{631,631,"Equatorial Guinea"},
  {632,632,"Guinea"},{633,633,"Burkina Faso"},{634,634,"Kenya"},{635,635,"Kerguelen Islands"},
  {636,636,"Liberia"},{637,637,"Liberia"},{642,642,"Libya"},{644,644,"Lesotho"},
  {645,645,"Mauritius"},{647,647,"Madagascar"},{649,649,"Mali"},{650,650,"Mozambique"},
  {654,654,"Mauritania"},{655,655,"Malawi"},{656,656,"Niger"},{657,657,"Nigeria"},
  {659,659,"Namibia"},{660,660,"Reunion"},{661,661,"Rwanda"},{662,662,"Sudan"},
  {663,663,"Senegal"},{664,664,"Seychelles"},{665,665,"Saint Helena"},{666,666,"Somalia"},
  {667,667,"Sierra Leone"},{668,668,"Sao Tome and Principe"},{669,669,"Eswatini"},
  {670,670,"Chad"},{671,671,"Togo"},{672,672,"Tunisia"},{674,674,"Tanzania"},
  {675,675,"Uganda"},{676,676,"DRC"},{677,677,"Tanzania"},{678,678,"Zambia"},
  {679,679,"Zimbabwe"},{701,701,"Argentina"},{710,710,"Brazil"},{720,720,"Bolivia"},
  {725,725,"Chile"},{730,730,"Colombia"},{735,735,"Ecuador"},{740,740,"Falkland Islands"},
  {745,745,"Guiana"},{750,750,"Guyana"},{755,755,"Paraguay"},{760,760,"Peru"},
  {765,765,"Suriname"},{770,770,"Uruguay"},{775,775,"Venezuela"},{0,0,NULL}
};

static int is_valid_mmsi(const char *m) {
  if (!m || strlen(m) != 9) return 0;
  for (int i = 0; i < 9; i++) if (!isdigit((unsigned char)m[i])) return 0;
  return 1;
}

static int is_valid_imo(const char *imo) {
  if (!imo) return 0;
  const char *d = imo;
  if (strncasecmp(imo, "IMO", 3) == 0) d = imo + 3;
  if (strlen(d) != 7) return 0;
  for (int i = 0; i < 7; i++) if (!isdigit((unsigned char)d[i])) return 0;
  int sum = 0, w = 7;
  for (int i = 0; i < 6; i++) { sum += (d[i] - '0') * w; w--; }
  return (sum % 10) == (d[6] - '0');
}

static const char *country_from_mmsi(const char *m) {
  if (!m || strlen(m) < 3) return "Unknown";
  char ms[4]; strncpy(ms, m, 3); ms[3] = 0;
  int mid = atoi(ms);
  for (int i = 0; MID_CODES[i].c; i++)
    if (mid >= MID_CODES[i].s && mid <= MID_CODES[i].e) return MID_CODES[i].c;
  return "Unknown";
}

static cJSON *analyze_imo(const char *imo) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "imo", imo);
  if (!is_valid_imo(imo)) {
    cJSON_AddBoolToObject(r, "valid_format", 0);
    cJSON_AddStringToObject(r, "error",
      "Invalid IMO format (must be 7 digits with valid check digit)");
    return r;
  }
  cJSON_AddBoolToObject(r, "valid_format", 1);
  cJSON_AddBoolToObject(r, "check_digit_valid", 1);
  cJSON_AddStringToObject(r, "note",
    "IMO number is a unique permanent identifier assigned to ships. "
    "It does not change even if the ship changes name or flag.");
  cJSON *res = cJSON_CreateArray();
  cJSON_AddItemToArray(res, cJSON_CreateString("https://www.marinetraffic.com/"));
  cJSON_AddItemToArray(res, cJSON_CreateString("https://www.vesselfinder.com/"));
  cJSON_AddItemToArray(res, cJSON_CreateString("https://www.equasis.org/"));
  cJSON_AddItemToObject(r, "lookup_resources", res);
  return r;
}

static cJSON *analyze_mmsi(const char *m) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "mmsi", m);
  if (!is_valid_mmsi(m)) {
    cJSON_AddBoolToObject(r, "valid_format", 0);
    cJSON_AddStringToObject(r, "error", "Invalid MMSI format (must be 9 digits)");
    return r;
  }
  cJSON_AddBoolToObject(r, "valid_format", 1);
  const char *t = "Unknown";
  char f = m[0];
  if (f == '0') t = "Coast Station";
  else if (f >= '2' && f <= '7') t = "Ship Station";
  else if (f == '8') t = "Handheld VHF";
  else if (f == '9') {
    if (m[1] == '7') t = "AIS SART";
    else if (m[1] == '8') t = "MOB Device";
    else if (m[1] == '9') t = "EPIRB";
    else t = "Special Purpose";
  }
  cJSON_AddStringToObject(r, "mmsi_type", t);
  cJSON_AddStringToObject(r, "flag_country", country_from_mmsi(m));
  char mid[4]; strncpy(mid, m, 3); mid[3] = 0;
  cJSON_AddStringToObject(r, "mid", mid);
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  size_t len = strlen(q);
  cJSON *data;
  if (strncasecmp(q, "IMO", 3) == 0 || len == 7) {
    data = analyze_imo(q);
  } else if (len == 9 && is_valid_mmsi(q)) {
    data = analyze_mmsi(q);
  } else {
    data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "note",
      "Vessel name searches require access to commercial AIS databases");
    cJSON *links = cJSON_CreateArray();
    cJSON_AddItemToArray(links, cJSON_CreateString("https://www.marinetraffic.com/"));
    cJSON_AddItemToArray(links, cJSON_CreateString("https://www.vesselfinder.com/"));
    cJSON_AddItemToArray(links, cJSON_CreateString("https://www.myshiptracking.com/"));
    cJSON_AddItemToObject(data, "search_resources", links);
  }

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 75);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "VESSEL_TRACKER");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 75);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "vessel:%s", q);
  char title[320]; snprintf(title, sizeof title, "VESSEL_TRACKER — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "vessel lookup";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"VESSEL_TRACKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def vessel_tracker_def = {
  .id = "VESSEL_TRACKER", .collector = "osint",
  .name = "Vessel Tracker", .name_ja = "船舶追跡",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(vessel_tracker_def)
