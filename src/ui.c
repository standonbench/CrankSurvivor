#include "game.h"
#include <stdio.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Helper: draw centered text at y
// ---------------------------------------------------------------------------
void ui_draw_centered_text(const char* text, int y)
{
    int tw = pd->graphics->getTextWidth(NULL, text, strlen(text), kASCIIEncoding, 0);
    pd->graphics->drawText(text, strlen(text), kASCIIEncoding, (SCREEN_W - tw) / 2, y);
}

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
void ui_draw_title(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    // Twinkling stars (varied brightness and sizes)
    for (int i = 0; i < 40; i++) {
        uint32_t seed = (uint32_t)(i * 7919 + 13);
        int sx = (int)(seed % SCREEN_W);
        int sy = (int)((seed * 31) % 75);
        int rate = (i < 8) ? 8 : ((i < 20) ? 15 : 25);
        int blink = ((game.frameCount + i * 7) / rate) % 4;
        if (blink < 2) {
            int sz = (i < 5) ? 2 : 1;
            GFX_FILL(sx, sy, sz, sz, kColorWhite);
        }
    }

    // Title (large font)
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    ui_draw_centered_text("THE LAST", 30);
    ui_draw_centered_text("LIGHTHOUSE KEEPER", 52);
    pd->graphics->setFont(NULL);

    // Lighthouse silhouette — detailed architecture
    GFX_FILL(190, 100, 20, 38, kColorWhite);   // lower tower
    GFX_FILL(192, 90, 16, 12, kColorWhite);     // upper tower
    GFX_FILL(186, 86, 28, 5, kColorWhite);      // gallery walkway
    GFX_FILL(186, 84, 1, 2, kColorWhite);       // left railing post
    GFX_FILL(213, 84, 1, 2, kColorWhite);       // right railing post
    GFX_FILL(186, 84, 28, 1, kColorWhite);      // railing bar
    GFX_FILL(194, 78, 12, 8, kColorWhite);      // lantern room
    GFX_FILL(196, 76, 8, 3, kColorWhite);       // dome
    GFX_FILL(199, 74, 2, 3, kColorWhite);       // peak
    GFX_FILL(186, 138, 28, 4, kColorWhite);     // stone base
    // Windows + door (cut out in black)
    GFX_FILL(196, 105, 3, 4, kColorBlack);
    GFX_FILL(201, 105, 3, 4, kColorBlack);
    GFX_FILL(198, 120, 4, 6, kColorBlack);
    GFX_FILL(197, 131, 6, 7, kColorBlack);      // door
    // Horizontal stone banding (dithered)
    for (int stripe = 95; stripe < 138; stripe += 6) {
        for (int x = 191; x < 209; x += 2) {
            GFX_FILL(x, stripe, 1, 1, kColorBlack);
        }
    }

    // Rotating light beam (full 360°) with trailing glow
    {
        float angle = (float)game.frameCount * 0.02f;
        int beamLen = 70;
        // Trailing ghost beams (dithered)
        for (int trail = 2; trail >= 1; trail--) {
            float tA = (float)((int)game.frameCount - trail * 3) * 0.02f;
            float tCos = cosf(tA);
            float tSin = sinf(tA);
            int tLen = beamLen - trail * 15;
            for (int step = 4; step < tLen; step += 3) {
                float frac = (float)step / (float)tLen;
                int px = 200 + (int)(tCos * frac * (float)tLen);
                int py = 82 + (int)(tSin * frac * (float)tLen);
                GFX_FILL(px, py, 1, 1, kColorWhite);
            }
        }
        // Primary beam (solid, 3 lines thick)
        float cosA = cosf(angle);
        float sinA = sinf(angle);
        for (int r = -1; r <= 1; r++) {
            int ex = 200 + (int)(cosA * (float)beamLen);
            int ey = 82 + (int)(sinA * (float)beamLen) + r * 2;
            GFX_LINE(200, 82 + r, ex, ey, 1, kColorWhite);
        }
    }

    // Horizon fog band (dithered haze between sky and water)
    for (int y = 130; y < 142; y++) {
        int density = (y < 136) ? 4 : 3;
        for (int x = 0; x < SCREEN_W; x += density) {
            if ((x + y + (int)game.frameCount / 8) % density == 0)
                GFX_FILL(x, y, 1, 1, kColorWhite);
        }
    }

    // Lighthouse reflection in water (dithered shimmer)
    for (int ry = 0; ry < 25; ry += 2) {
        int rw = 20 - ry * 2 / 3;
        if (rw < 2) break;
        int wobble = (int)(sinf((float)(ry * 3 + (int)game.frameCount) * 0.15f) * 3.0f);
        int rx = 200 - rw / 2 + wobble;
        for (int x = rx; x < rx + rw; x += 2) {
            GFX_FILL(x, 144 + ry, 1, 1, kColorWhite);
        }
    }

    // Dramatic layered ocean waves
    for (int x = 0; x < SCREEN_W; x += 6) {
        int wy1 = 141 + (int)(sinf((float)x * 0.03f + (float)game.frameCount * 0.08f) * 4.0f);
        GFX_FILL(x, wy1, 5, 1, kColorWhite);
    }
    for (int x = 0; x < SCREEN_W; x += 7) {
        int wy2 = 149 + (int)(sinf((float)x * 0.06f + (float)game.frameCount * 0.12f) * 2.5f);
        GFX_FILL(x + 2, wy2, 4, 1, kColorWhite);
    }
    for (int x = 0; x < SCREEN_W; x += 9) {
        int wy3 = 157 + (int)(sinf((float)x * 0.04f + (float)game.frameCount * 0.05f) * 2.0f);
        GFX_FILL(x + 1, wy3, 3, 1, kColorWhite);
    }
    // Foam spray at wave crests
    for (int i = 0; i < 8; i++) {
        uint32_t seed = (uint32_t)(i * 3137 + game.frameCount / 4);
        int fx = (int)(seed % (uint32_t)SCREEN_W);
        float waveY = sinf((float)fx * 0.03f + (float)game.frameCount * 0.08f);
        if (waveY > 2.0f) {
            int fy = 139 - (int)(waveY * 1.5f);
            GFX_FILL(fx, fy, 1, 1, kColorWhite);
            GFX_FILL(fx + 1, fy - 1, 1, 1, kColorWhite);
        }
    }

    // --- Menu layout ---
    // Row 0: "Start Game" (prominent boxed button)
    // Row 1: [Keeper]   [Armory]     (2-column grid)
    // Row 2: [Bestiary] [Logbook]    (2-column grid)
    if (game.fontBold) pd->graphics->setFont(game.fontBold);

    // Start Game button (wide, prominent)
    {
        int btnW = 150, btnH = 22;
        int bx = (SCREEN_W - btnW) / 2;
        int by = 154;
        const char* label = "Start Game";
        int tw = pd->graphics->getTextWidth(game.fontBold, label, strlen(label), kASCIIEncoding, 0);
        int textX = bx + (btnW - tw) / 2;
        int textY = by + 3;
        if (game.menuSelection == 1) {
            GFX_FILL(bx, by, btnW, btnH, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->drawText(label, strlen(label), kASCIIEncoding, textX, textY);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            GFX_FILL(bx - 10, by + btnH / 2 - 3, 4, 6, kColorWhite);
            GFX_FILL(bx + btnW + 6, by + btnH / 2 - 3, 4, 6, kColorWhite);
        } else {
            GFX_RECT(bx, by, btnW, btnH, kColorWhite);
            pd->graphics->drawText(label, strlen(label), kASCIIEncoding, textX, textY);
        }
    }

    // 2x2 grid of secondary options
    {
        const char* gridLabels[] = { "Keeper", "Armory", "Bestiary", "Logbook" };
        int gridMenuIds[] = { 2, 3, 4, 5 }; // menuSelection values
        int cellW = 90, cellH = 18;
        int gridX = (SCREEN_W - cellW * 2 - 10) / 2; // 10px gap between columns
        int gridY = 182;
        int rowGap = 22;
        int colGap = cellW + 10;

        for (int i = 0; i < 4; i++) {
            int col = i % 2;
            int row = i / 2;
            int cx = gridX + col * colGap;
            int cy = gridY + row * rowGap;
            const char* label = gridLabels[i];
            int tw = pd->graphics->getTextWidth(game.fontBold, label, strlen(label), kASCIIEncoding, 0);
            int textX = cx + (cellW - tw) / 2;
            int textY = cy + 1;

            if (game.menuSelection == gridMenuIds[i]) {
                GFX_FILL(cx, cy, cellW, cellH, kColorWhite);
                pd->graphics->setDrawMode(kDrawModeFillBlack);
                pd->graphics->drawText(label, strlen(label), kASCIIEncoding, textX, textY);
                pd->graphics->setDrawMode(kDrawModeFillWhite);
            } else {
                GFX_RECT(cx, cy, cellW, cellH, kColorWhite);
                pd->graphics->drawText(label, strlen(label), kASCIIEncoding, textX, textY);
            }
        }
    }

    if (game.fontBold) pd->graphics->setFont(NULL);

    // Salvage + Keeper info (top right, small font)
    {
        char buf[48];
        int ty = 4;
        if (game.highScore > 0) {
            snprintf(buf, sizeof(buf), "Best: %d", game.highScore);
            int bw = pd->graphics->getTextWidth(NULL, buf, strlen(buf), kASCIIEncoding, 0);
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, SCREEN_W - bw - 6, ty);
            ty += 12;
        }
        int salvage = save_get_salvage();
        if (salvage > 0 || game.totalRuns > 0) {
            snprintf(buf, sizeof(buf), "Salvage: %d", salvage);
            int sw = pd->graphics->getTextWidth(NULL, buf, strlen(buf), kASCIIEncoding, 0);
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, SCREEN_W - sw - 6, ty);
            ty += 12;
        }
        if (game.selectedKeeper != KEEPER_DEFAULT) {
            const KeeperDef* k = &KEEPER_DEFS[game.selectedKeeper];
            int kw = pd->graphics->getTextWidth(NULL, k->name, strlen(k->name), kASCIIEncoding, 0);
            pd->graphics->drawText(k->name, strlen(k->name), kASCIIEncoding, SCREEN_W - kw - 6, ty);
        }
    }

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
void ui_draw_hud(void)
{
    if (game.fontBold) pd->graphics->setFont(game.fontBold);

    int mins = (int)game.gameTime / 60;
    int secs = (int)game.gameTime % 60;
    char buf[64];
    int tw;

    // Top-left: timer
    snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
    tw = pd->graphics->getTextWidth(game.fontBold, buf, strlen(buf), kASCIIEncoding, 0);
    GFX_FILL(2, 0, tw + 4, 18, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 4, 2);

    // Top-left: HP
    snprintf(buf, sizeof(buf), "HP:%d/%d", player.hp, player.maxHp);
    tw = pd->graphics->getTextWidth(game.fontBold, buf, strlen(buf), kASCIIEncoding, 0);
    GFX_FILL(58, 0, tw + 4, 18, kColorBlack);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 60, 2);

    // Top-right: score
    snprintf(buf, sizeof(buf), "%d", game.score);
    tw = pd->graphics->getTextWidth(game.fontBold, buf, strlen(buf), kASCIIEncoding, 0);
    GFX_FILL(SCREEN_W - tw - 6, 0, tw + 4, 18, kColorBlack);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, SCREEN_W - tw - 4, 2);

    // Bottom-left: level label
    snprintf(buf, sizeof(buf), "Lv%d", player.level);
    tw = pd->graphics->getTextWidth(game.fontBold, buf, strlen(buf), kASCIIEncoding, 0);
    GFX_FILL(2, SCREEN_H - 24, tw + 4, 18, kColorBlack);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 4, SCREEN_H - 22);

    // XP bar (animated fill)
    int barX = 4, barY = SCREEN_H - 6, barW = SCREEN_W - 8, barH = 4;
    GFX_RECT(barX, barY, barW, barH, kColorWhite);
    if (player.xpToNext > 0) {
        int targetFill = (player.xp * (barW - 2)) / player.xpToNext;
        // Animate toward target
        if (game.xpBarDisplayFill < targetFill) {
            game.xpBarDisplayFill += 8;
            if (game.xpBarDisplayFill > targetFill) game.xpBarDisplayFill = targetFill;
        } else if (game.xpBarDisplayFill > targetFill) {
            game.xpBarDisplayFill = targetFill; // snap back on level-up
        }
        int fill = game.xpBarDisplayFill;
        if (fill > 0) {
            int solidW = fill * 7 / 10;
            if (solidW > 0) GFX_FILL(barX + 1, barY + 1, solidW, barH - 2, kColorWhite);
            for (int x = solidW; x < fill; x += 2)
                GFX_FILL(barX + 1 + x, barY + 1, 1, barH - 2, kColorWhite);
        }
    }

    // Tier name (right-aligned below score)
    if (game.currentTierName) {
        int tnw = pd->graphics->getTextWidth(game.fontBold, game.currentTierName, strlen(game.currentTierName), kASCIIEncoding, 0);
        GFX_FILL(SCREEN_W - tnw - 6, 22, tnw + 4, 18, kColorBlack);
        pd->graphics->drawText(game.currentTierName, strlen(game.currentTierName), kASCIIEncoding, SCREEN_W - tnw - 4, 24);
    }

    // Weapon icons + cooldown progress arcs
    {
        uint32_t nowMs = pd->system->getCurrentTimeMilliseconds();
        for (int i = 0; i < player.weaponCount; i++) {
            int ix = SCREEN_W - 18 - i * 16;
            int cy_icon = 53; // icon center y
            GFX_FILL(ix - 2, 43, 16, 16, kColorBlack);
            LCDBitmap* icon = images_get_weapon_icon(player.weapons[i].id);
            if (icon) pd->graphics->drawBitmap(icon, ix, 45, kBitmapUnflipped);
            uint32_t elapsed = nowMs - player.weapons[i].lastFiredMs;
            int cd = player.weapons[i].cooldownMs;
            if (cd <= 0 || elapsed >= (uint32_t)cd) {
                // Ready: filled dot
                GFX_FILL(ix + 3, cy_icon + 3, 4, 4, kColorWhite);
            } else {
                // In progress: arc from 270° (top) clockwise to progress angle
                float pct = (float)elapsed / (float)cd;
                int endDeg = (int)(270.0f + pct * 360.0f) % 360;
                pd->graphics->setDrawMode(kDrawModeXOR);
                pd->graphics->drawEllipse(ix - 1, cy_icon - 5, 14, 10, 1, 270, endDeg, kColorWhite);
                pd->graphics->setDrawMode(kDrawModeFillWhite);
            }
        }
    }

    // Streak counter (centered, below timer)
    if (game.streakKills >= 5 && game.streakWindow > 0) {
        snprintf(buf, sizeof(buf), "x%d", game.streakKills);
        int skw = pd->graphics->getTextWidth(game.fontBold, buf, strlen(buf), kASCIIEncoding, 0);
        GFX_FILL((SCREEN_W - skw) / 2 - 2, 12, skw + 4, 18, kColorBlack);
        if ((game.frameCount / 4) % 2 == 0) {
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, (SCREEN_W - skw) / 2, 14);
        } else {
            pd->graphics->setDrawMode(kDrawModeXOR);
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, (SCREEN_W - skw) / 2, 14);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
        }
    }

    #if DEBUG_BUILD
    snprintf(buf, sizeof(buf), "E:%d B:%d G:%d", enemyCount, bulletCount, xpGemCount);
    tw = pd->graphics->getTextWidth(game.fontBold, buf, strlen(buf), kASCIIEncoding, 0);
    GFX_FILL(2, 24, tw + 4, 18, kColorBlack);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 4, 26);
    #endif

    // Boss health bar (centered, top)
    boss_render_health_bar();

    if (game.fontBold) pd->graphics->setFont(NULL);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Tier announcement
// ---------------------------------------------------------------------------
void ui_draw_tier_announcement(void)
{
    if (game.announceTimer <= 0) return;
    game.announceTimer--;
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    ui_draw_centered_text(game.announceText, SCREEN_H / 2 - 8);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Game Over
// ---------------------------------------------------------------------------
void ui_draw_game_over(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    // Sparse twinkling stars (subtle, 8 only)
    for (int i = 0; i < 8; i++) {
        uint32_t seed = (uint32_t)(i * 6131 + 41);
        int sx = (int)(seed % SCREEN_W);
        int sy = (int)((seed * 23) % 40); // top area only
        int blink = ((game.frameCount + i * 11) / 20) % 3;
        if (blink == 0) {
            int sz = (i < 2) ? 2 : 1;
            GFX_FILL(sx, sy, sz, sz, kColorWhite);
        }
    }

    // Top border
    GFX_FILL(0, 0, SCREEN_W, 2, kColorWhite);
    GFX_FILL(0, 4, SCREEN_W, 1, kColorWhite);

    // Title Background Box
    GFX_FILL(0, 16, SCREEN_W, 28, kColorWhite);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    ui_draw_centered_text("THE KEEPER HAS FALLEN", 22);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontBold) pd->graphics->setFont(NULL);

    // Stats Box (taller to include relic line if active)
    int statsBoxH = (game.activeRelic != RELIC_NONE) ? 64 : 48;
    GFX_RECT(30, 56, SCREEN_W - 60, statsBoxH, kColorWhite);

    char buf[64];
    int mins = (int)game.gameTime / 60;
    int secs = (int)game.gameTime % 60;

    // Left column stats
    snprintf(buf, sizeof(buf), "Time: %d:%02d", mins, secs);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 50, 64);
    snprintf(buf, sizeof(buf), "Level: %d", player.level);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 50, 84);

    // Right column stats
    snprintf(buf, sizeof(buf), "Score: %d", game.score);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 220, 64);
    snprintf(buf, sizeof(buf), "Kills: %d", game.runKills);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 220, 84);

    // Relic line (if active)
    if (game.activeRelic != RELIC_NONE) {
        snprintf(buf, sizeof(buf), "Relic: %s", relic_get_name(game.activeRelic));
        ui_draw_centered_text(buf, 100);
    }

    // NEW BEST indicator inside stats box if applicable
    if (game.score >= game.highScore && game.score > 0) {
        if ((game.frameCount / 15) % 2 == 0) {
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            GFX_FILL(150, 72, 100, 16, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            ui_draw_centered_text("NEW BEST!", 72);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
        }
    }

    // Salvage earned this run
    {
        snprintf(buf, sizeof(buf), "+%d Salvage", game.runSalvage);
        ui_draw_centered_text(buf, 108);
    }

    // Weapons (Horizontal Row)
    ui_draw_centered_text("- Equipment -", 122);
    
    {
        int totalWeapons = player.weaponCount;
        int iconBmp = 24;  // actual bitmap size
        int boxSize = 28;  // bounding box size
        int spacing = 36;
        int totalW = (totalWeapons - 1) * spacing + boxSize;
        int startX = (SCREEN_W - totalW) / 2;

        for (int i = 0; i < totalWeapons; i++) {
            int bx = startX + i * spacing;

            // Bounding box
            GFX_RECT(bx, 140, boxSize, boxSize, kColorWhite);

            // Center 24x24 icon in box
            int iconOff = (boxSize - iconBmp) / 2;
            LCDBitmap* icon = images_get_weapon_icon_large(player.weapons[i].id);
            if (icon) pd->graphics->drawBitmap(icon, bx + iconOff, 140 + iconOff, kBitmapUnflipped);

            // Centered level below the box
            snprintf(buf, sizeof(buf), "Lv%d", player.weapons[i].level);
            int tw = pd->graphics->getTextWidth(NULL, buf, strlen(buf), kASCIIEncoding, 0);
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, bx + boxSize / 2 - tw / 2, 172);
        }
    }

    // Boxed buttons — uniform width, properly centered
    int btnY = 200;
    const char* options[] = { "Try Again", "Menu" };
    int tw0 = pd->graphics->getTextWidth(NULL, options[0], strlen(options[0]), kASCIIEncoding, 0);
    int tw1 = pd->graphics->getTextWidth(NULL, options[1], strlen(options[1]), kASCIIEncoding, 0);
    int btnW = maxi(tw0, tw1) + 32;
    int gap = 24;
    int totalBtnW = btnW * 2 + gap;
    int bx0 = (SCREEN_W - totalBtnW) / 2;
    int bx1 = bx0 + btnW + gap;

    for (int i = 0; i < 2; i++) {
        int bx = (i == 0) ? bx0 : bx1;
        int tw = (i == 0) ? tw0 : tw1;
        int textX = bx + (btnW - tw) / 2;

        if (i == game.gameOverSelection) {
            GFX_FILL(bx, btnY, btnW, 24, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->drawText(options[i], strlen(options[i]), kASCIIEncoding, textX, btnY + 4);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            // selection triangles
            GFX_FILL(bx - 10, btnY + 8, 4, 8, kColorWhite);
            GFX_FILL(bx + btnW + 6, btnY + 8, 4, 8, kColorWhite);
        } else {
            GFX_RECT(bx, btnY, btnW, 24, kColorWhite);
            pd->graphics->drawText(options[i], strlen(options[i]), kASCIIEncoding, textX, btnY + 4);
        }
    }

    // Dithered bottom border frame
    for (int x = 20; x < SCREEN_W - 20; x += 2)
        GFX_FILL(x, 232, 1, 1, kColorWhite);
    for (int x = 21; x < SCREEN_W - 20; x += 2)
        GFX_FILL(x, 234, 1, 1, kColorWhite);

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Victory
// ---------------------------------------------------------------------------
void ui_draw_victory(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    // Sparse twinkling stars
    for (int i = 0; i < 8; i++) {
        uint32_t seed = (uint32_t)(i * 7919 + 13);
        int sx = (int)(seed % SCREEN_W);
        int sy = (int)((seed * 31) % 40);
        int blink = ((game.frameCount + i * 11) / 20) % 3;
        if (blink == 0) {
            int sz = (i < 2) ? 2 : 1;
            GFX_FILL(sx, sy, sz, sz, kColorWhite);
        }
    }

    // Top border
    GFX_FILL(0, 0, SCREEN_W, 2, kColorWhite);
    GFX_FILL(0, 4, SCREEN_W, 1, kColorWhite);

    // Title Background Box
    GFX_FILL(0, 16, SCREEN_W, 28, kColorWhite);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    ui_draw_centered_text("DAWN BREAKS", 22);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontBold) pd->graphics->setFont(NULL);

    ui_draw_centered_text("The Keeper endures.", 50);

    // Stats Box
    GFX_RECT(30, 68, SCREEN_W - 60, 48, kColorWhite);

    char buf[64];
    int mins = (int)game.gameTime / 60;
    int secs = (int)game.gameTime % 60;

    // Left column stats
    snprintf(buf, sizeof(buf), "Time: %d:%02d", mins, secs);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 50, 76);
    snprintf(buf, sizeof(buf), "Level: %d", player.level);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 50, 96);

    // Right column stats
    snprintf(buf, sizeof(buf), "Score: %d", game.score);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 220, 76);
    snprintf(buf, sizeof(buf), "Kills: %d", game.runKills);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 220, 96);

    // Salvage earned
    snprintf(buf, sizeof(buf), "+%d Salvage", game.runSalvage);
    ui_draw_centered_text(buf, 120);

    // Equipment
    ui_draw_centered_text("- Equipment -", 136);

    {
        int totalWeapons = player.weaponCount;
        int iconBmp = 24;
        int boxSize = 28;
        int spacing = 36;
        int totalW = (totalWeapons - 1) * spacing + boxSize;
        int startX = (SCREEN_W - totalW) / 2;

        for (int i = 0; i < totalWeapons; i++) {
            int bx = startX + i * spacing;
            GFX_RECT(bx, 152, boxSize, boxSize, kColorWhite);
            int iconOff = (boxSize - iconBmp) / 2;
            LCDBitmap* icon = images_get_weapon_icon_large(player.weapons[i].id);
            if (icon) pd->graphics->drawBitmap(icon, bx + iconOff, 152 + iconOff, kBitmapUnflipped);
            snprintf(buf, sizeof(buf), "Lv%d", player.weapons[i].level);
            int tw = pd->graphics->getTextWidth(NULL, buf, strlen(buf), kASCIIEncoding, 0);
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, bx + boxSize / 2 - tw / 2, 184);
        }
    }

    // Boxed buttons
    int btnY = 200;
    const char* options[] = { "Play Again", "Menu" };
    int tw0 = pd->graphics->getTextWidth(NULL, options[0], strlen(options[0]), kASCIIEncoding, 0);
    int tw1 = pd->graphics->getTextWidth(NULL, options[1], strlen(options[1]), kASCIIEncoding, 0);
    int btnW = maxi(tw0, tw1) + 32;
    int gap = 24;
    int totalBtnW = btnW * 2 + gap;
    int bx0 = (SCREEN_W - totalBtnW) / 2;
    int bx1 = bx0 + btnW + gap;

    for (int i = 0; i < 2; i++) {
        int bx = (i == 0) ? bx0 : bx1;
        int tw = (i == 0) ? tw0 : tw1;
        int textX = bx + (btnW - tw) / 2;

        if (i == game.gameOverSelection) {
            GFX_FILL(bx, btnY, btnW, 24, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->drawText(options[i], strlen(options[i]), kASCIIEncoding, textX, btnY + 4);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            GFX_FILL(bx - 10, btnY + 8, 4, 8, kColorWhite);
            GFX_FILL(bx + btnW + 6, btnY + 8, 4, 8, kColorWhite);
        } else {
            GFX_RECT(bx, btnY, btnW, 24, kColorWhite);
            pd->graphics->drawText(options[i], strlen(options[i]), kASCIIEncoding, textX, btnY + 4);
        }
    }

    // Dithered bottom border
    for (int x = 20; x < SCREEN_W - 20; x += 2)
        GFX_FILL(x, 232, 1, 1, kColorWhite);
    for (int x = 21; x < SCREEN_W - 20; x += 2)
        GFX_FILL(x, 234, 1, 1, kColorWhite);

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Upgrade screen
// ---------------------------------------------------------------------------
void ui_draw_upgrade_screen(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    // Stars (mostly static, subtle rare twinkle)
    for (int i = 0; i < 30; i++) {
        uint32_t seed = (uint32_t)(i * 7919 + 13);
        int sx = (int)(seed % SCREEN_W);
        int sy = (int)((seed * 31) % 200);
        int blink = ((game.frameCount + i * 7) / 30) % 6;
        if (blink != 0) {
            int sz = (i < 3) ? 2 : 1;
            GFX_FILL(sx, sy, sz, sz, kColorWhite);
        }
    }

    // Horizon fog band (dithered haze above waves)
    for (int y = 200; y < 210; y++) {
        int density = (y < 205) ? 5 : 3;
        for (int x = 0; x < SCREEN_W; x += density) {
            if ((x + y + (int)game.frameCount / 8) % density == 0)
                GFX_FILL(x, y, 1, 1, kColorWhite);
        }
    }

    // Dramatic layered ocean waves (matching title screen quality)
    for (int x = 0; x < SCREEN_W; x += 6) {
        int wy1 = 210 + (int)(sinf((float)x * 0.03f + (float)game.frameCount * 0.08f) * 4.0f);
        GFX_FILL(x, wy1, 5, 1, kColorWhite);
    }
    for (int x = 0; x < SCREEN_W; x += 7) {
        int wy2 = 218 + (int)(sinf((float)x * 0.06f + (float)game.frameCount * 0.12f) * 2.5f);
        GFX_FILL(x + 2, wy2, 4, 1, kColorWhite);
    }
    for (int x = 0; x < SCREEN_W; x += 9) {
        int wy3 = 226 + (int)(sinf((float)x * 0.04f + (float)game.frameCount * 0.05f) * 2.0f);
        GFX_FILL(x + 1, wy3, 3, 1, kColorWhite);
    }
    // Foam spray at wave crests
    for (int i = 0; i < 6; i++) {
        uint32_t seed = (uint32_t)(i * 3137 + game.frameCount / 4);
        int fx = (int)(seed % (uint32_t)SCREEN_W);
        float waveY = sinf((float)fx * 0.03f + (float)game.frameCount * 0.08f);
        if (waveY > 2.0f) {
            int fy = 208 - (int)(waveY * 1.5f);
            GFX_FILL(fx, fy, 1, 1, kColorWhite);
        }
    }

    // Title
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("LEVEL UP!", 8);
    if (game.fontBold) pd->graphics->setFont(NULL);
    ui_draw_centered_text("Choose an upgrade:", 26);

    // Slide-in offset (cards slide from right, 8 frames)
    int slideOff = (game.upgradeOpenTimer > 0) ? game.upgradeOpenTimer * 6 : 0;

    for (int i = 0; i < game.upgradeChoiceCount; i++) {
        int y = 50 + i * 52;
        UpgradeChoice* c = &game.upgradeChoices[i];
        int cx1 = 16 + slideOff, cy1 = y - 4, cw = SCREEN_W - 32, ch = 44;

        // Weapon level delta label for weapon upgrades
        char nameBuf[64];
        if (c->type == 1 && c->name) {
            int curLevel = 0;
            for (int j = 0; j < player.weaponCount; j++) {
                if (player.weapons[j].id == (WeaponId)c->id) { curLevel = player.weapons[j].level; break; }
            }
            if (curLevel > 0)
                snprintf(nameBuf, sizeof(nameBuf), "> %s  L%d->L%d", c->name, curLevel, curLevel + 1);
            else
                snprintf(nameBuf, sizeof(nameBuf), "> %s", c->name);
        } else if (c->name) {
            snprintf(nameBuf, sizeof(nameBuf), "> %s", c->name);
        } else {
            nameBuf[0] = '\0';
        }

        if (i == game.selectedUpgrade) {
            // Selected: white fill + border
            GFX_FILL(cx1, cy1, cw, ch, kColorWhite);

            // Evolution card: pulsing double-ring border
            if (c->isEvolution) {
                int phase = (game.frameCount / 4) % 2;
                pd->graphics->setDrawMode(kDrawModeXOR);
                GFX_RECT(cx1 - 4 - phase, cy1 - 4 - phase, cw + 8 + phase*2, ch + 8 + phase*2, kColorWhite);
                pd->graphics->setDrawMode(kDrawModeFillBlack);
            } else {
                // Normal pulsing outer border
                if ((game.frameCount / 8) % 2 == 0)
                    GFX_RECT(cx1 - 2, cy1 - 2, cw + 4, ch + 4, kColorWhite);
                pd->graphics->setDrawMode(kDrawModeFillBlack);
            }

            // Draw large icon
            if (c->type == 0 || c->type == 1 || c->type == 3) {
                LCDBitmap* icon = images_get_weapon_icon_large((WeaponId)c->id);
                if (icon) pd->graphics->drawBitmap(icon, cx1 + 6, y + 6, kBitmapUnflipped);
            } else if (c->type == 2) {
                LCDBitmap* icon = images_get_passive_icon_large(c->id);
                if (icon) pd->graphics->drawBitmap(icon, cx1 + 6, y + 6, kBitmapUnflipped);
            }

            // Name in bold font
            if (game.fontBold) pd->graphics->setFont(game.fontBold);
            pd->graphics->drawText(nameBuf, strlen(nameBuf), kASCIIEncoding, cx1 + 38, y + 2);
            if (game.fontBold) pd->graphics->setFont(NULL);
            // Description in default font
            if (c->desc) pd->graphics->drawText(c->desc, strlen(c->desc), kASCIIEncoding, cx1 + 38, y + 20);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
        } else {
            // Unselected: opaque black fill + white border
            GFX_FILL(cx1 + 1, cy1 + 1, cw - 2, ch - 2, kColorBlack);
            GFX_RECT(cx1, cy1, cw, ch, kColorWhite);

            // Draw large icon
            if (c->type == 0 || c->type == 1 || c->type == 3) {
                LCDBitmap* icon = images_get_weapon_icon_large((WeaponId)c->id);
                if (icon) pd->graphics->drawBitmap(icon, cx1 + 6, y + 6, kBitmapUnflipped);
            } else if (c->type == 2) {
                LCDBitmap* icon = images_get_passive_icon_large(c->id);
                if (icon) pd->graphics->drawBitmap(icon, cx1 + 6, y + 6, kBitmapUnflipped);
            }

            // Name in bold font (unselected: show without ">" prefix)
            if (c->name) {
                if (game.fontBold) pd->graphics->setFont(game.fontBold);
                pd->graphics->drawText(c->name, strlen(c->name), kASCIIEncoding, cx1 + 38, y + 2);
                if (game.fontBold) pd->graphics->setFont(NULL);
            }
            if (c->desc) pd->graphics->drawText(c->desc, strlen(c->desc), kASCIIEncoding, cx1 + 38, y + 20);
        }

        // Corner bracket decorations
        int bLen = 6;
        int cx2 = cx1 + cw, cy2 = cy1 + ch;
        GFX_FILL(cx1, cy1, bLen, 1, kColorWhite);
        GFX_FILL(cx1, cy1, 1, bLen, kColorWhite);
        GFX_FILL(cx2 - bLen, cy1, bLen, 1, kColorWhite);
        GFX_FILL(cx2 - 1, cy1, 1, bLen, kColorWhite);
        GFX_FILL(cx1, cy2 - 1, bLen, 1, kColorWhite);
        GFX_FILL(cx1, cy2 - bLen, 1, bLen, kColorWhite);
        GFX_FILL(cx2 - bLen, cy2 - 1, bLen, 1, kColorWhite);
        GFX_FILL(cx2 - 1, cy2 - bLen, 1, bLen, kColorWhite);
    }

    // Reroll hint
    if (game.rerollsLeft > 0) {
        const char* hint = "~ Crank to reroll ~";
        pd->graphics->drawText(hint, strlen(hint), kASCIIEncoding,
            SCREEN_W / 2 - 55, SCREEN_H - 18);
    }

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Cutscene helper: lighthouse silhouette + waves
// ---------------------------------------------------------------------------
static void draw_lighthouse(int baseX, int baseY)
{
    GFX_FILL(baseX - 8, baseY, 16, 50, kColorWhite);
    GFX_FILL(baseX - 12, baseY - 4, 24, 6, kColorWhite);
    GFX_FILL(baseX - 6, baseY - 10, 12, 8, kColorWhite);
    // Rotating beam (full 360°)
    float angle = (float)game.cutscene.framesLeft * 0.03f;
    float cosA = cosf(angle);
    float sinA = sinf(angle);
    int lx = baseX;
    int ly = baseY - 8;
    for (int r = -1; r <= 1; r++) {
        int ex = lx + (int)(cosA * 55.0f);
        int ey = ly + (int)(sinA * 55.0f) + r * 2;
        GFX_LINE(lx, ly + r, ex, ey, 1, kColorWhite);
    }
}

static void draw_sea_waves(int baseY, int frame)
{
    for (int x = 0; x < SCREEN_W; x += 8) {
        int wy = baseY + (int)(sinf((float)x * 0.05f + frame * 0.1f) * 2.0f);
        GFX_FILL(x, wy, 6, 1, kColorWhite);
    }
}

void ui_draw_opening_scene(void)
{
    // Dense mist/rain particles with horizontal drift and size variation
    int elapsed = game.cutscene.totalFrames - game.cutscene.framesLeft;
    for (int i = 0; i < 25; i++) {
        uint32_t seed = (uint32_t)(i * 4729 + 37);
        int px = (int)((seed + (uint32_t)(elapsed * (1 + i % 3))) % (uint32_t)SCREEN_W);
        int py = (int)((seed * 17 + (uint32_t)(elapsed * (2 + i % 3))) % (uint32_t)SCREEN_H);
        int sz = (i < 5) ? 2 : 1;
        GFX_FILL(px, py, sz, sz, kColorWhite);
    }
}

void ui_draw_fog_scene(void)
{
    draw_sea_waves(175, game.cutscene.framesLeft);
    draw_sea_waves(185, game.cutscene.framesLeft + 40);
    // Layered horizontal fog bands with slow drift
    int drift = game.cutscene.framesLeft / 2;
    for (int i = 0; i < 12; i++) {
        uint32_t seed = (uint32_t)(i * 3571 + 47);
        int fy = 30 + (int)(seed % 130);
        int fw = 80 + (int)((seed * 1237) % 120);
        int fx = (int)((seed * 5113 + drift * (1 + i % 3)) % (SCREEN_W + 40)) - 20;
        // Dithered fog band (2px tall for density)
        for (int x = fx; x < fx + fw && x < SCREEN_W; x += 2) {
            if (x >= 0) {
                GFX_FILL(x, fy, 1, 1, kColorWhite);
                if (i % 2 == 0 && fy + 1 < SCREEN_H)
                    GFX_FILL(x + 1, fy + 1, 1, 1, kColorWhite);
            }
        }
    }
}

void ui_draw_corruption_scene(void)
{
    // Screen-edge dither corruption effect
    int depth = 12 + (game.cutscene.framesLeft / 30) % 8;
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < depth; x++) {
            if ((x + y + game.cutscene.framesLeft) % 3 == 0)
                GFX_FILL(x, y, 1, 1, kColorWhite);
        }
        for (int x = SCREEN_W - depth; x < SCREEN_W; x++) {
            if ((x + y + game.cutscene.framesLeft) % 3 == 0)
                GFX_FILL(x, y, 1, 1, kColorWhite);
        }
    }
    for (int x = depth; x < SCREEN_W - depth; x++) {
        for (int y = 0; y < depth; y++) {
            if ((x + y + game.cutscene.framesLeft) % 3 == 0)
                GFX_FILL(x, y, 1, 1, kColorWhite);
        }
        for (int y = SCREEN_H - depth; y < SCREEN_H; y++) {
            if ((x + y + game.cutscene.framesLeft) % 3 == 0)
                GFX_FILL(x, y, 1, 1, kColorWhite);
        }
    }
    // Random glitch pixels inside the corruption zone — unsettling flicker
    for (int i = 0; i < 20; i++) {
        uint32_t seed = (uint32_t)(i * 8713 + game.cutscene.framesLeft * 37);
        int gx = (int)(seed % (uint32_t)SCREEN_W);
        int gy = (int)((seed * 19) % (uint32_t)SCREEN_H);
        // Only inside the border zone
        if (gx < depth || gx >= SCREEN_W - depth || gy < depth || gy >= SCREEN_H - depth) {
            GFX_FILL(gx, gy, 2, 1, kColorWhite);
        }
    }
}

void ui_draw_sunrise_scene(void)
{
    // Expanding horizontal rays from center
    int elapsed = 180 - game.cutscene.framesLeft;
    int maxRays = 6;
    for (int i = 0; i < maxRays; i++) {
        int rayW = elapsed * 2 - i * 8;
        if (rayW < 4) continue;
        if (rayW > SCREEN_W) rayW = SCREEN_W;
        int ry = SCREEN_H / 2 + (i - maxRays / 2) * 12;
        GFX_FILL((SCREEN_W - rayW) / 2, ry, rayW, 2, kColorWhite);
    }
}

// ---------------------------------------------------------------------------
// Cutscene
// ---------------------------------------------------------------------------
void ui_draw_cutscene(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.cutscene.drawFunc) game.cutscene.drawFunc();

    int elapsed = game.cutscene.totalFrames - game.cutscene.framesLeft;
    int startY = SCREEN_H / 2 - (game.cutscene.lineCount * 11);

    if (game.fontBold) pd->graphics->setFont(game.fontBold);

    // Line-by-line reveal with smooth horizontal wipe-in
    for (int i = 0; i < game.cutscene.lineCount; i++) {
        int lineStart = i * 25;
        if (elapsed < lineStart) break;
        if (!game.cutscene.lines[i]) continue;
        int lineElapsed = elapsed - lineStart;
        int lineY = startY + i * 22;
        int len = (int)strlen(game.cutscene.lines[i]);
        int tw = pd->graphics->getTextWidth(game.fontBold, game.cutscene.lines[i], len, kASCIIEncoding, 0);
        int textX = (SCREEN_W - tw) / 2;
        if (lineElapsed >= 15) {
            pd->graphics->drawText(game.cutscene.lines[i], len, kASCIIEncoding, textX, lineY);
        } else {
            // Wipe-in: reveal left to right
            int revealW = (tw * lineElapsed) / 15;
            if (revealW < 2) revealW = 2;
            pd->graphics->setClipRect(textX, lineY, revealW, 20);
            pd->graphics->drawText(game.cutscene.lines[i], len, kASCIIEncoding, textX, lineY);
            pd->graphics->clearClipRect();
        }
    }

    // "Press A to continue" after all lines fully revealed
    int allRevealed = elapsed >= game.cutscene.lineCount * 25 + 15;
    if (allRevealed && (game.frameCount / 30) % 2 == 0) {
        int tw = pd->graphics->getTextWidth(game.fontBold, "Press A to continue", 19, kASCIIEncoding, 0);
        pd->graphics->drawText("Press A to continue", 19, kASCIIEncoding, (SCREEN_W - tw) / 2, SCREEN_H - 24);
    }
    if (game.fontBold) pd->graphics->setFont(NULL);

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Crate reward
// ---------------------------------------------------------------------------
void ui_draw_crate_reward(void)
{
    if (game.crateRewardTimer <= 0) return;
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    int tw = pd->graphics->getTextWidth(NULL, game.crateRewardText, strlen(game.crateRewardText), kASCIIEncoding, 0);
    int bx = (SCREEN_W - tw) / 2 - 12;
    int by = SCREEN_H / 2 - 34;
    int bw = tw + 24;
    int bh = 28;
    GFX_FILL(bx, by, bw, bh, kColorBlack);
    // Double-line border
    GFX_RECT(bx, by, bw, bh, kColorWhite);
    GFX_RECT(bx + 2, by + 2, bw - 4, bh - 4, kColorWhite);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text(game.crateRewardText, by + 8);
    if (game.fontBold) pd->graphics->setFont(NULL);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Armory
// ---------------------------------------------------------------------------
void ui_draw_armory(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    // Sparse stars
    for (int i = 0; i < 12; i++) {
        uint32_t seed = (uint32_t)(i * 4391 + 19);
        int sx = (int)(seed % SCREEN_W);
        int sy = (int)((seed * 29) % 30);
        int blink = ((game.frameCount + i * 9) / 18) % 3;
        if (blink == 0) GFX_FILL(sx, sy, 1, 1, kColorWhite);
    }

    // Single wave at bottom
    for (int x = 0; x < SCREEN_W; x += 7) {
        int wy = 228 + (int)(sinf((float)x * 0.04f + (float)game.frameCount * 0.06f) * 2.0f);
        GFX_FILL(x, wy, 5, 1, kColorWhite);
    }

    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("ARMORY", 6);
    if (game.fontBold) pd->graphics->setFont(NULL);

    GFX_FILL(10, 22, SCREEN_W - 20, 1, kColorWhite);
    GFX_FILL(10, 24, SCREEN_W - 20, 1, kColorWhite);

    // Scrollable: show 4 entries at a time with 48px each
    int itemH = 48;
    int visibleCount = 4;
    int sel = game.armorySelection - 1; // 0-based
    int scrollTop = sel - 1;
    if (scrollTop > WEAPON_COUNT - visibleCount) scrollTop = WEAPON_COUNT - visibleCount;
    if (scrollTop < 0) scrollTop = 0;

    pd->graphics->setClipRect(0, 30, SCREEN_W, SCREEN_H - 30);
    for (int i = scrollTop; i < scrollTop + visibleCount && i < WEAPON_COUNT; i++) {
        int y = 34 + (i - scrollTop) * itemH;
        if (i + 1 == game.armorySelection) {
            GFX_FILL(14, y - 2, SCREEN_W - 28, itemH - 6, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
        }
        // Weapon icon (large)
        if (game.unlockedWeapons[i]) {
            LCDBitmap* icon = images_get_weapon_icon_large((WeaponId)i);
            if (icon) pd->graphics->drawBitmap(icon, 20, y + 6, kBitmapUnflipped);
        }
        if (game.unlockedWeapons[i]) {
            const char* name = weapon_get_name((WeaponId)i);
            if (game.fontBold) pd->graphics->setFont(game.fontBold);
            pd->graphics->drawText(name, strlen(name), kASCIIEncoding, 52, y + 2);
            if (game.fontBold) pd->graphics->setFont(NULL);
            const char* desc = weapon_get_desc((WeaponId)i, 1);
            pd->graphics->drawText(desc, strlen(desc), kASCIIEncoding, 52, y + 18);
        } else {
            pd->graphics->drawText("???", 3, kASCIIEncoding, 52, y + 2);
        }
        if (i + 1 == game.armorySelection) pd->graphics->setDrawMode(kDrawModeFillWhite);
    }
    pd->graphics->clearClipRect();

    // Scroll indicators
    if (scrollTop > 0) {
        ui_draw_centered_text("^", 26);
    }
    if (scrollTop + visibleCount < WEAPON_COUNT) {
        ui_draw_centered_text("v", SCREEN_H - 14);
    }

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Bestiary
// ---------------------------------------------------------------------------
void ui_draw_bestiary(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    // Sparse stars
    for (int i = 0; i < 10; i++) {
        uint32_t seed = (uint32_t)(i * 5113 + 23);
        int sx = (int)(seed % SCREEN_W);
        int sy = (int)((seed * 31) % 25);
        int blink = ((game.frameCount + i * 13) / 20) % 3;
        if (blink == 0) GFX_FILL(sx, sy, 1, 1, kColorWhite);
    }

    // Single wave at bottom
    for (int x = 0; x < SCREEN_W; x += 7) {
        int wy = 230 + (int)(sinf((float)x * 0.05f + (float)game.frameCount * 0.07f) * 1.5f);
        GFX_FILL(x, wy, 4, 1, kColorWhite);
    }

    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("BESTIARY", 6);
    if (game.fontBold) pd->graphics->setFont(NULL);

    GFX_FILL(10, 22, SCREEN_W - 20, 1, kColorWhite);
    GFX_FILL(10, 24, SCREEN_W - 20, 1, kColorWhite);

    static const char* names[] = {
        "Creeper","Tendril","Wraith","Abyssal","Seer","Lamprey","Bloat","Harbinger"
    };
    static const char* descs[] = {
        "Eyeless. Relentless. They smell the light.",
        "Born from drowned sailors' regrets.",
        "Passes through walls like grief through time.",
        "The deep given form and hunger.",
        "It remembers what you'll do next.",
        "Split one, and two take its place.",
        "Swollen with the pressure of fathoms.",
        "Where it walks, others follow stronger."
    };

    // Scrollable: show 4 entries at a time with 48px each
    int itemH = 48;
    int visibleCount = 4;
    int sel = game.bestiarySelection - 1; // 0-based
    int scrollTop = sel - 1;
    if (scrollTop > ENEMY_TYPE_COUNT - visibleCount) scrollTop = ENEMY_TYPE_COUNT - visibleCount;
    if (scrollTop < 0) scrollTop = 0;

    pd->graphics->setClipRect(0, 30, SCREEN_W, SCREEN_H - 30);
    for (int i = scrollTop; i < scrollTop + visibleCount && i < ENEMY_TYPE_COUNT; i++) {
        int y = 34 + (i - scrollTop) * itemH;
        if (i + 1 == game.bestiarySelection) {
            GFX_FILL(14, y - 2, SCREEN_W - 28, itemH - 6, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
        }
        if (game.unlockedEnemies[i]) {
            LCDBitmap* img = images_get_enemy((EnemyType)i, 0);
            if (img) pd->graphics->drawBitmap(img, 18, y + 4, kBitmapUnflipped);
            if (game.fontBold) pd->graphics->setFont(game.fontBold);
            pd->graphics->drawText(names[i], strlen(names[i]), kASCIIEncoding, 80, y + 2);
            if (game.fontBold) pd->graphics->setFont(NULL);
            pd->graphics->drawText(descs[i], strlen(descs[i]), kASCIIEncoding, 80, y + 18);
        } else {
            pd->graphics->drawText("???", 3, kASCIIEncoding, 80, y + 2);
        }
        if (i + 1 == game.bestiarySelection) pd->graphics->setDrawMode(kDrawModeFillWhite);
    }
    pd->graphics->clearClipRect();

    // Scroll indicators
    if (scrollTop > 0) {
        ui_draw_centered_text("^", 26);
    }
    if (scrollTop + visibleCount < ENEMY_TYPE_COUNT) {
        ui_draw_centered_text("v", SCREEN_H - 14);
    }

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Logbook (lifetime stats)
// ---------------------------------------------------------------------------
void ui_draw_logbook(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("KEEPER'S LOGBOOK", 6);
    if (game.fontBold) pd->graphics->setFont(NULL);
    GFX_FILL(10, 24, SCREEN_W - 20, 1, kColorWhite);

    char buf[64];
    int y = 34;
    int lineH = 18;

    snprintf(buf, sizeof(buf), "Total Runs: %lu", (unsigned long)game.totalRuns);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;

    snprintf(buf, sizeof(buf), "Total Kills: %lu", (unsigned long)game.totalKills);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;

    snprintf(buf, sizeof(buf), "Salvage Earned: %lu", (unsigned long)game.totalSalvage);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;

    snprintf(buf, sizeof(buf), "Salvage Available: %d", save_get_salvage());
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;

    snprintf(buf, sizeof(buf), "High Score: %d", game.highScore);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;

    {
        uint32_t t = game.bestTimeMs / 1000;
        int bm = (int)(t / 60);
        int bs = (int)(t % 60);
        snprintf(buf, sizeof(buf), "Best Time: %d:%02d", bm, bs);
        pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;
    }

    // Enemies discovered count
    {
        int discovered = 0;
        for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
            if (game.unlockedEnemies[i]) discovered++;
        snprintf(buf, sizeof(buf), "Creatures Discovered: %d/%d", discovered, ENEMY_TYPE_COUNT);
        pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;
    }

    // Weapons discovered count
    {
        int discovered = 0;
        for (int i = 0; i < WEAPON_COUNT; i++)
            if (game.unlockedWeapons[i]) discovered++;
        snprintf(buf, sizeof(buf), "Weapons Discovered: %d/%d", discovered, WEAPON_COUNT);
        pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 30, y); y += lineH;
    }

    // Footer
    y = SCREEN_H - 18;
    ui_draw_centered_text("B: Back", y);

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Keeper Select
// ---------------------------------------------------------------------------
void ui_draw_keeper_select(void)
{
    GFX_CLEAR(kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);

    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    ui_draw_centered_text("CHOOSE YOUR KEEPER", 6);
    if (game.fontBold) pd->graphics->setFont(NULL);
    GFX_FILL(10, 24, SCREEN_W - 20, 1, kColorWhite);

    // Salvage display
    {
        char sbuf[32];
        snprintf(sbuf, sizeof(sbuf), "Salvage: %d", save_get_salvage());
        int sw = pd->graphics->getTextWidth(NULL, sbuf, strlen(sbuf), kASCIIEncoding, 0);
        pd->graphics->drawText(sbuf, strlen(sbuf), kASCIIEncoding, SCREEN_W - sw - 10, 8);
    }

    char buf[64];
    int itemH = 50;
    int startY = 28;

    for (int i = 0; i < KEEPER_COUNT; i++) {
        const KeeperDef* k = &KEEPER_DEFS[i];
        int y = startY + i * itemH;
        int isSelected = (i == game.keeperSelection);
        int isActive = (i == game.selectedKeeper);
        int isUnlocked = game.keeperUnlocked[i];

        if (isSelected) {
            GFX_FILL(10, y, SCREEN_W - 20, itemH - 4, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
        } else {
            GFX_RECT(10, y, SCREEN_W - 20, itemH - 4, kColorWhite);
        }

        int textX = 22;

        if (isUnlocked) {
            // Name + active indicator
            if (game.fontBold) pd->graphics->setFont(game.fontBold);
            if (isActive) {
                snprintf(buf, sizeof(buf), "%s  [ACTIVE]", k->name);
            } else {
                snprintf(buf, sizeof(buf), "%s", k->name);
            }
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, textX, y + 4);
            if (game.fontBold) pd->graphics->setFont(NULL);

            // Description + starter on same line
            snprintf(buf, sizeof(buf), "%s  Starts: %s", k->desc, weapon_get_name((WeaponId)k->starterWeapon));
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, textX, y + 22);
        } else {
            // Locked — show name + cost
            if (game.fontBold) pd->graphics->setFont(game.fontBold);
            snprintf(buf, sizeof(buf), "%s", k->name);
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, textX, y + 4);
            if (game.fontBold) pd->graphics->setFont(NULL);

            snprintf(buf, sizeof(buf), "Locked  -  %d Salvage", k->unlockCost);
            pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, textX, y + 22);
        }

        if (isSelected) {
            pd->graphics->setDrawMode(kDrawModeFillWhite);
        }
    }

    // Footer
    ui_draw_centered_text("A: Select   B: Back", SCREEN_H - 16);

    pd->graphics->setDrawMode(kDrawModeCopy);
}
