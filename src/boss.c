#include "game.h"

// ---------------------------------------------------------------------------
// Boss spawn thresholds (seconds)
// ---------------------------------------------------------------------------
#define BOSS_CAPTAIN_TIME  180.0f   // minute 3
#define BOSS_MAW_TIME      360.0f   // minute 6
#define BOSS_SHADOW_TIME   450.0f   // minute 7:30

// ---------------------------------------------------------------------------
// Boss stats
// ---------------------------------------------------------------------------
static const float BOSS_HP[]    = { 625.0f, 1125.0f, 1625.0f };
static const float BOSS_SPEED[] = { 1.2f,  0.0f,   1.5f  };
static const int   BOSS_SIZE[]  = { 28,    38,     24    }; // half-size for collision

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void boss_init(void)
{
    memset(&game.boss, 0, sizeof(Boss));
    game.boss.type = BOSS_NONE;
    memset(game.bossSpawned, 0, sizeof(game.bossSpawned));
    game.bossActive = 0;
    game.bossEntranceTimer = 0;
}

// ---------------------------------------------------------------------------
// Boss title card scene draw callbacks
// ---------------------------------------------------------------------------
static void draw_boss_title_captain(void)
{
    // drawFunc is called after GFX_CLEAR + setDrawMode(FillWhite) by ui_draw_cutscene
    ui_draw_centered_text("-- A shape rises from the deep --", 70);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    ui_draw_centered_text("THE DROWNED CAPTAIN", 105);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("\"Still dragging his anchor...\"", 145);
}

static void draw_boss_title_maw(void)
{
    ui_draw_centered_text("-- The sea floor splits open --", 70);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    ui_draw_centered_text("THE ABYSSAL MAW", 105);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("\"It has been waiting.\"", 145);
}

static void draw_boss_title_shadow(void)
{
    ui_draw_centered_text("-- The light turns against you --", 70);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    ui_draw_centered_text("THE LIGHTHOUSE'S SHADOW", 105);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("\"It wears your face.\"", 145);
}

// ---------------------------------------------------------------------------
// Cutscene completion: actually place the boss in the world
// ---------------------------------------------------------------------------
static BossType pendingBossType = BOSS_NONE;

static void boss_cutscene_complete(void)
{
    BossType type = pendingBossType;
    Boss* b = &game.boss;

    // Position the boss
    int s = game.arenaShrink;
    if (type == BOSS_CAPTAIN) {
        int side = rng_range(1, 4);
        if (side == 1)      { b->x = MAP_W / 2.0f; b->y = (float)(s + 20); }
        else if (side == 2) { b->x = MAP_W / 2.0f; b->y = (float)(MAP_H - s - 20); }
        else if (side == 3) { b->x = (float)(s + 20); b->y = MAP_H / 2.0f; }
        else                { b->x = (float)(MAP_W - s - 20); b->y = MAP_H / 2.0f; }
    } else if (type == BOSS_MAW) {
        b->x = MAP_W / 2.0f;
        b->y = (float)(MAP_H - s - 15);
    } else {
        b->x = MAP_W - player.x;
        b->y = MAP_H - player.y;
        b->x = clampf(b->x, (float)(s + 30), (float)(MAP_W - s - 30));
        b->y = clampf(b->y, (float)(s + 30), (float)(MAP_H - s - 30));
    }

    // Entrance FX on return to gameplay
    game.bossEntranceTimer = 45; // 1.5s invuln entrance
    game.invertTimer = 4;
    game.mosaicTimer = 8;
    game_trigger_shake(10);
    sound_play_boom(40.0f, 1.0f, 0.3f);

    game.state = STATE_PLAYING;
}

// ---------------------------------------------------------------------------
// Spawn a boss (triggers title card cutscene first)
// ---------------------------------------------------------------------------
static void boss_spawn(BossType type)
{
    Boss* b = &game.boss;
    memset(b, 0, sizeof(Boss));
    b->type = type;
    b->maxHp = BOSS_HP[type];
    b->hp = b->maxHp;
    b->speed = BOSS_SPEED[type];
    b->alive = 1;
    b->phase = 0;
    b->attackTimer = 90;   // 3s before first attack
    b->summonTimer = 120;
    b->stateTimer = 0;
    b->invulnFrames = 45;  // invuln during entrance

    game.bossActive = 1;
    game.bossSpawned[type] = 1;

    // Kill all existing normal enemies — clear the stage
    for (int i = 0; i < enemyCount; i++) {
        if (enemies[i].alive) {
            enemies[i].alive = 0;
            // Give XP for cleared enemies
            entities_spawn_xp_gem(enemies[i].x, enemies[i].y, 5);
        }
    }
    game.spawnQueue = 0;

    // Title card cutscene
    pendingBossType = type;
    static void (*drawFuncs[])(void) = { draw_boss_title_captain, draw_boss_title_maw, draw_boss_title_shadow };
    static const char* emptyLines[] = { "" };
    game_start_cutscene(emptyLines, 1, drawFuncs[type], 120, boss_cutscene_complete);
}

// ---------------------------------------------------------------------------
// Check if it's time to spawn a boss
// ---------------------------------------------------------------------------
void boss_check_spawn(void)
{
    if (game.bossActive) return;

    float t = game.gameTime;
    if (t >= BOSS_CAPTAIN_TIME && !game.bossSpawned[BOSS_CAPTAIN]) {
        boss_spawn(BOSS_CAPTAIN);
    } else if (t >= BOSS_MAW_TIME && !game.bossSpawned[BOSS_MAW]) {
        boss_spawn(BOSS_MAW);
    } else if (t >= BOSS_SHADOW_TIME && !game.bossSpawned[BOSS_SHADOW]) {
        boss_spawn(BOSS_SHADOW);
    }
}

// ---------------------------------------------------------------------------
// Boss takes damage
// ---------------------------------------------------------------------------
void boss_damage(float amount)
{
    Boss* b = &game.boss;
    if (!b->alive || b->invulnFrames > 0) return;

    b->hp -= amount;
    b->flashTimer = 4;
    entities_spawn_particles(b->x, b->y, 3, 1);

    if (b->hp <= 0) {
        b->hp = 0;
        b->alive = 0;
        game.bossActive = 0;

        // Death FX
        game_trigger_shake(12);
        game.invertTimer = 6;
        entities_spawn_particles(b->x, b->y, 20, 1);
        entities_spawn_fx(b->x, b->y, 0, 1);
        sound_play_boom(35.0f, 1.0f, 0.4f);

        // Score
        game.score += 500;
        game.runKills += 10;

        // Boss rewards
        if (b->type == BOSS_CAPTAIN) {
            // Guaranteed passive upgrade — trigger level up
            snprintf(game.announceText, sizeof(game.announceText), "Captain defeated!");
            game.announceTimer = 60;
            // Give XP to trigger a level-up
            player.xp = player.xpToNext;
        } else if (b->type == BOSS_MAW) {
            snprintf(game.announceText, sizeof(game.announceText), "The Maw silenced!");
            game.announceTimer = 60;
            // Heal player and give big XP
            player.hp = player.maxHp;
            player.xp += player.xpToNext / 2;
        } else if (b->type == BOSS_SHADOW) {
            // Final boss defeat = victory!
            snprintf(game.announceText, sizeof(game.announceText), "Dawn breaks!");
            game.announceTimer = 60;
        }
    }
}

// ---------------------------------------------------------------------------
// Captain AI: charge at player, leave wake, summon creepers
// ---------------------------------------------------------------------------
static void update_captain(Boss* b)
{
    int s = game.arenaShrink;

    // Phase 0: chase player
    // Phase 1: charging (fast dash toward stored angle)
    if (b->phase == 0) {
        // Move toward player
        float dx = player.x - b->x;
        float dy = player.y - b->y;
        float d2 = dx * dx + dy * dy;
        if (d2 > 4.0f) {
            float inv_d = fast_inv_sqrt(d2);
            b->x += dx * inv_d * b->speed;
            b->y += dy * inv_d * b->speed;
        }

        // Attack: charge
        b->attackTimer--;
        if (b->attackTimer <= 0) {
            b->phase = 1;
            b->stateTimer = 20; // windup frames
            b->stateAngle = atan2f(player.y - b->y, player.x - b->x);
            b->attackTimer = 120; // 4s between charges
        }

        // Summon creepers
        b->summonTimer--;
        if (b->summonTimer <= 0) {
            b->summonTimer = 150; // 5s between summons
            for (int i = 0; i < 4 && enemyCount < MAX_ENEMIES - 4; i++) {
                float angle = (float)i * (PI_F * 2.0f / 4.0f);
                float sx = b->x + cosf(angle) * 30.0f;
                float sy = b->y + sinf(angle) * 30.0f;
                int ei = entities_spawn_enemy(sx, sy, game_get_current_speed() * 1.2f, ENEMY_CREEPER);
                if (ei >= 0) {
                    enemies[ei].hp = 3.0f;
                    enemies[ei].maxHp = 3.0f;
                    enemies[ei].packBoost = 1.5f;
                }
            }
            sound_play_boom(70.0f, 0.5f, 0.15f);
            entities_spawn_particles(b->x, b->y, 6, 0);
        }
    } else if (b->phase == 1) {
        b->stateTimer--;
        if (b->stateTimer > 0) {
            // Windup: vibrate
            b->x += (rng_range(-1, 1)) * 1.5f;
            b->y += (rng_range(-1, 1)) * 1.5f;
        } else if (b->stateTimer > -20) {
            // Charge! Fast dash in stored direction
            float chargeSpeed = 6.0f;
            b->x += cosf(b->stateAngle) * chargeSpeed;
            b->y += sinf(b->stateAngle) * chargeSpeed;

            // Leave damaging wake particles
            if (game.frameCount % 2 == 0) {
                entities_spawn_particles(b->x, b->y, 2, 1);
            }

            // Damage player on contact
            if (!player.dead) {
                float pdx = b->x - player.x;
                float pdy = b->y - player.y;
                if (pdx * pdx + pdy * pdy < 900.0f) {
                    player_take_damage();
                    game_trigger_shake(4);
                }
            }
        } else {
            b->phase = 0;
        }
    }

    // Clamp to arena
    b->x = clampf(b->x, (float)(s + 10), (float)(MAP_W - s - 10));
    b->y = clampf(b->y, (float)(s + 10), (float)(MAP_H - s - 10));

    // Contact damage (always)
    if (!player.dead && b->phase == 0) {
        float pdx = b->x - player.x;
        float pdy = b->y - player.y;
        if (pdx * pdx + pdy * pdy < 625.0f) {
            player_take_damage();
        }
    }
}

// ---------------------------------------------------------------------------
// Maw AI: stationary mouth, fires tongue arcs, spawns lampreys
// ---------------------------------------------------------------------------
static void update_maw(Boss* b)
{
    // Maw stays near bottom, sways slowly
    float swayTarget = MAP_W / 2.0f + sinf(game.gameTime * 0.5f) * 100.0f;
    float dx = swayTarget - b->x;
    if (fabsf(dx) > 2.0f) {
        b->x += (dx > 0 ? 1.0f : -1.0f) * 0.8f;
    }

    int s = game.arenaShrink;
    b->x = clampf(b->x, (float)(s + 40), (float)(MAP_W - s - 40));

    // Tongue lash: fire enemy bullets in a spread
    b->attackTimer--;
    if (b->attackTimer <= 0) {
        b->attackTimer = 45; // 1.5s between attacks
        int bulletCount_local = 5 + b->phase * 2;
        float spreadTotal = 1.2f; // ~70 degrees total arc
        float baseAngle = atan2f(player.y - b->y, player.x - b->x);

        for (int i = 0; i < bulletCount_local; i++) {
            float off = (i - (bulletCount_local - 1) * 0.5f) * (spreadTotal / (float)bulletCount_local);
            float angle = baseAngle + off;
            entities_spawn_enemy_bullet(b->x, b->y, cosf(angle), sinf(angle));
        }
        sound_play_weapon(180.0f);
        game_trigger_shake(2);
    }

    // Spawn lampreys
    b->summonTimer--;
    if (b->summonTimer <= 0) {
        b->summonTimer = 120;
        int count = 3 + b->phase;
        for (int i = 0; i < count && enemyCount < MAX_ENEMIES - 4; i++) {
            float sx = b->x + rng_range(-40, 40);
            float sy = b->y - 20.0f;
            int ei = entities_spawn_enemy(sx, sy, game_get_current_speed() * 1.5f, ENEMY_LAMPREY);
            if (ei >= 0) {
                enemies[ei].hp = 1.5f;
                enemies[ei].maxHp = 1.5f;
            }
        }
        entities_spawn_particles(b->x, b->y, 5, 0);
    }

    // Phase transitions based on HP
    if (b->hp < b->maxHp * 0.5f && b->phase == 0) {
        b->phase = 1;
        game_trigger_shake(6);
        game.invertTimer = 3;
        snprintf(game.announceText, sizeof(game.announceText), "The Maw widens!");
        game.announceTimer = 45;
    }

    // Contact damage for player near the maw
    if (!player.dead) {
        float pdx = b->x - player.x;
        float pdy = b->y - player.y;
        if (pdx * pdx + pdy * pdy < 1200.0f) {
            player_take_damage();
        }
    }
}

// ---------------------------------------------------------------------------
// Shadow AI: mirror of player, 3 phases
// ---------------------------------------------------------------------------
static void update_shadow(Boss* b)
{
    int s = game.arenaShrink;

    // Phase 0: ranged — fires bullet spreads at player, maintains distance
    // Phase 1: summon — stops, spawns waves of mixed enemies
    // Phase 2: enrage — rushes player, faster + arena shrinks

    if (b->phase == 0) {
        // Maintain distance: move to keep ~120px from player
        float dx = b->x - player.x;
        float dy = b->y - player.y;
        float d2 = dx * dx + dy * dy;
        float targetDist = 120.0f;

        if (d2 > 1.0f) {
            float d = sqrtf(d2);
            float inv_d = 1.0f / d;
            if (d < targetDist - 10.0f) {
                // Too close, back away
                b->x += dx * inv_d * b->speed * 0.8f;
                b->y += dy * inv_d * b->speed * 0.8f;
            } else if (d > targetDist + 30.0f) {
                // Too far, approach
                b->x -= dx * inv_d * b->speed;
                b->y -= dy * inv_d * b->speed;
            } else {
                // Orbit: move perpendicular
                b->x += -dy * inv_d * b->speed * 0.5f;
                b->y += dx * inv_d * b->speed * 0.5f;
            }
        }

        // Fire bullet spreads
        b->attackTimer--;
        if (b->attackTimer <= 0) {
            b->attackTimer = 30;
            float angle = atan2f(player.y - b->y, player.x - b->x);
            for (int i = -2; i <= 2; i++) {
                entities_spawn_enemy_bullet(b->x, b->y,
                    cosf(angle + i * 0.2f), sinf(angle + i * 0.2f));
            }
            sound_play_weapon(350.0f);
        }

        // Transition to phase 1 at 60% HP
        if (b->hp < b->maxHp * 0.6f) {
            b->phase = 1;
            b->stateTimer = 150; // 5s summon phase
            b->invulnFrames = 20;
            game_trigger_shake(6);
            game.invertTimer = 4;
            snprintf(game.announceText, sizeof(game.announceText), "The Shadow gathers...");
            game.announceTimer = 60;
        }
    } else if (b->phase == 1) {
        // Summon phase: stationary, spawns enemies in waves
        b->stateTimer--;

        if (b->stateTimer % 30 == 0 && b->stateTimer > 0) {
            // Spawn wave
            for (int i = 0; i < 3 && enemyCount < MAX_ENEMIES - 4; i++) {
                float angle = rng_float() * PI_F * 2.0f;
                float sx = b->x + cosf(angle) * 50.0f;
                float sy = b->y + sinf(angle) * 50.0f;
                EnemyType types[] = { ENEMY_WRAITH, ENEMY_TENDRIL, ENEMY_ABYSSAL };
                EnemyType t = types[rng_range(0, 2)];
                int ei = entities_spawn_enemy(sx, sy, game_get_current_speed(), t);
                if (ei >= 0) {
                    enemies[ei].hp = 3.0f;
                    enemies[ei].maxHp = 3.0f;
                }
            }
            entities_spawn_particles(b->x, b->y, 4, 1);
            sound_play_boom(60.0f, 0.4f, 0.1f);
        }

        if (b->stateTimer <= 0) {
            b->phase = 2;
            b->speed = 2.5f;
            game_trigger_shake(8);
            game.invertTimer = 5;
            snprintf(game.announceText, sizeof(game.announceText), "The Shadow enrages!");
            game.announceTimer = 60;
        }
    } else if (b->phase == 2) {
        // Enrage: rush player aggressively
        float dx = player.x - b->x;
        float dy = player.y - b->y;
        float d2 = dx * dx + dy * dy;
        if (d2 > 4.0f) {
            float inv_d = fast_inv_sqrt(d2);
            b->x += dx * inv_d * b->speed;
            b->y += dy * inv_d * b->speed;
        }

        // Rapid fire
        b->attackTimer--;
        if (b->attackTimer <= 0) {
            b->attackTimer = 20;
            float angle = atan2f(player.y - b->y, player.x - b->x);
            for (int i = -1; i <= 1; i++) {
                entities_spawn_enemy_bullet(b->x, b->y,
                    cosf(angle + i * 0.3f), sinf(angle + i * 0.3f));
            }
        }

        // Contact damage
        if (!player.dead) {
            float pdx = b->x - player.x;
            float pdy = b->y - player.y;
            if (pdx * pdx + pdy * pdy < 700.0f) {
                player_take_damage();
            }
        }
    }

    b->x = clampf(b->x, (float)(s + 15), (float)(MAP_W - s - 15));
    b->y = clampf(b->y, (float)(s + 15), (float)(MAP_H - s - 15));
}

// ---------------------------------------------------------------------------
// Update boss (dispatches to per-type AI)
// ---------------------------------------------------------------------------
void boss_update(void)
{
    if (!game.bossActive) return;

    Boss* b = &game.boss;
    if (!b->alive) return;

    // Entrance timer
    if (game.bossEntranceTimer > 0) {
        game.bossEntranceTimer--;
        return; // no AI during entrance
    }

    if (b->invulnFrames > 0) b->invulnFrames--;
    if (b->flashTimer > 0) b->flashTimer--;

    // Boss takes damage from player bullets (check collision)
    int bossSize = BOSS_SIZE[b->type];
    float bossR2 = (float)(bossSize * bossSize) * 4.0f;
    for (int i = 0; i < bulletCount; i++) {
        Bullet* bul = &bullets[i];
        if (!bul->alive) continue;
        float bdx = bul->x - b->x;
        float bdy = bul->y - b->y;
        if (bdx * bdx + bdy * bdy < bossR2) {
            boss_damage(bul->dmg);
            if (!bul->piercing) {
                bul->alive = 0;
            }
            entities_spawn_fx(bul->x, bul->y, 1, 2);
        }
    }

    // Boss takes damage from area weapons (depth charges, anchors, riptides)
    for (int i = 0; i < depthChargeCount; i++) {
        DepthCharge* dc = &depthCharges[i];
        if (!dc->alive || !dc->detonated) continue;
        // Check if boss is in slow field (tick damage)
        float ddx = b->x - dc->x;
        float ddy = b->y - dc->y;
        if (ddx * ddx + ddy * ddy < dc->blastRadius * dc->blastRadius) {
            if (game.frameCount % 10 == 0) boss_damage(dc->dmg * 0.3f);
        }
    }
    for (int i = 0; i < anchorCount; i++) {
        Anchor* a = &anchors[i];
        if (!a->alive) continue;
        float adx = b->x - a->x;
        float ady = b->y - a->y;
        if (adx * adx + ady * ady < 900.0f && game.frameCount % 15 == 0) {
            boss_damage(a->dmg);
        }
    }
    for (int i = 0; i < riptideCount; i++) {
        Riptide* r = &riptides[i];
        if (!r->alive) continue;
        float rdx = b->x - r->x;
        float rdy = b->y - r->y;
        if (rdx * rdx + rdy * rdy < r->pullRadius * r->pullRadius && game.frameCount % 20 == 0) {
            boss_damage(r->dmg);
        }
    }

    // Boss type-specific AI (skip if just died from bullet/area damage)
    if (b->alive) {
        switch (b->type) {
        case BOSS_CAPTAIN: update_captain(b); break;
        case BOSS_MAW:     update_maw(b);     break;
        case BOSS_SHADOW:  update_shadow(b);  break;
        default: break;
        }
    }

    // Check if final boss defeated → victory
    if (b->type == BOSS_SHADOW && !b->alive) {
        static const char* lines[] = {
            "Keeper's Log  --  Dawn",
            "The shadow recedes. Light returns.",
            "The glass is clear at last.",
            "I can see the sea again --",
            "and the horizon is real."
        };
        game_start_cutscene(lines, 5, ui_draw_sunrise_scene, 240, victory_cutscene_complete);
    }
}

// ---------------------------------------------------------------------------
// Boss rendering (called from rendering_draw_playing)
// ---------------------------------------------------------------------------
void boss_render(int cx, int cy)
{
    Boss* b = &game.boss;
    if (!b->alive && !game.bossEntranceTimer) return;

    int sx = (int)b->x - cx;
    int sy = (int)b->y - cy;

    // Skip if off-screen
    if (sx < -60 || sx > SCREEN_W + 60 || sy < -60 || sy > SCREEN_H + 60) return;

    // Flash white on damage
    int flash = (b->flashTimer > 0 && b->flashTimer % 2 == 0);

    // Entrance shimmer
    if (game.bossEntranceTimer > 0 && game.bossEntranceTimer % 3 == 0) return;

    LCDBitmapDrawMode prevMode = pd->graphics->setDrawMode(kDrawModeCopy);

    if (b->type == BOSS_CAPTAIN) {
        // Large humanoid: tall body + anchor + hat
        int sz = BOSS_SIZE[BOSS_CAPTAIN];
        if (flash) pd->graphics->setDrawMode(kDrawModeXOR);

        // Body (tall dark rectangle)
        GFX_FILL(sx - sz + 4, sy - sz, (sz - 4) * 2, sz * 2 + 8, kColorBlack);
        // Shoulders (wider)
        GFX_FILL(sx - sz, sy - sz + 4, sz * 2, 12, kColorBlack);
        // Head
        GFX_ELLIPSE(sx - 10, sy - sz - 16, 20, 18, kColorBlack);
        // Captain's hat (flat top)
        GFX_FILL(sx - 14, sy - sz - 18, 28, 6, kColorBlack);
        // Eyes (white, menacing)
        GFX_FILL(sx - 6, sy - sz - 10, 4, 3, kColorWhite);
        GFX_FILL(sx + 3, sy - sz - 10, 4, 3, kColorWhite);
        // Anchor dragging behind (thick line + crossbar)
        int anchorY = sy + sz + 8;
        pd->graphics->drawLine(sx + sz - 4, sy + 4, sx + sz + 4, anchorY + 10, 3, kColorBlack);
        pd->graphics->drawLine(sx + sz - 2, anchorY + 10, sx + sz + 12, anchorY + 4, 2, kColorBlack);
        pd->graphics->drawLine(sx + sz - 2, anchorY + 10, sx + sz - 8, anchorY + 4, 2, kColorBlack);

        // Charge windup indicator
        if (b->phase == 1 && b->stateTimer > 0) {
            pd->graphics->setDrawMode(kDrawModeXOR);
            int wr = 24 + (20 - b->stateTimer);
            GFX_CIRCLE(sx - wr, sy - wr, wr * 2, wr * 2, 2, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    } else if (b->type == BOSS_MAW) {
        // Massive mouth: wide body with teeth and tentacles
        int sz = BOSS_SIZE[BOSS_MAW];
        if (flash) pd->graphics->setDrawMode(kDrawModeXOR);

        // Body (wide dark ellipse)
        GFX_ELLIPSE(sx - sz * 2, sy - sz, sz * 4, sz * 2, kColorBlack);
        // Mouth opening (white interior, breathing)
        int mouthOpen = 10 + (int)(sinf((float)game.frameCount * 0.1f) * 6.0f);
        GFX_ELLIPSE(sx - sz + 4, sy - mouthOpen, (sz - 4) * 2, mouthOpen * 2, kColorWhite);
        // Teeth (sharp, larger)
        for (int t = -4; t <= 4; t++) {
            int tx = sx + t * (sz / 4);
            int toothH = 10 + (t % 2 == 0 ? 2 : 0);
            // Top teeth
            pd->graphics->drawLine(tx - 4, sy - mouthOpen + 2, tx, sy - mouthOpen + toothH, 2, kColorBlack);
            pd->graphics->drawLine(tx, sy - mouthOpen + toothH, tx + 4, sy - mouthOpen + 2, 2, kColorBlack);
            // Bottom teeth
            pd->graphics->drawLine(tx - 4, sy + mouthOpen - 2, tx, sy + mouthOpen - toothH, 2, kColorBlack);
            pd->graphics->drawLine(tx, sy + mouthOpen - toothH, tx + 4, sy + mouthOpen - 2, 2, kColorBlack);
        }
        // Side tentacles
        for (int t = 0; t < 3; t++) {
            float wave = sinf((float)game.frameCount * 0.12f + t * 2.0f) * 8.0f;
            int lx = sx - sz * 2 - 6 - t * 4;
            int ly = sy - sz / 2 + t * 10 + (int)wave;
            pd->graphics->drawLine(sx - sz * 2, sy - sz / 2 + t * 10, lx, ly, 2, kColorBlack);
            int rx = sx + sz * 2 + 6 + t * 4;
            int ry = sy - sz / 2 + t * 10 + (int)wave;
            pd->graphics->drawLine(sx + sz * 2, sy - sz / 2 + t * 10, rx, ry, 2, kColorBlack);
        }

        // Phase 1: wider mouth, pulsing danger ring
        if (b->phase >= 1) {
            pd->graphics->setDrawMode(kDrawModeXOR);
            int pulse = (game.frameCount / 3) % 2 ? 5 : 0;
            GFX_CIRCLE(sx - sz * 2 - pulse, sy - sz - pulse, (sz * 2 + pulse) * 2, (sz + pulse) * 2, 2, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    } else if (b->type == BOSS_SHADOW) {
        // Mirror of player: large dark silhouette with corruption
        int sz = BOSS_SIZE[BOSS_SHADOW];
        if (flash) pd->graphics->setDrawMode(kDrawModeXOR);

        // Outer aura (always visible — makes boss unmissable)
        pd->graphics->setDrawMode(kDrawModeXOR);
        int auraR = sz + 10 + ((game.frameCount / 4) % 4);
        GFX_CIRCLE(sx - auraR, sy - auraR, auraR * 2, auraR * 2, 1, kColorWhite);
        if (flash) pd->graphics->setDrawMode(kDrawModeXOR);
        else pd->graphics->setDrawMode(kDrawModeCopy);

        // Body (large dark diamond shape)
        GFX_FILL(sx - sz, sy - sz, sz * 2, sz * 2, kColorBlack);
        GFX_ELLIPSE(sx - sz - 4, sy - sz - 4, sz * 2 + 8, sz * 2 + 8, kColorBlack);

        // 6 corruption tendrils (rotating, long)
        int tendrilCount = 6 + b->phase * 2;
        float tendrilLen = (float)(sz + 14 + b->phase * 6);
        for (int t = 0; t < tendrilCount; t++) {
            float angle = (float)t * (PI_F * 2.0f / tendrilCount) + (float)game.frameCount * 0.04f;
            float wave = sinf((float)game.frameCount * 0.15f + t * 1.3f) * 4.0f;
            int tx = sx + (int)(cosf(angle) * (tendrilLen + wave));
            int ty = sy + (int)(sinf(angle) * (tendrilLen + wave));
            pd->graphics->drawLine(sx, sy, tx, ty, 2, kColorBlack);
        }

        // Glowing eyes (larger, wider apart in enrage)
        int eyeSpread = (b->phase == 2) ? 9 : 6;
        int eyeSz = (b->phase == 2) ? 4 : 3;
        GFX_FILL(sx - eyeSpread - 1, sy - 5, eyeSz, eyeSz, kColorWhite);
        GFX_FILL(sx + eyeSpread - eyeSz + 1, sy - 5, eyeSz, eyeSz, kColorWhite);

        // Phase 2: pulsing danger aura (double ring)
        if (b->phase == 2) {
            pd->graphics->setDrawMode(kDrawModeXOR);
            int outerR = sz + 20 + ((game.frameCount / 2) % 8);
            GFX_CIRCLE(sx - outerR, sy - outerR, outerR * 2, outerR * 2, 2, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }

        // Phase 1: summoning ring
        if (b->phase == 1) {
            pd->graphics->setDrawMode(kDrawModeXOR);
            int ringR = 45 + ((game.frameCount / 2) % 12);
            GFX_CIRCLE(sx - ringR, sy - ringR, ringR * 2, ringR * 2, 1, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    pd->graphics->setDrawMode(prevMode);
}

// ---------------------------------------------------------------------------
// Boss health bar (drawn in HUD layer, called from ui_draw_hud)
// ---------------------------------------------------------------------------
void boss_render_health_bar(void)
{
    if (!game.bossActive || !game.boss.alive) return;

    Boss* b = &game.boss;

    // Health bar — wider, taller, name above
    int barW = 260;
    int barH = 10;
    int barX = (SCREEN_W - barW) / 2;
    int barY = 14; // moved down to make room for name above

    // Boss name above bar in bold font
    static const char* bossNames[] = { "DROWNED CAPTAIN", "ABYSSAL MAW", "LIGHTHOUSE SHADOW" };
    if (b->type >= 0 && b->type < BOSS_TYPE_COUNT) {
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        const char* bname = bossNames[b->type];
        int tw = pd->graphics->getTextWidth(game.fontBold, bname, strlen(bname), kASCIIEncoding, 0);
        pd->graphics->drawText(bname, strlen(bname), kASCIIEncoding, (SCREEN_W - tw) / 2, barY - 14);
        if (game.fontBold) pd->graphics->setFont(NULL);
    }

    // Background (white outline)
    GFX_FILL(barX - 1, barY - 1, barW + 2, barH + 2, kColorWhite);

    // Health fill (black = remaining HP)
    float ratio = b->hp / b->maxHp;
    if (ratio < 0) ratio = 0;
    int fillW = (int)(barW * ratio);
    if (fillW > 0) {
        GFX_FILL(barX, barY, fillW, barH, kColorBlack);
    }

    // Depleted section: 1-in-3 dither pattern (crumbling look)
    int emptyX = barX + fillW;
    int emptyW = barW - fillW;
    for (int xx = 0; xx < emptyW; xx += 3) {
        GFX_FILL(emptyX + xx, barY, 1, barH, kColorBlack);
    }
}
