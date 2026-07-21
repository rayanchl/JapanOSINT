/* core/geoproxyapi.h — cached external geo proxies (split from w4api):
 * GET /api/geocode(+/reverse) provider fallback chain, /api/plateau/tilesets
 * GraphQL catalog. Both libcurl-backed with a 24h in-proc TTL cache. */
#ifndef JO_GEOPROXYAPI_H
#define JO_GEOPROXYAPI_H
char *geoproxy_geocode_forward(const char *q, const char *qAlt);
char *geoproxy_geocode_reverse(double lat, double lon);
char *geoproxy_plateau_tilesets(int lod, int *status);
#endif
