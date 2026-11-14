/* tests/gtfsrt_test.c — offline check of lib/gtfsrt.c, the GTFS Realtime
 * protobuf decoder. `make gtfsrttest`. No network, no protobuf library: every
 * fixture is a byte array encoded by hand from gtfs-realtime.proto, so what
 * is pinned is the WIRE FORMAT, not one library's reading of it.
 *
 * Pinned:
 *   1. a trip update whose stop-time delay is NEGATIVE: int32 is a plain
 *      varint, so -45 is its 64-bit sign extension, ten bytes on the wire —
 *      read as uint32 or truncated it becomes 4294967251 or garbage;
 *   2. an alert whose header carries TWO translations, both kept, and the
 *      language pick choosing between them;
 *   3. an unknown field skipped, of every wire type (varint, fixed64,
 *      length-delimited, a group, fixed32), counted, with the known fields
 *      around it still read;
 *   4. truncated input REJECTED at every cut point of a valid feed — never a
 *      partial result, never a read past the end;
 *   5. the discriminator a collector relies on: a zero-byte body (an empty
 *      protobuf message, and what kumagaya.bus-go.com answered with HTTP 200
 *      on 2026-11-12) is NOT a feed, while a FeedHeader with no entity (58 of
 *      126 Japanese bodies that day) IS one, with zero entities;
 *   6. malformed shapes refused: an over-long varint, a known field with the
 *      wrong wire type, an end-group with no start, wire type 7, field 0,
 *      groups nested past the depth bound;
 *   7. a vehicle position's fixed32 floats and occupancy.
 *
 * The real-world check is not here because it needs the network: on
 * 2026-11-12 the decoder agreed with the reference protobuf bindings on 129
 * live bodies (126 Japanese, MBTA's three) entity for entity and on a
 * 39,718-line field dump. See collectors/sources/gtfs_rt_jp.c. */
#include "../lib/gtfsrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_fail, g_pass;
#define CHECK(cond, ...) do { if (cond) g_pass++; else { g_fail++; \
  printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ── a tiny encoder, so fixtures say what they mean ───────────────────── */
typedef struct { unsigned char b[4096]; size_t n; } buf;

static void put_byte(buf *o, unsigned char c) { if (o->n < sizeof o->b) o->b[o->n++] = c; }
static void put_varint(buf *o, unsigned long long v) {
  do { unsigned char c = v & 0x7F; v >>= 7; if (v) c |= 0x80; put_byte(o, c); } while (v);
}
static void put_tag(buf *o, unsigned f, unsigned wt) { put_varint(o, ((unsigned long long)f << 3) | wt); }
static void put_uint(buf *o, unsigned f, unsigned long long v) { put_tag(o, f, 0); put_varint(o, v); }
/* int32/int64/enum: a negative value is its 64-bit two's complement */
static void put_int(buf *o, unsigned f, long long v) { put_tag(o, f, 0); put_varint(o, (unsigned long long)v); }
static void put_bytes(buf *o, unsigned f, const void *p, size_t n) {
  put_tag(o, f, 2); put_varint(o, n);
  for (size_t i = 0; i < n; i++) put_byte(o, ((const unsigned char *)p)[i]);
}
static void put_str(buf *o, unsigned f, const char *s) { put_bytes(o, f, s, strlen(s)); }
static void put_msg(buf *o, unsigned f, const buf *m) { put_bytes(o, f, m->b, m->n); }
static void put_float(buf *o, unsigned f, float x) {
  unsigned int u; memcpy(&u, &x, 4); put_tag(o, f, 5);
  for (int i = 0; i < 4; i++) put_byte(o, (u >> (8 * i)) & 0xFF);
}
static void put_fixed64(buf *o, unsigned f, unsigned long long u) {
  put_tag(o, f, 1); for (int i = 0; i < 8; i++) put_byte(o, (u >> (8 * i)) & 0xFF);
}

static void header(buf *feed, unsigned long long ts) {
  buf h = {0};
  put_str(&h, 1, "2.0");
  put_uint(&h, 2, 0);                    /* FULL_DATASET */
  put_uint(&h, 3, ts);
  put_msg(feed, 1, &h);
}

/* FeedMessage{header, entity{id "t1", trip_update{trip{trip_id "T9",
 * route_id "R3", start_date "20261112"}, stop_time_update{stop_sequence 4,
 * stop_id "S4", arrival{delay -45, time 1794470000}, departure{delay 120}},
 * stop_time_update{stop_sequence 5, arrival{time 1794470300}}, timestamp,
 * delay -45}}} */
static void build_trip_update(buf *feed) {
  memset(feed, 0, sizeof *feed);
  header(feed, 1794478342ULL);
  buf trip = {0}, arr = {0}, dep = {0}, stu = {0}, stu2 = {0}, arr2 = {0}, tu = {0}, ent = {0};
  put_str(&trip, 1, "T9"); put_str(&trip, 3, "20261112"); put_str(&trip, 5, "R3");
  put_int(&arr, 1, -45); put_int(&arr, 2, 1794470000LL);
  put_int(&dep, 1, 120);
  put_uint(&stu, 1, 4); put_msg(&stu, 2, &arr); put_msg(&stu, 3, &dep); put_str(&stu, 4, "S4");
  put_int(&arr2, 2, 1794470300LL);
  put_uint(&stu2, 1, 5); put_msg(&stu2, 2, &arr2);
  put_msg(&tu, 1, &trip); put_msg(&tu, 2, &stu); put_msg(&tu, 2, &stu2);
  put_uint(&tu, 4, 1794478300ULL); put_int(&tu, 5, -45);
  put_str(&ent, 1, "t1"); put_msg(&ent, 3, &tu);
  put_msg(feed, 2, &ent);
}

static void test_trip_update_negative_delay(void) {
  buf f; build_trip_update(&f);
  gtfsrt_feed d; char err[128];
  int rc = gtfsrt_decode(f.b, f.n, &d, err, sizeof err);
  CHECK(rc == 0, "trip update decodes (%s)", err);
  if (rc) return;
  CHECK(d.version && !strcmp(d.version, "2.0"), "version");
  CHECK(d.has_timestamp && d.timestamp == 1794478342ULL, "header timestamp");
  CHECK(d.has_incrementality && d.incrementality == 0, "FULL_DATASET");
  CHECK(d.nent == 1 && d.ent[0].tu && !d.ent[0].vp && !d.ent[0].al, "one trip update");
  const gtfsrt_trip_update *t = d.ent[0].tu;
  CHECK(!strcmp(d.ent[0].id, "t1"), "entity id");
  CHECK(t->trip.trip_id && !strcmp(t->trip.trip_id, "T9"), "trip_id");
  CHECK(t->trip.route_id && !strcmp(t->trip.route_id, "R3"), "route_id");
  CHECK(t->trip.start_date && !strcmp(t->trip.start_date, "20261112"), "start_date");
  CHECK(t->has_delay && t->delay == -45, "trip-level delay -45, got %d", t->delay);
  CHECK(t->nstu == 2, "two stop-time updates, got %zu", t->nstu);
  const gtfsrt_stu *u = &t->stu[0];
  CHECK(u->has_stop_sequence && u->stop_sequence == 4, "stop_sequence 4");
  CHECK(u->stop_id && !strcmp(u->stop_id, "S4"), "stop_id");
  CHECK(u->has_arrival && u->arrival.has_delay && u->arrival.delay == -45,
        "NEGATIVE arrival delay -45 (a ten-byte varint), got %d", u->arrival.delay);
  CHECK(u->arrival.has_time && u->arrival.time == 1794470000LL, "arrival time");
  CHECK(u->has_departure && u->departure.has_delay && u->departure.delay == 120, "departure delay");
  CHECK(!u->departure.has_time, "departure time absent is absent, not 0");
  const gtfsrt_stu *u2 = &t->stu[1];
  CHECK(u2->has_arrival && !u2->arrival.has_delay && u2->arrival.has_time,
        "time-only event: has_time, no delay");
  CHECK(!u2->has_departure && !u2->stop_id, "absent departure/stop_id stay absent");
  CHECK(d.unknown_fields == 0, "nothing skipped");
  gtfsrt_free(&d);

  /* the ten-byte encoding really is what was on the wire */
  buf neg = {0}; put_varint(&neg, (unsigned long long)(long long)-45);
  CHECK(neg.n == 10, "-45 as int32 varint is 10 bytes, got %zu", neg.n);
}

static void test_alert_two_translations(void) {
  buf f = {0}; header(&f, 1794478591ULL);
  buf ie1 = {0}, ie2 = {0}, tripd = {0}, tr_ja = {0}, tr_en = {0}, hdr = {0},
      desc = {0}, tr_d = {0}, per = {0}, al = {0}, ent = {0};
  put_str(&ie1, 2, "R1"); put_str(&ie1, 5, "S7");
  put_str(&tripd, 1, "T2"); put_msg(&ie2, 4, &tripd); put_uint(&ie2, 6, 1);
  put_str(&tr_ja, 1, "迂回運行"); put_str(&tr_ja, 2, "ja");
  put_str(&tr_en, 1, "Detour"); put_str(&tr_en, 2, "en");
  put_msg(&hdr, 1, &tr_ja); put_msg(&hdr, 1, &tr_en);
  put_str(&tr_d, 1, "工事により停車しません");           /* no language tag */
  put_msg(&desc, 1, &tr_d);
  put_uint(&per, 1, 1787497200ULL); put_uint(&per, 2, 1800111540ULL);
  put_msg(&al, 1, &per);
  put_msg(&al, 5, &ie1); put_msg(&al, 5, &ie2);
  put_uint(&al, 6, 10);                                   /* CONSTRUCTION */
  put_uint(&al, 7, 4);                                    /* DETOUR */
  put_msg(&al, 10, &hdr); put_msg(&al, 11, &desc);
  put_uint(&al, 14, 3);                                   /* WARNING */
  put_str(&ent, 1, "AR1"); put_msg(&ent, 5, &al);
  put_msg(&f, 2, &ent);

  gtfsrt_feed d; char err[128];
  int rc = gtfsrt_decode(f.b, f.n, &d, err, sizeof err);
  CHECK(rc == 0, "alert decodes (%s)", err);
  if (rc) return;
  const gtfsrt_alert *a = d.nent == 1 ? d.ent[0].al : NULL;
  CHECK(a != NULL, "one alert");
  if (!a) { gtfsrt_free(&d); return; }
  CHECK(a->header.n == 2, "BOTH header translations kept, got %zu", a->header.n);
  CHECK(a->header.n == 2 && !strcmp(a->header.tr[0].text, "迂回運行") &&
        !strcmp(a->header.tr[0].language, "ja") &&
        !strcmp(a->header.tr[1].text, "Detour") &&
        !strcmp(a->header.tr[1].language, "en"), "translation text + language, in order");
  CHECK(!strcmp(gtfsrt_tstring_pick(&a->header, "en"), "Detour"), "pick en");
  CHECK(!strcmp(gtfsrt_tstring_pick(&a->header, "JA"), "迂回運行"), "pick ja, case-insensitive");
  CHECK(!strcmp(gtfsrt_tstring_pick(&a->header, "fr"), "迂回運行"),
        "no fr and no untagged: the first");
  CHECK(!strcmp(gtfsrt_tstring_pick(&a->description, "ja"), "工事により停車しません"),
        "untagged translation chosen when the language is absent");
  CHECK(a->description.n == 1 && a->description.tr[0].language == NULL,
        "an absent language is NULL, not \"\"");
  CHECK(a->has_cause && a->cause == 10 && !strcmp(gtfsrt_cause_name(a->cause), "CONSTRUCTION"), "cause");
  CHECK(a->has_effect && a->effect == 4 && !strcmp(gtfsrt_effect_name(a->effect), "DETOUR"), "effect");
  CHECK(a->has_severity_level && !strcmp(gtfsrt_severity_name(a->severity_level), "WARNING"), "severity");
  CHECK(a->nperiods == 1 && a->periods[0].start == 1787497200ULL &&
        a->periods[0].end == 1800111540ULL, "active period");
  CHECK(a->nie == 2 && !strcmp(a->ie[0].route_id, "R1") && !strcmp(a->ie[0].stop_id, "S7"),
        "informed entity 1");
  CHECK(a->nie == 2 && a->ie[1].has_trip && !strcmp(a->ie[1].trip.trip_id, "T2") &&
        a->ie[1].has_direction_id && a->ie[1].direction_id == 1 && !a->ie[1].route_id,
        "informed entity 2 by trip");
  CHECK(gtfsrt_cause_name(99) == NULL && gtfsrt_effect_name(0) == NULL,
        "an enum value the .proto does not name has no name");
  gtfsrt_free(&d);
}

static void test_unknown_fields_skipped(void) {
  buf f = {0}; header(&f, 1);
  buf vp = {0}, pos = {0}, ent = {0}, trip = {0}, grp_inner = {0};
  (void)grp_inner;
  put_str(&trip, 1, "T1");
  put_float(&pos, 1, 35.6812f); put_float(&pos, 2, 139.7671f);
  put_uint(&pos, 999, 7);                        /* unknown varint inside Position */
  put_msg(&vp, 1, &trip);
  put_fixed64(&vp, 1001, 0x0102030405060708ULL); /* unknown fixed64 (an extension number) */
  put_msg(&vp, 2, &pos);
  put_str(&vp, 1002, "ignored payload");         /* unknown length-delimited */
  /* unknown GROUP 1003 with a nested group 1 and a varint inside */
  put_tag(&vp, 1003, 3); put_tag(&vp, 1, 3); put_uint(&vp, 2, 5); put_tag(&vp, 1, 4);
  put_uint(&vp, 3, 9); put_tag(&vp, 1003, 4);
  put_tag(&vp, 1004, 5); put_byte(&vp, 1); put_byte(&vp, 2); put_byte(&vp, 3); put_byte(&vp, 4);
  put_uint(&vp, 5, 1794478000ULL);               /* known field AFTER the unknowns */
  put_uint(&vp, 9, 2);                           /* occupancy FEW_SEATS_AVAILABLE */
  put_str(&ent, 1, "v1"); put_msg(&ent, 4, &vp);
  put_uint(&ent, 6, 1);                          /* FeedEntity.shape-number with a varint: unknown */
  put_msg(&f, 2, &ent);
  put_uint(&f, 77, 1);                           /* unknown at the top level too */

  gtfsrt_feed d; char err[128];
  int rc = gtfsrt_decode(f.b, f.n, &d, err, sizeof err);
  CHECK(rc == 0, "unknown fields are skipped, not fatal (%s)", err);
  if (rc) return;
  CHECK(d.unknown_fields == 7, "seven unknown fields counted, got %ld", d.unknown_fields);
  const gtfsrt_vehicle *v = d.nent == 1 ? d.ent[0].vp : NULL;
  CHECK(v && v->has_position && fabs(v->lat - 35.6812) < 1e-4 && fabs(v->lon - 139.7671) < 1e-4,
        "fixed32 floats around the unknowns");
  CHECK(v && v->has_timestamp && v->timestamp == 1794478000ULL, "known field after the skipped ones");
  CHECK(v && v->trip.trip_id && !strcmp(v->trip.trip_id, "T1"), "trip before the skipped ones");
  CHECK(v && v->has_occupancy_status &&
        !strcmp(gtfsrt_occupancy_name(v->occupancy_status), "FEW_SEATS_AVAILABLE"), "occupancy");
  CHECK(v && !v->has_bearing && !v->has_speed, "absent floats stay absent");
  gtfsrt_free(&d);

  /* A Position missing longitude is not a position (both are required). */
  buf f2 = {0}; header(&f2, 1);
  buf vp2 = {0}, pos2 = {0}, ent2 = {0};
  put_float(&pos2, 1, 35.0f); put_msg(&vp2, 2, &pos2);
  put_str(&ent2, 1, "v2"); put_msg(&ent2, 4, &vp2); put_msg(&f2, 2, &ent2);
  rc = gtfsrt_decode(f2.b, f2.n, &d, err, sizeof err);
  CHECK(rc == 0 && d.nent == 1 && d.ent[0].vp && !d.ent[0].vp->has_position,
        "lat without lon is no position, never 35,0");
  if (!rc) gtfsrt_free(&d);
}

static void test_truncation_rejected(void) {
  buf f; build_trip_update(&f);
  int rejected = 0, accepted = 0;
  for (size_t cut = 1; cut < f.n; cut++) {
    /* a heap copy of exactly `cut` bytes, so a read past the end is a real
     * out-of-bounds access under ASan rather than a read of the fixture */
    unsigned char *p = malloc(cut);
    memcpy(p, f.b, cut);
    gtfsrt_feed d; char err[128];
    int rc = gtfsrt_decode(p, cut, &d, err, sizeof err);
    if (rc != 0) {
      rejected++;
      CHECK(d.nent == 0 && d.ent == NULL && d.version == NULL,
            "a rejected decode hands back nothing (cut %zu)", cut);
      CHECK(err[0] != 0, "a rejection names its reason (cut %zu)", cut);
    } else {
      /* Protobuf is self-delimiting only per field: a cut that lands exactly
       * between two TOP-LEVEL fields is a shorter valid message. That can only
       * happen after the header, and then the entity is simply absent. */
      accepted++;
      CHECK(d.nent == 0, "a cut accepted only at a top-level field boundary (cut %zu)", cut);
      gtfsrt_free(&d);
    }
    free(p);
  }
  CHECK(rejected > 0 && accepted <= 1,
        "every cut inside a field is rejected (%d rejected, %d accepted)", rejected, accepted);

  /* a length prefix that claims more than the buffer holds */
  unsigned char lie[] = { 0x0A, 0x7F, 0x0A, 0x03, '2', '.', '0' };
  gtfsrt_feed d; char err[128];
  CHECK(gtfsrt_decode(lie, sizeof lie, &d, err, sizeof err) != 0, "length runs past end");
  /* a varint that never terminates */
  unsigned char run[] = { 0x08, 0xFF, 0xFF, 0xFF };
  CHECK(gtfsrt_decode(run, sizeof run, &d, err, sizeof err) != 0, "unterminated varint");
}

static void test_empty_vs_not_a_feed(void) {
  gtfsrt_feed d; char err[128];
  CHECK(gtfsrt_decode((const unsigned char *)"", 0, &d, err, sizeof err) != 0,
        "zero bytes is NOT a feed (an empty message has no FeedHeader)");
  CHECK(strstr(err, "FeedHeader") != NULL, "and says why: %s", err);
  CHECK(gtfsrt_decode(NULL, 0, &d, err, sizeof err) != 0, "NULL, 0 likewise");

  buf f = {0}; header(&f, 1794478342ULL);
  CHECK(f.n == 15, "the measured 15-byte bare header, got %zu", f.n);
  int rc = gtfsrt_decode(f.b, f.n, &d, err, sizeof err);
  CHECK(rc == 0 && d.nent == 0, "a header with no entity IS a feed, with nothing in it");
  if (!rc) gtfsrt_free(&d);

  buf nov = {0}, h = {0}; put_uint(&h, 3, 5); put_msg(&nov, 1, &h);
  CHECK(gtfsrt_decode(nov.b, nov.n, &d, err, sizeof err) != 0,
        "a header without gtfs_realtime_version is refused");

  /* an HTML body: '<' is field 7 wire type 4, an end-group with no start */
  const char *html = "<!DOCTYPE html><html>";
  CHECK(gtfsrt_decode((const unsigned char *)html, strlen(html), &d, err, sizeof err) != 0,
        "an HTML page is refused");
  const char *json = "{\"error\":\"x\"}";
  CHECK(gtfsrt_decode((const unsigned char *)json, strlen(json), &d, err, sizeof err) != 0,
        "a JSON body is refused");
}

static void test_malformed(void) {
  gtfsrt_feed d; char err[128];
  buf f = {0}; header(&f, 1);
  buf b;

  b = f; put_tag(&b, 9, 0);
  for (int i = 0; i < 10; i++) put_byte(&b, 0xFF);
  put_byte(&b, 0x01);
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "11-byte varint refused");

  b = f; put_tag(&b, 9, 0);
  for (int i = 0; i < 9; i++) put_byte(&b, 0xFF);
  put_byte(&b, 0x02);
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "varint overflowing 64 bits refused");

  b = f; put_uint(&b, 2, 5);           /* entity (a message) as a varint */
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "known field, wrong wire type");

  b = f; put_tag(&b, 9, 4);
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "end-group without start");

  b = f; put_tag(&b, 9, 7);
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "wire type 7");

  b = f; put_byte(&b, 0x00);
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "field number 0");

  b = f; put_tag(&b, 9, 3); put_tag(&b, 10, 4);
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "mismatched end-group");

  b = f;
  for (int i = 0; i < GTFSRT_MAX_DEPTH + 2; i++) put_tag(&b, 9, 3);
  for (int i = 0; i < GTFSRT_MAX_DEPTH + 2; i++) put_tag(&b, 9, 4);
  CHECK(gtfsrt_decode(b.b, b.n, &d, err, sizeof err) != 0, "groups nested past the bound");

  b = f;
  for (int i = 0; i < GTFSRT_MAX_DEPTH - 1; i++) put_tag(&b, 9, 3);
  for (int i = 0; i < GTFSRT_MAX_DEPTH - 1; i++) put_tag(&b, 9, 4);
  int rc = gtfsrt_decode(b.b, b.n, &d, err, sizeof err);
  CHECK(rc == 0, "groups nested inside the bound are skipped (%s)", err);
  if (!rc) gtfsrt_free(&d);

  CHECK(gtfsrt_decode(f.b, f.n, NULL, err, sizeof err) != 0, "NULL out refused");
}

int main(void) {
  test_trip_update_negative_delay();
  test_alert_two_translations();
  test_unknown_fields_skipped();
  test_truncation_rejected();
  test_empty_vs_not_a_feed();
  test_malformed();
  printf("gtfsrt_test: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
