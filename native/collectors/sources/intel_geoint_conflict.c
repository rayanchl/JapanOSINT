/* collectors/sources/intel_geoint_conflict.c — geopolitical, conflict-monitoring
 * and investigative-OSINT open feeds (think-tank analysis, conflict trackers,
 * cross-border investigative journalism, defense reporting). Real RSS/Atom
 * endpoints via rss_collect; each item becomes an FTS-indexed intel row that the
 * OSINT entity/alert hooks can pivot on. Scheduled hourly. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define COLL     "osint"
#define INTERVAL 3600

#include "_source_macros.inc"

RSS(gc_bellingcat, "bellingcat", "Bellingcat",
  "ベリングキャット", "news",
  "https://www.bellingcat.com/feed/", "en",
  "[\"osint\",\"investigation\",\"geoint\"]",
  "Bellingcat — open-source investigations, geolocation and conflict verification");

RSS(gc_isw, "isw", "Institute for the Study of War",
  "戦争研究所 (ISW)", "news",
  "https://www.understandingwar.org/backgrounder/feed", "en",
  "[\"osint\",\"conflict\",\"analysis\"]",
  "ISW — daily conflict assessments (Ukraine, Russia, Middle East, China)");

RSS(gc_lwj, "long-war-journal", "FDD's Long War Journal",
  "Long War Journal", "news",
  "https://www.longwarjournal.org/feed", "en",
  "[\"osint\",\"conflict\",\"counterterrorism\"]",
  "Long War Journal — militant-group and counterterrorism conflict tracking");

RSS(gc_occrp, "occrp", "OCCRP",
  "OCCRP", "news",
  "https://www.occrp.org/en/feed", "en",
  "[\"osint\",\"investigation\",\"corruption\"]",
  "Organized Crime and Corruption Reporting Project — cross-border investigations");

RSS(gc_icij, "icij", "ICIJ",
  "ICIJ", "news",
  "https://www.icij.org/feed/", "en",
  "[\"osint\",\"investigation\",\"leaks\"]",
  "International Consortium of Investigative Journalists — offshore leaks and finance");

RSS(gc_cfr, "cfr", "Council on Foreign Relations",
  "外交問題評議会 (CFR)", "news",
  /* www.cfr.org/rss.xml has been 404 since the site rebuild (it now returns the
   * HTML 404 page, 269 KB of it); the live site feed is /feed. */
  "https://www.cfr.org/feed", "en",
  "[\"osint\",\"geopolitics\",\"analysis\"]",
  "Council on Foreign Relations — geopolitical analysis and expert briefs");

RSS(gc_wotr, "war-on-the-rocks", "War on the Rocks",
  "War on the Rocks", "news",
  "https://warontherocks.com/feed/", "en",
  "[\"osint\",\"defense\",\"strategy\"]",
  "War on the Rocks — national security, strategy and military analysis");

RSS(gc_cipher, "cipher-brief", "The Cipher Brief",
  "The Cipher Brief", "news",
  "https://www.thecipherbrief.com/feed", "en",
  "[\"osint\",\"intelligence\",\"analysis\"]",
  "The Cipher Brief — national security and intelligence-community analysis");

RSS(gc_diplomat, "the-diplomat", "The Diplomat (Asia-Pacific)",
  "The Diplomat", "news",
  "https://thediplomat.com/feed/", "en",
  "[\"osint\",\"asia-pacific\",\"geopolitics\"]",
  "The Diplomat — Asia-Pacific geopolitics, security and diplomacy");

RSS(gc_defnews, "defense-news", "Defense News",
  "Defense News", "news",
  "https://www.defensenews.com/arc/outboundfeeds/rss/?outputType=xml", "en",
  "[\"osint\",\"defense\",\"procurement\"]",
  "Defense News — global military procurement, programs and policy");

RSS(gc_breakingdef, "breaking-defense", "Breaking Defense",
  "Breaking Defense", "news",
  "https://breakingdefense.com/feed/", "en",
  "[\"osint\",\"defense\",\"technology\"]",
  "Breaking Defense — defense technology, budgets and industry reporting");

RSS(gc_twz, "the-war-zone", "The War Zone (TWZ)",
  "The War Zone", "news",
  "https://www.twz.com/feed", "en",
  "[\"osint\",\"military\",\"aviation\"]",
  "The War Zone — military hardware, aviation and defense analysis");

RSS(gc_navalnews, "naval-news", "Naval News",
  "Naval News", "news",
  "https://www.navalnews.com/feed/", "en",
  "[\"osint\",\"navy\",\"maritime\"]",
  "Naval News — warship programs, naval technology and maritime security");

RSS(gc_meduza, "meduza-en", "Meduza (English)",
  "メドゥーザ", "news",
  "https://meduza.io/rss/en/all", "en",
  "[\"osint\",\"russia\",\"news\"]",
  "Meduza — independent Russia/CIS reporting in English");

RSS(gc_kyivindep, "kyiv-independent", "The Kyiv Independent",
  "キーウ・インディペンデント", "news",
  /* /feed/ 404s; the Ghost feed lives at /feed/rss/. */
  "https://kyivindependent.com/feed/rss/", "en",
  "[\"osint\",\"ukraine\",\"conflict\"]",
  "The Kyiv Independent — Ukraine war and regional reporting");

RSS(gc_moscowtimes, "moscow-times", "The Moscow Times",
  "モスクワ・タイムズ", "news",
  "https://www.themoscowtimes.com/rss/news", "en",
  "[\"osint\",\"russia\",\"news\"]",
  "The Moscow Times — independent English-language Russia coverage");

RSS(gc_rferl, "rferl", "Radio Free Europe / Radio Liberty",
  "RFE/RL", "news",
  /* the old api/zrqiteuuir feed still 200s but its channel has been empty for
   * some time; api/zbqiml-vomx-tpeqkmy is the English "News" feed off
   * rferl.org/rssfeeds and carries 20 items. */
  "https://www.rferl.org/api/zbqiml-vomx-tpeqkmy", "en",
  "[\"osint\",\"eurasia\",\"news\"]",
  "RFE/RL — reporting across Eastern Europe, Caucasus and Central Asia");

RSS(gc_scmp, "scmp-china", "South China Morning Post (China)",
  "サウスチャイナ・モーニング・ポスト", "news",
  "https://www.scmp.com/rss/4/feed", "en",
  "[\"osint\",\"china\",\"geopolitics\"]",
  "South China Morning Post — China politics, economy and regional affairs");

RSS(gc_38north, "38north", "38 North (Korea)",
  "38ノース", "news",
  "https://www.38north.org/feed/", "en",
  "[\"osint\",\"north-korea\",\"analysis\"]",
  "38 North — North Korea analysis, imagery and nuclear/missile tracking");

RSS(gc_maritime_exec, "maritime-executive", "The Maritime Executive",
  "The Maritime Executive", "news",
  /* /rss 301s to a 404; the site's declared feed is the Atom /articles.rss on
   * the apex host (www 301s again). */
  "https://maritime-executive.com/articles.rss", "en",
  "[\"osint\",\"maritime\",\"shipping\"]",
  "The Maritime Executive — shipping, ports, and maritime-security incidents");
