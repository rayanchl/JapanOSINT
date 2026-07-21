/* lib/sgp4.h — SGP4 satellite propagator (Vallado "Revisiting Spacetrack
 * Report #3", WGS-72), the canonical algorithm satellite.js ports. Powers
 * satellite-tracking: CelesTrak TLE → ECI → geodetic lat/lon/alt.
 *
 * SCOPE: near-Earth SGP4 (period < 225 min). Deep-space objects (SDP4) are
 * flagged and skipped (propagate returns 0) — those rarely sit low "over
 * Japan"; documented post-parity vs satellite.js's bundled SDP4. */
#ifndef JO_SGP4_H
#define JO_SGP4_H

typedef struct {
  int    error, deep;
  double jdsatepoch;
  /* sgp4init-derived (near-earth) */
  double no, a, ecco, inclo, nodeo, argpo, mo, bstar;
  double aycof, con41, cc1, cc4, cc5, d2, d3, d4, delmo, eta, argpdot,
         omgcof, sinmao, t2cof, t3cof, t4cof, t5cof, x1mth2, x7thm1,
         mdot, nodedot, xlcof, xmcof, nodecf, gsto, isimp;
} sgp4_rec;

/* Parse the two TLE lines (69-char standard). Returns 0 on success. */
int sgp4_twoline(const char *l1, const char *l2, sgp4_rec *r);

/* Propagate `min` minutes past epoch → TEME position (km) + velocity
 * (km/s). Returns 0 on success (non-zero → unusable, skip the sat). */
int sgp4_propagate(const sgp4_rec *r, double min, double pos[3], double vel[3]);

/* Greenwich mean sidereal time (rad) for a Julian date. */
double sgp4_gstime(double jdut1);

/* TEME/ECI position (km) + gmst (rad) → geodetic. lon/lat in DEGREES,
 * alt in km (matches satellite.js eciToGeodetic+degreesLong/Lat). */
void sgp4_geodetic(const double pos[3], double gmst,
                   double *lon_deg, double *lat_deg, double *alt_km);

/* Julian date of a TLE epoch (full year, fractional day-of-year). */
double sgp4_jday_epoch(int year, double days);

#endif
