// BC → ASTC 4x4 transcode (nvn_bc_astc.c), without Metal: for every covered NVN format, BC blocks go through nvn_bc_astc_image, the ASTC result is decoded with astcenc and compared with bcn_decode's
// decode of the same blocks (PSNR); also the switch, sizes, odd image sizes, threads. Needs astcenc: make run ASTCENC_DIR=<dir with include/astcenc.h and lib/libastcenc.a>
#include <astcenc.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../bcn_decode.h"
#include "../nvn_bc_astc.h"
#include "../nvn_fmt_mtl.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint64_t rng = 0x243F6A8885A308D3ull;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 16); }

static int astc_decode(int srgb, const uint8_t *blocks, size_t len, unsigned w, unsigned h, uint8_t *rgba) {
    struct astcenc_config cfg; struct astcenc_context *ctx;
    if (astcenc_config_init(srgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR, 4, 4, 1, ASTCENC_PRE_FAST, ASTCENC_FLG_DECOMPRESS_ONLY, &cfg) != ASTCENC_SUCCESS) return -1;
    if (astcenc_context_alloc(&cfg, 1, &ctx, NULL) != ASTCENC_SUCCESS) return -1;
    void *slice = rgba; struct astcenc_image img = {w, h, 1, ASTCENC_TYPE_U8, &slice};
    struct astcenc_swizzle swz = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
    enum astcenc_error rc = astcenc_decompress_image(ctx, blocks, len, &img, &swz, 0);
    astcenc_context_free(ctx);
    return rc == ASTCENC_SUCCESS ? 0 : -1;
}
static double psnr(const uint8_t *a, const uint8_t *b, size_t n) {
    double se = 0; for (size_t i = 0; i < n; i++) { double d = (double)a[i] - b[i]; se += d * d; }
    return se == 0 ? 99 : 10 * log10(255.0 * 255.0 / (se / n));
}

// Random blocks of a kind a 4x4 ASTC block can follow closely: BC1 colours on one line (c0 > c1: four colours), BC2 / BC3 the same with their alpha, BC7 mode 6 (one subset, two RGBA endpoints, 16 steps)
static void make_blocks(int fmt, uint8_t *b, size_t nblk) {
    int bb = bcn_block_bytes(fmt);
    for (size_t i = 0; i < nblk; i++, b += bb) {
        for (int k = 0; k < bb; k++) b[k] = (uint8_t)rnd();
        if (fmt == BCN_BC1 || fmt == BCN_BC2 || fmt == BCN_BC3) {
            uint8_t *c = b + (fmt == BCN_BC1 ? 0 : 8);   // the colour half: c0 > c1 as 16-bit values
            uint16_t c0 = (uint16_t)(c[0] | c[1] << 8), c1 = (uint16_t)(c[2] | c[3] << 8);
            if (c0 == c1) c1 = (uint16_t)(c0 ^ 0x0841);
            if (c0 < c1) { uint16_t t = c0; c0 = c1; c1 = t; }
            c[0] = (uint8_t)c0; c[1] = (uint8_t)(c0 >> 8); c[2] = (uint8_t)c1; c[3] = (uint8_t)(c1 >> 8);
        }
        if (fmt == BCN_BC7) b[0] = (uint8_t)((b[0] & 0x80) | 0x40);   // mode 6: six zero bits, then a one
    }
}

typedef struct { uint8_t blk[8 * 64]; int ok; } thr_t;
static void *thread_main(void *p) {   // two threads encoding at once, each with its own encoder (the blocks are made before, by one thread: the generator is shared)
    thr_t *t = p; uint8_t out[16 * 64];
    t->ok = nvn_bc_astc_image(0x42, t->blk, 8 * 64, 32, 32, out, sizeof out) == 0;
    return NULL;
}

// Large images (128×128 and up) are spread over DM_NVN_BC_ASTC_THREADS threads: the result must be the one a single thread makes, also with several large images at once (the second one finds the shared encoder taken and uses its own thread)
#define BIG_W 256
#define BIG_H 192
typedef struct { int nvn, bcn; uint8_t *bc, *out, *ref; int ok; } big_t;
static void *big_main(void *p) {
    big_t *b = p; size_t n = (size_t)BIG_W / 4 * (BIG_H / 4) * bcn_block_bytes(b->bcn);
    b->ok = nvn_bc_astc_image(b->nvn, b->bc, n, BIG_W, BIG_H, b->out, nvn_bc_astc_size(BIG_W, BIG_H)) == 0 && !memcmp(b->out, b->ref, nvn_bc_astc_size(BIG_W, BIG_H));
    return NULL;
}
static void test_threads(void) {
    static const struct { int nvn, bcn; } k[] = {{0x44, BCN_BC3}, {0x4E, BCN_BC7}, {0x4B, BCN_BC5}, {0x42, BCN_BC1}};
    size_t asz = nvn_bc_astc_size(BIG_W, BIG_H);
    big_t b[4];
    setenv("DM_NVN_BC_ASTC_THREADS", "1", 1); nvn_bc_astc_init();
    CHECK(nvn_bc_astc_threads() == 1, "DM_NVN_BC_ASTC_THREADS=1 gave %u", nvn_bc_astc_threads());
    for (int i = 0; i < 4; i++) {   // the single-thread results are the reference
        size_t n = (size_t)BIG_W / 4 * (BIG_H / 4) * bcn_block_bytes(k[i].bcn);
        b[i] = (big_t){k[i].nvn, k[i].bcn, malloc(n), malloc(asz), malloc(asz), 0};
        make_blocks(k[i].bcn, b[i].bc, (size_t)BIG_W / 4 * (BIG_H / 4));
        CHECK(nvn_bc_astc_image(k[i].nvn, b[i].bc, n, BIG_W, BIG_H, b[i].ref, asz) == 0, "single-thread reference %#x failed", k[i].nvn);
    }
    setenv("DM_NVN_BC_ASTC_THREADS", "0", 1); nvn_bc_astc_init(); CHECK(nvn_bc_astc_threads() == 1, "DM_NVN_BC_ASTC_THREADS=0 is not one thread");
    setenv("DM_NVN_BC_ASTC_THREADS", "999", 1); nvn_bc_astc_init(); CHECK(nvn_bc_astc_threads() == 64, "DM_NVN_BC_ASTC_THREADS=999 is %u, want 64", nvn_bc_astc_threads());
    unsetenv("DM_NVN_BC_ASTC_THREADS"); nvn_bc_astc_init(); CHECK(nvn_bc_astc_threads() >= 1 && nvn_bc_astc_threads() <= 6, "default is %u threads", nvn_bc_astc_threads());
    static const char *counts[] = {"3", "64"};   // 64: more threads than blocks in a band, and than CPUs
    for (int c = 0; c < 2; c++) {
        setenv("DM_NVN_BC_ASTC_THREADS", counts[c], 1); nvn_bc_astc_init();
        for (int i = 0; i < 4; i++) {   // one image at a time
            memset(b[i].out, 0, asz); big_main(&b[i]);
            CHECK(b[i].ok, "%s threads: format %#x differs from the single-thread result", counts[c], k[i].nvn);
        }
        pthread_t th[4];   // all at once
        for (int i = 0; i < 4; i++) { memset(b[i].out, 0, asz); pthread_create(&th[i], NULL, big_main, &b[i]); }
        for (int i = 0; i < 4; i++) { pthread_join(th[i], NULL); CHECK(b[i].ok, "%s threads, concurrent: format %#x differs from the single-thread result", counts[c], k[i].nvn); }
    }
    for (int i = 0; i < 4; i++) { free(b[i].bc); free(b[i].out); free(b[i].ref); }
    unsetenv("DM_NVN_BC_ASTC_THREADS"); nvn_bc_astc_init();
}

int main(void) {
    unsetenv("DM_NVN_BC_ASTC_THREADS");
    // the switch: default on with astcenc, DM_NVN_BC_ASTC=0 off; the format mapping follows
    unsetenv("DM_NVN_BC_ASTC");
    CHECK(nvn_bc_astc_init() == 1 && nvn_bc_astc_active() && nvn_fmt_bc_to_astc_on(), "default is not on");
    CHECK(nvn_bc_astc_covers(0x42) && nvn_bc_astc_covers(0x4E) && nvn_bc_astc_covers(0x49) && nvn_bc_astc_covers(0x4B) && nvn_bc_astc_covers(0x4A) && nvn_bc_astc_covers(0x4C) && !nvn_bc_astc_covers(0x4F) && !nvn_bc_astc_covers(0x25), "coverage with the switch on");
    setenv("DM_NVN_BC_ASTC", "0", 1);
    CHECK(nvn_bc_astc_init() == 0 && !nvn_bc_astc_active() && !nvn_fmt_bc_to_astc_on() && !nvn_bc_astc_covers(0x42), "DM_NVN_BC_ASTC=0 did not turn it off");
    setenv("DM_NVN_BC_ASTC", "1", 1);
    CHECK(nvn_bc_astc_init() == 1, "DM_NVN_BC_ASTC=1 did not turn it on");

    CHECK(nvn_bc_astc_size(1, 1) == 16 && nvn_bc_astc_size(4, 4) == 16 && nvn_bc_astc_size(5, 4) == 32 && nvn_bc_astc_size(37, 21) == 10 * 6 * 16, "sizes");

    static const struct { int nvn, bcn, srgb; double min_psnr; } f[] = {   // minimum PSNR between the BC decode and the ASTC decode, by what ASTC can follow; measured, with room
        {0x41, BCN_BC1, 0, 40}, {0x42, BCN_BC1, 0, 40}, {0x45, BCN_BC1, 1, 40}, {0x46, BCN_BC1, 1, 40},
        {0x43, BCN_BC2, 0, 25}, {0x47, BCN_BC2, 1, 25}, {0x44, BCN_BC3, 0, 30}, {0x48, BCN_BC3, 1, 30},
        {0x4D, BCN_BC7, 0, 35}, {0x4E, BCN_BC7, 1, 35},
        {0x49, BCN_BC4, 0, 45}, {0x4B, BCN_BC5, 0, 28},   // random blocks are the worst case for these two (measured: BC4 48, BC5 32); smooth data, which real textures are, does better
    };
    static const unsigned sz[][2] = {{4, 4}, {64, 64}, {37, 21}, {1, 1}, {2, 2}, {128, 8}};
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) for (unsigned s = 0; s < sizeof sz / sizeof *sz; s++) {
        unsigned w = sz[s][0], h = sz[s][1], nb = (w + 3) / 4 * ((h + 3) / 4); int bb = bcn_block_bytes(f[i].bcn);
        uint8_t *bc = malloc((size_t)nb * bb), *astc = malloc(nvn_bc_astc_size(w, h)), *ref = malloc((size_t)w * h * 4), *got = malloc((size_t)w * h * 4);
        make_blocks(f[i].bcn, bc, nb);
        memset(astc, 0xAB, nvn_bc_astc_size(w, h));
        int rc = nvn_bc_astc_image(f[i].nvn, bc, (size_t)nb * bb, w, h, astc, nvn_bc_astc_size(w, h));
        CHECK(rc == 0, "format %#x %ux%u: transcode failed", f[i].nvn, w, h);
        if (rc == 0) {
            CHECK(!bcn_decode_image(f[i].bcn, bc, (size_t)nb * bb, w, h, ref) && !astc_decode(f[i].srgb, astc, nvn_bc_astc_size(w, h), w, h, got), "decode failed");
            double p = psnr(ref, got, (size_t)w * h * 4);
            if (w >= 64 || (w == 37)) printf("format %#x %ux%u psnr %.1f\n", f[i].nvn, w, h, p);
            if (f[i].bcn == BCN_BC4 || f[i].bcn == BCN_BC5) {   // the channels BC4 / BC5 do not have must still read as 0 (g for BC4, b) and 1 (a): the texture is sampled without a swizzle
                int dev = 0;
                for (size_t k = 0; k < (size_t)w * h; k++) {
                    int c0 = f[i].bcn == BCN_BC4 ? got[k * 4 + 1] : 0, c1 = got[k * 4 + 2], c2 = 255 - got[k * 4 + 3];
                    dev = dev > c0 ? dev : c0; dev = dev > c1 ? dev : c1; dev = dev > c2 ? dev : c2;
                }
                CHECK(dev <= 3, "format %#x %ux%u: a constant channel is off by %d", f[i].nvn, w, h, dev);
            }
            CHECK(p >= f[i].min_psnr, "format %#x %ux%u: psnr %.1f below %.0f", f[i].nvn, w, h, p, f[i].min_psnr);
        }
        free(bc); free(astc); free(ref); free(got);
    }
    // refusals: uncovered formats, short buffers, no switch
    uint8_t blk[16] = {0}, out[16];
    {
        int bw, bh, bpb;
        CHECK(nvn_bc_astc_host(0x42, &bw, &bh, &bpb) && bw == 4 && bh == 4 && bpb == 16, "BC1 host block");
        CHECK(nvn_bc_astc_host(0x4A, &bw, &bh, &bpb) && bw == 1 && bh == 1 && bpb == 1, "signed BC4 host block");
        CHECK(nvn_bc_astc_host(0x4C, &bw, &bh, &bpb) && bw == 1 && bh == 1 && bpb == 2, "signed BC5 host block");
        CHECK(!nvn_bc_astc_host(0x4F, &bw, &bh, &bpb) && !nvn_bc_astc_host(0x25, &bw, &bh, &bpb), "uncovered host block");
        CHECK(nvn_bc_astc_host_size(0x4A, 37, 21) == 37 * 21 && nvn_bc_astc_host_size(0x4C, 37, 21) == 37 * 21 * 2 && nvn_bc_astc_host_size(0x42, 37, 21) == nvn_bc_astc_size(37, 21) && nvn_bc_astc_host_size(0x4F, 4, 4) == 0, "host sizes");
    }
    for (int k = 0; k < 2; k++) for (unsigned s = 0; s < sizeof sz / sizeof *sz; s++) {
        int nvn = k ? 0x4C : 0x4A, bcn = k ? BCN_BC5S : BCN_BC4S; unsigned w = sz[s][0], h = sz[s][1], nb = (w + 3) / 4 * ((h + 3) / 4), n = (unsigned)k + 1; int bb = bcn_block_bytes(bcn);
        uint8_t *bc = malloc((size_t)nb * bb), *got = malloc((size_t)w * h * n), *ref = malloc((size_t)w * h * 4);
        make_blocks(bcn, bc, nb);
        CHECK(nvn_bc_astc_image(nvn, bc, (size_t)nb * bb, w, h, got, (size_t)w * h * n) == 0 && !bcn_decode_image(bcn, bc, (size_t)nb * bb, w, h, ref), "signed format %#x %ux%u: transcode failed", nvn, w, h);
        int bad = 0;
        for (size_t i = 0; i < (size_t)w * h; i++) for (unsigned c = 0; c < n; c++) bad += got[i * n + c] != ref[i * 4 + c];
        CHECK(!bad, "signed format %#x %ux%u: %d bytes differ from the decode", nvn, w, h, bad);
        CHECK(nvn_bc_astc_image(nvn, bc, (size_t)nb * bb, w, h, got, (size_t)w * h * n - 1) == -1, "signed format %#x: short destination accepted", nvn);
        free(bc); free(got); free(ref);
    }
    CHECK(nvn_bc_astc_image(0x4F, blk, 16, 4, 4, out, 16) == -1, "BC6H accepted");
    CHECK(nvn_bc_astc_image(0x42, blk, 4, 4, 4, out, 16) == -1, "short source accepted");
    CHECK(nvn_bc_astc_image(0x42, blk, 8, 4, 4, out, 8) == -1, "short destination accepted");
    CHECK(nvn_bc_astc_image(0x42, blk, 8, 0, 4, out, 16) == -1, "zero width accepted");
    // threads
    pthread_t th[2]; thr_t tr[2] = {{{0}, 0}, {{0}, 0}};
    for (int i = 0; i < 2; i++) make_blocks(BCN_BC1, tr[i].blk, 64);
    for (int i = 0; i < 2; i++) pthread_create(&th[i], NULL, thread_main, &tr[i]);
    for (int i = 0; i < 2; i++) { pthread_join(th[i], NULL); CHECK(tr[i].ok, "thread %d failed", i); }

    test_threads();

    if (fails) { printf("t_bc_astc: %d failures\n", fails); return 1; }
    printf("t_bc_astc: all passed\n");
    return 0;
}
