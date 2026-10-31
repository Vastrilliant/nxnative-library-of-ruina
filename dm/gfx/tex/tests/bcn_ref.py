import sys
import numpy as np
from PIL import Image

M = (1 << 64) - 1
FORMATS = {1: (8, "RGBA", 1), 2: (16, "RGBA", 2), 3: (16, "RGBA", 3), 4: (8, "L", 4), 5: (16, "RGB", 5), 6: (16, "RGBA", 7)}
BLOCKS_X = BLOCKS_Y = 32


def splitmix(state):
    state = (state + 0x9E3779B97F4A7C15) & M
    z = state
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M
    return state, z ^ (z >> 31)


def make_blocks(fmt):
    bb = FORMATS[fmt][0]
    state = 0x1000 + fmt
    out = bytearray()
    for i in range(BLOCKS_X * BLOCKS_Y):
        blk = bytearray()
        while len(blk) < bb:
            state, z = splitmix(state)
            blk += z.to_bytes(8, "little")
        if fmt == 6:
            mode = i % 8
            blk[0] = (blk[0] & ~((1 << (mode + 1)) - 1) & 0xFF) | (1 << mode)
        out += blk
    return bytes(out)


def fnv(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & M
    return h


def reference(fmt):
    bb, mode, pillow_fmt = FORMATS[fmt]
    data = make_blocks(fmt)
    size = (BLOCKS_X * 4, BLOCKS_Y * 4)
    img = np.asarray(Image.frombytes(mode, size, data, "bcn", (pillow_fmt,)))
    if fmt == 4:
        rgba = np.zeros(size[::-1] + (4,), np.uint8)
        rgba[..., 0] = img
        rgba[..., 3] = 255
    elif fmt == 5:
        rgba = np.zeros(size[::-1] + (4,), np.uint8)
        rgba[..., 0] = img[..., 0]
        rgba[..., 1] = img[..., 1]
        rgba[..., 3] = 255
    else:
        rgba = img
    return rgba.tobytes()


if __name__ == "__main__":
    for fmt in FORMATS:
        print(f"    {{{fmt}, 0x{fnv(reference(fmt)):016x}ull}},")
