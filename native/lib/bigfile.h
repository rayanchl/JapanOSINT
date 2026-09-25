/* lib/bigfile.h — bounded-memory streaming line reader for large breach dumps.
 * The breach-ingest pipeline (core/breach_index.c) feeds multi-GB dumps through
 * this so we never load a whole file into RAM. Lines longer than BF_MAXLINE are
 * skipped (counted), never truncated-and-guessed. See docs/breach-check-pipeline.md S2. */
#ifndef JO_BIGFILE_H
#define JO_BIGFILE_H
#include <stddef.h>

typedef struct bigfile bigfile;

/* Open for streaming; NULL on error. Reads binary so CRLF is handled here. */
bigfile *bigfile_open(const char *path);

/* Next logical line, NUL-terminated, CR/LF stripped. Returns a pointer into an
 * internal buffer valid until the next call; NULL at EOF. *len (optional) gets
 * the stripped length. Over-long lines are silently skipped and counted. */
const char *bigfile_next(bigfile *bf, size_t *len);

/* Lines dropped for exceeding BF_MAXLINE, and lines successfully returned.
 * `struct bigfile` is opaque, so before these existed a caller had no way to
 * read the skip count bigfile_next() was already keeping — the tally was
 * real but unreachable, which is the same invisible discard house rule 2
 * (docs/SOURCE_EXHAUSTIVENESS.md) forbids for a collector: a shortfall has to
 * be reported as data, and it cannot be while nothing outside this file can
 * even ask for the number. A caller streaming a breach dump should check
 * bigfile_skipped() after the last bigfile_next() and, when it is non-zero,
 * emit a collector-truncation-notice (or equivalent) rather than let the scan
 * report a clean, silently incomplete pass. */
unsigned long long bigfile_skipped(const bigfile *bf);
unsigned long long bigfile_lines(const bigfile *bf);

void bigfile_close(bigfile *bf);

#endif
