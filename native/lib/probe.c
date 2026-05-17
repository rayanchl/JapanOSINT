/* lib/probe.c — port of _liveHelpers.fetchHead + ISO timestamp. See header. */
#include "probe.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

int probe_head(http_client *http, const char *url) {
  if (!http || !url || !*url) return 0;
  const char *h[] = {
    "User-Agent: JapanOSINT/1.0 (+https://github.com/rayanchl/JapanOSINT)",
    NULL };
  http_response r = {0};
  int rc = http_request(http, "HEAD", url, h, NULL, 0, 8000, 0, &r);
  long st = r.status;
  http_response_free(&r);
  if (rc != 0) return 0;                 /* transport error → false */
  return (st >= 200 && st < 400) ? 1 : 0; /* res.ok || 3xx redirect */
}

void probe_iso_now(char *out, size_t n) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm g;
  gmtime_r(&tv.tv_sec, &g);
  snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec, (int)(tv.tv_usec / 1000));
}
