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

void bigfile_close(bigfile *bf);

#endif
