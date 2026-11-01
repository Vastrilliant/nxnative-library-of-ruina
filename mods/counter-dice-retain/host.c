// Mod "counter-dice-retain": XDTM's Counter Dice Retain (Steam Workshop 2787425839), whose three Harmony prefixes on BattlePlayingCardDataInUnitModel become host callbacks on observation gates at the entries of the same methods (x0 = the card):
//   win_parry_pre     OnWinParryingAttack, OnWinParryingDefense:  if currentBehavior is a counter (Standby) die, currentBehavior.isBonusAttack = true
//   after_action_pre  AfterAction:                                if currentBehavior is a counter (Standby) die, currentBehavior.isUsed = false
// Each gate runs before the game's own method, as a Harmony prefix that returns true does. So a counter die that wins a clash is flagged as a bonus attack, and is not marked used after its action: it answers the next attack too, until it loses a clash.
// Off by default: DM_MOD_COUNTERDICERETAIN=1 turns the callbacks on (the launcher's Mods > Retain Counter Dice switch, or env.txt). The gates are always in the code and pass through while it is off.
#include "dm.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));

#define K_BATTLEDICEBEHAVIOR_GET_TYPE 0x244E5D0u   // BattleDiceBehavior.get_Type

#define F_PLAYINGCARD_CURRENT_BEHAVIOR 0x60        // BattlePlayingCardDataInUnitModel.currentBehavior
#define F_DICEBEHAVIOR_IS_BONUS_ATTACK 0x29        // BattleDiceBehavior.isBonusAttack (bool)
#define F_DICEBEHAVIOR_IS_USED 0x2B                // BattleDiceBehavior.isUsed (bool)

#define BEHAVIOUR_STANDBY 2                        // LOR_DiceSystem.BehaviourType: Atk 0, Def 1, Standby 2 (the counter dice)

static uint8_t *s_main;
static int g_hits[2];
static const char *const g_gate_name[2] = { "after_action_pre", "win_parry_pre" };
static int first_hits(int which) { return g_hits[which]++ < 4; }

// main's image start, so main + RVA is the game function
static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(s_main + (rva)))

// card->currentBehavior when it is a counter die, else NULL
static void *counter_die(const uint64_t *r) {
    if (!main_base() || !r[0]) return NULL;
    void *cur = *(void *const *)((uintptr_t)r[0] + F_PLAYINGCARD_CURRENT_BEHAVIOR);
    if (!cur) return NULL;
    return FN(K_BATTLEDICEBEHAVIOR_GET_TYPE, int, void *, const void *)(cur, NULL) == BEHAVIOUR_STANDBY ? cur : NULL;
}

static void after_action_run(uint64_t *r, int param) {
    (void)param;
    void *die = counter_die(r);
    if (!die) return;
    *(uint8_t *)((uintptr_t)die + F_DICEBEHAVIOR_IS_USED) = 0;
    if (first_hits(0)) LOG("[mod counter-dice-retain] %s: counter die %#llx kept", g_gate_name[0], (unsigned long long)(uintptr_t)die);
}
static void win_parry_run(uint64_t *r, int param) {
    (void)param;
    void *die = counter_die(r);
    if (!die) return;
    *(uint8_t *)((uintptr_t)die + F_DICEBEHAVIOR_IS_BONUS_ATTACK) = 1;
    if (first_hits(1)) LOG("[mod counter-dice-retain] %s: counter die %#llx flagged as bonus attack", g_gate_name[1], (unsigned long long)(uintptr_t)die);
}

// Read once, at the first gate hit: the launcher's choices are in the environment by then, but not yet when constructors run
static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_COUNTERDICERETAIN");
        on = v && v[0] == '1';
        LOG("[mod counter-dice-retain] %s (DM_MOD_COUNTERDICERETAIN=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}
static void on_after_action_pre(uint64_t *r, int p) { if (!mod_enabled()) return; dm_mod_gate_run(r, p, after_action_run); }
static void on_win_parry_pre(uint64_t *r, int p) { if (!mod_enabled()) return; dm_mod_gate_run(r, p, win_parry_run); }

__attribute__((constructor)) static void mod_counter_dice_retain_attach(void) {
    gate_obs_set(MODGATE_COUNTER_DICE_RETAIN_AFTER_ACTION_PRE, on_after_action_pre);
    gate_obs_set(MODGATE_COUNTER_DICE_RETAIN_WIN_PARRY_PRE, on_win_parry_pre);
}
