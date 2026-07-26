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

/* Named-entry lookup via the central directory. Returns the UNCOMPRESSED bytes
 * of the entry called `name` (malloc'd, NUL-terminated; caller frees) and sets
 * *out_len to the uncompressed length; NULL if absent/unsupported/corrupt.
 *
 * Matches `name` against each entry's full path AND its basename, so a feed
 * zipped with a wrapping directory ("feed/stop_times.txt") still resolves for
 * a lookup of "stop_times.txt".
 *
 * Unlike zip_first_entry this reads the compressed/uncompressed sizes from the
 * central directory rather than the local file header: an archive written with
 * a streaming data descriptor (general-purpose bit 3) leaves both sizes zero in
 * the local header, and the CD is the only authoritative copy. GTFS-JP feeds
 * from api.gtfs-data.jp are written this way. */
char *zip_find_entry(const char *buf, size_t len, const char *name,
                     size_t *out_len);

#endif
