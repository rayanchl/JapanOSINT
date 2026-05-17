/* lib/zipread.h — minimal single-entry ZIP reader (zlib raw inflate).
 * GDELT/CAMEO exports are a ZIP holding ONE CSV; this extracts the first
 * local file entry (stored or deflate). Not a general unzip — exactly the
 * subset gdelt-events needs. */
#ifndef JO_ZIPREAD_H
#define JO_ZIPREAD_H
#include <stddef.h>

/* Parse the first ZIP local file header in `buf` (binary, `len` bytes,
 * e.g. http_response.body/body_len) and return the entry's UNCOMPRESSED
 * bytes (malloc'd, NUL-terminated for CSV convenience; caller frees);
 * *out_len = uncompressed length. NULL on any malformity / unsupported
 * compression / inflate error. */
char *zip_first_entry(const char *buf, size_t len, size_t *out_len);

#endif
