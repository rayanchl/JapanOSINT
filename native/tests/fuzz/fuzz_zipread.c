/* libFuzzer harness: lib/zipread.c on arbitrary bytes (a ZIP central directory
 * is attacker-controlled: offsets, sizes, names, deflate streams). */
#include "../../lib/zipread.h"
#include <stdint.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const char *buf = (const char *)data;
  size_t n = 0;
  char name[256];
  free(zip_first_entry(buf, size, &n));
  free(zip_find_entry(buf, size, "xl/workbook.xml", &n));
  int cnt = zip_entry_count(buf, size);
  for (int i = 0; i < cnt && i < 4; i++)
    free(zip_entry_at(buf, size, i, name, sizeof name, &n));
  free(zip_entry_at(buf, size, 0, NULL, 0, &n));
  return 0;
}
