// Mod "speed-dice": the speed-dice recolour of Library-of-Ruina-CustomDice-NX-Mod, as host callbacks on observation gates (the Switch plugin patched code at run time through exlaunch, which iOS cannot).
// The callbacks run on the guest's stack through dm_mod_gate_run (hle_modcall.c), so they may call the game's functions at main + RVA exactly as the plugin did.
//   stage_pre         x0 = StageController                       (InitCommon, StartBattle entries)
//   card_phase_pre    x0 = StageController                       (ApplyLibrarianCardPhase entry)
//   dice_init_post    x23 = SpeedDiceUI, w27 = faction          (after the call of SpeedDiceUI.Init)
//   dice_sprite_post  x19 = SpeedDiceUI, w20 = value (>= 999 only) (ChangeSprite's infinite-value path)
// Off by default: DM_MOD_SPEEDDICE=1 turns the callbacks on (the launcher's Mods > Custom Speed Dice switch, or env.txt). The gates are always in the code and pass through while it is off.
#include "dm.h"
#include "hle_mods_gen.h"

void dm_mod_gate_run(uint64_t *regs, int param, void (*fn)(uint64_t *, int));

#define K_SPEEDDICE_UI_INIT 0x233BF20u
#define K_SPEEDDICE_UI_CHANGE_SPRITE 0x233B750u

#define F_NORMAL_FRAME 0xB8
#define F_LIGHT_FRAME 0xC0
#define F_TENS_NUM 0xD0
#define F_UNITS_NUM 0xD8
#define F_ROULETTE_IMG 0xE8
#define F_TXT_SPEED_RANGE 0xF0
#define F_TXT_SPEED_MAX 0xF8
#define F_BREAKED_FRAME 0x108
#define F_BREAKED_LINEAR_DODGE 0x110
#define F_LOCKED_FRAME 0x120
#define F_LOCKED_ICON 0x128
#define F_HIGHLIGHT_FRAME 0x140

#define K_SPRITEUTIL_LOAD_SPRITE 0x1DBB8E0u
#define K_SPRITE_CREATE 0x19C3210u
#define K_SPRITE_GET_TEXTURE 0x19C1BD0u
#define K_TEXTURE_GET_WIDTH 0x2BE3930u
#define K_TEXTURE_GET_HEIGHT 0x2BE39D0u
#define K_IMAGE_GET_SPRITE 0x24E55C0u
#define K_IMAGE_SET_SPRITE 0x24D56D0u
#define K_IMAGE_GET_PPU 0x24E64A0u
#define K_GRAPHIC_SET_COLOR 0x24DE110u
#define K_GRAPHIC_GET_COLOR 0x24DE100u
#define K_TMP_SET_COLOR 0x2069670u
#define K_COMPONENT_GET_GAMEOBJECT 0x19AE220u
#define K_COMPONENT_GET_TRANSFORM 0x19AE1D0u
#define K_GAMEOBJECT_SET_ACTIVE 0x19AFD70u
#define K_TRANSFORM_GET_LOCALPOS 0x19BAB40u
#define K_TRANSFORM_SET_LOCALPOS 0x19BAC70u
#define K_TRANSFORM_SET_LOCALSCALE 0x19BD790u
#define K_STREAMING_ASSETS_PATH 0x17E7490u
#define K_PTR_TO_STRING_ANSI 0x20EEF30u
#define K_STRING_CONCAT2 0x2F1B580u
#define K_FILE_EXISTS 0x2045D00u
#define K_GCHANDLE_ALLOC 0x20EDF30u
#define K_GCHANDLE_GET_TARGET 0x20EDD10u
#define K_GCHANDLE_FREE 0x20EE000u
#define K_OBJECT_OP_IMPLICIT 0x19B3DC0u
#define K_OBJECT_SET_HIDEFLAGS 0x19B57E0u
#define HIDE_DONT_UNLOAD_UNUSED_ASSET 0x20

#define K_STAGECTRL_INITCOMMON 0x1CFAAD0u
#define K_STAGECTRL_STARTBATTLE 0x1CFBF10u
#define K_STAGECTRL_APPLY_CARD_PHASE 0x1D06410u
#define K_STAGECLASSINFO_GET_ID 0x1A41830u
#define K_STAGECLASSINFO_GET_STORYTYPE 0x1A53550u
#define K_SPECIALSTAGEIDS_IS_BLUEBAND 0x1A534C0u
#define K_SPECIALSTAGEIDS_IS_KETERFINAL 0x1A534E0u
#define K_LORID_OP_EQ_INT 0x1DA4280u
#define K_BATTLEUNIT_GET_UNITDATA 0x1EEF510u
#define K_BATTLEUNIT_IS_BREAK_LIFE_ZERO 0x1EF0990u
#define K_UNITDATA_GET_DEFAULTBOOK 0x1D26D40u
#define K_UNITDATA_GET_ENEMYUNITID 0x1D26F70u
#define K_BOOK_GET_CLASSINFO_ID 0x20AC560u
#define K_BUFLIST_HAS_ASSIMILATION 0x1B3E2F0u
#define K_BUF_IS_DESTROYED 0x1E25390u
#define K_DICECARD_GET_ID 0x2446BE0u
#define K_BATTLEOBJMGR_GET_INSTANCE 0x28807A0u
#define K_BATTLEOBJMGR_GET_ALIVE_LIST_FACTION 0x2885570u
#define K_SPEEDDICESETTER_GET_BY_INDEX 0x233B5B0u

#define F_VIEW 0x98
#define F_VIEW_MODEL 0x28
#define F_MODEL_VIEW 0x50
#define F_VIEW_SPEEDDICE_SETTER 0x1C0
#define F_MODEL_CARDSLOT_DETAIL 0x100
#define F_MODEL_BUFLIST_DETAIL 0x108
#define F_MODEL_PASSIVE_DETAIL 0x128
#define F_CARDSLOT_CARDARY 0x30
#define F_PLAYINGCARD_CARD 0x28
#define F_BUFLIST_BUFLIST 0x18
#define F_PASSIVEDETAIL_LIST 0x18
#define F_PASSIVE_DESTROYED 0x18
#define F_UNITBATTLEDATA_UNITDATA 0x10
#define F_UNITDATA_IS_SEPHIRAH 0xB8
#define F_STAGECTRL_STAGETYPE 0x18
#define F_STAGECTRL_CURRENT_FLOOR 0x20
#define F_STAGECTRL_CURRENT_WAVE 0x24
#define F_STAGECTRL_STAGEMODEL 0x10
#define F_STAGEMODEL_CLASSINFO 0x10
#define F_STAGECLASSINFO_CHAPTER 0x48
#define F_LORID_ID 0x10
#define F_LIST_ITEMS 0x10
#define F_LIST_SIZE 0x18
#define F_ARRAY_DATA 0x20
#define F_CLASS_NAME 0x10
#define F_CLASS_PARENT 0x58
#define F_STRING_LENGTH 0x10
#define F_STRING_CHARS 0x14

#define INFINITE_THRESHOLD 999
#define MAX_BASE 256
#define MAX_SCALED 512

typedef struct { float x, y; } Vector2;
typedef struct { float x, y, z; } Vector3;
typedef struct { float x, y, w, h; } Rect;
typedef struct { float r, g, b, a; } Color;
typedef struct { intptr_t handle; } GcHandle;

typedef struct { char name[96]; int tried, ok; GcHandle handle; } BaseEntry;
typedef struct { char name[96]; float ppu; GcHandle handle; } ScaledEntry;

static BaseEntry s_base[MAX_BASE]; static int s_base_count;
static ScaledEntry s_scaled[MAX_SCALED]; static int s_scaled_count;
static int s_log_budget = 40;
static GcHandle s_stage_handle; static int s_stage_handle_valid; static void *s_stage_raw;
static uint8_t *s_main;

// main's image start: the plugin's GetMainModuleInfo().m_Total.m_Start, so main + RVA is the game function
static uint8_t *main_base(void) {
    if (!s_main)
        for (int i = 0; i < g_nmod; i++)
            if (!strcmp(g_mod[i].name, "main") && !g_mod[i].nro) { s_main = g_mod[i].base; break; }
    return s_main;
}

#define FN(rva, ret, ...) ((ret (*)(__VA_ARGS__))(s_main + (rva)))

static void *field(void *self, uintptr_t off) { return self ? *(void *const *)((uintptr_t)self + off) : NULL; }
static int field_int(void *self, uintptr_t off) { return self ? *(const int *)((uintptr_t)self + off) : 0; }
static int field_bool(void *self, uintptr_t off) { return self ? *(const uint8_t *)((uintptr_t)self + off) != 0 : 0; }

static void *deref(GcHandle *h) { return FN(K_GCHANDLE_GET_TARGET, void *, GcHandle *, const void *)(h, NULL); }
static int alive(void *obj) { return obj ? FN(K_OBJECT_OP_IMPLICIT, int, void *, const void *)(obj, NULL) & 1 : 0; }
static void pin_asset(void *obj) {
    if (!alive(obj)) return;
    FN(K_OBJECT_SET_HIDEFLAGS, void, void *, int, const void *)(obj, HIDE_DONT_UNLOAD_UNUSED_ASSET, NULL);
}
static void free_handle(GcHandle *h) {
    if (h->handle) FN(K_GCHANDLE_FREE, void, GcHandle *, const void *)(h, NULL);
    h->handle = 0;
}
static GcHandle alloc_handle(void *obj) { return FN(K_GCHANDLE_ALLOC, GcHandle, void *, const void *)(obj, NULL); }
static void *sprite_texture(void *sprite) { return FN(K_SPRITE_GET_TEXTURE, void *, void *, const void *)(sprite, NULL); }

static void *load_base_sprite(const char *name) {
    BaseEntry *entry = NULL;
    for (int i = 0; i < s_base_count; i++)
        if (!strcmp(s_base[i].name, name)) { entry = &s_base[i]; break; }
    if (!entry) {
        if (s_base_count >= MAX_BASE) return NULL;
        entry = &s_base[s_base_count++];
        snprintf(entry->name, sizeof entry->name, "%s", name);
        entry->tried = 0; entry->ok = 0; entry->handle.handle = 0;
    }
    if (entry->tried) {
        if (!entry->ok) return NULL;
        void *cached = deref(&entry->handle);
        if (alive(cached)) return cached;
        if (s_log_budget-- > 0) LOG("[mod speed-dice] stale base sprite, reloading: %s", name);
        free_handle(&entry->handle);
        entry->ok = 0; entry->tried = 0;
    }
    entry->tried = 1;

    char rel[160];
    snprintf(rel, sizeof rel, "/SpeedDice/%s.png", name);

    void *dir = FN(K_STREAMING_ASSETS_PATH, void *, const void *)(NULL);
    void *tail = FN(K_PTR_TO_STRING_ANSI, void *, intptr_t, const void *)((intptr_t)rel, NULL);
    if (!dir || !tail) return NULL;
    void *path = FN(K_STRING_CONCAT2, void *, void *, void *, const void *)(dir, tail, NULL);
    if (!path) return NULL;
    if (!(FN(K_FILE_EXISTS, int, void *, const void *)(path, NULL) & 1)) {
        if (s_log_budget-- > 0) LOG("[mod speed-dice] missing: %s", rel);
        return NULL;
    }
    const Vector2 pivot = { 0.5f, 0.5f };
    void *sprite = FN(K_SPRITEUTIL_LOAD_SPRITE, void *, void *, Vector2, const void *)(path, pivot, NULL);
    if (!sprite) {
        if (s_log_budget-- > 0) LOG("[mod speed-dice] load failed: %s", rel);
        return NULL;
    }
    pin_asset(sprite);
    pin_asset(sprite_texture(sprite));
    entry->handle = alloc_handle(sprite);
    entry->ok = 1;
    if (s_log_budget-- > 0) LOG("[mod speed-dice] loaded: %s", rel);
    return sprite;
}

static void *get_scaled_sprite(const char *name, void *image) {
    if (!image) return NULL;
    const float ppu = FN(K_IMAGE_GET_PPU, float, void *, const void *)(image, NULL);
    int reuse = -1;
    for (int i = 0; i < s_scaled_count; i++) {
        if (s_scaled[i].ppu == ppu && !strcmp(s_scaled[i].name, name)) {
            void *cached = deref(&s_scaled[i].handle);
            if (alive(cached) && alive(sprite_texture(cached))) return cached;
            free_handle(&s_scaled[i].handle);
            reuse = i;
            break;
        }
    }
    void *base = load_base_sprite(name);
    if (!base) return NULL;
    void *texture = sprite_texture(base);
    if (!alive(texture)) return NULL;
    const int w = FN(K_TEXTURE_GET_WIDTH, int, void *, const void *)(texture, NULL);
    const int h = FN(K_TEXTURE_GET_HEIGHT, int, void *, const void *)(texture, NULL);
    const Rect rect = { 0.0f, 0.0f, (float)w, (float)h };
    const Vector2 pivot = { 0.5f, 0.5f };
    void *sprite = FN(K_SPRITE_CREATE, void *, void *, Rect, Vector2, float, uint32_t, int, const void *)(texture, rect, pivot, ppu, 0, 0, NULL);
    if (!sprite) return NULL;
    pin_asset(sprite);
    ScaledEntry *e = NULL;
    if (reuse >= 0) e = &s_scaled[reuse];
    else if (s_scaled_count < MAX_SCALED) e = &s_scaled[s_scaled_count++];
    if (e) {
        snprintf(e->name, sizeof e->name, "%s", name);
        e->ppu = ppu;
        e->handle = alloc_handle(sprite);
    }
    return sprite;
}

static int set_image_sprite(void *image, const char *name) {
    void *sprite = get_scaled_sprite(name, image);
    if (!sprite) return 0;
    if (FN(K_IMAGE_GET_SPRITE, void *, void *, const void *)(image, NULL) == sprite) return 1;
    FN(K_IMAGE_SET_SPRITE, void, void *, void *, const void *)(image, sprite, NULL);
    return 1;
}
static void set_graphic_color(void *g, Color c) { if (g) FN(K_GRAPHIC_SET_COLOR, void, void *, Color, const void *)(g, c, NULL); }
static void set_tmp_color(void *t, Color c) { if (t) FN(K_TMP_SET_COLOR, void, void *, Color, const void *)(t, c, NULL); }

static void change_speed_dice_color(const char *name, const char *suffix, Color color, void *ui) {
    if (!ui) return;
    char normal[96], glow[96], hovered[96];
    if (suffix && suffix[0]) snprintf(normal, sizeof normal, "%s_%s", name, suffix);
    else snprintf(normal, sizeof normal, "%s", name);
    snprintf(glow, sizeof glow, "%s_Glow", name);
    snprintf(hovered, sizeof hovered, "%s_Hovered", name);

    set_image_sprite(field(ui, F_NORMAL_FRAME), normal);
    set_image_sprite(field(ui, F_LIGHT_FRAME), glow);
    set_image_sprite(field(ui, F_HIGHLIGHT_FRAME), hovered);

    set_graphic_color(field(ui, F_TXT_SPEED_RANGE), color);
    set_graphic_color(field(ui, F_ROULETTE_IMG), color);
    set_tmp_color(field(ui, F_TXT_SPEED_MAX), color);
    set_graphic_color(field(ui, F_TENS_NUM), color);
    set_graphic_color(field(ui, F_UNITS_NUM), color);

    Color dim = color;
    dim.a -= 0.6f;
    set_graphic_color(field(ui, F_BREAKED_FRAME), dim);
    set_graphic_color(field(ui, F_LOCKED_FRAME), dim);
    set_graphic_color(field(ui, F_LOCKED_ICON), dim);
    set_graphic_color(field(ui, F_BREAKED_LINEAR_DODGE), dim);
}

static int string_equals(void *str, const char *ascii) {
    if (!str) return 0;
    const int length = field_int(str, F_STRING_LENGTH);
    const uint16_t *chars = (const uint16_t *)((uintptr_t)str + F_STRING_CHARS);
    int i = 0;
    for (; ascii[i]; i++)
        if (i >= length || chars[i] != (uint16_t)(unsigned char)ascii[i]) return 0;
    return i == length;
}
static int list_count(void *list) { return field_int(list, F_LIST_SIZE); }
static void *list_at(void *list, int index) {
    if (!list || index < 0 || index >= list_count(list)) return NULL;
    void *items = field(list, F_LIST_ITEMS);
    if (!items) return NULL;
    return *(void *const *)((uintptr_t)items + F_ARRAY_DATA + sizeof(void *) * (size_t)index);
}
static int is_instance_of(void *obj, const char *class_name) {
    if (!obj) return 0;
    void *klass = *(void *const *)obj;
    for (; klass; klass = field(klass, F_CLASS_PARENT)) {
        const char *n = *(const char *const *)((uintptr_t)klass + F_CLASS_NAME);
        if (n && !strcmp(n, class_name)) return 1;
    }
    return 0;
}
static int lor_id_is(void *id, int value) {
    return id ? FN(K_LORID_OP_EQ_INT, int, void *, int, const void *)(id, value, NULL) & 1 : 0;
}
static void *unit_data_of(void *model) {
    if (!model) return NULL;
    void *battle = FN(K_BATTLEUNIT_GET_UNITDATA, void *, void *, const void *)(model, NULL);
    return field(battle, F_UNITBATTLEDATA_UNITDATA);
}
static void *unit_enemy_id(void *unit_data) { return FN(K_UNITDATA_GET_ENEMYUNITID, void *, void *, const void *)(unit_data, NULL); }
static int enemy_unit_id_is(void *unit, int value) {
    void *ud = unit_data_of(unit);
    return ud ? lor_id_is(unit_enemy_id(ud), value) : 0;
}
static void *story_type_of(void *stage) {
    void *ci = field(field(stage, F_STAGECTRL_STAGEMODEL), F_STAGEMODEL_CLASSINFO);
    return ci ? FN(K_STAGECLASSINFO_GET_STORYTYPE, void *, void *, const void *)(ci, NULL) : NULL;
}
static void *current_stage(void) { return s_stage_handle_valid ? deref(&s_stage_handle) : NULL; }
static void cache_stage(void *stage) {
    if (!stage || (s_stage_handle_valid && stage == s_stage_raw)) return;
    if (s_stage_handle_valid) free_handle(&s_stage_handle);
    s_stage_handle = alloc_handle(stage);
    s_stage_handle_valid = 1;
    s_stage_raw = stage;
}

// ---- the rules (speeddice_rules.hpp) ----
enum { FACTION_ENEMY = 0, FACTION_PLAYER = 1, STAGETYPE_INVITATION = 0, STAGETYPE_CREATURE = 1 };

typedef struct {
    void *ui; int faction; void *stage, *class_info, *model, *unit_data, *story; int story_fetched;
} env_t;

static void env_init(env_t *e, void *ui, int faction, void *stage) {
    memset(e, 0, sizeof *e);
    e->ui = ui; e->faction = faction; e->stage = stage;
    e->class_info = field(field(stage, F_STAGECTRL_STAGEMODEL), F_STAGEMODEL_CLASSINFO);
    e->model = field(field(ui, F_VIEW), F_VIEW_MODEL);
    e->unit_data = unit_data_of(e->model);
}
static void *e_stage_id(env_t *e) { return e->class_info ? FN(K_STAGECLASSINFO_GET_ID, void *, void *, const void *)(e->class_info, NULL) : NULL; }
static void *e_default_book(env_t *e) { return e->unit_data ? FN(K_UNITDATA_GET_DEFAULTBOOK, void *, void *, const void *)(e->unit_data, NULL) : NULL; }
static int e_wave(env_t *e) { return field_int(e->stage, F_STAGECTRL_CURRENT_WAVE); }
static int e_floor(env_t *e) { return field_int(e->stage, F_STAGECTRL_CURRENT_FLOOR); }
static int e_stage_type(env_t *e) { return field_int(e->stage, F_STAGECTRL_STAGETYPE); }
static int e_chapter(env_t *e) { return field_int(e->class_info, F_STAGECLASSINFO_CHAPTER); }
static int e_stage_id_value(env_t *e) { return field_int(e_stage_id(e), F_LORID_ID); }
static int e_stage_id_is(env_t *e, int v) { return lor_id_is(e_stage_id(e), v); }
static int e_keter_final(env_t *e) {
    void *id = e_stage_id(e);
    return id ? FN(K_SPECIALSTAGEIDS_IS_KETERFINAL, int, void *, const void *)(id, NULL) & 1 : 0;
}
static int e_blue_band(int id) { return FN(K_SPECIALSTAGEIDS_IS_BLUEBAND, int, int, const void *)(id, NULL) & 1; }
static int e_story_is(env_t *e, const char *v) {
    if (!e->story_fetched) { e->story = story_type_of(e->stage); e->story_fetched = 1; }
    return string_equals(e->story, v);
}
static int e_has_assimilation(env_t *e) {
    void *bufs = field(e->model, F_MODEL_BUFLIST_DETAIL);
    return bufs ? FN(K_BUFLIST_HAS_ASSIMILATION, int, void *, const void *)(bufs, NULL) & 1 : 0;
}
static int e_has_buf(env_t *e, const char *class_name) {
    void *list = field(field(e->model, F_MODEL_BUFLIST_DETAIL), F_BUFLIST_BUFLIST);
    const int count = list_count(list);
    for (int i = 0; i < count; i++) {
        void *buf = list_at(list, i);
        if (!buf || !is_instance_of(buf, class_name)) continue;
        if (!(FN(K_BUF_IS_DESTROYED, int, void *, const void *)(buf, NULL) & 1)) return 1;
    }
    return 0;
}
static int e_has_passive(env_t *e, const char *class_name) {
    void *list = field(field(e->model, F_MODEL_PASSIVE_DETAIL), F_PASSIVEDETAIL_LIST);
    const int count = list_count(list);
    for (int i = 0; i < count; i++) {
        void *p = list_at(list, i);
        if (!p || !is_instance_of(p, class_name)) continue;
        if (!field_bool(p, F_PASSIVE_DESTROYED)) return 1;
    }
    return 0;
}
static int e_is_sephirah(env_t *e) { return field_bool(e->unit_data, F_UNITDATA_IS_SEPHIRAH); }
static int e_book_valid(env_t *e) { return e_default_book(e) != NULL; }
static int e_default_book_is(env_t *e, int v) {
    void *book = e_default_book(e);
    return book ? lor_id_is(FN(K_BOOK_GET_CLASSINFO_ID, void *, void *, const void *)(book, NULL), v) : 0;
}
static int e_enemy_unit_is(env_t *e, int v) { return e->unit_data ? lor_id_is(unit_enemy_id(e->unit_data), v) : 0; }

static void emit(env_t *e, const char *name, const char *suffix, float r, float g, float b) {
    Color c = { r, g, b, 1.0f };
    change_speed_dice_color(name, suffix, c, e->ui);
}

typedef struct { const char *story, *name; float r, g, b; } StoryStyle;
typedef struct { int chapter; const char *story, *name; } ChapterStyle;

static const StoryStyle k_story_styles[] = {
    { "Rats", "Rats", 0.7f, 0.5f, 0.4f },
    { "YunsOffice", "YunOffice", 0.7f, 0.7f, 0.7f },
    { "TheBrotherhoodofIron", "BrotherhoodofIron", 0.7f, 0.5f, 0.4f },
    { "HookOffice", "HookOffice", 0.7f, 0.5f, 0.4f },
    { "PierresMeatPies", "PierreBistro", 0.7f, 0.5f, 0.4f },
    { "StreetlightOffice", "StreetlightOffice", 0.4f, 0.5f, 0.7f },
    { "ZweiAssociation", "ZweiAssociation", 0.6f, 0.6f, 0.8f },
    { "MolarOffice", "MolarOffice", 0.8f, 0.8f, 0.8f },
    { "TheStrayDogs", "StrayDogs", 0.6f, 0.4f, 0.4f },
    { "TheCarnival", "TheCarnival", 0.7f, 0.7f, 0.7f },
    { "FullStopOffice", "FullstopOffice", 0.6f, 0.3f, 0.3f },
    { "DawnOffice", "DawnOffice", 0.8f, 0.3f, 0.0f },
    { "GazeOffice", "GazeOffice", 0.7f, 0.7f, 0.7f },
    { "Tomerry", "LoveTown", 1.0f, 0.3f, 0.7f },
    { "KurokumoClan", "KurokumoClan", 0.7f, 0.7f, 0.7f },
    { "TheMusiciansofBremen", "Bremen", 0.6f, 0.1f, 0.9f },
    { "ShiAssociation", "ShiAssociation", 0.6f, 0.2f, 0.1f },
    { "Puppeteer", "Puppeteer", 0.6f, 0.2f, 0.1f },
    { "TheEightOClockCircus", "Circus", 0.8f, 0.2f, 0.1f },
    { "Sweepers", "Sweeper", 0.7f, 0.7f, 0.7f },
    { "TheIndex", "Index", 0.8f, 0.8f, 0.8f },
    { "WarpCrew", "WarpCrew", 0.4f, 0.4f, 1.0f },
    { "SmilingFaces", "SmilingFace", 0.7f, 0.7f, 0.7f },
    { "CryingChild", "CryingChildren", 0.5f, 0.3f, 0.2f },
    { "LiuAssociation2", "LiuAssociation", 0.7f, 0.3f, 0.1f },
    { "LiuAssociation1", "LiuAssociation", 0.7f, 0.3f, 0.1f },
    { "TheThumb", "TheThumb", 0.7f, 0.4f, 0.4f },
    { "CaneOffice", "CaneOffice", 0.8f, 0.8f, 0.8f },
    { "TheBlueReverberation", "BlueReverberation", 0.3f, 0.4f, 1.0f },
    { "BlackSilence", "BlackSilence", 0.8f, 0.8f, 0.8f },
    { "Final", "TheHead", 0.8f, 0.65f, 0.0f },
    { "Roland", "BlackSilence", 0.8f, 0.8f, 0.8f },
    { "Argalia", "BlueReverberation", 0.3f, 0.4f, 1.0f },
    { "HeadArbiter", "TheHead", 0.8f, 0.65f, 0.0f },
    { "GambleMod", "GambleMod", 0.9f, 0.4f, 0.0f },
    { "MagicalGirl", "MagicalGirl", 0.6f, 0.1f, 0.9f },
};
static const ChapterStyle k_chapter_styles[] = {
    { 1, "Chapter1", "Canard" }, { 2, "Chapter2", "UrbanMyth" }, { 3, "Chapter3", "UrbanLegend" }, { 4, "Chapter4", "UrbanPlague" },
    { 5, "Chapter5", "UrbanNightmare" }, { 6, "Chapter6", "StaroftheCity" }, { 7, "Chapter7", "Impurity" },
};
static const char *const k_blue_band_floors[] = { "Malkuth", "Yesod", "Hod", "Netzach", "Tiphereth", "Gebura", "Chesed", "Binah", "Hokma", "Keter" };
#define BLUE_BAND_FIRST_ID 70001
#define BLUE_BAND_COUNT 10
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void decide_player(env_t *e) {
    if (e_has_assimilation(e) && !e_has_passive(e, "PassiveAbility_250422")) {
        emit(e, "Abnormalities", NULL, 0.7f, 0.5f, 0.4f);
        return;
    }
    switch (e_floor(e)) {
        case 1: emit(e, "Malkuth", NULL, 0.7f, 0.5f, 0.2f); break;
        case 2: emit(e, "Yesod", NULL, 0.8f, 0.8f, 1.0f); break;
        case 3: emit(e, "Hod", NULL, 1.0f, 0.4f, 0.0f); break;
        case 4: emit(e, "Netzach", NULL, 0.3f, 1.0f, 0.3f); break;
        case 5: emit(e, "Tiphereth", NULL, 1.0f, 0.9f, 0.01f); break;
        case 6:
            if ((e_has_passive(e, "PassiveAbility_250122") || e_has_passive(e, "PassiveAbility_250022")) && e_is_sephirah(e))
                emit(e, "RedMist", e_has_buf(e, "BattleUnitBuf_RedMistEgo") ? "EGO" : NULL, 0.9f, 0.4f, 0.4f);
            else
                emit(e, "Gebura", NULL, 0.8f, 0.4f, 0.4f);
            break;
        case 7: emit(e, "Chesed", NULL, 0.3f, 0.5f, 1.0f); break;
        case 8: emit(e, "Binah", NULL, 0.8f, 0.65f, 0.0f); break;
        case 9: emit(e, "Hokma", NULL, 0.7f, 0.7f, 0.7f); break;
        case 10:
            if (e_has_passive(e, "PassiveAbility_10012") && e_is_sephirah(e))
                emit(e, "BlackSilence", NULL, 0.8f, 0.8f, 0.8f);
            else
                emit(e, "Keter", NULL, 0.9f, 0.9f, 0.9f);
            break;
        default: break;
    }
    if (e_stage_id_is(e, 60007)) {
        if (!e_book_valid(e)) return;
        if (e_default_book_is(e, 10)) emit(e, "BlackSilence", NULL, 0.8f, 0.8f, 0.8f);
        else if (e_default_book_is(e, 6)) emit(e, "RedMist", e_has_buf(e, "BattleUnitBuf_RedMistEgo") ? "EGO" : NULL, 0.9f, 0.4f, 0.4f);
        else emit(e, "Binah", NULL, 0.8f, 0.65f, 0.0f);
        return;
    }
    if (e_stage_id_is(e, 210005) || e_keter_final(e)) {
        if (e_is_sephirah(e)) emit(e, "Angela", NULL, 0.1f, 0.7f, 0.8f);
        else emit(e, "Keter", NULL, 0.9f, 0.9f, 0.9f);
        return;
    }
    if (e_story_is(e, "Roland") || e_story_is(e, "Argalia")) {
        if (!e_book_valid(e)) return;
        if (e_default_book_is(e, 9100501)) emit(e, "Angela", NULL, 0.1f, 0.7f, 0.8f);
        else emit(e, "BlackSilence", NULL, 0.8f, 0.8f, 0.8f);
    }
}

static void decide_enemy_invitation(env_t *e) {
    const int stage_id = e_stage_id_value(e);
    if (e_blue_band(stage_id)) {
        const int k = stage_id - BLUE_BAND_FIRST_ID;
        if (k >= 0 && k < BLUE_BAND_COUNT) emit(e, "BlueReverberation", k_blue_band_floors[k], 0.3f, 0.4f, 1.0f);
        return;
    }
    if (stage_id >= 60004 && stage_id <= 60006) { emit(e, "BlueReverberation", NULL, 0.3f, 0.4f, 1.0f); return; }
    for (int i = 0; i < COUNT(k_story_styles); i++) {
        const StoryStyle *s = &k_story_styles[i];
        if (e_story_is(e, s->story)) { emit(e, s->name, NULL, s->r, s->g, s->b); return; }
    }
    if (e_story_is(e, "WedgeOffice")) {
        if (e_wave(e) == 1) {
            if (e_enemy_unit_is(e, 30027)) emit(e, "DawnOffice", NULL, 0.8f, 0.3f, 0.0f);
            else emit(e, "WedgeOffice", NULL, 0.6f, 0.5f, 0.01f);
        } else emit(e, "PhilipEGO", NULL, 0.9f, 0.3f, 0.0f);
        return;
    }
    if (e_story_is(e, "TheIndex2")) {
        if (e_enemy_unit_is(e, 51001) || e_enemy_unit_is(e, 51002)) emit(e, "Index", NULL, 0.3f, 0.5f, 0.8f);
        else if (e_has_buf(e, "BattleUnitBuf_indexRelease")) emit(e, "Index", "Proxies", 0.3f, 0.5f, 0.8f);
        else emit(e, "Index", "ProxiesChained", 0.3f, 0.5f, 0.8f);
        return;
    }
    if (e_story_is(e, "TheRedMist")) {
        emit(e, "RedMist", e_has_buf(e, "BattleUnitBuf_RedMistEgo") ? "EGO" : NULL, 0.9f, 0.1f, 0.1f);
        return;
    }
    if (e_story_is(e, "RCorp1")) {
        if (e_wave(e) == 1) emit(e, "RRR", "Reindeer", 0.9f, 0.4f, 0.0f);
        else if (e_wave(e) == 2) emit(e, "RRR", "Rhino", 0.9f, 0.4f, 0.0f);
        else if (e_enemy_unit_is(e, 50023)) emit(e, "RRR", NULL, 0.9f, 0.4f, 0.0f);
        else emit(e, "RRR", "Rabbit", 0.9f, 0.4f, 0.0f);
        return;
    }
    if (e_story_is(e, "RCorp2")) {
        if (e_wave(e) == 1) emit(e, "RRR", "Reindeer", 0.9f, 0.4f, 0.0f);
        else if (e_wave(e) == 2) emit(e, "RRR", "Rhino", 0.9f, 0.4f, 0.0f);
        else if (e_wave(e) == 3) emit(e, "RRR", "Rabbit", 0.9f, 0.4f, 0.0f);
        else if (e_enemy_unit_is(e, 50032)) emit(e, "RRR", "Rabbit", 0.9f, 0.4f, 0.0f);
        else if (e_enemy_unit_is(e, 50033)) emit(e, "RRR", "Rhino", 0.9f, 0.4f, 0.0f);
        else if (e_enemy_unit_is(e, 50034)) emit(e, "RRR", "Reindeer", 0.9f, 0.4f, 0.0f);
        else emit(e, "RRR", NULL, 0.9f, 0.4f, 0.0f);
        return;
    }
    if (e_story_is(e, "ThePurpleTear")) {
        if (e_has_buf(e, "BattleUnitBuf_purpleSlash")) emit(e, "PurpleTear", "Slash", 0.6f, 0.1f, 0.9f);
        else if (e_has_buf(e, "BattleUnitBuf_purplePenetrate")) emit(e, "PurpleTear", "Pierce", 0.6f, 0.1f, 0.9f);
        else if (e_has_buf(e, "BattleUnitBuf_purpleHit")) emit(e, "PurpleTear", "Blunt", 0.6f, 0.1f, 0.9f);
        else if (e_has_buf(e, "BattleUnitBuf_purpleDefense")) emit(e, "PurpleTear", "Guard", 0.6f, 0.1f, 0.9f);
        else emit(e, "PurpleTear", NULL, 0.6f, 0.1f, 0.9f);
        return;
    }
    if (e_story_is(e, "Shyao")) {
        if (e_wave(e) == 1) emit(e, "LiuAssociation", NULL, 0.7f, 0.3f, 0.1f);
        else if (e_wave(e) == 2) emit(e, "Xiao", NULL, 0.7f, 0.3f, 0.1f);
        return;
    }
    if (e_story_is(e, "Yan")) {
        if (e_enemy_unit_is(e, 50051)) emit(e, "Yan", "Main", 0.1f, 0.7f, 0.9f);
        else emit(e, "Yan", NULL, 0.1f, 0.7f, 0.9f);
        return;
    }
    if (e_story_is(e, "HanaAssociation")) {
        if (e_has_buf(e, "BattleUnitBuf_hana1")) emit(e, "Hana", "Geon", 0.8f, 0.8f, 0.8f);
        else if (e_has_buf(e, "BattleUnitBuf_hana2")) emit(e, "Hana", "Gon", 0.8f, 0.8f, 0.8f);
        else if (e_has_buf(e, "BattleUnitBuf_hana3")) emit(e, "Hana", "Gam", 0.8f, 0.8f, 0.8f);
        else if (e_has_buf(e, "BattleUnitBuf_hana4")) emit(e, "Hana", "Ri", 0.8f, 0.8f, 0.8f);
        else emit(e, "Hana", NULL, 0.8f, 0.8f, 0.8f);
        return;
    }
    const int chapter = e_chapter(e);
    for (int i = 0; i < COUNT(k_chapter_styles); i++) {
        const ChapterStyle *c = &k_chapter_styles[i];
        if (chapter == c->chapter && e_story_is(e, c->story)) { emit(e, c->name, NULL, 0.7f, 0.4f, 0.2f); return; }
    }
}

static void decide_enemy(env_t *e) {
    if (e_stage_id_is(e, 210005) || e_keter_final(e)) { emit(e, "LobotomyCorporation", NULL, 1.0f, 0.4f, 0.4f); return; }
    const int stage_type = e_stage_type(e);
    if (stage_type == STAGETYPE_CREATURE) { emit(e, "Abnormalities", NULL, 0.7f, 0.5f, 0.4f); return; }
    if (stage_type != STAGETYPE_INVITATION) return;
    decide_enemy_invitation(e);
}

static void apply_dice_style(void *ui, int faction) {
    if (!ui) return;
    void *stage = current_stage();
    if (!stage) {
        if (s_log_budget-- > 0) LOG("[mod speed-dice] stage controller not captured yet");
        return;
    }
    env_t env;
    env_init(&env, ui, faction, stage);
    if (faction == FACTION_PLAYER) decide_player(&env);
    else if (faction == FACTION_ENEMY) decide_enemy(&env);
}

// ---- card phase and the infinity die ----
static int find_card_index(void *card_ary, const int *ids, int id_count) {
    const int count = list_count(card_ary);
    for (int i = 0; i < count; i++) {
        void *card = field(list_at(card_ary, i), F_PLAYINGCARD_CARD);
        if (!card) continue;
        void *card_id = FN(K_DICECARD_GET_ID, void *, void *, const void *)(card, NULL);
        for (int k = 0; k < id_count; k++)
            if (lor_id_is(card_id, ids[k])) return i;
    }
    return -1;
}

static void style_dice_at_card(void *unit, const int *ids, int id_count, const char *name, const char *suffix, Color color) {
    void *card_ary = field(field(unit, F_MODEL_CARDSLOT_DETAIL), F_CARDSLOT_CARDARY);
    const int index = find_card_index(card_ary, ids, id_count);
    if (index < 0) return;
    void *setter = field(field(unit, F_MODEL_VIEW), F_VIEW_SPEEDDICE_SETTER);
    if (!setter) return;
    void *dice = FN(K_SPEEDDICESETTER_GET_BY_INDEX, void *, void *, int, const void *)(setter, index, NULL);
    if (!dice) return;
    change_speed_dice_color(name, suffix, color, dice);
}

static void apply_card_phase_styles(void *stage) {
    void *story = story_type_of(stage);
    if (!story) return;
    const int shyao = string_equals(story, "Shyao");
    const int index2 = !shyao && string_equals(story, "TheIndex2");
    const int hana = !shyao && !index2 && string_equals(story, "HanaAssociation");
    if (!shyao && !index2 && !hana) return;

    void *manager = FN(K_BATTLEOBJMGR_GET_INSTANCE, void *, const void *)(NULL);
    if (!manager) return;
    void *units = FN(K_BATTLEOBJMGR_GET_ALIVE_LIST_FACTION, void *, void *, int, const void *)(manager, FACTION_ENEMY, NULL);
    const int unit_count = list_count(units);

    if (shyao) {
        static const int ids[] = { 610012, 610013 };
        for (int i = 0; i < unit_count; i++) {
            void *unit = list_at(units, i);
            if (!unit) continue;
            if (!enemy_unit_id_is(unit, 50038)) continue;
            if (FN(K_BATTLEUNIT_IS_BREAK_LIFE_ZERO, int, void *, const void *)(unit, NULL) & 1) return;
            style_dice_at_card(unit, ids, 2, "Xiao", "ReversedScale", (Color){ 0.7f, 0.3f, 0.1f, 1.0f });
            return;
        }
        return;
    }
    for (int i = 0; i < unit_count; i++) {
        void *unit = list_at(units, i);
        if (!unit) continue;
        if (index2) {
            static const int ids[] = { 605001, 605002, 605003 };
            style_dice_at_card(unit, ids, 3, "Index", "ProxiesSpecial", (Color){ 0.3f, 0.5f, 0.8f, 1.0f });
        } else {
            static const int ids[] = { 701001 };
            style_dice_at_card(unit, ids, 1, "Hana", NULL, (Color){ 0.8f, 0.8f, 0.8f, 1.0f });
        }
    }
}

static void apply_infinite(void *ui, int value) {
    if (!ui || value < INFINITE_THRESHOLD) return;
    void *tens = field(ui, F_TENS_NUM), *units = field(ui, F_UNITS_NUM), *max_text = field(ui, F_TXT_SPEED_MAX);
    if (!tens) return;
    void *sprite = get_scaled_sprite("Infinite", tens);
    if (!sprite) return;
    if (FN(K_IMAGE_GET_SPRITE, void *, void *, const void *)(tens, NULL) == sprite) return;
    if (max_text) {
        void *go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(max_text, NULL);
        if (go) FN(K_GAMEOBJECT_SET_ACTIVE, void, void *, int, const void *)(go, 0, NULL);
    }
    FN(K_IMAGE_SET_SPRITE, void, void *, void *, const void *)(tens, sprite, NULL);
    void *tf = FN(K_COMPONENT_GET_TRANSFORM, void *, void *, const void *)(tens, NULL);
    if (tf) {
        Vector3 pos = FN(K_TRANSFORM_GET_LOCALPOS, Vector3, void *, const void *)(tf, NULL);
        pos.x += 13.0f;
        FN(K_TRANSFORM_SET_LOCALPOS, void, void *, Vector3, const void *)(tf, pos, NULL);
        const Vector3 scale = { 0.95f, 0.85f, 0.0f };
        FN(K_TRANSFORM_SET_LOCALSCALE, void, void *, Vector3, const void *)(tf, scale, NULL);
    }
    if (units) {
        Color c = FN(K_GRAPHIC_GET_COLOR, Color, void *, const void *)(units, NULL);
        set_graphic_color(tens, c);
    }
    void *tens_go = FN(K_COMPONENT_GET_GAMEOBJECT, void *, void *, const void *)(tens, NULL);
    if (tens_go) FN(K_GAMEOBJECT_SET_ACTIVE, void, void *, int, const void *)(tens_go, 1, NULL);
}

// ---- gates ----
static int g_hits[4];
static const char *const g_gate_name[4] = { "stage_pre", "card_phase_pre", "dice_init_post", "dice_sprite_post" };
static int first_hits(int which) { return g_hits[which]++ < 4; }

static void stage_pre_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    cache_stage((void *)r[0]);
    if (first_hits(0)) LOG("[mod speed-dice] %s: StageController %#llx", g_gate_name[0], (unsigned long long)r[0]);
}
static void card_phase_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    cache_stage((void *)r[0]);
    apply_card_phase_styles((void *)r[0]);
    if (first_hits(1)) LOG("[mod speed-dice] %s: StageController %#llx", g_gate_name[1], (unsigned long long)r[0]);
}
static void dice_init_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    apply_dice_style((void *)r[23], (int)(uint32_t)r[27]);
    if (first_hits(2)) LOG("[mod speed-dice] %s: SpeedDiceUI %#llx faction %d", g_gate_name[2], (unsigned long long)r[23], (int)(uint32_t)r[27]);
}
static void dice_sprite_run(uint64_t *r, int param) {
    (void)param;
    if (!main_base()) return;
    apply_infinite((void *)r[19], (int)(uint32_t)r[20]);
    if (first_hits(3)) LOG("[mod speed-dice] %s: SpeedDiceUI %#llx value %d", g_gate_name[3], (unsigned long long)r[19], (int)(uint32_t)r[20]);
}
// Read once, at the first gate hit: the launcher's choices are in the environment by then, but not yet when constructors run
static int mod_enabled(void) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("DM_MOD_SPEEDDICE");
        on = v && v[0] == '1';
        LOG("[mod speed-dice] %s (DM_MOD_SPEEDDICE=%s)", on ? "on" : "off", v ? v : "unset");
    }
    return on;
}
static void on_stage_pre(uint64_t *r, int p) { if (!mod_enabled()) return; dm_mod_gate_run(r, p, stage_pre_run); }
static void on_card_phase_pre(uint64_t *r, int p) { if (!mod_enabled()) return; dm_mod_gate_run(r, p, card_phase_run); }
static void on_dice_init_post(uint64_t *r, int p) { if (!mod_enabled()) return; dm_mod_gate_run(r, p, dice_init_run); }
static void on_dice_sprite_post(uint64_t *r, int p) { if (!mod_enabled()) return; dm_mod_gate_run(r, p, dice_sprite_run); }

__attribute__((constructor)) static void mod_speed_dice_attach(void) {
    gate_obs_set(MODGATE_SPEED_DICE_STAGE_PRE, on_stage_pre);
    gate_obs_set(MODGATE_SPEED_DICE_DICE_INIT_POST, on_dice_init_post);
    gate_obs_set(MODGATE_SPEED_DICE_DICE_SPRITE_POST, on_dice_sprite_post);
    gate_obs_set(MODGATE_SPEED_DICE_CARD_PHASE_PRE, on_card_phase_pre);
}
