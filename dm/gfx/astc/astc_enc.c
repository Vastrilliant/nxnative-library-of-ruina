#include "astc_enc.h"

#include <astcenc.h>
#include <pthread.h>
#include <stdlib.h>

struct astc_enc {
    struct astcenc_context *ctx;
    unsigned threads;
};

static astc_enc_t *enc_new(int srgb, float quality, const astc_enc_t *parent, unsigned threads) {
    struct astcenc_config cfg;
    struct astcenc_config *cfgp = NULL;
    if (!parent) {
        enum astcenc_profile prof = srgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR;
        if (astcenc_config_init(prof, 4, 4, 1, quality, 0, &cfg) != ASTCENC_SUCCESS) return NULL;
        cfgp = &cfg;
    }
    astc_enc_t *e = calloc(1, sizeof *e);
    if (!e) return NULL;
    if (astcenc_context_alloc(cfgp, threads, &e->ctx, parent ? parent->ctx : NULL) != ASTCENC_SUCCESS) {
        free(e);
        return NULL;
    }
    e->threads = threads;
    return e;
}

astc_enc_t *astc_enc_new(int srgb, float quality, const astc_enc_t *parent) { return enc_new(srgb, quality, parent, 1); }
astc_enc_t *astc_enc_new_mt(int srgb, float quality, unsigned threads) { return threads < 2 ? NULL : enc_new(srgb, quality, NULL, threads > 64 ? 64 : threads); }

size_t astc_enc_size_4x4(unsigned w, unsigned h) {
    return (size_t)((w + 3) / 4) * ((h + 3) / 4) * 16;
}

int astc_enc_rgba8_4x4(astc_enc_t *e, const uint8_t *rgba, unsigned w, unsigned h, uint8_t *out, size_t out_len) {
    if (!e || !rgba || !out || !w || !h || out_len < astc_enc_size_4x4(w, h)) return -1;
    void *slice = (void *)(uintptr_t)rgba;
    struct astcenc_image img = {w, h, 1, ASTCENC_TYPE_U8, &slice};
    struct astcenc_swizzle swz = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
    astcenc_compress_reset(e->ctx);
    enum astcenc_error rc = astcenc_compress_image(e->ctx, &img, &swz, out, astc_enc_size_4x4(w, h), 0);
    return rc == ASTCENC_SUCCESS ? 0 : -1;
}

typedef struct { astc_enc_t *e; struct astcenc_image *img; const struct astcenc_swizzle *swz; uint8_t *out; size_t len; unsigned index; int rc; } job_t;
static void *job_main(void *p) {
    job_t *j = p;
    j->rc = astcenc_compress_image(j->e->ctx, j->img, j->swz, j->out, j->len, j->index) == ASTCENC_SUCCESS ? 0 : -1;
    return NULL;
}

// Every thread calls astcenc_compress_image with its own index and takes blocks until none are left; the image is complete when all calls have returned
int astc_enc_rgba8_4x4_mt(astc_enc_t *e, const uint8_t *rgba, unsigned w, unsigned h, uint8_t *out, size_t out_len) {
    if (!e || e->threads < 2 || !rgba || !out || !w || !h || out_len < astc_enc_size_4x4(w, h)) return -1;
    void *slice = (void *)(uintptr_t)rgba;
    struct astcenc_image img = {w, h, 1, ASTCENC_TYPE_U8, &slice};
    struct astcenc_swizzle swz = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
    job_t jobs[64]; pthread_t th[64]; int started[64] = {0};
    unsigned n = e->threads;   // at most 64 (astc_enc_new_mt)
    astcenc_compress_reset(e->ctx);
    for (unsigned i = 0; i < n; i++) jobs[i] = (job_t){e, &img, &swz, out, astc_enc_size_4x4(w, h), i, -1};
    for (unsigned i = 1; i < n; i++) started[i] = pthread_create(&th[i], NULL, job_main, &jobs[i]) == 0;
    job_main(&jobs[0]);
    int rc = jobs[0].rc;
    for (unsigned i = 1; i < n; i++) {
        if (!started[i]) continue;   // a helper that could not start is not an error: astcenc hands out the blocks to whoever asks, so the threads that did start finish the image
        pthread_join(th[i], NULL);
        if (jobs[i].rc) rc = -1;
    }
    return rc;
}

void astc_enc_free(astc_enc_t *e) {
    if (!e) return;
    astcenc_context_free(e->ctx);
    free(e);
}
