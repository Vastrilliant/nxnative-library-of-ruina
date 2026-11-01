# The stub of one switchable code patch (mods/<id>/exefs, scripts/apply_mods.py; dm/tools/patch_nso.py "Switchable patches"): the instruction word at a site in the text is replaced by a branch to this stub,
# which reads the mod's switch byte (state page of the host, loader.c dm_modsw, +0x100 + the mod's index; 1 = on) and runs either the original instruction or the patched one, then goes on at site + 4.
# The code is position independent apart from the instruction it moves: x16 and x17 are saved around the test and restored before either instruction runs, so the instruction sees every register and sp as at the site;
# the flags are not touched. A moved instruction is re-encoded for its new address (branches, adr / adrp); a call (bl, blr) is emulated by setting x30 to site + 4 and jumping, so the callee returns into the
# original code and an unwinder never meets a frame of the patch area
from nso_asm import B, ADRP, ADDI, BR, STP_PRE, LDP_POST

SWITCH_BYTE0 = 0x100   # the first mod's byte in the state page; the page is 0x4000 bytes and the frame step state (patch_nso.py FRAME_STEP) uses the first 0x40
SWITCH_MAX = 0xE00

def _sext(v, bits): return v - (1 << bits) if v >> (bits - 1) else v

def moved(w, src, at):
    """The words that run the instruction w of address src from address at, going on at src + 4 (a taken branch goes to its own target, a ret / br leaves). Raises ValueError for one it cannot move"""
    nxt = src + 4
    if w & 0x7C000000 == 0x14000000:   # b / bl
        tgt = src + (_sext(w & 0x3FFFFFF, 26) << 2)
        if w >> 31: return [ADRP(30, at, nxt), ADDI(30, 30, nxt & 0xFFF), B(at + 8, tgt)]
        return [B(at, tgt)]
    if w & 0xFF000010 == 0x54000000 or w & 0x7E000000 in (0x34000000, 0x36000000):   # b.cond, cbz / cbnz, tbz / tbnz: the condition jumps over the fall-through branch to the one that reaches the target
        bits = 14 if w & 0x7E000000 == 0x36000000 else 19
        tgt = src + (_sext((w >> 5) & ((1 << bits) - 1), bits) << 2)
        return [w & ~(((1 << bits) - 1) << 5) | (2 << 5), B(at + 4, nxt), B(at + 8, tgt)]
    if w & 0x9F000000 in (0x10000000, 0x90000000):   # adr, adrp
        rd = w & 31
        if rd == 31: raise ValueError(f'{w:08x}: adr / adrp with xzr as its destination')
        imm = _sext(((w >> 5) & 0x7FFFF) << 2 | (w >> 29) & 3, 21)
        if w & 0x80000000: return [ADRP(rd, at, (src & ~0xFFF) + (imm << 12)), B(at + 4, nxt)]
        tgt = src + imm
        return [ADRP(rd, at, tgt), ADDI(rd, rd, tgt & 0xFFF), B(at + 8, nxt)]
    if w & 0x3B000000 == 0x18000000: raise ValueError(f'{w:08x}: ldr (literal) cannot be moved to the patch area')
    if w & 0xFFFFFC1F == 0xD63F0000:   # blr xn
        n = (w >> 5) & 31
        if n == 30: raise ValueError(f'{w:08x}: blr x30')
        return [ADRP(30, at, nxt), ADDI(30, 30, nxt & 0xFFF), BR(n)]
    return [w, B(at + 4, nxt)]

def stub(start, off, idx, orig, new, state_off):
    """The words of the stub of the site at text offset off, to be placed at start: mod switch idx, the original word orig, the patched word new, state_off = the state page's offset in the image"""
    assert 0 <= idx < SWITCH_MAX, f'mod switch {idx}'
    n = len(moved(orig, off, start + 20))
    on = start + 20 + 4 * n
    cbnz = 0x35000000 | (((on - (start + 12)) >> 2) & 0x7FFFF) << 5 | 16   # cbnz w16, on
    return ([STP_PRE(16, 17, 31, -16), ADRP(16, start + 4, state_off),
             0x39400000 | (SWITCH_BYTE0 + idx) << 10 | 16 << 5 | 16,        # ldrb w16, [x16, #byte]
             cbnz, LDP_POST(16, 17, 31, 16)] + moved(orig, off, start + 20)
            + [LDP_POST(16, 17, 31, 16)] + moved(new, off, on + 4))
