#include "bcn_decode.h"

#include <string.h>

typedef struct {
    uint8_t ns, pb, rb, isb, cb, ab, epb, spb, ib, ib2;
} bc7_mode_t;

static const bc7_mode_t bc7_modes[8] = {
    {3, 4, 0, 0, 4, 0, 1, 0, 3, 0},
    {2, 6, 0, 0, 6, 0, 0, 1, 3, 0},
    {3, 6, 0, 0, 5, 0, 0, 0, 2, 0},
    {2, 6, 0, 0, 7, 0, 1, 0, 2, 0},
    {1, 0, 2, 1, 5, 6, 0, 0, 2, 3},
    {1, 0, 2, 0, 7, 8, 0, 0, 2, 2},
    {1, 0, 0, 0, 7, 7, 1, 0, 4, 0},
    {2, 6, 0, 0, 5, 5, 1, 0, 2, 0}
};

static const uint16_t bc7_part2[64] = {
    0xcccc, 0x8888, 0xeeee, 0xecc8, 0xc880, 0xfeec, 0xfec8, 0xec80, 0xc800, 0xffec,
    0xfe80, 0xe800, 0xffe8, 0xff00, 0xfff0, 0xf000, 0xf710, 0x008e, 0x7100, 0x08ce,
    0x008c, 0x7310, 0x3100, 0x8cce, 0x088c, 0x3110, 0x6666, 0x366c, 0x17e8, 0x0ff0,
    0x718e, 0x399c, 0xaaaa, 0xf0f0, 0x5a5a, 0x33cc, 0x3c3c, 0x55aa, 0x9696, 0xa55a,
    0x73ce, 0x13c8, 0x324c, 0x3bdc, 0x6996, 0xc33c, 0x9966, 0x0660, 0x0272, 0x04e4,
    0x4e40, 0x2720, 0xc936, 0x936c, 0x39c6, 0x639c, 0x9336, 0x9cc6, 0x817e, 0xe718,
    0xccf0, 0x0fcc, 0x7744, 0xee22
};

static const uint32_t bc7_part3[64] = {
    0xaa685050, 0x6a5a5040, 0x5a5a4200, 0x5450a0a8, 0xa5a50000, 0xa0a05050, 0x5555a0a0,
    0x5a5a5050, 0xaa550000, 0xaa555500, 0xaaaa5500, 0x90909090, 0x94949494, 0xa4a4a4a4,
    0xa9a59450, 0x2a0a4250, 0xa5945040, 0x0a425054, 0xa5a5a500, 0x55a0a0a0, 0xa8a85454,
    0x6a6a4040, 0xa4a45000, 0x1a1a0500, 0x0050a4a4, 0xaaa59090, 0x14696914, 0x69691400,
    0xa08585a0, 0xaa821414, 0x50a4a450, 0x6a5a0200, 0xa9a58000, 0x5090a0a8, 0xa8a09050,
    0x24242424, 0x00aa5500, 0x24924924, 0x24499224, 0x50a50a50, 0x500aa550, 0xaaaa4444,
    0x66660000, 0xa5a0a5a0, 0x50a050a0, 0x69286928, 0x44aaaa44, 0x66666600, 0xaa444444,
    0x54a854a8, 0x95809580, 0x96969600, 0xa85454a8, 0x80959580, 0xaa141414, 0x96960000,
    0xaaaa1414, 0xa05050a0, 0xa0a5a5a0, 0x96000000, 0x40804080, 0xa9a8a9a8, 0xaaaaaa44,
    0x2a4a5254
};

static const uint8_t bc7_anchor2[64] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 2, 8, 2, 2, 8, 8, 15, 2, 8, 2, 2, 8, 8, 2, 2,
    15, 15, 6, 8, 2, 8, 15, 15, 2, 8, 2, 2, 2, 15, 15, 6,
    6, 2, 6, 8, 15, 15, 2, 2, 15, 15, 15, 15, 15, 2, 2, 15
};

static const uint8_t bc7_anchor3a[64] = {
    3, 3, 15, 15, 8, 3, 15, 15, 8, 8, 6, 6, 6, 5, 3, 3,
    3, 3, 8, 15, 3, 3, 6, 10, 5, 8, 8, 6, 8, 5, 15, 15,
    8, 15, 3, 5, 6, 10, 8, 15, 15, 3, 15, 5, 15, 15, 15, 15,
    3, 15, 5, 5, 5, 8, 5, 10, 5, 10, 8, 13, 15, 12, 3, 3
};

static const uint8_t bc7_anchor3b[64] = {
    15, 8, 8, 3, 15, 15, 3, 8, 15, 15, 15, 15, 15, 15, 15, 8,
    15, 8, 15, 3, 15, 8, 15, 8, 3, 15, 6, 10, 15, 15, 10, 8,
    15, 3, 15, 10, 10, 8, 9, 10, 6, 15, 8, 15, 3, 6, 6, 8,
    15, 3, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3, 15, 15, 8
};

static const uint8_t bc7_w2[4] = {0, 21, 43, 64};
static const uint8_t bc7_w3[8] = {0, 9, 18, 27, 37, 46, 55, 64};
static const uint8_t bc7_w4[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

static const uint8_t *bc7_weights(unsigned bits) {
    return bits == 2 ? bc7_w2 : bits == 3 ? bc7_w3 : bc7_w4;
}

static uint64_t ld64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

static unsigned getbits(const uint64_t w[2], unsigned pos, unsigned n) {
    if (!n) return 0;
    unsigned i = pos >> 6, s = pos & 63;
    uint64_t v = w[i] >> s;
    if (i == 0 && s + n > 64) v |= w[1] << (64 - s);
    return (unsigned)(v & ((1u << n) - 1));
}

static unsigned expand_bits(unsigned v, unsigned bits) {
    v <<= 8 - bits;
    return v | (v >> bits);
}

static void rgb565(unsigned c, int out[3]) {
    out[0] = (int)expand_bits((c >> 11) & 31, 5);
    out[1] = (int)expand_bits((c >> 5) & 63, 6);
    out[2] = (int)expand_bits(c & 31, 5);
}

static void bc1_color(const uint8_t *b, uint8_t *rgba, int force4) {
    unsigned c0 = b[0] | (b[1] << 8), c1 = b[2] | (b[3] << 8);
    int p[4][4], e0[3], e1[3];
    rgb565(c0, e0);
    rgb565(c1, e1);
    for (int k = 0; k < 3; k++) {
        p[0][k] = e0[k];
        p[1][k] = e1[k];
        if (c0 > c1 || force4) {
            p[2][k] = (2 * e0[k] + e1[k]) / 3;
            p[3][k] = (e0[k] + 2 * e1[k]) / 3;
        } else {
            p[2][k] = (e0[k] + e1[k]) / 2;
            p[3][k] = 0;
        }
    }
    p[0][3] = p[1][3] = p[2][3] = 255;
    p[3][3] = (c0 > c1 || force4) ? 255 : 0;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            int s = (b[4 + y] >> (2 * x)) & 3;
            for (int k = 0; k < 4; k++) rgba[(y * 4 + x) * 4 + k] = (uint8_t)p[s][k];
        }
}

static void bc4_channel(const uint8_t *b, uint8_t *rgba, int ch) {
    int a0 = b[0], a1 = b[1], a[8];
    a[0] = a0;
    a[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; i++) a[1 + i] = ((7 - i) * a0 + i * a1) / 7;
    } else {
        for (int i = 1; i <= 4; i++) a[1 + i] = ((5 - i) * a0 + i * a1) / 5;
        a[6] = 0;
        a[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 5; i >= 0; i--) bits = (bits << 8) | b[2 + i];
    for (int i = 0; i < 16; i++) rgba[i * 4 + ch] = (uint8_t)a[(bits >> (3 * i)) & 7];
}

static int snorm_round(int num, int den) {
    return num >= 0 ? (2 * num + den) / (2 * den) : -((2 * -num + den) / (2 * den));
}

static void bc4s_channel(const uint8_t *b, uint8_t *out, int ch) {
    int a0 = (int8_t)b[0], a1 = (int8_t)b[1], a[8];
    if (a0 < -127) a0 = -127;
    if (a1 < -127) a1 = -127;
    a[0] = a0;
    a[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; i++) a[1 + i] = snorm_round((7 - i) * a0 + i * a1, 7);
    } else {
        for (int i = 1; i <= 4; i++) a[1 + i] = snorm_round((5 - i) * a0 + i * a1, 5);
        a[6] = -127;
        a[7] = 127;
    }
    uint64_t bits = 0;
    for (int i = 5; i >= 0; i--) bits = (bits << 8) | b[2 + i];
    for (int i = 0; i < 16; i++) out[i * 4 + ch] = (uint8_t)(int8_t)a[(bits >> (3 * i)) & 7];
}

static void bc7_block(const uint8_t *src, uint8_t *rgba) {
    unsigned m = src[0];
    if (!m) {
        memset(rgba, 0, 64);
        return;
    }
    unsigned mode = 0;
    while (!((m >> mode) & 1)) mode++;
    const bc7_mode_t *md = &bc7_modes[mode];
    uint64_t w[2] = {ld64(src), ld64(src + 8)};
    unsigned pos = mode + 1;
    unsigned part = getbits(w, pos, md->pb); pos += md->pb;
    unsigned rot = getbits(w, pos, md->rb); pos += md->rb;
    unsigned isel = getbits(w, pos, md->isb); pos += md->isb;
    unsigned ne = md->ns * 2u;
    unsigned ep[6][4];
    for (unsigned c = 0; c < 3; c++)
        for (unsigned e = 0; e < ne; e++) {
            ep[e][c] = getbits(w, pos, md->cb);
            pos += md->cb;
        }
    for (unsigned e = 0; e < ne; e++) {
        ep[e][3] = md->ab ? getbits(w, pos, md->ab) : 255;
        pos += md->ab;
    }
    unsigned cbits = md->cb, abits = md->ab;
    if (md->epb) {
        for (unsigned e = 0; e < ne; e++) {
            unsigned p = getbits(w, pos++, 1);
            for (unsigned c = 0; c < 3; c++) ep[e][c] = (ep[e][c] << 1) | p;
            if (md->ab) ep[e][3] = (ep[e][3] << 1) | p;
        }
        cbits++;
        if (md->ab) abits++;
    } else if (md->spb) {
        for (unsigned s = 0; s < md->ns; s++) {
            unsigned p = getbits(w, pos++, 1);
            for (unsigned e = s * 2; e < s * 2 + 2; e++) {
                for (unsigned c = 0; c < 3; c++) ep[e][c] = (ep[e][c] << 1) | p;
                if (md->ab) ep[e][3] = (ep[e][3] << 1) | p;
            }
        }
        cbits++;
        if (md->ab) abits++;
    }
    for (unsigned e = 0; e < ne; e++) {
        for (unsigned c = 0; c < 3; c++) ep[e][c] = expand_bits(ep[e][c], cbits);
        if (md->ab) ep[e][3] = expand_bits(ep[e][3], abits);
    }
    unsigned cpos = pos;
    unsigned apos = pos + 16 * md->ib - md->ns;
    const uint8_t *cw = bc7_weights(md->ib);
    const uint8_t *aw = bc7_weights(md->ib2 ? md->ib2 : md->ib);
    for (unsigned i = 0; i < 16; i++) {
        unsigned s = 0, anchor = i == 0;
        if (md->ns == 2) {
            s = (bc7_part2[part] >> i) & 1;
            anchor |= i == bc7_anchor2[part];
        } else if (md->ns == 3) {
            s = (bc7_part3[part] >> (2 * i)) & 3;
            anchor |= i == bc7_anchor3a[part] || i == bc7_anchor3b[part];
        }
        unsigned ibits = md->ib - anchor;
        unsigned i0 = getbits(w, cpos, ibits);
        cpos += ibits;
        const unsigned *e0 = ep[s * 2], *e1 = ep[s * 2 + 1];
        unsigned wc, wa;
        if (md->ib2) {
            unsigned ibits2 = md->ib2 - (i == 0);
            unsigned i1 = getbits(w, apos, ibits2);
            apos += ibits2;
            if (isel) {
                wc = aw[i1];
                wa = cw[i0];
            } else {
                wc = cw[i0];
                wa = aw[i1];
            }
        } else {
            wc = wa = cw[i0];
        }
        unsigned px[4];
        for (unsigned c = 0; c < 3; c++) px[c] = ((64 - wc) * e0[c] + wc * e1[c] + 32) >> 6;
        px[3] = ((64 - wa) * e0[3] + wa * e1[3] + 32) >> 6;
        if (rot) {
            unsigned t = px[3];
            px[3] = px[rot - 1];
            px[rot - 1] = t;
        }
        for (unsigned c = 0; c < 4; c++) rgba[i * 4 + c] = (uint8_t)px[c];
    }
}

int bcn_block_bytes(int fmt) {
    switch (fmt) {
    case BCN_BC1:
    case BCN_BC4:
    case BCN_BC4S:
        return 8;
    case BCN_BC2:
    case BCN_BC3:
    case BCN_BC5:
    case BCN_BC5S:
    case BCN_BC7:
        return 16;
    }
    return 0;
}

int bcn_is_signed(int fmt) { return fmt == BCN_BC4S || fmt == BCN_BC5S; }

void bcn_decode_block(int fmt, const uint8_t *blk, uint8_t rgba[64]) {
    switch (fmt) {
    case BCN_BC1:
        bc1_color(blk, rgba, 0);
        break;
    case BCN_BC2:
        bc1_color(blk + 8, rgba, 1);
        for (int i = 0; i < 16; i++) {
            unsigned a = (blk[i >> 1] >> ((i & 1) * 4)) & 15;
            rgba[i * 4 + 3] = (uint8_t)(a | (a << 4));
        }
        break;
    case BCN_BC3:
        bc1_color(blk + 8, rgba, 1);
        bc4_channel(blk, rgba, 3);
        break;
    case BCN_BC4:
        for (int i = 0; i < 16; i++) {
            rgba[i * 4 + 1] = 0;
            rgba[i * 4 + 2] = 0;
            rgba[i * 4 + 3] = 255;
        }
        bc4_channel(blk, rgba, 0);
        break;
    case BCN_BC5:
        for (int i = 0; i < 16; i++) {
            rgba[i * 4 + 2] = 0;
            rgba[i * 4 + 3] = 255;
        }
        bc4_channel(blk, rgba, 0);
        bc4_channel(blk + 8, rgba, 1);
        break;
    case BCN_BC7:
        bc7_block(blk, rgba);
        break;
    case BCN_BC4S:
        for (int i = 0; i < 16; i++) {
            rgba[i * 4 + 1] = 0;
            rgba[i * 4 + 2] = 0;
            rgba[i * 4 + 3] = 127;
        }
        bc4s_channel(blk, rgba, 0);
        break;
    case BCN_BC5S:
        for (int i = 0; i < 16; i++) {
            rgba[i * 4 + 2] = 0;
            rgba[i * 4 + 3] = 127;
        }
        bc4s_channel(blk, rgba, 0);
        bc4s_channel(blk + 8, rgba, 1);
        break;
    default:
        memset(rgba, 0, 64);
    }
}

int bcn_decode_image(int fmt, const uint8_t *blocks, size_t len, unsigned w, unsigned h, uint8_t *rgba) {
    int bb = bcn_block_bytes(fmt);
    if (!bb || !blocks || !rgba || !w || !h) return -1;
    unsigned bw = (w + 3) / 4, bh = (h + 3) / 4;
    if (len < (size_t)bw * bh * (size_t)bb) return -1;
    uint8_t tmp[64];
    for (unsigned by = 0; by < bh; by++)
        for (unsigned bx = 0; bx < bw; bx++) {
            bcn_decode_block(fmt, blocks + ((size_t)by * bw + bx) * (size_t)bb, tmp);
            unsigned cw = w - bx * 4 < 4 ? w - bx * 4 : 4, ch = h - by * 4 < 4 ? h - by * 4 : 4;
            for (unsigned y = 0; y < ch; y++)
                memcpy(rgba + ((size_t)(by * 4 + y) * w + bx * 4) * 4, tmp + y * 16, cw * 4);
        }
    return 0;
}
