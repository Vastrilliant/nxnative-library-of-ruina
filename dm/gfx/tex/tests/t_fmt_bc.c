// BC → ASTC 4x4 mapping mode of nvn_fmt_to_mtl (pure C, no Metal): the table with the mode off is unchanged, and with it on exactly BC1 / BC2 / BC3 / BC7 and unsigned BC4 / BC5 move to ASTC 4x4, and signed BC4 / BC5 to R8Snorm / RG8Snorm.
#include <stdio.h>
#include <string.h>
#include "../nvn_fmt_mtl.h"
#include "../bcn_decode.h"

enum { MTL_R8_SNORM = 12, MTL_RG8_SNORM = 32, MTL_BC1 = 130, MTL_BC7_SRGB = 153, MTL_ASTC_SRGB_4x4 = 186, MTL_ASTC_LDR_4x4 = 204 };   // MTLPixelFormat values (as in nvn_fmt_mtl.c)

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    uint32_t off_m[0x9D]; uint8_t off_s[0x9D][4]; int off_k[0x9D];
    CHECK(!nvn_fmt_bc_to_astc_on(), "mode is on by default");
    for (int f = 0; f < 0x9D; f++) off_k[f] = nvn_fmt_to_mtl(f, &off_m[f], off_s[f]);

    // the BC formats this stage covers, with what each decodes as
    static const struct { int f, bcn, srgb, noalpha; } cov[] = {
        {0x41, BCN_BC1, 0, 1}, {0x42, BCN_BC1, 0, 0}, {0x43, BCN_BC2, 0, 0}, {0x44, BCN_BC3, 0, 0},
        {0x45, BCN_BC1, 1, 1}, {0x46, BCN_BC1, 1, 0}, {0x47, BCN_BC2, 1, 0}, {0x48, BCN_BC3, 1, 0},
        {0x4D, BCN_BC7, 0, 0}, {0x4E, BCN_BC7, 1, 0},
        {0x49, BCN_BC4, 0, 0}, {0x4B, BCN_BC5, 0, 0}, {0x4A, BCN_BC4S, 0, 0}, {0x4C, BCN_BC5S, 0, 0},   // unsigned RGTC: read as (r, 0, 0, 1) and (r, g, 0, 1), which the decoded blocks hold, so no remapping
    };
    for (unsigned i = 0; i < sizeof cov / sizeof *cov; i++) {
        int srgb = -1, b = nvn_fmt_bcn(cov[i].f, &srgb);
        CHECK(b == cov[i].bcn, "nvn_fmt_bcn(%#x) = %d, want %d", cov[i].f, b, cov[i].bcn);
        CHECK(srgb == cov[i].srgb, "nvn_fmt_bcn(%#x) srgb = %d, want %d", cov[i].f, srgb, cov[i].srgb);
        CHECK(off_k[cov[i].f] == 1 && off_m[cov[i].f] >= MTL_BC1 && off_m[cov[i].f] <= MTL_BC7_SRGB, "mode off: %#x did not stay a BC format (%u)", cov[i].f, off_m[cov[i].f]);
    }
    // every other format (BC6H included) is not covered, in either direction
    for (int f = 0; f < 0x9D + 8; f++) {
        int covered = 0; for (unsigned i = 0; i < sizeof cov / sizeof *cov; i++) covered |= cov[i].f == f;
        CHECK(!!nvn_fmt_bcn(f, NULL) == covered, "nvn_fmt_bcn(%#x) coverage wrong", f);
    }

    nvn_fmt_bc_to_astc(1);
    CHECK(nvn_fmt_bc_to_astc_on(), "mode did not turn on");
    for (int f = 0; f < 0x9D; f++) {
        uint32_t m = 99; uint8_t s[4]; int k = nvn_fmt_to_mtl(f, &m, s), covered = nvn_fmt_bcn(f, NULL);
        if (!covered) {   // unchanged, BC6H included
            CHECK(k == off_k[f] && m == off_m[f] && !memcmp(s, off_s[f], 4), "mode on changed uncovered format %#x", f);
            continue;
        }
        int srgb; nvn_fmt_bcn(f, &srgb);
        CHECK(k == 2, "%#x: kind %d, want 2 (substitute)", f, k);
        uint32_t want = covered == BCN_BC4S ? MTL_R8_SNORM : covered == BCN_BC5S ? MTL_RG8_SNORM : srgb ? MTL_ASTC_SRGB_4x4 : MTL_ASTC_LDR_4x4;
        CHECK(m == want, "%#x: Metal format %u, want %u", f, m, want);
        CHECK(s[0] == NVN_SWZ_R && s[1] == NVN_SWZ_G && s[2] == NVN_SWZ_B, "%#x: rgb remapped", f);
        CHECK(s[3] == (off_s[f][3]), "%#x: alpha remap differs from BC (%d vs %d)", f, s[3], off_s[f][3]);
    }
    CHECK(nvn_fmt_to_mtl(0x41, NULL, NULL) == 2 && nvn_fmt_to_mtl(0x4F, NULL, NULL) == 1, "NULL out-params or BC6H");   // BC6H stays BC
    { uint32_t m = 0; CHECK(nvn_fmt_to_mtl(0x4A, &m, NULL) == 2 && m == MTL_R8_SNORM, "signed BC4 did not become R8Snorm (%u)", m); CHECK(nvn_fmt_to_mtl(0x4C, &m, NULL) == 2 && m == MTL_RG8_SNORM, "signed BC5 did not become RG8Snorm (%u)", m); }

    // a GPU without BC (nvn_fmt_bc_native(0)): BC6H has no counterpart; the covered formats keep their ASTC / Snorm mapping; with the transcode off every BC format has none
    CHECK(nvn_fmt_bc_native_on(), "BC native is not on by default");
    nvn_fmt_bc_native(0);
    CHECK(!nvn_fmt_bc_native_on(), "BC native did not turn off");
    for (int f = 0; f < 0x9D; f++) {
        uint32_t m = 99; uint8_t s[4]; int k = nvn_fmt_to_mtl(f, &m, s), covered = nvn_fmt_bcn(f, NULL);
        int bc = f >= 0x41 && f <= 0x50;
        if (!bc || covered) continue;
        CHECK(k == 0 && m == 0, "no BC GPU: BC6H %#x gave kind %d format %u, want 0 / 0", f, k, m);
    }
    for (int f = 0; f < 0x9D; f++) {   // everything that is not BC is untouched, covered BC keeps the mapping checked above
        uint32_t m = 99; uint8_t s[4]; int k = nvn_fmt_to_mtl(f, &m, s);
        if (f >= 0x41 && f <= 0x50) { if (nvn_fmt_bcn(f, NULL)) CHECK(k == 2 && m != 0, "no BC GPU: covered %#x lost its mapping (%d, %u)", f, k, m); }
        else CHECK(k == off_k[f] && m == off_m[f] && !memcmp(s, off_s[f], 4), "no BC GPU changed non-BC format %#x", f);
    }
    CHECK(nvn_fmt_to_mtl(0x4F, NULL, NULL) == 0 && nvn_fmt_to_mtl(0x50, NULL, NULL) == 0, "no BC GPU: BC6H S / U not refused");
    nvn_fmt_bc_to_astc(0);
    for (int f = 0x41; f <= 0x50; f++) { uint32_t m = 99; CHECK(nvn_fmt_to_mtl(f, &m, NULL) == 0 && m == 0, "no BC GPU, transcode off: %#x still maps (%u)", f, m); }
    nvn_fmt_bc_native(1);
    CHECK(nvn_fmt_to_mtl(0x4F, NULL, NULL) == 1, "BC native back on: BC6H did not return to BC");

    nvn_fmt_bc_to_astc(0);
    for (int f = 0; f < 0x9D; f++) { uint32_t m; uint8_t s[4]; int k = nvn_fmt_to_mtl(f, &m, s); CHECK(k == off_k[f] && m == off_m[f] && !memcmp(s, off_s[f], 4), "mode off again: %#x differs", f); }

    if (fails) { printf("t_fmt_bc: %d failures\n", fails); return 1; }
    printf("t_fmt_bc: all passed\n");
    return 0;
}
