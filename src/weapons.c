#include "game.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Weapon stat tables
// ---------------------------------------------------------------------------
static const int cooldownTable[WEAPON_COUNT][3] = {
    { 700,  500,  350  }, // Signal Beam (was 450/250)
    {   0,    0,    0  }, // Tide Pool (always active)
    { 1800, 1400, 1000 }, // Harpoon (was 800)
    {    0,    0,    0  }, // Brine Splash (always active)
    { 1500,  900,  600 }, // Ghost Light (was 1800/1100)
    { 3000, 2200, 1500 }, // Anchor Drop (was 4000/2800/1800)
    { 6000, 4500, 2800 }, // Foghorn (was 7000)
    { 1200,  900,  700 }, // Chain Lightning (was 600)
    { 3000, 2400, 1800 }, // Riptide
    { 2500, 2000, 1800 }, // Depth Charge (was 1500)
};

static const char* descTable[WEAPON_COUNT][3] = {
    { "Single shot",         "3-bullet fan",    "4-bullet pierce"   },
    { "2 tidal orbs",        "3 orbs, wider",   "3 orbs, 1.5 dmg"  },
    { "Heavy pierce, 3 dmg", "4 dmg, faster",   "6 dmg, double"    },
    { "Aura ring, 0.5/s",   "Wider, 1 dmg/s",  "Large, 2/s+slow"  },
    { "Homing wisp, 1.5",   "2 wisps, 2 dmg",   "3 wisps, retarget"},
    { "Ground trap, 1.5",   "2.5 dmg, longer",  "4 dmg + slow"    },
    { "Push + 1 dmg",        "Wider, 2 dmg",     "Huge, 3 dmg+stun"},
    { "Chain 2, 1 dmg",      "Chain 3, 1.5 dmg", "Chain 5+stun"    },
    { "Pull vortex, 1 dmg",  "Wider, 1.5 dmg",  "2 vortexes, 1.3x"},
    { "Mine, 2 dmg",         "2 mines, 3 dmg",   "2 mines+slow"    },
};

const char* weapon_get_name(WeaponId id)
{
    static const char* names[] = {
        "Light Beam", "Ward Circle", "Spear of Light",
        "Holy Burst", "Spirit Wisp", "Binding Rune", "Banishing Cry",
        "Chain Bolt", "Undertow", "Abyssal Mine"
    };
    if (id < 0 || id >= WEAPON_COUNT) return "Unknown";
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
    // Arsenal Power: Foghorn + 4+ weapons → cry cooldown -25%
    game.synergyCryCooldown = (hasWeapon[WEAPON_FOGHORN] && player.weaponCount >= 4) ? 0.75f : 1.0f;
    // Static Charge: Chain Lightning + Tide Pool → orb chain chance
    game.synergyStaticCharge = (hasWeapon[WEAPON_CHAIN_LIGHTNING] && hasWeapon[WEAPON_TIDE_POOL]) ? 1.0f : 0.0f;
    // Maelstrom: Riptide + Brine Splash → Brine +20% dmg in vortex
    game.synergyMaelstrom = (hasWeapon[WEAPON_RIPTIDE] && hasWeapon[WEAPON_BRINE_SPLASH]) ? 1.2f : 1.0f;
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
    int orbCount[] = { 2, 3, 3 };
    float radius[] = { 40.0f, 52.0f, 60.0f };
    float dmg[] = { 1.0f, 1.0f, 1.5f };
    float speed[] = { 0.10f, 0.13f, 0.16f };
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

        // Check enemy collisions via spatial grid
        int nearby[64];
        int nearCount = collision_query_point(ox, oy, 21.0f, nearby, 64);
        for (int ni = 0; ni < nearCount; ni++) {
            int i = nearby[ni];
            Enemy* e = &enemies[i];
            float edx = e->x - ox;
            float edy = e->y - oy;
            if (edx * edx + edy * edy < 441.0f) {
                if (game.frameCount - e->lastHitByTidepool >= 15) {
                    e->lastHitByTidepool = game.frameCount;
                    enemy_damage(i, dmg[li]);
                    // Static Charge synergy: 5% chain on orb hit
                    if (game.synergyStaticCharge > 0 && enemies[i].alive && rng_range(1, 100) <= 5) {
                        weapons_fire_chain_at(e->x, e->y, 1.0f, 2, 0, i);
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Brine Splash (Constant "Garlic" Aura)
// ---------------------------------------------------------------------------
void weapons_update_brine_splash(void)
{
    int bsIdx = -1;
    for (int i = 0; i < player.weaponCount; i++) {
        if (player.weapons[i].id == WEAPON_BRINE_SPLASH) { bsIdx = i; break; }
    }
    if (bsIdx < 0 || player.dead) {
        game.brineSplash.active = 0;
        return; 
    }

    int lv = player.weapons[bsIdx].level;
    float radius[] = { 50.0f, 68.0f, 85.0f };
    float dmg[] = { 0.5f, 1.0f, 2.0f };
    float r = radius[lv - 1] * game.synergyBurstRadius;

    // Visual ring stays permanently on the player
    game.brineSplash.active = 1;
    game.brineSplash.x = player.x;
    game.brineSplash.y = player.y;
    game.brineSplash.maxRadius = r;
    game.brineSplash.radius = r + (sinf(game.frameCount * 0.1f) * 2.0f);

    // Tick damage every 30 frames (1s) via spatial grid
    if (game.frameCount % 30 == 0) {
        int nearby[64];
        int nearCount = collision_query_point(player.x, player.y, r, nearby, 64);
        for (int ni = 0; ni < nearCount; ni++) {
            int j = nearby[ni];
            Enemy* e = &enemies[j];
            float edx = e->x - player.x;
            float edy = e->y - player.y;
            if (edx * edx + edy * edy <= r * r) {
                float bdmg = dmg[lv - 1];
                // Maelstrom synergy
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
                if (lv >= 3 && enemies[j].alive) {
                    enemies[j].slowTimer = 30;
                    enemies[j].slowFactor = 0.5f;
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

        // Damage enemies in range via spatial grid (27px radius)
        int nearby[64];
        int nearCount = collision_query_point(anch->x, anch->y, 27.0f, nearby, 64);
        for (int ni = 0; ni < nearCount; ni++) {
            int i = nearby[ni];
            if (i >= MAX_ENEMIES) continue;
            Enemy* e = &enemies[i];
            float dx = e->x - anch->x;
            float dy = e->y - anch->y;
            if (dx * dx + dy * dy < 729.0f) {
                if (anch->hitCooldowns[i] <= 0) {
                    anch->hitCooldowns[i] = 21;
                    enemy_damage(i, anch->dmg);
                    if (anch->slowFactor < 1.0f && enemies[i].alive) {
                        enemies[i].slowTimer = 3;
                        enemies[i].slowFactor = anch->slowFactor;
                    }
                }
            }
        }

        // Tick down cooldowns for nearby enemies only
        for (int ni = 0; ni < nearCount; ni++) {
            int i = nearby[ni];
            if (i < MAX_ENEMIES && anch->hitCooldowns[i] > 0) anch->hitCooldowns[i]--;
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
    float cx = x, cy = y;
    int src = sourceEnemy;

    for (int c = 0; c < chainsLeft; c++) {
        // Find nearest enemy within 60px via spatial grid
        int nearby[16];
        int nearCount = collision_query_point(cx, cy, 60.0f, nearby, 16);
        float bestDist = 3600.0f;
        int bestIdx = -1;
        for (int ni = 0; ni < nearCount; ni++) {
            int i = nearby[ni];
            if (i == src || !enemies[i].alive) continue;
            float dx = enemies[i].x - cx;
            float dy = enemies[i].y - cy;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestDist) {
                bestDist = d2;
                bestIdx = i;
            }
        }

        if (bestIdx < 0) break;

        // Visual arc
        ChainVisual* cv = &chainVisuals[chainVisualIdx];
        cv->x1 = cx;
        cv->y1 = cy;
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

        if (!enemies[bestIdx].alive) break;
        cx = enemies[bestIdx].x;
        cy = enemies[bestIdx].y;
        src = bestIdx;
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

        // Query nearby enemies via spatial grid
        int nearby[64];
        int nearCount = collision_query_point(r->x, r->y, r->pullRadius, nearby, 64);
        for (int ni = 0; ni < nearCount; ni++) {
            int i = nearby[ni];
            if (i >= MAX_ENEMIES) continue;
            Enemy* e = &enemies[i];
            float dx = r->x - e->x;
            float dy = r->y - e->y;
            float d2 = dx * dx + dy * dy;
            if (d2 < r2 && d2 > 1.0f) {
                // Pull toward center using fast_inv_sqrt
                float inv_d = fast_inv_sqrt(d2);
                float d = d2 * inv_d;
                float pullRatio = 1.0f - (d / r->pullRadius);
                float pullStr = 0.8f + (pullRatio * 2.2f); // max 3.0 at center (was 4.5)
                int s = game.arenaShrink;
                float tx = -dy * inv_d;
                float ty = dx * inv_d;
                float ndx = dx * inv_d;
                float ndy = dy * inv_d;
                e->x = clampf(e->x + (ndx * pullStr) + (tx * pullRatio * 2.0f), (float)(s + 5), (float)(MAP_W - s - 5));
                e->y = clampf(e->y + (ndy * pullStr) + (ty * pullRatio * 2.0f), (float)(s + 5), (float)(MAP_H - s - 5));

                // Tick damage every 20 frames (was 15)
                if (r->tickTimer % 20 == 0) {
                    if (r->hitCooldowns[i] <= 0) {
                        float dmg = r->dmg;
                        if (d < r->pullRadius * 0.3f) dmg *= r->dmgMult;
                        enemy_damage(i, dmg);
                        r->hitCooldowns[i] = 10;
                    }
                }
            }
        }

        // Tick down cooldowns for nearby enemies only
        if (r->tickTimer % 20 == 0) {
            for (int ni = 0; ni < nearCount; ni++) {
                int i = nearby[ni];
                if (i < MAX_ENEMIES && r->hitCooldowns[i] > 0) r->hitCooldowns[i]--;
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
    dc->fuseFrames = 20; // ~0.67 second fuse (faster detonation)
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

                // Detonation: damage nearby enemies via spatial grid
                int detNear[64];
                int detCount = collision_query_point(dc->x, dc->y, dc->blastRadius, detNear, 64);
                for (int ni = 0; ni < detCount; ni++) {
                    int i = detNear[ni];
                    Enemy* e = &enemies[i];
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
            // Slow enemies in range via spatial grid
            int slowNear[64];
            int slowCount = collision_query_point(dc->x, dc->y, dc->blastRadius, slowNear, 64);
            float r2 = dc->blastRadius * dc->blastRadius;
            for (int ni = 0; ni < slowCount; ni++) {
                int i = slowNear[ni];
                Enemy* e = &enemies[i];
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
    a->durationFrames = durationFrames;
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
            // Now handled passively in weapons_update_brine_splash
            break;
        }

        case WEAPON_GHOST_LIGHT:
        {
            float glDmg[] = { 1.5f, 2.0f, 2.5f };
            float glTurn[] = { 10.0f, 20.0f, 25.0f };
            int glCount[] = { 1, 2, 3 };
            int lv = w->level - 1;
            float bulletSpd = 2.0f;
            if (player.seaLegs >= 2) bulletSpd *= 1.15f;
            int life = (int)(2000.0f / FRAME_MS);
            float turn = glTurn[lv] * PI_F / 180.0f;
            uint8_t retarget = (w->level >= 3) ? 1 : 0;
            float angle = atan2f(player.aimDy, player.aimDx);
            int count = glCount[lv];
            float spread = 0.3f;

            for (int wi = 0; wi < count; wi++) {
                float off = (count > 1) ? (wi - (count - 1) * 0.5f) * spread : 0.0f;
                entities_spawn_bullet(player.x, player.y,
                    cosf(angle + off), sinf(angle + off),
                    glDmg[lv], bulletSpd, life, 0, 1, turn, retarget, 2);
            }
            sound_play_weapon(600.0f);
            break;
        }

        case WEAPON_ANCHOR_DROP:
        {
            int lv = w->level - 1;
            float dmg[] = { 1.5f, 2.5f, 4.0f };
            int duration[] = { 240, 300, 360 }; // 8s, 10s, 12s in frames
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

            // Use spatial grid for foghorn blast
            int fhNear[64];
            int fhCount = collision_query_point(player.x, player.y, radius[lv], fhNear, 64);
            for (int ni = 0; ni < fhCount; ni++) {
                int j = fhNear[ni];
                Enemy* e = &enemies[j];
                float edx = e->x - player.x;
                float edy = e->y - player.y;
                float d2 = edx * edx + edy * edy;
                if (d2 < radius[lv] * radius[lv] && d2 > 1.0f) {
                    float inv_d = fast_inv_sqrt(d2);
                    float pushX = edx * inv_d * pushDist[lv];
                    float pushY = edy * inv_d * pushDist[lv];
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
