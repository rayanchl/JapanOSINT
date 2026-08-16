/* Reddit geo/country/city/OSINT subreddit feeds (deterministic /r/<sub>/.rss). Community signal + local reporting worldwide, via rss_collect. */

/* AUDIT NOTE (slice a3, 2026-07-31) — why the interval is 7200 and not 1800.
 *
 * All 142 sources in this file hit ONE host, www.reddit.com, and reddit
 * rate-limits that host per client IP. Measured from this machine:
 *   - back-to-back requests, or ~2 s apart:  first one 200, EVERY later one 429
 *   - requests spaced 30 s / 60 s / 90 s / 120 s apart: 200 every time
 * So the practical floor is roughly one reddit request per 30 s per IP.
 *
 * At the previous update_interval_sec of 1800, the scheduler wants all 142
 * feeds inside a 30-minute window — one request every 12.7 s on average, i.e.
 * about twice the rate reddit tolerates. The result is what the bulk sweep
 * recorded: 135 of these 142 sources returned rc=-1 with zero rows, because
 * rss_collect() maps any non-2xx (including a transient 429) to -1, which the
 * anomaly detector then turns into `status_bad` and quarantines. The collector
 * itself is fine — reddit-japan and reddit-afghanistan each returned a full 25
 * real entries when they were the first request in a window.
 *
 * 7200 s puts the average spacing at ~51 s, above the measured floor. This is
 * a mitigation, not a cure: it only holds if the scheduler spreads sources
 * across the interval rather than firing a category together. The real fix is
 * a shared per-host rate limiter plus a distinct "throttled" outcome in
 * lib/rss_atom.c so a 429 stops counting as a collector failure — both live in
 * shared code and are reported rather than patched here.
 */
#include "source.h"
#include "lib/rss_atom.h"

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#include "_source_macros.inc"

RSSX(reddit_worldnews, "reddit-worldnews", "Reddit r/worldnews", "Reddit r/worldnews", "osint", "social",
  "https://www.reddit.com/r/worldnews/.rss", "en", "[\"reddit\",\"social\",\"world\",\"community\"]", 7200,
  "Reddit r/worldnews feed — community-sourced signal (world)");

RSSX(reddit_news, "reddit-news", "Reddit r/news", "Reddit r/news", "osint", "social",
  "https://www.reddit.com/r/news/.rss", "en", "[\"reddit\",\"social\",\"global\",\"community\"]", 7200,
  "Reddit r/news feed — community-sourced signal (global)");

RSSX(reddit_geopolitics, "reddit-geopolitics", "Reddit r/geopolitics", "Reddit r/geopolitics", "osint", "social",
  "https://www.reddit.com/r/geopolitics/.rss", "en", "[\"reddit\",\"social\",\"geopolitics\",\"community\"]", 7200,
  "Reddit r/geopolitics feed — community-sourced signal (geopolitics)");

RSSX(reddit_anime_titties, "reddit-anime-titties", "Reddit r/anime_titties", "Reddit r/anime_titties", "osint", "social",
  "https://www.reddit.com/r/anime_titties/.rss", "en", "[\"reddit\",\"social\",\"world-news-alt\",\"community\"]", 7200,
  "Reddit r/anime_titties feed — community-sourced signal (world-news-alt)");

RSSX(reddit_osint, "reddit-osint", "Reddit r/OSINT", "Reddit r/OSINT", "osint", "social",
  "https://www.reddit.com/r/OSINT/.rss", "en", "[\"reddit\",\"social\",\"osint\",\"community\"]", 7200,
  "Reddit r/OSINT feed — community-sourced signal (osint)");

RSSX(reddit_credibledefense, "reddit-credibledefense", "Reddit r/CredibleDefense", "Reddit r/CredibleDefense", "osint", "social",
  "https://www.reddit.com/r/CredibleDefense/.rss", "en", "[\"reddit\",\"social\",\"defense\",\"community\"]", 7200,
  "Reddit r/CredibleDefense feed — community-sourced signal (defense)");

RSSX(reddit_lesscredibledefence, "reddit-lesscredibledefence", "Reddit r/LessCredibleDefence", "Reddit r/LessCredibleDefence", "osint", "social",
  "https://www.reddit.com/r/LessCredibleDefence/.rss", "en", "[\"reddit\",\"social\",\"defense\",\"community\"]", 7200,
  "Reddit r/LessCredibleDefence feed — community-sourced signal (defense)");

RSSX(reddit_warcollege, "reddit-warcollege", "Reddit r/WarCollege", "Reddit r/WarCollege", "osint", "social",
  "https://www.reddit.com/r/WarCollege/.rss", "en", "[\"reddit\",\"social\",\"military\",\"community\"]", 7200,
  "Reddit r/WarCollege feed — community-sourced signal (military)");

RSSX(reddit_combatfootage, "reddit-combatfootage", "Reddit r/CombatFootage", "Reddit r/CombatFootage", "osint", "social",
  "https://www.reddit.com/r/CombatFootage/.rss", "en", "[\"reddit\",\"social\",\"conflict\",\"community\"]", 7200,
  "Reddit r/CombatFootage feed — community-sourced signal (conflict)");

RSSX(reddit_ukrainewarvideoreport, "reddit-ukrainewarvideoreport", "Reddit r/UkraineWarVideoReport", "Reddit r/UkraineWarVideoReport", "osint", "social",
  "https://www.reddit.com/r/UkraineWarVideoReport/.rss", "en", "[\"reddit\",\"social\",\"ukraine\",\"community\"]", 7200,
  "Reddit r/UkraineWarVideoReport feed — community-sourced signal (ukraine)");

RSSX(reddit_syriancivilwar, "reddit-syriancivilwar", "Reddit r/syriancivilwar", "Reddit r/syriancivilwar", "osint", "social",
  "https://www.reddit.com/r/syriancivilwar/.rss", "en", "[\"reddit\",\"social\",\"syria\",\"community\"]", 7200,
  "Reddit r/syriancivilwar feed — community-sourced signal (syria)");

RSSX(reddit_europe, "reddit-europe", "Reddit r/europe", "Reddit r/europe", "osint", "social",
  "https://www.reddit.com/r/europe/.rss", "en", "[\"reddit\",\"social\",\"europe\",\"community\"]", 7200,
  "Reddit r/europe feed — community-sourced signal (europe)");

RSSX(reddit_middleeastnews, "reddit-middleeastnews", "Reddit r/MiddleEastNews", "Reddit r/MiddleEastNews", "osint", "social",
  "https://www.reddit.com/r/MiddleEastNews/.rss", "en", "[\"reddit\",\"social\",\"middle-east\",\"community\"]", 7200,
  "Reddit r/MiddleEastNews feed — community-sourced signal (middle-east)");

RSSX(reddit_africa, "reddit-africa", "Reddit r/africa", "Reddit r/africa", "osint", "social",
  "https://www.reddit.com/r/africa/.rss", "en", "[\"reddit\",\"social\",\"africa\",\"community\"]", 7200,
  "Reddit r/africa feed — community-sourced signal (africa)");

RSSX(reddit_asia, "reddit-asia", "Reddit r/asia", "Reddit r/asia", "osint", "social",
  "https://www.reddit.com/r/asia/.rss", "en", "[\"reddit\",\"social\",\"asia\",\"community\"]", 7200,
  "Reddit r/asia feed — community-sourced signal (asia)");

RSSX(reddit_southasia, "reddit-southasia", "Reddit r/southasia", "Reddit r/southasia", "osint", "social",
  "https://www.reddit.com/r/southasia/.rss", "en", "[\"reddit\",\"social\",\"south-asia\",\"community\"]", 7200,
  "Reddit r/southasia feed — community-sourced signal (south-asia)");

RSSX(reddit_latinamerica, "reddit-latinamerica", "Reddit r/latinamerica", "Reddit r/latinamerica", "osint", "social",
  "https://www.reddit.com/r/latinamerica/.rss", "en", "[\"reddit\",\"social\",\"latam\",\"community\"]", 7200,
  "Reddit r/latinamerica feed — community-sourced signal (latam)");

RSSX(reddit_neutralnews, "reddit-neutralnews", "Reddit r/neutralnews", "Reddit r/neutralnews", "osint", "social",
  "https://www.reddit.com/r/neutralnews/.rss", "en", "[\"reddit\",\"social\",\"news\",\"community\"]", 7200,
  "Reddit r/neutralnews feed — community-sourced signal (news)");

RSSX(reddit_qualitynews, "reddit-qualitynews", "Reddit r/qualitynews", "Reddit r/qualitynews", "osint", "social",
  "https://www.reddit.com/r/qualitynews/.rss", "en", "[\"reddit\",\"social\",\"news\",\"community\"]", 7200,
  "Reddit r/qualitynews feed — community-sourced signal (news)");

RSSX(reddit_intelligence, "reddit-intelligence", "Reddit r/Intelligence", "Reddit r/Intelligence", "osint", "social",
  "https://www.reddit.com/r/Intelligence/.rss", "en", "[\"reddit\",\"social\",\"intelligence\",\"community\"]", 7200,
  "Reddit r/Intelligence feed — community-sourced signal (intelligence)");

RSSX(reddit_natsecpol, "reddit-natsecpol", "Reddit r/NatSecPol", "Reddit r/NatSecPol", "osint", "social",
  "https://www.reddit.com/r/NatSecPol/.rss", "en", "[\"reddit\",\"social\",\"national-security\",\"community\"]", 7200,
  "Reddit r/NatSecPol feed — community-sourced signal (national-security)");

RSSX(reddit_geopolitics2, "reddit-geopolitics2", "Reddit r/geopolitics2", "Reddit r/geopolitics2", "osint", "social",
  "https://www.reddit.com/r/geopolitics2/.rss", "en", "[\"reddit\",\"social\",\"geopolitics\",\"community\"]", 7200,
  "Reddit r/geopolitics2 feed — community-sourced signal (geopolitics)");

RSSX(reddit_yemenicrisis, "reddit-yemenicrisis", "Reddit r/YemeniCrisis", "Reddit r/YemeniCrisis", "osint", "social",
  "https://www.reddit.com/r/YemeniCrisis/.rss", "en", "[\"reddit\",\"social\",\"yemen\",\"community\"]", 7200,
  "Reddit r/YemeniCrisis feed — community-sourced signal (yemen)");

RSSX(reddit_israelpalestine, "reddit-israelpalestine", "Reddit r/IsraelPalestine", "Reddit r/IsraelPalestine", "osint", "social",
  "https://www.reddit.com/r/IsraelPalestine/.rss", "en", "[\"reddit\",\"social\",\"israel-palestine\",\"community\"]", 7200,
  "Reddit r/IsraelPalestine feed — community-sourced signal (israel-palestine)");

RSSX(reddit_china_debate, "reddit-china-debate", "Reddit r/China_Debate", "Reddit r/China_Debate", "osint", "social",
  "https://www.reddit.com/r/China_Debate/.rss", "en", "[\"reddit\",\"social\",\"china\",\"community\"]", 7200,
  "Reddit r/China_Debate feed — community-sourced signal (china)");

RSSX(reddit_aviation, "reddit-aviation", "Reddit r/aviation", "Reddit r/aviation", "osint", "social",
  "https://www.reddit.com/r/aviation/.rss", "en", "[\"reddit\",\"social\",\"aviation\",\"community\"]", 7200,
  "Reddit r/aviation feed — community-sourced signal (aviation)");

RSSX(reddit_navy, "reddit-navy", "Reddit r/navy", "Reddit r/navy", "osint", "social",
  "https://www.reddit.com/r/navy/.rss", "en", "[\"reddit\",\"social\",\"navy\",\"community\"]", 7200,
  "Reddit r/navy feed — community-sourced signal (navy)");

RSSX(reddit_military, "reddit-military", "Reddit r/Military", "Reddit r/Military", "osint", "social",
  "https://www.reddit.com/r/Military/.rss", "en", "[\"reddit\",\"social\",\"military\",\"community\"]", 7200,
  "Reddit r/Military feed — community-sourced signal (military)");

RSSX(reddit_tankporn, "reddit-tankporn", "Reddit r/TankPorn", "Reddit r/TankPorn", "osint", "social",
  "https://www.reddit.com/r/TankPorn/.rss", "en", "[\"reddit\",\"social\",\"military\",\"community\"]", 7200,
  "Reddit r/TankPorn feed — community-sourced signal (military)");

RSSX(reddit_hackernews, "reddit-hackernews", "Reddit r/hackernews", "Reddit r/hackernews", "osint", "social",
  "https://www.reddit.com/r/hackernews/.rss", "en", "[\"reddit\",\"social\",\"cyber\",\"community\"]", 7200,
  "Reddit r/hackernews feed — community-sourced signal (cyber)");

RSSX(reddit_cybersecurity, "reddit-cybersecurity", "Reddit r/cybersecurity", "Reddit r/cybersecurity", "osint", "social",
  "https://www.reddit.com/r/cybersecurity/.rss", "en", "[\"reddit\",\"social\",\"cyber\",\"community\"]", 7200,
  "Reddit r/cybersecurity feed — community-sourced signal (cyber)");

RSSX(reddit_netsec, "reddit-netsec", "Reddit r/netsec", "Reddit r/netsec", "osint", "social",
  "https://www.reddit.com/r/netsec/.rss", "en", "[\"reddit\",\"social\",\"cyber\",\"community\"]", 7200,
  "Reddit r/netsec feed — community-sourced signal (cyber)");

RSSX(reddit_malware, "reddit-malware", "Reddit r/Malware", "Reddit r/Malware", "osint", "social",
  "https://www.reddit.com/r/Malware/.rss", "en", "[\"reddit\",\"social\",\"cyber\",\"community\"]", 7200,
  "Reddit r/Malware feed — community-sourced signal (cyber)");

RSSX(reddit_blackhat, "reddit-blackhat", "Reddit r/blackhat", "Reddit r/blackhat", "osint", "social",
  "https://www.reddit.com/r/blackhat/.rss", "en", "[\"reddit\",\"social\",\"cyber\",\"community\"]", 7200,
  "Reddit r/blackhat feed — community-sourced signal (cyber)");

RSSX(reddit_unitedkingdom, "reddit-unitedkingdom", "Reddit r/unitedkingdom", "Reddit r/unitedkingdom", "osint", "social",
  "https://www.reddit.com/r/unitedkingdom/.rss", "en", "[\"reddit\",\"social\",\"uk\",\"community\"]", 7200,
  "Reddit r/unitedkingdom feed — community-sourced signal (uk)");

RSSX(reddit_ukpolitics, "reddit-ukpolitics", "Reddit r/ukpolitics", "Reddit r/ukpolitics", "osint", "social",
  "https://www.reddit.com/r/ukpolitics/.rss", "en", "[\"reddit\",\"social\",\"uk\",\"community\"]", 7200,
  "Reddit r/ukpolitics feed — community-sourced signal (uk)");

RSSX(reddit_france, "reddit-france", "Reddit r/france", "Reddit r/france", "osint", "social",
  "https://www.reddit.com/r/france/.rss", "en", "[\"reddit\",\"social\",\"france\",\"community\"]", 7200,
  "Reddit r/france feed — community-sourced signal (france)");

RSSX(reddit_germany, "reddit-germany", "Reddit r/germany", "Reddit r/germany", "osint", "social",
  "https://www.reddit.com/r/germany/.rss", "en", "[\"reddit\",\"social\",\"germany\",\"community\"]", 7200,
  "Reddit r/germany feed — community-sourced signal (germany)");

RSSX(reddit_italy, "reddit-italy", "Reddit r/italy", "Reddit r/italy", "osint", "social",
  "https://www.reddit.com/r/italy/.rss", "en", "[\"reddit\",\"social\",\"italy\",\"community\"]", 7200,
  "Reddit r/italy feed — community-sourced signal (italy)");

RSSX(reddit_spain, "reddit-spain", "Reddit r/spain", "Reddit r/spain", "osint", "social",
  "https://www.reddit.com/r/spain/.rss", "en", "[\"reddit\",\"social\",\"spain\",\"community\"]", 7200,
  "Reddit r/spain feed — community-sourced signal (spain)");

RSSX(reddit_portugal, "reddit-portugal", "Reddit r/portugal", "Reddit r/portugal", "osint", "social",
  "https://www.reddit.com/r/portugal/.rss", "en", "[\"reddit\",\"social\",\"portugal\",\"community\"]", 7200,
  "Reddit r/portugal feed — community-sourced signal (portugal)");

RSSX(reddit_netherlands, "reddit-netherlands", "Reddit r/netherlands", "Reddit r/netherlands", "osint", "social",
  "https://www.reddit.com/r/netherlands/.rss", "en", "[\"reddit\",\"social\",\"netherlands\",\"community\"]", 7200,
  "Reddit r/netherlands feed — community-sourced signal (netherlands)");

RSSX(reddit_belgium, "reddit-belgium", "Reddit r/belgium", "Reddit r/belgium", "osint", "social",
  "https://www.reddit.com/r/belgium/.rss", "en", "[\"reddit\",\"social\",\"belgium\",\"community\"]", 7200,
  "Reddit r/belgium feed — community-sourced signal (belgium)");

RSSX(reddit_ireland, "reddit-ireland", "Reddit r/ireland", "Reddit r/ireland", "osint", "social",
  "https://www.reddit.com/r/ireland/.rss", "en", "[\"reddit\",\"social\",\"ireland\",\"community\"]", 7200,
  "Reddit r/ireland feed — community-sourced signal (ireland)");

RSSX(reddit_sweden, "reddit-sweden", "Reddit r/sweden", "Reddit r/sweden", "osint", "social",
  "https://www.reddit.com/r/sweden/.rss", "en", "[\"reddit\",\"social\",\"sweden\",\"community\"]", 7200,
  "Reddit r/sweden feed — community-sourced signal (sweden)");

RSSX(reddit_norway, "reddit-norway", "Reddit r/norway", "Reddit r/norway", "osint", "social",
  "https://www.reddit.com/r/norway/.rss", "en", "[\"reddit\",\"social\",\"norway\",\"community\"]", 7200,
  "Reddit r/norway feed — community-sourced signal (norway)");

RSSX(reddit_denmark, "reddit-denmark", "Reddit r/denmark", "Reddit r/denmark", "osint", "social",
  "https://www.reddit.com/r/denmark/.rss", "en", "[\"reddit\",\"social\",\"denmark\",\"community\"]", 7200,
  "Reddit r/denmark feed — community-sourced signal (denmark)");

RSSX(reddit_finland, "reddit-finland", "Reddit r/Finland", "Reddit r/Finland", "osint", "social",
  "https://www.reddit.com/r/Finland/.rss", "en", "[\"reddit\",\"social\",\"finland\",\"community\"]", 7200,
  "Reddit r/Finland feed — community-sourced signal (finland)");

RSSX(reddit_iceland, "reddit-iceland", "Reddit r/iceland", "Reddit r/iceland", "osint", "social",
  "https://www.reddit.com/r/iceland/.rss", "en", "[\"reddit\",\"social\",\"iceland\",\"community\"]", 7200,
  "Reddit r/iceland feed — community-sourced signal (iceland)");

RSSX(reddit_poland, "reddit-poland", "Reddit r/poland", "Reddit r/poland", "osint", "social",
  "https://www.reddit.com/r/poland/.rss", "en", "[\"reddit\",\"social\",\"poland\",\"community\"]", 7200,
  "Reddit r/poland feed — community-sourced signal (poland)");

RSSX(reddit_czech, "reddit-czech", "Reddit r/czech", "Reddit r/czech", "osint", "social",
  "https://www.reddit.com/r/czech/.rss", "en", "[\"reddit\",\"social\",\"czechia\",\"community\"]", 7200,
  "Reddit r/czech feed — community-sourced signal (czechia)");

RSSX(reddit_slovakia, "reddit-slovakia", "Reddit r/Slovakia", "Reddit r/Slovakia", "osint", "social",
  "https://www.reddit.com/r/Slovakia/.rss", "en", "[\"reddit\",\"social\",\"slovakia\",\"community\"]", 7200,
  "Reddit r/Slovakia feed — community-sourced signal (slovakia)");

RSSX(reddit_hungary, "reddit-hungary", "Reddit r/hungary", "Reddit r/hungary", "osint", "social",
  "https://www.reddit.com/r/hungary/.rss", "en", "[\"reddit\",\"social\",\"hungary\",\"community\"]", 7200,
  "Reddit r/hungary feed — community-sourced signal (hungary)");

RSSX(reddit_romania, "reddit-romania", "Reddit r/Romania", "Reddit r/Romania", "osint", "social",
  "https://www.reddit.com/r/Romania/.rss", "en", "[\"reddit\",\"social\",\"romania\",\"community\"]", 7200,
  "Reddit r/Romania feed — community-sourced signal (romania)");

RSSX(reddit_bulgaria, "reddit-bulgaria", "Reddit r/bulgaria", "Reddit r/bulgaria", "osint", "social",
  "https://www.reddit.com/r/bulgaria/.rss", "en", "[\"reddit\",\"social\",\"bulgaria\",\"community\"]", 7200,
  "Reddit r/bulgaria feed — community-sourced signal (bulgaria)");

RSSX(reddit_greece, "reddit-greece", "Reddit r/greece", "Reddit r/greece", "osint", "social",
  "https://www.reddit.com/r/greece/.rss", "en", "[\"reddit\",\"social\",\"greece\",\"community\"]", 7200,
  "Reddit r/greece feed — community-sourced signal (greece)");

RSSX(reddit_croatia, "reddit-croatia", "Reddit r/croatia", "Reddit r/croatia", "osint", "social",
  "https://www.reddit.com/r/croatia/.rss", "en", "[\"reddit\",\"social\",\"croatia\",\"community\"]", 7200,
  "Reddit r/croatia feed — community-sourced signal (croatia)");

RSSX(reddit_serbia, "reddit-serbia", "Reddit r/serbia", "Reddit r/serbia", "osint", "social",
  "https://www.reddit.com/r/serbia/.rss", "en", "[\"reddit\",\"social\",\"serbia\",\"community\"]", 7200,
  "Reddit r/serbia feed — community-sourced signal (serbia)");

RSSX(reddit_austria, "reddit-austria", "Reddit r/Austria", "Reddit r/Austria", "osint", "social",
  "https://www.reddit.com/r/Austria/.rss", "en", "[\"reddit\",\"social\",\"austria\",\"community\"]", 7200,
  "Reddit r/Austria feed — community-sourced signal (austria)");

RSSX(reddit_switzerland, "reddit-switzerland", "Reddit r/switzerland", "Reddit r/switzerland", "osint", "social",
  "https://www.reddit.com/r/switzerland/.rss", "en", "[\"reddit\",\"social\",\"switzerland\",\"community\"]", 7200,
  "Reddit r/switzerland feed — community-sourced signal (switzerland)");

RSSX(reddit_russia, "reddit-russia", "Reddit r/russia", "Reddit r/russia", "osint", "social",
  "https://www.reddit.com/r/russia/.rss", "en", "[\"reddit\",\"social\",\"russia\",\"community\"]", 7200,
  "Reddit r/russia feed — community-sourced signal (russia)");

RSSX(reddit_ukraina, "reddit-ukraina", "Reddit r/ukraina", "Reddit r/ukraina", "osint", "social",
  "https://www.reddit.com/r/ukraina/.rss", "en", "[\"reddit\",\"social\",\"ukraine\",\"community\"]", 7200,
  "Reddit r/ukraina feed — community-sourced signal (ukraine)");

RSSX(reddit_belarus, "reddit-belarus", "Reddit r/belarus", "Reddit r/belarus", "osint", "social",
  "https://www.reddit.com/r/belarus/.rss", "en", "[\"reddit\",\"social\",\"belarus\",\"community\"]", 7200,
  "Reddit r/belarus feed — community-sourced signal (belarus)");

RSSX(reddit_lithuania, "reddit-lithuania", "Reddit r/lithuania", "Reddit r/lithuania", "osint", "social",
  "https://www.reddit.com/r/lithuania/.rss", "en", "[\"reddit\",\"social\",\"lithuania\",\"community\"]", 7200,
  "Reddit r/lithuania feed — community-sourced signal (lithuania)");

RSSX(reddit_latvia, "reddit-latvia", "Reddit r/latvia", "Reddit r/latvia", "osint", "social",
  "https://www.reddit.com/r/latvia/.rss", "en", "[\"reddit\",\"social\",\"latvia\",\"community\"]", 7200,
  "Reddit r/latvia feed — community-sourced signal (latvia)");

RSSX(reddit_eesti, "reddit-eesti", "Reddit r/Eesti", "Reddit r/Eesti", "osint", "social",
  "https://www.reddit.com/r/Eesti/.rss", "en", "[\"reddit\",\"social\",\"estonia\",\"community\"]", 7200,
  "Reddit r/Eesti feed — community-sourced signal (estonia)");

RSSX(reddit_india, "reddit-india", "Reddit r/india", "Reddit r/india", "osint", "social",
  "https://www.reddit.com/r/india/.rss", "en", "[\"reddit\",\"social\",\"india\",\"community\"]", 7200,
  "Reddit r/india feed — community-sourced signal (india)");

RSSX(reddit_indiaspeaks, "reddit-indiaspeaks", "Reddit r/IndiaSpeaks", "Reddit r/IndiaSpeaks", "osint", "social",
  "https://www.reddit.com/r/IndiaSpeaks/.rss", "en", "[\"reddit\",\"social\",\"india\",\"community\"]", 7200,
  "Reddit r/IndiaSpeaks feed — community-sourced signal (india)");

RSSX(reddit_china, "reddit-china", "Reddit r/china", "Reddit r/china", "osint", "social",
  "https://www.reddit.com/r/china/.rss", "en", "[\"reddit\",\"social\",\"china\",\"community\"]", 7200,
  "Reddit r/china feed — community-sourced signal (china)");

RSSX(reddit_china_irl, "reddit-china-irl", "Reddit r/China_irl", "Reddit r/China_irl", "osint", "social",
  "https://www.reddit.com/r/China_irl/.rss", "en", "[\"reddit\",\"social\",\"china\",\"community\"]", 7200,
  "Reddit r/China_irl feed — community-sourced signal (china)");

RSSX(reddit_japan, "reddit-japan", "Reddit r/japan", "Reddit r/japan", "osint", "social",
  "https://www.reddit.com/r/japan/.rss", "en", "[\"reddit\",\"social\",\"japan\",\"community\"]", 7200,
  "Reddit r/japan feed — community-sourced signal (japan)");

RSSX(reddit_korea, "reddit-korea", "Reddit r/korea", "Reddit r/korea", "osint", "social",
  "https://www.reddit.com/r/korea/.rss", "en", "[\"reddit\",\"social\",\"korea\",\"community\"]", 7200,
  "Reddit r/korea feed — community-sourced signal (korea)");

RSSX(reddit_taiwan, "reddit-taiwan", "Reddit r/taiwan", "Reddit r/taiwan", "osint", "social",
  "https://www.reddit.com/r/taiwan/.rss", "en", "[\"reddit\",\"social\",\"taiwan\",\"community\"]", 7200,
  "Reddit r/taiwan feed — community-sourced signal (taiwan)");

RSSX(reddit_hongkong, "reddit-hongkong", "Reddit r/HongKong", "Reddit r/HongKong", "osint", "social",
  "https://www.reddit.com/r/HongKong/.rss", "en", "[\"reddit\",\"social\",\"hong-kong\",\"community\"]", 7200,
  "Reddit r/HongKong feed — community-sourced signal (hong-kong)");

RSSX(reddit_indonesia, "reddit-indonesia", "Reddit r/indonesia", "Reddit r/indonesia", "osint", "social",
  "https://www.reddit.com/r/indonesia/.rss", "en", "[\"reddit\",\"social\",\"indonesia\",\"community\"]", 7200,
  "Reddit r/indonesia feed — community-sourced signal (indonesia)");

RSSX(reddit_thailand, "reddit-thailand", "Reddit r/Thailand", "Reddit r/Thailand", "osint", "social",
  "https://www.reddit.com/r/Thailand/.rss", "en", "[\"reddit\",\"social\",\"thailand\",\"community\"]", 7200,
  "Reddit r/Thailand feed — community-sourced signal (thailand)");

RSSX(reddit_vietnam, "reddit-vietnam", "Reddit r/VietNam", "Reddit r/VietNam", "osint", "social",
  "https://www.reddit.com/r/VietNam/.rss", "en", "[\"reddit\",\"social\",\"vietnam\",\"community\"]", 7200,
  "Reddit r/VietNam feed — community-sourced signal (vietnam)");

RSSX(reddit_philippines, "reddit-philippines", "Reddit r/Philippines", "Reddit r/Philippines", "osint", "social",
  "https://www.reddit.com/r/Philippines/.rss", "en", "[\"reddit\",\"social\",\"philippines\",\"community\"]", 7200,
  "Reddit r/Philippines feed — community-sourced signal (philippines)");

RSSX(reddit_malaysia, "reddit-malaysia", "Reddit r/malaysia", "Reddit r/malaysia", "osint", "social",
  "https://www.reddit.com/r/malaysia/.rss", "en", "[\"reddit\",\"social\",\"malaysia\",\"community\"]", 7200,
  "Reddit r/malaysia feed — community-sourced signal (malaysia)");

RSSX(reddit_singapore, "reddit-singapore", "Reddit r/singapore", "Reddit r/singapore", "osint", "social",
  "https://www.reddit.com/r/singapore/.rss", "en", "[\"reddit\",\"social\",\"singapore\",\"community\"]", 7200,
  "Reddit r/singapore feed — community-sourced signal (singapore)");

RSSX(reddit_pakistan, "reddit-pakistan", "Reddit r/pakistan", "Reddit r/pakistan", "osint", "social",
  "https://www.reddit.com/r/pakistan/.rss", "en", "[\"reddit\",\"social\",\"pakistan\",\"community\"]", 7200,
  "Reddit r/pakistan feed — community-sourced signal (pakistan)");

RSSX(reddit_bangladesh, "reddit-bangladesh", "Reddit r/bangladesh", "Reddit r/bangladesh", "osint", "social",
  "https://www.reddit.com/r/bangladesh/.rss", "en", "[\"reddit\",\"social\",\"bangladesh\",\"community\"]", 7200,
  "Reddit r/bangladesh feed — community-sourced signal (bangladesh)");

RSSX(reddit_nepal, "reddit-nepal", "Reddit r/Nepal", "Reddit r/Nepal", "osint", "social",
  "https://www.reddit.com/r/Nepal/.rss", "en", "[\"reddit\",\"social\",\"nepal\",\"community\"]", 7200,
  "Reddit r/Nepal feed — community-sourced signal (nepal)");

RSSX(reddit_srilanka, "reddit-srilanka", "Reddit r/srilanka", "Reddit r/srilanka", "osint", "social",
  "https://www.reddit.com/r/srilanka/.rss", "en", "[\"reddit\",\"social\",\"sri-lanka\",\"community\"]", 7200,
  "Reddit r/srilanka feed — community-sourced signal (sri-lanka)");

RSSX(reddit_myanmar, "reddit-myanmar", "Reddit r/myanmar", "Reddit r/myanmar", "osint", "social",
  "https://www.reddit.com/r/myanmar/.rss", "en", "[\"reddit\",\"social\",\"myanmar\",\"community\"]", 7200,
  "Reddit r/myanmar feed — community-sourced signal (myanmar)");

RSSX(reddit_australia, "reddit-australia", "Reddit r/australia", "Reddit r/australia", "osint", "social",
  "https://www.reddit.com/r/australia/.rss", "en", "[\"reddit\",\"social\",\"australia\",\"community\"]", 7200,
  "Reddit r/australia feed — community-sourced signal (australia)");

RSSX(reddit_newzealand, "reddit-newzealand", "Reddit r/newzealand", "Reddit r/newzealand", "osint", "social",
  "https://www.reddit.com/r/newzealand/.rss", "en", "[\"reddit\",\"social\",\"new-zealand\",\"community\"]", 7200,
  "Reddit r/newzealand feed — community-sourced signal (new-zealand)");

RSSX(reddit_canada, "reddit-canada", "Reddit r/canada", "Reddit r/canada", "osint", "social",
  "https://www.reddit.com/r/canada/.rss", "en", "[\"reddit\",\"social\",\"canada\",\"community\"]", 7200,
  "Reddit r/canada feed — community-sourced signal (canada)");

RSSX(reddit_mexico, "reddit-mexico", "Reddit r/mexico", "Reddit r/mexico", "osint", "social",
  "https://www.reddit.com/r/mexico/.rss", "en", "[\"reddit\",\"social\",\"mexico\",\"community\"]", 7200,
  "Reddit r/mexico feed — community-sourced signal (mexico)");

RSSX(reddit_brasil, "reddit-brasil", "Reddit r/brasil", "Reddit r/brasil", "osint", "social",
  "https://www.reddit.com/r/brasil/.rss", "en", "[\"reddit\",\"social\",\"brazil\",\"community\"]", 7200,
  "Reddit r/brasil feed — community-sourced signal (brazil)");

RSSX(reddit_argentina, "reddit-argentina", "Reddit r/argentina", "Reddit r/argentina", "osint", "social",
  "https://www.reddit.com/r/argentina/.rss", "en", "[\"reddit\",\"social\",\"argentina\",\"community\"]", 7200,
  "Reddit r/argentina feed — community-sourced signal (argentina)");

RSSX(reddit_chile, "reddit-chile", "Reddit r/chile", "Reddit r/chile", "osint", "social",
  "https://www.reddit.com/r/chile/.rss", "en", "[\"reddit\",\"social\",\"chile\",\"community\"]", 7200,
  "Reddit r/chile feed — community-sourced signal (chile)");

RSSX(reddit_colombia, "reddit-colombia", "Reddit r/Colombia", "Reddit r/Colombia", "osint", "social",
  "https://www.reddit.com/r/Colombia/.rss", "en", "[\"reddit\",\"social\",\"colombia\",\"community\"]", 7200,
  "Reddit r/Colombia feed — community-sourced signal (colombia)");

RSSX(reddit_peru, "reddit-peru", "Reddit r/PERU", "Reddit r/PERU", "osint", "social",
  "https://www.reddit.com/r/PERU/.rss", "en", "[\"reddit\",\"social\",\"peru\",\"community\"]", 7200,
  "Reddit r/PERU feed — community-sourced signal (peru)");

RSSX(reddit_vzla, "reddit-vzla", "Reddit r/vzla", "Reddit r/vzla", "osint", "social",
  "https://www.reddit.com/r/vzla/.rss", "en", "[\"reddit\",\"social\",\"venezuela\",\"community\"]", 7200,
  "Reddit r/vzla feed — community-sourced signal (venezuela)");

RSSX(reddit_ecuador, "reddit-ecuador", "Reddit r/ecuador", "Reddit r/ecuador", "osint", "social",
  "https://www.reddit.com/r/ecuador/.rss", "en", "[\"reddit\",\"social\",\"ecuador\",\"community\"]", 7200,
  "Reddit r/ecuador feed — community-sourced signal (ecuador)");

RSSX(reddit_bolivia, "reddit-bolivia", "Reddit r/bolivia", "Reddit r/bolivia", "osint", "social",
  "https://www.reddit.com/r/bolivia/.rss", "en", "[\"reddit\",\"social\",\"bolivia\",\"community\"]", 7200,
  "Reddit r/bolivia feed — community-sourced signal (bolivia)");

RSSX(reddit_uruguay, "reddit-uruguay", "Reddit r/uruguay", "Reddit r/uruguay", "osint", "social",
  "https://www.reddit.com/r/uruguay/.rss", "en", "[\"reddit\",\"social\",\"uruguay\",\"community\"]", 7200,
  "Reddit r/uruguay feed — community-sourced signal (uruguay)");

RSSX(reddit_southafrica, "reddit-southafrica", "Reddit r/southafrica", "Reddit r/southafrica", "osint", "social",
  "https://www.reddit.com/r/southafrica/.rss", "en", "[\"reddit\",\"social\",\"south-africa\",\"community\"]", 7200,
  "Reddit r/southafrica feed — community-sourced signal (south-africa)");

RSSX(reddit_nigeria, "reddit-nigeria", "Reddit r/Nigeria", "Reddit r/Nigeria", "osint", "social",
  "https://www.reddit.com/r/Nigeria/.rss", "en", "[\"reddit\",\"social\",\"nigeria\",\"community\"]", 7200,
  "Reddit r/Nigeria feed — community-sourced signal (nigeria)");

RSSX(reddit_kenya, "reddit-kenya", "Reddit r/Kenya", "Reddit r/Kenya", "osint", "social",
  "https://www.reddit.com/r/Kenya/.rss", "en", "[\"reddit\",\"social\",\"kenya\",\"community\"]", 7200,
  "Reddit r/Kenya feed — community-sourced signal (kenya)");

RSSX(reddit_egypt, "reddit-egypt", "Reddit r/egypt", "Reddit r/egypt", "osint", "social",
  "https://www.reddit.com/r/egypt/.rss", "en", "[\"reddit\",\"social\",\"egypt\",\"community\"]", 7200,
  "Reddit r/egypt feed — community-sourced signal (egypt)");

RSSX(reddit_morocco, "reddit-morocco", "Reddit r/Morocco", "Reddit r/Morocco", "osint", "social",
  "https://www.reddit.com/r/Morocco/.rss", "en", "[\"reddit\",\"social\",\"morocco\",\"community\"]", 7200,
  "Reddit r/Morocco feed — community-sourced signal (morocco)");

RSSX(reddit_ethiopia, "reddit-ethiopia", "Reddit r/ethiopia", "Reddit r/ethiopia", "osint", "social",
  "https://www.reddit.com/r/ethiopia/.rss", "en", "[\"reddit\",\"social\",\"ethiopia\",\"community\"]", 7200,
  "Reddit r/ethiopia feed — community-sourced signal (ethiopia)");

RSSX(reddit_ghana, "reddit-ghana", "Reddit r/ghana", "Reddit r/ghana", "osint", "social",
  "https://www.reddit.com/r/ghana/.rss", "en", "[\"reddit\",\"social\",\"ghana\",\"community\"]", 7200,
  "Reddit r/ghana feed — community-sourced signal (ghana)");

RSSX(reddit_turkey, "reddit-turkey", "Reddit r/turkey", "Reddit r/turkey", "osint", "social",
  "https://www.reddit.com/r/turkey/.rss", "en", "[\"reddit\",\"social\",\"turkey\",\"community\"]", 7200,
  "Reddit r/turkey feed — community-sourced signal (turkey)");

RSSX(reddit_iran, "reddit-iran", "Reddit r/iran", "Reddit r/iran", "osint", "social",
  "https://www.reddit.com/r/iran/.rss", "en", "[\"reddit\",\"social\",\"iran\",\"community\"]", 7200,
  "Reddit r/iran feed — community-sourced signal (iran)");

RSSX(reddit_israel, "reddit-israel", "Reddit r/Israel", "Reddit r/Israel", "osint", "social",
  "https://www.reddit.com/r/Israel/.rss", "en", "[\"reddit\",\"social\",\"israel\",\"community\"]", 7200,
  "Reddit r/Israel feed — community-sourced signal (israel)");

RSSX(reddit_saudiarabia, "reddit-saudiarabia", "Reddit r/saudiarabia", "Reddit r/saudiarabia", "osint", "social",
  "https://www.reddit.com/r/saudiarabia/.rss", "en", "[\"reddit\",\"social\",\"saudi-arabia\",\"community\"]", 7200,
  "Reddit r/saudiarabia feed — community-sourced signal (saudi-arabia)");

RSSX(reddit_dubai, "reddit-dubai", "Reddit r/dubai", "Reddit r/dubai", "osint", "social",
  "https://www.reddit.com/r/dubai/.rss", "en", "[\"reddit\",\"social\",\"uae\",\"community\"]", 7200,
  "Reddit r/dubai feed — community-sourced signal (uae)");

RSSX(reddit_qatar, "reddit-qatar", "Reddit r/qatar", "Reddit r/qatar", "osint", "social",
  "https://www.reddit.com/r/qatar/.rss", "en", "[\"reddit\",\"social\",\"qatar\",\"community\"]", 7200,
  "Reddit r/qatar feed — community-sourced signal (qatar)");

RSSX(reddit_lebanon, "reddit-lebanon", "Reddit r/lebanon", "Reddit r/lebanon", "osint", "social",
  "https://www.reddit.com/r/lebanon/.rss", "en", "[\"reddit\",\"social\",\"lebanon\",\"community\"]", 7200,
  "Reddit r/lebanon feed — community-sourced signal (lebanon)");

RSSX(reddit_iraq, "reddit-iraq", "Reddit r/iraq", "Reddit r/iraq", "osint", "social",
  "https://www.reddit.com/r/iraq/.rss", "en", "[\"reddit\",\"social\",\"iraq\",\"community\"]", 7200,
  "Reddit r/iraq feed — community-sourced signal (iraq)");

RSSX(reddit_jordan, "reddit-jordan", "Reddit r/jordan", "Reddit r/jordan", "osint", "social",
  "https://www.reddit.com/r/jordan/.rss", "en", "[\"reddit\",\"social\",\"jordan\",\"community\"]", 7200,
  "Reddit r/jordan feed — community-sourced signal (jordan)");

RSSX(reddit_syria, "reddit-syria", "Reddit r/Syria", "Reddit r/Syria", "osint", "social",
  "https://www.reddit.com/r/Syria/.rss", "en", "[\"reddit\",\"social\",\"syria\",\"community\"]", 7200,
  "Reddit r/Syria feed — community-sourced signal (syria)");

RSSX(reddit_afghanistan, "reddit-afghanistan", "Reddit r/afghanistan", "Reddit r/afghanistan", "osint", "social",
  "https://www.reddit.com/r/afghanistan/.rss", "en", "[\"reddit\",\"social\",\"afghanistan\",\"community\"]", 7200,
  "Reddit r/afghanistan feed — community-sourced signal (afghanistan)");

RSSX(reddit_kazakhstan, "reddit-kazakhstan", "Reddit r/Kazakhstan", "Reddit r/Kazakhstan", "osint", "social",
  "https://www.reddit.com/r/Kazakhstan/.rss", "en", "[\"reddit\",\"social\",\"kazakhstan\",\"community\"]", 7200,
  "Reddit r/Kazakhstan feed — community-sourced signal (kazakhstan)");

RSSX(reddit_uzbekistan, "reddit-uzbekistan", "Reddit r/uzbekistan", "Reddit r/uzbekistan", "osint", "social",
  "https://www.reddit.com/r/uzbekistan/.rss", "en", "[\"reddit\",\"social\",\"uzbekistan\",\"community\"]", 7200,
  "Reddit r/uzbekistan feed — community-sourced signal (uzbekistan)");

RSSX(reddit_azerbaijan, "reddit-azerbaijan", "Reddit r/azerbaijan", "Reddit r/azerbaijan", "osint", "social",
  "https://www.reddit.com/r/azerbaijan/.rss", "en", "[\"reddit\",\"social\",\"azerbaijan\",\"community\"]", 7200,
  "Reddit r/azerbaijan feed — community-sourced signal (azerbaijan)");

RSSX(reddit_armenia, "reddit-armenia", "Reddit r/armenia", "Reddit r/armenia", "osint", "social",
  "https://www.reddit.com/r/armenia/.rss", "en", "[\"reddit\",\"social\",\"armenia\",\"community\"]", 7200,
  "Reddit r/armenia feed — community-sourced signal (armenia)");

RSSX(reddit_sakartvelo, "reddit-sakartvelo", "Reddit r/Sakartvelo", "Reddit r/Sakartvelo", "osint", "social",
  "https://www.reddit.com/r/Sakartvelo/.rss", "en", "[\"reddit\",\"social\",\"georgia\",\"community\"]", 7200,
  "Reddit r/Sakartvelo feed — community-sourced signal (georgia)");

RSSX(reddit_mongolia, "reddit-mongolia", "Reddit r/mongolia", "Reddit r/mongolia", "osint", "social",
  "https://www.reddit.com/r/mongolia/.rss", "en", "[\"reddit\",\"social\",\"mongolia\",\"community\"]", 7200,
  "Reddit r/mongolia feed — community-sourced signal (mongolia)");

RSSX(reddit_london, "reddit-london", "Reddit r/london", "Reddit r/london", "osint", "social",
  "https://www.reddit.com/r/london/.rss", "en", "[\"reddit\",\"social\",\"london\",\"community\"]", 7200,
  "Reddit r/london feed — community-sourced signal (london)");

RSSX(reddit_paris, "reddit-paris", "Reddit r/paris", "Reddit r/paris", "osint", "social",
  "https://www.reddit.com/r/paris/.rss", "en", "[\"reddit\",\"social\",\"paris\",\"community\"]", 7200,
  "Reddit r/paris feed — community-sourced signal (paris)");

RSSX(reddit_tokyo, "reddit-tokyo", "Reddit r/Tokyo", "Reddit r/Tokyo", "osint", "social",
  "https://www.reddit.com/r/Tokyo/.rss", "en", "[\"reddit\",\"social\",\"tokyo\",\"community\"]", 7200,
  "Reddit r/Tokyo feed — community-sourced signal (tokyo)");

RSSX(reddit_berlin, "reddit-berlin", "Reddit r/berlin", "Reddit r/berlin", "osint", "social",
  "https://www.reddit.com/r/berlin/.rss", "en", "[\"reddit\",\"social\",\"berlin\",\"community\"]", 7200,
  "Reddit r/berlin feed — community-sourced signal (berlin)");

RSSX(reddit_moscow, "reddit-moscow", "Reddit r/moscow", "Reddit r/moscow", "osint", "social",
  "https://www.reddit.com/r/moscow/.rss", "en", "[\"reddit\",\"social\",\"moscow\",\"community\"]", 7200,
  "Reddit r/moscow feed — community-sourced signal (moscow)");

RSSX(reddit_nyc, "reddit-nyc", "Reddit r/nyc", "Reddit r/nyc", "osint", "social",
  "https://www.reddit.com/r/nyc/.rss", "en", "[\"reddit\",\"social\",\"new-york\",\"community\"]", 7200,
  "Reddit r/nyc feed — community-sourced signal (new-york)");

RSSX(reddit_losangeles, "reddit-losangeles", "Reddit r/LosAngeles", "Reddit r/LosAngeles", "osint", "social",
  "https://www.reddit.com/r/LosAngeles/.rss", "en", "[\"reddit\",\"social\",\"los-angeles\",\"community\"]", 7200,
  "Reddit r/LosAngeles feed — community-sourced signal (los-angeles)");

RSSX(reddit_toronto, "reddit-toronto", "Reddit r/toronto", "Reddit r/toronto", "osint", "social",
  "https://www.reddit.com/r/toronto/.rss", "en", "[\"reddit\",\"social\",\"toronto\",\"community\"]", 7200,
  "Reddit r/toronto feed — community-sourced signal (toronto)");

RSSX(reddit_sydney, "reddit-sydney", "Reddit r/sydney", "Reddit r/sydney", "osint", "social",
  "https://www.reddit.com/r/sydney/.rss", "en", "[\"reddit\",\"social\",\"sydney\",\"community\"]", 7200,
  "Reddit r/sydney feed — community-sourced signal (sydney)");

RSSX(reddit_melbourne, "reddit-melbourne", "Reddit r/melbourne", "Reddit r/melbourne", "osint", "social",
  "https://www.reddit.com/r/melbourne/.rss", "en", "[\"reddit\",\"social\",\"melbourne\",\"community\"]", 7200,
  "Reddit r/melbourne feed — community-sourced signal (melbourne)");

RSSX(reddit_delhi, "reddit-delhi", "Reddit r/delhi", "Reddit r/delhi", "osint", "social",
  "https://www.reddit.com/r/delhi/.rss", "en", "[\"reddit\",\"social\",\"delhi\",\"community\"]", 7200,
  "Reddit r/delhi feed — community-sourced signal (delhi)");

RSSX(reddit_mumbai, "reddit-mumbai", "Reddit r/mumbai", "Reddit r/mumbai", "osint", "social",
  "https://www.reddit.com/r/mumbai/.rss", "en", "[\"reddit\",\"social\",\"mumbai\",\"community\"]", 7200,
  "Reddit r/mumbai feed — community-sourced signal (mumbai)");

RSSX(reddit_shanghai, "reddit-shanghai", "Reddit r/shanghai", "Reddit r/shanghai", "osint", "social",
  "https://www.reddit.com/r/shanghai/.rss", "en", "[\"reddit\",\"social\",\"shanghai\",\"community\"]", 7200,
  "Reddit r/shanghai feed — community-sourced signal (shanghai)");

RSSX(reddit_beijing, "reddit-beijing", "Reddit r/beijing", "Reddit r/beijing", "osint", "social",
  "https://www.reddit.com/r/beijing/.rss", "en", "[\"reddit\",\"social\",\"beijing\",\"community\"]", 7200,
  "Reddit r/beijing feed — community-sourced signal (beijing)");

RSSX(reddit_istanbul, "reddit-istanbul", "Reddit r/istanbul", "Reddit r/istanbul", "osint", "social",
  "https://www.reddit.com/r/istanbul/.rss", "en", "[\"reddit\",\"social\",\"istanbul\",\"community\"]", 7200,
  "Reddit r/istanbul feed — community-sourced signal (istanbul)");

RSSX(reddit_bangkok, "reddit-bangkok", "Reddit r/bangkok", "Reddit r/bangkok", "osint", "social",
  "https://www.reddit.com/r/bangkok/.rss", "en", "[\"reddit\",\"social\",\"bangkok\",\"community\"]", 7200,
  "Reddit r/bangkok feed — community-sourced signal (bangkok)");

RSSX(reddit_jakarta, "reddit-jakarta", "Reddit r/jakarta", "Reddit r/jakarta", "osint", "social",
  "https://www.reddit.com/r/jakarta/.rss", "en", "[\"reddit\",\"social\",\"jakarta\",\"community\"]", 7200,
  "Reddit r/jakarta feed — community-sourced signal (jakarta)");

RSSX(reddit_seoul, "reddit-seoul", "Reddit r/Seoul", "Reddit r/Seoul", "osint", "social",
  "https://www.reddit.com/r/Seoul/.rss", "en", "[\"reddit\",\"social\",\"seoul\",\"community\"]", 7200,
  "Reddit r/Seoul feed — community-sourced signal (seoul)");

RSSX(reddit_hongkong_2, "reddit-hongkong-2", "Reddit r/hongkong", "Reddit r/hongkong", "osint", "social",
  "https://www.reddit.com/r/hongkong/.rss", "en", "[\"reddit\",\"social\",\"hong-kong-city\",\"community\"]", 7200,
  "Reddit r/hongkong feed — community-sourced signal (hong-kong-city)");

RSSX(reddit_kyiv, "reddit-kyiv", "Reddit r/kyiv", "Reddit r/kyiv", "osint", "social",
  "https://www.reddit.com/r/kyiv/.rss", "en", "[\"reddit\",\"social\",\"kyiv\",\"community\"]", 7200,
  "Reddit r/kyiv feed — community-sourced signal (kyiv)");
