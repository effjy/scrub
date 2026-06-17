/* PNG metadata scrubber.
 *
 * A PNG is an 8-byte signature followed by chunks:
 *   [4-byte length][4-byte type][length bytes of data][4-byte CRC]
 *
 * We copy critical and rendering chunks unchanged and drop the
 * metadata-bearing ancillary ones:
 *
 *   tEXt, zTXt, iTXt — textual metadata (author, software, comments)
 *   tIME             — last-modification timestamp
 *   eXIf             — embedded Exif block
 *   caBX             — C2PA / JUMBF content-provenance manifest
 *
 * Because kept chunks are byte-for-byte identical, their stored CRCs remain
 * valid — no recomputation needed.
 */
#include "scrub.h"
#include <string.h>

static const unsigned char PNG_SIG[8] =
    {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

static int read_u32(FILE *f, uint32_t *out) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    *out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  |  (uint32_t)b[3];
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

static int is_metadata_chunk(const char t[4]) {
    static const char *drop[] = {"tEXt", "zTXt", "iTXt", "tIME", "eXIf",
                                 "caBX"};
    for (size_t i = 0; i < sizeof drop / sizeof *drop; i++)
        if (memcmp(t, drop[i], 4) == 0) return 1;
    return 0;
}

scrub_result_t scrub_png(FILE *in, FILE *out) {
    scrub_result_t r = {0};
    unsigned char sig[8];

    if (fread(sig, 1, 8, in) != 8 || memcmp(sig, PNG_SIG, 8) != 0) {
        snprintf(r.err, sizeof r.err, "not a PNG (bad signature)");
        return r;
    }
    fwrite(PNG_SIG, 1, 8, out);
    r.bytes_out += 8;

    for (;;) {
        uint32_t len;
        char type[4];
        if (!read_u32(in, &len)) break;            /* clean EOF */
        if (fread(type, 1, 4, in) != 4) {
            snprintf(r.err, sizeof r.err, "truncated chunk type");
            return r;
        }

        long whole = (long)len + 4;                /* data + CRC */

        if (is_metadata_chunk(type)) {
            if (fseek(in, whole, SEEK_CUR) != 0) {
                snprintf(r.err, sizeof r.err, "seek past chunk failed");
                return r;
            }
            r.removed++;
            r.bytes_in += whole + 8;
        } else {
            unsigned char hdr[8];
            hdr[0] = len >> 24; hdr[1] = len >> 16;
            hdr[2] = len >> 8;  hdr[3] = len;
            memcpy(hdr + 4, type, 4);
            fwrite(hdr, 1, 8, out);
            if (!copy_bytes(in, out, whole)) {
                snprintf(r.err, sizeof r.err, "short read copying chunk");
                return r;
            }
            r.bytes_out += whole + 8;
        }

        if (memcmp(type, "IEND", 4) == 0) break;
    }

    r.ok = 1;
    return r;
}
