// Mod "enhanced-combat-page-view": Cyaminthe's Enhanced Combat Page View (Steam Workshop 2802216129), as host callbacks on observation gates.
// The PC mod scales BattleDiceCardUI.vibeRect (a child RectTransform) of the card under the cursor between 1.0 and 1.75 (mouse wheel), and moves it by sizeDelta * (scale - 1) / 2 so the page grows away from its anchor.
// Here the scale is one launcher setting (DM_MOD_ECPV_ZOOM, percent, 100 to 175, default 125: selected pages are 25% bigger), applied when a page is shown in detail and undone when it is hidden:
//   show_pre     BattleDiceCardUI.ShowDetail entry (x0 = the card): scale and offset vibeRect
//   hide_pre     BattleDiceCardUI.HideDetail entry (x0 = the card): back to scale 1, offset 0
//   vibe_pre     BattleDiceCardUI.PlayVibeCard entry (x0 = the card): back to scale 1, so the vibration plays on a normal page
//   setcard_pre  BattleDiceCardUI.SetCard entry (x0 = the card, x1 = the new model): a card reused for another page starts at scale 1
// Not ported: hands of more than 10 pages (the PC mod adds card UI objects), the 65 to 60 rotation constant, and the mouse wheel.
// Off by default: DM_MOD_ECPV=1 turns the callbacks on (the launcher's Mods > Enhanced Combat Page View switch, or env.txt). The gates are always in the code and pass through while it is off.
#include "dm.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));

#define K_RECT_GET_SIZEDELTA 0x19BA760u         // RectTransform.get_sizeDelta
#define K_TRANSFORM_SET_LOCALPOS 0x19BAC70u
#define K_TRANSFORM_SET_LOCALSCALE 0x19BD790u

#define F_CARD_VIBERECT 0x18                    // BattleDiceCardUI.vibeRect
#define F_CARD_MODEL 0x128                      // BattleDiceCardUI._cardModel

typedef struct { float x, y; } v2;
typedef struct { float x, y, z; } v3;

static uint8_t *s_main;
static int g_hits[4];
static int first_hits(int which) { return g_hits[which]++ < 4; }

static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(s_main + (rva)))

static void *field(void *self, uintptr_t off) { return self ? *(void *const *)((uintptr_t)self + off) : NULL; }

static float zoom(void) {
    static float z = 0;
    if (z <= 0) {
        const char *v = getenv("DM_MOD_ECPV_ZOOM");
        int pct = v && v[0] ? atoi(v) : 125;
        if (pct < 100) pct = 100;
        if (pct > 175) pct = 175;
        z = (float)pct / 100.0f;
        LOG("[mod enhanced-combat-page-view] zoom %d%% (DM_MOD_ECPV_ZOOM=%s)", pct, v ? v : "unset");
    }
    return z;
}

static void set_zoom(void *card, float scale) {
    void *rect = field(card, F_CARD_VIBERECT);
    if (!rect) return;
    v3 pos = {0, 0, 0};
    if (scale > 1.0f) {
        v2 size = FN(K_RECT_GET_SIZEDELTA, v2, void *, const void *)(rect, NULL);
        pos.x = size.x * (scale - 1.0f) / 2.0f;
        pos.y = size.y * (scale - 1.0f) / 2.0f;
    }
    v3 sc = {scale, scale, 1.0f};
    FN(K_TRANSFORM_SET_LOCALSCALE, void, void *, v3, const void *)(rect, sc, NULL);
    FN(K_TRANSFORM_SET_LOCALPOS, void, void *, v3, const void *)(rect, pos, NULL);
}

static void show_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    set_zoom((void *)r[0], zoom());
    if (first_hits(0)) LOG("[mod enhanced-combat-page-view] show: card %#llx", (unsigned long long)r[0]);
}
static void reset_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    set_zoom((void *)r[0], 1.0f);
}
static void setcard_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    if (field((void *)r[0], F_CARD_MODEL) != (void *)r[1]) set_zoom((void *)r[0], 1.0f);
}

static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_ECPV");
        on = v && v[0] == '1';
        LOG("[mod enhanced-combat-page-view] %s (DM_MOD_ECPV=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}

static void on_show_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, show_run); }
static void on_hide_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, reset_run); }
static void on_vibe_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, reset_run); }
static void on_setcard_pre(uint64_t *r, int p) { if (mod_enabled()) dm_mod_gate_run(r, p, setcard_run); }

__attribute__((constructor)) static void mod_enhanced_combat_page_view_attach(void) {
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_SHOW_PRE, on_show_pre);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_HIDE_PRE, on_hide_pre);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_VIBE_PRE, on_vibe_pre);
    gate_obs_set(MODGATE_ENHANCED_COMBAT_PAGE_VIEW_SETCARD_PRE, on_setcard_pre);
}
