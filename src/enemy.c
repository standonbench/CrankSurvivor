#include "game.h"

// ---------------------------------------------------------------------------
// Update all enemies
// ---------------------------------------------------------------------------
void enemy_update_all(void)
{
    // Build spatial grid for this frame
    collision_clear();
    for (int i = 0; i < enemyCount; i++) {
        if (enemies[i].alive) {
            collision_insert_enemy(i, enemies[i].x, enemies[i].y);
        }
    }

    for (int i = 0; i < enemyCount; i++) {
        Enemy* e = &enemies[i];
        if (!e->alive || player.dead) continue;

        // Animation timers
        e->bobTimer++;
        e->animFrame++;

        // Stun
        if (e->stunTimer > 0) { e->stunTimer--; continue; }

        // Flash decay
        if (e->flashTimer > 0) e->flashTimer--;

        // Slow decay
        if (e->slowTimer > 0) {
            e->slowTimer--;
            if (e->slowTimer <= 0) e->slowFactor = 1.0f;
        }

        // Direction to player
        float dx = player.x - e->x;
        float dy = player.y - e->y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.001f) { dx /= dist; dy /= dist; }

        float slowMo = (game.slowMotionTimer > 0) ? 0.5f : 1.0f;
        float moveX = dx * e->speed * e->slowFactor * slowMo;
        float moveY = dy * e->speed * e->slowFactor * slowMo;

        // Type-specific AI
        switch (e->type) {
        case ENEMY_CREEPER:
            if (game.frameCount % 10 == 0) {
                moveX += (rng_float() - 0.5f) * 0.8f;
                moveY += (rng_float() - 0.5f) * 0.8f;
            }
            break;

        case ENEMY_TENDRIL:
        {
            float wobble = sinf(game.gameTime * 0.1f + e->phase) * 0.5f;
            moveX += (-dy) * wobble;
            moveY += dx * wobble;
            break;
        }

        case ENEMY_WRAITH:
            moveX += sinf(game.gameTime * 0.08f + e->phase) * 0.4f;
            moveY += cosf(game.gameTime * 0.06f + e->phase * 1.3f) * 0.3f;
            break;

        case ENEMY_ABYSSAL:
            e->lungeTimer++;
            if (e->lunging > 0) {
                e->lunging--;
                moveX *= 2.0f;
                moveY *= 2.0f;
            } else if (e->lungeTimer >= 90 && dist < 75.0f) {
                e->lungeTimer = 0;
                e->lunging = 3;
            }
            break;

        case ENEMY_SEER:
            if (dist < 150.0f) {
                moveX = (-dy) * e->speed * 0.3f * e->slowFactor;
                moveY = dx * e->speed * 0.3f * e->slowFactor;
            }
            e->shotCooldown++;
            if (e->shotCooldown >= 50) e->charging = 1;
            if (e->shotCooldown >= 60) {
                e->shotCooldown = 0;
                e->charging = 0;
                entities_spawn_enemy_bullet(e->x, e->y, dx, dy);
            }
            break;

        case ENEMY_LAMPREY:
            // Fast pursuit, no special movement
            break;

        case ENEMY_BLOAT:
            if (dist < 90.0f && (e->bobTimer / 4) % 2 == 0) {
                e->flashTimer = 1;
            }
            break;

        case ENEMY_HARBINGER:
            if (dist < 90.0f) {
                moveX = -dx * e->speed * 0.6f * e->slowFactor;
                moveY = -dy * e->speed * 0.6f * e->slowFactor;
            } else if (dist <= 150.0f) {
                moveX = (-dy) * e->speed * 0.3f * e->slowFactor;
                moveY = dx * e->speed * 0.3f * e->slowFactor;
            }
            // Buff aura every 4th frame
            if (game.frameCount % 4 == 0) {
                for (int j = 0; j < enemyCount; j++) {
                    if (j == i || !enemies[j].alive) continue;
                    if (dist_sq(e->x, e->y, enemies[j].x, enemies[j].y) < 8100.0f) {
                        if (enemies[j].slowFactor < 1.4f) enemies[j].slowFactor = 1.4f;
                        if (enemies[j].slowTimer < 4) enemies[j].slowTimer = 4;
                    }
                }
            }
            break;

        default:
            break;
        }

        // Soft separation: check 5 random neighbors every 3rd frame
        if (game.frameCount % 3 == 0 && enemyCount > 1) {
            int ne = enemyCount;
            int checkCount = ne - 1 < 5 ? ne - 1 : 5;
            int startJ = rng_range(0, ne - 1);
            for (int k = 0; k < checkCount; k++) {
                int j = (startJ + k) % ne;
                if (j == i || !enemies[j].alive) continue;
                float sdx = e->x - enemies[j].x;
                float sdy = e->y - enemies[j].y;
                float sdist = sdx * sdx + sdy * sdy;
                if (sdist < 900.0f && sdist > 0.0f) {
                    float d = sqrtf(sdist);
                    float push = (30.0f - d) * 0.15f;
                    moveX += (sdx / d) * push;
                    moveY += (sdy / d) * push;
                }
            }
        }

        // Apply movement
        float newX = e->x + moveX;
        float newY = e->y + moveY;

        // Wall clamping (wraiths pass through)
        if (e->type != ENEMY_WRAITH) {
            int s = game.arenaShrink;
            newX = clampf(newX, (float)(s + 3), (float)(MAP_W - s - 3));
            newY = clampf(newY, (float)(s + 3), (float)(MAP_H - s - 3));
        }

        e->x = newX;
        e->y = newY;

        // Contact damage with player
        if (!player.dead) {
            float pdx = e->x - player.x;
            float pdy = e->y - player.y;
            if (pdx * pdx + pdy * pdy < 1296.0f) {
                player_take_damage();
                if (player.barnacleArmor) {
                    enemy_damage(i, 1.0f);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Damage an enemy
// ---------------------------------------------------------------------------
void enemy_damage(int idx, float amount)
{
    ASSERT(idx >= 0 && idx < enemyCount);
    Enemy* e = &enemies[idx];
    if (!e->alive) return;

    // Tidecaller passive: +5% damage per stack
    if (player.tidecaller > 0) {
        amount *= (1.0f + 0.05f * player.tidecaller);
    }

    // Critical hit
    int crit = 0;
    int critChance = player.lighthouseLens ? 15 : 5;
    if (rng_range(1, 100) <= critChance) {
        amount *= 3.0f;
        crit = 1;
        entities_spawn_fx(e->x, e->y, 1, 1); // spark FX
        game_trigger_shake(1);
    }

    // Floating damage number
    rendering_spawn_dmg_number(e->x, e->y - 10.0f, (int)amount, crit);

    e->hp -= amount;
    if (e->hp <= 0) {
        e->alive = 0;
        game.score += 10;

        // Death particles
        int deathParticles = 5;
        if (e->type == ENEMY_CREEPER || e->type == ENEMY_LAMPREY) deathParticles = 6;
        else if (e->type == ENEMY_ABYSSAL) { deathParticles = 12; game_trigger_shake(2); }
        else if (e->type == ENEMY_SEER || e->type == ENEMY_HARBINGER) deathParticles = 10;
        else if (e->type == ENEMY_BLOAT) deathParticles = 10;
        else deathParticles = 6;

        entities_spawn_particles(e->x, e->y, deathParticles, 1);
        sound_play_kill();
        entities_spawn_fx(e->x, e->y, 0, 2); // death burst ring

        // Spawn XP
        entities_spawn_xp_gem(e->x, e->y, XP_PER_KILL);

        // Lamprey split
        if (e->type == ENEMY_LAMPREY && !e->isMini && enemyCount < MAX_ENEMIES - 1) {
            for (int m = 0; m < 2; m++) {
                float ox = e->x + (rng_float() - 0.5f) * 15.0f;
                float oy = e->y + (rng_float() - 0.5f) * 15.0f;
                int mi = entities_spawn_enemy(ox, oy, game_get_current_speed() * 1.5f, ENEMY_LAMPREY);
                if (mi >= 0) {
                    enemies[mi].isMini = 1;
                    enemies[mi].hp = 1.0f;
                }
            }
        }

        // Bloat explosion
        if (e->type == ENEMY_BLOAT) {
            sound_play_boom(50.0f, 0.6f, 0.2f);
            // Damage player if close
            if (!player.dead) {
                if (dist_sq(e->x, e->y, player.x, player.y) < 1600.0f) {
                    player_take_damage();
                }
            }
            // Damage nearby enemies
            for (int j = 0; j < enemyCount; j++) {
                if (j == idx || !enemies[j].alive) continue;
                if (dist_sq(e->x, e->y, enemies[j].x, enemies[j].y) < 2500.0f) {
                    enemy_damage(j, 3.0f);
                }
            }
        }

        // Kill streak tracking
        game.streakKills++;
        game.streakWindow = 30;
        if (game.streakKills >= 5 && game.streakMultiplier <= 1.0f) {
            game.streakMultiplier = 2.0f;
            game.streakTimer = 90;
        }

        // Kill streak visual milestones
        if (game.streakKills == 10) {
            game.streakFlashTimer = 2;
            sound_play_streak();
        } else if (game.streakKills == 25) {
            game.invertTimer = 3;
            snprintf(game.announceText, sizeof(game.announceText), "MASSACRE!");
            game.announceTimer = 45;
            sound_play_streak();
        } else if (game.streakKills == 50) {
            game_trigger_shake(6);
            snprintf(game.announceText, sizeof(game.announceText), "ANNIHILATION!");
            game.announceTimer = 60;
            sound_play_streak();
        }

        // Kill milestone announcements
        int kills = game.score / 10;
        if (kills == 50 || kills == 100 || kills == 200 || kills == 500 || kills == 1000) {
            snprintf(game.announceText, sizeof(game.announceText), "%d KILLS!", kills);
            game.announceTimer = 30;
        }

        // Multi-kill shake
        game.recentKills += 4;
        if (game.recentKills >= 12) {
            game_trigger_shake(3);
        }
    } else {
        e->flashTimer = 2;
        entities_spawn_fx(e->x, e->y, 1, 2); // spark FX
    }
}
