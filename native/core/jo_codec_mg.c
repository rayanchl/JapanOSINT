/* core/jo_codec_mg.c — jo_qs_get_mg(), the mongoose-backed half of
 * jo_codec.h (split out so jo_codec.c links without mongoose; see there). */
#include "jo_codec.h"
#include "../third_party/mongoose.h"

int jo_qs_get_mg(const char *qs, const char *key, char *out, size_t cap,
                 int *too_long) {
  struct mg_str s = mg_str(qs ? qs : "");
  int n = mg_http_get_var(&s, key, out, cap);
  if (n <= 0 && out && cap) out[0] = 0;
  if (n == -3) { if (too_long) *too_long = 1; return -1; }
  return n > 0;
}
