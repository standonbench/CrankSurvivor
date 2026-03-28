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
    { 1200,  900,  600 }, // Chain Lightning
    { 3000, 2400, 1800 }, // Riptide
    { 2500, 2000, 1500 }, // Depth Charge
};

static const char* descTable[WEAPON_COUNT][3] = {
    { "Single shot",         "3-bullet fan",    "5-bullet pierce"   },
    { "2 tidal orbs",        "3 orbs, wider",   "4 orbs, 2 dmg"    },
    { "Heavy pierce, 3 dmg", "5 dmg, faster",   "8 dmg, double"    },
    { "AoE ring, 1 dmg",     "Wider, 2 dmg",    "Huge, 3 dmg+slow" },
    { "Homing wisp",         "2 dmg, sharper",   "2 wisps, retarget"},
    { "Ground hazard, 1 dmg","2 dmg, longer",    "3 dmg + slow"    },
    { "Push + 1 dmg",        "Wider, 2 dmg",     "Huge, 3 dmg+stun"},
    { "Chain 2, 1 dmg",      "Chain 4, 2 dmg",   "Chain 6+stun"    },
    { "Pull vortex, 1 dmg",  "Wider, 2 dmg",     "2 vortexes, 1.5x"},
    { "Mine, 3 dmg",         "2 mines, 4 dmg",   "3 mines+slow"    },
};

const char* weapon_get_name(WeaponId id)
{
    static const char* names[] = {
        "Light Beam", "Ward Circle", "Spear of Light",
        "Holy Burst", "Spirit Wisp", "Binding Rune", "Banishing Cry",
        "Chain Bolt", "Undertow", "Abyssal Mine"
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
    // Static Charge: Chain Lightning + Tide Pool → orb chain chance
    game.synergyStaticCharge = (hasWeapon[WEAPON_CHAIN_LIGHTNING] && hasWeapon[WEAPON_TIDE_POOL]) ? 1.0f : 0.0f;
    // Maelstrom: Riptide + Brine Splash → Brine +30% dmg in vortex
    game.synergyMaelstrom = (hasWeapon[WEAPON_RIPTIDE] && hasWeapon[WEAPON_BRINE_SPLASH]) ? 1.3f : 1.0f;
    // Depth Hunter: Depth Charge + Harpoon → harpoon kills drop mini-mines
    game.synergyDepthHunter = (hasWeapon[WEAPON_DEPTH_CHARGE] && hasWeapon[WEAPON_HARPOON]) ? 1.0f : 0.0f;
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
                    // Static Charge synergy: 10% chain on orb hit
                    if (game.synergyStaticCharge > 0 && enemies[i].alive && rng_range(1, 100) <= 10) {
                        weapons_fire_chain_at(e->x, e->y, 1.0f, 2, 0, i);
                    }
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
// Chain Lightning: bolt that chains between nearby enemies
// ---------------------------------------------------------------------------
void weapons_fire_chain_at(float x, float y, float dmg, int chainsLeft, int stunChance, int sourceEnemy)
{
    if (chainsLeft <= 0) return;

    // Find nearest enemy within 60px that isn't the source
    float bestDist = 3600.0f; // 60px squared
    int bestIdx = -1;
    for (int i = 0; i < enemyCount; i++) {
        if (i == sourceEnemy || !enemies[i].alive) continue;
        float dx = enemies[i].x - x;
        float dy = enemies[i].y - y;
        float d2 = dx * dx + dy * dy;
        if (d2 < bestDist) {
            bestDist = d2;
            bestIdx = i;
        }
    }

    if (bestIdx < 0) return;

    // Visual arc
    ChainVisual* cv = &chainVisuals[chainVisualIdx];
    cv->x1 = x;
    cv->y1 = y;
    cv->x2 = enemies[bestIdx].x;
    cv->y2 = enemies[bestIdx].y;
    cv->life = 3;
    chainVisualIdx = (chainVisualIdx + 1) % MAX_CHAIN_VISUALS;

    // Damage + effects
    enemy_damage(bestIdx, dmg);
    entities_spawn_particles(enemies[bestIdx].x, enemies[bestIdx].y, 2, 0);

    // Stun chance
    if (stunChance > 0 && enemies[bestIdx].alive && rng_range(1, 100) <= stunChance) {
        enemies[bestIdx].stunTimer = 10;
    }

    // Chain further
    if (chainsLeft > 1 && enemies[bestIdx].alive) {
        weapons_fire_chain_at(enemies[bestIdx].x, enemies[bestIdx].y, dmg, chainsLeft - 1, stunChance, bestIdx);
    }
}

// ---------------------------------------------------------------------------
// Riptide: pulling vortex that damages enemies
// ---------------------------------------------------------------------------
static void spawn_riptide(float x, float y, float dmg, float pullRadius, float dmgMult, int lifeFrames)
{
    if (riptideCount >= MAX_RIPTIDES) {
        // Expire oldest
        riptides[0].alive = 0;
        riptides[0] = riptides[riptideCount - 1];
        riptideCount--;
    }
    Riptide* r = &riptides[riptideCount];
    memset(r, 0, sizeof(Riptide));
    r->x = x;
    r->y = y;
    r->dmg = dmg;
    r->pullRadius = pullRadius;
    r->dmgMult = dmgMult;
    r->lifeFrames = lifeFrames;
    r->tickTimer = 0;
    r->alive = 1;
    riptideCount++;
}

void weapons_update_riptides(void)
{
    for (int ri = 0; ri < riptideCount; ri++) {
        Riptide* r = &riptides[ri];
        if (!r->alive) continue;

        r->lifeFrames--;
        if (r->lifeFrames <= 0) {
            r->alive = 0;
            continue;
        }

        r->tickTimer++;
        float r2 = r->pullRadius * r->pullRadius;

        for (int i = 0; i < enemyCount; i++) {
            Enemy* e = &enemies[i];
            if (!e->alive) continue;
            float dx = r->x - e->x;
            float dy = r->y - e->y;
            float d2 = dx * dx + dy * dy;
            if (d2 < r2 && d2 > 1.0f) {
                // Pull toward center
                float d = sqrtf(d2);
                float pullStr = 1.5f;
                int s = game.arenaShrink;
                e->x = clampf(e->x + dx / d * pullStr, (float)(s + 5), (float)(MAP_W - s - 5));
                e->y = clampf(e->y + dy / d * pullStr, (float)(s + 5), (float)(MAP_H - s - 5));

                // Tick damage every 15 frames
                if (r->tickTimer % 15 == 0) {
                    if (r->hitCooldowns[i] <= 0) {
                        float dmg = r->dmg;
                        // Center bonus (within 30% of radius)
                        if (d < r->pullRadius * 0.3f) dmg *= r->dmgMult;
                        // Maelstrom synergy check done at Brine Splash fire time
                        enemy_damage(i, dmg);
                        r->hitCooldowns[i] = 10;
                    }
                }
            }
        }

        // Tick down cooldowns
        if (r->tickTimer % 15 == 0) {
            for (int i = 0; i < enemyCount; i++) {
                if (r->hitCooldowns[i] > 0) r->hitCooldowns[i]--;
            }
        }

        // Particles spiraling inward
        if (game.frameCount % 3 == 0) {
            float angle = rng_float() * PI_F * 2.0f;
            float px = r->x + cosf(angle) * r->pullRadius * 0.8f;
            float py = r->y + sinf(angle) * r->pullRadius * 0.8f;
            entities_spawn_particles(px, py, 1, 0);
        }
    }

    // Cleanup dead riptides
    int i = 0;
    while (i < riptideCount) {
        if (!riptides[i].alive) {
            riptides[i] = riptides[riptideCount - 1];
            riptideCount--;
        } else {
            i++;
        }
    }
}

// ---------------------------------------------------------------------------
// Depth Charge: delayed detonation mine
// ---------------------------------------------------------------------------
static void spawn_depth_charge(float x, float y, float dmg, float blastRadius, float slowFactor, int slowFieldLife)
{
    if (depthChargeCount >= MAX_DEPTH_CHARGES) {
        // Expire oldest
        depthCharges[0].alive = 0;
        depthCharges[0] = depthCharges[depthChargeCount - 1];
        depthChargeCount--;
    }
    DepthCharge* dc = &depthCharges[depthChargeCount];
    memset(dc, 0, sizeof(DepthCharge));
    dc->x = x;
    dc->y = y;
    dc->dmg = dmg;
    dc->blastRadius = blastRadius;
    dc->slowFactor = slowFactor;
    dc->fuseFrames = 30; // 1 second fuse
    dc->slowFieldLife = slowFieldLife;
    dc->alive = 1;
    dc->detonated = 0;
    depthChargeCount++;
}

void weapons_update_depth_charges(void)
{
    for (int di = 0; di < depthChargeCount; di++) {
        DepthCharge* dc = &depthCharges[di];
        if (!dc->alive) continue;

        if (!dc->detonated) {
            dc->fuseFrames--;
            if (dc->fuseFrames <= 0) {
                // DETONATE
                dc->detonated = 1;
                float r2 = dc->blastRadius * dc->blastRadius;

                for (int i = 0; i < enemyCount; i++) {
                    Enemy* e = &enemies[i];
                    if (!e->alive) continue;
                    float dx = e->x - dc->x;
                    float dy = e->y - dc->y;
                    if (dx * dx + dy * dy < r2) {
                        enemy_damage(i, dc->dmg);
                        if (dc->slowFactor > 0 && enemies[i].alive) {
                            enemies[i].slowTimer = 6;
                            enemies[i].slowFactor = 0.5f;
                        }
                    }
                }

                game_trigger_shake(4);
                entities_spawn_particles(dc->x, dc->y, 10, 1);
                sound_play_boom(60.0f, 0.7f, 0.2f);
                entities_spawn_fx(dc->x, dc->y, 0, 1);

                // Leave slow field or die
                if (dc->slowFieldLife > 0) {
                    dc->fuseFrames = dc->slowFieldLife; // reuse as slow field timer
                } else {
                    dc->alive = 0;
                }
            }
        } else {
            // Slow field phase
            dc->fuseFrames--;
            if (dc->fuseFrames <= 0) {
                dc->alive = 0;
                continue;
            }
            // Slow enemies in range
            float r2 = dc->blastRadius * dc->blastRadius;
            for (int i = 0; i < enemyCount; i++) {
                Enemy* e = &enemies[i];
                if (!e->alive) continue;
                float dx = e->x - dc->x;
                float dy = e->y - dc->y;
                if (dx * dx + dy * dy < r2) {
                    e->slowTimer = 3;
                    e->slowFactor = 0.5f;
                }
            }
        }
    }

    // Cleanup
    int i = 0;
    while (i < depthChargeCount) {
        if (!depthCharges[i].alive) {
            depthCharges[i] = depthCharges[depthChargeCount - 1];
            depthChargeCount--;
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
        // Riptide vortexes are persistent (handled in weapons_update_riptides)
        // but still has a cooldown for spawning new ones

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
            entities_spawn_fx(player.x, player.y, 1, 1); // muzzle flash
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

            game_trigger_shake(2);
            entities_spawn_particles(player.x, player.y, 3, 1);

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
                    float bdmg = dmg[lv];
                    // Maelstrom synergy: +30% dmg if enemy is in a riptide
                    if (game.synergyMaelstrom > 1.0f) {
                        for (int ri = 0; ri < riptideCount; ri++) {
                            if (!riptides[ri].alive) continue;
                            float rdx = e->x - riptides[ri].x;
                            float rdy = e->y - riptides[ri].y;
                            if (rdx * rdx + rdy * rdy < riptides[ri].pullRadius * riptides[ri].pullRadius) {
                                bdmg *= game.synergyMaelstrom;
                                break;
                            }
                        }
                    }
                    enemy_damage(j, bdmg);
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

        case WEAPON_CHAIN_LIGHTNING:
        {
            int lv = w->level - 1;
            float clDmg[] = { 1.0f, 2.0f, 2.0f };
            int chains[] = { 3, 5, 7 }; // initial hit + chains
            int stunPct = (w->level >= 3) ? 20 : 0;

            // Find nearest enemy to fire initial bolt at
            float bestDist = 10000.0f;
            int bestIdx = -1;
            for (int j = 0; j < enemyCount; j++) {
                if (!enemies[j].alive) continue;
                float dx = enemies[j].x - player.x;
                float dy = enemies[j].y - player.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestDist) {
                    bestDist = d2;
                    bestIdx = j;
                }
            }

            if (bestIdx >= 0) {
                // Visual arc from player to first target
                ChainVisual* cv = &chainVisuals[chainVisualIdx];
                cv->x1 = player.x;
                cv->y1 = player.y;
                cv->x2 = enemies[bestIdx].x;
                cv->y2 = enemies[bestIdx].y;
                cv->life = 3;
                chainVisualIdx = (chainVisualIdx + 1) % MAX_CHAIN_VISUALS;

                enemy_damage(bestIdx, clDmg[lv]);
                entities_spawn_particles(enemies[bestIdx].x, enemies[bestIdx].y, 3, 0);

                if (stunPct > 0 && enemies[bestIdx].alive && rng_range(1, 100) <= stunPct) {
                    enemies[bestIdx].stunTimer = 10;
                }

                // Chain to more enemies
                if (enemies[bestIdx].alive) {
                    weapons_fire_chain_at(enemies[bestIdx].x, enemies[bestIdx].y, clDmg[lv], chains[lv] - 1, stunPct, bestIdx);
                }

                sound_play_weapon(800.0f);
                game_trigger_shake(1);
            }
            break;
        }

        case WEAPON_RIPTIDE:
        {
            int lv = w->level - 1;
            float rtDmg[] = { 1.0f, 2.0f, 2.0f };
            float rtRadius[] = { 45.0f, 55.0f, 65.0f };
            int rtLife[] = { 90, 120, 120 };
            float rtMult = (w->level >= 3) ? 1.5f : 1.0f;

            // Fire toward aim direction, spawn vortex 50px away
            float spawnDist = 50.0f;
            float vx = player.x + player.aimDx * spawnDist;
            float vy = player.y + player.aimDy * spawnDist;
            int s = game.arenaShrink;
            vx = clampf(vx, (float)(s + 10), (float)(MAP_W - s - 10));
            vy = clampf(vy, (float)(s + 10), (float)(MAP_H - s - 10));

            spawn_riptide(vx, vy, rtDmg[lv], rtRadius[lv], rtMult, rtLife[lv]);

            if (w->level >= 3) {
                // Second vortex at 30 degrees offset
                float angle = atan2f(player.aimDy, player.aimDx) + 0.52f;
                float vx2 = player.x + cosf(angle) * spawnDist;
                float vy2 = player.y + sinf(angle) * spawnDist;
                vx2 = clampf(vx2, (float)(s + 10), (float)(MAP_W - s - 10));
                vy2 = clampf(vy2, (float)(s + 10), (float)(MAP_H - s - 10));
                spawn_riptide(vx2, vy2, rtDmg[lv], rtRadius[lv], rtMult, rtLife[lv]);
            }

            sound_play_weapon(150.0f);
            entities_spawn_particles(vx, vy, 4, 0);
            break;
        }

        case WEAPON_DEPTH_CHARGE:
        {
            int lv = w->level - 1;
            float dcDmg[] = { 3.0f, 4.0f, 5.0f };
            float dcRadius[] = { 35.0f, 40.0f, 45.0f };
            int mineCount[] = { 1, 2, 3 };
            int slowLife = (w->level >= 3) ? 60 : 0;
            float slowFact = (w->level >= 3) ? 0.5f : 0.0f;

            float baseAngle = atan2f(player.aimDy, player.aimDx);
            float spawnDist = 60.0f;
            float spreadAngle = 0.35f; // ~20 degrees

            for (int m = 0; m < mineCount[lv]; m++) {
                float angle = baseAngle;
                if (mineCount[lv] > 1) {
                    angle += (m - (mineCount[lv] - 1) * 0.5f) * spreadAngle;
                }
                float mx = player.x + cosf(angle) * spawnDist;
                float my = player.y + sinf(angle) * spawnDist;
                int s = game.arenaShrink;
                mx = clampf(mx, (float)(s + 10), (float)(MAP_W - s - 10));
                my = clampf(my, (float)(s + 10), (float)(MAP_H - s - 10));
                spawn_depth_charge(mx, my, dcDmg[lv], dcRadius[lv], slowFact, slowLife);
            }

            sound_play_weapon(250.0f);
            break;
        }

        default:
            break;
        }
    }
}
