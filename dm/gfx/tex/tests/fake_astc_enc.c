// A stand-in for ../../astc/astc_enc.c (which needs astcenc) for t_astc_cache: the same API, a deterministic fake "encoding" of the RGBA bytes, and a count of the calls.
// Lets nvn_bc_astc.c be built with DM_HAVE_ASTCENC and its cache path tested without astcenc.
#include <stdatomic.h>
#include <stdlib.h>
#include "../../astc/astc_enc.h"

atomic_int fake_encodes;
struct astc_enc { int srgb; };

astc_enc_t *astc_enc_new(int srgb, float quality, const astc_enc_t *parent) { (void)quality; (void)parent; astc_enc_t *e = malloc(sizeof *e); if (e) e->srgb = srgb; return e; }
astc_enc_t *astc_enc_new_mt(int srgb, float quality, unsigned threads) { (void)threads; return astc_enc_new(srgb, quality, NULL); }
size_t astc_enc_size_4x4(unsigned w, unsigned h) { return (size_t)((w + 3) / 4) * ((h + 3) / 4) * 16; }
int astc_enc_rgba8_4x4(astc_enc_t *e, const uint8_t *rgba, unsigned w, unsigned h, uint8_t *out, size_t out_len) {
    size_t n = astc_enc_size_4x4(w, h), px = (size_t)w * h * 4;
    if (out_len < n) return -1;
    for (size_t i = 0; i < n; i++) out[i] = (uint8_t)(rgba[(i * 7) % px] ^ (uint8_t)i ^ (e->srgb ? 0x55 : 0));
    atomic_fetch_add(&fake_encodes, 1);
    return 0;
}
int astc_enc_rgba8_4x4_mt(astc_enc_t *e, const uint8_t *rgba, unsigned w, unsigned h, uint8_t *out, size_t out_len) { return astc_enc_rgba8_4x4(e, rgba, w, h, out, out_len); }
void astc_enc_free(astc_enc_t *e) { free(e); }
