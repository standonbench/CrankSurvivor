#include "game.h"

// ---------------------------------------------------------------------------
// Shorthand macros for Playdate C API drawing (color as last param)
// ---------------------------------------------------------------------------
#define GFX_CLEAR(c)           pd->graphics->clear((c))
#define GFX_FILL(x,y,w,h,c)   pd->graphics->fillRect((x),(y),(w),(h),(c))
#define GFX_RECT(x,y,w,h,c)   pd->graphics->drawRect((x),(y),(w),(h),(c))
#define GFX_ELLIPSE(x,y,w,h,c) pd->graphics->fillEllipse((x),(y),(w),(h),0,360,(c))
#define GFX_CIRCLE(x,y,w,h,lw,c) pd->graphics->drawEllipse((x),(y),(w),(h),(lw),0,360,(c))
#define GFX_PUSH(bmp)          pd->graphics->pushContext((bmp))
#define GFX_POP()              pd->graphics->popContext()

// ---------------------------------------------------------------------------
// Forward declarations for helpers
// ---------------------------------------------------------------------------
static void dither_fill(int x, int y, int w, int h, int phase);
static void dither_ellipse(int cx, int cy, int rx, int ry, int density);

// ---------------------------------------------------------------------------
// All procedural bitmaps
// ---------------------------------------------------------------------------
static LCDBitmap* enemyImgs[ENEMY_TYPE_COUNT][2]; // 2 animation frames each
static LCDBitmap* playerFrames[4];
static LCDBitmap* bulletImgs[4]; // 0=bullet, 1=harpoon, 2=ghost_light, 3=chain_bolt
static LCDBitmap* enemyBulletImg;
static LCDBitmap* xpGemImg;
static LCDBitmap* xpGemImgSmall;
static LCDBitmap* anchorImg;
static LCDBitmap* crateImg;
static LCDBitmap* tileImgs[8];
static LCDBitmap* weaponIconImgs[WEAPON_COUNT];
static LCDBitmap* weaponIconLargeImgs[WEAPON_COUNT];
static LCDBitmap* passiveIconImgs[5];
static LCDBitmap* passiveIconLargeImgs[5];
static LCDBitmap* pickupImgs[PICKUP_TYPE_COUNT];
static LCDBitmap* depthChargeImg;
static LCDBitmap* vortexMask;
static LCDBitmap* rippleMask;
static LCDBitmap* boltSprite;
static LCDBitmap* wispSprite;
static LCDBitmap* dropShadow;
static LCDBitmap* vignetteMask;

// 6 character designs x 4 walk frames each
#define DESIGN_COUNT 6
static LCDBitmap* designFrames[DESIGN_COUNT][4];

// Enemy design alternatives: 2 per type (original + 1 redesign), 2 animation frames each
#define ENEMY_DESIGN_COUNT 2
static LCDBitmap* enemyDesignImgs[ENEMY_TYPE_COUNT][ENEMY_DESIGN_COUNT][2]; // [type][design][frame]
static const char* enemyDesignLabels[ENEMY_DESIGN_COUNT] = {
    "Original", "Redesign"
};
static const char* designNames[DESIGN_COUNT] = {
    "Original",
    "Weathered Mariner",
    "The Hermit",
    "Old Captain",
    "Storm Walker",
    "The Beacon"
};

static int enemyHW[ENEMY_TYPE_COUNT] = { 15, 21, 18, 27, 24, 17, 20, 23 };
static int enemyHH[ENEMY_TYPE_COUNT] = { 15, 21, 18, 27, 24, 17, 20, 23 };

static LCDBitmap* make_bitmap(int w, int h)
{
    return pd->graphics->newBitmap(w, h, kColorClear);
}

// ---------------------------------------------------------------------------
// Tile images (16x16 each)
// ---------------------------------------------------------------------------
static void create_tile_images(void)
{
    // Types 0-3: Random noise dots (natural ground texture)
    for (int t = 0; t < 4; t++) {
        tileImgs[t] = make_bitmap(TILE_SIZE, TILE_SIZE);
        GFX_PUSH(tileImgs[t]);
        GFX_CLEAR(kColorBlack);
        int count = 6 + t * 2;
        for (int i = 0; i < count; i++)
            GFX_FILL(rng_range(1, TILE_SIZE-2), rng_range(1, TILE_SIZE-2), 1, 1, kColorWhite);
        GFX_POP();
    }

    // Wall tile (type 4)
    tileImgs[4] = make_bitmap(TILE_SIZE, TILE_SIZE);
    GFX_PUSH(tileImgs[4]);
    GFX_CLEAR(kColorBlack);
    GFX_FILL(0, 6, TILE_SIZE, 1, kColorWhite);
    GFX_FILL(0, 15, TILE_SIZE, 1, kColorWhite);
    GFX_FILL(12, 0, 1, 6, kColorWhite);
    GFX_FILL(0, 6, 1, 9, kColorWhite);
    GFX_FILL(18, 6, 1, 9, kColorWhite);
    GFX_FILL(6, 15, 1, 9, kColorWhite);
    GFX_POP();

    // Dark dither (type 5)
    tileImgs[5] = make_bitmap(TILE_SIZE, TILE_SIZE);
    GFX_PUSH(tileImgs[5]);
    GFX_CLEAR(kColorBlack);
    for (int i = 0; i < 6; i++) {
        GFX_FILL(rng_range(0, TILE_SIZE-1), rng_range(0, TILE_SIZE-1), 1, 1, kColorWhite);
    }
    GFX_POP();

    // Sand ripples (type 6)
    tileImgs[6] = make_bitmap(TILE_SIZE, TILE_SIZE);
    GFX_PUSH(tileImgs[6]);
    GFX_CLEAR(kColorBlack);
    for (int y = 5; y < 20; y += 7) {
        int off = rng_range(-3, 3);
        for (int x = 2; x < 22; x++) {
            if ((x + y + off) % 4 == 0) GFX_FILL(x, y + off, 1, 1, kColorWhite);
        }
    }
    GFX_POP();

    // Debris/Cracks (type 7)
    tileImgs[7] = make_bitmap(TILE_SIZE, TILE_SIZE);
    GFX_PUSH(tileImgs[7]);
    GFX_CLEAR(kColorBlack);
    for (int i = 0; i < 4; i++) {
        int rx = rng_range(2, 22);
        int ry = rng_range(2, 22);
        GFX_FILL(rx, ry, 2, 1, kColorWhite);
        GFX_FILL(rx, ry, 1, 2, kColorWhite);
    }
    GFX_POP();
}

// ---------------------------------------------------------------------------
// Enemy sprites — 2 animation frames each
// Based on original detailed designs with added idle animation
// ---------------------------------------------------------------------------
static void create_enemy_images(void)
{
    // ---- CREEPER (30x30) — "Trench Skitterer" crustacean ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_CREEPER][f] = make_bitmap(30, 30);
    GFX_PUSH(enemyImgs[ENEMY_CREEPER][f]);
    // Core body (carapace)
    GFX_ELLIPSE(5, 4, 20, 16, kColorWhite);
    GFX_FILL(7, 6, 16, 12, kColorBlack);
    // Carapace ridges
    GFX_FILL(9, 8, 12, 1, kColorWhite);
    GFX_FILL(11, 11, 8, 1, kColorWhite);
    // Eyes cluster — shift slightly between frames
    int eyeShift = f ? 1 : 0;
    GFX_FILL(11+eyeShift, 15, 2, 2, kColorWhite);
    GFX_FILL(14+eyeShift, 14, 2, 2, kColorWhite);
    GFX_FILL(17+eyeShift, 15, 2, 2, kColorWhite);
    // Legs — alternate positions for scuttling
    int lf = f ? 2 : 0;
    // Left legs
    pd->graphics->drawLine(6, 8, 1+lf, 5-lf, 1, kColorWhite);
    pd->graphics->drawLine(1+lf, 5-lf, 0+lf, 10-lf, 1, kColorWhite);
    pd->graphics->drawLine(5, 12, 0-lf, 12+lf, 1, kColorWhite);
    pd->graphics->drawLine(0-lf, 12+lf, 1-lf, 17+lf, 1, kColorWhite);
    pd->graphics->drawLine(6, 15, 2+lf, 18-lf, 1, kColorWhite);
    pd->graphics->drawLine(2+lf, 18-lf, 3+lf, 23-lf, 1, kColorWhite);
    // Left pincer
    pd->graphics->drawLine(9, 20, 6-lf, 25+lf, 1, kColorWhite);
    pd->graphics->drawLine(6-lf, 25+lf, 4-lf, 28+lf, 1, kColorWhite);
    pd->graphics->drawLine(6-lf, 25+lf, 9-lf, 28+lf, 1, kColorWhite);
    // Right legs
    pd->graphics->drawLine(24, 8, 29-lf, 5-lf, 1, kColorWhite);
    pd->graphics->drawLine(29-lf, 5-lf, 30-lf, 10-lf, 1, kColorWhite);
    pd->graphics->drawLine(25, 12, 30+lf, 12+lf, 1, kColorWhite);
    pd->graphics->drawLine(30+lf, 12+lf, 29+lf, 17+lf, 1, kColorWhite);
    pd->graphics->drawLine(24, 15, 28-lf, 18-lf, 1, kColorWhite);
    pd->graphics->drawLine(28-lf, 18-lf, 27-lf, 23-lf, 1, kColorWhite);
    // Right pincer
    pd->graphics->drawLine(21, 20, 24+lf, 25+lf, 1, kColorWhite);
    pd->graphics->drawLine(24+lf, 25+lf, 26+lf, 28+lf, 1, kColorWhite);
    pd->graphics->drawLine(24+lf, 25+lf, 21+lf, 28+lf, 1, kColorWhite);
    GFX_POP();
    }

    // ---- TENDRIL (44x44) — "Drowned Sailor" skull with jellyfish tentacles ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_TENDRIL][f] = make_bitmap(44, 44);
    GFX_PUSH(enemyImgs[ENEMY_TENDRIL][f]);
    // Skull top
    GFX_FILL(14, 4, 16, 12, kColorWhite);
    // Skull cheekbones
    GFX_FILL(12, 10, 4, 8, kColorWhite);
    GFX_FILL(28, 10, 4, 8, kColorWhite);
    GFX_FILL(18, 16, 8, 8, kColorWhite); // Upper jaw
    // Eye sockets (deep black)
    GFX_FILL(16, 10, 4, 5, kColorBlack);
    GFX_FILL(24, 10, 4, 5, kColorBlack);
    // Glinting pupils inside sockets — shift per frame
    int pupOff = f ? 1 : 0;
    GFX_FILL(17+pupOff, 12, 2, 2, kColorWhite);
    GFX_FILL(25+pupOff, 12, 2, 2, kColorWhite);
    // Nose cavity
    GFX_FILL(21, 16, 2, 3, kColorBlack);
    // Jellyfish bell over skull
    pd->graphics->drawEllipse(4, 0, 36, 20, 1, 180, 0, kColorWhite);
    GFX_FILL(4, 10, 10, 1, kColorWhite);
    GFX_FILL(30, 10, 10, 1, kColorWhite);
    // Bell texture arcs
    pd->graphics->drawEllipse(10, 2, 24, 12, 1, 200, 340, kColorWhite);
    pd->graphics->drawEllipse(14, 3, 16, 8, 1, 210, 330, kColorWhite);
    // Flowing tentacles — sway between frames
    int sw = f ? 2 : -2;
    for (int i = 0; i < 5; i++) {
        int tx = 10 + i * 6;
        int ty = 18 + (i%2)*2;
        int len = 16 + (i%3)*2;
        int sway = (i%2 == 0) ? sw : -sw;
        pd->graphics->drawLine(tx, ty, tx - 2 + (i%2)*4 + sway, ty + len, 1, kColorWhite);
        GFX_FILL(tx - 3 + (i%2)*4 + sway, ty + len/2, 2, 2, kColorWhite);
        GFX_FILL(tx - 4 + (i%2)*4 + sway, ty + len, 2, 2, kColorWhite);
    }
    GFX_POP();
    }

    // ---- WRAITH (40x40) — "Abyssal Banshee" shrieking face with torn cloak ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_WRAITH][f] = make_bitmap(40, 40);
    GFX_PUSH(enemyImgs[ENEMY_WRAITH][f]);
    // Hollow hood silhouette
    GFX_ELLIPSE(6, 2, 28, 26, kColorWhite);
    GFX_FILL(8, 4, 24, 22, kColorBlack);
    // Shrieking face (skull-like, stretched)
    GFX_FILL(14, 8, 12, 16, kColorWhite);
    // Deep black eyes, slanted — shift per frame (darting)
    int ed = f ? 1 : 0;
    pd->graphics->drawLine(15+ed, 10, 18+ed, 12, 2, kColorBlack);
    pd->graphics->drawLine(25-ed, 10, 22-ed, 12, 2, kColorBlack);
    // Shrieking mouth — opens wider on frame 1
    int mouthH = f ? 9 : 7;
    GFX_FILL(18, 15, 4, mouthH, kColorBlack);
    // Tattered cloak draped from hood — sway per frame
    int cs = f ? 2 : 0;
    GFX_FILL(4-cs, 18, 5, 14, kColorWhite);
    GFX_FILL(31+cs, 18, 5, 14, kColorWhite);
    GFX_FILL(8, 22, 4, 10, kColorWhite);
    GFX_FILL(28, 22, 4, 10, kColorWhite);
    GFX_FILL(12, 26, 16, 6, kColorWhite);
    // Horizontal fade stripes for tattered robe bottom — shift per frame
    GFX_FILL(6-cs, 32, 28+cs*2, 1, kColorWhite);
    GFX_FILL(8, 34, 24, 1, kColorWhite);
    GFX_FILL(12+cs, 36, 16-cs, 1, kColorWhite);
    GFX_FILL(16-cs, 38, 8+cs, 1, kColorWhite);
    GFX_POP();
    }

    // ---- ABYSSAL (56x56) — "Leviathan Maw" angler bursting from below ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_ABYSSAL][f] = make_bitmap(56, 56);
    GFX_PUSH(enemyImgs[ENEMY_ABYSSAL][f]);
    // Massive jaw silhouette — opens slightly on frame 1
    int jo = f ? 2 : 0;
    pd->graphics->fillPolygon(5, (int[]){12,56+jo, 44,56+jo, 52,24-jo, 28,12-jo, 4,24-jo}, kColorWhite, kPolygonFillNonZero);
    GFX_FILL(16, 18-jo, 24, 34+jo*2, kColorBlack); // Deep black mouth cavity
    // Rows of jagged teeth (White on black)
    // Upper row
    pd->graphics->fillPolygon(3, (int[]){18,18-jo, 22,28, 26,18-jo}, kColorWhite, kPolygonFillNonZero);
    pd->graphics->fillPolygon(3, (int[]){26,18-jo, 30,28, 34,18-jo}, kColorWhite, kPolygonFillNonZero);
    pd->graphics->fillPolygon(3, (int[]){34,18-jo, 38,28, 42,18-jo}, kColorWhite, kPolygonFillNonZero);
    // Lower row
    pd->graphics->fillPolygon(3, (int[]){20,52+jo, 24,38, 28,52+jo}, kColorWhite, kPolygonFillNonZero);
    pd->graphics->fillPolygon(3, (int[]){28,52+jo, 32,38, 36,52+jo}, kColorWhite, kPolygonFillNonZero);
    // Head stalk & Glowing Lure (Anglerfish part)
    pd->graphics->drawLine(28, 12-jo, 28, 2, 2, kColorWhite);
    pd->graphics->drawEllipse(24, 0, 8, 8, 2, 0, 360, kColorWhite);
    // Lure flicker on frame 1
    if (f) {
        GFX_FILL(22, 2, 2, 4, kColorWhite);
        GFX_FILL(30, 2, 2, 4, kColorWhite);
        GFX_FILL(26, 0, 4, 1, kColorWhite);
        GFX_FILL(26, 7, 4, 1, kColorWhite);
    }
    // Tiny deep-set eyes — with glinting pupils
    GFX_ELLIPSE(10, 24-jo, 6, 6, kColorBlack);
    GFX_FILL(12, 26-jo, 2, 2, kColorWhite);
    GFX_ELLIPSE(40, 24-jo, 6, 6, kColorBlack);
    GFX_FILL(42, 26-jo, 2, 2, kColorWhite);
    // Scale texture on flanks
    pd->graphics->drawLine(8, 30, 8, 44, 1, kColorBlack);
    pd->graphics->drawLine(12, 34, 12, 50, 1, kColorBlack);
    pd->graphics->drawLine(44, 30, 44, 44, 1, kColorBlack);
    pd->graphics->drawLine(48, 34, 48, 50, 1, kColorBlack);
    GFX_POP();
    }

    // ---- SEER (48x48) — "Watcher Entity" giant eyeball ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_SEER][f] = make_bitmap(48, 48);
    GFX_PUSH(enemyImgs[ENEMY_SEER][f]);
    // Optic nerve stalks — sway per frame
    int ss = f ? 3 : 0;
    pd->graphics->drawLine(24, 24, 4+ss, 8-ss, 2, kColorWhite);
    pd->graphics->drawLine(24, 24, 44-ss, 6+ss, 2, kColorWhite);
    pd->graphics->drawLine(24, 24, 6-ss, 40+ss, 2, kColorWhite);
    pd->graphics->drawLine(24, 24, 42+ss, 42-ss, 2, kColorWhite);
    // Eye buds at stalk ends
    pd->graphics->fillEllipse(2+ss, 6-ss, 6, 6, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(42-ss, 4+ss, 6, 6, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(4-ss, 38+ss, 6, 6, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(40+ss, 40-ss, 6, 6, 0, 360, kColorWhite);
    // Main eyeball
    GFX_ELLIPSE(6, 6, 36, 36, kColorWhite);
    // Vein lines
    pd->graphics->drawLine(10, 14, 20, 20, 1, kColorBlack);
    pd->graphics->drawLine(38, 14, 28, 20, 1, kColorBlack);
    pd->graphics->drawLine(10, 34, 20, 28, 1, kColorBlack);
    pd->graphics->drawLine(38, 34, 28, 28, 1, kColorBlack);
    // Iris ring
    GFX_ELLIPSE(14, 14, 20, 20, kColorBlack);
    // Iris texture (cross lines)
    pd->graphics->drawLine(24, 16, 24, 32, 1, kColorWhite);
    pd->graphics->drawLine(16, 24, 32, 24, 1, kColorWhite);
    pd->graphics->drawLine(18, 18, 30, 30, 1, kColorWhite);
    pd->graphics->drawLine(30, 18, 18, 30, 1, kColorWhite);
    // Deep pupil — shifts position (looking around)
    int px = f ? 26 : 22;
    int py = f ? 22 : 26;
    pd->graphics->fillEllipse(px-6, py-6, 12, 12, 0, 360, kColorBlack);
    // Sharp glint
    pd->graphics->fillEllipse(px-2, py-4, 4, 4, 0, 360, kColorWhite);
    GFX_POP();
    }

    // ---- LAMPREY (36x36) — "Horrifying Maw" concentric rings of teeth ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_LAMPREY][f] = make_bitmap(36, 36);
    GFX_PUSH(enemyImgs[ENEMY_LAMPREY][f]);
    // Outer fleshy ring
    pd->graphics->drawEllipse(2, 2, 32, 32, 2, 0, 360, kColorWhite);
    // Inner fleshy ring
    pd->graphics->drawEllipse(6, 6, 24, 24, 2, 0, 360, kColorWhite);
    // Black abyss center
    pd->graphics->fillEllipse(10, 10, 16, 16, 0, 360, kColorBlack);
    // Outer tooth layer — rotate offset between frames
    int rotOff = f * 22;
    for (int a = rotOff; a < 360 + rotOff; a += 45) {
        float radOuter = 12.0f;
        float radInner = 8.0f;
        int ox = 18 + (int)(radOuter * cosf((float)a * 3.14159f / 180.0f));
        int oy = 18 + (int)(radOuter * sinf((float)a * 3.14159f / 180.0f));
        int ix = 18 + (int)(radInner * cosf((float)a * 3.14159f / 180.0f));
        int iy = 18 + (int)(radInner * sinf((float)a * 3.14159f / 180.0f));
        pd->graphics->drawLine(ox, oy, ix, iy, 2, kColorWhite);
    }
    // Inner tooth layer (finer)
    for (int a = 22 + rotOff; a < 360 + 22 + rotOff; a += 45) {
        float radOuter = 8.0f;
        float radInner = 4.0f;
        int ox = 18 + (int)(radOuter * cosf((float)a * 3.14159f / 180.0f));
        int oy = 18 + (int)(radOuter * sinf((float)a * 3.14159f / 180.0f));
        int ix = 18 + (int)(radInner * cosf((float)a * 3.14159f / 180.0f));
        int iy = 18 + (int)(radInner * sinf((float)a * 3.14159f / 180.0f));
        pd->graphics->drawLine(ox, oy, ix, iy, 1, kColorWhite);
    }
    // Outer spikes (4 cardinal points) — pulse per frame
    int spk = f ? 2 : 0;
    for (int a = 0; a < 360; a += 90) {
        int sx = 18 + (int)((18+spk) * cosf((float)a * 3.14159f / 180.0f));
        int sy = 18 + (int)((18+spk) * sinf((float)a * 3.14159f / 180.0f));
        GFX_FILL(sx-1, sy-1, 3, 3, kColorWhite);
    }
    GFX_POP();
    }

    // ---- BLOAT (44x44) — "Putrid Swell" rotting husk wrapped in chains ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_BLOAT][f] = make_bitmap(44, 44);
    GFX_PUSH(enemyImgs[ENEMY_BLOAT][f]);
    // Massive bloated spherical body — swells on frame 1
    int sw = f ? 1 : 0;
    pd->graphics->fillEllipse(2-sw, 4-sw, 38+sw*2, 34+sw*2, 0, 360, kColorWhite);
    // Large dark festering pustules
    pd->graphics->fillEllipse(4, 8, 8, 8, 0, 360, kColorBlack);
    GFX_FILL(6, 10, 4, 4, kColorWhite); // pustule glint
    pd->graphics->fillEllipse(28, 6, 9, 9, 0, 360, kColorBlack);
    GFX_FILL(30, 8, 4, 4, kColorWhite);
    pd->graphics->fillEllipse(16, 24, 7, 7, 0, 360, kColorBlack);
    GFX_FILL(18, 26, 3, 3, kColorWhite);
    // Leaking fluid lines (dark cracks)
    pd->graphics->drawLine(10, 16, 20, 28, 1, kColorBlack);
    pd->graphics->drawLine(22, 10, 14, 22, 1, kColorBlack);
    pd->graphics->drawLine(32, 14, 28, 24, 1, kColorBlack);
    // Binding straps (2 diagonal lines)
    pd->graphics->drawLine(4, 30, 38, 36, 2, kColorBlack);
    pd->graphics->drawLine(6, 36, 36, 30, 2, kColorBlack);
    // Small angry eyes — glint shifts per frame
    GFX_FILL(14, 14, 4, 4, kColorBlack);
    GFX_FILL(15+f, 15, 2, 2, kColorWhite);
    GFX_FILL(24, 13, 4, 4, kColorBlack);
    GFX_FILL(25+f, 14, 2, 2, kColorWhite);
    // Wide leering smile
    GFX_FILL(12, 22, 2, 1, kColorBlack); GFX_FILL(16, 21, 2, 1, kColorBlack);
    GFX_FILL(20, 22, 2, 1, kColorBlack); GFX_FILL(24, 21, 2, 1, kColorBlack);
    // Oozing drips on frame 1
    if (f) {
        GFX_FILL(20, 28, 1, 5, kColorBlack);
        GFX_FILL(14, 22, 1, 4, kColorBlack);
        GFX_FILL(28, 24, 1, 5, kColorBlack);
    }
    GFX_POP();
    }

    // ---- HARBINGER (52x52) — "Deep King" eldritch deity with coral crown ----
    for (int f = 0; f < 2; f++) {
    enemyImgs[ENEMY_HARBINGER][f] = make_bitmap(52, 52);
    GFX_PUSH(enemyImgs[ENEMY_HARBINGER][f]);
    // Tall robe/cloak silhouette
    pd->graphics->fillPolygon(5, (int[]){10,52, 42,52, 46,24, 26,16, 6,24}, kColorWhite, kPolygonFillNonZero);
    // Inner dark void
    pd->graphics->fillPolygon(5, (int[]){14,52, 38,52, 40,26, 26,20, 12,26}, kColorBlack, kPolygonFillNonZero);
    // Segmented armor plating on torso — shift per frame (breathing)
    int bp = f ? 1 : 0;
    GFX_FILL(15, 28+bp, 22, 2, kColorWhite);
    GFX_FILL(16, 32+bp, 20, 2, kColorWhite);
    GFX_FILL(17, 36+bp, 18, 2, kColorWhite);
    GFX_FILL(18, 40+bp, 16, 2, kColorWhite);
    // Head (skull-like)
    GFX_FILL(18, 12, 16, 12, kColorWhite);
    GFX_FILL(20, 14, 12, 10, kColorBlack); // hollow inside
    // Jagged Coral Crown (3 spikes)
    pd->graphics->drawLine(22, 12, 19, 2, 2, kColorWhite);
    pd->graphics->drawLine(26, 11, 26, 0, 2, kColorWhite);
    pd->graphics->drawLine(30, 12, 33, 2, 2, kColorWhite);
    // Crown gems
    GFX_FILL(18, 1, 3, 3, kColorWhite);
    GFX_FILL(25, 0, 3, 3, kColorWhite);
    GFX_FILL(32, 1, 3, 3, kColorWhite);
    // Piercing hollow eyes — glint shifts per frame
    GFX_FILL(21, 16, 4, 5, kColorWhite);
    GFX_FILL(22+f, 17, 2, 3, kColorBlack);
    GFX_FILL(27, 16, 4, 5, kColorWhite);
    GFX_FILL(28+f, 17, 2, 3, kColorBlack);
    // Claw arms — extend/retract between frames
    int armExt = f ? 2 : 0;
    // Left claw
    pd->graphics->drawLine(10, 28, 2-armExt, 22-armExt, 2, kColorWhite);
    pd->graphics->drawLine(2-armExt, 22-armExt, 0-armExt, 17-armExt, 1, kColorWhite);
    pd->graphics->drawLine(2-armExt, 22-armExt, 5-armExt, 16-armExt, 1, kColorWhite);
    // Right claw
    pd->graphics->drawLine(42, 28, 50+armExt, 22-armExt, 2, kColorWhite);
    pd->graphics->drawLine(50+armExt, 22-armExt, 52+armExt, 17-armExt, 1, kColorWhite);
    pd->graphics->drawLine(50+armExt, 22-armExt, 47+armExt, 16-armExt, 1, kColorWhite);
    GFX_POP();
    }
}

// ---------------------------------------------------------------------------
// Player frames (32x32, 4 frames)
// ---------------------------------------------------------------------------
static void create_player_images(void)
{
    // The Hermit — hooded cloak silhouette, staff with lantern, peering eyes
    for (int f = 0; f < 4; f++) {
        playerFrames[f] = make_bitmap(30, 30);
        GFX_PUSH(playerFrames[f]);
        int bob = (f == 1 || f == 3) ? 1 : 0;

        // Staff with lantern (right side)
        GFX_FILL(23, 0, 1, 28, kColorWhite);
        // Lantern at top of staff
        GFX_FILL(21, 0, 5, 1, kColorWhite);
        GFX_RECT(21, 1, 5, 5, kColorWhite);
        GFX_FILL(22, 2, 3, 3, kColorWhite);
        GFX_FILL(23, 3, 1, 1, kColorBlack);
        // Flickering glow halo
        {
            int glowSz = (f == 0) ? 2 : (f == 2) ? 1 : 3;
            dither_fill(21 - glowSz, 0, glowSz, 6, f);
            dither_fill(26, 0, glowSz, 6, f);
            dither_fill(21 - glowSz/2, 6, 5 + glowSz, 1 + glowSz/2, f + 1);
            if (glowSz >= 2) {
                dither_fill(22, -1, 3, 1, f);
            }
        }

        // Round hood (smooth dome shape)
        pd->graphics->fillEllipse(4, 1+bob, 18, 16, 0, 360, kColorWhite);
        pd->graphics->fillEllipse(6, 3+bob, 14, 12, 0, 360, kColorBlack);
        pd->graphics->drawEllipse(4, 1+bob, 18, 16, 1, 180, 360, kColorWhite);

        // Two bright eyes deep inside the round hood
        GFX_FILL(10, 8+bob, 2, 2, kColorWhite);
        GFX_FILL(15, 8+bob, 2, 2, kColorWhite);

        // Long flowing cloak
        pd->graphics->drawLine(6, 13+bob, 4, 27, 1, kColorWhite);
        pd->graphics->drawLine(20, 13+bob, 22, 27, 1, kColorWhite);
        GFX_FILL(5, 27, 18, 2, kColorWhite);
        // Fill cloak interior
        for (int py = 13; py < 27; py++) {
            int leftX = 6 - (py - 13) * 2 / 14;
            int rightX = 20 + (py - 13) * 2 / 14;
            leftX = clampi(leftX, 4, 20);
            rightX = clampi(rightX, 6, 22);
            GFX_FILL(leftX+1, py+bob, rightX - leftX - 1, 1, kColorBlack);
        }
        // Ragged bottom edge
        GFX_FILL(5, 28, 2, 1, kColorWhite);
        GFX_FILL(9, 29, 2, 1, kColorWhite);
        GFX_FILL(15, 28, 2, 1, kColorWhite);
        GFX_FILL(19, 29, 2, 1, kColorWhite);

        // Hand wraps holding staff
        GFX_FILL(21, 14+bob, 4, 3, kColorWhite);
        GFX_FILL(22, 15+bob, 2, 1, kColorBlack);

        // Subtle foot movement at cloak base
        int step = (f == 1) ? 1 : (f == 3) ? -1 : 0;
        GFX_FILL(8+step, 28, 3, 2, kColorWhite);
        GFX_FILL(15-step, 28, 3, 2, kColorWhite);

        GFX_POP();
    }
}
// ---------------------------------------------------------------------------
// Bullet / gem / pickup images
// ---------------------------------------------------------------------------
static void create_misc_images(void)
{
    // Light Streak (12x4, replace old bullet)
    bulletImgs[0] = make_bitmap(12, 4);
    GFX_PUSH(bulletImgs[0]);
    GFX_FILL(2, 1, 8, 2, kColorWhite); // Bright core
    GFX_FILL(0, 1, 2, 2, kColorWhite); // Flicker head
    GFX_FILL(10, 1, 2, 2, kColorWhite); // Tail
    GFX_POP();

    // Harpoon of the Deep (14x4, reinforced)
    bulletImgs[1] = make_bitmap(14, 4);
    GFX_PUSH(bulletImgs[1]);
    GFX_FILL(0, 1, 12, 2, kColorWhite); // Shaft
    GFX_FILL(10, 0, 4, 4, kColorWhite); // Heavy Barb
    GFX_POP();

    // Ghost light (6x6)
    bulletImgs[2] = make_bitmap(6, 6);
    GFX_PUSH(bulletImgs[2]); GFX_ELLIPSE(0, 0, 6, 6, kColorWhite); GFX_POP();

    // Chain bolt (3x8 zigzag)
    bulletImgs[3] = make_bitmap(3, 8);
    GFX_PUSH(bulletImgs[3]);
    GFX_FILL(0, 0, 2, 2, kColorWhite);
    GFX_FILL(1, 2, 2, 2, kColorWhite);
    GFX_FILL(0, 4, 2, 2, kColorWhite);
    GFX_FILL(1, 6, 2, 2, kColorWhite);
    GFX_POP();

    // Depth charge (8x8 circle with crosshair)
    depthChargeImg = make_bitmap(8, 8);
    GFX_PUSH(depthChargeImg);
    pd->graphics->drawEllipse(0, 0, 8, 8, 1, 0, 360, kColorWhite);
    GFX_FILL(3, 0, 2, 8, kColorWhite);
    GFX_FILL(0, 3, 8, 2, kColorWhite);
    GFX_POP();

    // Enemy bullet (9x9 diamond)
    enemyBulletImg = make_bitmap(9, 9);
    GFX_PUSH(enemyBulletImg);
    GFX_FILL(3, 0, 3, 9, kColorWhite);
    GFX_FILL(0, 3, 9, 3, kColorWhite);
    GFX_POP();

    // XP gem (8x8, diamond shaped, subtle)
    xpGemImg = make_bitmap(8, 8);
    GFX_PUSH(xpGemImg);
    pd->graphics->drawLine(4, 0, 8, 4, 1, kColorWhite);
    pd->graphics->drawLine(8, 4, 4, 8, 1, kColorWhite);
    pd->graphics->drawLine(4, 8, 0, 4, 1, kColorWhite);
    pd->graphics->drawLine(0, 4, 4, 0, 1, kColorWhite);
    GFX_FILL(3, 3, 2, 2, kColorWhite); // Core glint
    GFX_POP();

    // XP gem small (4x4)
    xpGemImgSmall = make_bitmap(4, 4);
    GFX_PUSH(xpGemImgSmall);
    GFX_FILL(1, 0, 2, 4, kColorWhite);
    GFX_FILL(0, 1, 4, 2, kColorWhite);
    GFX_POP();

    // Anchor (12x12)
    anchorImg = make_bitmap(12, 12);
    GFX_PUSH(anchorImg);
    GFX_FILL(5, 0, 3, 9, kColorWhite);
    GFX_FILL(2, 8, 9, 3, kColorWhite);
    GFX_FILL(0, 5, 2, 5, kColorWhite);
    GFX_FILL(11, 5, 1, 5, kColorWhite);
    GFX_POP();

    // Crate (18x18, Nautical/Industrial refined design)
    crateImg = make_bitmap(18, 18);
    GFX_PUSH(crateImg);
    GFX_RECT(0, 0, 18, 18, kColorWhite);
    // Wood slats / reinforced frame
    GFX_FILL(3, 0, 2, 18, kColorWhite);
    GFX_FILL(13, 0, 2, 18, kColorWhite);
    GFX_FILL(0, 3, 18, 2, kColorWhite);
    GFX_FILL(0, 13, 18, 2, kColorWhite);
    // Corner rivets (black dots on white frame)
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    GFX_FILL(1, 1, 1, 1, kColorBlack);
    GFX_FILL(16, 1, 1, 1, kColorBlack);
    GFX_FILL(1, 16, 1, 1, kColorBlack);
    GFX_FILL(16, 16, 1, 1, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    // Center lock/seal
    GFX_FILL(7, 7, 4, 4, kColorWhite);
    GFX_POP();

    // Weapon icons (15x15) — unique per weapon
    // Weapon icons (15x15) — small UI icons (simple shapes)
    for (int i = 0; i < WEAPON_COUNT; i++) {
        weaponIconImgs[i] = make_bitmap(15, 15);
        GFX_PUSH(weaponIconImgs[i]);
        switch (i) {
        case WEAPON_SIGNAL_BEAM:
            pd->graphics->drawEllipse(0, 0, 15, 15, 1, 0, 360, kColorWhite);
            pd->graphics->drawEllipse(4, 4, 7, 7, 1, 0, 360, kColorWhite);
            break;
        case WEAPON_TIDE_POOL:
            // Orbiting orbs look
            pd->graphics->drawEllipse(4, 4, 7, 7, 1, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(6, 0, 3, 3, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(12, 10, 3, 3, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(0, 10, 3, 3, 0, 360, kColorWhite);
            break;
        case WEAPON_HARPOON:
            pd->graphics->drawLine(0, 14, 14, 0, 1, kColorWhite);
            pd->graphics->drawLine(10, 0, 14, 0, 1, kColorWhite);
            pd->graphics->drawLine(14, 0, 14, 4, 1, kColorWhite);
            break;
        case WEAPON_BRINE_SPLASH:
            // Sunburst / Aura look
            pd->graphics->drawEllipse(2, 2, 11, 11, 1, 0, 360, kColorWhite);
            pd->graphics->drawLine(7, 0, 7, 15, 1, kColorWhite);
            pd->graphics->drawLine(0, 7, 15, 7, 1, kColorWhite);
            pd->graphics->drawLine(3, 3, 12, 12, 1, kColorWhite);
            pd->graphics->drawLine(12, 3, 3, 12, 1, kColorWhite);
            break;
        case WEAPON_GHOST_LIGHT:
            pd->graphics->fillEllipse(3, 3, 9, 9, 0, 360, kColorWhite);
            GFX_FILL(7, 0, 1, 3, kColorWhite);
            GFX_FILL(7, 12, 1, 3, kColorWhite);
            GFX_FILL(0, 7, 3, 1, kColorWhite);
            GFX_FILL(12, 7, 3, 1, kColorWhite);
            break;
        case WEAPON_ANCHOR_DROP:
            GFX_FILL(6, 0, 3, 1, kColorWhite);
            GFX_FILL(7, 1, 1, 10, kColorWhite);
            GFX_FILL(3, 11, 9, 1, kColorWhite);
            GFX_FILL(2, 9, 3, 3, kColorWhite);
            GFX_FILL(10, 9, 3, 3, kColorWhite);
            break;
        case WEAPON_FOGHORN:
            GFX_FILL(0, 5, 4, 5, kColorWhite);
            GFX_FILL(4, 4, 2, 7, kColorWhite);
            pd->graphics->drawEllipse(7, 3, 3, 9, 1, 270, 90, kColorWhite);
            pd->graphics->drawEllipse(11, 2, 3, 11, 1, 270, 90, kColorWhite);
            break;
        case WEAPON_CHAIN_LIGHTNING:
            pd->graphics->drawLine(7, 0, 4, 5, 1, kColorWhite);
            pd->graphics->drawLine(4, 5, 11, 7, 1, kColorWhite);
            pd->graphics->drawLine(11, 7, 7, 14, 1, kColorWhite);
            break;
        case WEAPON_RIPTIDE:
            pd->graphics->drawEllipse(1, 1, 13, 13, 1, 0, 270, kColorWhite);
            pd->graphics->drawEllipse(4, 4, 7, 7, 1, 180, 450, kColorWhite);
            break;
        case WEAPON_DEPTH_CHARGE:
            pd->graphics->fillEllipse(3, 3, 9, 9, 0, 360, kColorWhite);
            GFX_FILL(7, 0, 1, 3, kColorWhite);
            GFX_FILL(7, 12, 1, 3, kColorWhite);
            GFX_FILL(0, 7, 3, 1, kColorWhite);
            GFX_FILL(12, 7, 3, 1, kColorWhite);
            break;
        }
        GFX_POP();
    }

    // Large weapon icons (24x24) — for armory/upgrade screens (simple shapes)
    for (int i = 0; i < WEAPON_COUNT; i++) {
        weaponIconLargeImgs[i] = make_bitmap(24, 24);
        GFX_PUSH(weaponIconLargeImgs[i]);
        switch (i) {
        case WEAPON_SIGNAL_BEAM:
            pd->graphics->drawEllipse(0, 0, 24, 24, 2, 0, 360, kColorWhite);
            pd->graphics->drawEllipse(6, 6, 12, 12, 1, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(10, 10, 4, 4, 0, 360, kColorWhite);
            break;
        case WEAPON_TIDE_POOL:
            // Orbiting orbs look (large)
            pd->graphics->drawEllipse(6, 6, 12, 12, 2, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(10, 0, 4, 4, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(20, 18, 4, 4, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(0, 18, 4, 4, 0, 360, kColorWhite);
            break;
        case WEAPON_HARPOON:
            pd->graphics->drawLine(2, 22, 22, 2, 2, kColorWhite);
            pd->graphics->drawLine(14, 2, 22, 2, 2, kColorWhite);
            pd->graphics->drawLine(22, 2, 22, 10, 2, kColorWhite);
            break;
        case WEAPON_BRINE_SPLASH:
            // Sunburst / Aura look (large)
            pd->graphics->drawEllipse(4, 4, 16, 16, 2, 0, 360, kColorWhite);
            pd->graphics->drawLine(12, 0, 12, 24, 2, kColorWhite);
            pd->graphics->drawLine(0, 12, 24, 12, 2, kColorWhite);
            pd->graphics->drawLine(5, 5, 19, 19, 1, kColorWhite);
            pd->graphics->drawLine(19, 5, 5, 19, 1, kColorWhite);
            break;
        case WEAPON_GHOST_LIGHT:
            pd->graphics->fillEllipse(6, 6, 12, 12, 0, 360, kColorWhite);
            GFX_FILL(11, 0, 3, 5, kColorWhite);
            GFX_FILL(11, 20, 3, 4, kColorWhite);
            GFX_FILL(0, 11, 5, 3, kColorWhite);
            GFX_FILL(20, 11, 4, 3, kColorWhite);
            GFX_FILL(3, 3, 2, 2, kColorWhite);
            GFX_FILL(20, 3, 2, 2, kColorWhite);
            GFX_FILL(3, 20, 2, 2, kColorWhite);
            GFX_FILL(20, 20, 2, 2, kColorWhite);
            break;
        case WEAPON_ANCHOR_DROP:
            GFX_FILL(9, 0, 6, 3, kColorWhite);
            GFX_FILL(11, 3, 3, 14, kColorWhite);
            GFX_FILL(5, 15, 15, 3, kColorWhite);
            GFX_FILL(3, 12, 5, 5, kColorWhite);
            GFX_FILL(17, 12, 5, 5, kColorWhite);
            GFX_FILL(6, 8, 12, 2, kColorWhite);
            GFX_FILL(11, 20, 3, 3, kColorWhite);
            break;
        case WEAPON_FOGHORN:
            GFX_FILL(0, 8, 8, 9, kColorWhite);
            GFX_FILL(8, 6, 3, 12, kColorWhite);
            pd->graphics->drawEllipse(12, 5, 5, 15, 1, 270, 90, kColorWhite);
            pd->graphics->drawEllipse(17, 3, 4, 18, 1, 270, 90, kColorWhite);
            pd->graphics->drawEllipse(21, 2, 3, 21, 1, 270, 90, kColorWhite);
            break;
        case WEAPON_CHAIN_LIGHTNING:
            pd->graphics->drawLine(11, 0, 6, 8, 2, kColorWhite);
            pd->graphics->drawLine(6, 8, 16, 10, 2, kColorWhite);
            pd->graphics->drawLine(16, 10, 8, 18, 2, kColorWhite);
            pd->graphics->drawLine(8, 18, 13, 24, 2, kColorWhite);
            GFX_FILL(9, 0, 5, 3, kColorWhite);
            break;
        case WEAPON_RIPTIDE:
            pd->graphics->drawEllipse(2, 2, 20, 20, 1, 0, 270, kColorWhite);
            pd->graphics->drawEllipse(5, 5, 14, 14, 1, 90, 360, kColorWhite);
            pd->graphics->drawEllipse(8, 8, 8, 8, 1, 180, 450, kColorWhite);
            GFX_FILL(10, 10, 4, 4, kColorWhite);
            break;
        case WEAPON_DEPTH_CHARGE:
            pd->graphics->fillEllipse(5, 5, 14, 14, 0, 360, kColorWhite);
            GFX_FILL(10, 0, 4, 5, kColorWhite);
            GFX_FILL(10, 19, 4, 5, kColorWhite);
            GFX_FILL(0, 10, 5, 4, kColorWhite);
            GFX_FILL(19, 10, 5, 4, kColorWhite);
            GFX_FILL(3, 3, 3, 3, kColorWhite);
            GFX_FILL(18, 3, 3, 3, kColorWhite);
            GFX_FILL(3, 18, 3, 3, kColorWhite);
            GFX_FILL(18, 18, 3, 3, kColorWhite);
            break;
        }
        GFX_POP();
    }

    // Passive icons (15x15)
    for (int i = 0; i < 5; i++) {
        passiveIconImgs[i] = make_bitmap(15, 15);
        GFX_PUSH(passiveIconImgs[i]);
        switch (i) {
        case 0: // Oilskin Coat
            GFX_FILL(3, 0, 9, 2, kColorWhite);
            GFX_FILL(1, 2, 13, 6, kColorWhite);
            GFX_FILL(2, 8, 11, 3, kColorWhite);
            GFX_FILL(4, 11, 7, 2, kColorWhite);
            break;
        case 1: // Sea Legs
            GFX_FILL(4, 0, 5, 3, kColorWhite);
            GFX_FILL(3, 3, 6, 4, kColorWhite);
            GFX_FILL(2, 7, 7, 3, kColorWhite);
            break;
        case 2: // Barnacle Armor
            pd->graphics->drawEllipse(2, 2, 11, 11, 1, 0, 360, kColorWhite);
            GFX_FILL(6, 6, 3, 3, kColorWhite);
            break;
        case 3: // Lighthouse Lens
            pd->graphics->drawEllipse(0, 3, 15, 9, 1, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(5, 5, 5, 5, 0, 360, kColorWhite);
            break;
        case 4: // Tidecaller
            pd->graphics->drawLine(0, 7, 7, 2, 1, kColorWhite);
            pd->graphics->drawLine(7, 2, 14, 7, 1, kColorWhite);
            pd->graphics->drawLine(0, 11, 7, 6, 1, kColorWhite);
            pd->graphics->drawLine(7, 6, 14, 11, 1, kColorWhite);
            break;
        }
        GFX_POP();
    }

    // Large Passive icons (24x24) for upgrade screen
    for (int i = 0; i < 5; i++) {
        passiveIconLargeImgs[i] = make_bitmap(24, 24);
        GFX_PUSH(passiveIconLargeImgs[i]);
        switch (i) {
        case 0: // Oilskin Coat (Raincoat silhouette)
            GFX_FILL(8, 0, 8, 4, kColorWhite); // Hood
            GFX_FILL(4, 4, 16, 18, kColorWhite); // Body
            GFX_FILL(2, 6, 4, 12, kColorWhite); // Left arm
            GFX_FILL(18, 6, 4, 12, kColorWhite); // Right arm
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->drawLine(12, 4, 12, 22, 1, kColorBlack); // middle line
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            break;
        case 1: // Sea Legs (Boots)
            GFX_FILL(6, 2, 5, 12, kColorWhite); // Boot 1 shaft
            GFX_FILL(4, 14, 8, 4, kColorWhite); // Boot 1 foot
            GFX_FILL(13, 2, 5, 12, kColorWhite); // Boot 2 shaft
            GFX_FILL(11, 14, 8, 4, kColorWhite); // Boot 2 foot
            break;
        case 2: // Barnacle Armor (Rough spiked shield)
            pd->graphics->fillEllipse(2, 2, 20, 20, 0, 360, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            for(int a=0; a<360; a+=45) {
                int dx = 12 + 7 * cos(a * 3.14/180);
                int dy = 12 + 7 * sin(a * 3.14/180);
                GFX_FILL(dx-1, dy-1, 2, 2, kColorBlack);
            }
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            break;
        case 3: // Lighthouse Lens (Detailed lens)
            pd->graphics->drawEllipse(1, 4, 22, 16, 2, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(7, 7, 10, 10, 0, 360, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->fillEllipse(10, 10, 4, 4, 0, 360, kColorBlack); // Pupil
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            // Light rays
            pd->graphics->drawLine(3, 3, 0, 0, 1, kColorWhite);
            pd->graphics->drawLine(21, 3, 24, 0, 1, kColorWhite);
            pd->graphics->drawLine(3, 21, 0, 24, 1, kColorWhite);
            pd->graphics->drawLine(21, 21, 24, 24, 1, kColorWhite);
            break;
        case 4: // Tidecaller (Crashing waves)
            for(int j=0; j<3; j++) {
                int oy = j * 6;
                pd->graphics->drawLine(0, 10+oy, 6, 4+oy, 2, kColorWhite);
                pd->graphics->drawLine(6, 4+oy, 12, 12+oy, 2, kColorWhite);
                pd->graphics->drawLine(12, 12+oy, 18, 4+oy, 2, kColorWhite);
                pd->graphics->drawLine(18, 4+oy, 24, 10+oy, 2, kColorWhite);
            }
            break;
        }
        GFX_POP();
    }

    // Pickup sprites (12x12) — distinct per type
    pickupImgs[PICKUP_HEALTH] = make_bitmap(12, 12);
    GFX_PUSH(pickupImgs[PICKUP_HEALTH]);
    GFX_FILL(2, 2, 4, 3, kColorWhite); // Left lobe
    GFX_FILL(6, 2, 4, 3, kColorWhite); // Right lobe
    GFX_FILL(2, 5, 8, 3, kColorWhite); // Middle
    GFX_FILL(3, 8, 6, 2, kColorWhite); // Taper
    GFX_FILL(5, 10, 2, 2, kColorWhite); // Tip
    GFX_POP();

    // XP Burst: angular star/diamond
    pickupImgs[PICKUP_XP_BURST] = make_bitmap(12, 12);
    GFX_PUSH(pickupImgs[PICKUP_XP_BURST]);
    GFX_FILL(5, 0, 2, 12, kColorWhite);
    GFX_FILL(0, 5, 12, 2, kColorWhite);
    GFX_FILL(3, 2, 6, 8, kColorWhite);
    GFX_FILL(2, 3, 8, 6, kColorWhite);
    GFX_FILL(5, 5, 2, 2, kColorBlack);
    GFX_POP();

    pickupImgs[PICKUP_WEAPON_UPGRADE] = make_bitmap(12, 12);
    GFX_PUSH(pickupImgs[PICKUP_WEAPON_UPGRADE]);
    GFX_FILL(5, 0, 2, 12, kColorWhite); // Vertical beam
    GFX_FILL(0, 5, 12, 2, kColorWhite); // Horizontal beam
    // Diagonal flares
    GFX_FILL(3, 3, 2, 2, kColorWhite);
    GFX_FILL(7, 3, 2, 2, kColorWhite);
    GFX_FILL(3, 7, 2, 2, kColorWhite);
    GFX_FILL(7, 7, 2, 2, kColorWhite);
    GFX_POP();

    // New Weapon: sword silhouette
    pickupImgs[PICKUP_NEW_WEAPON] = make_bitmap(12, 12);
    GFX_PUSH(pickupImgs[PICKUP_NEW_WEAPON]);
    pd->graphics->drawLine(2, 10, 10, 2, 2, kColorWhite);
    GFX_FILL(8, 0, 4, 4, kColorWhite);
    GFX_FILL(6, 5, 3, 1, kColorWhite);
    GFX_FILL(5, 6, 1, 3, kColorWhite);
    GFX_FILL(0, 9, 3, 3, kColorWhite);
    GFX_POP();
}

// ---------------------------------------------------------------------------
// Character Design Alternatives (5 new + copy original)
// ---------------------------------------------------------------------------

// Helper: checkerboard fill (simulates 50% grey)
static void dither_fill(int x, int y, int w, int h, int phase)
{
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if ((px + py + phase) % 2 == 0) GFX_FILL(px, py, 1, 1, kColorWhite);
        }
    }
}

// Helper: dithered ellipse fill (simulates grey circle)
static void dither_ellipse(int cx, int cy, int rx, int ry, int density)
{
    for (int py = cy - ry; py <= cy + ry; py++) {
        for (int px = cx - rx; px <= cx + rx; px++) {
            float dx = (float)(px - cx) / (float)rx;
            float dy = (float)(py - cy) / (float)ry;
            if (dx * dx + dy * dy <= 1.0f) {
                if ((px + py) % density == 0) GFX_FILL(px, py, 1, 1, kColorWhite);
            }
        }
    }
}

static void create_design_images(void)
{
    // Design 0: Copy of original (same as create_player_images)
    for (int f = 0; f < 4; f++) {
        designFrames[0][f] = make_bitmap(30, 30);
        GFX_PUSH(designFrames[0][f]);
        int bob = (f == 1 || f == 3) ? 1 : 0;
        GFX_FILL(5, 7+bob, 20, 2, kColorWhite);
        pd->graphics->drawEllipse(8, 1+bob, 14, 8, 2, 0, 360, kColorWhite);
        GFX_FILL(10, 3+bob, 10, 4, kColorBlack);
        GFX_FILL(10, 8+bob, 10, 5, kColorBlack);
        GFX_FILL(12, 10+bob, 2, 1, kColorWhite);
        GFX_FILL(16, 10+bob, 2, 1, kColorWhite);
        // Coat (ellipse outline with detailed interior)
        pd->graphics->drawEllipse(6, 12+bob, 18, 14, 1, 0, 360, kColorWhite);
        GFX_FILL(8, 14+bob, 14, 10, kColorBlack);
        // Lapel V-lines (like Captain's coat)
        pd->graphics->drawLine(12, 13+bob, 15, 17+bob, 1, kColorWhite);
        pd->graphics->drawLine(18, 13+bob, 15, 17+bob, 1, kColorWhite);
        // Button row (3 small dots down the center)
        GFX_FILL(14, 18+bob, 2, 1, kColorWhite);
        GFX_FILL(14, 20+bob, 2, 1, kColorWhite);
        GFX_FILL(14, 22+bob, 2, 1, kColorWhite);
        // Belt with buckle
        GFX_FILL(9, 20+bob, 12, 1, kColorWhite);
        GFX_FILL(13, 19+bob, 4, 3, kColorWhite);
        GFX_FILL(14, 20+bob, 2, 1, kColorBlack);         // buckle hole

        int armOff = (f == 0) ? -1 : (f == 2) ? 1 : 0;
        pd->graphics->drawLine(9, 16+bob, 4+armOff, 21+bob, 1, kColorWhite);
        int lx = 25 - armOff;
        int ly = 20 + bob;
        pd->graphics->drawLine(21, 16+bob, lx, ly, 1, kColorWhite);
        GFX_FILL(lx-1, ly, 3, 3, kColorWhite);
        GFX_FILL(lx, ly+1, 1, 1, kColorBlack);
        int step = (f == 1) ? 2 : (f == 3) ? -2 : 0;
        pd->graphics->drawLine(12, 25+bob, 10+step, 29, 1, kColorWhite);
        pd->graphics->drawLine(18, 25+bob, 20-step, 29, 1, kColorWhite);
        GFX_POP();
    }

    // Design 1: "Weathered Mariner" — stocky, dithered grey peacoat, bearded
    for (int f = 0; f < 4; f++) {
        designFrames[1][f] = make_bitmap(30, 30);
        GFX_PUSH(designFrames[1][f]);
        int bob = (f == 1 || f == 3) ? 1 : 0;

        // Wide sou'wester hat with dithered brim texture
        GFX_FILL(5, 5+bob, 20, 2, kColorWhite);      // wide brim
        dither_fill(5, 5+bob, 20, 2, 1);               // brim texture (half-grey)
        GFX_FILL(8, 5+bob, 14, 2, kColorWhite);        // restore brim core solid
        GFX_FILL(9, 1+bob, 12, 5, kColorWhite);        // hat crown
        GFX_FILL(10, 2+bob, 10, 3, kColorBlack);       // crown hollow

        // Face — wider, with dithered beard
        GFX_FILL(9, 7+bob, 12, 6, kColorBlack);        // face bg
        GFX_FILL(11, 9+bob, 2, 2, kColorWhite);        // left eye
        GFX_FILL(17, 9+bob, 2, 2, kColorWhite);        // right eye
        // Dithered beard/stubble below eyes
        dither_fill(10, 11+bob, 10, 3, 0);

        // Stocky peacoat body (dithered grey)
        GFX_FILL(5, 13+bob, 20, 12, kColorBlack);      // body bg
        dither_fill(5, 13+bob, 20, 12, 0);              // grey coat fill
        // Coat outline
        pd->graphics->drawRect(5, 13+bob, 20, 12, kColorWhite);
        // Double-breast line
        GFX_FILL(14, 14+bob, 1, 10, kColorWhite);
        GFX_FILL(16, 14+bob, 1, 10, kColorWhite);
        // Belt
        GFX_FILL(6, 20+bob, 18, 1, kColorWhite);
        // Belt buckle
        GFX_FILL(13, 19+bob, 4, 3, kColorWhite);
        GFX_FILL(14, 20+bob, 2, 1, kColorBlack);

        // Arms — thicker
        int armOff = (f == 0) ? -1 : (f == 2) ? 1 : 0;
        pd->graphics->drawLine(6, 15+bob, 2+armOff, 20+bob, 2, kColorWhite);
        pd->graphics->drawLine(24, 15+bob, 27-armOff, 20+bob, 2, kColorWhite);

        // Lantern at hip
        GFX_FILL(26-armOff, 19+bob, 3, 4, kColorWhite);
        GFX_FILL(27-armOff, 20+bob, 1, 2, kColorBlack);

        // Thick boots
        int step = (f == 1) ? 2 : (f == 3) ? -2 : 0;
        GFX_FILL(9+step, 25+bob, 4, 4, kColorWhite);
        GFX_FILL(17-step, 25+bob, 4, 4, kColorWhite);

        GFX_POP();
    }

    // Design 2: "The Hermit" — hooded cloak silhouette, staff with lantern
    for (int f = 0; f < 4; f++) {
        designFrames[2][f] = make_bitmap(30, 30);
        GFX_PUSH(designFrames[2][f]);
        int bob = (f == 1 || f == 3) ? 1 : 0;

        // Staff with lantern (right side)
        GFX_FILL(23, 0, 1, 28, kColorWhite);           // staff pole
        // Lantern at top of staff
        GFX_FILL(21, 0, 5, 1, kColorWhite);             // top bar
        GFX_RECT(21, 1, 5, 5, kColorWhite);             // lantern body
        GFX_FILL(22, 2, 3, 3, kColorWhite);             // lantern glow (solid)
        GFX_FILL(23, 3, 1, 1, kColorBlack);             // flame core
        // Flickering glow halo — varies per walk frame
        // Frame 0: medium glow, Frame 1: large glow, Frame 2: small glow, Frame 3: large glow
        {
            int glowSz = (f == 0) ? 2 : (f == 2) ? 1 : 3;
            // Side glow (dithered, varies in width)
            dither_fill(21 - glowSz, 0, glowSz, 6, f);
            dither_fill(26, 0, glowSz, 6, f);
            // Bottom glow
            dither_fill(21 - glowSz/2, 6, 5 + glowSz, 1 + glowSz/2, f + 1);
            // Top glow (small upward spill on bigger frames)
            if (glowSz >= 2) {
                dither_fill(22, -1, 3, 1, f);
            }
        }

        // Round hood (smooth dome shape)
        pd->graphics->fillEllipse(4, 1+bob, 18, 16, 0, 360, kColorWhite);
        pd->graphics->fillEllipse(6, 3+bob, 14, 12, 0, 360, kColorBlack); // deep void inside
        // Hood rim (thicker bottom arc)
        pd->graphics->drawEllipse(4, 1+bob, 18, 16, 1, 180, 360, kColorWhite);

        // Two bright eyes deep inside the round hood
        GFX_FILL(10, 8+bob, 2, 2, kColorWhite);
        GFX_FILL(15, 8+bob, 2, 2, kColorWhite);

        // Long flowing cloak (tapers down, solid black silhouette with white edge)
        pd->graphics->drawLine(6, 13+bob, 4, 27, 1, kColorWhite);    // left edge
        pd->graphics->drawLine(20, 13+bob, 22, 27, 1, kColorWhite);  // right edge
        GFX_FILL(5, 27, 18, 2, kColorWhite);            // bottom hem
        // Fill cloak interior
        for (int py = 13; py < 27; py++) {
            int leftX = 6 - (py - 13) * 2 / 14;
            int rightX = 20 + (py - 13) * 2 / 14;
            leftX = clampi(leftX, 4, 20);
            rightX = clampi(rightX, 6, 22);
            GFX_FILL(leftX+1, py+bob, rightX - leftX - 1, 1, kColorBlack);
        }
        // Ragged bottom edge (torn strips)
        GFX_FILL(5, 28, 2, 1, kColorWhite);
        GFX_FILL(9, 29, 2, 1, kColorWhite);
        GFX_FILL(15, 28, 2, 1, kColorWhite);
        GFX_FILL(19, 29, 2, 1, kColorWhite);

        // Hand wraps holding staff (visible at staff intersection)
        GFX_FILL(21, 14+bob, 4, 3, kColorWhite);
        GFX_FILL(22, 15+bob, 2, 1, kColorBlack);

        // Subtle foot movement at cloak base
        int step = (f == 1) ? 1 : (f == 3) ? -1 : 0;
        GFX_FILL(8+step, 28, 3, 2, kColorWhite);
        GFX_FILL(15-step, 28, 3, 2, kColorWhite);

        GFX_POP();
    }

    // Design 3: "Old Captain" — peaked cap, double-breasted coat, pipe, sideburns
    for (int f = 0; f < 4; f++) {
        designFrames[3][f] = make_bitmap(30, 30);
        GFX_PUSH(designFrames[3][f]);
        int bob = (f == 1 || f == 3) ? 1 : 0;

        // Captain's peaked cap
        GFX_FILL(8, 4+bob, 14, 4, kColorWhite);        // cap body
        GFX_FILL(9, 5+bob, 12, 2, kColorBlack);         // dark band
        GFX_FILL(13, 5+bob, 4, 1, kColorWhite);         // badge/insignia
        GFX_FILL(6, 7+bob, 18, 2, kColorWhite);         // visor (wide brim)
        // Visor shading (dithered underside)
        dither_fill(7, 8+bob, 16, 1, 0);

        // Face with sideburns
        GFX_FILL(9, 9+bob, 12, 6, kColorBlack);         // face bg
        // Sideburns (white blocks on sides of dark face)
        GFX_FILL(9, 10+bob, 2, 4, kColorWhite);
        GFX_FILL(19, 10+bob, 2, 4, kColorWhite);
        // Eyes
        GFX_FILL(12, 10+bob, 2, 2, kColorWhite);
        GFX_FILL(16, 10+bob, 2, 2, kColorWhite);
        // Stern mouth
        GFX_FILL(13, 13+bob, 4, 1, kColorWhite);
        // Pipe extending from mouth
        GFX_FILL(17, 12+bob, 5, 1, kColorWhite);        // pipe stem
        GFX_FILL(21, 10+bob, 3, 3, kColorWhite);        // pipe bowl
        GFX_FILL(22, 11+bob, 1, 1, kColorBlack);        // bowl hollow
        // Pipe smoke (dithered puff, only on some frames)
        if (f == 0 || f == 2) {
            dither_fill(22, 8+bob, 3, 2, f);
        }

        // Double-breasted coat
        GFX_FILL(7, 15+bob, 16, 11, kColorWhite);       // coat body
        GFX_FILL(8, 16+bob, 14, 9, kColorBlack);        // coat interior
        // Epaulettes
        GFX_FILL(6, 15+bob, 3, 2, kColorWhite);
        GFX_FILL(21, 15+bob, 3, 2, kColorWhite);
        // Coat lapels (V-shape)
        pd->graphics->drawLine(11, 16+bob, 15, 19+bob, 1, kColorWhite);
        pd->graphics->drawLine(19, 16+bob, 15, 19+bob, 1, kColorWhite);
        // Buttons (2 rows of 2)
        GFX_FILL(11, 20+bob, 2, 2, kColorWhite);
        GFX_FILL(17, 20+bob, 2, 2, kColorWhite);
        GFX_FILL(11, 23+bob, 2, 2, kColorWhite);
        GFX_FILL(17, 23+bob, 2, 2, kColorWhite);
        // Belt
        GFX_FILL(8, 21+bob, 14, 1, kColorWhite);

        // Arms
        int armOff = (f == 0) ? -1 : (f == 2) ? 1 : 0;
        pd->graphics->drawLine(7, 16+bob, 3+armOff, 22+bob, 2, kColorWhite);
        pd->graphics->drawLine(23, 16+bob, 27-armOff, 22+bob, 2, kColorWhite);

        // Boots (tall)
        int step = (f == 1) ? 2 : (f == 3) ? -2 : 0;
        GFX_FILL(10+step, 26+bob, 3, 4, kColorWhite);
        GFX_FILL(17-step, 26+bob, 3, 4, kColorWhite);
        // Boot tops
        GFX_FILL(9+step, 26+bob, 5, 1, kColorWhite);
        GFX_FILL(16-step, 26+bob, 5, 1, kColorWhite);

        GFX_POP();
    }

    // Design 4: "Storm Walker" — dynamic forward lean, billowing coat, scarf
    for (int f = 0; f < 4; f++) {
        designFrames[4][f] = make_bitmap(30, 30);
        GFX_PUSH(designFrames[4][f]);
        int bob = (f == 1 || f == 3) ? 1 : 0;
        // This design leans forward — offset everything left by 2
        int lean = -2;

        // Rain hat pulled low (flat, wide)
        GFX_FILL(4+lean, 5+bob, 16, 2, kColorWhite);   // wide flat brim
        GFX_FILL(6+lean, 2+bob, 12, 4, kColorWhite);    // hat crown
        GFX_FILL(7+lean, 3+bob, 10, 2, kColorBlack);    // crown interior
        // Hat drip detail
        GFX_FILL(5+lean, 7+bob, 1, 2, kColorWhite);

        // Face barely visible under hat
        GFX_FILL(7+lean, 7+bob, 10, 5, kColorBlack);
        // Just a glint of eyes under the brim
        GFX_FILL(10+lean, 9+bob, 2, 1, kColorWhite);
        GFX_FILL(14+lean, 9+bob, 2, 1, kColorWhite);

        // Coat body — angled, leaning into wind
        // Left side (front, closer) is more solid
        GFX_FILL(4+lean, 12+bob, 14, 13, kColorWhite);
        GFX_FILL(5+lean, 13+bob, 12, 11, kColorBlack);
        // Coat outline on leading edge
        pd->graphics->drawLine(4+lean, 12+bob, 3+lean, 25+bob, 1, kColorWhite);

        // Billowing coat tail (trailing right, dithered for transparency)
        for (int py = 14; py < 26; py++) {
            int tailLen = (py - 14) * 2 / 3 + 1;
            int startX = 18 + lean;
            dither_fill(startX, py+bob, tailLen + 2, 1, py % 2);
        }
        // Solid trailing edge
        pd->graphics->drawLine(18+lean, 14+bob, 24+lean, 24+bob, 1, kColorWhite);

        // Scarf streaming behind (dithered trail)
        pd->graphics->drawLine(16+lean, 8+bob, 22, 6+bob, 1, kColorWhite);
        pd->graphics->drawLine(22, 6+bob, 27, 8+bob, 1, kColorWhite);
        dither_fill(24, 5+bob, 4, 3, f);
        // Scarf flutters differently per frame
        if (f % 2 == 0) {
            pd->graphics->drawLine(27, 8+bob, 29, 5+bob, 1, kColorWhite);
        } else {
            pd->graphics->drawLine(27, 8+bob, 29, 10+bob, 1, kColorWhite);
        }

        // Forward arm with lantern
        int armBob = (f == 1) ? 1 : (f == 3) ? -1 : 0;
        pd->graphics->drawLine(5+lean, 14+bob, 1, 18+bob+armBob, 2, kColorWhite);
        // Lantern
        GFX_FILL(0, 17+bob+armBob, 3, 4, kColorWhite);
        GFX_FILL(1, 18+bob+armBob, 1, 2, kColorBlack);
        // Lantern glow (dithered)
        dither_fill(0, 14+bob+armBob, 4, 3, 0);

        // Back arm bracing
        pd->graphics->drawLine(16+lean, 14+bob, 21, 18+bob, 1, kColorWhite);

        // Legs in dynamic stride
        int step = (f == 1) ? 3 : (f == 3) ? -3 : 0;
        pd->graphics->drawLine(9+lean, 24+bob, 6+lean+step, 29, 2, kColorWhite);
        pd->graphics->drawLine(14+lean, 24+bob, 17+lean-step, 29, 2, kColorWhite);

        GFX_POP();
    }

    // Design 5: "The Beacon" — geometric, iconic, lighthouse-inspired
    for (int f = 0; f < 4; f++) {
        designFrames[5][f] = make_bitmap(30, 30);
        GFX_PUSH(designFrames[5][f]);
        int bob = (f == 1 || f == 3) ? 1 : 0;

        // Head — bright circle like a lighthouse lamp
        pd->graphics->fillEllipse(9, 1+bob, 12, 12, 0, 360, kColorWhite);
        // Face cutout (simple, iconic)
        GFX_FILL(12, 5+bob, 2, 2, kColorBlack);        // left eye
        GFX_FILL(16, 5+bob, 2, 2, kColorBlack);        // right eye
        GFX_FILL(13, 9+bob, 4, 1, kColorBlack);        // mouth line

        // 4 radiating light beams from head
        GFX_FILL(6, 6+bob, 3, 1, kColorWhite);          // left beam
        GFX_FILL(21, 6+bob, 3, 1, kColorWhite);         // right beam
        GFX_FILL(14, 0, 2, 1, kColorWhite);             // top beam
        GFX_FILL(14, 13+bob, 2, 1, kColorWhite);        // bottom beam (short)
        // Diagonal beams
        GFX_FILL(7, 2+bob, 2, 1, kColorWhite);
        GFX_FILL(21, 2+bob, 2, 1, kColorWhite);
        GFX_FILL(7, 11+bob, 2, 1, kColorWhite);
        GFX_FILL(21, 11+bob, 2, 1, kColorWhite);

        // Body — tapered column (wide shoulders, narrow waist)
        // Gradient dither: dense at top, sparse at bottom
        for (int py = 14; py < 25; py++) {
            int rowIdx = py - 14;
            int halfW = 8 - rowIdx * 4 / 11;  // taper from 8 to ~4
            int cx = 15;
            // Dithering density increases toward top
            int density = 2 + rowIdx / 4;
            for (int px = cx - halfW; px <= cx + halfW; px++) {
                if (rowIdx < 3) {
                    // Top rows: solid white (shoulders)
                    GFX_FILL(px, py+bob, 1, 1, kColorWhite);
                } else if ((px + py) % density == 0) {
                    GFX_FILL(px, py+bob, 1, 1, kColorWhite);
                }
            }
        }
        // Shoulder line (solid)
        GFX_FILL(7, 14+bob, 16, 2, kColorWhite);
        // Waist line
        GFX_FILL(11, 23+bob, 8, 1, kColorWhite);

        // Arms — clean single lines with dot hands
        int armOff = (f == 0) ? -1 : (f == 2) ? 1 : 0;
        pd->graphics->drawLine(8, 15+bob, 4+armOff, 20+bob, 1, kColorWhite);
        pd->graphics->drawLine(22, 15+bob, 26-armOff, 20+bob, 1, kColorWhite);
        GFX_FILL(3+armOff, 20+bob, 2, 2, kColorWhite);  // left hand
        GFX_FILL(26-armOff, 20+bob, 2, 2, kColorWhite); // right hand

        // Peg legs — simple, geometric
        int step = (f == 1) ? 2 : (f == 3) ? -2 : 0;
        GFX_FILL(12+step, 24+bob, 2, 5, kColorWhite);
        GFX_FILL(16-step, 24+bob, 2, 5, kColorWhite);
        // Feet (small rectangles)
        GFX_FILL(11+step, 28, 4, 2, kColorWhite);
        GFX_FILL(15-step, 28, 4, 2, kColorWhite);

        GFX_POP();
    }
}

// ---------------------------------------------------------------------------
// Enemy Design Alternatives: Original + 1 animated redesign per enemy
// ---------------------------------------------------------------------------
static void create_enemy_design_images(void)
{
    // Slot 0: copy both animation frames of the originals
    for (int t = 0; t < ENEMY_TYPE_COUNT; t++) {
        enemyDesignImgs[t][0][0] = enemyImgs[t][0];
        enemyDesignImgs[t][0][1] = enemyImgs[t][1];
    }

    // ================================================================
    // Slot 1: One high-quality animated redesign per enemy
    // ================================================================

    // ---- CREEPER (30x30) — Hermit-style: dark dome shell, peering eyes, thin legs ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_CREEPER][1][f] = make_bitmap(30, 30);
    GFX_PUSH(enemyDesignImgs[ENEMY_CREEPER][1][f]);
    // Dome shell — solid black with white outline
    pd->graphics->fillEllipse(6, 2, 18, 14, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(8, 4, 14, 10, 0, 360, kColorBlack);
    pd->graphics->drawEllipse(6, 2, 18, 14, 1, 180, 360, kColorWhite);
    // Three peering eyes from the dark — shift per frame
    int es = f ? 1 : 0;
    GFX_FILL(10+es, 7, 2, 2, kColorWhite);
    GFX_FILL(14+es, 6, 2, 3, kColorWhite); // center eye taller
    GFX_FILL(18+es, 7, 2, 2, kColorWhite);
    // Thin legs — clean 1px lines, alternate positions
    int la = f ? 2 : 0;
    // Left 3
    pd->graphics->drawLine(8, 12, 3+la, 6-la, 1, kColorWhite);
    pd->graphics->drawLine(7, 14, 1-la, 14+la, 1, kColorWhite);
    pd->graphics->drawLine(8, 15, 3+la, 22-la, 1, kColorWhite);
    // Right 3
    pd->graphics->drawLine(22, 12, 27-la, 6-la, 1, kColorWhite);
    pd->graphics->drawLine(23, 14, 29+la, 14+la, 1, kColorWhite);
    pd->graphics->drawLine(22, 15, 27-la, 22-la, 1, kColorWhite);
    // Small pincers at front (bottom)
    pd->graphics->drawLine(11, 16, 9, 22, 1, kColorWhite);
    pd->graphics->drawLine(9, 22, 12, 24, 1, kColorWhite);
    pd->graphics->drawLine(19, 16, 21, 22, 1, kColorWhite);
    pd->graphics->drawLine(21, 22, 18, 24, 1, kColorWhite);
    GFX_POP();
    }

    // ---- TENDRIL (44x44) — Drowned Jellyfish: smaller dark bell, dot eyes, dangling tendrils ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_TENDRIL][1][f] = make_bitmap(44, 44);
    GFX_PUSH(enemyDesignImgs[ENEMY_TENDRIL][1][f]);
    // Smaller dark bell dome
    pd->graphics->fillEllipse(10, 2, 24, 16, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(12, 4, 20, 12, 0, 360, kColorBlack);
    pd->graphics->drawEllipse(10, 2, 24, 16, 1, 180, 360, kColorWhite);
    // Two dot eyes peering from inside the dark bell
    int ep = f ? 1 : 0;
    GFX_FILL(17+ep, 8, 2, 2, kColorWhite);
    GFX_FILL(25+ep, 8, 2, 2, kColorWhite);
    // Trailing tendrils — sway per frame
    int sw = f ? 2 : -2;
    pd->graphics->drawLine(15, 16, 11+sw, 30, 1, kColorWhite);
    pd->graphics->drawLine(19, 16, 17-sw, 34, 1, kColorWhite);
    pd->graphics->drawLine(22, 16, 22, 36, 1, kColorWhite);
    pd->graphics->drawLine(25, 16, 27+sw, 34, 1, kColorWhite);
    pd->graphics->drawLine(29, 16, 33-sw, 30, 1, kColorWhite);
    // Barbs at tips
    GFX_FILL(10+sw, 29, 2, 2, kColorWhite);
    GFX_FILL(16-sw, 33, 2, 2, kColorWhite);
    GFX_FILL(21, 35, 2, 2, kColorWhite);
    GFX_FILL(26+sw, 33, 2, 2, kColorWhite);
    GFX_FILL(32-sw, 29, 2, 2, kColorWhite);
    GFX_POP();
    }

    // ---- WRAITH (40x40) — Spectral Manta: dark diamond wings, slit eyes, flowing tail ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_WRAITH][1][f] = make_bitmap(40, 40);
    GFX_PUSH(enemyDesignImgs[ENEMY_WRAITH][1][f]);
    // Wing undulation offset
    int wu = f ? 2 : 0;
    // Dark manta body — diamond/kite shape, filled black, outlined white
    pd->graphics->drawLine(20, 4-wu, 2, 16+wu, 1, kColorWhite);   // top-left edge
    pd->graphics->drawLine(20, 4-wu, 38, 16+wu, 1, kColorWhite);  // top-right edge
    pd->graphics->drawLine(2, 16+wu, 20, 24-wu, 1, kColorWhite);  // bottom-left edge
    pd->graphics->drawLine(38, 16+wu, 20, 24-wu, 1, kColorWhite); // bottom-right edge
    // Fill interior black (scanline)
    for (int py = 5-wu; py < 24-wu; py++) {
        int mid = 16 + wu;
        int lx, rx;
        if (py < mid) {
            float t = (float)(py - (4-wu)) / (float)(mid - (4-wu));
            lx = 20 - (int)(18.0f * t);
            rx = 20 + (int)(18.0f * t);
        } else {
            float t = (float)(py - mid) / (float)((24-wu) - mid);
            lx = 2 + (int)(18.0f * t);
            rx = 38 - (int)(18.0f * t);
        }
        if (lx < rx) GFX_FILL(lx+1, py, rx - lx - 1, 1, kColorBlack);
    }
    // Two forward-facing slit eyes
    GFX_FILL(15, 12, 3, 2, kColorWhite);
    GFX_FILL(22, 12, 3, 2, kColorWhite);
    // Cephalic fins (horn shapes at front)
    pd->graphics->drawLine(14, 10, 10, 4-wu, 1, kColorWhite);
    pd->graphics->drawLine(26, 10, 30, 4-wu, 1, kColorWhite);
    // Long whip tail
    int tailSw = f ? 3 : -3;
    pd->graphics->drawLine(20, 24-wu, 20+tailSw, 34, 1, kColorWhite);
    pd->graphics->drawLine(20+tailSw, 34, 20-tailSw, 40, 1, kColorWhite);
    GFX_POP();
    }

    // ---- ABYSSAL (56x56) — Deep Angler: wide dark irregular body, round jaw with teeth, dot eyes, lure ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_ABYSSAL][1][f] = make_bitmap(56, 56);
    GFX_PUSH(enemyDesignImgs[ENEMY_ABYSSAL][1][f]);
    int jo = f ? 2 : 0;
    // Irregular dark body — wider than tall, not perfectly round
    // Top outline (humped, asymmetric)
    pd->graphics->drawLine(8, 20, 16, 10-jo, 1, kColorWhite);
    pd->graphics->drawLine(16, 10-jo, 28, 8-jo, 1, kColorWhite);
    pd->graphics->drawLine(28, 8-jo, 40, 10-jo, 1, kColorWhite);
    pd->graphics->drawLine(40, 10-jo, 48, 20, 1, kColorWhite);
    // Side outlines curving into jaw
    pd->graphics->drawLine(8, 20, 4, 28, 1, kColorWhite);
    pd->graphics->drawLine(48, 20, 52, 28, 1, kColorWhite);
    // Round jaw outline — arc connecting the sides at y=28
    pd->graphics->drawEllipse(4, 20, 48, 28+jo*2, 1, 0, 180, kColorWhite);
    // Fill body black (scanline between outlines)
    for (int py = 10-jo; py < 34+jo; py++) {
        float t;
        int lx, rx;
        if (py < 20) {
            t = (float)(py - (10-jo)) / (float)(20 - (10-jo));
            lx = 16 - (int)(8.0f * t);
            rx = 40 + (int)(8.0f * t);
        } else if (py < 28) {
            t = (float)(py - 20) / 8.0f;
            lx = 8 - (int)(4.0f * t);
            rx = 48 + (int)(4.0f * t);
        } else {
            t = (float)(py - 28) / (float)(6+jo);
            lx = 4 + (int)(10.0f * t);
            rx = 52 - (int)(10.0f * t);
        }
        if (lx < rx) GFX_FILL(lx+1, py, rx - lx - 1, 1, kColorBlack);
    }
    // Dot eyes — small and deep-set
    GFX_FILL(16+f, 18, 2, 2, kColorWhite);
    GFX_FILL(38+f, 18, 2, 2, kColorWhite);
    // Upper teeth — free triangles hanging down inside jaw
    pd->graphics->drawLine(14, 28, 18, 34, 1, kColorWhite);
    pd->graphics->drawLine(22, 28, 18, 34, 1, kColorWhite);
    pd->graphics->drawLine(24, 28, 28, 36+jo, 1, kColorWhite);
    pd->graphics->drawLine(32, 28, 28, 36+jo, 1, kColorWhite);
    pd->graphics->drawLine(34, 28, 38, 34, 1, kColorWhite);
    pd->graphics->drawLine(42, 28, 38, 34, 1, kColorWhite);
    // Lower teeth — pointing up from jaw arc
    pd->graphics->drawLine(16, 44+jo, 20, 38, 1, kColorWhite);
    pd->graphics->drawLine(24, 44+jo, 20, 38, 1, kColorWhite);
    pd->graphics->drawLine(32, 44+jo, 36, 38, 1, kColorWhite);
    pd->graphics->drawLine(40, 44+jo, 36, 38, 1, kColorWhite);
    // Angler lure stalk
    pd->graphics->drawLine(28, 8-jo, 30, 2, 1, kColorWhite);
    pd->graphics->drawLine(30, 2, 34, 0, 1, kColorWhite);
    GFX_FILL(33, 0, 3, 3, kColorWhite);
    if (f) { GFX_FILL(32, 0, 1, 1, kColorWhite); GFX_FILL(36, 1, 1, 1, kColorWhite); }
    GFX_POP();
    }

    // ---- SEER (48x48) — Abyssal Eye Squid: dark bulbous head, single massive eye, grasping arms ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_SEER][1][f] = make_bitmap(48, 48);
    GFX_PUSH(enemyDesignImgs[ENEMY_SEER][1][f]);
    // Large dark bulbous head — dominates upper half
    pd->graphics->fillEllipse(6, 0, 36, 28, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(8, 2, 32, 24, 0, 360, kColorBlack);
    pd->graphics->drawEllipse(6, 0, 36, 28, 1, 0, 360, kColorWhite);
    // One enormous eye — just the outline and a pupil dot
    pd->graphics->drawEllipse(12, 6, 24, 16, 1, 0, 360, kColorWhite);
    // Pupil — shifts per frame (looking around)
    int px = f ? 26 : 22;
    int py = 13;
    GFX_FILL(px-2, py-2, 4, 4, kColorWhite);
    GFX_FILL(px-1, py-1, 2, 2, kColorBlack); // dark center
    // Pointed mantle tip at top
    pd->graphics->drawLine(24, 0, 24, -2, 1, kColorWhite);
    pd->graphics->drawLine(22, 1, 24, -2, 1, kColorWhite);
    pd->graphics->drawLine(26, 1, 24, -2, 1, kColorWhite);
    // 8 grasping tentacle arms — sway per frame
    int ts = f ? 2 : -2;
    // Dense cluster of arms below the head
    pd->graphics->drawLine(12, 26, 6+ts, 38, 1, kColorWhite);
    pd->graphics->drawLine(16, 27, 12-ts, 40, 1, kColorWhite);
    pd->graphics->drawLine(20, 28, 18+ts, 42, 1, kColorWhite);
    pd->graphics->drawLine(24, 28, 24, 44, 1, kColorWhite);
    pd->graphics->drawLine(28, 28, 30-ts, 42, 1, kColorWhite);
    pd->graphics->drawLine(32, 27, 36+ts, 40, 1, kColorWhite);
    pd->graphics->drawLine(36, 26, 42-ts, 38, 1, kColorWhite);
    // Two long hunting tentacles — reach further, with club tips
    pd->graphics->drawLine(10, 26, 2+ts, 42, 1, kColorWhite);
    pd->graphics->drawLine(2+ts, 42, 4+ts, 46, 1, kColorWhite);
    GFX_FILL(2+ts, 45, 4, 2, kColorWhite);  // left club
    pd->graphics->drawLine(38, 26, 46-ts, 42, 1, kColorWhite);
    pd->graphics->drawLine(46-ts, 42, 44-ts, 46, 1, kColorWhite);
    GFX_FILL(42-ts, 45, 4, 2, kColorWhite);  // right club
    GFX_POP();
    }

    // ---- LAMPREY (36x36) — Lamprey Maw: dark ring body, inward teeth, dot eyes ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_LAMPREY][1][f] = make_bitmap(36, 36);
    GFX_PUSH(enemyDesignImgs[ENEMY_LAMPREY][1][f]);
    // Dark fleshy body ring
    pd->graphics->fillEllipse(4, 4, 28, 28, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(6, 6, 24, 24, 0, 360, kColorBlack);
    pd->graphics->drawEllipse(4, 4, 28, 28, 1, 0, 360, kColorWhite);
    // Dark void center (throat)
    pd->graphics->fillEllipse(12, 12, 12, 12, 0, 360, kColorBlack);
    // Inward-pointing teeth — 1px, rotate per frame
    int rot = f * 22;
    for (int a = rot; a < 360 + rot; a += 45) {
        float angle = (float)a * 3.14159f / 180.0f;
        int ox = 18 + (int)(11.0f * cosf(angle));
        int oy = 18 + (int)(11.0f * sinf(angle));
        int ix = 18 + (int)(7.0f * cosf(angle));
        int iy = 18 + (int)(7.0f * sinf(angle));
        pd->graphics->drawLine(ox, oy, ix, iy, 1, kColorWhite);
    }
    // Two dot eyes hidden in the flesh ring
    GFX_FILL(10, 14, 2, 2, kColorWhite);
    GFX_FILL(24, 20, 2, 2, kColorWhite);
    GFX_POP();
    }

    // ---- BLOAT (44x44) — Abyssal Pufferfish: dark spiky body, dot eyes, no mouth ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_BLOAT][1][f] = make_bitmap(44, 44);
    GFX_PUSH(enemyDesignImgs[ENEMY_BLOAT][1][f]);
    // Round body — inflates on frame 1
    int sw = f ? 2 : 0;
    pd->graphics->fillEllipse(6-sw, 4-sw, 32+sw*2, 30+sw*2, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(8-sw, 6-sw, 28+sw*2, 26+sw*2, 0, 360, kColorBlack);
    pd->graphics->drawEllipse(6-sw, 4-sw, 32+sw*2, 30+sw*2, 1, 0, 360, kColorWhite);
    // Dense sharp spines — skip bottom region where fins/tail are
    int spLen = f ? 6 : 4;
    for (int a = 0; a < 360; a += 20) {
        if (a > 200 && a < 340) continue; // skip bottom
        float angle = (float)a * 3.14159f / 180.0f;
        int bx = 22 + (int)((15.0f + sw) * cosf(angle));
        int by = 19 + (int)((14.0f + sw) * sinf(angle));
        int tx = 22 + (int)((15.0f + sw + spLen) * cosf(angle));
        int ty = 19 + (int)((14.0f + sw + spLen) * sinf(angle));
        pd->graphics->drawLine(bx, by, tx, ty, 1, kColorWhite);
    }
    // Dot eyes — slightly bigger (3x3), circular
    GFX_FILL(15+f, 14, 3, 3, kColorWhite);
    GFX_FILL(26+f, 14, 3, 3, kColorWhite);
    GFX_POP();
    }

    // ---- HARBINGER (52x52) — Kraken: tall dark mantle with ridges, dot eyes, thick tentacles ----
    for (int f = 0; f < 2; f++) {
    enemyDesignImgs[ENEMY_HARBINGER][1][f] = make_bitmap(52, 52);
    GFX_PUSH(enemyDesignImgs[ENEMY_HARBINGER][1][f]);
    // Tall dark mantle — built from polygon for organic tapered shape
    // Narrow at top, bulges wide in middle, narrows at base
    pd->graphics->drawLine(22, 0, 14, 6, 1, kColorWhite);   // top-left slope
    pd->graphics->drawLine(30, 0, 38, 6, 1, kColorWhite);   // top-right slope
    pd->graphics->drawLine(14, 6, 6, 16, 1, kColorWhite);   // left bulge out
    pd->graphics->drawLine(38, 6, 46, 16, 1, kColorWhite);   // right bulge out
    pd->graphics->drawLine(6, 16, 10, 26, 1, kColorWhite);   // left narrows
    pd->graphics->drawLine(46, 16, 42, 26, 1, kColorWhite);  // right narrows
    pd->graphics->drawLine(22, 0, 30, 0, 1, kColorWhite);    // top cap
    // Fill mantle black
    for (int py = 1; py < 26; py++) {
        int lx, rx;
        if (py < 6) {
            float t = (float)py / 6.0f;
            lx = 22 - (int)(8.0f * t);
            rx = 30 + (int)(8.0f * t);
        } else if (py < 16) {
            float t = (float)(py - 6) / 10.0f;
            lx = 14 - (int)(8.0f * t);
            rx = 38 + (int)(8.0f * t);
        } else {
            float t = (float)(py - 16) / 10.0f;
            lx = 6 + (int)(4.0f * t);
            rx = 46 - (int)(4.0f * t);
        }
        if (lx < rx) GFX_FILL(lx+1, py, rx - lx - 1, 1, kColorBlack);
    }
    // Mantle ridges/texture — subtle horizontal lines for organic feel
    pd->graphics->drawLine(16, 8, 36, 8, 1, kColorWhite);
    pd->graphics->drawLine(10, 14, 42, 14, 1, kColorWhite);
    pd->graphics->drawLine(12, 20, 40, 20, 1, kColorWhite);
    // Dot eyes — peering from the dark mass
    GFX_FILL(18+f, 10, 3, 3, kColorWhite);
    GFX_FILL(31+f, 10, 3, 3, kColorWhite);
    // 8 thick tentacles — spread wide from narrowed base
    int ts = f ? 3 : -3;
    // Far left — reaches out and curls
    pd->graphics->drawLine(10, 26, 2+ts, 38, 1, kColorWhite);
    pd->graphics->drawLine(2+ts, 38, 0+ts, 46, 1, kColorWhite);
    pd->graphics->drawLine(0+ts, 46, 4+ts, 50, 1, kColorWhite);
    // Mid left
    pd->graphics->drawLine(16, 26, 8-ts, 40, 1, kColorWhite);
    pd->graphics->drawLine(8-ts, 40, 6-ts, 50, 1, kColorWhite);
    // Inner left
    pd->graphics->drawLine(20, 26, 16+ts, 42, 1, kColorWhite);
    pd->graphics->drawLine(16+ts, 42, 14+ts, 52, 1, kColorWhite);
    // Center left
    pd->graphics->drawLine(24, 26, 22-ts, 44, 1, kColorWhite);
    pd->graphics->drawLine(22-ts, 44, 20-ts, 52, 1, kColorWhite);
    // Center right
    pd->graphics->drawLine(28, 26, 30+ts, 44, 1, kColorWhite);
    pd->graphics->drawLine(30+ts, 44, 32+ts, 52, 1, kColorWhite);
    // Inner right
    pd->graphics->drawLine(32, 26, 36-ts, 42, 1, kColorWhite);
    pd->graphics->drawLine(36-ts, 42, 38-ts, 52, 1, kColorWhite);
    // Mid right
    pd->graphics->drawLine(36, 26, 44+ts, 40, 1, kColorWhite);
    pd->graphics->drawLine(44+ts, 40, 46+ts, 50, 1, kColorWhite);
    // Far right — reaches out and curls
    pd->graphics->drawLine(42, 26, 50-ts, 38, 1, kColorWhite);
    pd->graphics->drawLine(50-ts, 38, 52-ts, 46, 1, kColorWhite);
    pd->graphics->drawLine(52-ts, 46, 48-ts, 50, 1, kColorWhite);
    // Sucker dots along outer tentacles
    GFX_FILL(1+ts, 42, 1, 1, kColorWhite);
    GFX_FILL(0+ts, 46, 1, 1, kColorWhite);
    GFX_FILL(51-ts, 42, 1, 1, kColorWhite);
    GFX_FILL(52-ts, 46, 1, 1, kColorWhite);
    GFX_POP();
    }
}

static void create_vfx_images(void)
{
    // Vortex Mask (48x48 dithered spiral) for Riptide and Brine Splash
    vortexMask = make_bitmap(48, 48);
    GFX_PUSH(vortexMask);
    for (int i = 0; i < 360; i += 15) {
        float angle = i * 3.14159f / 180.0f;
        for (int r = 4; r < 24; r += 1) {
            float spiral = angle + r * 0.2f;
            int x = 24 + (int)(r * cosf(spiral));
            int y = 24 + (int)(r * sinf(spiral));
            if ((x + y + r) % 2 == 0) GFX_FILL(x, y, 1, 1, kColorWhite);
        }
    }
    GFX_POP();

    // Ripple Mask (32x32 concentric dithered arcs) for Foghorn
    rippleMask = make_bitmap(32, 32);
    GFX_PUSH(rippleMask);
    for (int r = 8; r < 16; r += 4) {
        for (int a = 0; a < 360; a += 10) {
            int x = 16 + (int)(r * cosf(a * 3.14159f / 180.0f));
            int y = 16 + (int)(r * sinf(a * 3.14159f / 180.0f));
            if ((x + y) % 2 == 0) GFX_FILL(x, y, 2, 2, kColorWhite);
        }
    }
    GFX_POP();

    // Bolt Sprite (16x8 jagged electrical segment) for Chain Bolt
    boltSprite = make_bitmap(16, 8);
    GFX_PUSH(boltSprite);
    pd->graphics->drawLine(0, 4, 6, 2, 2, kColorWhite);
    pd->graphics->drawLine(6, 2, 10, 6, 2, kColorWhite);
    pd->graphics->drawLine(10, 6, 16, 4, 2, kColorWhite);
    GFX_POP();

    // Wisp Sprite (8x8 flickering orb) for Ghost Light
    wispSprite = make_bitmap(8, 8);
    GFX_PUSH(wispSprite);
    GFX_FILL(2, 0, 4, 1, kColorWhite);
    GFX_FILL(1, 1, 6, 6, kColorWhite);
    GFX_FILL(2, 7, 4, 1, kColorWhite);
    GFX_FILL(3, 3, 2, 2, kColorBlack); // Hollow core for flicker
    GFX_POP();

    // Drop Shadow (8x4 small dithered oval)
    dropShadow = make_bitmap(8, 4);
    GFX_PUSH(dropShadow);
    for (int y = 0; y < 4; y++) {
        for (int x = 1; x < 7; x++) {
            if ((x + y) % 2 == 0) GFX_FILL(x, y, 1, 1, kColorWhite);
        }
    }
    GFX_POP();

    // Vignette Mask (400x240 full screen)
    // Dark dithered edges to focus on center
    vignetteMask = make_bitmap(SCREEN_W, SCREEN_H);
    GFX_PUSH(vignetteMask);
    GFX_CLEAR(kColorClear);
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            float dx = (float)x - (SCREEN_W / 2);
            float dy = (float)y - (SCREEN_H / 2);
            float distSq = dx * dx + dy * dy;
            float rMax = 180.0f;
            if (distSq > rMax * rMax) {
                // Higher density dither outside center
                float radial = (sqrtf(distSq) - rMax) / 100.0f;
                int threshold = (int)(radial * 16.0f);
                if ((x * 13 + y * 7) % 16 < threshold) {
                    GFX_FILL(x, y, 1, 1, kColorWhite);
                }
            }
        }
    }
    GFX_POP();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void images_init(void)
{
    create_tile_images();
    create_enemy_images();
    create_player_images();
    create_misc_images();
    create_vfx_images();
    create_design_images();
    create_enemy_design_images();
    DLOG("Images initialized: %d enemy types, 4 player frames, 6 designs, %d enemy designs, 7 tiles", ENEMY_TYPE_COUNT, ENEMY_TYPE_COUNT * ENEMY_DESIGN_COUNT);
}

LCDBitmap* images_get_enemy(EnemyType type, int frame) { return enemyImgs[type][frame & 1]; }
LCDBitmap* images_get_player_frame(int frame) { return playerFrames[frame % 4]; }
LCDBitmap* images_get_bullet(void) { return bulletImgs[0]; }
LCDBitmap* images_get_enemy_bullet(void) { return enemyBulletImg; }
LCDBitmap* images_get_xp_gem(int small) { return small ? xpGemImgSmall : xpGemImg; }
LCDBitmap* images_get_harpoon(void) { return bulletImgs[1]; }
LCDBitmap* images_get_anchor(void) { return anchorImg; }
LCDBitmap* images_get_ghost_light(void) { return bulletImgs[2]; }
LCDBitmap* images_get_crate(void) { return crateImg; }
LCDBitmap* images_get_pickup(PickupType type) { return (type >= 0 && type < PICKUP_TYPE_COUNT) ? pickupImgs[type] : pickupImgs[0]; }
LCDBitmap* images_get_chain_bolt(void) { return bulletImgs[3]; }
LCDBitmap* images_get_depth_charge(void) { return depthChargeImg; }
LCDBitmap* images_get_tile(int tileIdx) { return tileImgs[clampi(tileIdx, 0, 7)]; }
LCDBitmap* images_get_weapon_icon(WeaponId id) { return weaponIconImgs[id]; }
LCDBitmap* images_get_weapon_icon_large(WeaponId id) { return (id >= 0 && id < WEAPON_COUNT) ? weaponIconLargeImgs[id] : NULL; }
LCDBitmap* images_get_passive_icon(int idx) { return passiveIconImgs[clampi(idx, 0, 4)]; }
LCDBitmap* images_get_passive_icon_large(int idx) { return (idx >= 0 && idx < 5) ? passiveIconLargeImgs[idx] : NULL; }
LCDBitmap* images_get_vortex_mask(void) { return vortexMask; }
LCDBitmap* images_get_ripple_mask(void) { return rippleMask; }
LCDBitmap* images_get_bolt_sprite(void) { return boltSprite; }
LCDBitmap* images_get_wisp_sprite(void) { return wispSprite; }
LCDBitmap* images_get_drop_shadow(void) { return dropShadow; }
LCDBitmap* images_get_vignette(void) { return vignetteMask; }
LCDBitmap* images_get_design_frame(int design, int frame) {
    return designFrames[clampi(design, 0, DESIGN_COUNT-1)][frame % 4];
}
const char* images_get_design_name(int design) {
    return designNames[clampi(design, 0, DESIGN_COUNT-1)];
}
int images_get_design_count(void) { return DESIGN_COUNT; }
LCDBitmap* images_get_enemy_design(EnemyType type, int design, int frame) {
    return enemyDesignImgs[clampi(type, 0, ENEMY_TYPE_COUNT-1)][clampi(design, 0, ENEMY_DESIGN_COUNT-1)][frame & 1];
}
const char* images_get_enemy_design_label(int design) {
    return enemyDesignLabels[clampi(design, 0, ENEMY_DESIGN_COUNT-1)];
}
int images_get_enemy_design_count(void) { return ENEMY_DESIGN_COUNT; }
int images_get_enemy_half_w(EnemyType type) { return enemyHW[type]; }
int images_get_enemy_half_h(EnemyType type) { return enemyHH[type]; }
