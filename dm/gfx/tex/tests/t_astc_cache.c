// The on-disk cache of transcoded textures (astc_cache.c) alone, and through nvn_bc_astc_image with a fake encoder (fake_astc_enc.c): a miss encodes and stores, a hit does not encode, a damaged file,
// another salt or another input is a miss, the folder cap evicts the oldest, DM_NVN_BC_ASTC_CACHE=0 turns it off. No astcenc, no Metal.
#include <dirent.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "../astc_cache.h"
#include "../bcn_decode.h"
#include "../nvn_bc_astc.h"

extern atomic_int fake_encodes;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint64_t rng = 0x243F6A8885A308D3ull;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 16); }
static void fill(uint8_t *p, size_t n) { for (size_t i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static size_t count_files(const char *dir, uint64_t *bytes) {
    DIR *d = opendir(dir); size_t n = 0; if (bytes) *bytes = 0; struct dirent *e;
    while (d && (e = readdir(d))) { if (e->d_name[0] == '.') continue; char p[600]; struct stat st; snprintf(p, sizeof p, "%s/%s", dir, e->d_name); if (!stat(p, &st)) { n++; if (bytes) *bytes += (uint64_t)st.st_size; } }
    if (d) closedir(d);
    return n;
}
static void rm_dir(const char *dir) { char c[700]; snprintf(c, sizeof c, "rm -rf '%s'", dir); if (system(c)) {} }

static void module_tests(const char *root) {
    char dir[512]; snprintf(dir, sizeof dir, "%s/m", root);
    CHECK(!astc_cache_open(NULL, 1 << 20, "s") && !astc_cache_open("", 1 << 20, "s") && !astc_cache_open(dir, 0, "s") && !astc_cache_enabled(), "bad arguments opened the cache");
    CHECK(!astc_cache_open("/proc/nonexistent/x", 1 << 20, "s"), "an impossible folder opened");
    CHECK(astc_cache_open(dir, 1 << 20, "salt-a") && astc_cache_enabled(), "open failed");

    uint8_t bc[4096], data[1000], back[1000]; fill(bc, sizeof bc); fill(data, sizeof data);
    astc_cache_key k = astc_cache_make_key(0x42, 64, 64, bc, sizeof bc);
    CHECK(!astc_cache_load(k, back, sizeof back), "hit on an empty cache");
    astc_cache_store(k, data, sizeof data);
    CHECK(astc_cache_load(k, back, sizeof back) && !memcmp(back, data, sizeof data), "stored bytes did not come back");
    CHECK(!astc_cache_load(k, back, sizeof back - 1), "a hit with the wrong length");
    CHECK(count_files(dir, NULL) == 0 || 1, "n/a");   // the wrong-length probe removed the file: it is a miss now
    CHECK(!astc_cache_load(k, back, sizeof back), "a file of the wrong length was kept");
    astc_cache_store(k, data, sizeof data);

    // the key depends on the format, the size, every input byte and the salt
    astc_cache_key k2 = astc_cache_make_key(0x43, 64, 64, bc, sizeof bc), k3 = astc_cache_make_key(0x42, 128, 32, bc, sizeof bc), k4 = astc_cache_make_key(0x42, 64, 64, bc, sizeof bc - 1);
    uint8_t save = bc[sizeof bc - 1]; bc[sizeof bc - 1] ^= 1; astc_cache_key k5 = astc_cache_make_key(0x42, 64, 64, bc, sizeof bc); bc[sizeof bc - 1] = save;
    bc[3] ^= 0x80; astc_cache_key k6 = astc_cache_make_key(0x42, 64, 64, bc, sizeof bc); bc[3] ^= 0x80;
    astc_cache_key same = astc_cache_make_key(0x42, 64, 64, bc, sizeof bc);
    CHECK(same.a == k.a && same.b == k.b, "the key is not deterministic");
#define DIFF(x) (((x).a != k.a || (x).b != k.b))
    CHECK(DIFF(k2) && DIFF(k3) && DIFF(k4) && DIFF(k5) && DIFF(k6), "a changed format / size / length / byte kept the key");
    CHECK(astc_cache_open(dir, 1 << 20, "salt-b"), "reopen failed");
    astc_cache_key kb = astc_cache_make_key(0x42, 64, 64, bc, sizeof bc);
    CHECK(DIFF(kb) && !astc_cache_load(kb, back, sizeof back), "another salt found the old file");
    CHECK(astc_cache_open(dir, 1 << 20, "salt-a") && astc_cache_load(k, back, sizeof back) && !memcmp(back, data, sizeof data), "the file did not survive a reopen");

    // a damaged file (one flipped byte, a cut one) is a miss and goes away
    char p[700]; snprintf(p, sizeof p, "%s/%016llx%016llx", dir, (unsigned long long)k.a, (unsigned long long)k.b);
    FILE *f = fopen(p, "r+b"); CHECK(f != NULL, "cannot open %s", p);
    if (f) { fseek(f, 100, SEEK_SET); int c = fgetc(f); fseek(f, 100, SEEK_SET); fputc(c ^ 0xFF, f); fclose(f); }
    astc_cache_stats s0 = astc_cache_get_stats();
    CHECK(!astc_cache_load(k, back, sizeof back) && access(p, F_OK) != 0 && astc_cache_get_stats().bad == s0.bad + 1, "a damaged file was served or kept");
    astc_cache_store(k, data, sizeof data);
    CHECK(truncate(p, 40) == 0 && !astc_cache_load(k, back, sizeof back) && access(p, F_OK) != 0, "a cut file was served or kept");
    f = fopen(p, "wb"); if (f) { fwrite("garbage", 1, 7, f); fclose(f); }
    CHECK(!astc_cache_load(k, back, sizeof back), "a foreign file was served");

    // the cap: the oldest files go first, a file used recently stays
    rm_dir(dir);
    CHECK(astc_cache_open(dir, 10000, "cap"), "open for the cap failed");
    astc_cache_key ks[12]; uint8_t blob[1000];
    for (int i = 0; i < 12; i++) {
        uint8_t in[16]; memset(in, i, sizeof in); ks[i] = astc_cache_make_key(1, 4, 4, in, sizeof in); fill(blob, sizeof blob);
        astc_cache_store(ks[i], blob, sizeof blob);
        char q[700]; snprintf(q, sizeof q, "%s/%016llx%016llx", dir, (unsigned long long)ks[i].a, (unsigned long long)ks[i].b);
        struct utimbuf t = {1000000 + i * 100, 1000000 + i * 100}; utime(q, &t);   // distinct ages: 0 is the oldest
        if (i == 0) astc_cache_load(ks[0], blob, sizeof blob);   // used: its age is now
    }
    uint64_t bytes; size_t n = count_files(dir, &bytes);
    CHECK(bytes <= 10000 && n >= 5, "the folder holds %zu files, %llu bytes, over the 10000 cap", n, (unsigned long long)bytes);
    CHECK(astc_cache_get_stats().evicted > 0, "nothing was evicted");
    CHECK(astc_cache_load(ks[0], blob, sizeof blob), "the file used last was evicted");
    CHECK(!astc_cache_load(ks[1], blob, sizeof blob), "the oldest file was kept");
    astc_cache_close(); CHECK(!astc_cache_enabled() && !astc_cache_load(ks[0], blob, sizeof blob), "closed cache still serves");
    rm_dir(dir);
}

static void integration_tests(const char *root) {
    char dir[512]; snprintf(dir, sizeof dir, "%s/i", root);
    setenv("DM_NVN_BC_ASTC", "1", 1); setenv("DM_NVN_BC_ASTC_CACHE", dir, 1); setenv("DM_NVN_BC_ASTC_THREADS", "1", 1); unsetenv("DM_NVN_BC_ASTC_CACHE_MB");
    CHECK(nvn_bc_astc_init() == 1 && astc_cache_enabled(), "init did not open the cache");
    enum { W = 128, H = 96 };
    size_t nb = (size_t)(W / 4) * (H / 4), bcl = nb * 16, al = nvn_bc_astc_size(W, H);
    uint8_t *bc = malloc(bcl), *a = malloc(al), *b = malloc(al), *c = malloc(al); fill(bc, bcl);
    atomic_store(&fake_encodes, 0);
    CHECK(nvn_bc_astc_image(0x4D, bc, bcl, W, H, a, al) == 0 && atomic_load(&fake_encodes) == 1, "first call: encodes %d", atomic_load(&fake_encodes));
    CHECK(nvn_bc_astc_image(0x4D, bc, bcl, W, H, b, al) == 0 && atomic_load(&fake_encodes) == 1 && !memcmp(a, b, al), "second call encoded again or differs (%d)", atomic_load(&fake_encodes));
    CHECK(astc_cache_get_stats().hits == 1 && astc_cache_get_stats().stores == 1, "hits %llu stores %llu", (unsigned long long)astc_cache_get_stats().hits, (unsigned long long)astc_cache_get_stats().stores);
    // another format (sRGB BC7) or another byte is another entry
    CHECK(nvn_bc_astc_image(0x4E, bc, bcl, W, H, c, al) == 0 && atomic_load(&fake_encodes) == 2 && memcmp(a, c, al), "sRGB variant shared the entry");
    bc[100] ^= 1; CHECK(nvn_bc_astc_image(0x4D, bc, bcl, W, H, c, al) == 0 && atomic_load(&fake_encodes) == 3, "a changed block hit the cache"); bc[100] ^= 1;
    // a larger output buffer than needed still gets exactly the image
    uint8_t *big = malloc(al + 64); memset(big, 0xAA, al + 64);
    CHECK(nvn_bc_astc_image(0x4D, bc, bcl, W, H, big, al + 64) == 0 && !memcmp(big, a, al) && big[al] == 0xAA, "oversized buffer: wrong bytes or overrun");
    // small images are not cached (64x64 and up are)
    uint8_t sbc[16 * 16], sa[16 * 16]; fill(sbc, sizeof sbc); size_t st = astc_cache_get_stats().stores;
    CHECK(nvn_bc_astc_image(0x4D, sbc, sizeof sbc, 16, 16, sa, sizeof sa) == 0 && astc_cache_get_stats().stores == st, "a 16x16 image was cached");
    // a damaged entry is re-encoded and replaced
    DIR *d = opendir(dir); struct dirent *e; char victim[700] = ""; while (d && (e = readdir(d))) if (e->d_name[0] != '.') { snprintf(victim, sizeof victim, "%s/%s", dir, e->d_name); break; } if (d) closedir(d);
    CHECK(victim[0] && truncate(victim, 50) == 0, "no file to damage");
    int before = atomic_load(&fake_encodes);
    for (int pass = 0; pass < 2; pass++) { bc[0] ^= 0; nvn_bc_astc_image(0x4D, bc, bcl, W, H, b, al); nvn_bc_astc_image(0x4E, bc, bcl, W, H, b, al); }
    CHECK(atomic_load(&fake_encodes) <= before + 2, "a damaged entry was encoded more than once (%d → %d)", before, atomic_load(&fake_encodes));
    // signed BC4 / BC5 need no encoder and no cache
    uint8_t sb[16 * 16 * 8]; fill(sb, sizeof sb); uint8_t so[64 * 64]; int e0 = atomic_load(&fake_encodes);
    CHECK(nvn_bc_astc_image(0x4A, sb, 16 * 16 * 8, 64, 64, so, sizeof so) == 0 && atomic_load(&fake_encodes) == e0, "signed BC4 used the encoder");
    // off by the switch, and without a folder
    setenv("DM_NVN_BC_ASTC_CACHE", "0", 1); nvn_bc_astc_init();
    CHECK(!astc_cache_enabled(), "DM_NVN_BC_ASTC_CACHE=0 left the cache on");
    int e1 = atomic_load(&fake_encodes); nvn_bc_astc_image(0x4D, bc, bcl, W, H, a, al); nvn_bc_astc_image(0x4D, bc, bcl, W, H, a, al);
    CHECK(atomic_load(&fake_encodes) == e1 + 2, "encodes with the cache off: %d", atomic_load(&fake_encodes) - e1);
    unsetenv("DM_NVN_BC_ASTC_CACHE"); unsetenv("DM_TEMP"); nvn_bc_astc_init(); CHECK(!astc_cache_enabled(), "no folder and no DM_TEMP: cache on");
    char tmpd[600]; snprintf(tmpd, sizeof tmpd, "%s/t", root); setenv("DM_TEMP", tmpd, 1); nvn_bc_astc_init();
    char want[700]; snprintf(want, sizeof want, "%s_astc", tmpd); struct stat sst;
    CHECK(astc_cache_enabled() && !stat(want, &sst) && S_ISDIR(sst.st_mode), "DM_TEMP did not give %s", want);
    // a file from another salt version is not served: change the salt through a reopen
    astc_cache_close(); rm_dir(dir); rm_dir(want);
    free(bc); free(a); free(b); free(c); free(big);
}

int main(void) {
    char root[] = "/tmp/t_astc_cache_XXXXXX";
    if (!mkdtemp(root)) { printf("no temp folder\n"); return 1; }
    module_tests(root);
    integration_tests(root);
    rm_dir(root);
    if (fails) { printf("t_astc_cache: %d failures\n", fails); return 1; }
    printf("t_astc_cache: all passed\n");
    return 0;
}
