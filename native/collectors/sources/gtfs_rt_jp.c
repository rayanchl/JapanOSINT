/* collectors/sources/gtfs_rt_jp.c — GTFS Realtime for the Japanese operators
 * whose static GTFS gtfs_jp.c already loads.
 *
 * WHY THIS EXISTS. The departures board (core/transitapi.c query_stop_times,
 * the trip detail's next_stop, collect_alerts) LEFT JOINs gtfs_rt_trip_updates
 * and reads gtfs_rt_alerts, and until 2026-11-12 nothing wrote either table, so
 * every delay on the board was NULL and every alert list empty. The protobuf
 * decoder that was missing is lib/gtfsrt.c.
 *
 * WHERE THE FEEDS COME FROM — the publisher's own catalogue, not a list in C.
 * api.gtfs-data.jp/v2/feeds answers every feed it hosts (610 on 2026-11-12,
 * one request, 568,991 bytes) and each carries a `real_time` block of
 * trip_update_url / vehicle_position_url / alert_url. 55 feeds of 610 carry at
 * least one (52 trip-update, 54 vehicle-position, 23 alert URLs; 129 URLs, 127
 * distinct, on 9 hosts — gtfs-rt-files.buscatch.jp 63, bus-vision.jp 30,
 * realtime.gtfs.info 15, km.bus-vision.jp 12, nagai.nolbe.net 3, an S3 bucket
 * 3, and one each on three more). Every one is keyless. The `organization_id`
 * and `feed_id` there are the same keys gtfs_jp.c writes into gtfs_stop_times
 * (it reads /v2/files, whose rows carry the same two ids), so a row written
 * here joins the static timetable with no mapping table.
 *
 * FIVE of the 55 are marked `feed_is_discontinued` and are NOT polled (9
 * URLs): four Toyama buscatch feeds that answered a bare 15-byte header, and
 * Nagai's `tamarin`, whose one alert sits in a feed whose header is dated
 * 2024-09-30 — the timetable they describe is retired, so nothing they say
 * can be about a departure. (Those four buscatch feeds also account for the
 * two duplicate URLs: kurobecity's discontinued `ishida-aimoto` lists the
 * same two .pb files as its live `shinkansenikuji-ishida-aimoto-minibus`.)
 * That leaves 120 URLs on 50 feeds; 49 of the 50 have a current static file
 * in /v2/files, and the 50th (uozucity `omotenashi`) is polled and emitted
 * all the same — its board rows simply find no timetable to join.
 *
 * The catalogue is re-read at most once a day (the run's resume token carries
 * when it was last read — `d=<epoch>`, opaque to the scheduler) and the URLs
 * are kept in gtfs_rt_feeds, the registry table of RT URLs that schema.sql has
 * carried since the C port and nothing filled. One row per URL, keyed
 * "<org>*<feed>#<kind>" — the "<org>*<feed>" composite is gtfs-data.jp's own
 * permalink form, the one gtfs_jp.c already keys gtfs_feeds on.
 *
 * MEASURED 2026-11-12, every listed URL once (all 127 distinct, the
 * discontinued nine included), paced 1.5 s per host (19:12 JST, a weekday
 * evening): 127 of 127 answered HTTP 200, 668,809 bytes in all; 126
 * decoded; 58 of those were a bare FeedHeader with no entity — 15 bytes
 * each, an honest empty (a community bus that is not running at 19:00) —
 * and kumagaya.bus-go.com/GTFS-RT/encode_vehicle.php answered 200 with ZERO
 * bytes under `Content-Type: application/json`, which lib/gtfsrt.c refuses as
 * "no FeedHeader". That is the distinction this file keeps: a header with no
 * entities is a source with nothing to say; a body with no header is not a
 * feed, and the run discloses it rather than reading it as a quiet hour.
 * Largest bodies: sankobus TripUpdates 199,978 B (113 trips, 4,627 stop
 * updates), kumamoto-toshibus 71,086 B (51 / 1,608), kumamotodentetsu 56,616 B
 * (31 / 1,307), mie-kotsu sancoyokkaichi 29,154 B (32 / 957). Totals over the
 * 126: 466 trip updates carrying 14,616 stop-time updates, 467 vehicle
 * positions, 216 alerts. Every Japanese body decoded with no field skipped.
 *
 * The decoder was checked against the reference protobuf bindings
 * (gtfs-realtime-bindings for Python) on those 126 bodies plus MBTA's three
 * (cdn.mbta.com/realtime/{TripUpdates,VehiclePositions,Alerts}.pb: 697,765 B /
 * 925 trips / 23,205 stop updates; 29,533 B / 288 vehicles; 212,758 B / 83
 * alerts): entity, stop-update, vehicle and alert counts identical on all 129,
 * and a field-level dump — trip, sequence, stop, every arrival/departure
 * delay and time, coordinates, cause, effect, every informed entity and
 * active period, every translation — identical on all 39,718 lines. MBTA is
 * the reference only: its static GTFS is not loaded into gtfs_stop_times
 * (trn_mbta_gtfs_static.c emits stops as intel), its vehicles are already
 * read by trn_mbta_vehicle_positions.c and its alerts by two api-v3 rows, so a
 * second MBTA source would re-fetch what the tree holds and join nothing.
 *
 * DELAYS. The board reads gtfs_rt_trip_updates.{arrival,departure}_delay_s,
 * and the table has no column for an absolute time. Of the 14,616 stop-time
 * updates measured, 13,216 state `delay` (bus-vision.jp 4,490,
 * km.bus-vision.jp 8,519, the Keisei S3 bucket 197, nagai.nolbe.net 10) and
 * 1,400 state only `time` (gtfs-rt-files.buscatch.jp 1,093 — Toyama,
 * Chitetsu, Manyosen … — and realtime.gtfs.info 307). Writing NULL for those
 * would discard what the feed said,
 * so a time-only event's delay is DERIVED: time − (service-day start) −
 * scheduled seconds from gtfs_stop_times for the same (org, feed, trip,
 * stop_sequence). The rule was checked where the publisher states BOTH:
 * kumamoto-toshibus 1,608 of 1,608 and sankobus 4,627 of 4,627 derived delays
 * equal the publisher's own. A delay the feed states is stored as stated; a
 * derived one is marked `*_delay_derived` in the record's properties so the
 * two can be told apart in the data, since the table cannot say it.
 *   - The service day is the trip's start_date when the feed gives one. When
 *     it does not (every buscatch feed), it is the JST date of the update's
 *     timestamp, or the day before if that is the only reading within 12 h —
 *     an after-midnight trip of yesterday's service. Neither within 12 h:
 *     NOT derived, counted. Times are JST throughout because every feed in
 *     this catalogue is Japanese and Japan has no DST, so GTFS's "noon minus
 *     12 h" service-day origin is plain midnight.
 *   - No matching static row (gtfs_jp has not run, or the static file is a
 *     different edition): NOT derived, counted, the delay stays NULL. The raw
 *     time is in the emitted record either way.
 *
 * WHAT A RUN REPLACES. Every body measured is FULL_DATASET (129 of 129): a
 * trip absent from the snapshot has no realtime information. So each
 * operator's rows in the three RT tables are deleted and rewritten from this
 * run's bodies in one transaction — trip ids repeat every service day, and a
 * row left from yesterday would put yesterday's delay on today's departure. A
 * feed that failed this run therefore leaves its operator with no RT rows
 * rather than stale ones, and the failure is disclosed. A DIFFERENTIAL feed is
 * refused (none measured) because replacing would erase what it did not
 * restate.
 *
 * EVERY RECORD IS ALSO EMITTED (rule 2): the tables hold a subset — no
 * vehicle id, occupancy, uncertainty, schedule relationship, absolute time,
 * translation other than one — so each trip update, vehicle position and
 * alert is an intel record carrying every decoded field, keyed per
 * (operator feed, trip / vehicle / alert) so a re-run updates rather than
 * accumulates.
 *
 * Collectors normally hold no database code; this one writes its tables
 * directly through ctx->db for the reason gtfs_jp.c does — the board's tables
 * are relational and the sink is not — and with the same mechanism. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/gtfsrt.h"
#include "core/db.h"
#include "core/httpclient.h"
#include "third_party/sqlite3.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#define GRT_CATALOG_URL  "https://api.gtfs-data.jp/v2/feeds"
#define GRT_REDISCOVER   86400          /* catalogue re-read at most daily */
#define GRT_JST          (9 * 3600)
#define GRT_HALF_DAY     43200
#define GRT_TAG          "[gtfs-rt-jp]"

/* ── small helpers ────────────────────────────────────────────────────── */
static void bind_txt(sqlite3_stmt *s, int i, const char *v) {
  if (v) sqlite3_bind_text(s, i, v, -1, SQLITE_TRANSIENT);
  else   sqlite3_bind_null(s, i);
}

static char *trim_dup(const char *s) {
  if (!s) return NULL;
  while (*s && isspace((unsigned char)*s)) s++;
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) n--;
  if (!n) return NULL;
  char *o = malloc(n + 1);
  if (!o) return NULL;
  memcpy(o, s, n); o[n] = 0;
  return o;
}

/* days since 1970-01-01 for a proleptic Gregorian date (Hinnant). */
static long days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

/* "YYYYMMDD" -> epoch of that date's JST midnight; -1 when not a date. */
static long long jst_midnight_of(const char *ymd) {
  if (!ymd || strlen(ymd) != 8) return -1;
  for (int i = 0; i < 8; i++) if (!isdigit((unsigned char)ymd[i])) return -1;
  int y = (ymd[0]-'0')*1000 + (ymd[1]-'0')*100 + (ymd[2]-'0')*10 + (ymd[3]-'0');
  int m = (ymd[4]-'0')*10 + (ymd[5]-'0');
  int d = (ymd[6]-'0')*10 + (ymd[7]-'0');
  if (m < 1 || m > 12 || d < 1 || d > 31) return -1;
  return (long long)days_from_civil(y, m, d) * 86400 - GRT_JST;
}

static void iso_utc(long long t, char *o, size_t n) {
  time_t tt = (time_t)t;
  struct tm g;
  if (t <= 0 || !gmtime_r(&tt, &g)) { if (n) o[0] = 0; return; }
  strftime(o, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

/* A heap string of exactly the formatted length: keys and titles carry
 * publisher text (Japanese trip ids, alert headers) whose length is not ours
 * to bound, and a fixed buffer would cut a UTF-8 sequence in half. */
static char *fmt_dup(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static char *fmt_dup(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0) return NULL;
  char *o = malloc((size_t)n + 1);
  if (!o) return NULL;
  va_start(ap, fmt);
  vsnprintf(o, (size_t)n + 1, fmt, ap);
  va_end(ap);
  return o;
}

static void put_s(cJSON *o, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(o, k, v);
}
static void put_n(cJSON *o, const char *k, int has, double v) {
  if (has) cJSON_AddNumberToObject(o, k, v);
}
static void put_enum(cJSON *o, const char *k, int has, int v, const char *name) {
  if (!has) return;
  if (name) cJSON_AddStringToObject(o, k, name);
  else      cJSON_AddNumberToObject(o, k, v);   /* a value the .proto does not name */
}

static cJSON *trip_json(const gtfsrt_trip *t) {
  cJSON *o = cJSON_CreateObject();
  put_s(o, "trip_id", t->trip_id);
  put_s(o, "route_id", t->route_id);
  put_s(o, "start_date", t->start_date);
  put_s(o, "start_time", t->start_time);
  put_n(o, "direction_id", t->has_direction_id, t->direction_id);
  put_enum(o, "schedule_relationship", t->has_schedule_relationship,
           t->schedule_relationship,
           gtfsrt_trip_relationship_name(t->schedule_relationship));
  return o;
}
static cJSON *vdesc_json(const gtfsrt_vehicle_desc *v) {
  cJSON *o = cJSON_CreateObject();
  put_s(o, "id", v->id); put_s(o, "label", v->label);
  put_s(o, "license_plate", v->license_plate);
  return o;
}
static cJSON *tstring_json(const gtfsrt_tstring *s) {
  cJSON *a = cJSON_CreateArray();
  for (size_t i = 0; i < s->n; i++) {
    cJSON *t = cJSON_CreateObject();
    put_s(t, "text", s->tr[i].text);
    put_s(t, "language", s->tr[i].language);
    cJSON_AddItemToArray(a, t);
  }
  return a;
}
static cJSON *event_json(const gtfsrt_event *e) {
  cJSON *o = cJSON_CreateObject();
  put_n(o, "delay", e->has_delay, e->delay);
  put_n(o, "time", e->has_time, (double)e->time);
  put_n(o, "uncertainty", e->has_uncertainty, e->uncertainty);
  return o;
}

/* ── run state ────────────────────────────────────────────────────────── */
typedef struct {
  long feeds, feeds_ok, feeds_empty, feeds_failed, feeds_differential;
  long entities, deleted_entities, unknown_fields;
  long tu, tu_no_trip, stu, stu_rows, stu_no_seq, stu_no_trip;
  long delay_given, delay_derived, delay_no_static, delay_out_of_range;
  long vp, vp_rows, vp_no_trip, vp_no_position;
  long alerts, alert_rows;
  long emitted;
} grt_stats;

typedef struct {
  sqlite3_stmt *ins_tu, *ins_vp, *ins_al, *sched, *seq_by_stop, *feed_status;
} grt_stmts;

typedef struct {               /* one row of gtfs_rt_feeds */
  char *feed_key, *org, *org_name, *url;
  char feed[160], kind[40];
  /* this run */
  int ok; long status; size_t bytes; char err[256];
  gtfsrt_feed f;
} grt_src;

static void src_free(grt_src *s) {
  free(s->feed_key); free(s->org); free(s->org_name); free(s->url);
  if (s->ok) gtfsrt_free(&s->f);
}

/* "<org>*<feed>#<kind>" -> feed, kind. */
static void split_key(grt_src *s) {
  s->feed[0] = s->kind[0] = 0;
  const char *star = strchr(s->feed_key, '*');
  const char *hash = strrchr(s->feed_key, '#');
  if (!star || !hash || hash < star) return;
  snprintf(s->feed, sizeof s->feed, "%.*s", (int)(hash - star - 1), star + 1);
  snprintf(s->kind, sizeof s->kind, "%s", hash + 1);
}

/* ── discovery: /v2/feeds -> gtfs_rt_feeds ────────────────────────────── */
/* Returns the number of RT URLs written, or -1 when the catalogue could not
 * be read (the table is then left exactly as it was). */
static long discover(const source_ctx *ctx, sqlite3 *h, long *discontinued) {
  cJSON *wrap = feed_get_json(ctx->http, GRT_CATALOG_URL, 60000);
  cJSON *body = wrap ? cJSON_GetObjectItem(wrap, "body") : NULL;
  if (!cJSON_IsArray(body) || cJSON_GetArraySize(body) == 0) {
    cJSON_Delete(wrap);
    return -1;
  }
  static const char *const KINDS[3][2] = {
    { "trip_update_url",      "trip_updates" },
    { "vehicle_position_url", "vehicle_positions" },
    { "alert_url",            "alerts" },
  };
  sqlite3_stmt *up = NULL, *seen = NULL;
  sqlite3_exec(h, "CREATE TEMP TABLE IF NOT EXISTS _gtfsrt_seen(feed_id TEXT PRIMARY KEY)",
               NULL, NULL, NULL);
  sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
  sqlite3_exec(h, "DELETE FROM temp._gtfsrt_seen", NULL, NULL, NULL);
  /* ON CONFLICT keeps last_polled_at / last_ok_at / last_status /
   * consecutive_fails across a re-discovery; only what the catalogue says is
   * refreshed. poll_interval_s is the publisher's own `update_interval`, and
   * 0 when it declares none (48 of the 55 do) — the column's DEFAULT 30 would
   * be a number nobody published. */
  sqlite3_prepare_v2(h,
    "INSERT INTO gtfs_rt_feeds(feed_id,ag_id,ag_name,rt_url,poll_interval_s) "
    "VALUES(?1,?2,?3,?4,?5) ON CONFLICT(feed_id) DO UPDATE SET "
    "ag_id=excluded.ag_id,ag_name=excluded.ag_name,rt_url=excluded.rt_url,"
    "poll_interval_s=excluded.poll_interval_s", -1, &up, NULL);
  sqlite3_prepare_v2(h, "INSERT OR IGNORE INTO temp._gtfsrt_seen(feed_id) VALUES(?1)",
                     -1, &seen, NULL);
  long n = 0;
  cJSON *row;
  cJSON_ArrayForEach(row, body) {
    const char *org = jo_sv(row, "organization_id");
    const char *feed = jo_sv(row, "feed_id");
    cJSON *rt = cJSON_GetObjectItem(row, "real_time");
    if (!org || !feed || !cJSON_IsObject(rt)) continue;
    cJSON *disc = cJSON_GetObjectItem(row, "feed_is_discontinued");
    int is_disc = cJSON_IsTrue(disc);
    double iv = 0;
    jo_num(rt, "update_interval", &iv);
    for (int k = 0; k < 3; k++) {
      char *url = trim_dup(jo_sv(rt, KINDS[k][0]));
      if (!url) continue;
      if (is_disc) { (*discontinued)++; free(url); continue; }
      char key[512];
      snprintf(key, sizeof key, "%s*%s#%s", org, feed, KINDS[k][1]);
      if (up) {
        sqlite3_reset(up); sqlite3_clear_bindings(up);
        bind_txt(up, 1, key); bind_txt(up, 2, org);
        bind_txt(up, 3, jo_sv(row, "organization_name"));
        bind_txt(up, 4, url);
        sqlite3_bind_int(up, 5, iv > 0 ? (int)iv : 0);
        if (sqlite3_step(up) == SQLITE_DONE) n++;
      }
      if (seen) { sqlite3_reset(seen); bind_txt(seen, 1, key); sqlite3_step(seen); }
      free(url);
    }
  }
  /* A URL the catalogue no longer lists is not polled again. Only this
   * source's rows (the '#' suffix) are swept. */
  sqlite3_exec(h,
    "DELETE FROM gtfs_rt_feeds WHERE feed_id GLOB '*#*' AND feed_id NOT IN "
    "(SELECT feed_id FROM temp._gtfsrt_seen)", NULL, NULL, NULL);
  sqlite3_finalize(up); sqlite3_finalize(seen);
  if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
    n = -1;
  }
  cJSON_Delete(wrap);
  return n;
}

/* ── fetch + decode one URL ───────────────────────────────────────────── */
static void fetch_one(const source_ctx *ctx, grt_src *s) {
  http_response r = {0};
  s->ok = 0; s->status = 0; s->bytes = 0; s->err[0] = 0;
  int rc = http_request_max(ctx->http, "GET", s->url, NULL, NULL, 0, 30000, 1,
                            ctx->body_ceiling, &r);
  s->status = r.status;
  s->bytes = r.body_len;
  if (rc != 0) {
    snprintf(s->err, sizeof s->err, "transport failure (no HTTP answer)");
  } else if (r.status < 200 || r.status >= 300) {
    snprintf(s->err, sizeof s->err, "HTTP %ld", r.status);
  } else {
    char derr[120];
    if (gtfsrt_decode((const unsigned char *)r.body, r.body_len, &s->f,
                      derr, sizeof derr) != 0)
      snprintf(s->err, sizeof s->err, "HTTP %ld, %zu bytes, not a GTFS-RT "
               "FeedMessage: %s", r.status, (size_t)r.body_len, derr);
    else if (s->f.has_incrementality && s->f.incrementality != 0) {
      snprintf(s->err, sizeof s->err, "DIFFERENTIAL feed (incrementality=%d): "
               "this collector replaces per operator and would erase what a "
               "differential update does not restate", s->f.incrementality);
      gtfsrt_free(&s->f);
      s->status = -2;          /* marker for the stats, not an HTTP status */
    } else s->ok = 1;
  }
  http_response_free(&r);
}

/* ── scheduled seconds for (org, feed, trip, seq) from the static table ── */
static int sched_secs(grt_stmts *st, const char *org, const char *feed,
                      const char *trip, uint32_t seq, long long *arr, int *has_arr,
                      long long *dep, int *has_dep) {
  *has_arr = *has_dep = 0;
  if (!st->sched) return 0;
  sqlite3_reset(st->sched); sqlite3_clear_bindings(st->sched);
  bind_txt(st->sched, 1, org); bind_txt(st->sched, 2, feed);
  bind_txt(st->sched, 3, trip); sqlite3_bind_int64(st->sched, 4, seq);
  if (sqlite3_step(st->sched) != SQLITE_ROW) return 0;
  if (sqlite3_column_type(st->sched, 0) != SQLITE_NULL) {
    *arr = sqlite3_column_int64(st->sched, 0); *has_arr = 1;
  }
  if (sqlite3_column_type(st->sched, 1) != SQLITE_NULL) {
    *dep = sqlite3_column_int64(st->sched, 1); *has_dep = 1;
  }
  return 1;
}

/* A stop_time_update that names its stop but not its sequence: the sequence
 * is read from the static timetable when exactly one stop of the trip has
 * that stop_id (a loop route visits a stop twice, and then it is not
 * knowable). */
static int seq_for_stop(grt_stmts *st, const char *org, const char *feed,
                        const char *trip, const char *stop_id, uint32_t *seq) {
  if (!st->seq_by_stop || !stop_id) return 0;
  sqlite3_reset(st->seq_by_stop); sqlite3_clear_bindings(st->seq_by_stop);
  bind_txt(st->seq_by_stop, 1, org); bind_txt(st->seq_by_stop, 2, feed);
  bind_txt(st->seq_by_stop, 3, trip); bind_txt(st->seq_by_stop, 4, stop_id);
  int n = 0; long long v = 0;
  while (sqlite3_step(st->seq_by_stop) == SQLITE_ROW) {
    v = sqlite3_column_int64(st->seq_by_stop, 0);
    if (++n > 1) break;   /* exhaustive-ok: an ambiguity test on a local table (2 rows = ambiguous), not records */
  }
  if (n != 1 || v < 0) return 0;
  *seq = (uint32_t)v;
  return 1;
}

/* delay = time − service-day start − scheduled seconds; see the header for
 * the rule and its measurement. 1 = derived, 0 = out of range. */
static int derive_delay(long long t, long long sched, const char *start_date,
                        long long ref_ts, long long *out) {
  long long mid = jst_midnight_of(start_date);
  if (mid >= 0) {
    long long d = t - mid - sched;
    if (d >= -GRT_HALF_DAY && d <= GRT_HALF_DAY) { *out = d; return 1; }
    return 0;
  }
  long long day = (ref_ts + GRT_JST) / 86400;
  mid = day * 86400 - GRT_JST;
  long long d = t - mid - sched;
  if (d >= -GRT_HALF_DAY && d <= GRT_HALF_DAY) { *out = d; return 1; }
  d += 86400;                       /* yesterday's service, after midnight */
  if (d >= -GRT_HALF_DAY && d <= GRT_HALF_DAY) { *out = d; return 1; }
  return 0;
}

/* ── pending intel records (emitted after the operator's COMMIT) ──────── */
typedef struct {
  char *key, *title, *props, *record_type, *published, *lang, *summary;
  int has_geo; double lat, lon;
} grt_item;
typedef struct { grt_item *v; size_t n, cap; } grt_items;

/* Takes ownership of key, title and props. */
static void items_add(grt_items *L, char *key, char *title,
                      cJSON *props, const char *rtype, const char *published,
                      const char *lang, const char *summary,
                      int has_geo, double lat, double lon) {
  if (L->n == L->cap) {
    size_t nc = L->cap ? L->cap * 2 : 64;
    grt_item *nv = realloc(L->v, nc * sizeof *nv);
    if (!nv) { cJSON_Delete(props); free(key); free(title); return; }
    L->v = nv; L->cap = nc;
  }
  grt_item *it = &L->v[L->n++];
  memset(it, 0, sizeof *it);
  it->key = key;
  it->title = title;
  it->props = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);
  it->record_type = strdup(rtype);
  it->published = (published && published[0]) ? strdup(published) : NULL;
  it->lang = lang ? strdup(lang) : NULL;
  it->summary = summary ? strdup(summary) : NULL;
  it->has_geo = has_geo; it->lat = lat; it->lon = lon;
}

static void items_emit_free(grt_items *L, intel_sink *sink, const char *source_id,
                            grt_stats *S) {
  for (size_t i = 0; i < L->n; i++) {
    grt_item *it = &L->v[i];
    intel_item x = {0};
    x.remote_key = it->key;
    x.title = it->title ? it->title : it->key;
    x.summary = it->summary;
    x.lang = it->lang;
    x.published_at = it->published;
    x.has_geo = it->has_geo; x.lat = it->lat; x.lon = it->lon;
    x.record_type = it->record_type;
    x.properties_json = it->props ? it->props : "{}";
    x.tags_json = "[\"transport\",\"transit\",\"gtfs-rt\",\"japan\"]";
    if (it->key && sink->emit(sink, &x) >= 0) S->emitted++;
    free(it->key); free(it->title); free(it->props); free(it->record_type);
    free(it->published); free(it->lang); free(it->summary);
  }
  free(L->v);
  memset(L, 0, sizeof *L);
}

/* ── one decoded body -> table rows + pending records ─────────────────── */
static void take_body(grt_stmts *st, grt_src *s, const char *now_iso,
                      long long now, grt_items *L, grt_stats *S) {
  const gtfsrt_feed *f = &s->f;
  long long hdr_ts = f->has_timestamp ? (long long)f->timestamp : 0;
  S->unknown_fields += f->unknown_fields;
  const char *who = s->org_name ? s->org_name : s->org;
  /* "<org>*<feed>" as gtfs-data.jp writes it; built once per body */
  char *fk = fmt_dup("%s*%s", s->org, s->feed);
  if (!fk) return;

  for (size_t i = 0; i < f->nent; i++) {
    const gtfsrt_entity *e = &f->ent[i];
    S->entities++;
    if (e->is_deleted) { S->deleted_entities++; continue; }

    if (e->tu) {
      const gtfsrt_trip_update *tu = e->tu;
      S->tu++;
      const char *trip = tu->trip.trip_id;
      long long ts = tu->has_timestamp ? (long long)tu->timestamp : hdr_ts;
      long long ref = ts > 0 ? ts : now;
      long long reported = ts > 0 ? ts : now;
      cJSON *p = cJSON_CreateObject();
      put_s(p, "org_id", s->org); put_s(p, "feed_id", s->feed);
      put_s(p, "entity_id", e->id);
      if (tu->has_trip) cJSON_AddItemToObject(p, "trip", trip_json(&tu->trip));
      if (tu->has_vehicle) cJSON_AddItemToObject(p, "vehicle", vdesc_json(&tu->vehicle));
      put_n(p, "timestamp", tu->has_timestamp, (double)tu->timestamp);
      put_n(p, "delay", tu->has_delay, tu->delay);
      cJSON *arr = cJSON_AddArrayToObject(p, "stop_time_updates");
      if (!trip) S->tu_no_trip++;
      for (size_t k = 0; k < tu->nstu; k++) {
        const gtfsrt_stu *u = &tu->stu[k];
        S->stu++;
        cJSON *o = cJSON_CreateObject();
        put_n(o, "stop_sequence", u->has_stop_sequence, u->stop_sequence);
        put_s(o, "stop_id", u->stop_id);
        if (u->has_arrival) cJSON_AddItemToObject(o, "arrival", event_json(&u->arrival));
        if (u->has_departure) cJSON_AddItemToObject(o, "departure", event_json(&u->departure));
        put_enum(o, "schedule_relationship", u->has_schedule_relationship,
                 u->schedule_relationship,
                 gtfsrt_stu_relationship_name(u->schedule_relationship));
        cJSON_AddItemToArray(arr, o);
        if (!trip) { S->stu_no_trip++; continue; }

        uint32_t seq = u->stop_sequence;
        int have_seq = u->has_stop_sequence;
        if (!have_seq && seq_for_stop(st, s->org, s->feed, trip, u->stop_id, &seq)) {
          have_seq = 1;
          cJSON_AddNumberToObject(o, "stop_sequence_from_static", seq);
        }
        if (!have_seq) { S->stu_no_seq++; continue; }

        long long sa = 0, sd = 0; int ha = 0, hd = 0, looked = 0;
        int has_ad = 0, has_dd = 0; long long ad = 0, dd = 0;
        const gtfsrt_event *ev[2] = { u->has_arrival ? &u->arrival : NULL,
                                      u->has_departure ? &u->departure : NULL };
        for (int w = 0; w < 2; w++) {
          const gtfsrt_event *x = ev[w];
          if (!x) continue;
          long long *dst = w ? &dd : &ad;
          int *hdst = w ? &has_dd : &has_ad;
          if (x->has_delay) { *dst = x->delay; *hdst = 1; S->delay_given++; continue; }
          if (!x->has_time) continue;
          if (!looked) { sched_secs(st, s->org, s->feed, trip, seq, &sa, &ha, &sd, &hd); looked = 1; }
          int hs = w ? hd : ha;
          long long sc = w ? sd : sa;
          if (!hs) { S->delay_no_static++; continue; }
          if (derive_delay(x->time, sc, tu->trip.start_date, ref, dst)) {
            *hdst = 1; S->delay_derived++;
            cJSON_AddNumberToObject(o, w ? "departure_delay_derived" : "arrival_delay_derived",
                                    (double)*dst);
          } else S->delay_out_of_range++;
        }
        if (st->ins_tu) {
          sqlite3_reset(st->ins_tu); sqlite3_clear_bindings(st->ins_tu);
          bind_txt(st->ins_tu, 1, s->org);
          bind_txt(st->ins_tu, 2, trip);
          bind_txt(st->ins_tu, 3, tu->trip.route_id);
          bind_txt(st->ins_tu, 4, u->stop_id);
          sqlite3_bind_int64(st->ins_tu, 5, seq);
          if (has_ad) sqlite3_bind_int64(st->ins_tu, 6, ad); else sqlite3_bind_null(st->ins_tu, 6);
          if (has_dd) sqlite3_bind_int64(st->ins_tu, 7, dd); else sqlite3_bind_null(st->ins_tu, 7);
          sqlite3_bind_int64(st->ins_tu, 8, reported);
          bind_txt(st->ins_tu, 9, now_iso);
          if (sqlite3_step(st->ins_tu) == SQLITE_DONE) S->stu_rows++;
        }
      }
      char pub[32];
      const char *tk = trip ? trip : (e->id ? e->id : "?");
      iso_utc(ts, pub, sizeof pub);
      items_add(L, fmt_dup("%s|trip|%s", fk, tk),
                fmt_dup("%s trip %s%s%s", who, tk,
                        tu->trip.route_id ? " route " : "",
                        tu->trip.route_id ? tu->trip.route_id : ""),
                p, "transit-trip-update", pub, NULL, NULL, 0, 0, 0);
    }

    if (e->vp) {
      const gtfsrt_vehicle *v = e->vp;
      S->vp++;
      long long ts = v->has_timestamp ? (long long)v->timestamp : hdr_ts;
      cJSON *p = cJSON_CreateObject();
      put_s(p, "org_id", s->org); put_s(p, "feed_id", s->feed);
      put_s(p, "entity_id", e->id);
      if (v->has_trip) cJSON_AddItemToObject(p, "trip", trip_json(&v->trip));
      if (v->has_vehicle) cJSON_AddItemToObject(p, "vehicle", vdesc_json(&v->vehicle));
      if (v->has_position) {
        cJSON_AddNumberToObject(p, "latitude", v->lat);
        cJSON_AddNumberToObject(p, "longitude", v->lon);
      }
      put_n(p, "bearing", v->has_bearing, v->bearing);
      put_n(p, "odometer", v->has_odometer, v->odometer);
      put_n(p, "speed_mps", v->has_speed, v->speed);
      put_n(p, "current_stop_sequence", v->has_current_stop_sequence, v->current_stop_sequence);
      put_s(p, "stop_id", v->stop_id);
      put_enum(p, "current_status", v->has_current_status, v->current_status,
               gtfsrt_vehicle_status_name(v->current_status));
      put_enum(p, "congestion_level", v->has_congestion_level, v->congestion_level,
               gtfsrt_congestion_name(v->congestion_level));
      put_enum(p, "occupancy_status", v->has_occupancy_status, v->occupancy_status,
               gtfsrt_occupancy_name(v->occupancy_status));
      put_n(p, "occupancy_percentage", v->has_occupancy_percentage, v->occupancy_percentage);
      put_n(p, "timestamp", v->has_timestamp, (double)v->timestamp);

      /* gtfs_rt_positions is keyed (org_id, trip_id): a vehicle with no trip
       * (deadheading, or a feed that omits it) has no row there, only its
       * record. A position with no lat/lon is not a position (both are
       * `required` in Position) and is not written as 0,0. */
      if (!v->has_position) S->vp_no_position++;
      else if (!v->trip.trip_id) S->vp_no_trip++;
      else if (st->ins_vp) {
        sqlite3_reset(st->ins_vp); sqlite3_clear_bindings(st->ins_vp);
        bind_txt(st->ins_vp, 1, s->org);
        bind_txt(st->ins_vp, 2, v->trip.trip_id);
        bind_txt(st->ins_vp, 3, v->trip.route_id);
        sqlite3_bind_double(st->ins_vp, 4, v->lat);
        sqlite3_bind_double(st->ins_vp, 5, v->lon);
        if (v->has_bearing) sqlite3_bind_double(st->ins_vp, 6, v->bearing);
        else sqlite3_bind_null(st->ins_vp, 6);
        if (v->has_speed) sqlite3_bind_double(st->ins_vp, 7, v->speed);
        else sqlite3_bind_null(st->ins_vp, 7);
        sqlite3_bind_int64(st->ins_vp, 8, ts > 0 ? ts : now);
        bind_txt(st->ins_vp, 9, now_iso);
        if (sqlite3_step(st->ins_vp) == SQLITE_DONE) S->vp_rows++;
      }
      const char *vid = v->vehicle.id ? v->vehicle.id
                      : e->id ? e->id : v->trip.trip_id ? v->trip.trip_id : "?";
      char pub[32];
      iso_utc(ts, pub, sizeof pub);
      items_add(L, fmt_dup("%s|vehicle|%s", fk, vid),
                fmt_dup("%s vehicle %s", who, v->vehicle.label ? v->vehicle.label : vid),
                p, "transit-vehicle-position", pub, NULL, NULL,
                v->has_position, v->lat, v->lon);
    }

    if (e->al) {
      const gtfsrt_alert *a = e->al;
      S->alerts++;
      const char *hdr = gtfsrt_tstring_pick(&a->header, "ja");
      const char *desc = gtfsrt_tstring_pick(&a->description, "ja");
      const char *hdr_lang = NULL;
      for (size_t k = 0; k < a->header.n; k++)
        if (a->header.tr[k].text == hdr) { hdr_lang = a->header.tr[k].language; break; }
      cJSON *routes = cJSON_CreateArray(), *trips = cJSON_CreateArray(),
            *stops = cJSON_CreateArray(), *ie = cJSON_CreateArray();
      for (size_t k = 0; k < a->nie; k++) {
        const gtfsrt_selector *x = &a->ie[k];
        cJSON *o = cJSON_CreateObject();
        put_s(o, "agency_id", x->agency_id); put_s(o, "route_id", x->route_id);
        put_s(o, "stop_id", x->stop_id);
        put_n(o, "route_type", x->has_route_type, x->route_type);
        put_n(o, "direction_id", x->has_direction_id, x->direction_id);
        if (x->has_trip) cJSON_AddItemToObject(o, "trip", trip_json(&x->trip));
        cJSON_AddItemToArray(ie, o);
        /* distinct ids per list: the board matches route_ids by LIKE */
        struct { cJSON *arr; const char *v; } add[3] = {
          { routes, x->route_id }, { trips, x->trip.trip_id }, { stops, x->stop_id } };
        for (int j = 0; j < 3; j++) {
          if (!add[j].v) continue;
          int dup = 0; cJSON *c;
          cJSON_ArrayForEach(c, add[j].arr)
            if (cJSON_IsString(c) && !strcmp(c->valuestring, add[j].v)) { dup = 1; break; }
          if (!dup) cJSON_AddItemToArray(add[j].arr, cJSON_CreateString(add[j].v));
        }
      }
      const char *cause = a->has_cause ? gtfsrt_cause_name(a->cause) : NULL;
      const char *effect = a->has_effect ? gtfsrt_effect_name(a->effect) : NULL;
      char cbuf[16], ebuf[16];
      if (a->has_cause && !cause) { snprintf(cbuf, sizeof cbuf, "%d", a->cause); cause = cbuf; }
      if (a->has_effect && !effect) { snprintf(ebuf, sizeof ebuf, "%d", a->effect); effect = ebuf; }
      /* alert_id is feed-qualified: entity ids like "ALERT00001" repeat
       * across one operator's feeds, and the PK is (org_id, alert_id). */
      char *aid = fmt_dup("%s:%s", s->feed, e->id ? e->id : "?");
      if (st->ins_al) {
        char *rj = cJSON_PrintUnformatted(routes), *tj = cJSON_PrintUnformatted(trips),
             *sj = cJSON_PrintUnformatted(stops);
        sqlite3_reset(st->ins_al); sqlite3_clear_bindings(st->ins_al);
        bind_txt(st->ins_al, 1, s->org);
        bind_txt(st->ins_al, 2, aid);
        bind_txt(st->ins_al, 3, rj ? rj : "[]");
        bind_txt(st->ins_al, 4, tj ? tj : "[]");
        bind_txt(st->ins_al, 5, sj ? sj : "[]");
        bind_txt(st->ins_al, 6, hdr);
        bind_txt(st->ins_al, 7, desc);
        bind_txt(st->ins_al, 8, cause);
        bind_txt(st->ins_al, 9, effect);
        /* An Alert carries no timestamp of its own; reported_at is the
         * feed header's (present on all 126 bodies measured), else the
         * time we received it — the column is NOT NULL. */
        sqlite3_bind_int64(st->ins_al, 10, hdr_ts > 0 ? hdr_ts : now);
        bind_txt(st->ins_al, 11, now_iso);
        if (sqlite3_step(st->ins_al) == SQLITE_DONE) S->alert_rows++;
        free(rj); free(tj); free(sj);
      }
      free(aid);
      cJSON *p = cJSON_CreateObject();
      put_s(p, "org_id", s->org); put_s(p, "feed_id", s->feed);
      put_s(p, "entity_id", e->id);
      put_s(p, "cause", cause); put_s(p, "effect", effect);
      put_enum(p, "severity_level", a->has_severity_level, a->severity_level,
               gtfsrt_severity_name(a->severity_level));
      cJSON *per = cJSON_AddArrayToObject(p, "active_period");
      for (size_t k = 0; k < a->nperiods; k++) {
        cJSON *o = cJSON_CreateObject();
        put_n(o, "start", a->periods[k].has_start, (double)a->periods[k].start);
        put_n(o, "end", a->periods[k].has_end, (double)a->periods[k].end);
        cJSON_AddItemToArray(per, o);
      }
      cJSON_AddItemToObject(p, "informed_entity", ie);
      cJSON_AddItemToObject(p, "route_ids", routes);
      cJSON_AddItemToObject(p, "trip_ids", trips);
      cJSON_AddItemToObject(p, "stop_ids", stops);
      cJSON_AddItemToObject(p, "header_text", tstring_json(&a->header));
      cJSON_AddItemToObject(p, "description_text", tstring_json(&a->description));
      if (a->url.n) cJSON_AddItemToObject(p, "url", tstring_json(&a->url));
      if (a->tts_header.n) cJSON_AddItemToObject(p, "tts_header_text", tstring_json(&a->tts_header));
      if (a->tts_description.n)
        cJSON_AddItemToObject(p, "tts_description_text", tstring_json(&a->tts_description));
      if (a->image_alternative_text.n)
        cJSON_AddItemToObject(p, "image_alternative_text", tstring_json(&a->image_alternative_text));
      if (a->cause_detail.n) cJSON_AddItemToObject(p, "cause_detail", tstring_json(&a->cause_detail));
      if (a->effect_detail.n) cJSON_AddItemToObject(p, "effect_detail", tstring_json(&a->effect_detail));
      char pub[32];
      iso_utc(hdr_ts, pub, sizeof pub);
      items_add(L, fmt_dup("%s|alert|%s", fk, e->id ? e->id : "?"),
                fmt_dup("%s: %s", who, hdr ? hdr : (effect ? effect : "alert")),
                p, "transit-alert", pub, hdr_lang, desc, 0, 0, 0);
    }
  }
  free(fk);
}

static void feed_status(grt_stmts *st, const grt_src *s, const char *now_iso) {
  if (!st->feed_status) return;
  char status[300];
  if (s->ok) snprintf(status, sizeof status, "ok: HTTP %ld, %zu bytes, %zu entities",
                      s->status, s->bytes, s->f.nent);
  else snprintf(status, sizeof status, "%s", s->err);
  sqlite3_reset(st->feed_status); sqlite3_clear_bindings(st->feed_status);
  bind_txt(st->feed_status, 1, now_iso);
  sqlite3_bind_int(st->feed_status, 2, s->ok);
  bind_txt(st->feed_status, 3, status);
  bind_txt(st->feed_status, 4, s->feed_key);
  sqlite3_step(st->feed_status);
}

static void prep(sqlite3 *h, grt_stmts *st) {
  memset(st, 0, sizeof *st);
  sqlite3_prepare_v2(h,
    "INSERT OR REPLACE INTO gtfs_rt_trip_updates(org_id,trip_id,route_id,stop_id,"
    "stop_sequence,arrival_delay_s,departure_delay_s,reported_at,received_at) "
    "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)", -1, &st->ins_tu, NULL);
  sqlite3_prepare_v2(h,
    "INSERT OR REPLACE INTO gtfs_rt_positions(org_id,trip_id,route_id,lat,lon,"
    "bearing,speed_mps,reported_at,received_at) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)",
    -1, &st->ins_vp, NULL);
  sqlite3_prepare_v2(h,
    "INSERT OR REPLACE INTO gtfs_rt_alerts(org_id,alert_id,route_ids,trip_ids,"
    "stop_ids,header_text,description_text,cause,effect,reported_at,received_at) "
    "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)", -1, &st->ins_al, NULL);
  sqlite3_prepare_v2(h,
    "SELECT arrival_sec,departure_sec FROM gtfs_stop_times "
    "WHERE org_id=?1 AND feed_id=?2 AND trip_id=?3 AND stop_sequence=?4",
    -1, &st->sched, NULL);
  sqlite3_prepare_v2(h,
    "SELECT stop_sequence FROM gtfs_stop_times "
    "WHERE org_id=?1 AND feed_id=?2 AND trip_id=?3 AND stop_id=?4 LIMIT 2",
    -1, &st->seq_by_stop, NULL);
  sqlite3_prepare_v2(h,
    "UPDATE gtfs_rt_feeds SET last_polled_at=?1,"
    "last_ok_at=CASE WHEN ?2 THEN ?1 ELSE last_ok_at END,last_status=?3,"
    "consecutive_fails=CASE WHEN ?2 THEN 0 ELSE consecutive_fails+1 END "
    "WHERE feed_id=?4", -1, &st->feed_status, NULL);
}
static void unprep(grt_stmts *st) {
  sqlite3_finalize(st->ins_tu); sqlite3_finalize(st->ins_vp);
  sqlite3_finalize(st->ins_al); sqlite3_finalize(st->sched);
  sqlite3_finalize(st->seq_by_stop); sqlite3_finalize(st->feed_status);
}

/* Replace one operator's realtime rows with this run's snapshot. */
static int commit_org(sqlite3 *h, grt_stmts *st, grt_src *v, size_t a, size_t b,
                      const char *now_iso, long long now, grt_items *L, grt_stats *S) {
  static const char *const DEL[] = {
    "DELETE FROM gtfs_rt_trip_updates WHERE org_id=?1",
    "DELETE FROM gtfs_rt_positions WHERE org_id=?1",
    "DELETE FROM gtfs_rt_alerts WHERE org_id=?1",
  };
  sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
  for (size_t i = 0; i < sizeof DEL / sizeof *DEL; i++) {
    sqlite3_stmt *d = NULL;
    if (sqlite3_prepare_v2(h, DEL[i], -1, &d, NULL) == SQLITE_OK) {
      bind_txt(d, 1, v[a].org);
      sqlite3_step(d);
    }
    sqlite3_finalize(d);
  }
  for (size_t i = a; i < b; i++) {
    if (v[i].ok) take_body(st, &v[i], now_iso, now, L, S);
    feed_status(st, &v[i], now_iso);
  }
  if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
    /* the records are still real even though the board rows did not land */
    fprintf(stderr, GRT_TAG " %s: commit failed: %s\n", v[a].org, sqlite3_errmsg(h));
    return -1;
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->db || !ctx->db->h) {
    fprintf(stderr, GRT_TAG " no db handle\n");
    return -1;
  }
  sqlite3 *h = ctx->db->h;
  long long now = (long long)time(NULL);
  char now_iso[40];
  jo_iso_now_sec(now_iso, sizeof now_iso);

  /* 1. the catalogue, at most daily */
  long long last = 0;
  if (ctx->resume_in && !strncmp(ctx->resume_in, "d=", 2))
    last = strtoll(ctx->resume_in + 2, NULL, 10);
  long have = 0;
  {
    sqlite3_stmt *c = NULL;
    if (sqlite3_prepare_v2(h, "SELECT count(*) FROM gtfs_rt_feeds WHERE feed_id GLOB '*#*'",
                           -1, &c, NULL) == SQLITE_OK && sqlite3_step(c) == SQLITE_ROW)
      have = sqlite3_column_int64(c, 0);
    sqlite3_finalize(c);
  }
  long discovered = -2, discontinued = 0;   /* -2: not attempted this run */
  if (have == 0 || last <= 0 || now - last >= GRT_REDISCOVER || last > now) {
    discovered = discover(ctx, h, &discontinued);
    if (discovered >= 0 && ctx->resume_out) {
      char tok[40];
      snprintf(tok, sizeof tok, "d=%lld", now);
      ctx->resume_out(ctx, tok);
    }
    fprintf(stderr, GRT_TAG " catalogue: %ld RT URLs%s, %ld on discontinued feeds skipped\n",
            discovered, discovered < 0 ? " (READ FAILED; previous list kept)" : "",
            discontinued);
  }

  /* 2. the URL list */
  grt_src *v = NULL; size_t n = 0, cap = 0;
  {
    sqlite3_stmt *q = NULL;
    if (sqlite3_prepare_v2(h,
          "SELECT feed_id,ag_id,ag_name,rt_url FROM gtfs_rt_feeds "
          "WHERE feed_id GLOB '*#*' ORDER BY ag_id,feed_id", -1, &q, NULL) == SQLITE_OK) {
      while (sqlite3_step(q) == SQLITE_ROW) {
        const char *fid = (const char *)sqlite3_column_text(q, 0);
        const char *org = (const char *)sqlite3_column_text(q, 1);
        const char *url = (const char *)sqlite3_column_text(q, 3);
        if (!fid || !org || !url) continue;
        if (n == cap) {
          size_t nc = cap ? cap * 2 : 64;
          grt_src *nv = realloc(v, nc * sizeof *nv);
          if (!nv) break;
          v = nv; cap = nc;
        }
        grt_src *s = &v[n];
        memset(s, 0, sizeof *s);
        s->feed_key = strdup(fid);
        s->org = strdup(org);
        const char *on = (const char *)sqlite3_column_text(q, 2);
        s->org_name = on ? strdup(on) : NULL;
        s->url = trim_dup(url);
        if (!s->feed_key || !s->org || !s->url) { src_free(s); continue; }
        split_key(s);
        n++;
      }
    }
    sqlite3_finalize(q);
  }
  if (n == 0) {
    free(v);
    /* No list and no catalogue: the run fetched nothing usable. rc=-1 lets
     * core/scheduler.c file the fetch-failure disclosure. */
    fprintf(stderr, GRT_TAG " no GTFS-RT URLs known and the catalogue %s\n",
            discovered == -1 ? "could not be read" : "lists none");
    return discovered == -1 ? -1 : 0;
  }

  /* 3. per operator: fetch every URL, then replace its rows in one commit */
  grt_stats S; memset(&S, 0, sizeof S);
  grt_stmts st; prep(h, &st);
  cJSON *failed = cJSON_CreateArray();
  int cancelled = 0;
  size_t reached = 0;
  for (size_t a = 0; a < n && !cancelled; ) {
    size_t b = a + 1;
    while (b < n && !strcmp(v[b].org, v[a].org)) b++;
    for (size_t i = a; i < b; i++) {
      if (ctx->cancel && *ctx->cancel) { cancelled = 1; break; }
      fetch_one(ctx, &v[i]);
      S.feeds++;
      if (v[i].ok) {
        S.feeds_ok++;
        if (v[i].f.nent == 0) S.feeds_empty++;
      } else {
        if (v[i].status == -2) S.feeds_differential++;
        S.feeds_failed++;
        cJSON *o = cJSON_CreateObject();
        put_s(o, "feed_id", v[i].feed_key);
        put_s(o, "url", v[i].url);
        if (v[i].status > 0) cJSON_AddNumberToObject(o, "http_status", (double)v[i].status);
        cJSON_AddNumberToObject(o, "bytes", (double)v[i].bytes);
        put_s(o, "reason", v[i].err);
        cJSON_AddItemToArray(failed, o);
      }
    }
    if (cancelled) break;          /* this operator's rows stay as they were */
    grt_items L = {0};
    commit_org(h, &st, v, a, b, now_iso, now, &L, &S);
    items_emit_free(&L, sink, ctx->source_id, &S);
    for (size_t i = a; i < b; i++) src_free(&v[i]);
    reached = b;
    a = b;
  }
  for (size_t i = reached; i < n; i++) src_free(&v[i]);
  free(v);
  unprep(&st);

  fprintf(stderr,
    GRT_TAG " %ld URLs: %ld decoded (%ld with no entity), %ld failed%s | "
    "%ld trip updates, %ld stop updates -> %ld rows (%ld no stop_sequence, "
    "%ld trip updates without trip_id); delays %ld stated, %ld derived, "
    "%ld time-only with no static row, %ld out of range | %ld vehicles -> %ld rows "
    "(%ld no trip_id, %ld no position) | %ld alerts -> %ld rows | %ld fields "
    "skipped | %ld records emitted\n",
    S.feeds, S.feeds_ok, S.feeds_empty, S.feeds_failed, cancelled ? " (CANCELLED)" : "",
    S.tu, S.stu, S.stu_rows, S.stu_no_seq, S.tu_no_trip, S.delay_given,
    S.delay_derived, S.delay_no_static, S.delay_out_of_range, S.vp, S.vp_rows,
    S.vp_no_trip, S.vp_no_position, S.alerts, S.alert_rows, S.unknown_fields,
    S.emitted);

  /* Rule 2 / rule 1: what this run did not take is stated in the data. Every
   * decoded record was emitted above, so what can be missing is (a) a URL
   * that did not answer with a feed, (b) a stop update the board's table
   * cannot key (no stop_sequence, none in the static timetable), (c) fields
   * the decoder skipped by wire type, (d) the operators a cancel left
   * unread, and (e) a failed catalogue re-read. */
  long unkeyed = S.stu_no_seq + S.stu_no_trip;
  if (S.feeds_failed || unkeyed || S.unknown_fields || cancelled || discovered == -1) {
    char why[600];
    snprintf(why, sizeof why,
             "%ld of %ld GTFS-RT URLs decoded; %ld yielded no decodable feed "
             "(refused, unreachable, or not a GTFS-RT FeedMessage — each is in "
             "`failed` with its reason)%s; %ld stop-time updates could not be written to "
             "gtfs_rt_trip_updates (no trip_id, or no stop_sequence and none "
             "resolvable from the static timetable — each is still in its "
             "trip-update record); %ld fields skipped by wire type%s%s",
             S.feeds_ok, (long)n, S.feeds_failed,
             S.feeds_differential ? " (some are DIFFERENTIAL, not applied)" : "",
             unkeyed, S.unknown_fields,
             cancelled ? "; run cancelled before every operator was read" : "",
             discovered == -1 ? "; the gtfs-data.jp catalogue re-read failed, "
                                "the previous URL list was used" : "");
    cJSON *extra = cJSON_CreateObject();
    if (extra) {
      cJSON_AddNumberToObject(extra, "urls_known", (double)n);
      cJSON_AddNumberToObject(extra, "urls_polled", (double)S.feeds);
      cJSON_AddNumberToObject(extra, "urls_decoded", (double)S.feeds_ok);
      cJSON_AddNumberToObject(extra, "urls_failed", (double)S.feeds_failed);
      cJSON_AddItemToObject(extra, "failed", failed);
      failed = NULL;
      cJSON_AddNumberToObject(extra, "stop_updates_unkeyed", (double)unkeyed);
      cJSON_AddNumberToObject(extra, "fields_skipped", (double)S.unknown_fields);
      cJSON_AddNumberToObject(extra, "delays_derived", (double)S.delay_derived);
      cJSON_AddNumberToObject(extra, "delays_time_only_no_static", (double)S.delay_no_static);
      cJSON_AddBoolToObject(extra, "cancelled", cancelled);
      cJSON_AddBoolToObject(extra, "catalogue_read_failed", discovered == -1);
    }
    jo_truncation_notice_ex(sink, ctx->source_id, NULL, S.feeds_ok, (long)n, why,
      "a URL answering a non-feed body is the publisher's (its row in "
      "gtfs_rt_feeds carries last_status); a refused or reset fetch is re-read "
      "next run; an unkeyed stop update needs gtfs_jp's static timetable for "
      "that feed", extra);
  }
  cJSON_Delete(failed);

  /* rc=-1 only when nothing decoded at all, so the scheduler's own
   * fetch-failure disclosure fires; a run where some URLs failed has said
   * so above and is otherwise a success. Feeds that decoded with no entity
   * are an honest empty, not a failure. */
  return (S.feeds_ok > 0 || cancelled) ? 0 : -1;
}

static const source_def gtfs_rt_jp_def = {
  .id = "gtfs-rt-jp", .collector = "transport",
  .name = "GTFS Realtime — Japan (gtfs-data.jp operators)",
  .name_ja = "GTFSリアルタイム（GTFSデータリポジトリ掲載事業者）",
  /* Declared at 60 s because that is the scale a delay is true on; the
   * publishers that declare a cadence say 15–20 s, the rest none. The
   * system-wide cadence policy holds every row at 6 h unless the operator
   * sets this source's own interval, and a 6-hour-old snapshot is not a
   * departure board — see the source page's Settings › Collection interval.
   * One run is 120 GETs on 9 hosts (the 127 distinct listed URLs measured
   * 2026-11-12 came to 668,809 bytes), plus the 568,991-byte catalogue at
   * most once a day. Run end to end against those saved bodies: 120 URLs,
   * 119 decoded, 14,616 stop updates -> 14,616 board rows, 467 vehicles,
   * 215 alerts, 1,148 records plus one notice naming the zero-byte URL. */
  .update_interval_sec = 60,
  .category = "transport", .type = "api",
  .url = "https://gtfs-data.jp/",
  .description = "Trip updates, vehicle positions and service alerts from every "
                 "keyless GTFS-RT URL the GTFS data repository (gtfs-data.jp) "
                 "lists, written to the departures board's realtime tables and "
                 "emitted as records.",
  /* the 55 RT feeds' own licences, 2026-11-12: CC0 1.0 31, CC BY 4.0 24 */
  .license = "per operator, as listed on gtfs-data.jp (CC0 1.0 or CC BY 4.0)",
  .free_tier = 1,
  .run = run };
REGISTER_SOURCE(gtfs_rt_jp_def)
