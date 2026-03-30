#include "game.h"

// ---------------------------------------------------------------------------
// Find nearest enemy to a point
// ---------------------------------------------------------------------------
static int find_nearest_enemy(float x, float y)
{
    int nearest = -1;
    float nearDist = 1e18f;
    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].alive) continue;
        float d = dist_sq(x, y, enemies[i].x, enemies[i].y);
        if (d < nearDist) {
            nearDist = d;
            nearest = i;
        }
    }
    return nearest;
}

// ---------------------------------------------------------------------------
// Update all player bullets
// ---------------------------------------------------------------------------
void bullets_update_all(void)
{
    for (int i = 0; i < bulletCount; i++) {
        Bullet* b = &bullets[i];
        if (!b->alive) continue;

        b->lifeFrames--;
        if (b->lifeFrames <= 0) {
            b->alive = 0;
            continue;
        }

        // Homing logic
        if (b->homing) {
            // Enhanced sparkle trail
            if (game.frameCount % 2 == 0) {
                entities_spawn_particles(b->x, b->y, 1, 1);
            }
            // Acquire/reacquire target
            if (b->homingTarget < 0 || b->homingTarget >= enemyCount ||
                !enemies[b->homingTarget].alive) {
                b->homingTarget = (int16_t)find_nearest_enemy(b->x, b->y);
            }
            // Steer toward target
            if (b->homingTarget >= 0 && enemies[b->homingTarget].alive) {
                float tdx = enemies[b->homingTarget].x - b->x;
                float tdy = enemies[b->homingTarget].y - b->y;
                float targetAngle = atan2f(tdy, tdx);
                float currentAngle = atan2f(b->vy, b->vx);

                float diff = targetAngle - currentAngle;
                while (diff > PI_F) diff -= 2.0f * PI_F;
                while (diff < -PI_F) diff += 2.0f * PI_F;

                float maxTurn = b->turnRate;
                if (diff > maxTurn) diff = maxTurn;
                else if (diff < -maxTurn) diff = -maxTurn;

                float newAngle = currentAngle + diff;
                b->vx = cosf(newAngle) * b->speed;
                b->vy = sinf(newAngle) * b->speed;
            }
        }

        // Move
        b->x += b->vx;
        b->y += b->vy;

        // Check enemy hits via spatial grid
        int hitIndices[8];
        int hitCount = collision_query_point(b->x, b->y, 30.0f, hitIndices, 8);
        for (int h = 0; h < hitCount; h++) {
            int ei = hitIndices[h];
            if (enemies[ei].alive) {
                enemy_damage(ei, b->dmg);
                // Harpoon Knockback (Heaviness)
                if (b->imageId == 1) {
                    float vlen = sqrtf(b->vx * b->vx + b->vy * b->vy);
                    if (vlen > 0.01f) {
                        enemies[ei].x += (b->vx / vlen) * 12.0f;
                        enemies[ei].y += (b->vy / vlen) * 12.0f;
                    }
                }
                if (!b->piercing) {
                    b->alive = 0;
                    break;
                }
                if (b->retargets && !enemies[ei].alive) {
                    b->homingTarget = -1;
                }
            }
        }

        // Out of bounds
        if (b->x < -10 || b->x > MAP_W + 10 || b->y < -10 || b->y > MAP_H + 10) {
            b->alive = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Update all enemy bullets
// ---------------------------------------------------------------------------
void enemy_bullets_update_all(void)
{
    for (int i = 0; i < enemyBulletCount; i++) {
        EnemyBullet* b = &enemyBullets[i];
        if (!b->alive) continue;

        b->lifeFrames--;
        if (b->lifeFrames <= 0) {
            b->alive = 0;
            continue;
        }

        b->x += b->vx;
        b->y += b->vy;

        // Hit player
        if (!player.dead) {
            float pdx = b->x - player.x;
            float pdy = b->y - player.y;
            if (pdx * pdx + pdy * pdy < 576.0f) {
                player_take_damage();
                b->alive = 0;
                continue;
            }
        }

        // Out of bounds
        if (b->x < -10 || b->x > MAP_W + 10 || b->y < -10 || b->y > MAP_H + 10) {
            b->alive = 0;
        }
    }
}
