#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <string.h>
#include "pd_api.h"
#include "debug.h"
#include "mathutil.h"

// ---------------------------------------------------------------------------
// Global Playdate API pointer (set once in eventHandler)
// ---------------------------------------------------------------------------
extern PlaydateAPI* pd;

// ---------------------------------------------------------------------------
// Shorthand macros for Playdate C API drawing
// In the C API, color is always the LAST parameter to draw functions.
// ---------------------------------------------------------------------------
#define GFX_CLEAR(c)              pd->graphics->clear((c))
#define GFX_FILL(x,y,w,h,c)      pd->graphics->fillRect((x),(y),(w),(h),(c))
#define GFX_RECT(x,y,w,h,c)      pd->graphics->drawRect((x),(y),(w),(h),(c))
#define GFX_ELLIPSE(x,y,w,h,c)   pd->graphics->fillEllipse((x),(y),(w),(h),0,360,(c))
#define GFX_CIRCLE(x,y,w,h,lw,c) pd->graphics->drawEllipse((x),(y),(w),(h),(lw),0,360,(c))
#define GFX_LINE(x1,y1,x2,y2,w,c) pd->graphics->drawLine((x1),(y1),(x2),(y2),(w),(c))
#define GFX_PUSH(bmp)             pd->graphics->pushContext((bmp))
#define GFX_POP()                 pd->graphics->popContext()

// ---------------------------------------------------------------------------
// Display / Map constants
// ---------------------------------------------------------------------------
#define SCREEN_W        400
#define SCREEN_H        240
#define MAP_W           960
#define MAP_H           576
#define TILE_SIZE        24
#define GRID_W           40   // MAP_W / TILE_SIZE
#define GRID_H           24   // MAP_H / TILE_SIZE
#define WALL_THICKNESS   15

// ---------------------------------------------------------------------------
// Gameplay constants
// ---------------------------------------------------------------------------
#define BULLET_SPEED       6.0f
#define BULLET_LIFETIME_F  15    // frames (~500ms at 30fps)
#define ENEMY_BASE_SPEED   1.0f
#define INVULN_FRAMES      45    // ~1500ms at 30fps
#define XP_PER_KILL        10
#define FRAME_MS           33    // ~30fps
#define MAX_WEAPONS         6
#define VICTORY_TIME      480.0f // 8 minutes
#define XP_LEVEL_SCALE     1.45f

// ---------------------------------------------------------------------------
// Entity pool sizes
// ---------------------------------------------------------------------------
#define MAX_ENEMIES       150
#define MAX_BULLETS        60
#define MAX_ENEMY_BULLETS  30
#define MAX_XP_GEMS       120
#define MAX_ANCHORS         8
#define MAX_PARTICLES      100
#define MAX_FX              20
#define MAX_CRATES           1

// ---------------------------------------------------------------------------
// Collision grid
// ---------------------------------------------------------------------------
#define COL_CELL_SIZE     96
#define COL_COLS          10   // MAP_W / COL_CELL_SIZE
#define COL_ROWS           6   // MAP_H / COL_CELL_SIZE
#define COL_MAX_PER_CELL  32

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_GAMEOVER,
    STATE_UPGRADE,
    STATE_CUTSCENE,
    STATE_CRATE_REWARD,
    STATE_VICTORY,
    STATE_ARMORY,
    STATE_BESTIARY
} GameState;

typedef enum {
    ENEMY_CREEPER = 0,   // horror1
    ENEMY_TENDRIL,       // horror2
    ENEMY_WRAITH,        // horror3
    ENEMY_ABYSSAL,       // horror4
    ENEMY_SEER,          // horror5
    ENEMY_LAMPREY,       // horror6
    ENEMY_BLOAT,         // horror7
    ENEMY_HARBINGER,     // horror8
    ENEMY_TYPE_COUNT
} EnemyType;

typedef enum {
    WEAPON_SIGNAL_BEAM = 0,
    WEAPON_TIDE_POOL,
    WEAPON_HARPOON,
    WEAPON_BRINE_SPLASH,
    WEAPON_GHOST_LIGHT,
    WEAPON_ANCHOR_DROP,
    WEAPON_FOGHORN,
    WEAPON_COUNT
} WeaponId;

// ---------------------------------------------------------------------------
// Difficulty tiers
// ---------------------------------------------------------------------------
typedef struct {
    float time;
    const char* name;
} TierDef;

#define TIER_COUNT 5

extern const TierDef TIERS[TIER_COUNT];

// ---------------------------------------------------------------------------
// Entity structs
// ---------------------------------------------------------------------------

typedef struct {
    float x, y;
    float speed, hp;
    EnemyType type;
    uint8_t alive;
    uint8_t isMini;
    int8_t flashTimer;
    int8_t slowTimer;
    int8_t stunTimer;
    float slowFactor;
    float phase;
    int16_t shotCooldown;
    int16_t bobTimer;
    int16_t animFrame;
    int16_t lungeTimer;
    int8_t lunging;
    int8_t charging;
    uint32_t lastHitByTidepool;
} Enemy;

typedef struct {
    float x, y;
    float vx, vy;
    float speed;
    float dmg;
    float turnRate;
    int16_t lifeFrames;
    int16_t homingTarget; // enemy index, -1 = none
    uint8_t alive;
    uint8_t piercing;
    uint8_t homing;
    uint8_t retargets;
    uint8_t imageId;
} Bullet;

typedef struct {
    float x, y;
    float vx, vy;
    int16_t lifeFrames;
    uint8_t alive;
} EnemyBullet;

typedef struct {
    float x, y;
    int value;
    int16_t lifeFrames; // despawn after ~300 frames (10s)
    uint8_t alive;
} XPGem;

typedef struct {
    float x, y;
    float dmg;
    int16_t durationFrames;
    int16_t lifeFrames;
    float slowFactor;
    uint8_t alive;
    int16_t hitCooldowns[MAX_ENEMIES]; // frames until can hit again
} Anchor;

typedef struct {
    float x, y;
    float baseY;
    int16_t lifeFrames;
    int16_t bobFrame;
    uint8_t alive;
} Crate;

typedef struct {
    float x, y;
    float vx, vy;
    int8_t life;
    uint8_t sz;
} Particle;

typedef struct {
    float x, y;
    uint8_t fxType;   // index into FX frame arrays
    uint8_t frameIdx;
    uint8_t timer;
    uint8_t speed;    // frames between anim steps
    uint8_t active;
} FXInstance;

// ---------------------------------------------------------------------------
// Damage numbers (floating combat text)
// ---------------------------------------------------------------------------
#define MAX_DMG_NUMBERS 16

typedef struct {
    float x, y;
    int value;
    int8_t life;    // frames remaining (~20 frames)
    uint8_t crit;   // 1 = critical hit
} DamageNumber;

// ---------------------------------------------------------------------------
// Weapon instance (equipped by player)
// ---------------------------------------------------------------------------
typedef struct {
    WeaponId id;
    int level;           // 1-3
    int cooldownMs;
    uint32_t lastFiredMs;
} Weapon;

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------
typedef struct {
    float x, y;
    float aimDx, aimDy;
    float moveSpeed;
    int hp, maxHp;
    int xp, level, xpToNext;
    uint32_t invulnUntil; // ms timestamp
    int dead;
    int cursedTimer;      // frames
    int frameIdx;         // 0-3 walk animation
    int animFrame;        // frame counter for animation timing

    // Passives
    int oilskinCoat;
    int seaLegs;
    int barnacleArmor;    // bool
    int lighthouseLens;   // bool
    int tidecaller;

    // Weapons
    Weapon weapons[MAX_WEAPONS];
    int weaponCount;
} Player;

// ---------------------------------------------------------------------------
// Brine splash / foghorn visual state
// ---------------------------------------------------------------------------
typedef struct {
    int active;
    float x, y;
    float radius, maxRadius;
    int frame;
} AreaVisual;

// ---------------------------------------------------------------------------
// Cutscene data
// ---------------------------------------------------------------------------
#define CUTSCENE_MAX_LINES 8

typedef struct {
    const char* lines[CUTSCENE_MAX_LINES];
    int lineCount;
    int framesLeft;
    int totalFrames;  // initial duration (for elapsed calculation)
    void (*onComplete)(void);
    void (*drawFunc)(void);
} CutsceneData;

// ---------------------------------------------------------------------------
// Upgrade choice
// ---------------------------------------------------------------------------
typedef struct {
    int type;    // 0 = new weapon, 1 = weapon upgrade, 2 = passive
    int id;      // weapon id or passive index
    const char* name;
    const char* desc;
} UpgradeChoice;

#define MAX_UPGRADE_CHOICES 3

// ---------------------------------------------------------------------------
// Game state (singleton)
// ---------------------------------------------------------------------------
typedef struct {
    GameState state;

    // Timing
    float gameTime;       // seconds elapsed
    uint32_t frameCount;
    float spawnAccum;     // ms accumulator
    int spawnQueue;

    // Difficulty
    int currentTier;      // 0-4
    float enemySpeedMult;
    float enemyHPBonus;
    int arenaShrink;      // pixels inward per side

    // Camera
    float cameraX, cameraY;

    // Screen effects
    int shakeFrames;
    int shakeX, shakeY;
    int recentKills;
    int invertTimer;
    int mosaicTimer;
    int cooldownBuffTimer;

    // Tier announcement
    char announceText[32];
    int announceTimer;

    // Score / persistence
    int score;
    int highScore;
    float bestTime;
    uint8_t unlockedWeapons[WEAPON_COUNT];
    uint8_t unlockedEnemies[ENEMY_TYPE_COUNT];

    // Crate state
    int activeCrateIdx;
    float lastCrateCheckTime;
    int firstCrateSpawned;
    char crateRewardText[48];
    int crateRewardTimer;

    // Streak
    int streakKills;
    int streakWindow;
    float streakMultiplier;
    int streakTimer;

    // Surge
    float surgeCheckTimer;
    int surgeTimer;

    // XP vacuum
    int xpVacuum;

    // Oilskin regen
    float oilskinRegenTimer;

    // Menu state
    int menuSelection;
    int armorySelection;
    int gameOverSelection;
    int selectedUpgrade;

    // Upgrade choices
    UpgradeChoice upgradeChoices[MAX_UPGRADE_CHOICES];
    int upgradeChoiceCount;

    // Cutscene
    CutsceneData cutscene;

    // Visual effects
    AreaVisual brineSplash;
    AreaVisual foghornVisual;

    // Cached nearest enemy (for auto-aim)
    int cachedTargetIdx;     // -1 = none
    uint32_t targetCacheFrame;

    // Synergy cache
    float synergyBurstRadius;   // 1.0 or 1.2
    float synergyBeamDmg;       // 0 or 1
    float synergyRuneSlow;      // 1.0 or 0.8
    float synergyCryCooldown;   // 1.0 or 0.8
    int synergyCacheWeaponCount;

    // Tide pool
    float tidePoolAngle;

    // Player animation (owned here since it's rendering state)
    int playerFrameIdx;
    int playerAnimFrame;

    // Damage numbers (ring buffer)
    DamageNumber dmgNumbers[MAX_DMG_NUMBERS];
    int dmgNumberIdx;

    // Kill streak visuals
    int streakFlashTimer;

    // Surge overlay
    int surgeOverlayTimer;

    // Tier name for HUD
    const char* currentTierName;

    // Bestiary selection (separate from armorySelection)
    int bestiarySelection;

    // Slow-motion timer (crate drama)
    int slowMotionTimer;

    // Bold font for UI
    LCDFont* fontBold;
} Game;

extern Game game;
extern Player player;

// Entity pools (declared in entities.c)
extern Enemy enemies[MAX_ENEMIES];
extern int enemyCount;
extern Bullet bullets[MAX_BULLETS];
extern int bulletCount;
extern EnemyBullet enemyBullets[MAX_ENEMY_BULLETS];
extern int enemyBulletCount;
extern XPGem xpGems[MAX_XP_GEMS];
extern int xpGemCount;
extern Anchor anchors[MAX_ANCHORS];
extern int anchorCount;
extern Crate crates[MAX_CRATES];
extern int crateCount;
extern Particle particles[MAX_PARTICLES];
extern int particleIdx;
extern FXInstance activeFX[MAX_FX];
extern int fxIdx;

// ---------------------------------------------------------------------------
// Function declarations (by file)
// ---------------------------------------------------------------------------

// game.c
int update(void* userdata);
void game_init(void);
void game_start(void);
void game_reset_stats(void);
int game_get_current_tier_index(void);
float game_get_current_speed(void);
float game_get_hp_scale(void);
int game_get_player_power(void);
float game_get_spawn_interval(void);
void game_trigger_shake(int frames);
void game_update_shake(void);
void game_update_camera(void);
void game_start_cutscene(const char* lines[], int lineCount,
                         void (*drawFunc)(void), int frames,
                         void (*onComplete)(void));
void game_generate_upgrades(void);
void game_apply_upgrade(int choice);
void game_update_xp_magnet(void);
void game_update_crates(void);
void game_collect_crate(void);
void game_death_cutscene(void);

// entities.c
void entities_init(void);
void entities_cleanup_dead(void);
int entities_spawn_enemy(float x, float y, float speed, EnemyType type);
int entities_spawn_bullet(float x, float y, float dx, float dy,
                          float dmg, float speed, int lifeFrames,
                          uint8_t piercing, uint8_t homing, float turnRate,
                          uint8_t retargets, uint8_t imageId);
int entities_spawn_enemy_bullet(float x, float y, float dx, float dy);
int entities_spawn_xp_gem(float x, float y, int value);
void entities_spawn_particles(float x, float y, int count, int big);
void entities_spawn_fx(float x, float y, int fxType, int speed);

// player.c
void player_init(void);
void player_update(void);
void player_take_damage(void);

// enemy.c
void enemy_update_all(void);
void enemy_damage(int idx, float amount);

// weapons.c
void weapons_fire_all(void);
void weapons_update_tide_pool(void);
void weapons_update_anchors(void);
const char* weapon_get_name(WeaponId id);
const char* weapon_get_desc(WeaponId id, int level);
int  weapon_get_cooldown(WeaponId id, int level);
void weapons_calc_synergies(void);

// bullets.c
void bullets_update_all(void);
void enemy_bullets_update_all(void);

// collision.c
void collision_clear(void);
void collision_insert_enemy(int idx, float x, float y);
int collision_query_point(float x, float y, float radius,
                          int* outIndices, int maxResults);

// rendering.c
void rendering_init(void);
void rendering_draw_playing(void);
void rendering_draw_background(void);
void rendering_update_background_shrink(void);
void rendering_spawn_dmg_number(float x, float y, int value, int crit);

// ui.c
void ui_draw_title(void);
void ui_draw_hud(void);
void ui_draw_upgrade_screen(void);
void ui_draw_game_over(void);
void ui_draw_victory(void);
void ui_draw_cutscene(void);
void ui_draw_crate_reward(void);
void ui_draw_armory(void);
void ui_draw_bestiary(void);
void ui_draw_tier_announcement(void);
void ui_draw_centered_text(const char* text, int y);
void ui_draw_opening_scene(void);
void ui_draw_fog_scene(void);
void ui_draw_corruption_scene(void);
void ui_draw_sunrise_scene(void);

// images.c
void images_init(void);
LCDBitmap* images_get_enemy(EnemyType type);
LCDBitmap* images_get_player_frame(int frame);
LCDBitmap* images_get_bullet(void);
LCDBitmap* images_get_enemy_bullet(void);
LCDBitmap* images_get_xp_gem(int small);
LCDBitmap* images_get_harpoon(void);
LCDBitmap* images_get_anchor(void);
LCDBitmap* images_get_ghost_light(void);
LCDBitmap* images_get_crate(void);
LCDBitmap* images_get_tile(int tileIdx);
LCDBitmap* images_get_weapon_icon(WeaponId id);
LCDBitmap* images_get_weapon_icon_large(WeaponId id);
LCDBitmap* images_get_passive_icon(int passiveIdx);
int images_get_enemy_half_w(EnemyType type);
int images_get_enemy_half_h(EnemyType type);

// sound.c
void sound_init(void);
void sound_play_hit(void);
void sound_play_kill(void);
void sound_play_xp(void);
void sound_play_levelup(void);
void sound_play_weapon(float freq);
void sound_play_boom(float freq, float vol, float len);
void sound_play_confirm(void);
void sound_play_menu(void);
void sound_play_shrink(void);
void sound_play_crate(void);
void sound_play_gameover(void);
void sound_play_streak(void);
void sound_play_surge(void);
void sound_play_tier(void);

// save.c
void save_load(void);
void save_write(void);
void save_update_high_score(void);

#endif // GAME_H
