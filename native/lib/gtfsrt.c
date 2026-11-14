/* lib/gtfsrt.c — see gtfsrt.h. Protobuf wire format, read by hand, every read
 * bounded by the end of the enclosing message. */
#include "gtfsrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* wire types */
#define WT_VARINT 0
#define WT_I64    1
#define WT_LEN    2
#define WT_SGROUP 3
#define WT_EGROUP 4
#define WT_I32    5

typedef struct {
  const unsigned char *p, *end;
  char *err; size_t errlen;
  long *unknown;
} rd;

static int fail(rd *r, const char *why) {
  if (r->err && r->errlen && !r->err[0]) snprintf(r->err, r->errlen, "%s", why);
  return -1;
}

static int rd_varint(rd *r, uint64_t *v) {
  uint64_t x = 0;
  *v = 0;
  for (int i = 0; i < 10; i++) {
    if (r->p >= r->end) return fail(r, "truncated varint");
    unsigned char b = *r->p++;
    /* the 10th byte carries bit 63 only; anything more overflows 64 bits */
    if (i == 9 && (b & 0xFE)) return fail(r, "varint overflows 64 bits");
    x |= (uint64_t)(b & 0x7F) << (7 * i);
    if (!(b & 0x80)) { *v = x; return 0; }
  }
  return fail(r, "varint longer than 10 bytes");
}

static int rd_fixed32(rd *r, uint32_t *v) {
  *v = 0;
  if (r->end - r->p < 4) return fail(r, "truncated fixed32");
  *v = (uint32_t)r->p[0] | (uint32_t)r->p[1] << 8 |
       (uint32_t)r->p[2] << 16 | (uint32_t)r->p[3] << 24;
  r->p += 4;
  return 0;
}

static int rd_fixed64(rd *r, uint64_t *v) {
  *v = 0;
  if (r->end - r->p < 8) return fail(r, "truncated fixed64");
  uint64_t x = 0;
  for (int i = 7; i >= 0; i--) x = (x << 8) | r->p[i];
  r->p += 8;
  *v = x;
  return 0;
}

/* A length-delimited field's payload as a sub-reader sharing err/unknown. */
static int rd_len(rd *r, rd *sub) {
  uint64_t n;
  if (rd_varint(r, &n) != 0) return -1;
  if (n > (uint64_t)(r->end - r->p)) return fail(r, "length runs past end of message");
  *sub = *r;
  sub->end = r->p + n;
  r->p += n;
  return 0;
}

static int rd_tag(rd *r, uint32_t *field, int *wt) {
  uint64_t k;
  *field = 0; *wt = 0;
  if (rd_varint(r, &k) != 0) return -1;
  *wt = (int)(k & 7);
  uint64_t f = k >> 3;
  if (f == 0 || f > 0x1FFFFFFF) return fail(r, "invalid field number");
  *field = (uint32_t)f;
  return 0;
}

/* Skip one value of wire type `wt` (whose tag was already read). A group is
 * skipped through its matching end-group, nested groups included, to depth
 * GTFSRT_MAX_DEPTH. */
static int rd_skip(rd *r, uint32_t field, int wt, int depth) {
  uint64_t v; uint32_t v32; rd sub;
  switch (wt) {
  case WT_VARINT: return rd_varint(r, &v);
  case WT_I64:    return rd_fixed64(r, &v);
  case WT_I32:    return rd_fixed32(r, &v32);
  case WT_LEN:    return rd_len(r, &sub);
  case WT_SGROUP:
    if (depth >= GTFSRT_MAX_DEPTH) return fail(r, "groups nested too deep");
    for (;;) {
      uint32_t f; int w;
      if (r->p >= r->end) return fail(r, "truncated group");
      if (rd_tag(r, &f, &w) != 0) return -1;
      if (w == WT_EGROUP) {
        if (f != field) return fail(r, "mismatched end-group");
        return 0;
      }
      if (rd_skip(r, f, w, depth + 1) != 0) return -1;
    }
  case WT_EGROUP: return fail(r, "end-group without start-group");
  default:        return fail(r, "invalid wire type");
  }
}

static int skip_unknown(rd *r, uint32_t field, int wt) {
  if (r->unknown) (*r->unknown)++;
  return rd_skip(r, field, wt, 0);
}

static int want(rd *r, int wt, int expect) {
  if (wt != expect) return fail(r, "known field with the wrong wire type");
  return 0;
}

static int rd_string(rd *r, int wt, char **out) {
  rd s;
  if (want(r, wt, WT_LEN) || rd_len(r, &s)) return -1;
  size_t n = (size_t)(s.end - s.p);
  char *c = malloc(n + 1);
  if (!c) return fail(r, "out of memory");
  memcpy(c, s.p, n);
  c[n] = 0;
  free(*out);              /* a repeated singular field: the last one wins */
  *out = c;
  return 0;
}

static int rd_u64(rd *r, int wt, uint64_t *out) {
  *out = 0;
  return (want(r, wt, WT_VARINT) || rd_varint(r, out)) ? -1 : 0;
}
static int rd_i32(rd *r, int wt, int32_t *out) {
  uint64_t v;
  if (rd_u64(r, wt, &v)) return -1;
  /* int32 is a plain (not zigzag) varint: a negative value is its 64-bit
   * sign extension, ten bytes on the wire; the low 32 bits are the value. */
  *out = (int32_t)(uint32_t)v;
  return 0;
}
static int rd_i64(rd *r, int wt, int64_t *out) {
  uint64_t v;
  if (rd_u64(r, wt, &v)) return -1;
  *out = (int64_t)v;
  return 0;
}
static int rd_u32(rd *r, int wt, uint32_t *out) {
  uint64_t v;
  if (rd_u64(r, wt, &v)) return -1;
  *out = (uint32_t)v;
  return 0;
}
static int rd_float(rd *r, int wt, float *out) {
  uint32_t v;
  if (want(r, wt, WT_I32) || rd_fixed32(r, &v)) return -1;
  memcpy(out, &v, sizeof *out);
  return 0;
}
static int rd_double(rd *r, int wt, double *out) {
  uint64_t v;
  if (want(r, wt, WT_I64) || rd_fixed64(r, &v)) return -1;
  memcpy(out, &v, sizeof *out);
  return 0;
}

/* Grow a dynamic array by one zeroed element. */
static void *grow(rd *r, void **arr, size_t *n, size_t *cap, size_t elsz) {
  if (*n == *cap) {
    size_t nc = *cap ? *cap * 2 : 4;
    void *na = realloc(*arr, nc * elsz);
    if (!na) { fail(r, "out of memory"); return NULL; }
    *arr = na; *cap = nc;
  }
  void *e = (char *)*arr + (*n)++ * elsz;
  memset(e, 0, elsz);
  return e;
}

/* ── frees ────────────────────────────────────────────────────────────── */
static void free_trip(gtfsrt_trip *t) {
  free(t->trip_id); free(t->route_id); free(t->start_date); free(t->start_time);
}
static void free_vdesc(gtfsrt_vehicle_desc *v) {
  free(v->id); free(v->label); free(v->license_plate);
}
static void free_tstring(gtfsrt_tstring *s) {
  for (size_t i = 0; i < s->n; i++) { free(s->tr[i].text); free(s->tr[i].language); }
  free(s->tr);
}
static void free_tu(gtfsrt_trip_update *t) {
  if (!t) return;
  free_trip(&t->trip); free_vdesc(&t->vehicle);
  for (size_t i = 0; i < t->nstu; i++) free(t->stu[i].stop_id);
  free(t->stu); free(t);
}
static void free_vp(gtfsrt_vehicle *v) {
  if (!v) return;
  free_trip(&v->trip); free_vdesc(&v->vehicle); free(v->stop_id); free(v);
}
static void free_alert(gtfsrt_alert *a) {
  if (!a) return;
  free(a->periods);
  for (size_t i = 0; i < a->nie; i++) {
    free(a->ie[i].agency_id); free(a->ie[i].route_id); free(a->ie[i].stop_id);
    free_trip(&a->ie[i].trip);
  }
  free(a->ie);
  free_tstring(&a->url); free_tstring(&a->header); free_tstring(&a->description);
  free_tstring(&a->tts_header); free_tstring(&a->tts_description);
  free_tstring(&a->image_alternative_text);
  free_tstring(&a->cause_detail); free_tstring(&a->effect_detail);
  free(a);
}

void gtfsrt_free(gtfsrt_feed *f) {
  if (!f) return;
  free(f->version);
  for (size_t i = 0; i < f->nent; i++) {
    free(f->ent[i].id);
    free_tu(f->ent[i].tu); free_vp(f->ent[i].vp); free_alert(f->ent[i].al);
  }
  free(f->ent);
  memset(f, 0, sizeof *f);
}

/* ── messages ─────────────────────────────────────────────────────────── */
#define LOOP(r) \
  while ((r)->p < (r)->end) { \
    uint32_t f; int wt; \
    if (rd_tag((r), &f, &wt) != 0) return -1;

static int m_trip(rd *r, gtfsrt_trip *t) {
  LOOP(r)
    int rc;
    switch (f) {
    case 1: rc = rd_string(r, wt, &t->trip_id); break;
    case 2: rc = rd_string(r, wt, &t->start_time); break;
    case 3: rc = rd_string(r, wt, &t->start_date); break;
    case 4: { uint64_t v; rc = rd_u64(r, wt, &v);
              t->has_schedule_relationship = 1; t->schedule_relationship = (int)v; break; }
    case 5: rc = rd_string(r, wt, &t->route_id); break;
    case 6: rc = rd_u32(r, wt, &t->direction_id); t->has_direction_id = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_vdesc(rd *r, gtfsrt_vehicle_desc *v) {
  LOOP(r)
    int rc;
    switch (f) {
    case 1: rc = rd_string(r, wt, &v->id); break;
    case 2: rc = rd_string(r, wt, &v->label); break;
    case 3: rc = rd_string(r, wt, &v->license_plate); break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int sub_msg(rd *r, int wt, rd *sub) {
  return (want(r, wt, WT_LEN) || rd_len(r, sub)) ? -1 : 0;
}

static int m_event(rd *r, gtfsrt_event *e) {
  LOOP(r)
    int rc;
    switch (f) {
    case 1: rc = rd_i32(r, wt, &e->delay); e->has_delay = 1; break;
    case 2: rc = rd_i64(r, wt, &e->time); e->has_time = 1; break;
    case 3: rc = rd_i32(r, wt, &e->uncertainty); e->has_uncertainty = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_stu(rd *r, gtfsrt_stu *s) {
  LOOP(r)
    int rc; rd sub;
    switch (f) {
    case 1: rc = rd_u32(r, wt, &s->stop_sequence); s->has_stop_sequence = 1; break;
    case 2: rc = sub_msg(r, wt, &sub) || m_event(&sub, &s->arrival); s->has_arrival = 1; break;
    case 3: rc = sub_msg(r, wt, &sub) || m_event(&sub, &s->departure); s->has_departure = 1; break;
    case 4: rc = rd_string(r, wt, &s->stop_id); break;
    case 5: { uint64_t v; rc = rd_u64(r, wt, &v);
              s->has_schedule_relationship = 1; s->schedule_relationship = (int)v; break; }
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_tu(rd *r, gtfsrt_trip_update *t) {
  LOOP(r)
    int rc; rd sub;
    switch (f) {
    case 1: rc = sub_msg(r, wt, &sub) || m_trip(&sub, &t->trip); t->has_trip = 1; break;
    case 2: {
      gtfsrt_stu *s = grow(r, (void **)&t->stu, &t->nstu, &t->capstu, sizeof *s);
      rc = !s || sub_msg(r, wt, &sub) || m_stu(&sub, s);
      break;
    }
    case 3: rc = sub_msg(r, wt, &sub) || m_vdesc(&sub, &t->vehicle); t->has_vehicle = 1; break;
    case 4: rc = rd_u64(r, wt, &t->timestamp); t->has_timestamp = 1; break;
    case 5: rc = rd_i32(r, wt, &t->delay); t->has_delay = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_position(rd *r, gtfsrt_vehicle *v) {
  int have_lat = 0, have_lon = 0;
  LOOP(r)
    int rc; float fl;
    switch (f) {
    case 1: rc = rd_float(r, wt, &fl); v->lat = fl; have_lat = 1; break;
    case 2: rc = rd_float(r, wt, &fl); v->lon = fl; have_lon = 1; break;
    case 3: rc = rd_float(r, wt, &v->bearing); v->has_bearing = 1; break;
    case 4: rc = rd_double(r, wt, &v->odometer); v->has_odometer = 1; break;
    case 5: rc = rd_float(r, wt, &v->speed); v->has_speed = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  /* latitude and longitude are both `required` in Position: a position with
   * either missing is not a position, and 0.0 is a real coordinate. */
  v->has_position = have_lat && have_lon;
  return 0;
}

static int m_vp(rd *r, gtfsrt_vehicle *v) {
  LOOP(r)
    int rc; rd sub;
    switch (f) {
    case 1: rc = sub_msg(r, wt, &sub) || m_trip(&sub, &v->trip); v->has_trip = 1; break;
    case 2: rc = sub_msg(r, wt, &sub) || m_position(&sub, v); break;
    case 3: rc = rd_u32(r, wt, &v->current_stop_sequence);
            v->has_current_stop_sequence = 1; break;
    case 4: { uint64_t x; rc = rd_u64(r, wt, &x);
              v->has_current_status = 1; v->current_status = (int)x; break; }
    case 5: rc = rd_u64(r, wt, &v->timestamp); v->has_timestamp = 1; break;
    case 6: { uint64_t x; rc = rd_u64(r, wt, &x);
              v->has_congestion_level = 1; v->congestion_level = (int)x; break; }
    case 7: rc = rd_string(r, wt, &v->stop_id); break;
    case 8: rc = sub_msg(r, wt, &sub) || m_vdesc(&sub, &v->vehicle); v->has_vehicle = 1; break;
    case 9: { uint64_t x; rc = rd_u64(r, wt, &x);
              v->has_occupancy_status = 1; v->occupancy_status = (int)x; break; }
    case 10: rc = rd_u32(r, wt, &v->occupancy_percentage);
             v->has_occupancy_percentage = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_translation(rd *r, gtfsrt_translation *t) {
  LOOP(r)
    int rc;
    switch (f) {
    case 1: rc = rd_string(r, wt, &t->text); break;
    case 2: rc = rd_string(r, wt, &t->language); break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_tstring(rd *r, gtfsrt_tstring *s) {
  LOOP(r)
    int rc; rd sub;
    if (f == 1) {
      gtfsrt_translation *t = grow(r, (void **)&s->tr, &s->n, &s->cap, sizeof *t);
      rc = !t || sub_msg(r, wt, &sub) || m_translation(&sub, t);
    } else rc = skip_unknown(r, f, wt);
    if (rc) return -1;
  }
  return 0;
}

static int m_period(rd *r, gtfsrt_period *p) {
  LOOP(r)
    int rc;
    switch (f) {
    case 1: rc = rd_u64(r, wt, &p->start); p->has_start = 1; break;
    case 2: rc = rd_u64(r, wt, &p->end); p->has_end = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_selector(rd *r, gtfsrt_selector *s) {
  LOOP(r)
    int rc; rd sub;
    switch (f) {
    case 1: rc = rd_string(r, wt, &s->agency_id); break;
    case 2: rc = rd_string(r, wt, &s->route_id); break;
    case 3: rc = rd_i32(r, wt, &s->route_type); s->has_route_type = 1; break;
    case 4: rc = sub_msg(r, wt, &sub) || m_trip(&sub, &s->trip); s->has_trip = 1; break;
    case 5: rc = rd_string(r, wt, &s->stop_id); break;
    case 6: rc = rd_u32(r, wt, &s->direction_id); s->has_direction_id = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_alert(rd *r, gtfsrt_alert *a) {
  LOOP(r)
    int rc; rd sub;
    switch (f) {
    case 1: {
      gtfsrt_period *p = grow(r, (void **)&a->periods, &a->nperiods, &a->capperiods, sizeof *p);
      rc = !p || sub_msg(r, wt, &sub) || m_period(&sub, p);
      break;
    }
    case 5: {
      gtfsrt_selector *s = grow(r, (void **)&a->ie, &a->nie, &a->capie, sizeof *s);
      rc = !s || sub_msg(r, wt, &sub) || m_selector(&sub, s);
      break;
    }
    case 6: { uint64_t v; rc = rd_u64(r, wt, &v); a->has_cause = 1; a->cause = (int)v; break; }
    case 7: { uint64_t v; rc = rd_u64(r, wt, &v); a->has_effect = 1; a->effect = (int)v; break; }
    case 8:  rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->url); break;
    case 10: rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->header); break;
    case 11: rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->description); break;
    case 12: rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->tts_header); break;
    case 13: rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->tts_description); break;
    case 14: { uint64_t v; rc = rd_u64(r, wt, &v);
               a->has_severity_level = 1; a->severity_level = (int)v; break; }
    case 16: rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->image_alternative_text); break;
    case 17: rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->cause_detail); break;
    case 18: rc = sub_msg(r, wt, &sub) || m_tstring(&sub, &a->effect_detail); break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_entity(rd *r, gtfsrt_entity *e) {
  LOOP(r)
    int rc; rd sub;
    switch (f) {
    case 1: rc = rd_string(r, wt, &e->id); break;
    case 2: { uint64_t v; rc = rd_u64(r, wt, &v); e->is_deleted = v != 0; break; }
    case 3:
      if (!e->tu && !(e->tu = calloc(1, sizeof *e->tu))) return fail(r, "out of memory");
      rc = sub_msg(r, wt, &sub) || m_tu(&sub, e->tu);
      break;
    case 4:
      if (!e->vp && !(e->vp = calloc(1, sizeof *e->vp))) return fail(r, "out of memory");
      rc = sub_msg(r, wt, &sub) || m_vp(&sub, e->vp);
      break;
    case 5:
      if (!e->al && !(e->al = calloc(1, sizeof *e->al))) return fail(r, "out of memory");
      rc = sub_msg(r, wt, &sub) || m_alert(&sub, e->al);
      break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_header(rd *r, gtfsrt_feed *fd) {
  LOOP(r)
    int rc;
    switch (f) {
    case 1: rc = rd_string(r, wt, &fd->version); break;
    case 2: { uint64_t v; rc = rd_u64(r, wt, &v);
              fd->has_incrementality = 1; fd->incrementality = (int)v; break; }
    case 3: rc = rd_u64(r, wt, &fd->timestamp); fd->has_timestamp = 1; break;
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}

static int m_feed(rd *r, gtfsrt_feed *fd) {
  LOOP(r)
    int rc; rd sub;
    switch (f) {
    case 1: rc = sub_msg(r, wt, &sub) || m_header(&sub, fd); fd->has_header = 1; break;
    case 2: {
      gtfsrt_entity *e = grow(r, (void **)&fd->ent, &fd->nent, &fd->capent, sizeof *e);
      rc = !e || sub_msg(r, wt, &sub) || m_entity(&sub, e);
      break;
    }
    default: rc = skip_unknown(r, f, wt);
    }
    if (rc) return -1;
  }
  return 0;
}
#undef LOOP

int gtfsrt_decode(const unsigned char *buf, size_t len, gtfsrt_feed *out,
                  char *err, size_t errlen) {
  if (err && errlen) err[0] = 0;
  if (!out) return -1;
  memset(out, 0, sizeof *out);
  rd r = { buf, buf ? buf + len : NULL, err, errlen, &out->unknown_fields };
  if (!buf && len) { fail(&r, "no buffer"); return -1; }
  if (m_feed(&r, out) != 0) { gtfsrt_free(out); return -1; }
  if (!out->has_header || !out->version) {
    fail(&r, out->has_header ? "FeedHeader has no gtfs_realtime_version"
                             : "no FeedHeader (not a GTFS-RT FeedMessage)");
    gtfsrt_free(out);
    return -1;
  }
  return 0;
}

/* ── names ────────────────────────────────────────────────────────────── */
const char *gtfsrt_cause_name(int v) {
  static const char *const N[] = { NULL,
    "UNKNOWN_CAUSE", "OTHER_CAUSE", "TECHNICAL_PROBLEM", "STRIKE",
    "DEMONSTRATION", "ACCIDENT", "HOLIDAY", "WEATHER", "MAINTENANCE",
    "CONSTRUCTION", "POLICE_ACTIVITY", "MEDICAL_EMERGENCY" };
  return (v > 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

const char *gtfsrt_effect_name(int v) {
  static const char *const N[] = { NULL,
    "NO_SERVICE", "REDUCED_SERVICE", "SIGNIFICANT_DELAYS", "DETOUR",
    "ADDITIONAL_SERVICE", "MODIFIED_SERVICE", "OTHER_EFFECT",
    "UNKNOWN_EFFECT", "STOP_MOVED", "NO_EFFECT", "ACCESSIBILITY_ISSUE" };
  return (v > 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

const char *gtfsrt_stu_relationship_name(int v) {
  static const char *const N[] = { "SCHEDULED", "SKIPPED", "NO_DATA", "UNSCHEDULED" };
  return (v >= 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

const char *gtfsrt_trip_relationship_name(int v) {
  /* the .proto assigns no value 4 */
  static const char *const N[] = { "SCHEDULED", "ADDED", "UNSCHEDULED",
    "CANCELED", NULL, "REPLACEMENT", "DUPLICATED", "DELETED", "NEW" };
  return (v >= 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

const char *gtfsrt_vehicle_status_name(int v) {
  static const char *const N[] = { "INCOMING_AT", "STOPPED_AT", "IN_TRANSIT_TO" };
  return (v >= 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

const char *gtfsrt_congestion_name(int v) {
  static const char *const N[] = { "UNKNOWN_CONGESTION_LEVEL", "RUNNING_SMOOTHLY",
    "STOP_AND_GO", "CONGESTION", "SEVERE_CONGESTION" };
  return (v >= 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

const char *gtfsrt_occupancy_name(int v) {
  static const char *const N[] = { "EMPTY", "MANY_SEATS_AVAILABLE",
    "FEW_SEATS_AVAILABLE", "STANDING_ROOM_ONLY", "CRUSHED_STANDING_ROOM_ONLY",
    "FULL", "NOT_ACCEPTING_PASSENGERS", "NO_DATA_AVAILABLE", "NOT_BOARDABLE" };
  return (v >= 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

const char *gtfsrt_severity_name(int v) {
  static const char *const N[] = { NULL, "UNKNOWN_SEVERITY", "INFO", "WARNING", "SEVERE" };
  return (v > 0 && v < (int)(sizeof N / sizeof *N)) ? N[v] : NULL;
}

static int lang_eq(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  return *a == *b;
}

const char *gtfsrt_tstring_pick(const gtfsrt_tstring *s, const char *lang) {
  if (!s || s->n == 0) return NULL;
  if (lang)
    for (size_t i = 0; i < s->n; i++)
      if (s->tr[i].text && s->tr[i].language && lang_eq(s->tr[i].language, lang))
        return s->tr[i].text;
  for (size_t i = 0; i < s->n; i++)
    if (s->tr[i].text && (!s->tr[i].language || !s->tr[i].language[0]))
      return s->tr[i].text;
  for (size_t i = 0; i < s->n; i++)
    if (s->tr[i].text) return s->tr[i].text;
  return NULL;
}
