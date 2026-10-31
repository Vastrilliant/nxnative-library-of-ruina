// On-disk cache of transcoded textures (BC_ASTC_PLAN.md stage 5c): the ASTC bytes nvn_bc_astc_image made for one BC image, found again by a hash of the BC bytes, so a texture is encoded once per install
// instead of at every launch. Pure C, no Metal, no astcenc. One file per image in one folder; a damaged or foreign file is a miss, never an error. Thread safe.
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct { uint64_t a, b; } astc_cache_key;

// Opens the cache: dir is created if missing (one level), max_bytes caps the folder (the oldest files go first, down to 90%, now and after later stores pass the cap); salt names everything
// the bytes depend on besides the input (astcenc version, quality, this file format): another salt finds nothing of an older one. Returns 1 when the cache is usable, 0 otherwise (it then stays off).
int astc_cache_open(const char *dir, uint64_t max_bytes, const char *salt);
int astc_cache_enabled(void);
void astc_cache_close(void);   // off again (tests)

// The key of one image: its NVN format, size and BC bytes, and the salt
astc_cache_key astc_cache_make_key(int nvn_format, unsigned w, unsigned h, const uint8_t *bc, size_t bc_len);

// 1 and out filled (len bytes) when the cache holds the image, 0 otherwise (also when the file's length or checksum is wrong: such a file is removed)
int astc_cache_load(astc_cache_key k, uint8_t *out, size_t len);
// Stores len bytes, atomically (a temporary file renamed over the name), best effort: a full disk or a read-only folder only means the next launch encodes again
void astc_cache_store(astc_cache_key k, const uint8_t *data, size_t len);

typedef struct { uint64_t hits, misses, stores, evicted, bad; } astc_cache_stats;
astc_cache_stats astc_cache_get_stats(void);
