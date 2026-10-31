# Library of Ruina (0100E7A019246000): the rewriter's patches of this game's own builds. dm/tools/patch_nso.py loads every profile's patches.py and applies those of a matching build:
# CUTS as described there ("Registered patches"). Patches of Nintendo SDK module builds are in dm/tools/sdk_patches.py


def _le(*words): return b''.join(w.to_bytes(4, 'little') for w in words).hex()
def _bl(at, to): return 0x94000000 | ((to - at) >> 2 & 0x3ffffff)
def _b(at, to): return 0x14000000 | ((to - at) >> 2 & 0x3ffffff)


_on_play_end, _goto, _cave = 0x210f5c0, 0x21100e0, 0x353dcf0  # EntryScene.OnPlayEnd, .GotoTitleOrSelectLanguge, LoadingManager.TestStart (a test entry: no direct caller, no call in a boot; not OnCompleteInitializePlatform_xboxlive, which despite its name runs at startup here)
_guard = _cave + 13 * 4

CUTS = {
    # 1.03 (v262144) main: the menu scene loads whichever of two events comes first (GitHub issue #739).
    # EntryScene.Update calls OnPlayEnd once SplashScreen.isFinished, which makes _option (+0x20, read by LoadOptionData) and _latestData. A coroutine calls GotoTitleOrSelectLanguge when
    # the Addressables load ends, and that reads _option. On the Switch's CPU and storage the load ends long after the splash; on a Mac it can end first, with _option still null:
    # StartMoveSceneCoroutine is never called, level1 is never opened, and the opening movie stays on its last frame. Measured with probes: stuck, the load ended at frame 496 and the splash
    # at 526; working, the splash at 491 and the load at 507. This is the game's own race, so it is cut here, and nothing is delayed: GotoTitleOrSelectLanguge makes the models first when
    # _option is null, and OnPlayEnd returns when they exist, so the later call from Update keeps what LoadOptionData read.
    'aeea627ddbd1214764dfef5f965e94a2': [
        (_goto, 'fd7bbea9', _le(_b(_goto, _cave)), 'GotoTitleOrSelectLanguge entry → OnPlayEnd first when _option is null'),
        (_on_play_end, 'fd7bbda9', _le(_b(_on_play_end, _guard)), 'OnPlayEnd entry → return when _option exists'),
        (_cave, 'fd7bbda9f50b00f9fd030091f44f02a994f900f0b5e700d088767739b54645f9f30300aac8000037a0e700d0004445f9684e39972800805288763739a10240f9e00313aaf50b40f9', _le(
            0xa9be7bfd,  # stp x29, x30, [sp, #-0x20]!
            0x910003fd,  # mov x29, sp
            0xf9000bf3,  # str x19, [sp, #0x10]
            0xaa0003f3,  # mov x19, x0
            0xf9401008,  # ldr x8, [x0, #0x20] _option
            0xb5000068,  # cbnz x8, +12
            0xaa1f03e1,  # mov x1, xzr
            _bl(_cave + 7 * 4, _on_play_end),
            0xaa1303e0,  # mov x0, x19
            0xf9400bf3,  # ldr x19, [sp, #0x10]
            0xa8c27bfd,  # ldp x29, x30, [sp], #0x20
            0xa9be7bfd,  # stp x29, x30, [sp, #-0x20]! GotoTitleOrSelectLanguge's first instruction
            _b(_cave + 12 * 4, _goto + 4),
            0xf9401008,  # ldr x8, [x0, #0x20] the guard: _option
            0xb4000048,  # cbz x8, +8
            0xd65f03c0,  # ret
            0xa9bd7bfd,  # stp x29, x30, [sp, #-0x30]! OnPlayEnd's first instruction
            _b(_cave + 17 * 4, _on_play_end + 4),
        ), 'the check and the guard, over LoadingManager.TestStart'),
    ],
}

# How general each patch is and could be (dm/tools/docs_patches.py "Binding", listed in docs/agents/game-patches.md)
BINDING = [
    {'what': 'EntryScene splash and load order', 'now': 'build', 'goal': 'build', 'purpose': 'race',
     'why': "the game's own race between its splash and its Addressables load; its fields and methods are this build's, and no engine or SDK behaviour is wrong"},
]
