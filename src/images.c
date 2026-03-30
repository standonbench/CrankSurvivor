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
// All procedural bitmaps
// ---------------------------------------------------------------------------
static LCDBitmap* enemyImgs[ENEMY_TYPE_COUNT];
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
// Enemy sprites
// ---------------------------------------------------------------------------
static void create_enemy_images(void)
{
    // Creeper (30x30) — "Trench Skitterer" crustacean
    enemyImgs[ENEMY_CREEPER] = make_bitmap(30, 30);
    GFX_PUSH(enemyImgs[ENEMY_CREEPER]);
    // Core body (carapace)
    GFX_ELLIPSE(5, 4, 20, 16, kColorWhite);
    GFX_FILL(7, 6, 16, 12, kColorBlack); 
    
    // Carapace ridges
    GFX_FILL(9, 8, 12, 1, kColorWhite);
    GFX_FILL(11, 11, 8, 1, kColorWhite);
    
    // Eyes cluster
    GFX_FILL(11, 15, 2, 2, kColorWhite);
    GFX_FILL(14, 14, 2, 2, kColorWhite);
    GFX_FILL(17, 15, 2, 2, kColorWhite);

    // Legs
    // Left
    pd->graphics->drawLine(6, 8, 1, 5, 1, kColorWhite);
    pd->graphics->drawLine(1, 5, 0, 10, 1, kColorWhite);
    pd->graphics->drawLine(5, 12, 0, 12, 1, kColorWhite);
    pd->graphics->drawLine(0, 12, 1, 17, 1, kColorWhite);
    pd->graphics->drawLine(6, 15, 2, 18, 1, kColorWhite);
    pd->graphics->drawLine(2, 18, 3, 23, 1, kColorWhite);
    pd->graphics->drawLine(9, 20, 6, 25, 1, kColorWhite); // Pincer
    pd->graphics->drawLine(6, 25, 9, 28, 1, kColorWhite);
    
    // Right
    pd->graphics->drawLine(24, 8, 29, 5, 1, kColorWhite);
    pd->graphics->drawLine(29, 5, 30, 10, 1, kColorWhite);
    pd->graphics->drawLine(25, 12, 30, 12, 1, kColorWhite);
    pd->graphics->drawLine(30, 12, 29, 17, 1, kColorWhite);
    pd->graphics->drawLine(24, 15, 28, 18, 1, kColorWhite);
    pd->graphics->drawLine(28, 18, 27, 23, 1, kColorWhite);
    pd->graphics->drawLine(21, 20, 24, 25, 1, kColorWhite); // Pincer
    pd->graphics->drawLine(24, 25, 21, 28, 1, kColorWhite);
    GFX_POP();

    // Tendril (44x44) — "Drowned Sailor" skull with jellyfish tentacles
    enemyImgs[ENEMY_TENDRIL] = make_bitmap(44, 44);
    GFX_PUSH(enemyImgs[ENEMY_TENDRIL]);
    // Skull top
    GFX_FILL(14, 4, 16, 12, kColorWhite);
    // Skull cheekbones
    GFX_FILL(12, 10, 4, 8, kColorWhite);
    GFX_FILL(28, 10, 4, 8, kColorWhite);
    GFX_FILL(18, 16, 8, 8, kColorWhite); // Upper jaw
    
    // Eye sockets (deep black)
    GFX_FILL(16, 10, 4, 5, kColorBlack);
    GFX_FILL(24, 10, 4, 5, kColorBlack);
    // Nose cavity
    GFX_FILL(21, 16, 2, 3, kColorBlack);

    // Jellyfish bell over skull
    pd->graphics->drawEllipse(4, 0, 36, 20, 1, 180, 0, kColorWhite);
    GFX_FILL(4, 10, 10, 1, kColorWhite);
    GFX_FILL(30, 10, 10, 1, kColorWhite);

    // Bell texture: clean arc lines instead of per-pixel dither
    pd->graphics->drawEllipse(10, 2, 24, 12, 1, 200, 340, kColorWhite);
    pd->graphics->drawEllipse(14, 3, 16, 8, 1, 210, 330, kColorWhite);

    // Flowing tentacles (5 — cleaner silhouette)
    for (int i = 0; i < 5; i++) {
        int tx = 10 + i * 6;
        int ty = 18 + (i%2)*2;
        int len = 16 + (i%3)*2;
        pd->graphics->drawLine(tx, ty, tx - 2 + (i%2)*4, ty + len, 1, kColorWhite);
        GFX_FILL(tx - 3 + (i%2)*4, ty + len/2, 2, 2, kColorWhite);
        GFX_FILL(tx - 4 + (i%2)*4, ty + len, 2, 2, kColorWhite);
    }
    GFX_POP();

    // Wraith (40x40) — "Abyssal Banshee" shrieking face with fading torn cloak
    enemyImgs[ENEMY_WRAITH] = make_bitmap(40, 40);
    GFX_PUSH(enemyImgs[ENEMY_WRAITH]);
    // Hollow hood silhouette
    GFX_ELLIPSE(6, 2, 28, 26, kColorWhite);
    GFX_FILL(8, 4, 24, 22, kColorBlack);

    // Shrieking face (skull-like, stretched)
    GFX_FILL(14, 8, 12, 16, kColorWhite);
    // Deep black eyes, slanted
    pd->graphics->drawLine(15, 10, 18, 12, 2, kColorBlack);
    pd->graphics->drawLine(25, 10, 22, 12, 2, kColorBlack);
    // Shrieking mouth (long black oval)
    GFX_FILL(18, 15, 4, 7, kColorBlack);

    // Tattered cloak draped from hood
    GFX_FILL(4, 18, 5, 14, kColorWhite);
    GFX_FILL(31, 18, 5, 14, kColorWhite);
    GFX_FILL(8, 22, 4, 10, kColorWhite);
    GFX_FILL(28, 22, 4, 10, kColorWhite);
    GFX_FILL(12, 26, 16, 6, kColorWhite);
    
    // Horizontal fade stripes for tattered robe bottom
    GFX_FILL(6, 32, 28, 1, kColorWhite);
    GFX_FILL(8, 34, 24, 1, kColorWhite);
    GFX_FILL(12, 36, 16, 1, kColorWhite);
    GFX_FILL(16, 38, 8, 1, kColorWhite);
    GFX_POP();

    // Abyssal (56x56) — "Leviathan Maw" deep-sea angler bursting from below
    enemyImgs[ENEMY_ABYSSAL] = make_bitmap(56, 56);
    GFX_PUSH(enemyImgs[ENEMY_ABYSSAL]);
    // Massive jaw silhouette
    pd->graphics->fillPolygon(5, (int[]){12,56, 44,56, 52,24, 28,12, 4,24}, kColorWhite, kPolygonFillNonZero);
    GFX_FILL(16, 18, 24, 34, kColorBlack); // Deep black mouth cavity
    
    // Rows of jagged teeth (White on black)
    // Upper row
    pd->graphics->fillPolygon(3, (int[]){18,18, 22,28, 26,18}, kColorWhite, kPolygonFillNonZero);
    pd->graphics->fillPolygon(3, (int[]){26,18, 30,28, 34,18}, kColorWhite, kPolygonFillNonZero);
    pd->graphics->fillPolygon(3, (int[]){34,18, 38,28, 42,18}, kColorWhite, kPolygonFillNonZero);
    // Lower row
    pd->graphics->fillPolygon(3, (int[]){20,52, 24,38, 28,52}, kColorWhite, kPolygonFillNonZero);
    pd->graphics->fillPolygon(3, (int[]){28,52, 32,38, 36,52}, kColorWhite, kPolygonFillNonZero);
    
    // Head stalk & Glowing Lure (Anglerfish part)
    pd->graphics->drawLine(28, 12, 28, 2, 2, kColorWhite);
    pd->graphics->drawEllipse(24, 0, 8, 8, 2, 0, 360, kColorWhite);
    
    // Tiny deep-set eyes
    GFX_ELLIPSE(10, 24, 6, 6, kColorBlack);
    GFX_FILL(12, 26, 2, 2, kColorWhite);
    GFX_ELLIPSE(40, 24, 6, 6, kColorBlack);
    GFX_FILL(42, 26, 2, 2, kColorWhite);
    
    // Scale texture: simple vertical marks on flanks
    pd->graphics->drawLine(8, 30, 8, 44, 1, kColorBlack);
    pd->graphics->drawLine(12, 34, 12, 50, 1, kColorBlack);
    pd->graphics->drawLine(44, 30, 44, 44, 1, kColorBlack);
    pd->graphics->drawLine(48, 34, 48, 50, 1, kColorBlack);
    GFX_POP();

    // Seer (48x48) — "Watcher Entity" biologically accurate grotesque giant eyeball
    enemyImgs[ENEMY_SEER] = make_bitmap(48, 48);
    GFX_PUSH(enemyImgs[ENEMY_SEER]);
    
    // Optic nerve stalks (2px — consistent with Creeper style)
    pd->graphics->drawLine(24, 24, 4, 8, 2, kColorWhite);
    pd->graphics->drawLine(24, 24, 44, 6, 2, kColorWhite);
    pd->graphics->drawLine(24, 24, 6, 40, 2, kColorWhite);
    pd->graphics->drawLine(24, 24, 42, 42, 2, kColorWhite);
    pd->graphics->fillEllipse(2, 6, 6, 6, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(42, 4, 6, 6, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(4, 38, 6, 6, 0, 360, kColorWhite);
    pd->graphics->fillEllipse(40, 40, 6, 6, 0, 360, kColorWhite);

    // Main eyeball
    GFX_ELLIPSE(6, 6, 36, 36, kColorWhite);

    // Vein lines (4 simple curves instead of loop)
    pd->graphics->drawLine(10, 14, 20, 20, 1, kColorBlack);
    pd->graphics->drawLine(38, 14, 28, 20, 1, kColorBlack);
    pd->graphics->drawLine(10, 34, 20, 28, 1, kColorBlack);
    pd->graphics->drawLine(38, 34, 28, 28, 1, kColorBlack);

    // Iris ring
    GFX_ELLIPSE(14, 14, 20, 20, kColorBlack);
    // Iris texture (4 cross lines instead of 12 spokes)
    pd->graphics->drawLine(24, 16, 24, 32, 1, kColorWhite);
    pd->graphics->drawLine(16, 24, 32, 24, 1, kColorWhite);
    pd->graphics->drawLine(18, 18, 30, 30, 1, kColorWhite);
    pd->graphics->drawLine(30, 18, 18, 30, 1, kColorWhite);
    // Deep pupil
    pd->graphics->fillEllipse(18, 18, 12, 12, 0, 360, kColorBlack);
    // Sharp glint
    pd->graphics->fillEllipse(24, 20, 4, 4, 0, 360, kColorWhite);
    GFX_POP();

    // Lamprey (36x36) — "Horrifying Maw" concentric rings of teeth
    enemyImgs[ENEMY_LAMPREY] = make_bitmap(36, 36);
    GFX_PUSH(enemyImgs[ENEMY_LAMPREY]);
    // Outer fleshy ring
    pd->graphics->drawEllipse(2, 2, 32, 32, 2, 0, 360, kColorWhite);
    // Inner fleshy ring
    pd->graphics->drawEllipse(6, 6, 24, 24, 2, 0, 360, kColorWhite);
    // Black abyss center
    pd->graphics->fillEllipse(10, 10, 16, 16, 0, 360, kColorBlack);

    // Concentric teeth layers pointing inward to center (18,18)
    for (int a = 0; a < 360; a += 45) {
        float radOuter = 12.0f;
        float radInner = 8.0f;
        int ox = 18 + radOuter * cosf(a * 3.14159f / 180.0f);
        int oy = 18 + radOuter * sinf(a * 3.14159f / 180.0f);
        int ix = 18 + radInner * cosf(a * 3.14159f / 180.0f);
        int iy = 18 + radInner * sinf(a * 3.14159f / 180.0f);
        pd->graphics->drawLine(ox, oy, ix, iy, 2, kColorWhite);
    }
    for (int a = 22; a < 360; a += 45) {
        float radOuter = 8.0f;
        float radInner = 4.0f;
        int ox = 18 + radOuter * cosf(a * 3.14159f / 180.0f);
        int oy = 18 + radOuter * sinf(a * 3.14159f / 180.0f);
        int ix = 18 + radInner * cosf(a * 3.14159f / 180.0f);
        int iy = 18 + radInner * sinf(a * 3.14159f / 180.0f);
        pd->graphics->drawLine(ox, oy, ix, iy, 1, kColorWhite);
    }
    
    // Outer spikes (4 cardinal points)
    for (int a = 0; a < 360; a += 90) {
        int x = 18 + 18 * cos(a * 3.1415/180);
        int y = 18 + 18 * sin(a * 3.1415/180);
        GFX_FILL(x-1, y-1, 3, 3, kColorWhite);
    }
    GFX_POP();

    // Bloat (44x44) — "Putrid Swell" rotting husk wrapped in chains
    enemyImgs[ENEMY_BLOAT] = make_bitmap(44, 44);
    GFX_PUSH(enemyImgs[ENEMY_BLOAT]);
    // Massive bloated spherical body
    pd->graphics->fillEllipse(2, 4, 38, 34, 0, 360, kColorWhite);
    // Large dark festering pustules (BLACK on white body = sunken)
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
    // Small angry eyes
    GFX_FILL(14, 14, 4, 4, kColorBlack);
    GFX_FILL(15, 15, 2, 2, kColorWhite);
    GFX_FILL(24, 13, 4, 4, kColorBlack);
    GFX_FILL(25, 14, 2, 2, kColorWhite);
    // Wide leering smile (simplified 4 segments)
    GFX_FILL(12, 22, 2, 1, kColorBlack); GFX_FILL(16, 21, 2, 1, kColorBlack);
    GFX_FILL(20, 22, 2, 1, kColorBlack); GFX_FILL(24, 21, 2, 1, kColorBlack);
    GFX_POP();

    // Harbinger (52x52) — "Deep King" eldritch deity with coral crown
    enemyImgs[ENEMY_HARBINGER] = make_bitmap(52, 52);
    GFX_PUSH(enemyImgs[ENEMY_HARBINGER]);

    // Tall robe/cloak silhouette
    pd->graphics->fillPolygon(5, (int[]){10,52, 42,52, 46,24, 26,16, 6,24}, kColorWhite, kPolygonFillNonZero);
    
    // Inner dark void
    pd->graphics->fillPolygon(5, (int[]){14,52, 38,52, 40,26, 26,20, 12,26}, kColorBlack, kPolygonFillNonZero);

    // Segmented armor plating on torso
    GFX_FILL(15, 28, 22, 2, kColorWhite); // plate1
    GFX_FILL(16, 32, 20, 2, kColorWhite); // plate2
    GFX_FILL(17, 36, 18, 2, kColorWhite); // plate3
    GFX_FILL(18, 40, 16, 2, kColorWhite); // plate4

    // Head (skull-like)
    GFX_FILL(18, 12, 16, 12, kColorWhite); // head block
    GFX_FILL(20, 14, 12, 10, kColorBlack); // hollow inside
    
    // Jagged Coral Crown (3 spikes)
    pd->graphics->drawLine(22, 12, 19, 2, 2, kColorWhite);
    pd->graphics->drawLine(26, 11, 26, 0, 2, kColorWhite);
    pd->graphics->drawLine(30, 12, 33, 2, 2, kColorWhite);
    // Crown gems
    GFX_FILL(18, 1, 3, 3, kColorWhite);
    GFX_FILL(25, 0, 3, 3, kColorWhite);
    GFX_FILL(32, 1, 3, 3, kColorWhite);

    // Piercing hollow eyes (white inside black head)
    GFX_FILL(21, 16, 4, 5, kColorWhite);
    GFX_FILL(22, 17, 2, 3, kColorBlack);
    GFX_FILL(27, 16, 4, 5, kColorWhite);
    GFX_FILL(28, 17, 2, 3, kColorBlack);
    
    // Claw arms protruding from the cloak
    // Left claw
    pd->graphics->drawLine(10, 28, 2, 22, 2, kColorWhite);
    pd->graphics->drawLine(2, 22, 0, 17, 1, kColorWhite); // upper claw
    pd->graphics->drawLine(2, 22, 5, 16, 1, kColorWhite);
    // Right claw
    pd->graphics->drawLine(42, 28, 50, 22, 2, kColorWhite);
    pd->graphics->drawLine(50, 22, 52, 17, 1, kColorWhite);
    pd->graphics->drawLine(50, 22, 47, 16, 1, kColorWhite);
    GFX_POP();
}

// ---------------------------------------------------------------------------
// Player frames (32x32, 4 frames)
// ---------------------------------------------------------------------------
static void create_player_images(void)
{
    for (int f = 0; f < 4; f++) {
        playerFrames[f] = make_bitmap(30, 30);
        GFX_PUSH(playerFrames[f]);

        int bob = (f == 1 || f == 3) ? 1 : 0;

        // Hat brim (thin line)
        GFX_FILL(7, 7+bob, 16, 1, kColorWhite);
        // Hat crown (ellipse, dark interior)
        pd->graphics->drawEllipse(9, 1+bob, 12, 7, 1, 0, 360, kColorWhite);
        GFX_FILL(10, 3+bob, 10, 4, kColorBlack);

        // Face (dark void, dot eyes)
        GFX_FILL(10, 8+bob, 10, 5, kColorBlack);
        GFX_FILL(12, 10+bob, 2, 1, kColorWhite);
        GFX_FILL(16, 10+bob, 2, 1, kColorWhite);

        // Coat (ellipse outline, dark interior — tapered slicker)
        pd->graphics->drawEllipse(6, 12+bob, 18, 14, 1, 0, 360, kColorWhite);
        GFX_FILL(8, 14+bob, 14, 10, kColorBlack);
        GFX_FILL(15, 13+bob, 1, 12, kColorWhite); // center seam
        GFX_FILL(9, 19+bob, 12, 1, kColorWhite);  // belt

        // Arms (1px lines)
        int armOff = (f == 0) ? -1 : (f == 2) ? 1 : 0;
        pd->graphics->drawLine(9, 16+bob, 4+armOff, 21+bob, 1, kColorWhite);
        int lx = 25 - armOff;
        int ly = 20 + bob;
        pd->graphics->drawLine(21, 16+bob, lx, ly, 1, kColorWhite);

        // Lantern (tiny 3x3)
        GFX_FILL(lx-1, ly, 3, 3, kColorWhite);
        GFX_FILL(lx, ly+1, 1, 1, kColorBlack);

        // Legs (1px lines)
        int step = (f == 1) ? 2 : (f == 3) ? -2 : 0;
        pd->graphics->drawLine(12, 25+bob, 10+step, 29, 1, kColorWhite);
        pd->graphics->drawLine(18, 25+bob, 20-step, 29, 1, kColorWhite);

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
    DLOG("Images initialized: %d enemy types, 4 player frames, 7 tiles", ENEMY_TYPE_COUNT);
}

LCDBitmap* images_get_enemy(EnemyType type) { return enemyImgs[type]; }
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
int images_get_enemy_half_w(EnemyType type) { return enemyHW[type]; }
int images_get_enemy_half_h(EnemyType type) { return enemyHH[type]; }
