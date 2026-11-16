/* tests/unit/test_sgp4.c — lib/sgp4.c against Vallado's published SGP4
 * verification vectors (SGP4-VER.TLE / tcppver.out, satellite 00005).
 *
 * Reference values (TEME, km and km/s) for
 *   1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753
 *   2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667
 *  t=0      x= 7022.46529266 y=-1400.08296755 z=    0.03995155
 *  t=360    x=-7154.03120202 y=-3783.17682504 z=-3536.19412294
 *  t=720    x=-7134.59340119 y= 6531.68641334 z= 3260.27186483
 *  t=1440   x= -938.55923943 y=-6268.18748831 z=-4294.02924751
 * Both come from Vallado's own tcppver.out (WGS-72, near-earth branch).
 *
 * TOLERANCE. 1e-3 km (1 m). The reference output is printed to 1e-8 km, so
 * the residual is the implementation's: this file uses the same WGS-72
 * constants but a hand-written port of sgp4init/sgp4 (no deep-space, no
 * Lyddane choice), so differences of a few 1e-6 km are expected from operation
 * order; 1 m is three orders larger than that and ~3 orders smaller than the
 * error of any drift/rounding BUG that matters (a wrong J2/J3 term moves the
 * position by kilometres). Velocity tolerance 1e-5 km/s (1 cm/s). */
#include <stdio.h>
#include "../../lib/sgp4.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static int near_(double a, double b, double tol) { return fabs(a - b) <= tol; }

static const char *L1 = "1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753";
static const char *L2 = "2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667";

int main(void) {
  sgp4_rec r; double p[3], v[3];

  printf("-- Vallado verification vectors (sat 00005)\n");
  ok(sgp4_twoline(L1, L2, &r) == 0, "TLE parses");
  ok(!r.deep, "period 133 min is near-earth");
  ok(sgp4_propagate(&r, 0.0, p, v) == 0, "propagate t=0");
  printf("   t=0   %.8f %.8f %.8f | %.9f %.9f %.9f\n", p[0], p[1], p[2], v[0], v[1], v[2]);
  ok(near_(p[0], 7022.46529266, 1e-3) && near_(p[1], -1400.08296755, 1e-3) && near_(p[2], 0.03995155, 1e-3),
     "position at epoch matches reference to 1 m");
  ok(near_(v[0], 1.893841015, 1e-5) && near_(v[1], 6.405893759, 1e-5) && near_(v[2], 4.534807250, 1e-5),
     "velocity at epoch matches reference to 1 cm/s");
  struct { double t, x, y, z, vx, vy, vz; } ref[] = {
    {360,  -7154.03120202, -3783.17682504, -3536.19412294,  4.741887409, -4.151817765, -2.093935425},
    {720,  -7134.59340119,  6531.68641334,  3260.27186483,  0, 0, 0}  /* velocity not asserted here */,
    {1440,  -938.55923943, -6268.18748831, -4294.02924751,   0, 0, 0},
  };
  for (int i = 0; i < 3; i++) {
    char w[96];
    ok(sgp4_propagate(&r, ref[i].t, p, v) == 0, "propagate");
    snprintf(w, sizeof w, "position at t=%g min matches reference to 1 m", ref[i].t);
    ok(near_(p[0], ref[i].x, 1e-3) && near_(p[1], ref[i].y, 1e-3) && near_(p[2], ref[i].z, 1e-3), w);
    if (i == 0) {
      ok(near_(v[0], ref[i].vx, 1e-5) && near_(v[1], ref[i].vy, 1e-5) && near_(v[2], ref[i].vz, 1e-5),
         "velocity at t=360 matches reference to 1 cm/s");
    }
  }

  printf("-- invariants\n");
  ok(sgp4_propagate(&r, -360.0, p, v) == 0, "propagating BEFORE epoch works");
  double rad = sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
  ok(rad > 6378.0 && rad < 6378.0 + 8000.0, "radius at t=-360 is between the surface and the apogee");
  for (double t = 0; t <= 1440; t += 17) {
    if (sgp4_propagate(&r, t, p, v) != 0) { ok(0, "propagation failed inside one day"); break; }
    double rr = sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
    /* perigee a(1-e) ~ 6600 km, apogee a(1+e) ~ 10000 km */
    if (rr < 6500.0 || rr > 10500.0) { ok(0, "radius left the perigee/apogee envelope"); break; }
  }
  ok(1, "radius stays inside the [perigee, apogee] envelope for a day");
  { /* energy: specific orbital energy is ~constant (drift only from bstar, tiny here) */
    double e0, e1;
    sgp4_propagate(&r, 0, p, v);   e0 = 0.5*(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]) - 398600.8/sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
    sgp4_propagate(&r, 700, p, v); e1 = 0.5*(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]) - 398600.8/sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
    ok(fabs(e1 - e0) < 0.05, "specific orbital energy conserved to <0.05 km2/s2 over 700 min");
  }

  printf("-- epoch and sidereal time\n");
  /* 2000 day 179.78495062 = June 27 2000 18:50:19 UTC: JD 2451723.28495062 */
  ok(near_(r.jdsatepoch, 2451723.28495062, 1e-6),
            "TLE epoch -> Julian date (lib/sgp4.c sgp4_jday_epoch is 306 days early: 367L*1/12 + 1 should be the Dec-31 terms 367L*10/12 + 31)");
  ok(near_(sgp4_jday_epoch(2000, 1.0), 2451544.5, 1e-9), "2000 day 1.0 = JD 2451544.5 (Jan 1 0h)");
  ok(near_(sgp4_jday_epoch(1999, 1.0), 2451179.5, 1e-9), "1999 day 1.0 = JD 2451179.5");
  ok(near_(sgp4_jday_epoch(2000, 2.0) - sgp4_jday_epoch(2000, 1.0), 1.0, 1e-12), "epoch is linear in fractional day");
  ok(near_(sgp4_jday_epoch(2001, 1.0) - sgp4_jday_epoch(2000, 1.0), 366.0, 1e-9) , "2000 -> 2001 is 366 days (leap year)");
  ok(near_(sgp4_gstime(2451545.0), 4.894961212823756, 1e-6), "GMST at J2000.0 = 280.46 deg");
  { double g = sgp4_gstime(2451545.0 + 123.456); ok(g >= 0 && g < 2 * PI, "GMST normalised to [0, 2pi)");
    g = sgp4_gstime(2400000.5); ok(g >= 0 && g < 2 * PI, "GMST normalised for a date before J2000 (negative branch)"); }
  /* what a CORRECT gsto would give: GMST(2451723.28495062) = 198.7689 deg */
  ok(near_(r.gsto, 198.76893360 * PI / 180.0, 1e-4), "gsto of the 00005 epoch = 198.77 deg (follows from the epoch JD)");
  ok(near_(sgp4_gstime(r.jdsatepoch), sgp4_gstime(2451723.28495062), 1e-6), "gstime(epoch) consistent");

  printf("-- geodetic conversion\n");
  { double lon, lat, alt;
    double eq[3] = {7000.0, 0.0, 0.0};
    sgp4_geodetic(eq, 0.0, &lon, &lat, &alt);
    ok(near_(lon, 0, 1e-9) && near_(lat, 0, 1e-9) && near_(alt, 7000.0 - 6378.137, 1e-6), "equatorial point: lat 0, alt = r - a");
    sgp4_geodetic(eq, PI / 2, &lon, &lat, &alt);
    ok(near_(lon, -90.0, 1e-9), "gmst rotates longitude (east positive): 90 deg of GMST -> -90");
    sgp4_geodetic(eq, -3 * PI, &lon, &lat, &alt);
    ok(lon >= -180.0001 && lon <= 180.0001, "longitude normalised for out-of-range gmst");
    double hi[3] = {100.0, 0.0, 7000.0};
    sgp4_geodetic(hi, 0.0, &lon, &lat, &alt);
    ok(lat > 89.0 && lat < 90.0 && alt > 600 && alt < 700, "high-latitude point: lat ~89.2, altitude ~640 km over the ellipsoid");
    double pole[3] = {0.0, 0.0, 7000.0};
    sgp4_geodetic(pole, 0.0, &lon, &lat, &alt);
    ok(near_(lat, 90.0, 1e-6), "exactly over the pole: latitude 90");
    ok(alt > 600 && alt < 700, "exactly over the pole: altitude ~643 km (alt = r/cos(lat) - aC divides 0 by 0 -> -6399 km, a singularity inherited from satellite.js)");
    sgp4_propagate(&r, 0, p, v);
    sgp4_geodetic(p, 0.0, &lon, &lat, &alt);
    ok(fabs(lat) < 0.01 && near_(alt, 782.5, 2.0), "sat 00005 at epoch: on the equator, ~782 km over the ellipsoid (radius 7160 km)");
  }

  printf("-- rejects and edge cases\n");
  ok(sgp4_twoline(NULL, L2, &r) == -1 && sgp4_twoline(L1, NULL, &r) == -1, "NULL line -> -1");
  ok(sgp4_twoline("1 short", L2, &r) == -1, "short line 1 -> -1");
  ok(sgp4_twoline(L1, "2 short", &r) == -1, "short line 2 -> -1");
  {
    /* geostationary: 1.0027 rev/day = period 1436 min -> deep space, flagged and skipped */
    const char *g1 = "1 00006U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753";
    const char *g2 = "2 00006   0.0500  75.0000 0002000 100.0000 260.0000  1.00270000 12345";
    sgp4_rec g;
    int rc = sgp4_twoline(g1, g2, &g);
    int pr = sgp4_propagate(&g, 10.0, p, v);
    ok((rc != 0 || g.deep) && pr != 0, "deep-space (24 h) TLE is flagged and propagate refuses it");
  }
  {
    /* Lines of the right length full of garbage digits / zero mean motion must not crash. */
    const char *z1 = "1 00007U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753";
    const char *z2 = "2 00007   0.0000   0.0000 0000000   0.0000   0.0000  0.00000000 00000";
    sgp4_rec z; int rc = sgp4_twoline(z1, z2, &z);
    int pr = sgp4_propagate(&z, 1.0, p, v);
    ok(rc != 0 || pr != 0, "zero mean motion TLE is refused by twoline or propagate (no NaN position accepted)");
    sgp4_rec e; memset(&e, 0, sizeof e); e.error = 3;
    ok(sgp4_propagate(&e, 0, p, v) != 0, "a record flagged with error refuses to propagate");
    memset(&e, 0, sizeof e); e.deep = 1;
    ok(sgp4_propagate(&e, 0, p, v) != 0, "a record flagged deep refuses to propagate");
  }
  {
    /* Decayed: huge drag and very long propagation must return non-zero or finite, never garbage silently. */
    sgp4_rec d; sgp4_twoline(L1, L2, &d); d.bstar = 5.0;
    int pr = sgp4_propagate(&d, 100000.0, p, v);
    ok(pr != 0 || (isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2])), "decayed orbit: error code or finite output, not NaN with rc=0");
  }

  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
