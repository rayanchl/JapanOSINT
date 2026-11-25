/* libFuzzer harness: lib/xlsx.c (a ZIP of XML parts) -> CSV, and sheet listing. */
#include "../../lib/xlsx.h"
#include <stdint.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const char *buf = (const char *)data;
  char err[256];
  char *csv = NULL; size_t csv_len = 0;
  if (xlsx_to_csv(buf, size, 0, NULL, &csv, &csv_len, err, sizeof err) == 0) free(csv);
  csv = NULL;
  if (xlsx_to_csv(buf, size, 1, "Sheet1", &csv, &csv_len, err, sizeof err) == 0) free(csv);
  free(xlsx_sheet_names(buf, size, err, sizeof err));
  return 0;
}
