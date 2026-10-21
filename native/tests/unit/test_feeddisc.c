/* test_feeddisc.c — the parsers behind ./bin/japanosint --discover-feeds.
 *
 * Each case is a mistake batch 50's discovery made or nearly made:
 *  - an href with &amp; must be decoded (Joomla served Atom where &amp; stayed);
 *  - WordPress declares a comments feed beside every posts feed — skipped;
 *  - relative hrefs resolve against the page asked for, or its <base href>;
 *  - RSS 1.0's <items> container is not an item;
 *  - "casino" inside "Liceo Pascasino" and "cialis" inside "Commercialisti"
 *    are not spam, while a city feed of Vietnamese betting posts is. */
#include "core/feeddisc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { char url[8][512]; char title[8][128]; int n; } got;

static void cb(const char *u, const char *t, void *ud) {
  got *g = ud;
  assert(g->n < 8);
  snprintf(g->url[g->n], 512, "%s", u);
  snprintf(g->title[g->n], 128, "%s", t);
  g->n++;
}

int main(void) {
  got g = {0};
  const char *page =
    "<html><head><title>Town</title>"
    "<link rel=\"alternate\" type=\"application/rss+xml\" title=\"News\" href=\"/feed/\">"
    "<link rel=\"alternate\" type=\"application/rss+xml\" title=\"Comments\" href=\"/comments/feed/\">"
    "<LINK REL=\"alternate\" TYPE=\"application/atom+xml\" href=\"index.php?format=feed&amp;type=atom\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"/s.css\">"
    "<link rel=\"alternate\" type=\"application/rss+xml\" href=\"//cdn.example.org/x.rss\">"
    "</head></html>";
  int n = feeddisc_links(page, "https://www.town.example.lg.jp/top/", cb, &g);
  assert(n == 3 && g.n == 3);
  assert(!strcmp(g.url[0], "https://www.town.example.lg.jp/feed/"));
  assert(!strcmp(g.title[0], "News"));
  assert(!strcmp(g.url[1], "https://www.town.example.lg.jp/top/index.php?format=feed&type=atom"));
  assert(!strcmp(g.url[2], "https://cdn.example.org/x.rss"));
  printf("  links: absolute, relative, protocol-relative, &amp; decoded, comments skipped: ok\n");

  got b = {0};
  feeddisc_links("<base href=\"https://other.example.fr/site/\">"
                 "<link rel='alternate' type='application/rss+xml' href='rss.xml'>",
                 "https://example.fr/", cb, &b);
  assert(b.n == 1 && !strcmp(b.url[0], "https://other.example.fr/site/rss.xml"));
  printf("  <base href> wins over the page URL: ok\n");

  got d = {0};
  feeddisc_links("<link rel=alternate type=application/rss+xml href=./rss/rss.xml>"
                 "<link rel=alternate type=application/rss+xml href=../up/feed.xml>",
                 "https://www.town.shimoichi.lg.jp/a/b/", cb, &d);
  assert(d.n == 2);
  assert(!strcmp(d.url[0], "https://www.town.shimoichi.lg.jp/a/b/rss/rss.xml"));
  assert(!strcmp(d.url[1], "https://www.town.shimoichi.lg.jp/a/up/feed.xml"));
  printf("  ./ and ../ segments removed: ok\n");

  const char *rdf = "<?xml version=\"1.0\"?><rdf:RDF><channel><items><rdf:Seq/></items></channel>"
                    "<item rdf:about=\"a\"><title>x</title></item><item><title>y</title></item></rdf:RDF>";
  assert(feeddisc_is_feed(rdf));
  assert(feeddisc_count_items(rdf) == 2);
  assert(feeddisc_count_items("<feed><entry><title>a</title></entry></feed>") == 1);
  assert(!feeddisc_is_feed("<!doctype html><html><body>not a feed</body></html>"));
  printf("  item count ignores the RSS 1.0 <items> container: ok\n");

  char why[160];
  const char *school = "<rss><channel><title>Liceo Statale Pascasino</title>"
    "<item><title>Liceo Pascasino: open day</title></item>"
    "<item><title>Ordine dei Dottori Commercialisti</title></item>"
    "<item><title>Specialist appointments</title></item></channel></rss>";
  assert(!feeddisc_takeover_suspect(school, 0, why, sizeof why));
  const char *hijacked = "<rss><channel><title>City of Hudson</title>"
    "<item><title>Review Nhà Cái 11Bet Và Thương Hiệu Sunwin</title></item></channel></rss>";
  assert(feeddisc_takeover_suspect(hijacked, 0, why, sizeof why));
  assert(strstr(why, "phrase"));
  const char *slots = "<rss><channel><title>South Houston</title>"
    "<item><title>Best slots tonight</title></item><item><title>Casino tips</title></item>"
    "<item><title>Jackpot strategy</title></item></channel></rss>";
  assert(feeddisc_takeover_suspect(slots, 0, why, sizeof why));
  const char *lottery = "<rss><channel><title>Gaming regulator</title>"
    "<item><title>Casino licence renewed</title></item></channel></rss>";
  assert(!feeddisc_takeover_suspect(lottery, 0, why, sizeof why));
  const char *vn = "<rss><channel><title>Báo</title>"
    "<item><title>Hội nghị phát triển kinh tế vùng đồng bằng sông Cửu Long được tổ chức</title></item>"
    "</channel></rss>";
  assert(feeddisc_takeover_suspect(vn, 0, why, sizeof why));
  assert(!feeddisc_takeover_suspect(vn, 1, why, sizeof why));
  const char *serbian = "<rss><channel><title>Blic</title>"
    "<item><title>Đoković pobedio, đaci i građani slave u Beogradu, međunarodni uspeh</title></item>"
    "</channel></rss>";
  assert(!feeddisc_takeover_suspect(serbian, 0, why, sizeof why));
  const char *vnlottery = "<rss><channel><title>Tuoi Tre</title>"
    "<item><title>Kết quả xổ số miền Bắc hôm nay</title></item></channel></rss>";
  assert(!feeddisc_takeover_suspect(vnlottery, 1, why, sizeof why));
  assert(feeddisc_takeover_suspect(vnlottery, 0, why, sizeof why));
  printf("  takeover check: whole words, phrases, one regulator word is not spam, "
         "Vietnamese only off a Vietnamese source: ok\n");

  printf("test_feeddisc: ok\n");
  return 0;
}
