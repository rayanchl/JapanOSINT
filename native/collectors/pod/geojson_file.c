/* test harness source: reads $GEOJSON_FILE, emits via the geojson toolkit.
 * Lets us parity-test featureToMasterItem/featureUid vs Node deterministically. */
#include "source.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
static int run(const source_ctx *c, intel_sink *s) {
  (void)c; const char *p = getenv("GEOJSON_FILE"); if (!p) return -1;
  FILE *f = fopen(p, "rb"); if (!f) return -1;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  if (n < 0) { fclose(f); return -1; }
  char *b=malloc((size_t)n+1); if(!b){ fclose(f); return -1; }
  /* A short read used to be ignored, leaving a truncated buffer that cJSON
   * then parsed as if it were the whole file. Read it all or fail honestly. */
  size_t got = fread(b,1,(size_t)n,f); int rerr = (got != (size_t)n);
  b[got]=0; fclose(f);
  if (rerr) { free(b); return -1; }
  cJSON *d=cJSON_Parse(b); free(b); if(!d) return -1;
  int e=geojson_emit_doc(s,c->source_id,d); cJSON_Delete(d);
  return e>=0?0:-1; }
static const source_def geojson_file_def = {
  .id="geojson-file", .collector="_test", .name="GeoJSON file test",
  .name_ja="",  .update_interval_sec=0, .run=run };
REGISTER_SOURCE(geojson_file_def)
