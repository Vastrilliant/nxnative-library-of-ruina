#include "astc_cache.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define FILE_MAGIC 0x31435441u   // "ATC1"
typedef struct { uint32_t magic, fmt, w, h; uint64_t len, sum; } hdr_t;   // 32 bytes, then the ASTC bytes; sum: hash of those bytes

static atomic_int g_on;
static char g_dir[1024];
static char g_salt[128];
static uint64_t g_max, g_total;   // g_total: bytes in the folder as counted at open plus what this run stored (approximate; only decides when to scan again)
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_ullong s_hits, s_misses, s_stores, s_evicted, s_bad;

// A 64-bit multiply-rotate hash over 8-byte words (non-cryptographic: the cache has no adversary, and two independent 64-bit lanes make an accidental collision a 2^-128 event)
static uint64_t rotl(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }
static void hash2(const void *p, size_t n, uint64_t s0, uint64_t s1, uint64_t *oa, uint64_t *ob) {
    const uint8_t *b = p; uint64_t a = s0 ^ 0x9E3779B97F4A7C15ull, c = s1 ^ 0xC2B2AE3D27D4EB4Full; size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        uint64_t w; memcpy(&w, b + i, 8);
        a = rotl(a ^ (w * 0xFF51AFD7ED558CCDull), 27) * 0x9FB21C651E98DF25ull + 0x52DCE729;
        c = rotl(c + (w ^ (w >> 29)) * 0xC4CEB9FE1A85EC53ull, 31) * 0x165667B19E3779F9ull;
    }
    uint64_t t = 0; for (size_t k = 0; i + k < n; k++) t |= (uint64_t)b[i + k] << (8 * k);
    a = rotl(a ^ (t * 0xFF51AFD7ED558CCDull), 27) * 0x9FB21C651E98DF25ull; c = rotl(c + t * 0xC4CEB9FE1A85EC53ull, 31) * 0x165667B19E3779F9ull;
    a ^= n; c ^= (uint64_t)n << 1;
    a ^= a >> 33; a *= 0xFF51AFD7ED558CCDull; a ^= a >> 33; c ^= c >> 29; c *= 0x9FB21C651E98DF25ull; c ^= c >> 32;
    *oa = a ^ rotl(c, 17); *ob = c ^ rotl(a, 41);
}

static void name_of(astc_cache_key k, char *out, size_t n) { snprintf(out, n, "%s/%016llx%016llx", g_dir, (unsigned long long)k.a, (unsigned long long)k.b); }

typedef struct { char name[64]; uint64_t size; time_t mt; } ent_t;
static int by_age(const void *x, const void *y) { const ent_t *a = x, *b = y; return a->mt < b->mt ? -1 : a->mt > b->mt; }

// Removes the oldest files until the folder is at 90% of the cap. Caller holds g_lock. Also removes leftover temporary files of a crashed run.
static void evict(void) {
    DIR *d = opendir(g_dir); if (!d) return;
    size_t cap = 256, n = 0; ent_t *v = malloc(cap * sizeof *v); uint64_t total = 0;
    struct dirent *e;
    while (v && (e = readdir(d))) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof v->name) continue;
        char p[1100]; struct stat st; snprintf(p, sizeof p, "%s/%s", g_dir, e->d_name);
        if (stat(p, &st) || !S_ISREG(st.st_mode)) continue;
        if (strstr(e->d_name, ".tmp")) { if (st.st_mtime < time(NULL) - 3600) unlink(p); continue; }
        if (n == cap) { cap *= 2; ent_t *nv = realloc(v, cap * sizeof *v); if (!nv) break; v = nv; }
        snprintf(v[n].name, sizeof v[n].name, "%s", e->d_name); v[n].size = (uint64_t)st.st_size; v[n].mt = st.st_mtime; total += v[n].size; n++;
    }
    closedir(d);
    if (v && total > g_max) {
        qsort(v, n, sizeof *v, by_age);
        uint64_t goal = g_max / 10 * 9;
        for (size_t i = 0; i < n && total > goal; i++) {
            char p[1100]; snprintf(p, sizeof p, "%s/%s", g_dir, v[i].name);
            if (!unlink(p)) { total -= v[i].size; atomic_fetch_add(&s_evicted, 1); }
        }
    }
    g_total = total; free(v);
}

int astc_cache_open(const char *dir, uint64_t max_bytes, const char *salt) {
    astc_cache_close();
    if (!dir || !*dir || !max_bytes || strlen(dir) >= 900) return 0;
    if (mkdir(dir, 0755) && errno != EEXIST) return 0;
    struct stat st; if (stat(dir, &st) || !S_ISDIR(st.st_mode) || access(dir, W_OK | X_OK)) return 0;
    pthread_mutex_lock(&g_lock);
    snprintf(g_dir, sizeof g_dir, "%s", dir); snprintf(g_salt, sizeof g_salt, "%s", salt ? salt : ""); g_max = max_bytes;
    evict();
    pthread_mutex_unlock(&g_lock);
    atomic_store(&g_on, 1);
    return 1;
}
int astc_cache_enabled(void) { return atomic_load(&g_on); }
void astc_cache_close(void) {
    atomic_store(&g_on, 0);
    atomic_store(&s_hits, 0); atomic_store(&s_misses, 0); atomic_store(&s_stores, 0); atomic_store(&s_evicted, 0); atomic_store(&s_bad, 0);
}

astc_cache_key astc_cache_make_key(int fmt, unsigned w, unsigned h, const uint8_t *bc, size_t len) {
    uint64_t sa, sb; hash2(g_salt, strlen(g_salt), 1, 2, &sa, &sb);
    uint64_t meta[4] = {(uint64_t)(unsigned)fmt, w, h, len};
    uint64_t ma, mb; hash2(meta, sizeof meta, sa, sb, &ma, &mb);
    astc_cache_key k; hash2(bc, len, ma, mb, &k.a, &k.b);
    return k;
}

int astc_cache_load(astc_cache_key k, uint8_t *out, size_t len) {
    if (!atomic_load(&g_on) || !out || !len) return 0;
    char p[1100]; name_of(k, p, sizeof p);
    int fd = open(p, O_RDONLY); if (fd < 0) { atomic_fetch_add(&s_misses, 1); return 0; }
    hdr_t h; int ok = read(fd, &h, sizeof h) == (ssize_t)sizeof h && h.magic == FILE_MAGIC && h.len == len;
    size_t got = 0;
    while (ok && got < len) { ssize_t r = read(fd, out + got, len - got); if (r <= 0) break; got += (size_t)r; }
    uint8_t extra; ok = ok && got == len && read(fd, &extra, 1) == 0;   // exactly len bytes
    close(fd);
    if (ok) { uint64_t a, b; hash2(out, len, 7, 11, &a, &b); ok = a == h.sum; }
    if (!ok) { unlink(p); atomic_fetch_add(&s_bad, 1); atomic_fetch_add(&s_misses, 1); return 0; }
    utimes(p, NULL);   // used now: the oldest-first eviction is least-recently-used
    atomic_fetch_add(&s_hits, 1);
    return 1;
}

void astc_cache_store(astc_cache_key k, const uint8_t *data, size_t len) {
    if (!atomic_load(&g_on) || !data || !len) return;
    char p[1100], t[1160]; name_of(k, p, sizeof p);
    snprintf(t, sizeof t, "%s.%lu.%llx.tmp", p, (unsigned long)getpid(), (unsigned long long)(uintptr_t)pthread_self());
    int fd = open(t, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd < 0) return;
    hdr_t h = {FILE_MAGIC, 0, 0, 0, len, 0}; uint64_t b; hash2(data, len, 7, 11, &h.sum, &b);
    int ok = write(fd, &h, sizeof h) == (ssize_t)sizeof h;
    size_t put = 0; while (ok && put < len) { ssize_t r = write(fd, data + put, len - put); if (r <= 0) { ok = 0; break; } put += (size_t)r; }
    if (close(fd)) ok = 0;
    if (!ok || rename(t, p)) { unlink(t); return; }
    atomic_fetch_add(&s_stores, 1);
    pthread_mutex_lock(&g_lock);
    g_total += len + sizeof h;
    if (g_total > g_max) evict();
    pthread_mutex_unlock(&g_lock);
}

astc_cache_stats astc_cache_get_stats(void) {
    astc_cache_stats s = {atomic_load(&s_hits), atomic_load(&s_misses), atomic_load(&s_stores), atomic_load(&s_evicted), atomic_load(&s_bad)};
    return s;
}
