/* scrub — forensic-grade metadata scrubber
 * Shared declarations.
 */
#ifndef SCRUB_H
#define SCRUB_H

#include <stdio.h>
#include <stdint.h>

/* Result of a scrub operation. */
typedef struct {
    int      ok;            /* 1 = success, 0 = failure                  */
    int      removed;       /* number of metadata segments/chunks dropped */
    uint64_t bytes_in;      /* original payload size                      */
    uint64_t bytes_out;     /* scrubbed payload size                      */
    char     err[256];      /* error message when ok == 0                 */
} scrub_result_t;

/* Detected file type. */
typedef enum {
    FT_UNKNOWN = 0,
    FT_JPEG,
    FT_PNG
} filetype_t;

/* Sniff a file's type from its magic bytes. */
filetype_t scrub_detect(const char *path);

/* Scrub one file in place (writes to a temp file, then renames).
 * If reset_times is non-zero, atime/mtime are reset to the epoch.
 * Returns a populated scrub_result_t. */
scrub_result_t scrub_file(const char *path, int reset_times);

/* Type-specific scrubbers: read all of `in`, write cleaned bytes to `out`. */
scrub_result_t scrub_jpeg(FILE *in, FILE *out);
scrub_result_t scrub_png(FILE *in, FILE *out);

#endif /* SCRUB_H */
