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
static LCDBitmap* pickupImgs[PICKUP_TYPE_COUNT];
static LCDBitmap* depthChargeImg;

static int enemyHW[ENEMY_TYPE_COUNT] = { 18, 21, 18, 27, 24, 17, 20, 23 };
static int enemyHH[ENEMY_TYPE_COUNT] = { 18, 21, 18, 27, 24, 17, 20, 23 };

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

    // Void (types 6 and 7)
    tileImgs[6] = make_bitmap(TILE_SIZE, TILE_SIZE);
    GFX_PUSH(tileImgs[6]);
    GFX_CLEAR(kColorBlack);
    GFX_POP();
    tileImgs[7] = tileImgs[6];
}

// ---------------------------------------------------------------------------
// Enemy sprites
// ---------------------------------------------------------------------------
static void create_enemy_images(void)
{
    // Creeper (36x36) — eyeless crawler with tentacle wisps
    enemyImgs[ENEMY_CREEPER] = make_bitmap(36, 36);
    GFX_PUSH(enemyImgs[ENEMY_CREEPER]);
    GFX_ELLIPSE(6, 2, 24, 21, kColorWhite);
    // Body dithering (texture)
    for (int y = 5; y < 20; y += 3)
        for (int x = 9; x < 27; x += 4)
            GFX_FILL(x, y, 1, 1, kColorBlack);
    // Tentacle wisps
    GFX_FILL(8, 21, 3, 11, kColorWhite);
    GFX_FILL(11, 27, 2, 6, kColorWhite);
    GFX_FILL(15, 21, 3, 9, kColorWhite);
    GFX_FILL(24, 21, 3, 11, kColorWhite);
    GFX_FILL(21, 27, 2, 6, kColorWhite);
    // Eye slits
    GFX_FILL(12, 8, 3, 6, kColorBlack);
    GFX_FILL(21, 8, 3, 6, kColorBlack);
    // Jagged mouth
    GFX_FILL(14, 18, 2, 2, kColorBlack);
    GFX_FILL(17, 17, 2, 3, kColorBlack);
    GFX_FILL(20, 18, 2, 2, kColorBlack);
    GFX_POP();

    // Tendril (42x42) — drowned sailor's regret with suction cups
    enemyImgs[ENEMY_TENDRIL] = make_bitmap(42, 42);
    GFX_PUSH(enemyImgs[ENEMY_TENDRIL]);
    GFX_ELLIPSE(6, 0, 30, 24, kColorWhite);
    // Body dithering
    for (int y = 3; y < 21; y += 3)
        for (int x = 9; x < 33; x += 4)
            GFX_FILL(x, y, 1, 1, kColorBlack);
    // Tentacles with suction cups
    GFX_FILL(8, 21, 3, 18, kColorWhite);
    GFX_FILL(8, 27, 1, 1, kColorBlack);
    GFX_FILL(9, 33, 1, 1, kColorBlack);
    GFX_FILL(15, 21, 3, 17, kColorWhite);
    GFX_FILL(15, 29, 1, 1, kColorBlack);
    GFX_FILL(17, 35, 1, 1, kColorBlack);
    GFX_FILL(23, 21, 3, 18, kColorWhite);
    GFX_FILL(23, 26, 1, 1, kColorBlack);
    GFX_FILL(24, 32, 1, 1, kColorBlack);
    GFX_FILL(30, 21, 3, 15, kColorWhite);
    GFX_FILL(30, 30, 1, 1, kColorBlack);
    // Eyes
    GFX_FILL(15, 6, 5, 6, kColorBlack);
    GFX_FILL(24, 6, 5, 6, kColorBlack);
    GFX_FILL(17, 8, 2, 2, kColorWhite);
    GFX_FILL(26, 8, 2, 2, kColorWhite);
    GFX_POP();

    // Wraith (36x36) — spectral with wispy dithered trail
    enemyImgs[ENEMY_WRAITH] = make_bitmap(36, 36);
    GFX_PUSH(enemyImgs[ENEMY_WRAITH]);
    GFX_ELLIPSE(3, 0, 30, 24, kColorWhite);
    // Flowing tattered wisps
    for (int i = 0; i < 5; i++) {
        int bx = 5 + i * 6;
        int bh = 6 + (i % 2) * 5;
        GFX_FILL(bx, 21, 3, bh, kColorWhite);
    }
    // Dithered fade at bottom
    for (int y = 29; y < 36; y++)
        for (int x = 3; x < 33; x += 2)
            if ((x + y) % 3 == 0) GFX_FILL(x, y, 1, 1, kColorWhite);
    // Hollow outlined eyes with glint
    GFX_RECT(11, 6, 6, 8, kColorBlack);
    GFX_FILL(14, 9, 2, 2, kColorWhite);
    GFX_RECT(20, 6, 6, 8, kColorBlack);
    GFX_FILL(23, 9, 2, 2, kColorWhite);
    GFX_POP();

    // Abyssal (54x54) — massive with scales and teeth
    enemyImgs[ENEMY_ABYSSAL] = make_bitmap(54, 54);
    GFX_PUSH(enemyImgs[ENEMY_ABYSSAL]);
    GFX_ELLIPSE(3, 3, 48, 36, kColorWhite);
    // Scale pattern (diagonal dither)
    for (int y = 8; y < 33; y += 4)
        for (int x = 6 + (y / 4) % 2; x < 48; x += 5)
            GFX_FILL(x, y, 1, 1, kColorBlack);
    // Widening tentacles
    GFX_FILL(9, 36, 6, 6, kColorWhite);
    GFX_FILL(8, 42, 9, 6, kColorWhite);
    GFX_FILL(39, 36, 6, 6, kColorWhite);
    GFX_FILL(38, 42, 9, 6, kColorWhite);
    // Eyes (deep-set)
    GFX_FILL(15, 12, 8, 8, kColorBlack);
    GFX_FILL(18, 15, 3, 3, kColorWhite);
    GFX_FILL(33, 12, 8, 8, kColorBlack);
    GFX_FILL(36, 15, 3, 3, kColorWhite);
    // Teeth row
    GFX_FILL(21, 26, 12, 3, kColorBlack);
    GFX_FILL(23, 25, 2, 2, kColorWhite);
    GFX_FILL(26, 25, 2, 2, kColorWhite);
    GFX_FILL(29, 25, 2, 2, kColorWhite);
    GFX_POP();

    // Seer (48x48) — hypnotic eye with eyelid and vein marks
    enemyImgs[ENEMY_SEER] = make_bitmap(48, 48);
    GFX_PUSH(enemyImgs[ENEMY_SEER]);
    GFX_ELLIPSE(6, 6, 36, 36, kColorWhite);
    GFX_ELLIPSE(15, 15, 18, 18, kColorBlack);
    GFX_ELLIPSE(20, 20, 9, 9, kColorWhite);
    // Eyelid arcs
    GFX_FILL(9, 5, 30, 2, kColorWhite);
    GFX_FILL(12, 3, 24, 2, kColorWhite);
    GFX_FILL(9, 42, 30, 2, kColorWhite);
    GFX_FILL(12, 44, 24, 2, kColorWhite);
    // Vein marks radiating outward
    GFX_FILL(5, 23, 5, 2, kColorWhite);
    GFX_FILL(39, 23, 5, 2, kColorWhite);
    GFX_FILL(8, 12, 2, 3, kColorWhite);
    GFX_FILL(39, 12, 2, 3, kColorWhite);
    GFX_FILL(8, 33, 2, 3, kColorWhite);
    GFX_FILL(39, 33, 2, 3, kColorWhite);
    GFX_POP();

    // Lamprey (33x33) — circular mouth with radiating teeth
    enemyImgs[ENEMY_LAMPREY] = make_bitmap(34, 34);
    GFX_PUSH(enemyImgs[ENEMY_LAMPREY]);
    GFX_ELLIPSE(2, 5, 30, 24, kColorWhite);
    // Spine ridge along top
    GFX_FILL(8, 2, 2, 5, kColorWhite);
    GFX_FILL(12, 0, 2, 6, kColorWhite);
    GFX_FILL(17, 0, 2, 6, kColorWhite);
    GFX_FILL(21, 2, 2, 5, kColorWhite);
    // Circular mouth with teeth
    GFX_FILL(6, 14, 9, 6, kColorBlack);
    GFX_FILL(5, 15, 2, 3, kColorWhite);   // left tooth
    GFX_FILL(15, 15, 2, 3, kColorWhite);  // right tooth
    GFX_FILL(9, 12, 3, 2, kColorWhite);   // top tooth
    GFX_FILL(9, 20, 3, 2, kColorWhite);   // bottom tooth
    // Eye
    GFX_FILL(21, 12, 5, 5, kColorBlack);
    GFX_FILL(23, 14, 2, 2, kColorWhite);
    // Scale texture
    for (int y = 8; y < 26; y += 4)
        for (int x = 18; x < 30; x += 4)
            GFX_FILL(x, y, 1, 1, kColorBlack);
    GFX_POP();

    // Bloat (39x39) — swollen with pustules and vein lines
    enemyImgs[ENEMY_BLOAT] = make_bitmap(40, 40);
    GFX_PUSH(enemyImgs[ENEMY_BLOAT]);
    GFX_ELLIPSE(2, 0, 36, 33, kColorWhite);
    // Asymmetric bulge
    GFX_ELLIPSE(27, 8, 11, 12, kColorWhite);
    // Stubby tentacles
    for (int i = 0; i < 4; i++) GFX_FILL(6 + i * 8, 30, 3, 8, kColorWhite);
    // Pustule bumps
    GFX_ELLIPSE(6, 5, 6, 6, kColorBlack);
    GFX_FILL(8, 6, 3, 3, kColorWhite);
    GFX_ELLIPSE(24, 18, 5, 5, kColorBlack);
    GFX_FILL(26, 20, 2, 2, kColorWhite);
    GFX_ELLIPSE(12, 21, 5, 5, kColorBlack);
    // Vein lines
    GFX_FILL(15, 5, 1, 8, kColorBlack);
    GFX_FILL(21, 9, 1, 6, kColorBlack);
    // Eyes (beady)
    GFX_FILL(12, 11, 5, 5, kColorBlack);
    GFX_FILL(14, 12, 2, 2, kColorWhite);
    GFX_FILL(23, 11, 5, 5, kColorBlack);
    GFX_FILL(24, 12, 2, 2, kColorWhite);
    GFX_POP();

    // Harbinger (45x45) — crowned horror with segmented body
    enemyImgs[ENEMY_HARBINGER] = make_bitmap(46, 46);
    GFX_PUSH(enemyImgs[ENEMY_HARBINGER]);
    // Crown/horn protrusions
    GFX_FILL(11, 5, 3, 6, kColorWhite);
    GFX_FILL(17, 2, 3, 9, kColorWhite);
    GFX_FILL(26, 2, 3, 9, kColorWhite);
    GFX_FILL(32, 5, 3, 6, kColorWhite);
    // Head
    GFX_FILL(8, 8, 30, 12, kColorWhite);
    // Body segments
    GFX_FILL(6, 20, 33, 2, kColorWhite);   // segment line
    GFX_FILL(8, 21, 30, 12, kColorWhite);
    GFX_FILL(9, 33, 27, 2, kColorWhite);   // segment line
    GFX_FILL(11, 35, 24, 9, kColorWhite);
    // Body panel dithering
    for (int y = 23; y < 33; y += 3)
        for (int x = 11; x < 35; x += 4)
            GFX_FILL(x, y, 1, 1, kColorBlack);
    // Eyes with brow marks
    GFX_FILL(14, 8, 2, 3, kColorWhite);    // left brow
    GFX_FILL(30, 8, 2, 3, kColorWhite);    // right brow
    GFX_FILL(15, 11, 6, 6, kColorBlack);
    GFX_FILL(17, 12, 3, 3, kColorWhite);
    GFX_FILL(26, 11, 6, 6, kColorBlack);
    GFX_FILL(27, 12, 3, 3, kColorWhite);
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
        // Hat with brim
        GFX_FILL(8, 0, 15, 3, kColorWhite);
        GFX_FILL(11, 0, 9, 2, kColorWhite);
        // Head
        GFX_ELLIPSE(11, 2, 11, 9, kColorWhite);
        // Keeper's coat
        GFX_FILL(9, 9, 14, 12, kColorWhite);
        // Coat texture (horizontal bands)
        GFX_FILL(11, 12, 10, 1, kColorBlack);
        GFX_FILL(11, 15, 10, 1, kColorBlack);
        GFX_FILL(11, 18, 10, 1, kColorBlack);
        // Arms
        GFX_FILL(5, 11, 4, 6, kColorWhite);
        GFX_FILL(23, 11, 4, 6, kColorWhite);
        // Lantern in right hand
        GFX_FILL(26, 15, 3, 3, kColorWhite);
        GFX_FILL(24, 18, 5, 3, kColorWhite);
        GFX_FILL(26, 18, 2, 2, kColorBlack);
        // Legs + boots (walk animation)
        int legOff = (f == 1 || f == 3) ? 2 : 0;
        GFX_FILL(11, 21, 4, 6 - legOff, kColorWhite);
        GFX_FILL(17, 21, 4, 6 - (2 - legOff), kColorWhite);
        GFX_FILL(9, 26 - legOff, 6, 3, kColorWhite);
        GFX_FILL(17, 26 - (2 - legOff), 6, 3, kColorWhite);
        // Eye
        GFX_FILL(18, 5, 3, 3, kColorBlack);
        GFX_FILL(20, 6, 2, 2, kColorWhite);
        GFX_POP();
    }
}

// ---------------------------------------------------------------------------
// Bullet / gem / pickup images
// ---------------------------------------------------------------------------
static void create_misc_images(void)
{
    // Bullet (6x6)
    bulletImgs[0] = make_bitmap(6, 6);
    GFX_PUSH(bulletImgs[0]); GFX_FILL(0, 0, 6, 6, kColorWhite); GFX_POP();

    // Harpoon (3x12)
    bulletImgs[1] = make_bitmap(3, 12);
    GFX_PUSH(bulletImgs[1]); GFX_FILL(0, 0, 3, 12, kColorWhite); GFX_POP();

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

    // XP gem (12x12)
    xpGemImg = make_bitmap(12, 12);
    GFX_PUSH(xpGemImg); GFX_ELLIPSE(0, 0, 12, 12, kColorWhite); GFX_POP();

    // XP gem small (9x9)
    xpGemImgSmall = make_bitmap(9, 9);
    GFX_PUSH(xpGemImgSmall); GFX_ELLIPSE(0, 0, 9, 9, kColorWhite); GFX_POP();

    // Anchor (12x12)
    anchorImg = make_bitmap(12, 12);
    GFX_PUSH(anchorImg);
    GFX_FILL(5, 0, 3, 9, kColorWhite);
    GFX_FILL(2, 8, 9, 3, kColorWhite);
    GFX_FILL(0, 5, 2, 5, kColorWhite);
    GFX_FILL(11, 5, 1, 5, kColorWhite);
    GFX_POP();

    // Crate (18x18)
    crateImg = make_bitmap(18, 18);
    GFX_PUSH(crateImg);
    GFX_RECT(0, 0, 18, 18, kColorWhite);
    GFX_FILL(8, 0, 3, 18, kColorWhite);
    GFX_FILL(0, 8, 18, 3, kColorWhite);
    GFX_POP();

    // Weapon icons (15x15) — unique per weapon
    for (int i = 0; i < WEAPON_COUNT; i++) {
        weaponIconImgs[i] = make_bitmap(15, 15);
        GFX_PUSH(weaponIconImgs[i]);
        switch (i) {
        case WEAPON_SIGNAL_BEAM:
            GFX_FILL(2, 3, 12, 2, kColorWhite);
            GFX_FILL(3, 8, 11, 2, kColorWhite);
            GFX_FILL(5, 12, 9, 2, kColorWhite);
            GFX_FILL(0, 2, 2, 12, kColorWhite);
            break;
        case WEAPON_TIDE_POOL:
            pd->graphics->drawEllipse(2, 2, 12, 12, 1, 0, 360, kColorWhite);
            GFX_FILL(6, 6, 3, 3, kColorWhite);
            break;
        case WEAPON_HARPOON:
            pd->graphics->drawLine(2, 14, 12, 3, 1, kColorWhite);
            GFX_FILL(11, 0, 4, 4, kColorWhite);
            GFX_FILL(9, 2, 2, 2, kColorWhite);
            break;
        case WEAPON_BRINE_SPLASH:
            pd->graphics->drawEllipse(0, 0, 15, 15, 1, 0, 360, kColorWhite);
            pd->graphics->drawEllipse(5, 5, 6, 6, 1, 0, 360, kColorWhite);
            break;
        case WEAPON_GHOST_LIGHT:
            pd->graphics->fillEllipse(3, 3, 9, 9, 0, 360, kColorWhite);
            GFX_FILL(0, 6, 2, 2, kColorWhite);
            GFX_FILL(14, 6, 1, 2, kColorWhite);
            GFX_FILL(6, 0, 2, 2, kColorWhite);
            GFX_FILL(6, 14, 2, 1, kColorWhite);
            break;
        case WEAPON_ANCHOR_DROP:
            GFX_FILL(6, 0, 3, 11, kColorWhite);
            GFX_FILL(3, 9, 9, 3, kColorWhite);
            GFX_FILL(2, 6, 3, 5, kColorWhite);
            GFX_FILL(11, 6, 3, 5, kColorWhite);
            GFX_FILL(5, 0, 5, 2, kColorWhite);
            break;
        case WEAPON_FOGHORN:
            GFX_FILL(0, 5, 5, 6, kColorWhite);
            pd->graphics->drawLine(6, 3, 6, 12, 1, kColorWhite);
            pd->graphics->drawLine(9, 2, 9, 14, 1, kColorWhite);
            pd->graphics->drawLine(12, 0, 12, 15, 1, kColorWhite);
            break;
        case WEAPON_CHAIN_LIGHTNING:
            // Zigzag lightning bolt
            pd->graphics->drawLine(7, 0, 4, 5, 1, kColorWhite);
            pd->graphics->drawLine(4, 5, 10, 6, 1, kColorWhite);
            pd->graphics->drawLine(10, 6, 5, 11, 1, kColorWhite);
            pd->graphics->drawLine(5, 11, 8, 15, 1, kColorWhite);
            GFX_FILL(6, 0, 3, 2, kColorWhite);
            break;
        case WEAPON_RIPTIDE:
            // Spiral vortex
            pd->graphics->drawEllipse(2, 2, 11, 11, 1, 0, 270, kColorWhite);
            pd->graphics->drawEllipse(4, 4, 7, 7, 1, 90, 360, kColorWhite);
            GFX_FILL(6, 6, 3, 3, kColorWhite);
            break;
        case WEAPON_DEPTH_CHARGE:
            // Mine with spikes
            pd->graphics->fillEllipse(3, 3, 9, 9, 0, 360, kColorWhite);
            GFX_FILL(6, 0, 3, 3, kColorWhite);
            GFX_FILL(6, 12, 3, 3, kColorWhite);
            GFX_FILL(0, 6, 3, 3, kColorWhite);
            GFX_FILL(12, 6, 3, 3, kColorWhite);
            break;
        }
        GFX_POP();
    }

    // Large weapon icons (24x24) — detailed versions
    for (int i = 0; i < WEAPON_COUNT; i++) {
        weaponIconLargeImgs[i] = make_bitmap(24, 24);
        GFX_PUSH(weaponIconLargeImgs[i]);
        switch (i) {
        case WEAPON_SIGNAL_BEAM:
            GFX_FILL(3, 5, 3, 15, kColorWhite);
            GFX_FILL(2, 18, 6, 3, kColorWhite);
            GFX_FILL(2, 3, 6, 3, kColorWhite);
            GFX_FILL(9, 3, 14, 2, kColorWhite);
            GFX_FILL(9, 8, 12, 2, kColorWhite);
            GFX_FILL(9, 12, 11, 2, kColorWhite);
            GFX_FILL(3, 2, 3, 2, kColorWhite);
            break;
        case WEAPON_TIDE_POOL:
            pd->graphics->drawEllipse(2, 2, 21, 21, 1, 0, 360, kColorWhite);
            pd->graphics->drawEllipse(6, 6, 12, 12, 1, 0, 360, kColorWhite);
            GFX_FILL(11, 11, 3, 3, kColorWhite);
            GFX_FILL(5, 11, 2, 2, kColorWhite);
            GFX_FILL(18, 11, 2, 2, kColorWhite);
            break;
        case WEAPON_HARPOON:
            pd->graphics->drawLine(2, 21, 18, 5, 1, kColorWhite);
            GFX_FILL(17, 2, 6, 6, kColorWhite);
            GFX_FILL(15, 3, 2, 2, kColorWhite);
            GFX_FILL(14, 8, 2, 2, kColorWhite);
            GFX_FILL(0, 21, 5, 3, kColorWhite);
            break;
        case WEAPON_BRINE_SPLASH:
            pd->graphics->drawEllipse(0, 0, 24, 24, 1, 0, 360, kColorWhite);
            pd->graphics->drawEllipse(5, 5, 15, 15, 1, 0, 360, kColorWhite);
            pd->graphics->drawEllipse(9, 9, 6, 6, 1, 0, 360, kColorWhite);
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
            // Large zigzag lightning bolt
            pd->graphics->drawLine(11, 0, 6, 8, 2, kColorWhite);
            pd->graphics->drawLine(6, 8, 16, 10, 2, kColorWhite);
            pd->graphics->drawLine(16, 10, 8, 18, 2, kColorWhite);
            pd->graphics->drawLine(8, 18, 13, 24, 2, kColorWhite);
            GFX_FILL(9, 0, 5, 3, kColorWhite);
            break;
        case WEAPON_RIPTIDE:
            // Large spiral vortex
            pd->graphics->drawEllipse(2, 2, 20, 20, 1, 0, 270, kColorWhite);
            pd->graphics->drawEllipse(5, 5, 14, 14, 1, 90, 360, kColorWhite);
            pd->graphics->drawEllipse(8, 8, 8, 8, 1, 180, 450, kColorWhite);
            GFX_FILL(10, 10, 4, 4, kColorWhite);
            break;
        case WEAPON_DEPTH_CHARGE:
            // Large mine with spikes
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

    // Passive icons (15x15) — unique per passive
    for (int i = 0; i < 5; i++) {
        passiveIconImgs[i] = make_bitmap(15, 15);
        GFX_PUSH(passiveIconImgs[i]);
        switch (i) {
        case 0: // Oilskin Coat — shield shape
            GFX_FILL(3, 0, 9, 2, kColorWhite);
            GFX_FILL(1, 2, 13, 6, kColorWhite);
            GFX_FILL(2, 8, 11, 3, kColorWhite);
            GFX_FILL(4, 11, 7, 2, kColorWhite);
            GFX_FILL(6, 13, 3, 2, kColorWhite);
            break;
        case 1: // Sea Legs — boot/chevron
            GFX_FILL(4, 0, 5, 3, kColorWhite);
            GFX_FILL(3, 3, 6, 4, kColorWhite);
            GFX_FILL(2, 7, 7, 3, kColorWhite);
            GFX_FILL(1, 10, 12, 3, kColorWhite);
            GFX_FILL(6, 13, 7, 2, kColorWhite);
            break;
        case 2: // Barnacle Armor — spiked border
            GFX_RECT(2, 2, 11, 11, kColorWhite);
            GFX_FILL(6, 0, 3, 2, kColorWhite);
            GFX_FILL(6, 13, 3, 2, kColorWhite);
            GFX_FILL(0, 6, 2, 3, kColorWhite);
            GFX_FILL(13, 6, 2, 3, kColorWhite);
            GFX_FILL(6, 6, 3, 3, kColorWhite);
            break;
        case 3: // Lighthouse Lens — eye/lens
            pd->graphics->drawEllipse(0, 3, 15, 9, 1, 0, 360, kColorWhite);
            pd->graphics->fillEllipse(5, 5, 5, 5, 0, 360, kColorWhite);
            pd->graphics->drawLine(0, 7, 3, 7, 1, kColorWhite);
            pd->graphics->drawLine(12, 7, 15, 7, 1, kColorWhite);
            break;
        case 4: // Tidecaller — wave symbol
            pd->graphics->drawLine(0, 7, 3, 4, 1, kColorWhite);
            pd->graphics->drawLine(3, 4, 7, 10, 1, kColorWhite);
            pd->graphics->drawLine(7, 10, 11, 4, 1, kColorWhite);
            pd->graphics->drawLine(11, 4, 14, 7, 1, kColorWhite);
            pd->graphics->drawLine(0, 11, 3, 8, 1, kColorWhite);
            pd->graphics->drawLine(3, 8, 7, 14, 1, kColorWhite);
            pd->graphics->drawLine(7, 14, 11, 8, 1, kColorWhite);
            pd->graphics->drawLine(11, 8, 14, 11, 1, kColorWhite);
            break;
        }
        GFX_POP();
    }

    // Pickup sprites (12x12) — distinct per type
    // Health: plus/cross
    pickupImgs[PICKUP_HEALTH] = make_bitmap(12, 12);
    GFX_PUSH(pickupImgs[PICKUP_HEALTH]);
    GFX_FILL(4, 0, 4, 12, kColorWhite);
    GFX_FILL(0, 4, 12, 4, kColorWhite);
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

    // Weapon Upgrade: upward arrow
    pickupImgs[PICKUP_WEAPON_UPGRADE] = make_bitmap(12, 12);
    GFX_PUSH(pickupImgs[PICKUP_WEAPON_UPGRADE]);
    GFX_FILL(5, 0, 2, 2, kColorWhite);
    GFX_FILL(3, 2, 6, 2, kColorWhite);
    GFX_FILL(1, 4, 10, 2, kColorWhite);
    GFX_FILL(4, 6, 4, 6, kColorWhite);
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
// Public API
// ---------------------------------------------------------------------------
void images_init(void)
{
    create_tile_images();
    create_enemy_images();
    create_player_images();
    create_misc_images();
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
int images_get_enemy_half_w(EnemyType type) { return enemyHW[type]; }
int images_get_enemy_half_h(EnemyType type) { return enemyHH[type]; }
