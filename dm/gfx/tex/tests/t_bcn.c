#include "../bcn_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define BLOCKS_X 32
#define BLOCKS_Y 32

typedef struct { int fmt; uint64_t hash; } ref_t;

static const ref_t refs[] = {
    {1, 0xfa02e93bb92752e5ull},
    {2, 0x3aa907325c7279f2ull},
    {3, 0x5fc767c61becfbdfull},
    {4, 0xdbcede345a40de49ull},
    {5, 0x779dc8d0d6ead406ull},
    {6, 0x9a460e3817685041ull},
};

static uint64_t splitmix(uint64_t *state) {
    *state += 0x9E3779B97F4A7C15ull;
    uint64_t z = *state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static uint64_t fnv(const uint8_t *p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

static void make_blocks(int fmt, uint8_t *out) {
    int bb = bcn_block_bytes(fmt);
    uint64_t state = 0x1000 + (uint64_t)fmt;
    for (int i = 0; i < BLOCKS_X * BLOCKS_Y; i++) {
        uint8_t *blk = out + (size_t)i * (size_t)bb;
        for (int o = 0; o < bb; o += 8) {
            uint64_t z = splitmix(&state);
            for (int k = 0; k < 8; k++) blk[o + k] = (uint8_t)(z >> (8 * k));
        }
        if (fmt == BCN_BC7) {
            unsigned mode = (unsigned)i % 8;
            blk[0] = (uint8_t)((blk[0] & ~((1u << (mode + 1)) - 1)) | (1u << mode));
        }
    }
}

static void check_pixel(const uint8_t *px, int r, int g, int b, int a, const char *what) {
    CHECK(px[0] == r && px[1] == g && px[2] == b && px[3] == a, "%s: got %d,%d,%d,%d want %d,%d,%d,%d", what, px[0], px[1], px[2], px[3], r, g, b, a);
}

int main(void) {
    CHECK(bcn_block_bytes(BCN_BC1) == 8 && bcn_block_bytes(BCN_BC4) == 8, "8-byte formats");
    CHECK(bcn_block_bytes(BCN_BC2) == 16 && bcn_block_bytes(BCN_BC3) == 16 && bcn_block_bytes(BCN_BC5) == 16 && bcn_block_bytes(BCN_BC7) == 16, "16-byte formats");
    CHECK(bcn_block_bytes(0) == 0 && bcn_block_bytes(99) == 0, "unknown format has no size");

    const unsigned w = BLOCKS_X * 4, h = BLOCKS_Y * 4;
    uint8_t *blocks = malloc((size_t)BLOCKS_X * BLOCKS_Y * 16), *img = malloc((size_t)w * h * 4);
    for (size_t i = 0; i < sizeof refs / sizeof refs[0]; i++) {
        int fmt = refs[i].fmt;
        make_blocks(fmt, blocks);
        size_t len = (size_t)BLOCKS_X * BLOCKS_Y * (size_t)bcn_block_bytes(fmt);
        CHECK(bcn_decode_image(fmt, blocks, len, w, h, img) == 0, "decode format %d", fmt);
        uint64_t got = fnv(img, (size_t)w * h * 4);
        CHECK(got == refs[i].hash, "format %d hash %016llx want %016llx", fmt, (unsigned long long)got, (unsigned long long)refs[i].hash);
    }
    free(blocks);
    free(img);

    uint8_t out[64];
    uint8_t bc1[8] = {0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    bcn_decode_block(BCN_BC1, bc1, out);
    check_pixel(out, 255, 0, 0, 255, "BC1 solid red");
    check_pixel(out + 60, 255, 0, 0, 255, "BC1 solid red last pixel");

    uint8_t bc1t[8] = {0x00, 0x00, 0xFF, 0xFF, 0xE4, 0xE4, 0xE4, 0xE4};
    bcn_decode_block(BCN_BC1, bc1t, out);
    check_pixel(out, 0, 0, 0, 255, "BC1 3-colour index 0");
    check_pixel(out + 4, 255, 255, 255, 255, "BC1 3-colour index 1");
    check_pixel(out + 8, 127, 127, 127, 255, "BC1 3-colour index 2");
    check_pixel(out + 12, 0, 0, 0, 0, "BC1 3-colour index 3 is transparent");

    uint8_t bc2[16] = {0x0F, 0xF0, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    bcn_decode_block(BCN_BC2, bc2, out);
    check_pixel(out, 255, 0, 0, 255, "BC2 pixel 0 alpha 15");
    check_pixel(out + 4, 255, 0, 0, 0, "BC2 pixel 1 alpha 0");
    check_pixel(out + 8, 255, 0, 0, 0, "BC2 pixel 2 alpha 0");
    check_pixel(out + 12, 255, 0, 0, 255, "BC2 pixel 3 alpha 15");

    uint8_t bc3[16] = {200, 100, 0, 0, 0, 0, 0, 0, 0xE0, 0x07, 0xE0, 0x07, 0, 0, 0, 0};
    bcn_decode_block(BCN_BC3, bc3, out);
    check_pixel(out, 0, 255, 0, 200, "BC3 index 0 alpha a0");

    uint8_t bc4[8] = {0, 255, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    bcn_decode_block(BCN_BC4, bc4, out);
    check_pixel(out, 255, 0, 0, 255, "BC4 six-value mode index 7 is 255");
    bc4[2] = bc4[3] = bc4[4] = bc4[5] = bc4[6] = bc4[7] = 0xB6;
    bcn_decode_block(BCN_BC4, bc4, out);
    check_pixel(out, 0, 0, 0, 255, "BC4 six-value mode index 6 is 0");

    uint8_t bc5[16] = {90, 90, 0, 0, 0, 0, 0, 0, 30, 30, 0, 0, 0, 0, 0, 0};
    bcn_decode_block(BCN_BC5, bc5, out);
    check_pixel(out + 20, 90, 30, 0, 255, "BC5 red and green");

    uint8_t bc4s[8] = {0x7F, 0x81, 0, 0, 0, 0, 0, 0};
    bcn_decode_block(BCN_BC4S, bc4s, out);
    check_pixel(out, 127, 0, 0, 127, "BC4S index 0 is 127");
    bc4s[2] = 0x49; bc4s[3] = 0x92; bc4s[4] = 0x24; bc4s[5] = 0x49; bc4s[6] = 0x92; bc4s[7] = 0x24;
    bcn_decode_block(BCN_BC4S, bc4s, out);
    check_pixel(out, 129, 0, 0, 127, "BC4S index 1 is -127");
    bc4s[2] = 2; bc4s[3] = bc4s[4] = bc4s[5] = bc4s[6] = bc4s[7] = 0;
    bcn_decode_block(BCN_BC4S, bc4s, out);
    check_pixel(out, 91, 0, 0, 127, "BC4S index 2 interpolates to 91");
    bc4s[0] = 0x81; bc4s[1] = 0x7F; bc4s[2] = bc4s[3] = bc4s[4] = bc4s[5] = bc4s[6] = bc4s[7] = 0xFF;
    bcn_decode_block(BCN_BC4S, bc4s, out);
    check_pixel(out, 127, 0, 0, 127, "BC4S four-value mode index 7 is 127");
    bc4s[2] = 6;
    bcn_decode_block(BCN_BC4S, bc4s, out);
    check_pixel(out, 129, 0, 0, 127, "BC4S four-value mode index 6 is -127");
    bc4s[0] = bc4s[1] = 0x80; bc4s[2] = 0;
    bcn_decode_block(BCN_BC4S, bc4s, out);
    check_pixel(out, 129, 0, 0, 127, "BC4S -128 reads as -127");

    uint8_t bc5s[16] = {0x7F, 0x7F, 0, 0, 0, 0, 0, 0, 0x81, 0x81, 0, 0, 0, 0, 0, 0};
    bcn_decode_block(BCN_BC5S, bc5s, out);
    check_pixel(out + 20, 127, 129, 0, 127, "BC5S red and green");
    CHECK(bcn_block_bytes(BCN_BC4S) == 8 && bcn_block_bytes(BCN_BC5S) == 16 && bcn_is_signed(BCN_BC4S) && bcn_is_signed(BCN_BC5S) && !bcn_is_signed(BCN_BC4) && !bcn_is_signed(BCN_BC7), "signed format sizes and flags");

    uint8_t zero7[16] = {0};
    memset(out, 9, sizeof out);
    bcn_decode_block(BCN_BC7, zero7, out);
    check_pixel(out, 0, 0, 0, 0, "BC7 reserved mode is transparent black");

    enum { CW = 5, CH = 3, GUARD = 16 };
    uint8_t two[4][8];
    uint16_t cols[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    for (int i = 0; i < 4; i++) {
        memset(two[i], 0, 8);
        two[i][0] = two[i][2] = (uint8_t)cols[i];
        two[i][1] = two[i][3] = (uint8_t)(cols[i] >> 8);
    }
    uint8_t *crop = malloc((size_t)CW * CH * 4 + GUARD);
    memset(crop, 0x5A, (size_t)CW * CH * 4 + GUARD);
    CHECK(bcn_decode_image(BCN_BC1, (const uint8_t *)two, sizeof two, CW, CH, crop) == 0, "cropped decode");
    check_pixel(crop, 255, 0, 0, 255, "crop (0,0) from block 0");
    check_pixel(crop + 3 * 4, 255, 0, 0, 255, "crop (3,0) from block 0");
    check_pixel(crop + 4 * 4, 0, 255, 0, 255, "crop (4,0) from block 1");
    check_pixel(crop + ((size_t)2 * CW + 4) * 4, 0, 255, 0, 255, "crop (4,2) from block 1");
    for (int i = 0; i < GUARD; i++) CHECK(crop[CW * CH * 4 + i] == 0x5A, "write past the cropped image at guard byte %d", i);
    free(crop);

    uint8_t dummy[64];
    CHECK(bcn_decode_image(BCN_BC1, (const uint8_t *)two, 7, 8, 4, dummy) != 0, "short input rejected");
    CHECK(bcn_decode_image(99, (const uint8_t *)two, sizeof two, 4, 4, dummy) != 0, "unknown format rejected");
    CHECK(bcn_decode_image(BCN_BC1, (const uint8_t *)two, sizeof two, 0, 4, dummy) != 0, "zero width rejected");

    printf(fails ? "t_bcn: %d failure(s)\n" : "t_bcn: all passed\n", fails);
    return fails != 0;
}
