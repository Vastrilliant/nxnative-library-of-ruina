"""The stub of a switchable code patch (dm/tools/nso_switch.py): where each path goes and what each moved instruction becomes, without game files."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nso_switch as sw

RET, STP_PROLOGUE, MOV_W19 = 0xD65F03C0, 0xA9BF7BFD, 0x5284E1F3


def b_target(word, at):
    assert word & 0xFC000000 == 0x14000000, hex(word)
    imm = word & 0x3FFFFFF
    return at + ((imm - (1 << 26) if imm >> 25 else imm) << 2)


class Moved(unittest.TestCase):
    def test_plain_instruction_returns_to_the_next_word(self):
        w = sw.moved(MOV_W19, 0x1000, 0x5000000)
        self.assertEqual(w[0], MOV_W19)
        self.assertEqual(b_target(w[1], 0x5000004), 0x1004)

    def test_branch_goes_to_its_own_target(self):
        w = sw.moved(0x14000041, 0x24410A8, 0x5000000)   # b +0x104
        self.assertEqual(len(w), 1)
        self.assertEqual(b_target(w[0], 0x5000000), 0x24410A8 + 0x104)

    def test_conditional_branch_keeps_both_exits(self):
        w = sw.moved(0xB4000A40, 0x2000, 0x5000000)   # cbz x0, +0x148
        self.assertEqual(w[0] & ~(0x7FFFF << 5), 0xB4000000)
        self.assertEqual((w[0] >> 5) & 0x7FFFF, 2)   # taken: the third word
        self.assertEqual(b_target(w[1], 0x5000004), 0x2004)
        self.assertEqual(b_target(w[2], 0x5000008), 0x2000 + 0x148)

    def test_call_sets_the_link_register_to_the_original_return_address(self):
        w = sw.moved(0x94000010, 0x3000, 0x5000000)   # bl +0x40
        self.assertEqual(w[0] & 31, 30)   # adrp x30
        self.assertEqual(w[1], sw.ADDI(30, 30, 0x3004 & 0xFFF))
        self.assertEqual(b_target(w[2], 0x5000008), 0x3040)

    def test_adrp_keeps_its_page(self):
        w = sw.moved(0xB0000009, 0x1234, 0x5000000)   # adrp x9, +0x1000 pages (immlo 1, immhi 0)
        self.assertEqual(len(w), 2)
        self.assertEqual(w[0], sw.ADRP(9, 0x5000000, 0x1000 + 0x1000))
        self.assertEqual(b_target(w[1], 0x5000004), 0x1238)

    def test_literal_load_is_refused(self):
        with self.assertRaises(ValueError): sw.moved(0x58000040, 0x1000, 0x5000000)


class Stub(unittest.TestCase):
    def test_layout_and_both_paths(self):
        start, off = 0x5000000, 0x244DC60
        s = sw.stub(start, off, 3, STP_PROLOGUE, RET, 0x4000000)
        self.assertEqual(s[0], sw.STP_PRE(16, 17, 31, -16))
        self.assertEqual(s[2], 0x39400000 | (sw.SWITCH_BYTE0 + 3) << 10 | 16 << 5 | 16)   # ldrb w16, [x16, #byte]
        self.assertEqual(s[4], sw.LDP_POST(16, 17, 31, 16))
        self.assertEqual(s[5], STP_PROLOGUE)   # off path: the original word
        self.assertEqual(b_target(s[6], start + 24), off + 4)
        on = 7
        self.assertEqual(s[on], sw.LDP_POST(16, 17, 31, 16))
        self.assertEqual(s[on + 1], RET)   # on path: the patched word
        cb = s[3]
        self.assertEqual(cb & 0xFF00001F, 0x35000010)
        self.assertEqual(start + 12 + (((cb >> 5) & 0x7FFFF) << 2), start + 4 * on)

    def test_patch_area_before_the_base(self):
        s = sw.stub(-0x2000000 + 0x100, 0x1000, 0, STP_PROLOGUE, RET, 0x6000000)
        self.assertEqual(len(s), 10)

    def test_byte_index_is_bounded(self):
        with self.assertRaises(AssertionError): sw.stub(0x5000000, 0x1000, sw.SWITCH_MAX, STP_PROLOGUE, RET, 0x4000000)


if __name__ == '__main__':
    unittest.main()
