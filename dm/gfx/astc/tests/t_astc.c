#include "../astc_enc.h"

#include <astcenc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int decode(int srgb, const uint8_t *blocks, size_t len, unsigned w, unsigned h, uint8_t *rgba) {
    struct astcenc_config cfg;
    struct astcenc_context *ctx;
    if (astcenc_config_init(srgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR, 4, 4, 1, ASTCENC_PRE_FAST, ASTCENC_FLG_DECOMPRESS_ONLY, &cfg) != ASTCENC_SUCCESS) return -1;
    if (astcenc_context_alloc(&cfg, 1, &ctx, NULL) != ASTCENC_SUCCESS) return -1;
    void *slice = rgba;
    struct astcenc_image img = {w, h, 1, ASTCENC_TYPE_U8, &slice};
    struct astcenc_swizzle swz = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
    enum astcenc_error rc = astcenc_decompress_image(ctx, blocks, len, &img, &swz, 0);
    astcenc_context_free(ctx);
    return rc == ASTCENC_SUCCESS ? 0 : -1;
}

static double psnr(const uint8_t *a, const uint8_t *b, size_t n) {
    double se = 0;
    for (size_t i = 0; i < n; i++) { double d = (double)a[i] - b[i]; se += d * d; }
    if (se == 0) return 99;
    return 10 * log10(255.0 * 255.0 / (se / n));
}

static void roundtrip(int srgb, unsigned w, unsigned h, double min_psnr, int solid) {
    uint8_t *src = malloc((size_t)w * h * 4), *dec = malloc((size_t)w * h * 4);
    size_t blen = astc_enc_size_4x4(w, h);
    uint8_t *blk = malloc(blen);
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++) {
            uint8_t *p = src + ((size_t)y * w + x) * 4;
            if (solid) { p[0] = 200; p[1] = 100; p[2] = 30; p[3] = 255; }
            else { p[0] = (uint8_t)(x * 255 / (w > 1 ? w - 1 : 1)); p[1] = (uint8_t)(y * 255 / (h > 1 ? h - 1 : 1)); p[2] = (uint8_t)((x + y) * 3); p[3] = 255; }
        }
    astc_enc_t *e = astc_enc_new(srgb, ASTC_ENC_QUALITY_FAST, NULL);
    CHECK(e != NULL, "encoder create %ux%u", w, h);
    if (e) {
        memset(blk, 0xAA, blen);
        CHECK(astc_enc_rgba8_4x4(e, src, w, h, blk, blen) == 0, "encode %ux%u srgb %d", w, h, srgb);
        CHECK(decode(srgb, blk, blen, w, h, dec) == 0, "decode %ux%u", w, h);
        double q = psnr(src, dec, (size_t)w * h * 4);
        CHECK(q >= min_psnr, "%ux%u srgb %d solid %d psnr %.1f < %.1f", w, h, srgb, solid, q, min_psnr);
        printf("ok %ux%u srgb %d solid %d psnr %.1f\n", w, h, srgb, solid, q);
        astc_enc_free(e);
    }
    free(src); free(dec); free(blk);
}

int main(void) {
    CHECK(astc_enc_size_4x4(4, 4) == 16, "size 4x4");
    CHECK(astc_enc_size_4x4(5, 5) == 64, "size 5x5");
    CHECK(astc_enc_size_4x4(1, 1) == 16, "size 1x1");
    CHECK(astc_enc_size_4x4(256, 128) == 64 * 32 * 16, "size 256x128");

    roundtrip(0, 4, 4, 40, 1);
    roundtrip(1, 4, 4, 40, 1);
    roundtrip(0, 64, 64, 35, 0);
    roundtrip(1, 64, 64, 30, 0);
    roundtrip(0, 37, 21, 30, 0);
    roundtrip(0, 1, 1, 40, 1);

    astc_enc_t *root = astc_enc_new(0, ASTC_ENC_QUALITY_FAST, NULL);
    astc_enc_t *child = astc_enc_new(0, ASTC_ENC_QUALITY_FAST, root);
    CHECK(root && child, "parent and child contexts");
    uint8_t px[4 * 4 * 4], a[16], b[16];
    memset(px, 77, sizeof px);
    CHECK(astc_enc_rgba8_4x4(root, px, 4, 4, a, sizeof a) == 0, "root encode");
    CHECK(astc_enc_rgba8_4x4(child, px, 4, 4, b, sizeof b) == 0, "child encode");
    CHECK(memcmp(a, b, 16) == 0, "root and child give the same blocks");
    CHECK(astc_enc_rgba8_4x4(root, px, 4, 4, a, 15) != 0, "short output buffer rejected");
    CHECK(astc_enc_rgba8_4x4(root, px, 0, 4, a, sizeof a) != 0, "zero width rejected");
    CHECK(astc_enc_rgba8_4x4(NULL, px, 4, 4, a, sizeof a) != 0, "null encoder rejected");
    astc_enc_free(child);
    astc_enc_free(root);
    astc_enc_free(NULL);

    printf(fails ? "t_astc: %d failure(s)\n" : "t_astc: all passed\n", fails);
    return fails != 0;
}
