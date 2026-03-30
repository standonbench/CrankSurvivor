#include "game.h"
#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Global game state
// ---------------------------------------------------------------------------
Game game;
Player player;
uint32_t _rng_state = 1;

const TierDef TIERS[TIER_COUNT] = {
    {   0.0f, "9 PM"       },
    {  45.0f, "10:30 PM"   },
    { 105.0f, "Midnight"   },
    { 195.0f, "2 AM ?"     },
    { 315.0f, "4 AM ??"    },
};

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void game_init(void)
{
    memset(&game, 0, sizeof(Game));
    game.state = STATE_TITLE;
    game.menuSelection = 1;
    game.enemySpeedMult = 1.0f;
    game.streakMultiplier = 1.0f;
    game.cachedTargetIdx = -1;
    game.activeCrateIdx = -1;

    images_init();
    rendering_init();
    sound_init();
    save_load();
    entities_init();

    const char* fontErr = NULL;
    game.fontBold = pd->graphics->loadFont("Asheville-Sans-14-Bold", &fontErr);
    if (!game.fontBold) DLOG("Failed to load bold font: %s", fontErr ? fontErr : "unknown");

    game.fontLarge = pd->graphics->loadFont("Roobert-20-Medium", &fontErr);
    if (!game.fontLarge) DLOG("Failed to load large font: %s", fontErr ? fontErr : "unknown");

    DLOG("Game initialized. DEBUG_BUILD=%d", DEBUG_BUILD);
}

// ---------------------------------------------------------------------------
// Cutscene completion: begin gameplay
// ---------------------------------------------------------------------------
static void game_start_playing(void)
{
    game.state = STATE_PLAYING;
}

// ---------------------------------------------------------------------------
// Start a new game
// ---------------------------------------------------------------------------
void game_start(void)
{
    entities_init();
    game_reset_stats();

    game.state = STATE_PLAYING;
    game.gameTime = 0.0f;
    game.frameCount = 0;
    game.spawnAccum = 0.0f;
    game.spawnQueue = 0;
    game.currentTier = 0;
    game.enemySpeedMult = 1.0f;
    game.enemyHPBonus = 0.0f;
    game.arenaShrink = 0;
    game.score = 0;
    game.cameraX = 0.0f;
    game.cameraY = 0.0f;
    game.shakeFrames = 0;
    game.shakeX = 0;
    game.shakeY = 0;
    game.recentKills = 0;
    game.invertTimer = 0;
    game.mosaicTimer = 0;
    game.cooldownBuffTimer = 0;
    game.announceTimer = 0;
    game.activeCrateIdx = -1;
    game.lastCrateCheckTime = 0.0f;
    game.firstCrateSpawned = 0;
    game.crateRewardTimer = 0;
    game.streakKills = 0;
    game.streakWindow = 0;
    game.streakMultiplier = 1.0f;
    game.streakTimer = 0;
    game.surgeCheckTimer = 0.0f;
    game.surgeTimer = 0;
    game.xpVacuum = 0;
    game.oilskinRegenTimer = 0.0f;
    game.cachedTargetIdx = -1;
    game.targetCacheFrame = 0;
    game.tidePoolAngle = 0.0f;
    game.synergyCacheWeaponCount = 0;
    game.brineSplash.active = 0;
    game.foghornVisual.active = 0;
    game.dmgNumberIdx = 0;
    game.streakFlashTimer = 0;
    game.surgeOverlayTimer = 0;
    game.currentTierName = TIERS[0].name;
    game.bestiarySelection = 1;
    game.slowMotionTimer = 0;
    memset(game.dmgNumbers, 0, sizeof(game.dmgNumbers));

    player_init();
    rendering_init(); // rebuild background

    // Give starter weapon: Signal Beam
    player.weapons[0].id = WEAPON_SIGNAL_BEAM;
    player.weapons[0].level = 1;
    player.weapons[0].cooldownMs = 700;
    player.weapons[0].lastFiredMs = 0;
    player.weaponCount = 1;

    // Mark as unlocked
    game.unlockedWeapons[WEAPON_SIGNAL_BEAM] = 1;

    // Opening cutscene (game_start_playing is forward-declared below)
    static const char* openingLines[] = {
        "Keeper's Log  --  9:00 PM",
        "",
        "The fog rolled in three days ago.",
        "The fish washed up dead on the second.",
        "Tonight, something answered the light.",
        "",
        "I will keep the flame burning."
    };
    game_start_cutscene(openingLines, 7, ui_draw_opening_scene, 300, game_start_playing);

    DLOG("Game started");
}

// ---------------------------------------------------------------------------
// Reset player stats to defaults
// ---------------------------------------------------------------------------
void game_reset_stats(void)
{
    memset(&player, 0, sizeof(Player));
    player.moveSpeed = 1.8f;
    player.maxHp = 5;
    player.hp = 5;
    player.xpToNext = 100;
    player.level = 1;
    player.aimDy = -1.0f; // aim up by default
    player.x = MAP_W / 2.0f;
    player.y = MAP_H / 2.0f;
}

// ---------------------------------------------------------------------------
// Tier helpers
// ---------------------------------------------------------------------------
int game_get_current_tier_index(void)
{
    int tier = 0;
    for (int i = TIER_COUNT - 1; i >= 0; i--) {
        if (game.gameTime >= TIERS[i].time) {
            tier = i;
            break;
        }
    }
    return tier;
}

float game_get_current_speed(void)
{
    float elapsed30s = game.gameTime / 30.0f;
    return (ENEMY_BASE_SPEED + (int)elapsed30s * 0.02f) * game.enemySpeedMult;
}

float game_get_hp_scale(void)
{
    static const float tierHP[] = { 1.0f, 1.8f, 2.8f, 4.0f, 6.0f };
    int tier = game_get_current_tier_index();
    float base = tierHP[tier];
    int power = game_get_player_power();
    if (power > 5) {
        base += (power - 5) * 0.5f;
        if (base > 8.0f) base = 8.0f;
    }
    return base;
}

int game_get_player_power(void)
{
    int power = player.weaponCount;
    for (int i = 0; i < player.weaponCount; i++) {
        power += player.weapons[i].level;
    }
    power += player.oilskinCoat + player.seaLegs + player.tidecaller;
    if (player.saltWardMax > 0) power++;
    if (player.lighthouseLens) power++;
    return power;
}

float game_get_spawn_interval(void)
{
    static const float tierIntervals[] = { 1200, 900, 650, 450, 300 };
    int tier = game_get_current_tier_index();
    float base = tierIntervals[tier];
    int power = game_get_player_power();
    if (power > 8) {
        base -= (power - 8) * 50.0f;
        if (base < 150.0f) base = 150.0f;
    }
    return base;
}

// ---------------------------------------------------------------------------
// Screen shake
// ---------------------------------------------------------------------------
void game_trigger_shake(int frames)
{
    if (frames > game.shakeFrames)
        game.shakeFrames = frames;
}

void game_update_shake(void)
{
    if (game.shakeFrames > 0) {
        game.shakeX = rng_range(-2, 2);
        game.shakeY = rng_range(-2, 2);
        game.shakeFrames--;
    } else {
        game.shakeX = 0;
        game.shakeY = 0;
    }
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
void game_update_camera(void)
{
    if (player.dead) return;
    float targetX = player.x - SCREEN_W / 2.0f;
    float targetY = player.y - SCREEN_H / 2.0f;
    game.cameraX = clampf(targetX, 0.0f, (float)(MAP_W - SCREEN_W));
    game.cameraY = clampf(targetY, 0.0f, (float)(MAP_H - SCREEN_H));
}

// ---------------------------------------------------------------------------
// Cutscene
// ---------------------------------------------------------------------------
void game_start_cutscene(const char* lines[], int lineCount,
                         void (*drawFunc)(void), int frames,
                         void (*onComplete)(void))
{
    game.cutscene.lineCount = lineCount < CUTSCENE_MAX_LINES ? lineCount : CUTSCENE_MAX_LINES;
    for (int i = 0; i < game.cutscene.lineCount; i++) {
        game.cutscene.lines[i] = lines[i];
    }
    game.cutscene.drawFunc = drawFunc;
    game.cutscene.framesLeft = frames;
    game.cutscene.totalFrames = frames;
    game.cutscene.onComplete = onComplete;
    game.state = STATE_CUTSCENE;
}

// ---------------------------------------------------------------------------
// Death cutscene
// ---------------------------------------------------------------------------
static void death_cutscene_complete(void)
{
    game.state = STATE_GAMEOVER;
}

void game_death_cutscene(void)
{
    static const char* deathLines[] = {
        "The flame gutters...",
        "",
        "...and I can no longer tell",
        "if the dark is outside",
        "or within."
    };
    game_start_cutscene(deathLines, 5, NULL, 300, death_cutscene_complete);
}

// ---------------------------------------------------------------------------
// Tier transition cutscene callbacks
// ---------------------------------------------------------------------------
static void tier_cutscene_complete(void)
{
    // Arena shrink at tiers 2-4
    if (game.currentTier >= 2) {
        game.arenaShrink += 12;
        sound_play_shrink();
        game.invertTimer = 5;
        game_trigger_shake(4);
        rendering_update_background_shrink();
    }
    game.state = STATE_PLAYING;
}

// ---------------------------------------------------------------------------
// Tier transitions
// ---------------------------------------------------------------------------
static void update_tier(void)
{
    int newTier = game_get_current_tier_index();
    if (newTier > game.currentTier) {
        game.currentTier = newTier;
        game.currentTierName = TIERS[newTier].name;

        // Speed boost per tier
        game.enemySpeedMult += 0.10f;

        // HP bonus per tier
        static const float hpBonus[] = { 0, 0, 1, 2, 3 };
        game.enemyHPBonus = hpBonus[newTier];

        sound_play_tier();

        // Tier cutscenes — Keeper's journal entries
        if (newTier == 1) {
            static const char* lines[] = {
                "Keeper's Log  --  10:30 PM",
                "Sounds from below the rocks.",
                "Not waves. Something scraping.",
                "The beam caught shapes in the water.",
                "Too many limbs. I must be tired."
            };
            game_start_cutscene(lines, 5, ui_draw_opening_scene, 300, tier_cutscene_complete);
        } else if (newTier == 2) {
            static const char* lines[] = {
                "Keeper's Log  --  Midnight",
                "The fog has a weight to it now.",
                "I swear it whispered my name.",
                "The walls feel thinner somehow.",
                "How long have I been up here?"
            };
            game_start_cutscene(lines, 5, ui_draw_fog_scene, 300, tier_cutscene_complete);
        } else if (newTier == 3) {
            static const char* lines[] = {
                "Keeper's Log  --  2:00 AM?",
                "My watch stopped. Or did I?",
                "The beam passed through something",
                "that was not fog.",
                "I no longer trust my eyes."
            };
            game_start_cutscene(lines, 5, ui_draw_opening_scene, 300, tier_cutscene_complete);
        } else if (newTier == 4) {
            static const char* lines[] = {
                "Keeper's Log  --  4:00 AM??",
                "The sun should have risen by now.",
                "The dark presses against the glass.",
                "I think something is wearing",
                "the shape of the horizon."
            };
            game_start_cutscene(lines, 5, ui_draw_corruption_scene, 300, tier_cutscene_complete);
        }

        DLOG("Tier transition: %s (tier %d)", TIERS[newTier].name, newTier);
    }
}

// ---------------------------------------------------------------------------
// Victory check
// ---------------------------------------------------------------------------
void victory_cutscene_complete(void)
{
    save_update_high_score();
    game.state = STATE_VICTORY;
}

static void check_victory(void)
{
    if (game.gameTime >= VICTORY_TIME && game.state == STATE_PLAYING) {
        static const char* lines[] = {
            "Keeper's Log  --  Dawn",
            "The fog lifts. The glass is clear.",
            "I can see the sea again --",
            "and for the first time tonight,",
            "I recognize my own hands."
        };
        game_start_cutscene(lines, 5, ui_draw_sunrise_scene, 240, victory_cutscene_complete);
    }
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------
static EnemyType pick_enemy_type(void)
{
    float t = game.gameTime;
    int roll = rng_range(1, 100);

    if (t < 45.0f) {
        // Tier 0: Creeper only
        return ENEMY_CREEPER;
    } else if (t < 75.0f) {
        // +Lamprey (fast, fragile, splits)
        if (roll <= 70) return ENEMY_CREEPER;
        return ENEMY_LAMPREY;
    } else if (t < 105.0f) {
        // +Tendril (strafing lateral pressure)
        if (roll <= 50) return ENEMY_CREEPER;
        if (roll <= 75) return ENEMY_LAMPREY;
        return ENEMY_TENDRIL;
    } else if (t < 150.0f) {
        // +Wraith (phases through walls)
        if (roll <= 35) return ENEMY_CREEPER;
        if (roll <= 55) return ENEMY_LAMPREY;
        if (roll <= 75) return ENEMY_TENDRIL;
        return ENEMY_WRAITH;
    } else if (t < 195.0f) {
        // +Bloat (explodes on death)
        if (roll <= 25) return ENEMY_CREEPER;
        if (roll <= 40) return ENEMY_LAMPREY;
        if (roll <= 55) return ENEMY_TENDRIL;
        if (roll <= 70) return ENEMY_WRAITH;
        return ENEMY_BLOAT;
    } else if (t < 255.0f) {
        // +Abyssal (heavy tank, lunges)
        if (roll <= 20) return ENEMY_CREEPER;
        if (roll <= 32) return ENEMY_LAMPREY;
        if (roll <= 44) return ENEMY_TENDRIL;
        if (roll <= 56) return ENEMY_WRAITH;
        if (roll <= 70) return ENEMY_BLOAT;
        return ENEMY_ABYSSAL;
    } else if (t < 315.0f) {
        // +Seer (ranged attacker)
        if (roll <= 15) return ENEMY_CREEPER;
        if (roll <= 27) return ENEMY_LAMPREY;
        if (roll <= 37) return ENEMY_TENDRIL;
        if (roll <= 49) return ENEMY_WRAITH;
        if (roll <= 61) return ENEMY_BLOAT;
        if (roll <= 75) return ENEMY_ABYSSAL;
        return ENEMY_SEER;
    } else {
        // +Harbinger (summoner/buffer) — full roster
        if (roll <= 12) return ENEMY_CREEPER;
        if (roll <= 22) return ENEMY_LAMPREY;
        if (roll <= 32) return ENEMY_TENDRIL;
        if (roll <= 42) return ENEMY_WRAITH;
        if (roll <= 52) return ENEMY_BLOAT;
        if (roll <= 64) return ENEMY_ABYSSAL;
        if (roll <= 78) return ENEMY_SEER;
        return ENEMY_HARBINGER;
    }
}

static void spawn_enemy(void)
{
    if (enemyCount >= MAX_ENEMIES) return;

    EnemyType type = pick_enemy_type();
    float speed = game_get_current_speed();
    int s = game.arenaShrink;

    float x, y;
    int side = rng_range(1, 4);

    // Type-specific spawn sides
    if (type == ENEMY_TENDRIL) {
        side = rng_range(1, 2) == 1 ? 3 : 4; // left/right flanking
    } else if (type == ENEMY_WRAITH) {
        side = 1; // top only
    }

    if (side == 1)      { x = (float)rng_range(s + 20, MAP_W - s - 20); y = (float)(s + 15); }
    else if (side == 2) { x = (float)rng_range(s + 20, MAP_W - s - 20); y = (float)(MAP_H - s - 15); }
    else if (side == 3) { x = (float)(s + 15); y = (float)rng_range(s + 20, MAP_H - s - 20); }
    else                { x = (float)(MAP_W - s - 15); y = (float)rng_range(s + 20, MAP_H - s - 20); }

    // Speed multipliers per type
    static const float speedMult[] = {
        1.0f,  // Creeper
        1.4f,  // Tendril
        0.6f,  // Wraith
        0.4f,  // Abyssal
        0.5f,  // Seer
        1.5f,  // Lamprey
        0.5f,  // Bloat
        0.45f, // Harbinger
    };
    float finalSpeed = speed * speedMult[type];

    // Base HP per type
    static const float baseHP[] = { 2, 2, 2, 5, 3, 1, 3, 4 };
    float hp = baseHP[type] * game_get_hp_scale() + game.enemyHPBonus;

    int idx = entities_spawn_enemy(x, y, finalSpeed, type);
    if (idx >= 0) {
        enemies[idx].hp = hp;
        enemies[idx].maxHp = hp;
        enemies[idx].packBoost = 1.0f;
        game.unlockedEnemies[type] = 1;

        // Wraith cluster spawn
        if (type == ENEMY_WRAITH) {
            int extra = rng_range(2, 4);
            for (int i = 0; i < extra && enemyCount < MAX_ENEMIES; i++) {
                float ox = x + rng_range(-45, 45);
                float oy = y + rng_range(-15, 15);
                int ci = entities_spawn_enemy(ox, oy, finalSpeed, ENEMY_WRAITH);
                if (ci >= 0) {
                    enemies[ci].hp = hp;
                    enemies[ci].maxHp = hp;
                    enemies[ci].packBoost = 1.0f;
                }
            }
        }
    }
}

static void update_spawning(void)
{
    game.spawnAccum += FRAME_MS;
    float interval = game_get_spawn_interval();

    while (game.spawnAccum >= interval) {
        game.spawnAccum -= interval;
        int elapsed30s = (int)(game.gameTime / 30.0f);
        int maxBatch = mini(3, 1 + elapsed30s / 3);
        int count = rng_range(1, maxBatch);
        game.spawnQueue += count;
    }

    int spawned = 0;
    while (game.spawnQueue > 0 && spawned < 3) {
        spawn_enemy();
        game.spawnQueue--;
        spawned++;
    }

    // Adaptive elite spawns
    int power = game_get_player_power();
    if (power >= 12 && spawned > 0 && rng_range(1, 100) <= 15) {
        if (enemyCount < MAX_ENEMIES) {
            int s = game.arenaShrink;
            int eside = rng_range(1, 4);
            float ex, ey;
            if (eside == 1)      { ex = (float)rng_range(s+20, MAP_W-s-20); ey = (float)(s+15); }
            else if (eside == 2) { ex = (float)rng_range(s+20, MAP_W-s-20); ey = (float)(MAP_H-s-15); }
            else if (eside == 3) { ex = (float)(s+15); ey = (float)rng_range(s+20, MAP_H-s-20); }
            else                 { ex = (float)(MAP_W-s-15); ey = (float)rng_range(s+20, MAP_H-s-20); }

            int ei = entities_spawn_enemy(ex, ey, game_get_current_speed() * 0.4f, ENEMY_ABYSSAL);
            if (ei >= 0) {
                enemies[ei].hp = (5.0f * game_get_hp_scale() + game.enemyHPBonus) * 3.0f;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// XP magnet + collection
// ---------------------------------------------------------------------------
void game_update_xp_magnet(void)
{
    if (player.dead) return;
    float px = player.x;
    float py = player.y;
    float baseRadius = 55.0f + player.level * 2.0f + player.tidecaller * 25.0f;
    float radiusSq = baseRadius * baseRadius;
    float collectDistSq = 225.0f;
    int vacuumActive = game.xpVacuum > 0;

    for (int i = 0; i < xpGemCount; i++) {
        XPGem* g = &xpGems[i];
        if (!g->alive) continue;

        // Despawn timer
        g->lifeFrames--;
        if (g->lifeFrames <= 0) {
            g->alive = 0;
            continue;
        }

        float dx = px - g->x;
        float dy = py - g->y;
        float distSq = dx * dx + dy * dy;

        if (distSq < collectDistSq) {
            // Collect
            g->alive = 0;
            sound_play_xp();
            entities_spawn_fx(g->x, g->y, 1, 2);

            int xpAmount = g->value;
            if (game.streakMultiplier > 1.0f) {
                xpAmount = (int)(xpAmount * game.streakMultiplier);
            }
            player.xp += xpAmount;

            if (player.xp >= player.xpToNext) {
                player.xp -= player.xpToNext;
                player.level++;
                player.xpToNext = (int)(player.xpToNext * XP_LEVEL_SCALE);
                entities_spawn_fx(player.x, player.y, 0, 4);
                sound_play_levelup();
                game.invertTimer = 1;
                game.streakFlashTimer = 2;
                game_generate_upgrades();
            }
        } else if (vacuumActive) {
            float dist = sqrtf(distSq);
            if (dist > 0.0f) {
                g->x += dx / dist * 8.0f;
                g->y += dy / dist * 8.0f;
            }
        } else if (distSq < radiusSq) {
            float dist = sqrtf(distSq);
            if (dist > 0.0f) {
                g->x += dx / dist * 3.0f;
                g->y += dy / dist * 3.0f;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Upgrade generation
// ---------------------------------------------------------------------------
#define PASSIVE_COUNT 5
#define PASSIVE_OILSKIN     0
#define PASSIVE_SEA_LEGS    1
#define PASSIVE_BARNACLE    2
#define PASSIVE_LENS        3
#define PASSIVE_TIDECALLER  4

static const char* passive_names[] = {
    "Oilskin Coat", "Sea Legs", "Salt Ward", "Lighthouse Lens", "Tidecaller"
};
static const char* passive_descs[] = {
    "+2 max HP, full heal", "+25% move speed", "Absorb shield, regen 3s",
    "All projectiles pierce", "+45px XP magnet radius"
};

static char upgNameBufs[MAX_UPGRADE_CHOICES][32];

void game_generate_upgrades(void)
{
    game.state = STATE_UPGRADE;
    game.selectedUpgrade = 0;
    game.upgradeChoiceCount = 0;

    int isFirstLevel = (player.level <= 1) || (player.weaponCount <= 1 && player.level <= 3);

    // Build pool: max ~WEAPON_COUNT + WEAPON_COUNT + PASSIVE_COUNT entries
    typedef struct { int type; int id; } PoolEntry;
    PoolEntry pool[WEAPON_COUNT * 2 + PASSIVE_COUNT];
    int poolCount = 0;

    // New weapons
    if (player.weaponCount < MAX_WEAPONS) {
        for (int wid = 0; wid < WEAPON_COUNT; wid++) {
            int has = 0;
            for (int j = 0; j < player.weaponCount; j++) {
                if (player.weapons[j].id == wid) { has = 1; break; }
            }
            if (!has) {
                pool[poolCount].type = 0; // new weapon
                pool[poolCount].id = wid;
                poolCount++;
            }
        }
    }

    // Weapon upgrades
    for (int j = 0; j < player.weaponCount; j++) {
        if (player.weapons[j].level < 3) {
            pool[poolCount].type = 1; // weapon upgrade
            pool[poolCount].id = player.weapons[j].id;
            poolCount++;
        }
    }

    // Passives (skip if first level, skip barnacle/lens if already owned)
    if (!isFirstLevel) {
        for (int p = 0; p < PASSIVE_COUNT; p++) {
            if (p == PASSIVE_BARNACLE && player.saltWardMax >= 3) continue;
            if (p == PASSIVE_LENS && player.lighthouseLens) continue;
            pool[poolCount].type = 2; // passive
            pool[poolCount].id = p;
            poolCount++;
        }
    }

    // First-level bias: only new weapons
    if (isFirstLevel) {
        int wpCount = 0;
        PoolEntry wpPool[WEAPON_COUNT];
        for (int i = 0; i < poolCount; i++) {
            if (pool[i].type == 0) {
                wpPool[wpCount++] = pool[i];
            }
        }
        if (wpCount > 0) {
            memcpy(pool, wpPool, wpCount * sizeof(PoolEntry));
            poolCount = wpCount;
        }
    }

    // Pick up to 3 random from pool
    int picks = poolCount < 3 ? poolCount : 3;
    for (int i = 0; i < picks; i++) {
        int idx = rng_range(0, poolCount - 1);
        UpgradeChoice* c = &game.upgradeChoices[game.upgradeChoiceCount];
        c->type = pool[idx].type;
        c->id = pool[idx].id;

        int ci = game.upgradeChoiceCount; // index for buffer
        if (pool[idx].type == 0) {
            // New weapon
            snprintf(upgNameBufs[ci], sizeof(upgNameBufs[ci]), "NEW: %s",
                     weapon_get_name((WeaponId)pool[idx].id));
            c->name = upgNameBufs[ci];
            c->desc = weapon_get_desc((WeaponId)pool[idx].id, 1);
        } else if (pool[idx].type == 1) {
            // Weapon upgrade
            int currentLv = 1;
            for (int j = 0; j < player.weaponCount; j++) {
                if (player.weapons[j].id == pool[idx].id) {
                    currentLv = player.weapons[j].level;
                    break;
                }
            }
            snprintf(upgNameBufs[ci], sizeof(upgNameBufs[ci]), "%s Lv%d",
                     weapon_get_name((WeaponId)pool[idx].id), currentLv + 1);
            c->name = upgNameBufs[ci];
            c->desc = weapon_get_desc((WeaponId)pool[idx].id, currentLv + 1);
        } else {
            c->name = passive_names[pool[idx].id];
            c->desc = passive_descs[pool[idx].id];
        }

        game.upgradeChoiceCount++;

        // Remove from pool
        pool[idx] = pool[poolCount - 1];
        poolCount--;
    }

    // Fallback: if no choices, offer passives
    if (game.upgradeChoiceCount == 0) {
        for (int p = 0; p < PASSIVE_COUNT && game.upgradeChoiceCount < 3; p++) {
            UpgradeChoice* c = &game.upgradeChoices[game.upgradeChoiceCount];
            c->type = 2;
            c->id = p;
            c->name = passive_names[p];
            c->desc = passive_descs[p];
            game.upgradeChoiceCount++;
        }
    }
}

// ---------------------------------------------------------------------------
// Apply selected upgrade
// ---------------------------------------------------------------------------
void game_apply_upgrade(int choice)
{
    if (choice < 0 || choice >= game.upgradeChoiceCount) return;

    UpgradeChoice* c = &game.upgradeChoices[choice];

    if (c->type == 0) {
        // New weapon
        if (player.weaponCount < MAX_WEAPONS) {
            Weapon* w = &player.weapons[player.weaponCount];
            w->id = (WeaponId)c->id;
            w->level = 1;
            w->cooldownMs = weapon_get_cooldown(w->id, 1);
            w->lastFiredMs = 0;
            player.weaponCount++;
            game.unlockedWeapons[c->id] = 1;
            game.invertTimer = 3;
            game_trigger_shake(2);
            weapons_calc_synergies();
            save_write();
        }
    } else if (c->type == 1) {
        // Weapon upgrade
        for (int i = 0; i < player.weaponCount; i++) {
            if (player.weapons[i].id == c->id) {
                player.weapons[i].level++;
                player.weapons[i].cooldownMs = weapon_get_cooldown(
                    player.weapons[i].id, player.weapons[i].level);
                break;
            }
        }
    } else if (c->type == 2) {
        // Passive
        switch (c->id) {
        case PASSIVE_OILSKIN:
            player.oilskinCoat++;
            player.maxHp += 2;
            player.hp = player.maxHp;
            break;
        case PASSIVE_SEA_LEGS:
            player.seaLegs++;
            player.moveSpeed = 1.8f * (1.0f + 0.25f * player.seaLegs);
            break;
        case PASSIVE_BARNACLE:
            player.saltWardMax += (player.saltWardMax == 0) ? 2 : 1;
            player.saltWardShield = player.saltWardMax;
            player.saltWardRegenCD = 0;
            break;
        case PASSIVE_LENS:
            player.lighthouseLens = 1;
            break;
        case PASSIVE_TIDECALLER:
            player.tidecaller++;
            break;
        }
    }

    sound_play_confirm();
    game.state = STATE_PLAYING;
}

// ---------------------------------------------------------------------------
// Crate system
// ---------------------------------------------------------------------------
static void spawn_crate(void)
{
    if (crateCount >= MAX_CRATES) return;
    if (player.dead) return;

    float x, y;
    for (int attempt = 0; attempt < 20; attempt++) {
        int s = game.arenaShrink;
        x = (float)rng_range(s + 20, MAP_W - s - 20);
        y = (float)rng_range(s + 20, MAP_H - s - 20);
        float dx = x - player.x;
        float dy = y - player.y;
        if (dx * dx + dy * dy >= 14400.0f) break;
    }

    Crate* c = &crates[crateCount];
    c->x = x;
    c->y = y;
    c->baseY = y;
    c->lifeFrames = 9999; // permanent
    c->bobFrame = 0;
    c->alive = 1;
    crateCount++;
    game.activeCrateIdx = crateCount - 1;
}

static const char* roll_crate_loot(void)
{
    // Crates now only give exotic rewards (weapons/HP/XP moved to enemy drops)
    int roll = rng_range(1, 100);

    if (roll <= 50) {
        player.invulnUntil = pd->system->getCurrentTimeMilliseconds() + 10000;
        return "Eldritch Ward! 10s invuln";
    } else {
        // XP vacuum
        game.xpVacuum = 30;
        game.invertTimer = 2;
        return "Tidecaller's Bell!";
    }
}

void game_collect_crate(void)
{
    const char* reward = roll_crate_loot();
    snprintf(game.crateRewardText, sizeof(game.crateRewardText), "%s", reward);
    game.crateRewardTimer = 30;
    sound_play_crate();
    // Crate drama effects
    game.slowMotionTimer = 6;
    game.streakFlashTimer = 2;
    entities_spawn_particles(player.x, player.y, 8, 1);
    game.state = STATE_CRATE_REWARD;
}

void game_update_crates(void)
{
    if (player.dead) return;

    // Update existing crates
    for (int ci = crateCount - 1; ci >= 0; ci--) {
        Crate* c = &crates[ci];
        if (!c->alive) continue;
        c->bobFrame++;
        if (c->bobFrame % 10 == 0) {
            int bob = ((c->bobFrame / 10) % 2 == 0) ? 1 : -1;
            c->y = c->baseY + bob;
        }

        // Check player collection (441 = 21px²)
        float dx = player.x - c->x;
        float dy = player.y - c->y;
        if (dx * dx + dy * dy < 441.0f) {
            c->alive = 0;
            // Swap-and-pop
            crates[ci] = crates[crateCount - 1];
            crateCount--;
            game.activeCrateIdx = -1;
            game_collect_crate();
            break; // only collect one per frame
        }
    }

    // Spawn logic — first crate at 30s
    if (!game.firstCrateSpawned && game.gameTime >= 30.0f) {
        game.firstCrateSpawned = 1;
        spawn_crate();
        game.lastCrateCheckTime = game.gameTime;
        return;
    }
    if (game.gameTime - game.lastCrateCheckTime >= 30.0f) {
        game.lastCrateCheckTime = game.gameTime;
        if (rng_range(1, 100) <= 40) {
            spawn_crate();
        }
    }
}

// ---------------------------------------------------------------------------
// Pickup system (enemy drops)
// ---------------------------------------------------------------------------
void game_collect_pickup(int idx)
{
    if (idx < 0 || idx >= pickupCount) return;
    Pickup* p = &pickups[idx];

    switch (p->type) {
    case PICKUP_HEALTH:
        player.hp = mini(player.hp + 2, player.maxHp);
        snprintf(game.crateRewardText, sizeof(game.crateRewardText), "Boundless Vigor");
        break;
    case PICKUP_XP_BURST:
        player.xp += player.xpToNext / 2;
        if (player.xp >= player.xpToNext) {
            player.xp -= player.xpToNext;
            player.level++;
            player.xpToNext = (int)(player.xpToNext * XP_LEVEL_SCALE);
            game_generate_upgrades();
        }
        snprintf(game.crateRewardText, sizeof(game.crateRewardText), "XP Burst!");
        break;
    case PICKUP_WEAPON_UPGRADE:
    {
        int upgradable[MAX_WEAPONS];
        int uc = 0;
        for (int i = 0; i < player.weaponCount; i++) {
            if (player.weapons[i].level < 3) {
                upgradable[uc++] = i;
            }
        }
        if (uc > 0) {
            int wi = upgradable[rng_range(0, uc - 1)];
            player.weapons[wi].level++;
            player.weapons[wi].cooldownMs = weapon_get_cooldown(
                player.weapons[wi].id, player.weapons[wi].level);
            snprintf(game.crateRewardText, sizeof(game.crateRewardText),
                     "Artifact Resonated: %s", weapon_get_name(player.weapons[wi].id));
        } else {
            player.hp = mini(player.hp + 2, player.maxHp);
            snprintf(game.crateRewardText, sizeof(game.crateRewardText), "HP Restored +2");
        }
        break;
    }
    case PICKUP_NEW_WEAPON:
    {
        if (player.weaponCount < MAX_WEAPONS) {
            int available[WEAPON_COUNT];
            int ac = 0;
            for (int wid = 0; wid < WEAPON_COUNT; wid++) {
                int has = 0;
                for (int j = 0; j < player.weaponCount; j++) {
                    if (player.weapons[j].id == wid) { has = 1; break; }
                }
                if (!has) available[ac++] = wid;
            }
            if (ac > 0) {
                int wid = available[rng_range(0, ac - 1)];
                Weapon* w = &player.weapons[player.weaponCount];
                w->id = (WeaponId)wid;
                w->level = 1;
                w->cooldownMs = weapon_get_cooldown(w->id, 1);
                w->lastFiredMs = 0;
                player.weaponCount++;
                game.unlockedWeapons[wid] = 1;
                weapons_calc_synergies();
                save_write();
                snprintf(game.crateRewardText, sizeof(game.crateRewardText),
                         "New: %s!", weapon_get_name((WeaponId)wid));
                game.invertTimer = 3;
                game_trigger_shake(2);
            } else {
                player.xp += player.xpToNext / 2;
                snprintf(game.crateRewardText, sizeof(game.crateRewardText), "XP Burst!");
            }
        } else {
            // All weapon slots full, give XP instead
            player.xp += player.xpToNext / 2;
            snprintf(game.crateRewardText, sizeof(game.crateRewardText), "XP Burst!");
        }
        break;
    }
    default:
        break;
    }

    game.crateRewardTimer = 30;
    sound_play_crate();
    entities_spawn_particles(p->x, p->y, 6, 0);
    entities_spawn_fx(p->x, p->y, 0, 1);
    p->alive = 0;
}

void game_update_pickups(void)
{
    if (player.dead) return;

    for (int i = 0; i < pickupCount; i++) {
        Pickup* p = &pickups[i];
        if (!p->alive) continue;

        // Bob animation
        p->bobFrame++;
        if (p->bobFrame % 10 == 0) {
            int bob = ((p->bobFrame / 10) % 2 == 0) ? 1 : -1;
            p->y = p->baseY + bob;
        }

        // Lifetime (health & weapon upgrades persist)
        if (p->type != PICKUP_HEALTH && p->type != PICKUP_WEAPON_UPGRADE) {
            p->lifeFrames--;
            if (p->lifeFrames <= 0) {
                p->alive = 0;
                continue;
            }
        }

        float dx = player.x - p->x;
        float dy = player.y - p->y;
        float d2 = dx * dx + dy * dy;

        // Magnet attraction (80px range) - Exclude Health/Upgrades
        if (p->type != PICKUP_HEALTH && p->type != PICKUP_WEAPON_UPGRADE) {
            if (d2 < 6400.0f && d2 > 1.0f) { // 80px
                float d = sqrtf(d2);
                p->x += dx / d * 2.5f;
                p->y += dy / d * 2.5f;
                p->baseY += dy / d * 2.5f;
            }
        }

        // Collection (18px)
        if (d2 < 324.0f) {
            game_collect_pickup(i);
            i--; // re-check this index after swap-and-pop
            continue;
        }
    }
}

// ---------------------------------------------------------------------------
// Oilskin Coat regen (3+ stacks: +1 HP per 60s)
// ---------------------------------------------------------------------------
static void update_oilskin_regen(void)
{
    if (player.oilskinCoat >= 3 && !player.dead) {
        game.oilskinRegenTimer += FRAME_MS / 1000.0f;
        if (game.oilskinRegenTimer >= 60.0f) {
            game.oilskinRegenTimer -= 60.0f;
            if (player.hp < player.maxHp) {
                player.hp++;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Main update (called every frame by Playdate)
// ---------------------------------------------------------------------------
int update(void* userdata)
{
    (void)userdata;

    PDButtons pushed, current, released;
    pd->system->getButtonState(&current, &pushed, &released);

    game.frameCount++;

    switch (game.state)
    {
    case STATE_TITLE:
        ui_draw_title();
        if (pushed & kButtonUp) {
            game.menuSelection = maxi(1, game.menuSelection - 1);
            sound_play_menu();
        } else if (pushed & kButtonDown) {
            game.menuSelection = mini(3, game.menuSelection + 1);
            sound_play_menu();
        } else if (pushed & kButtonA) {
            if (game.menuSelection == 1) {
                game_start();
            } else if (game.menuSelection == 2) {
                game.armorySelection = 1;
                game.state = STATE_ARMORY;
            } else {
                game.bestiarySelection = 1;
                game.state = STATE_BESTIARY;
            }
        }
        break;

    case STATE_PLAYING:
    {
        game.gameTime += FRAME_MS / 1000.0f;

        update_spawning();
        update_tier();
        check_victory();

        if (game.cooldownBuffTimer > 0) game.cooldownBuffTimer--;

        // Effect timers
        if (game.invertTimer > 0) {
            game.invertTimer--;
            pd->display->setInverted(game.invertTimer > 0);
        }
        if (game.mosaicTimer > 0) {
            game.mosaicTimer--;
            if (game.mosaicTimer > 0)
                pd->display->setMosaic(2, 2);
            else
                pd->display->setMosaic(0, 0);
        }
        if (game.recentKills > 0) game.recentKills--;

        // Streak decay
        if (game.streakWindow > 0) {
            game.streakWindow--;
            if (game.streakWindow <= 0) game.streakKills = 0;
        }
        if (game.streakTimer > 0) {
            game.streakTimer--;
            if (game.streakTimer <= 0) game.streakMultiplier = 1.0f;
        }

        // Speed surge events
        game.surgeCheckTimer += FRAME_MS / 1000.0f;
        if (game.surgeCheckTimer >= 60.0f) {
            game.surgeCheckTimer -= 60.0f;
            if (rng_range(1, 100) <= 20) {
                game.surgeOverlayTimer = 60;
                game.surgeTimer = 150;
                game.enemySpeedMult = 1.5f;
                game_trigger_shake(8);
                sound_play_surge();
            }
        }
        if (game.surgeTimer > 0) {
            game.surgeTimer--;
            if (game.surgeTimer <= 0) game.enemySpeedMult = 1.0f;
        }

        // XP vacuum decay
        if (game.xpVacuum > 0) game.xpVacuum--;

        // Slow-motion timer decay
        if (game.slowMotionTimer > 0) game.slowMotionTimer--;

        // Pickup reward text decay
        if (game.crateRewardTimer > 0) game.crateRewardTimer--;

        game_update_shake();
        game_update_camera();

        // Oilskin regen
        update_oilskin_regen();

        // Salt Ward shield regen (5s cooldown, then 1 point per 3s)
        if (player.saltWardMax > 0 && player.saltWardShield < player.saltWardMax) {
            if (player.saltWardRegenCD > 0) {
                player.saltWardRegenCD--;
            } else if (game.frameCount % 90 == 0) {
                player.saltWardShield++;
            }
        }

        // Update entities
        player_update();
        enemy_update_all();
        bullets_update_all();
        enemy_bullets_update_all();
        weapons_update_tide_pool();
        weapons_update_brine_splash();
        weapons_update_anchors();
        weapons_update_riptides();
        weapons_update_depth_charges();
        game_update_xp_magnet();
        game_update_crates();
        game_update_pickups();
        entities_cleanup_dead();

        // Render
        rendering_draw_playing();
        break;
    }



    case STATE_GAMEOVER:
        ui_draw_game_over();
        if (pushed & kButtonLeft) game.gameOverSelection = 0;
        else if (pushed & kButtonRight) game.gameOverSelection = 1;
        else if (pushed & kButtonA) {
            save_update_high_score();
            pd->display->setInverted(0);
            pd->display->setMosaic(0, 0);
            if (game.gameOverSelection == 0) {
                game_start();
            } else {
                game.gameOverSelection = 0;
                game.state = STATE_TITLE;
            }
        }
        break;

    case STATE_VICTORY:
        ui_draw_victory();
        if (pushed & kButtonA) {
            save_update_high_score();
            pd->display->setInverted(0);
            pd->display->setMosaic(0, 0);
            game_start();
        }
        break;

    case STATE_UPGRADE:
        ui_draw_upgrade_screen();
        if (pushed & kButtonUp) {
            game.selectedUpgrade = maxi(0, game.selectedUpgrade - 1);
            sound_play_menu();
        } else if (pushed & kButtonDown) {
            game.selectedUpgrade = mini(game.upgradeChoiceCount - 1, game.selectedUpgrade + 1);
            sound_play_menu();
        } else if (pushed & kButtonA) {
            game_apply_upgrade(game.selectedUpgrade);
        }
        break;


    case STATE_CUTSCENE:
        ui_draw_cutscene();
        if (game.cutscene.framesLeft > 0) game.cutscene.framesLeft--;
        {
            // Each A press jumps forward 15 frames (fast skip)
            if (pushed & kButtonA) {
                for (int i = 0; i < 15 && game.cutscene.framesLeft > 0; i++)
                    game.cutscene.framesLeft--;
            }
            // Holding A also speeds up continuously (~3x)
            if (current & kButtonA) {
                if (game.cutscene.framesLeft > 0) game.cutscene.framesLeft--;
                if (game.cutscene.framesLeft > 0) game.cutscene.framesLeft--;
            }
            int textElapsed = game.cutscene.totalFrames - game.cutscene.framesLeft;
            int allTextShown = textElapsed >= game.cutscene.lineCount * 25 + 15;
            if (allTextShown && (pushed & kButtonA)) {
                if (game.cutscene.onComplete)
                    game.cutscene.onComplete();
            }
        }
        break;

    case STATE_CRATE_REWARD:
        // Draw game world underneath
        rendering_draw_playing();
        ui_draw_crate_reward();
        game.crateRewardTimer--;
        if (game.crateRewardTimer <= 0)
            game.state = STATE_PLAYING;
        break;

    case STATE_ARMORY:
        ui_draw_armory();
        if (pushed & kButtonUp) game.armorySelection = maxi(1, game.armorySelection - 1);
        else if (pushed & kButtonDown) game.armorySelection = mini(WEAPON_COUNT, game.armorySelection + 1);
        else if (pushed & kButtonB) game.state = STATE_TITLE;
        break;

    case STATE_BESTIARY:
        ui_draw_bestiary();
        if (pushed & kButtonUp) game.bestiarySelection = maxi(1, game.bestiarySelection - 1);
        else if (pushed & kButtonDown) game.bestiarySelection = mini(ENEMY_TYPE_COUNT, game.bestiarySelection + 1);
        else if (pushed & kButtonB) game.state = STATE_TITLE;
        break;
    }

    // Debug overlay
    #if DEBUG_BUILD
    pd->system->drawFPS(380, 0);
    #endif

    return 1; // request next frame
}
