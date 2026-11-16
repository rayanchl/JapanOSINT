/* libFuzzer harness: lib/csv.c. The first byte picks delimiter/header mode, the
 * rest is the document (NUL-terminated, as every caller passes it). */
#include "../../lib/csv.h"
#include "../../third_party/cJSON.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 1) return 0;
  static const char delims[] = { ',', '\t', '|', ';' };
  char d = delims[data[0] & 3];
  int headers = (data[0] >> 2) & 1;
  size_t n = size - 1;
  char *text = malloc(n + 1);
  if (!text) return 0;
  memcpy(text, data + 1, n);
  text[n] = 0;
  cJSON *a = csv_parse_d(text, headers, d);
  cJSON_Delete(a);
  a = csv_parse_x(text, headers, NULL, (data[0] >> 3) & 3, "#");
  cJSON_Delete(a);
  a = csv_parse_wellformed(text, headers, NULL, 0, NULL);
  cJSON_Delete(a);
  (void)csv_record_end(text, n, 0, d);
  free(csv_decode_sjis(text, n));
  free(csv_decode_charset(text, n, "EUC-JP"));
  (void)csv_is_utf8(text, n);
  free(text);
  return 0;
}
