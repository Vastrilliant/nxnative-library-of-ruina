#ifndef BCN_DECODE_H
#define BCN_DECODE_H

#include <stddef.h>
#include <stdint.h>

enum {
    BCN_BC1 = 1,
    BCN_BC2,
    BCN_BC3,
    BCN_BC4,
    BCN_BC5,
    BCN_BC7,
    BCN_BC4S,
    BCN_BC5S
};

int bcn_block_bytes(int fmt);
int bcn_is_signed(int fmt);
void bcn_decode_block(int fmt, const uint8_t *blk, uint8_t rgba[64]);
int bcn_decode_image(int fmt, const uint8_t *blocks, size_t len, unsigned w, unsigned h, uint8_t *rgba);

#endif
