/* lib/sgp4.c — Vallado SGP4 (WGS-72), near-Earth. Condensed from the
 * public-domain "Revisiting Spacetrack Report #3" reference (the same
 * algorithm satellite.js implements). See header for scope. */
#include "sgp4.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define PI 3.14159265358979323846
#define TWOPI (2.0 * PI)
#define DEG2RAD (PI / 180.0)

/* WGS-72 (satellite.js default gravity model) */
static const double RE = 6378.135;
static const double XKE = 0.0743669161331734132;   /* 60/sqrt(RE^3/MU) */
static const double J2 = 0.001082616;
static const double J4 = -0.00000165597;
static const double J3OJ2 = -0.00000253881 / 0.001082616;

double sgp4_jday_epoch(int year, double days) {
  /* Jan 0.0 of `year` (jday of year-1 Dec 31 12h) + days. */
  int y = year - 1;
  long jd = 1461L * (y + 4800) / 4 + 367L * 1 / 12      /* month=1 */
          - 3L * ((y + 4900) / 100) / 4 + 1 - 32075;    /* day=1 */
  return (double)jd - 0.5 + (days - 1.0);
}

double sgp4_gstime(double jdut1) {
  double tut1 = (jdut1 - 2451545.0) / 36525.0;
  double t = -6.2e-6 * tut1 * tut1 * tut1
           + 0.093104 * tut1 * tut1
           + (876600.0 * 3600.0 + 8640184.812866) * tut1
           + 67310.54841;
  t = fmod(t * DEG2RAD / 240.0, TWOPI);
  if (t < 0.0) t += TWOPI;
  return t;
}

static double tlefield(const char *line, int a, int b) { /* cols [a,b] 1-based */
  char buf[32]; int n = 0;
  for (int i = a - 1; i < b && line[i]; i++) buf[n++] = line[i];
  buf[n] = 0;
  return atof(buf);
}

int sgp4_twoline(const char *l1, const char *l2, sgp4_rec *r) {
  if (!l1 || !l2 || strlen(l1) < 64 || strlen(l2) < 63) return -1;
  memset(r, 0, sizeof *r);

  int epochyr = (int)tlefield(l1, 19, 20);
  double epochdays = tlefield(l1, 21, 32);

  /* bstar: mantissa cols 54-59 with implied leading '.', exp col 60-61 */
  char mant[8]; int mi = 0;
  for (int i = 53; i < 59; i++) mant[mi++] = l1[i]; mant[mi] = 0;
  int bexp = atoi(l1 + 59);
  double bstar = atof(mant) * 1e-5 * pow(10.0, bexp);

  double inclo = tlefield(l2, 9, 16) * DEG2RAD;
  double nodeo = tlefield(l2, 18, 25) * DEG2RAD;
  char ecc[12]; ecc[0] = '.';
  for (int i = 0; i < 7; i++) ecc[1 + i] = l2[26 + i]; ecc[8] = 0;
  double ecco = atof(ecc);
  double argpo = tlefield(l2, 35, 42) * DEG2RAD;
  double mo    = tlefield(l2, 44, 51) * DEG2RAD;
  double no    = tlefield(l2, 53, 63) * TWOPI / 1440.0;   /* rad/min */

  int year = epochyr < 57 ? epochyr + 2000 : epochyr + 1900;
  r->jdsatepoch = sgp4_jday_epoch(year, epochdays);
  r->ecco = ecco; r->inclo = inclo; r->nodeo = nodeo;
  r->argpo = argpo; r->mo = mo; r->bstar = bstar;

  /* ---- sgp4init (Vallado 2006, near-earth) ---- */
  double ss = 78.0 / RE + 1.0;
  double qzms2t = pow((120.0 - 78.0) / RE, 4.0);
  double cosio = cos(inclo), sinio = sin(inclo);
  double cosio2 = cosio * cosio;
  double eccsq = ecco * ecco;
  double omeosq = 1.0 - eccsq;
  double rteosq = sqrt(omeosq);

  /* un-Kozai the mean motion */
  double ak = pow(XKE / no, 2.0 / 3.0);
  double d1 = 0.75 * J2 * (3.0 * cosio2 - 1.0) / (rteosq * omeosq);
  double del = d1 / (ak * ak);
  double adel = ak * (1.0 - del * del - del *
                (1.0 / 3.0 + 134.0 * del * del / 81.0));
  del = d1 / (adel * adel);
  no = no / (1.0 + del);

  double ao = pow(XKE / no, 2.0 / 3.0);
  double po = ao * omeosq;
  double con42 = 1.0 - 5.0 * cosio2;
  r->con41 = -con42 - cosio2 - cosio2;
  double posq = po * po;
  double rp = ao * (1.0 - ecco);

  if (TWOPI / no >= 225.0) { r->deep = 1; r->no = no; r->a = ao; return 0; }
  if (rp < 1.0) { r->error = 5; return -1; }

  double sfour = ss, qzms24 = qzms2t;
  double perige = (rp - 1.0) * RE;
  if (perige < 156.0) {
    sfour = perige - 78.0;
    if (perige < 98.0) sfour = 20.0;
    qzms24 = pow((120.0 - sfour) / RE, 4.0);
    sfour = sfour / RE + 1.0;
  }
  double pinvsq = 1.0 / posq;
  double tsi = 1.0 / (ao - sfour);
  r->eta = ao * ecco * tsi;
  double etasq = r->eta * r->eta;
  double eeta = ecco * r->eta;
  double psisq = fabs(1.0 - etasq);
  double coef = qzms24 * pow(tsi, 4.0);
  double coef1 = coef / pow(psisq, 3.5);
  double cc2 = coef1 * no * (ao * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq)) +
              0.375 * J2 * tsi / psisq * r->con41 *
              (8.0 + 3.0 * etasq * (8.0 + etasq)));
  r->cc1 = bstar * cc2;
  double cc3 = 0.0;
  if (ecco > 1.0e-4)
    cc3 = -2.0 * coef * tsi * J3OJ2 * no * sinio / ecco;
  r->x1mth2 = 1.0 - cosio2;
  r->cc4 = 2.0 * no * coef1 * ao * omeosq *
           (r->eta * (2.0 + 0.5 * etasq) + ecco * (0.5 + 2.0 * etasq) -
            J2 * tsi / (ao * psisq) *
            (-3.0 * r->con41 * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta)) +
             0.75 * r->x1mth2 * (2.0 * etasq - eeta * (1.0 + etasq)) *
             cos(2.0 * argpo)));
  r->cc5 = 2.0 * coef1 * ao * omeosq *
           (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);
  double cosio4 = cosio2 * cosio2;
  double temp1 = 1.5 * J2 * pinvsq * no;
  double temp2 = 0.5 * temp1 * J2 * pinvsq;
  double temp3 = -0.46875 * J4 * pinvsq * pinvsq * no;
  r->mdot = no + 0.5 * temp1 * rteosq * r->con41 +
            0.0625 * temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
  r->argpdot = -0.5 * temp1 * con42 +
               0.0625 * temp2 * (7.0 - 114.0 * cosio2 + 395.0 * cosio4) +
               temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4);
  double xhdot1 = -temp1 * cosio;
  r->nodedot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) +
               2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;
  double xpidot = r->argpdot + r->nodedot;
  r->omgcof = bstar * cc3 * cos(argpo);
  r->xmcof = 0.0;
  if (ecco > 1.0e-4) r->xmcof = -(2.0 / 3.0) * coef * bstar / eeta;
  r->nodecf = 3.5 * omeosq * xhdot1 * r->cc1;
  r->t2cof = 1.5 * r->cc1;
  r->xlcof = -0.25 * J3OJ2 * sinio * (3.0 + 5.0 * cosio) /
             ((fabs(1.0 + cosio) > 1.5e-12) ? (1.0 + cosio) : 1.5e-12);
  r->aycof = -0.5 * J3OJ2 * sinio;
  double delmotemp = 1.0 + r->eta * cos(mo);
  r->delmo = delmotemp * delmotemp * delmotemp;
  r->sinmao = sin(mo);
  r->x7thm1 = 7.0 * cosio2 - 1.0;

  r->isimp = 0;
  if (rp < 220.0 / RE + 1.0) r->isimp = 1;
  if (!r->isimp) {
    double cc1sq = r->cc1 * r->cc1;
    r->d2 = 4.0 * ao * tsi * cc1sq;
    double temp = r->d2 * tsi * r->cc1 / 3.0;
    r->d3 = (17.0 * ao + sfour) * temp;
    r->d4 = 0.5 * temp * ao * tsi * (221.0 * ao + 31.0 * sfour) * r->cc1;
    r->t3cof = r->d2 + 2.0 * cc1sq;
    r->t4cof = 0.25 * (3.0 * r->d3 + r->cc1 * (12.0 * r->d2 + 10.0 * cc1sq));
    r->t5cof = 0.2 * (3.0 * r->d4 + 12.0 * r->cc1 * r->d3 +
               6.0 * r->d2 * r->d2 + 15.0 * cc1sq * (2.0 * r->d2 + cc1sq));
  }
  r->no = no; r->a = ao;
  r->gsto = sgp4_gstime(r->jdsatepoch);
  (void)xpidot;
  return 0;
}

int sgp4_propagate(const sgp4_rec *r, double tsince, double pos[3],
                    double vel[3]) {
  if (r->error || r->deep) return -1;

  double xmdf = r->mo + r->mdot * tsince;
  double argpdf = r->argpo + r->argpdot * tsince;
  double nodedf = r->nodeo + r->nodedot * tsince;
  double argpm = argpdf, mm = xmdf;
  double t2 = tsince * tsince;
  double nodem = nodedf + r->nodecf * t2;
  double tempa = 1.0 - r->cc1 * tsince;
  double tempe = r->bstar * r->cc4 * tsince;
  double templ = r->t2cof * t2;

  if (!r->isimp) {
    double delomg = r->omgcof * tsince;
    double delmtemp = 1.0 + r->eta * cos(xmdf);
    double delm = r->xmcof * (delmtemp * delmtemp * delmtemp - r->delmo);
    double temp = delomg + delm;
    mm = xmdf + temp;
    argpm = argpdf - temp;
    double t3 = t2 * tsince, t4 = t3 * tsince;
    tempa = tempa - r->d2 * t2 - r->d3 * t3 - r->d4 * t4;
    tempe = tempe + r->bstar * r->cc5 * (sin(mm) - r->sinmao);
    templ = templ + r->t3cof * t3 + t4 * (r->t4cof + tsince * r->t5cof);
  }

  double nm = r->no;
  double em = r->ecco;
  double inclm = r->inclo;
  double am = pow((XKE / nm), 2.0 / 3.0) * tempa * tempa;
  nm = XKE / pow(am, 1.5);
  em = em - tempe;
  if (em >= 1.0 || em < -0.001) return -1;
  if (em < 1.0e-6) em = 1.0e-6;
  mm = mm + r->no * templ;
  double xlm = mm + argpm + nodem;
  nodem = fmod(nodem, TWOPI);
  argpm = fmod(argpm, TWOPI);
  xlm = fmod(xlm, TWOPI);
  mm = fmod(xlm - argpm - nodem, TWOPI);

  double sinim = sin(inclm), cosim = cos(inclm);
  double ep = em, argpp = argpm, nodep = nodem, mp = mm;

  /* long-period periodics */
  double axnl = ep * cos(argpp);
  double temp = 1.0 / (am * (1.0 - ep * ep));
  double aynl = ep * sin(argpp) + temp * r->aycof;
  double xl = mp + argpp + nodep + temp * r->xlcof * axnl;

  /* solve Kepler */
  double u = fmod(xl - nodep, TWOPI);
  double eo1 = u, tem5 = 9999.9; int ktr = 1;
  double sineo1 = 0, coseo1 = 0;
  while (fabs(tem5) >= 1.0e-12 && ktr <= 10) {
    sineo1 = sin(eo1); coseo1 = cos(eo1);
    tem5 = 1.0 - coseo1 * axnl - sineo1 * aynl;
    tem5 = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
    if (fabs(tem5) >= 0.95) tem5 = tem5 > 0.0 ? 0.95 : -0.95;
    eo1 = eo1 + tem5; ktr++;
  }

  double ecose = axnl * coseo1 + aynl * sineo1;
  double esine = axnl * sineo1 - aynl * coseo1;
  double el2 = axnl * axnl + aynl * aynl;
  double pl = am * (1.0 - el2);
  if (pl < 0.0) return -1;

  double rl = am * (1.0 - ecose);
  double rdotl = sqrt(am) * esine / rl;
  double rvdotl = sqrt(pl) / rl;
  double betal = sqrt(1.0 - el2);
  temp = esine / (1.0 + betal);
  double sinu = am / rl * (sineo1 - aynl - axnl * temp);
  double cosu = am / rl * (coseo1 - axnl + aynl * temp);
  double su = atan2(sinu, cosu);
  double sin2u = (cosu + cosu) * sinu;
  double cos2u = 1.0 - 2.0 * sinu * sinu;
  temp = 1.0 / pl;
  double temp1 = 0.5 * J2 * temp;
  double temp2 = temp1 * temp;

  double mrt = rl * (1.0 - 1.5 * temp2 * betal * r->con41) +
               0.5 * temp1 * r->x1mth2 * cos2u;
  su = su - 0.25 * temp2 * r->x7thm1 * sin2u;
  double xnode = nodep + 1.5 * temp2 * cosim * sin2u;
  double xinc = inclm + 1.5 * temp2 * cosim * sinim * cos2u;
  double mvt = rdotl - nm * temp1 * r->x1mth2 * sin2u / XKE;
  double rvdot = rvdotl + nm * temp1 *
                 (r->x1mth2 * cos2u + 1.5 * r->con41) / XKE;

  double sinsu = sin(su), cossu = cos(su);
  double snod = sin(xnode), cnod = cos(xnode);
  double sini = sin(xinc), cosi = cos(xinc);
  double xmx = -snod * cosi, xmy = cnod * cosi;
  double ux = xmx * sinsu + cnod * cossu;
  double uy = xmy * sinsu + snod * cossu;
  double uz = sini * sinsu;
  double vx = xmx * cossu - cnod * sinsu;
  double vy = xmy * cossu - snod * sinsu;
  double vz = sini * cossu;

  pos[0] = mrt * ux * RE;
  pos[1] = mrt * uy * RE;
  pos[2] = mrt * uz * RE;
  double vkmps = RE * XKE / 60.0;
  vel[0] = (mvt * ux + rvdot * vx) * vkmps;
  vel[1] = (mvt * uy + rvdot * vy) * vkmps;
  vel[2] = (mvt * uz + rvdot * vz) * vkmps;
  if (mrt < 1.0) return -1;
  return 0;
}

void sgp4_geodetic(const double pos[3], double gmst,
                   double *lon_deg, double *lat_deg, double *alt_km) {
  /* satellite.js eciToGeodetic (WGS-84 ellipsoid). */
  const double a = 6378.137;
  const double e2 = 6.69437999014e-3;
  double r = sqrt(pos[0] * pos[0] + pos[1] * pos[1]);
  double lon = atan2(pos[1], pos[0]) - gmst;
  while (lon < -PI) lon += TWOPI;
  while (lon > PI) lon -= TWOPI;
  double lat = atan2(pos[2], r);
  double C = 1.0;
  for (int i = 0; i < 20; i++) {
    C = 1.0 / sqrt(1.0 - e2 * sin(lat) * sin(lat));
    lat = atan2(pos[2] + a * C * e2 * sin(lat), r);
  }
  double alt = r / cos(lat) - a * C;
  *lon_deg = lon / DEG2RAD;
  *lat_deg = lat / DEG2RAD;
  *alt_km = alt;
}
