/* Reddit geo/country/city/OSINT subreddit feeds — CONSOLIDATED multireddit
 * groups (deterministic /r/<a>+<b>+…/.rss). Community signal + local
 * reporting worldwide, via rss_collect.
 *
 * WHY GROUPS AND NOT ONE SOURCE PER SUBREDDIT (2026-08-25, agent B).
 *
 * This file used to register 142 sources, one per subreddit, all against ONE
 * host, www.reddit.com. The 2026-08-24 registry sweep found 104 of the 142 in
 * `fetch-failure` (rc=-1). Re-probed today with the engine's own User-Agent
 * (JO_USER_AGENT, core/httpclient.h):
 *
 *     r/worldnews    200  43,481 bytes      <- first request in the window
 *     r/europe       429  (3 s later)
 *     r/geopolitics  429  (3 s later)
 *
 * So the host is NOT dead and does NOT block our UA — it rate-limits per
 * client IP at roughly one request per 30 s (same floor the 2026-07-31 audit
 * note measured). 142 sources sharing one interval become due together and
 * stay clustered forever (core/scheduler.c re-arms next[i] = now + interval),
 * and the per-host gate (core/hostgate.h, default 150 ms gap) is two orders of
 * magnitude too permissive for this host. No per-source interval can fix a
 * family that fires as a block; the per-host floor lives in shared code this
 * file must not patch.
 *
 * The fix is reddit's own multireddit endpoint: /r/a+b+c/.rss?limit=100 is one
 * request that returns the newest posts across the whole group, each entry
 * carrying its subreddit in <category term="…"> and its own permalink.
 * Verified today: 200, 193,124 bytes, 100 entries spanning all five subs
 * probed. 17 grouped requests per hour replace 142 requests per cycle — an
 * average of one reddit request every ~3.5 minutes, safely under the floor —
 * and a burst at boot self-heals because each group retries on its own
 * interval.
 *
 * WHAT THIS COST, STATED PLAINLY: `limit=100` is reddit's own per-request
 * ceiling, so a group whose members post more than 100 times per interval
 * yields its newest 100 — reddit's bound, not a slice this collector takes;
 * RSS has no second page to fetch. High-traffic subs therefore sit in SMALL
 * groups on a SHORT interval. The 142 per-subreddit ids
 * (reddit-worldnews … reddit-kyiv) are retired as consolidated-into-groups,
 * not as dead: every subreddit they covered appears in exactly one group URL
 * below. Rows previously stored under the old ids remain in the DB under
 * those ids.
 *
 * All 142 subreddits of the old roster, distributed 1:1 into 17 groups: */
#include "source.h"
#include "lib/rss_atom.h"

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#include "_source_macros.inc"

/* High-traffic global news subs: small group, short interval. */
RSSX(reddit_grp_worldnews, "reddit-group-worldnews", "Reddit world news (r/worldnews+news+anime_titties+neutralnews+qualitynews)", "Reddit world news (r/worldnews+news+anime_titties+neutralnews+qualitynews)", "osint", "social",
  "https://www.reddit.com/r/worldnews+news+anime_titties+neutralnews+qualitynews/.rss?limit=100", "en", "[\"reddit\",\"social\",\"world\",\"community\"]", 1800,
  "Multireddit feed: worldnews, news, anime_titties, neutralnews, qualitynews — community world-news signal (one request covers the group; entries carry their subreddit in <category>)");

RSSX(reddit_grp_conflict, "reddit-group-conflict", "Reddit conflict monitoring (r/CombatFootage+UkraineWarVideoReport+…)", "Reddit conflict monitoring (r/CombatFootage+UkraineWarVideoReport+…)", "osint", "social",
  "https://www.reddit.com/r/CombatFootage+UkraineWarVideoReport+syriancivilwar+YemeniCrisis+IsraelPalestine+MiddleEastNews/.rss?limit=100", "en", "[\"reddit\",\"social\",\"conflict\",\"community\"]", 1800,
  "Multireddit feed: CombatFootage, UkraineWarVideoReport, syriancivilwar, YemeniCrisis, IsraelPalestine, MiddleEastNews — conflict footage and reporting");

RSSX(reddit_grp_defense, "reddit-group-defense", "Reddit defense analysis (r/CredibleDefense+WarCollege+…)", "Reddit defense analysis (r/CredibleDefense+WarCollege+…)", "osint", "social",
  "https://www.reddit.com/r/CredibleDefense+LessCredibleDefence+WarCollege+aviation+navy+Military+TankPorn/.rss?limit=100", "en", "[\"reddit\",\"social\",\"defense\",\"community\"]", 3600,
  "Multireddit feed: CredibleDefense, LessCredibleDefence, WarCollege, aviation, navy, Military, TankPorn — defense and military analysis");

RSSX(reddit_grp_intel, "reddit-group-intel", "Reddit intelligence/geopolitics (r/OSINT+Intelligence+geopolitics+…)", "Reddit intelligence/geopolitics (r/OSINT+Intelligence+geopolitics+…)", "osint", "social",
  "https://www.reddit.com/r/OSINT+Intelligence+NatSecPol+geopolitics+geopolitics2+China_Debate/.rss?limit=100", "en", "[\"reddit\",\"social\",\"geopolitics\",\"community\"]", 3600,
  "Multireddit feed: OSINT, Intelligence, NatSecPol, geopolitics, geopolitics2, China_Debate — intelligence tradecraft and geopolitical analysis");

RSSX(reddit_grp_cyber, "reddit-group-cyber", "Reddit cybersecurity (r/cybersecurity+netsec+Malware+…)", "Reddit cybersecurity (r/cybersecurity+netsec+Malware+…)", "osint", "social",
  "https://www.reddit.com/r/hackernews+cybersecurity+netsec+Malware+blackhat/.rss?limit=100", "en", "[\"reddit\",\"social\",\"cyber\",\"community\"]", 3600,
  "Multireddit feed: hackernews, cybersecurity, netsec, Malware, blackhat — security community signal");

RSSX(reddit_grp_eur_west, "reddit-group-europe-west", "Reddit western Europe (r/europe+unitedkingdom+france+germany+…)", "Reddit western Europe (r/europe+unitedkingdom+france+germany+…)", "osint", "social",
  "https://www.reddit.com/r/europe+unitedkingdom+ukpolitics+france+germany+italy+spain+portugal+netherlands+belgium+ireland/.rss?limit=100", "en", "[\"reddit\",\"social\",\"europe\",\"community\"]", 3600,
  "Multireddit feed: europe, unitedkingdom, ukpolitics, france, germany, italy, spain, portugal, netherlands, belgium, ireland — western-Europe country subs");

RSSX(reddit_grp_eur_north, "reddit-group-europe-north", "Reddit northern Europe/Baltics (r/sweden+norway+denmark+…)", "Reddit northern Europe/Baltics (r/sweden+norway+denmark+…)", "osint", "social",
  "https://www.reddit.com/r/sweden+norway+denmark+Finland+iceland+lithuania+latvia+Eesti/.rss?limit=100", "en", "[\"reddit\",\"social\",\"europe\",\"nordics\",\"community\"]", 3600,
  "Multireddit feed: sweden, norway, denmark, Finland, iceland, lithuania, latvia, Eesti — Nordic and Baltic country subs");

RSSX(reddit_grp_eur_central, "reddit-group-europe-central", "Reddit central/southeast Europe (r/poland+czech+hungary+greece+…)", "Reddit central/southeast Europe (r/poland+czech+hungary+greece+…)", "osint", "social",
  "https://www.reddit.com/r/poland+czech+Slovakia+hungary+Romania+bulgaria+greece+croatia+serbia+Austria+switzerland/.rss?limit=100", "en", "[\"reddit\",\"social\",\"europe\",\"community\"]", 3600,
  "Multireddit feed: poland, czech, Slovakia, hungary, Romania, bulgaria, greece, croatia, serbia, Austria, switzerland — central and southeast Europe country subs");

RSSX(reddit_grp_eurasia, "reddit-group-eurasia", "Reddit Russia/Caucasus/Central Asia (r/russia+ukraina+Kazakhstan+…)", "Reddit Russia/Caucasus/Central Asia (r/russia+ukraina+Kazakhstan+…)", "osint", "social",
  "https://www.reddit.com/r/russia+ukraina+belarus+turkey+Kazakhstan+uzbekistan+azerbaijan+armenia+Sakartvelo+mongolia/.rss?limit=100", "en", "[\"reddit\",\"social\",\"eurasia\",\"community\"]", 3600,
  "Multireddit feed: russia, ukraina, belarus, turkey, Kazakhstan, uzbekistan, azerbaijan, armenia, Sakartvelo, mongolia — Russia, Caucasus and Central Asia country subs");

RSSX(reddit_grp_southasia, "reddit-group-southasia", "Reddit South Asia (r/india+pakistan+bangladesh+afghanistan+…)", "Reddit South Asia (r/india+pakistan+bangladesh+afghanistan+…)", "osint", "social",
  "https://www.reddit.com/r/india+IndiaSpeaks+southasia+pakistan+bangladesh+Nepal+srilanka+myanmar+afghanistan/.rss?limit=100", "en", "[\"reddit\",\"social\",\"south-asia\",\"community\"]", 3600,
  "Multireddit feed: india, IndiaSpeaks, southasia, pakistan, bangladesh, Nepal, srilanka, myanmar, afghanistan — South-Asia country subs");

RSSX(reddit_grp_eastasia, "reddit-group-eastasia", "Reddit East Asia (r/china+japan+korea+taiwan+…)", "Reddit East Asia (r/china+japan+korea+taiwan+…)", "osint", "social",
  "https://www.reddit.com/r/china+China_irl+japan+korea+taiwan+HongKong+asia/.rss?limit=100", "en", "[\"reddit\",\"social\",\"east-asia\",\"community\"]", 3600,
  "Multireddit feed: china, China_irl, japan, korea, taiwan, HongKong, asia — East-Asia country subs");

RSSX(reddit_grp_seasia_oceania, "reddit-group-seasia-oceania", "Reddit Southeast Asia/Oceania (r/indonesia+Thailand+australia+…)", "Reddit Southeast Asia/Oceania (r/indonesia+Thailand+australia+…)", "osint", "social",
  "https://www.reddit.com/r/indonesia+Thailand+VietNam+Philippines+malaysia+singapore+australia+newzealand/.rss?limit=100", "en", "[\"reddit\",\"social\",\"southeast-asia\",\"oceania\",\"community\"]", 3600,
  "Multireddit feed: indonesia, Thailand, VietNam, Philippines, malaysia, singapore, australia, newzealand — Southeast-Asia and Oceania country subs");

RSSX(reddit_grp_americas, "reddit-group-americas", "Reddit Americas (r/canada+mexico+brasil+argentina+…)", "Reddit Americas (r/canada+mexico+brasil+argentina+…)", "osint", "social",
  "https://www.reddit.com/r/canada+mexico+brasil+argentina+chile+Colombia+PERU+vzla+ecuador+bolivia+uruguay+latinamerica/.rss?limit=100", "en", "[\"reddit\",\"social\",\"americas\",\"community\"]", 3600,
  "Multireddit feed: canada, mexico, brasil, argentina, chile, Colombia, PERU, vzla, ecuador, bolivia, uruguay, latinamerica — Americas country subs");

RSSX(reddit_grp_africa, "reddit-group-africa", "Reddit Africa (r/africa+southafrica+Nigeria+Kenya+…)", "Reddit Africa (r/africa+southafrica+Nigeria+Kenya+…)", "osint", "social",
  "https://www.reddit.com/r/africa+southafrica+Nigeria+Kenya+egypt+Morocco+ethiopia+ghana/.rss?limit=100", "en", "[\"reddit\",\"social\",\"africa\",\"community\"]", 3600,
  "Multireddit feed: africa, southafrica, Nigeria, Kenya, egypt, Morocco, ethiopia, ghana — Africa country subs");

RSSX(reddit_grp_mideast, "reddit-group-mideast", "Reddit Middle East (r/iran+Israel+saudiarabia+iraq+…)", "Reddit Middle East (r/iran+Israel+saudiarabia+iraq+…)", "osint", "social",
  "https://www.reddit.com/r/iran+Israel+saudiarabia+dubai+qatar+lebanon+iraq+jordan+Syria/.rss?limit=100", "en", "[\"reddit\",\"social\",\"middle-east\",\"community\"]", 3600,
  "Multireddit feed: iran, Israel, saudiarabia, dubai, qatar, lebanon, iraq, jordan, Syria — Middle-East country subs");

RSSX(reddit_grp_cities_west, "reddit-group-cities-west", "Reddit western cities (r/london+paris+berlin+nyc+…)", "Reddit western cities (r/london+paris+berlin+nyc+…)", "osint", "social",
  "https://www.reddit.com/r/london+paris+berlin+moscow+nyc+LosAngeles+toronto+sydney+melbourne/.rss?limit=100", "en", "[\"reddit\",\"social\",\"cities\",\"community\"]", 3600,
  "Multireddit feed: london, paris, berlin, moscow, nyc, LosAngeles, toronto, sydney, melbourne — western/global city subs");

RSSX(reddit_grp_cities_asia, "reddit-group-cities-asia", "Reddit Asian cities (r/Tokyo+delhi+shanghai+Seoul+…)", "Reddit Asian cities (r/Tokyo+delhi+shanghai+Seoul+…)", "osint", "social",
  "https://www.reddit.com/r/Tokyo+delhi+mumbai+shanghai+beijing+istanbul+bangkok+jakarta+Seoul+hongkong+kyiv/.rss?limit=100", "en", "[\"reddit\",\"social\",\"cities\",\"community\"]", 3600,
  "Multireddit feed: Tokyo, delhi, mumbai, shanghai, beijing, istanbul, bangkok, jakarta, Seoul, hongkong, kyiv — Asian/Eurasian city subs");
