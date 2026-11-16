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

/* The `index`-th entry (0-based, in central-directory order), as
 * zip_find_entry returns a named one, with the entry's own path written into
 * `name_out` — an indexed caller has no other way to learn what it read.
 * NULL when the index is past the end or the entry's compression is
 * unsupported. For an archive whose ENTRIES ARE THE COLLECTION: the Assemblée
 * nationale publishes its 19,165 written questions as one JSON file each
 * inside a single 50 MB ZIP, so reading the first entry is 19,164 records
 * dropped at a seam. */
char *zip_entry_at(const char *buf, size_t len, int index,
                   char *name_out, size_t name_n, size_t *out_len);

/* How many entries the archive holds, from its End Of Central Directory
 * record — the archive's own count, not a scan of the bytes. Searching the
 * body for "PK\3\4" signatures (what hpengine used to do) also matches those
 * four bytes inside COMPRESSED data, so a one-entry archive could be reported
 * as holding more. Returns -1 when no EOCD record is found (a truncated or
 * non-ZIP body), in which case the count is unknown, not 1. A ZIP64 archive
 * reports the count from its ZIP64 end-of-directory record (clamped to INT_MAX). */
int zip_entry_count(const char *buf, size_t len);

/* One-pass iteration over every member (central-directory order). Use this to
 * read an archive whose ENTRIES ARE THE COLLECTION; zip_entry_at(i) re-walks
 * the directory from the start each call.
 *
 *   zip_iter it;
 *   if (zip_iter_open(&it, buf, len) == 0) {
 *     char name[512]; char *data; size_t n; int rc;
 *     while ((rc = zip_iter_next(&it, name, sizeof name, &data, &n)) != 0) {
 *       if (rc < 0) { ... a member that cannot be read: disclose it ... continue; }
 *       ... use data ...; free(data);
 *     }
 *     // it.declared entries were declared, it.walked were reached
 *   }
 *
 * zip_iter_next returns 1 with the member's bytes (caller frees), -1 for a
 * member whose framing is intact but whose bytes cannot be produced
 * (unsupported compression, truncated, will not inflate, or ZIP64 numbers the
 * directory does not carry) — `name_out` still holds its name — and 0 at the
 * end of the walkable directory. ZIP64 archives (more than 65,535 entries, or
 * members over 4 GiB) are read through the ZIP64 end-of-directory record and
 * extra fields; `walked < declared` at the end means part of the directory
 * could not be walked and those members are unread. */
typedef struct {
  const char *buf; size_t len;
  size_t p, cd_end;
  unsigned long long declared;   /* entries the archive says it holds */
  unsigned long long walked;     /* entries reached so far */
  int zip64, done;
} zip_iter;
int zip_iter_open(zip_iter *it, const char *buf, size_t len);
int zip_iter_next(zip_iter *it, char *name_out, size_t name_n,
                  char **data, size_t *out_len);

#endif
