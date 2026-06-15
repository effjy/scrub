/* scrub — forensic-grade metadata scrubber (CLI front-end).
 *
 * Strips metadata from images in place, with an optional filesystem
 * timestamp reset. Dependency-free: native JPEG/PNG parsing, no libexif.
 */
#include "scrub.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>

static const char *VERSION = "0.1.0";

filetype_t scrub_detect(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return FT_UNKNOWN;

    unsigned char m[8];
    size_t n = fread(m, 1, sizeof m, f);
    fclose(f);

    if (n >= 2 && m[0] == 0xFF && m[1] == 0xD8)
        return FT_JPEG;
    if (n >= 8 && m[0] == 0x89 && m[1] == 'P' && m[2] == 'N' && m[3] == 'G')
        return FT_PNG;
    return FT_UNKNOWN;
}

scrub_result_t scrub_file(const char *path, int reset_times) {
    scrub_result_t r = {0};

    filetype_t ft = scrub_detect(path);
    if (ft == FT_UNKNOWN) {
        snprintf(r.err, sizeof r.err, "unsupported or unreadable file type");
        return r;
    }

    FILE *in = fopen(path, "rb");
    if (!in) {
        snprintf(r.err, sizeof r.err, "cannot open input");
        return r;
    }

    /* Write to a sibling temp file so a crash never corrupts the original. */
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s.scrub.tmp", path);
    FILE *out = fopen(tmp, "wb");
    if (!out) {
        fclose(in);
        snprintf(r.err, sizeof r.err, "cannot create temp file");
        return r;
    }

    r = (ft == FT_JPEG) ? scrub_jpeg(in, out) : scrub_png(in, out);

    fclose(in);
    if (fclose(out) != 0 && r.ok) {
        r.ok = 0;
        snprintf(r.err, sizeof r.err, "failed to flush temp file");
    }

    if (!r.ok) {
        remove(tmp);
        return r;
    }

    /* Preserve the original permissions on the replacement. */
    struct stat st;
    if (stat(path, &st) == 0)
        chmod(tmp, st.st_mode & 07777);

    if (rename(tmp, path) != 0) {
        remove(tmp);
        r.ok = 0;
        snprintf(r.err, sizeof r.err, "failed to replace original");
        return r;
    }

    if (reset_times) {
        struct utimbuf t = {0, 0};   /* epoch */
        utime(path, &t);
    }

    return r;
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "scrub %s — forensic-grade metadata scrubber\n\n"
        "Usage: %s [options] <file>...\n\n"
        "Options:\n"
        "  -t, --reset-times   Reset file atime/mtime to the epoch\n"
        "  -h, --help          Show this help\n"
        "  -V, --version       Show version\n\n"
        "Supported: JPEG (Exif/XMP/IPTC/COM), PNG (tEXt/zTXt/iTXt/tIME/eXIf)\n",
        VERSION, argv0);
}

int main(int argc, char **argv) {
    int reset_times = 0;
    int first_file = -1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]); return 0;
        }
        if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
            printf("scrub %s\n", VERSION); return 0;
        }
        if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--reset-times")) {
            reset_times = 1; continue;
        }
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "scrub: unknown option '%s'\n", argv[i]);
            usage(argv[0]); return 2;
        }
        first_file = i;
        break;
    }

    if (first_file < 0) { usage(argv[0]); return 2; }

    int failures = 0;
    for (int i = first_file; i < argc; i++) {
        scrub_result_t r = scrub_file(argv[i], reset_times);
        if (r.ok) {
            struct stat st;
            unsigned long long sz = (stat(argv[i], &st) == 0)
                                    ? (unsigned long long)st.st_size : 0;
            printf("\033[36m✓\033[0m %s — removed %d segment%s, now %llu bytes\n",
                   argv[i], r.removed, r.removed == 1 ? "" : "s", sz);
        } else {
            fprintf(stderr, "\033[31m✗\033[0m %s — %s\n", argv[i], r.err);
            failures++;
        }
    }

    return failures ? 1 : 0;
}
