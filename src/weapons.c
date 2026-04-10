#include "game.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Weapon stat tables
// ---------------------------------------------------------------------------
static const int cooldownTable[WEAPON_COUNT][3] = {
    { 500,  400,  300  }, // Signal Beam (faster fire + auto-track)
    {   0,    0,    0  }, // Tide Pool (always active)
    { 1800, 1400, 1000 }, // Harpoon (was 800)
    {    0,    0,    0  }, // Brine Splash (always active)
    { 1500,  900,  600 }, // Ghost Light (was 1800/1100)
    { 3000, 2200, 1500 }, // Anchor Drop (was 4000/2800/1800)
    { 4500, 3500, 2800 }, // Foghorn (reduced L1/L2 for stun utility)
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
        "Holy Aura", "Spirit Wisp", "Binding Rune", "Banishing Cry",
        "Chain Bolt", "Undertow", "Abyssal Mine"
    };
    static const char* evolvedNames[] = {
        "Prismatic Flare", "Maelstrom", "Leviathan Spine",
        NULL, "Wisp Swarm", NULL, "Siren's Wail",
        "Storm Cage", NULL, "Kraken's Clutch"
    };
    if (id < 0 || id >= WEAPON_COUNT) return "Unknown";
    // Check if this weapon is evolved in the player's loadout
    for (int i = 0; i < player.weaponCount; i++) {
        if (player.weapons[i].id == id && player.weapons[i].evolved && evolvedNames[id]) {
            return evolvedNames[id];
        }
    }
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
    // Arsenal Power: Foghorn + 3+ weapons → cry cooldown -25%
    game.synergyCryCooldown = (hasWeapon[WEAPON_FOGHORN] && player.weaponCount >= 3) ? 0.75f : 1.0f;
    // Static Charge: Chain Lightning + Tide Pool → orb chain chance
    game.synergyStaticCharge = (hasWeapon[WEAPON_CHAIN_LIGHTNING] && hasWeapon[WEAPON_TIDE_POOL]) ? 1.0f : 0.0f;
    // Maelstrom: Riptide + Brine Splash → Brine +20% dmg in vortex
    game.synergyMaelstrom = (hasWeapon[WEAPON_RIPTIDE] && hasWeapon[WEAPON_BRINE_SPLASH]) ? 1.2f : 1.0f;
    // Depth Hunter: Depth Charge + Harpoon → harpoon kills drop mini-mines
    game.synergyDepthHunter = (hasWeapon[WEAPON_DEPTH_CHARGE] && hasWeapon[WEAPON_HARPOON]) ? 1.0f : 0.0f;

    // Synergy activation notification
    int synergyCount = (game.synergyBurstRadius > 1.0f) + (game.synergyBeamDmg > 0) +
                       (game.synergyRuneSlow < 1.0f) + (game.synergyCryCooldown < 1.0f) +
                       (game.synergyStaticCharge > 0) + (game.synergyMaelstrom > 1.0f) +
                       (game.synergyDepthHunter > 0);
    if (synergyCount > game.lastActiveSynergyCount && game.lastActiveSynergyCount >= 0) {
        snprintf(game.announceText, sizeof(game.announceText), "SYNERGY!");
        game.announceTimer = 45;
        sound_play_confirm();
    }
    game.lastActiveSynergyCount = synergyCount;
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
    int evolved = player.weapons[tpIdx].evolved;

    int orbCnt;
    float rad, dmgVal, spd;

    if (evolved) {
        // Maelstrom: more orbs, doubled radius, higher damage, pulls enemies
        orbCnt = 5;
        rad = 120.0f;
        dmgVal = 2.5f;
        spd = 0.18f;
    } else {
        int orbCount[] = { 2, 3, 3 };
        float radius[] = { 40.0f, 52.0f, 60.0f };
        float dmg[] = { 1.0f, 1.0f, 1.5f };
        float speed[] = { 0.10f, 0.13f, 0.16f };
        int li = lv - 1;
        orbCnt = orbCount[li];
        rad = radius[li];
        dmgVal = dmg[li];
        spd = speed[li];
    }

    // Tidebreaker: aura weapons 1.5x radius
    if (game.activeRelic == RELIC_TIDEBREAKER) {
        rad *= 1.5f;
    }

    game.tidePoolAngle += spd;

    float px = player.x;
    float py = player.y;
    float twoPi = 2.0f * PI_F;

    // Maelstrom: pull enemies toward orbit center
    if (evolved) {
        float pullRadius = rad + 20.0f;
        int pullNear[64];
        int pullCount = collision_query_point(px, py, pullRadius, pullNear, 64);
        float pr2 = pullRadius * pullRadius;
        for (int ni = 0; ni < pullCount; ni++) {
            int i = pullNear[ni];
            Enemy* e = &enemies[i];
            float edx = e->x - px;
            float edy = e->y - py;
            float d2 = edx * edx + edy * edy;
            if (d2 < pr2 && d2 > 100.0f) {
                float inv_d = fast_inv_sqrt(d2);
                float pullStr = 1.2f;
                int s = game.arenaShrink;
                e->x = clampf(e->x - edx * inv_d * pullStr, (float)(s + 5), (float)(MAP_W - s - 5));
                e->y = clampf(e->y - edy * inv_d * pullStr, (float)(s + 5), (float)(MAP_H - s - 5));
            }
        }
    }

    for (int o = 0; o < orbCnt; o++) {
        float a = game.tidePoolAngle + o * (twoPi / orbCnt);
        float ox = px + cosf(a) * rad;
        float oy = py + sinf(a) * rad;

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
                    enemy_damage(i, dmgVal);
                    // Static Charge synergy: 12% chain on orb hit
                    if (game.synergyStaticCharge > 0 && enemies[i].alive && rng_range(1, 100) <= 12) {
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
    float auraRadMult = (game.activeRelic == RELIC_TIDEBREAKER) ? 1.5f : 1.0f;
    float r = radius[lv - 1] * game.synergyBurstRadius * auraRadMult;

    // Visual ring stays permanently on the player
    game.brineSplash.active = 1;
    game.brineSplash.x = player.x;
    game.brineSplash.y = player.y;
    game.brineSplash.maxRadius = r;
    game.brineSplash.radius = r + (sinf(game.frameCount * 0.1f) * 2.0f);

    // Tick damage every 15 frames (0.5s) via spatial grid
    if (game.frameCount % 15 == 0) {
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
                // Slow at all levels (10% at L1-2, 50% at L3)
                if (enemies[j].alive) {
                    enemies[j].slowTimer = 15;
                    enemies[j].slowFactor = (lv >= 3) ? 0.5f : 0.9f;
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

        // Visual arc (Storm Cage evolved = persistent fences)
        int clEvo = 0;
        for (int wi = 0; wi < player.weaponCount; wi++) {
            if (player.weapons[wi].id == WEAPON_CHAIN_LIGHTNING && player.weapons[wi].evolved)
                { clEvo = 1; break; }
        }
        ChainVisual* cv = &chainVisuals[chainVisualIdx];
        cv->x1 = cx;
        cv->y1 = cy;
        cv->x2 = enemies[bestIdx].x;
        cv->y2 = enemies[bestIdx].y;
        cv->life = clEvo ? 15 : 3;
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
            // Check if depth charge weapon is evolved (Kraken's Clutch)
            int dcEvo = 0;
            for (int wi = 0; wi < player.weaponCount; wi++) {
                if (player.weapons[wi].id == WEAPON_DEPTH_CHARGE && player.weapons[wi].evolved) {
                    dcEvo = 1; break;
                }
            }

            // Kraken's Clutch: root nearby enemies during fuse phase
            if (dcEvo && dc->fuseFrames > 1) {
                int grabNear[32];
                int grabCount = collision_query_point(dc->x, dc->y, dc->blastRadius * 0.8f, grabNear, 32);
                float gr2 = (dc->blastRadius * 0.8f) * (dc->blastRadius * 0.8f);
                for (int ni = 0; ni < grabCount; ni++) {
                    int i = grabNear[ni];
                    Enemy* e = &enemies[i];
                    float dx = e->x - dc->x;
                    float dy = e->y - dc->y;
                    if (dx * dx + dy * dy < gr2) {
                        e->stunTimer = 3; // refresh root each frame
                    }
                }
            }

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

                // Boss damage from detonation
                if (game.bossActive && game.boss.alive) {
                    float bdx = game.boss.x - dc->x;
                    float bdy = game.boss.y - dc->y;
                    if (bdx * bdx + bdy * bdy < r2) {
                        boss_damage(dc->dmg);
                    }
                }

                game_trigger_shake(4);
                entities_spawn_particles(dc->x, dc->y, 10, 1);
                sound_play_boom(60.0f, 0.7f, 0.2f);
                entities_spawn_fx(dc->x, dc->y, 0, 1); // blast ring

                // Kraken's Clutch: chain-detonate nearby mines
                if (dcEvo) {
                    for (int dj = 0; dj < depthChargeCount; dj++) {
                        if (dj == di || !depthCharges[dj].alive || depthCharges[dj].detonated) continue;
                        float cdx = depthCharges[dj].x - dc->x;
                        float cdy = depthCharges[dj].y - dc->y;
                        if (cdx * cdx + cdy * cdy < 80.0f * 80.0f) {
                            depthCharges[dj].fuseFrames = 2; // detonate next frame
                        }
                    }
                }

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

        // Drowned Bell: no cooldowns for first 30s
        int bellActive = (game.activeRelic == RELIC_DROWNED_BELL && game.relicBellTimer > 0.0f);
        if (!bellActive && now - w->lastFiredMs < (uint32_t)cd) continue;
        w->lastFiredMs = now;

        switch (w->id) {
        case WEAPON_SIGNAL_BEAM:
        {
            // Auto-track: nudge aim toward nearest enemy within 30°
            float aimDx = player.aimDx, aimDy = player.aimDy;
            if (game.cachedTargetIdx >= 0 && game.cachedTargetIdx < enemyCount && enemies[game.cachedTargetIdx].alive) {
                float tdx = enemies[game.cachedTargetIdx].x - player.x;
                float tdy = enemies[game.cachedTargetIdx].y - player.y;
                float td = sqrtf(tdx * tdx + tdy * tdy);
                if (td > 1.0f) {
                    tdx /= td; tdy /= td;
                    // Check angle between aim and target (dot product)
                    float dot = aimDx * tdx + aimDy * tdy;
                    if (dot > 0.866f) { // within ~30°
                        aimDx = tdx; aimDy = tdy;
                    }
                }
            }

            if (w->evolved) {
                // Prismatic Flare: 3 wide piercing beams + burning ground on hit
                float dmg = 3.0f + game.synergyBeamDmg;
                float bulletSpd = BULLET_SPEED * 1.2f;
                if (player.seaLegs >= 2) bulletSpd *= 1.15f;
                float spreadRad = 18.0f * PI_F / 180.0f;
                for (int b = -1; b <= 1; b++) {
                    float angle = atan2f(aimDy, aimDx) + b * spreadRad;
                    entities_spawn_bullet(player.x, player.y,
                        cosf(angle), sinf(angle),
                        dmg, bulletSpd, BULLET_LIFETIME_F + 5, 255, 0, 0, 0, 0);
                }
                sound_play_weapon(500.0f);
                entities_spawn_fx(player.x, player.y, 1, 1);
                game_trigger_shake(1);
            } else {
                float dmg = 1.0f + game.synergyBeamDmg;
                float bulletSpd = BULLET_SPEED;
                if (player.seaLegs >= 2) bulletSpd *= 1.15f;
                uint8_t pierce = player.lighthouseLens ? 255 : 0;
                int life = BULLET_LIFETIME_F;

                if (w->level == 1) {
                    entities_spawn_bullet(player.x, player.y,
                        aimDx, aimDy,
                        dmg, bulletSpd, life, pierce, 0, 0, 0, 0);
                } else if (w->level == 2) {
                    float spreadRad = 12.0f * PI_F / 180.0f;
                    for (int b = -1; b <= 1; b++) {
                        float angle = atan2f(aimDy, aimDx) + b * spreadRad;
                        entities_spawn_bullet(player.x, player.y,
                            cosf(angle), sinf(angle),
                            dmg, bulletSpd, life, pierce, 0, 0, 0, 0);
                    }
                } else {
                    dmg = 2.0f + game.synergyBeamDmg;
                    float spreadRad = 10.0f * PI_F / 180.0f;
                    for (int b = -2; b <= 2; b++) {
                        float angle = atan2f(aimDy, aimDx) + b * spreadRad;
                        entities_spawn_bullet(player.x, player.y,
                            cosf(angle), sinf(angle),
                            dmg, bulletSpd, life, 255, 0, 0, 0, 0);
                    }
                }
                sound_play_weapon(400.0f);
                entities_spawn_fx(player.x, player.y, 1, 1);
            }
            break;
        }

        case WEAPON_HARPOON:
        {
            if (w->evolved) {
                // Leviathan Spine: single massive piercing bolt, roots + AoE
                float dmg = 12.0f;
                float bulletSpd = 5.5f;
                if (player.seaLegs >= 2) bulletSpd *= 1.15f;
                int life = (int)(1000.0f / FRAME_MS);
                entities_spawn_bullet(player.x, player.y,
                    player.aimDx, player.aimDy,
                    dmg, bulletSpd, life, 255, 0, 0, 0, 1);
                game_trigger_shake(3);
                entities_spawn_particles(player.x, player.y, 5, 1);
                sound_play_weapon(250.0f);
                entities_spawn_fx(player.x, player.y, 1, 1);
            } else {
                float hDmg[] = { 3.0f, 5.0f, 8.0f };
                float hSpd[] = { 4.0f, 5.0f, 5.0f };
                int lv = w->level - 1;
                float bulletSpd = hSpd[lv];
                if (player.seaLegs >= 2) bulletSpd *= 1.15f;
                int life = (int)(800.0f / FRAME_MS);

                // Bone Compass: pierce 3 targets; normal: infinite pierce
                uint8_t harpPierce = (game.activeRelic == RELIC_BONE_COMPASS) ? 3 : 255;

                game_trigger_shake(2);
                entities_spawn_particles(player.x, player.y, 3, 1);

                if (w->level < 3) {
                    entities_spawn_bullet(player.x, player.y,
                        player.aimDx, player.aimDy,
                        hDmg[lv], bulletSpd, life, harpPierce, 0, 0, 0, 1);
                } else {
                    float angle = atan2f(player.aimDy, player.aimDx);
                    float offset = 8.0f * PI_F / 180.0f;
                    entities_spawn_bullet(player.x, player.y,
                        cosf(angle - offset), sinf(angle - offset),
                        hDmg[lv], bulletSpd, life, harpPierce, 0, 0, 0, 1);
                    entities_spawn_bullet(player.x, player.y,
                        cosf(angle + offset), sinf(angle + offset),
                        hDmg[lv], bulletSpd, life, harpPierce, 0, 0, 0, 1);
                }
                sound_play_weapon(300.0f);
                entities_spawn_fx(player.x, player.y, 1, 1);
            }
            break;
        }

        case WEAPON_BRINE_SPLASH:
        {
            // Now handled passively in weapons_update_brine_splash
            break;
        }

        case WEAPON_GHOST_LIGHT:
        {
            if (w->evolved) {
                // Wisp Swarm: 5 fast homing wisps, retarget, higher damage
                float dmg = 2.0f;
                float bulletSpd = 2.5f;
                if (player.seaLegs >= 2) bulletSpd *= 1.15f;
                int life = (int)(2500.0f / FRAME_MS);
                float turn = 30.0f * PI_F / 180.0f;
                float angle = atan2f(player.aimDy, player.aimDx);
                for (int wi = 0; wi < 5; wi++) {
                    float off = (wi - 2) * 0.4f;
                    entities_spawn_bullet(player.x, player.y,
                        cosf(angle + off), sinf(angle + off),
                        dmg, bulletSpd, life, 0, 1, turn, 1, 2);
                }
                sound_play_weapon(650.0f);
                entities_spawn_fx(player.x, player.y, 1, 1);
            } else {
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
                entities_spawn_fx(player.x, player.y, 1, 1);
            }
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
            float fhRadius[] = { 90.0f, 120.0f, 150.0f };
            float pushDist[] = { 45.0f, 60.0f, 90.0f };
            float fhDmg[] = { 1.0f, 2.0f, 3.0f };
            int evolved = w->evolved;

            float rad = evolved ? 170.0f : fhRadius[lv];
            float push = evolved ? 70.0f : pushDist[lv];
            float dmgVal = evolved ? 4.0f : fhDmg[lv];
            // Tidebreaker: aura weapons 1.5x radius
            if (game.activeRelic == RELIC_TIDEBREAKER) rad *= 1.5f;

            game_trigger_shake(5);
            entities_spawn_particles(player.x, player.y, 12, 1);

            // Use spatial grid for foghorn blast
            int fhNear[64];
            int fhCount = collision_query_point(player.x, player.y, rad, fhNear, 64);
            for (int ni = 0; ni < fhCount; ni++) {
                int j = fhNear[ni];
                Enemy* e = &enemies[j];
                float edx = e->x - player.x;
                float edy = e->y - player.y;
                float d2 = edx * edx + edy * edy;
                if (d2 < rad * rad && d2 > 1.0f) {
                    float inv_d = fast_inv_sqrt(d2);
                    // Siren's Wail: PULL instead of push
                    float dir = evolved ? -1.0f : 1.0f;
                    float pushX = edx * inv_d * push * dir;
                    float pushY = edy * inv_d * push * dir;
                    int s = game.arenaShrink;
                    e->x = clampf(e->x + pushX, (float)(s + 5), (float)(MAP_W - s - 5));
                    e->y = clampf(e->y + pushY, (float)(s + 5), (float)(MAP_H - s - 5));
                    enemy_damage(j, dmgVal);
                    // Stun at all levels: 9f (L1), 15f (L2), 20f (L3), 60f evolved
                    if (enemies[j].alive) {
                        int stunFrames[] = { 9, 15, 20 };
                        enemies[j].stunTimer = evolved ? 60 : stunFrames[lv];
                    }
                }
            }

            // Boss damage from foghorn
            if (game.bossActive && game.boss.alive) {
                float bdx = game.boss.x - player.x;
                float bdy = game.boss.y - player.y;
                if (bdx * bdx + bdy * bdy < rad * rad) {
                    boss_damage(dmgVal * 2.0f);
                }
            }

            // Visual ring
            game.foghornVisual.active = 1;
            game.foghornVisual.x = player.x;
            game.foghornVisual.y = player.y;
            game.foghornVisual.maxRadius = rad;
            game.foghornVisual.radius = 10.0f;
            game.foghornVisual.frame = 0;

            sound_play_boom(evolved ? 60.0f : 80.0f, 0.5f, 0.15f);
            break;
        }

        case WEAPON_CHAIN_LIGHTNING:
        {
            int lv = w->level - 1;
            float clDmg[] = { 1.0f, 2.0f, 2.0f };
            int chains[] = { 3, 5, 7 };
            int stunPct = (w->level >= 3) ? 20 : 0;
            int evolved = w->evolved;

            // Storm Cage: more chains, longer visual, higher damage, guaranteed stun
            float dmgVal = evolved ? 3.0f : clDmg[lv];
            int chainCount = evolved ? 10 : chains[lv];
            int evStunPct = evolved ? 40 : stunPct;
            int chainLife = evolved ? 15 : 3; // persistent fences

            // Find nearest enemy
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
                ChainVisual* cv = &chainVisuals[chainVisualIdx];
                cv->x1 = player.x;
                cv->y1 = player.y;
                cv->x2 = enemies[bestIdx].x;
                cv->y2 = enemies[bestIdx].y;
                cv->life = chainLife;
                chainVisualIdx = (chainVisualIdx + 1) % MAX_CHAIN_VISUALS;

                enemy_damage(bestIdx, dmgVal);
                entities_spawn_particles(enemies[bestIdx].x, enemies[bestIdx].y, 3, 0);

                if (evStunPct > 0 && enemies[bestIdx].alive && rng_range(1, 100) <= evStunPct) {
                    enemies[bestIdx].stunTimer = evolved ? 30 : 10;
                }

                if (enemies[bestIdx].alive) {
                    weapons_fire_chain_at(enemies[bestIdx].x, enemies[bestIdx].y, dmgVal, chainCount - 1, evStunPct, bestIdx);
                }

                sound_play_weapon(evolved ? 900.0f : 800.0f);
                game_trigger_shake(evolved ? 2 : 1);
                entities_spawn_fx(player.x, player.y, 1, 1);
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
            if (w->evolved) {
                // Kraken's Clutch: 5 mines in a ring, root enemies, chain-detonate
                float dmgVal = 7.0f;
                float blastR = 55.0f;
                float baseAngle = rng_float() * PI_F * 2.0f;
                float spawnDist = 55.0f;
                for (int m = 0; m < 5; m++) {
                    float angle = baseAngle + m * (2.0f * PI_F / 5.0f);
                    float mx = player.x + cosf(angle) * spawnDist;
                    float my = player.y + sinf(angle) * spawnDist;
                    int s = game.arenaShrink;
                    mx = clampf(mx, (float)(s + 10), (float)(MAP_W - s - 10));
                    my = clampf(my, (float)(s + 10), (float)(MAP_H - s - 10));
                    spawn_depth_charge(mx, my, dmgVal, blastR, 0.5f, 45);
                }
                sound_play_weapon(220.0f);
                game_trigger_shake(2);
                entities_spawn_particles(player.x, player.y, 6, 1);
            } else {
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
            }
            break;
        }

        default:
            break;
        }
    }
}
