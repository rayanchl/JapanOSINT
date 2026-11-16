/* lib/gtfsrt.h — a minimal, bounds-checked GTFS Realtime (v2.0) decoder.
 *
 * GTFS Realtime is a protocol-buffers message (gtfs-realtime.proto). This tree
 * has no protobuf library and wants none for five messages, so this is the
 * wire format read by hand: varint, length-delimited, fixed32/fixed64, and
 * the deprecated group wire types SKIPPED (never interpreted). Every read is
 * checked against the end of its enclosing message, so a truncated or hostile
 * body is an error, never a read past the buffer.
 *
 * What is decoded (field numbers are the .proto's):
 *   FeedMessage{header=1, entity=2}
 *   FeedHeader{gtfs_realtime_version=1, incrementality=2, timestamp=3}
 *   FeedEntity{id=1, is_deleted=2, trip_update=3, vehicle=4, alert=5}
 *   TripUpdate{trip=1, stop_time_update=2, vehicle=3, timestamp=4, delay=5}
 *   StopTimeUpdate{stop_sequence=1, arrival=2, departure=3, stop_id=4,
 *                  schedule_relationship=5}
 *   StopTimeEvent{delay=1, time=2, uncertainty=3}
 *   VehiclePosition{trip=1, position=2, current_stop_sequence=3,
 *                   current_status=4, timestamp=5, congestion_level=6,
 *                   stop_id=7, vehicle=8, occupancy_status=9,
 *                   occupancy_percentage=10}
 *   Position{latitude=1, longitude=2, bearing=3, odometer=4, speed=5}
 *   Alert{active_period=1, informed_entity=5, cause=6, effect=7, url=8,
 *         header_text=10, description_text=11, tts_header_text=12,
 *         tts_description_text=13, severity_level=14,
 *         image_alternative_text=16, cause_detail=17, effect_detail=18}
 *   EntitySelector{agency_id=1, route_id=2, route_type=3, trip=4, stop_id=5,
 *                  direction_id=6}
 *   TripDescriptor{trip_id=1, start_time=2, start_date=3,
 *                  schedule_relationship=4, route_id=5, direction_id=6}
 *   VehicleDescriptor{id=1, label=2, license_plate=3}
 *   TranslatedString{translation=1}  Translation{text=1, language=2}
 *   TimeRange{start=1, end=2}
 * Every other field — extensions (1000+), newer fields, an Alert's image, a
 * FeedEntity's shape or stop — is skipped by wire type and COUNTED into
 * `unknown_fields`, so a caller can say how much of a body it did not read
 * instead of implying it read everything. Measured 2026-11-12 over the 127
 * keyless Japanese GTFS-RT bodies gtfs-data.jp lists plus MBTA's three: the
 * fields the first cut skipped were VehiclePosition.occupancy_status (17, 3
 * and 10 vehicles on three Japanese feeds) and MBTA's occupancy_percentage,
 * severity_level, cause/effect_detail and image — so those are decoded now
 * and every Japanese body reads with unknown_fields == 0; MBTA's Alerts.pb
 * still skips its 2 `image` messages.
 *
 * What is an error (gtfsrt_decode returns -1 and names it in `err`):
 *   - truncation anywhere (a varint, a length, a fixed field running off the
 *     end of its message);
 *   - a varint longer than 10 bytes, wire types 6/7, field number 0, an
 *     unmatched end-group, groups nested past GTFSRT_MAX_DEPTH;
 *   - a KNOWN field arriving with the wrong wire type (it is not this
 *     message, or it is corrupt — guessing would be worse);
 *   - a FeedMessage with no FeedHeader, or a header with no
 *     gtfs_realtime_version. Both are `required` in the .proto, and this is
 *     the discriminator that keeps a ZERO-BYTE body (which decodes as an
 *     empty protobuf message) from reading as a feed with no entities: an
 *     empty body is not a feed, a feed with zero entities is an honest empty.
 *
 * "has_" flags mark presence: proto2 fields are optional and an absent delay
 * is not a delay of zero. Nothing is defaulted that the body did not carry.
 *
 * Strings are copied and NUL-terminated (protobuf strings are length-prefixed
 * and may in principle contain a NUL; the copy is exact, a reader of the C
 * string sees up to the first NUL). gtfsrt_free releases everything. */
#ifndef JO_GTFSRT_H
#define JO_GTFSRT_H

#include <stddef.h>
#include <stdint.h>

#define GTFSRT_MAX_DEPTH 32

typedef struct {
  char *trip_id, *route_id, *start_date, *start_time;
  int has_schedule_relationship; int schedule_relationship;
  int has_direction_id; uint32_t direction_id;
} gtfsrt_trip;

typedef struct {
  char *id, *label, *license_plate;
} gtfsrt_vehicle_desc;

typedef struct {
  int has_delay;       int32_t  delay;        /* seconds; may be negative */
  int has_time;        int64_t  time;         /* POSIX seconds */
  int has_uncertainty; int32_t  uncertainty;
} gtfsrt_event;

typedef struct {
  int has_stop_sequence; uint32_t stop_sequence;
  char *stop_id;
  int has_arrival;   gtfsrt_event arrival;
  int has_departure; gtfsrt_event departure;
  int has_schedule_relationship; int schedule_relationship;
} gtfsrt_stu;

typedef struct {
  int has_trip; gtfsrt_trip trip;
  int has_vehicle; gtfsrt_vehicle_desc vehicle;
  gtfsrt_stu *stu; size_t nstu, capstu;
  int has_timestamp; uint64_t timestamp;
  int has_delay; int32_t delay;               /* trip-level delay */
} gtfsrt_trip_update;

typedef struct {
  int has_trip; gtfsrt_trip trip;
  int has_vehicle; gtfsrt_vehicle_desc vehicle;
  int has_position;
  double lat, lon;                            /* float on the wire */
  int has_bearing;  float bearing;
  int has_odometer; double odometer;
  int has_speed;    float speed;              /* metres per second */
  int has_current_stop_sequence; uint32_t current_stop_sequence;
  int has_current_status; int current_status;
  int has_timestamp; uint64_t timestamp;
  int has_congestion_level; int congestion_level;
  int has_occupancy_status; int occupancy_status;
  int has_occupancy_percentage; uint32_t occupancy_percentage;
  char *stop_id;
} gtfsrt_vehicle;

typedef struct { char *text, *language; } gtfsrt_translation;
typedef struct { gtfsrt_translation *tr; size_t n, cap; } gtfsrt_tstring;

typedef struct {
  char *agency_id, *route_id, *stop_id;
  int has_route_type; int32_t route_type;
  int has_trip; gtfsrt_trip trip;
  int has_direction_id; uint32_t direction_id;
} gtfsrt_selector;

typedef struct {
  int has_start; uint64_t start;
  int has_end;   uint64_t end;
} gtfsrt_period;

typedef struct {
  gtfsrt_period   *periods; size_t nperiods, capperiods;
  gtfsrt_selector *ie;      size_t nie, capie;
  int has_cause;  int cause;
  int has_effect; int effect;
  int has_severity_level; int severity_level;
  gtfsrt_tstring url, header, description, tts_header, tts_description,
                 image_alternative_text, cause_detail, effect_detail;
} gtfsrt_alert;

typedef struct {
  char *id;
  int is_deleted;
  gtfsrt_trip_update *tu;   /* NULL when absent */
  gtfsrt_vehicle     *vp;
  gtfsrt_alert       *al;
} gtfsrt_entity;

typedef struct {
  int has_header;
  char *version;
  int has_incrementality; int incrementality;
  int has_timestamp; uint64_t timestamp;
  gtfsrt_entity *ent; size_t nent, capent;
  long unknown_fields;      /* fields skipped by wire type, all levels */
} gtfsrt_feed;

/* Decode `len` bytes. 0 on success (fill *out, release with gtfsrt_free);
 * -1 on any malformation, with a short reason in err (may be NULL) and *out
 * left empty — nothing partial is handed back. */
int  gtfsrt_decode(const unsigned char *buf, size_t len, gtfsrt_feed *out,
                   char *err, size_t errlen);
void gtfsrt_free(gtfsrt_feed *f);

/* Enum names as the .proto spells them; NULL for a value it does not define
 * (the caller then has a number, not a name — it must not invent one). */
const char *gtfsrt_cause_name(int v);
const char *gtfsrt_effect_name(int v);
const char *gtfsrt_stu_relationship_name(int v);
const char *gtfsrt_trip_relationship_name(int v);
const char *gtfsrt_vehicle_status_name(int v);
const char *gtfsrt_congestion_name(int v);
const char *gtfsrt_occupancy_name(int v);
const char *gtfsrt_severity_name(int v);

/* The translation for `lang` (exact, case-insensitive), else the one with no
 * language tag, else the first. NULL when the string carries none. */
const char *gtfsrt_tstring_pick(const gtfsrt_tstring *s, const char *lang);

#endif
