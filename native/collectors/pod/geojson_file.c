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
  char *b=malloc(n+1); fread(b,1,n,f); b[n]=0; fclose(f);
  cJSON *d=cJSON_Parse(b); free(b); if(!d) return -1;
  int e=geojson_emit_doc(s,c->source_id,d); cJSON_Delete(d);
  return e>=0?0:-1; }
static const source_def geojson_file_def = {
  .id="geojson-file", .collector="_test", .name="GeoJSON file test",
  .name_ja="",  .update_interval_sec=0, .run=run };
REGISTER_SOURCE(geojson_file_def)
