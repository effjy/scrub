/* JPEG metadata scrubber.
 *
 * A JPEG is a sequence of marker segments. Every marker is 0xFF followed by a
 * type byte. Most segments carry a 2-byte big-endian length (including the
 * length field itself). We copy the structural segments and the entropy-coded
 * image data verbatim, and drop the metadata-bearing ones:
 *
 *   APP1  (0xE1) — Exif / XMP
 *   APP2..APP15  — ICC, Photoshop, vendor blobs
 *   APP13 (0xED) — IPTC / Photoshop IRB
 *   COM   (0xFE) — free-text comments
 *
 * APP0 (0xE0, the JFIF header) is kept so viewers still recognise the file.
 */
#include "scrub.h"
#include <string.h>

static int read_u16(FILE *f, uint16_t *out) {
    int hi = fgetc(f), lo = fgetc(f);
    if (hi == EOF || lo == EOF) return 0;
    *out = (uint16_t)((hi << 8) | lo);
    return 1;
}

static int copy_bytes(FILE *in, FILE *out, long n) {
    char buf[8192];
    while (n > 0) {
        size_t want = (n < (long)sizeof buf) ? (size_t)n : sizeof buf;
        size_t got = fread(buf, 1, want, in);
        if (got == 0) return 0;
        if (fwrite(buf, 1, got, out) != got) return 0;
        n -= (long)got;
    }
    return 1;
}

/* Is this marker a metadata segment we want to drop? */
static int is_metadata_marker(int m) {
    if (m == 0xFE) return 1;                 /* COM                       */
    if (m == 0xE1) return 1;                 /* APP1 (Exif/XMP)           */
    if (m >= 0xE2 && m <= 0xEF) return 1;    /* APP2..APP15               */
    return 0;                                /* keep APP0 (JFIF) and rest */
}

scrub_result_t scrub_jpeg(FILE *in, FILE *out) {
    scrub_result_t r = {0};
    uint16_t soi;

    if (!read_u16(in, &soi) || soi != 0xFFD8) {
        snprintf(r.err, sizeof r.err, "not a JPEG (bad SOI marker)");
        return r;
    }
    fputc(0xFF, out); fputc(0xD8, out);
    r.bytes_out += 2;

    for (;;) {
        int b = fgetc(in);
        if (b == EOF) break;
        if (b != 0xFF) {
            snprintf(r.err, sizeof r.err, "marker desync (expected 0xFF)");
            return r;
        }
        /* Skip any fill 0xFF bytes. */
        int marker;
        do { marker = fgetc(in); } while (marker == 0xFF);
        if (marker == EOF) break;

        /* Start of scan: copy the rest of the stream verbatim. */
        if (marker == 0xDA) {
            fputc(0xFF, out); fputc(0xDA, out);
            r.bytes_out += 2;
            int c;
            while ((c = fgetc(in)) != EOF) { fputc(c, out); r.bytes_out++; }
            break;
        }
        /* Standalone markers (no length): EOI and RSTn. */
        if (marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7)) {
            fputc(0xFF, out); fputc(marker, out);
            r.bytes_out += 2;
            if (marker == 0xD9) break;
            continue;
        }

        uint16_t len;
        if (!read_u16(in, &len) || len < 2) {
            snprintf(r.err, sizeof r.err, "truncated segment length");
            return r;
        }
        long payload = (long)len - 2;

        if (is_metadata_marker(marker)) {
            if (fseek(in, payload, SEEK_CUR) != 0) {
                snprintf(r.err, sizeof r.err, "seek past segment failed");
                return r;
            }
            r.removed++;
            r.bytes_in += payload + 4;
            continue;
        }

        /* Keep this segment: marker + length + payload. */
        fputc(0xFF, out); fputc(marker, out);
        fputc(len >> 8, out); fputc(len & 0xFF, out);
        if (!copy_bytes(in, out, payload)) {
            snprintf(r.err, sizeof r.err, "short read copying segment");
            return r;
        }
        r.bytes_out += payload + 4;
    }

    r.ok = 1;
    return r;
}
