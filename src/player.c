#include "game.h"

void player_init(void)
{
    game_reset_stats();
}

void player_update(void)
{
    if (player.dead) return;

    PDButtons current, pushed, released;
    pd->system->getButtonState(&current, &pushed, &released);

    // D-pad movement
    float dx = 0, dy = 0;
    if (current & kButtonLeft)  dx -= 1.0f;
    if (current & kButtonRight) dx += 1.0f;
    if (current & kButtonUp)    dy -= 1.0f;
    if (current & kButtonDown)  dy += 1.0f;

    // Diagonal normalization
    if (dx != 0 && dy != 0) {
        float len = 1.41421356f; // sqrt(2)
        dx /= len;
        dy /= len;
    }

    // Crank input
    if (!pd->system->isCrankDocked()) {
        float crankAngle = pd->system->getCrankAngle(); // 0=up, 90=right
        float crankRad = crankAngle * PI_F / 180.0f;
        float crankDx = sinf(crankRad);
        float crankDy = -cosf(crankRad);
        if (dx == 0 && dy == 0) {
            dx = crankDx;
            dy = crankDy;
        }
    }

    float spd = player.moveSpeed;
    if (dx != 0 || dy != 0) {
        // Walk animation
        player.animFrame++;
        if (player.animFrame % 8 == 0) {
            player.frameIdx = (player.frameIdx + 1) % 4;
        }

        float newX = player.x + dx * spd;
        float newY = player.y + dy * spd;

        // Wall clamping
        int s = game.arenaShrink;
        newX = clampf(newX, (float)(s + 24), (float)(MAP_W - s - 24));
        newY = clampf(newY, (float)(s + 24), (float)(MAP_H - s - 24));

        player.x = newX;
        player.y = newY;
    } else {
        if (player.frameIdx != 0) {
            player.frameIdx = 0;
        }
        player.animFrame = 0;
    }

    // Invulnerability blink
    uint32_t now = pd->system->getCurrentTimeMilliseconds();
    if (now < player.invulnUntil) {
        // blink handled in rendering
    }

    // Auto-aim: cache nearest enemy every 3 frames
    // Auto-aim: cache nearest enemy every 3 frames
    if (game.frameCount - game.targetCacheFrame >= 3) {
        game.cachedTargetIdx = -1;
        float nearDist = 1e18f;
        for (int i = 0; i < enemyCount; i++) {
            if (!enemies[i].alive) continue;
            float d = dist_sq(player.x, player.y, enemies[i].x, enemies[i].y);
            if (d < nearDist) {
                nearDist = d;
                game.cachedTargetIdx = i;
            }
        }
        game.targetCacheFrame = game.frameCount;
    }

    // Update aim direction
    int ti = game.cachedTargetIdx;
    if (ti >= 0 && ti < enemyCount && enemies[ti].alive) {
        float tdx = enemies[ti].x - player.x;
        float tdy = enemies[ti].y - player.y;
        float dist = sqrtf(tdx * tdx + tdy * tdy);
        if (dist > 0.001f) {
            player.aimDx = tdx / dist;
            player.aimDy = tdy / dist;
        }
    } else {
        game.cachedTargetIdx = -1;
    }

    // Weapon firing
    weapons_fire_all();

    // Cursed timer decay
    if (player.cursedTimer > 0) player.cursedTimer--;
}

void player_take_damage(void)
{
    uint32_t now = pd->system->getCurrentTimeMilliseconds();
    if (now < player.invulnUntil) return;

    // Salt Ward: absorb hit with shield
    if (player.saltWardShield > 0) {
        player.saltWardShield--;
        player.saltWardRegenCD = 150; // 5s cooldown before regen
        player.invulnUntil = now + (INVULN_FRAMES * FRAME_MS);
        game_trigger_shake(1);
        sound_play_hit();
        game.invertTimer = 2;
        entities_spawn_fx(player.x, player.y, 1, 1);
        return; // no HP damage
    }

    int dmg = player.cursedTimer > 0 ? 2 : 1;
    // Cursed Lantern: take 1.5x damage
    if (game.activeRelic == RELIC_CURSED_LANTERN) {
        dmg = (int)(dmg * 1.5f + 0.5f);
        if (dmg < 1) dmg = 1;
    }
    // Fathom Debt: taking damage charges next shot
    if (game.activeRelic == RELIC_FATHOM_DEBT) {
        game.relicFathomCharged = 1;
    }
    player.hp -= dmg;
    player.invulnUntil = now + (INVULN_FRAMES * FRAME_MS);
    game_trigger_shake(2);
    sound_play_hit();
    game.invertTimer = 3; // hit feedback flash

    // Low HP warning: slow-mo when dropping to 1 HP
    if (player.hp == 1) {
        game.slowMotionTimer = 20;
        snprintf(game.announceText, sizeof(game.announceText), "LAST STAND!");
        game.announceTimer = 30;
    }

    if (player.hp <= 0) {
        player.dead = 1;
        entities_spawn_particles(player.x, player.y, 15, 1);
        game_trigger_shake(8);
        game.invertTimer = 5;
        sound_play_gameover();
        save_update_high_score();
        game.gameOverSelection = 0;
        game_death_cutscene();
    }
}
