#include "game.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Weapon stat tables
// ---------------------------------------------------------------------------
static const int cooldownTable[WEAPON_COUNT][3] = {
    { 700,  450,  250  }, // Signal Beam
    {   0,    0,    0  }, // Tide Pool (always active)
    { 1800, 1400, 800  }, // Harpoon
    { 2800, 2200, 1400 }, // Brine Splash
    { 1800, 1100, 600  }, // Ghost Light
    { 4000, 2800, 1800 }, // Anchor Drop
    { 7000, 4500, 2800 }, // Foghorn
};

static const char* descTable[WEAPON_COUNT][3] = {
    { "Single shot",         "3-bullet fan",    "5-bullet pierce"   },
    { "2 tidal orbs",        "3 orbs, wider",   "4 orbs, 2 dmg"    },
    { "Heavy pierce, 3 dmg", "5 dmg, faster",   "8 dmg, double"    },
    { "AoE ring, 1 dmg",     "Wider, 2 dmg",    "Huge, 3 dmg+slow" },
    { "Homing wisp",         "2 dmg, sharper",   "2 wisps, retarget"},
    { "Ground hazard, 1 dmg","2 dmg, longer",    "3 dmg + slow"    },
    { "Push + 1 dmg",        "Wider, 2 dmg",     "Huge, 3 dmg+stun"},
};

const char* weapon_get_name(WeaponId id)
{
    static const char* names[] = {
        "Light Beam", "Ward Circle", "Spear of Light",
        "Holy Burst", "Spirit Wisp", "Binding Rune", "Banishing Cry"
    };
    return names[id];
}

const char* weapon_get_desc(WeaponId id, int level)
{
    int lv = clampi(level, 1, 3) - 1;
    return descTable[id][lv];
}

int weapon_get_cooldown(WeaponId id, int level)
{
    int lv = clampi(level, 1, 3) - 1;
    return cooldownTable[id][lv];
}

// ---------------------------------------------------------------------------
// Synergy calculation
// ---------------------------------------------------------------------------
void weapons_calc_synergies(void)
{
    if (game.synergyCacheWeaponCount == player.weaponCount) return;
    game.synergyCacheWeaponCount = player.weaponCount;

    int hasWeapon[WEAPON_COUNT] = {0};
    for (int i = 0; i < player.weaponCount; i++) {
        hasWeapon[player.weapons[i].id] = 1;
    }

    // Resonance: Tide Pool + Brine Splash → burst radius +20%
    game.synergyBurstRadius = (hasWeapon[WEAPON_TIDE_POOL] && hasWeapon[WEAPON_BRINE_SPLASH]) ? 1.2f : 1.0f;
    // Projectile Mastery: Harpoon + Signal Beam → beam +1 damage
    game.synergyBeamDmg = (hasWeapon[WEAPON_HARPOON] && hasWeapon[WEAPON_SIGNAL_BEAM]) ? 1.0f : 0.0f;
    // Supernatural Control: Ghost Light + Anchor → slow 20% stronger
    game.synergyRuneSlow = (hasWeapon[WEAPON_GHOST_LIGHT] && hasWeapon[WEAPON_ANCHOR_DROP]) ? 0.8f : 1.0f;
    // Arsenal Power: Foghorn + 4+ weapons → cry cooldown -20%
    game.synergyCryCooldown = (hasWeapon[WEAPON_FOGHORN] && player.weaponCount >= 4) ? 0.8f : 1.0f;
}

// ---------------------------------------------------------------------------
// Tide Pool: orbiting orbs (updated every frame, no cooldown)
// ---------------------------------------------------------------------------
void weapons_update_tide_pool(void)
{
    int tpIdx = -1;
    for (int i = 0; i < player.weaponCount; i++) {
        if (player.weapons[i].id == WEAPON_TIDE_POOL) { tpIdx = i; break; }
    }
    if (tpIdx < 0 || player.dead) return;

    int lv = player.weapons[tpIdx].level;
    int orbCount[] = { 2, 3, 4 };
    float radius[] = { 45.0f, 60.0f, 68.0f };
    float dmg[] = { 1.0f, 1.0f, 2.0f };
    float speed[] = { 0.08f, 0.10f, 0.14f };
    int li = lv - 1;

    game.tidePoolAngle += speed[li];

    float px = player.x;
    float py = player.y;
    float twoPi = 2.0f * PI_F;

    for (int o = 0; o < orbCount[li]; o++) {
        float a = game.tidePoolAngle + o * (twoPi / orbCount[li]);
        float ox = px + cosf(a) * radius[li];
        float oy = py + sinf(a) * radius[li];

        // Particle trail
        if (game.frameCount % 3 == 0) {
            entities_spawn_particles(ox, oy, 1, 0);
        }

        // Check enemy collisions (196 = 14px²)
        for (int i = 0; i < enemyCount; i++) {
            Enemy* e = &enemies[i];
            if (!e->alive) continue;
            float edx = e->x - ox;
            float edy = e->y - oy;
            if (edx * edx + edy * edy < 441.0f) {
                // Per-enemy hit cooldown: ~15 frames (500ms / 33ms)
                if (game.frameCount - e->lastHitByTidepool >= 15) {
                    e->lastHitByTidepool = game.frameCount;
                    enemy_damage(i, dmg[li]);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Anchor update (ground hazards, timed damage ticks)
// ---------------------------------------------------------------------------
void weapons_update_anchors(void)
{
    for (int a = 0; a < anchorCount; a++) {
        Anchor* anch = &anchors[a];
        if (!anch->alive) continue;

        // Duration expiry
        anch->lifeFrames--;
        if (anch->lifeFrames <= 0) {
            anch->alive = 0;
            continue;
        }

        // Damage enemies in range (324 = 18px²)
        for (int i = 0; i < enemyCount; i++) {
            Enemy* e = &enemies[i];
            if (!e->alive) continue;
            float dx = e->x - anch->x;
            float dy = e->y - anch->y;
            if (dx * dx + dy * dy < 729.0f) {
                // Per-enemy hit cooldown (~21 frames = 700ms)
                if (anch->hitCooldowns[i] <= 0) {
                    anch->hitCooldowns[i] = 21;
                    enemy_damage(i, anch->dmg);
                    // Apply slow if anchor has slow factor
                    if (anch->slowFactor < 1.0f && enemies[i].alive) {
                        enemies[i].slowTimer = 3;
                        enemies[i].slowFactor = anch->slowFactor;
                    }
                }
            }
        }

        // Tick down cooldowns (only active enemy slots)
        for (int i = 0; i < enemyCount; i++) {
            if (anch->hitCooldowns[i] > 0) anch->hitCooldowns[i]--;
        }
    }

    // Cleanup dead anchors (swap-and-pop)
    int i = 0;
    while (i < anchorCount) {
        if (!anchors[i].alive) {
            anchors[i] = anchors[anchorCount - 1];
            anchorCount--;
        } else {
            i++;
        }
    }
}

// ---------------------------------------------------------------------------
// Spawn an anchor
// ---------------------------------------------------------------------------
static void spawn_anchor(float x, float y, float dmg, int durationFrames, float slowFactor)
{
    if (anchorCount >= MAX_ANCHORS) {
        // Expire oldest
        anchors[0].alive = 0;
        anchors[0] = anchors[anchorCount - 1];
        anchorCount--;
    }
    Anchor* a = &anchors[anchorCount];
    memset(a, 0, sizeof(Anchor));
    a->x = x;
    a->y = y;
    a->dmg = dmg;
    a->lifeFrames = durationFrames;
    a->durationFrames = durationFrames;
    a->slowFactor = slowFactor;
    a->alive = 1;
    anchorCount++;
}

// ---------------------------------------------------------------------------
// Fire all weapons
// ---------------------------------------------------------------------------
void weapons_fire_all(void)
{
    uint32_t now = pd->system->getCurrentTimeMilliseconds();

    for (int i = 0; i < player.weaponCount; i++) {
        Weapon* w = &player.weapons[i];

        // Tide Pool fires continuously (handled in weapons_update_tide_pool)
        if (w->id == WEAPON_TIDE_POOL) continue;

        int cd = w->cooldownMs;

        // Cooldown buff from crate
        if (game.cooldownBuffTimer > 0) {
            cd = cd * 3 / 4;
        }

        if (w->id == WEAPON_FOGHORN) {
            cd = (int)(cd * game.synergyCryCooldown);
        }

        if (now - w->lastFiredMs < (uint32_t)cd) continue;
        w->lastFiredMs = now;

        switch (w->id) {
        case WEAPON_SIGNAL_BEAM:
        {
            float dmg = 1.0f + game.synergyBeamDmg;
            float bulletSpd = BULLET_SPEED;
            if (player.seaLegs >= 2) bulletSpd *= 1.15f;
            uint8_t pierce = player.lighthouseLens ? 1 : 0;
            int life = BULLET_LIFETIME_F;

            if (w->level == 1) {
                entities_spawn_bullet(player.x, player.y,
                    player.aimDx, player.aimDy,
                    dmg, bulletSpd, life, pierce, 0, 0, 0, 0);
            } else if (w->level == 2) {
                float spreadRad = 12.0f * PI_F / 180.0f;
                for (int b = -1; b <= 1; b++) {
                    float angle = atan2f(player.aimDy, player.aimDx) + b * spreadRad;
                    entities_spawn_bullet(player.x, player.y,
                        cosf(angle), sinf(angle),
                        dmg, bulletSpd, life, pierce, 0, 0, 0, 0);
                }
            } else {
                dmg = 2.0f + game.synergyBeamDmg;
                float spreadRad = 10.0f * PI_F / 180.0f;
                for (int b = -2; b <= 2; b++) {
                    float angle = atan2f(player.aimDy, player.aimDx) + b * spreadRad;
                    entities_spawn_bullet(player.x, player.y,
                        cosf(angle), sinf(angle),
                        dmg, bulletSpd, life, 1, 0, 0, 0, 0);
                }
            }
            sound_play_weapon(400.0f);
            break;
        }

        case WEAPON_HARPOON:
        {
            float hDmg[] = { 3.0f, 5.0f, 8.0f };
            float hSpd[] = { 4.0f, 5.0f, 5.0f };
            int lv = w->level - 1;
            float bulletSpd = hSpd[lv];
            if (player.seaLegs >= 2) bulletSpd *= 1.15f;
            int life = (int)(800.0f / FRAME_MS);

            game_trigger_shake(1);
            entities_spawn_particles(player.x, player.y, 2, 1);

            if (w->level < 3) {
                entities_spawn_bullet(player.x, player.y,
                    player.aimDx, player.aimDy,
                    hDmg[lv], bulletSpd, life, 1, 0, 0, 0, 1);
            } else {
                float angle = atan2f(player.aimDy, player.aimDx);
                float offset = 8.0f * PI_F / 180.0f;
                entities_spawn_bullet(player.x, player.y,
                    cosf(angle - offset), sinf(angle - offset),
                    hDmg[lv], bulletSpd, life, 1, 0, 0, 0, 1);
                entities_spawn_bullet(player.x, player.y,
                    cosf(angle + offset), sinf(angle + offset),
                    hDmg[lv], bulletSpd, life, 1, 0, 0, 0, 1);
            }
            sound_play_weapon(300.0f);
            break;
        }

        case WEAPON_BRINE_SPLASH:
        {
            int lv = w->level - 1;
            float radius[] = { 60.0f, 83.0f, 105.0f };
            float dmg[] = { 1.0f, 2.0f, 3.0f };
            float r = radius[lv] * game.synergyBurstRadius;

            for (int j = 0; j < enemyCount; j++) {
                Enemy* e = &enemies[j];
                if (!e->alive) continue;
                float edx = e->x - player.x;
                float edy = e->y - player.y;
                if (edx * edx + edy * edy < r * r) {
                    enemy_damage(j, dmg[lv]);
                    if (w->level >= 3 && enemies[j].alive) {
                        enemies[j].slowTimer = 6;
                        enemies[j].slowFactor = 0.5f;
                    }
                }
            }

            // Visual ring
            game.brineSplash.active = 1;
            game.brineSplash.x = player.x;
            game.brineSplash.y = player.y;
            game.brineSplash.maxRadius = r;
            game.brineSplash.radius = 10.0f;
            game.brineSplash.frame = 0;

            game_trigger_shake(2);
            entities_spawn_particles(player.x, player.y, 8, 1);
            sound_play_boom(80.0f, 0.5f, 0.15f);
            break;
        }

        case WEAPON_GHOST_LIGHT:
        {
            float glDmg[] = { 1.0f, 2.0f, 2.0f };
            float glTurn[] = { 10.0f, 20.0f, 25.0f };
            int lv = w->level - 1;
            float bulletSpd = 2.0f;
            if (player.seaLegs >= 2) bulletSpd *= 1.15f;
            int life = (int)(2000.0f / FRAME_MS);
            float turn = glTurn[lv] * PI_F / 180.0f;
            uint8_t retarget = (w->level >= 3) ? 1 : 0;

            if (w->level < 3) {
                entities_spawn_bullet(player.x, player.y,
                    player.aimDx, player.aimDy,
                    glDmg[lv], bulletSpd, life, 0, 1, turn, retarget, 2);
            } else {
                float angle = atan2f(player.aimDy, player.aimDx);
                entities_spawn_bullet(player.x, player.y,
                    cosf(angle - 0.3f), sinf(angle - 0.3f),
                    glDmg[lv], bulletSpd, life, 0, 1, turn, retarget, 2);
                entities_spawn_bullet(player.x, player.y,
                    cosf(angle + 0.3f), sinf(angle + 0.3f),
                    glDmg[lv], bulletSpd, life, 0, 1, turn, retarget, 2);
            }
            sound_play_weapon(600.0f);
            break;
        }

        case WEAPON_ANCHOR_DROP:
        {
            int lv = w->level - 1;
            float dmg[] = { 1.0f, 2.0f, 3.0f };
            int duration[] = { 180, 240, 300 }; // 6s, 8s, 10s in frames
            int maxAnch[] = { 3, 4, 5 };
            float slowFact = (w->level >= 3) ? 0.7f : 1.0f;
            slowFact *= game.synergyRuneSlow;

            // Enforce max anchors
            if (anchorCount >= maxAnch[lv]) {
                // Expire oldest
                if (anchorCount > 0) {
                    anchors[0].alive = 0;
                }
            }

            spawn_anchor(player.x, player.y, dmg[lv], duration[lv], slowFact);
            sound_play_weapon(200.0f);
            entities_spawn_fx(player.x, player.y, 1, 2);
            break;
        }

        case WEAPON_FOGHORN:
        {
            int lv = w->level - 1;
            float radius[] = { 90.0f, 120.0f, 150.0f };
            float pushDist[] = { 45.0f, 60.0f, 90.0f };
            float dmg[] = { 1.0f, 2.0f, 3.0f };
            int stun = (w->level >= 3) ? 1 : 0;

            game_trigger_shake(5);
            entities_spawn_particles(player.x, player.y, 12, 1);

            for (int j = 0; j < enemyCount; j++) {
                Enemy* e = &enemies[j];
                if (!e->alive) continue;
                float edx = e->x - player.x;
                float edy = e->y - player.y;
                float dist = sqrtf(edx * edx + edy * edy);
                if (dist < radius[lv] && dist > 0.0f) {
                    float pushX = edx / dist * pushDist[lv];
                    float pushY = edy / dist * pushDist[lv];
                    int s = game.arenaShrink;
                    e->x = clampf(e->x + pushX, (float)(s + 5), (float)(MAP_W - s - 5));
                    e->y = clampf(e->y + pushY, (float)(s + 5), (float)(MAP_H - s - 5));
                    enemy_damage(j, dmg[lv]);
                    if (stun && enemies[j].alive) {
                        enemies[j].stunTimer = 20;
                    }
                }
            }

            // Visual ring
            game.foghornVisual.active = 1;
            game.foghornVisual.x = player.x;
            game.foghornVisual.y = player.y;
            game.foghornVisual.maxRadius = radius[lv];
            game.foghornVisual.radius = 10.0f;
            game.foghornVisual.frame = 0;

            sound_play_boom(80.0f, 0.5f, 0.15f);
            break;
        }

        default:
            break;
        }
    }
}
