#include "game.h"
#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Tile grid and background
// ---------------------------------------------------------------------------
static int tileGrid[GRID_H][GRID_W];
static LCDBitmap* bgBitmap = NULL;

// ---------------------------------------------------------------------------
// Build tile grid
// ---------------------------------------------------------------------------
static void build_tile_grid(void)
{
    for (int ty = 0; ty < GRID_H; ty++) {
        for (int tx = 0; tx < GRID_W; tx++) {
            if (tx == 0 || tx == GRID_W - 1 || ty == 0 || ty == GRID_H - 1) {
                tileGrid[ty][tx] = 6;
            } else if (tx == 1 || tx == GRID_W - 2 || ty == 1 || ty == GRID_H - 2) {
                tileGrid[ty][tx] = 5;
            } else {
                int r = rng_range(0, 100);
                if (r < 60) tileGrid[ty][tx] = rng_range(0, 3);
                else if (r < 85) tileGrid[ty][tx] = 6;
                else tileGrid[ty][tx] = 7;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Pre-render background
// ---------------------------------------------------------------------------
static void render_background(void)
{
    if (bgBitmap) pd->graphics->freeBitmap(bgBitmap);
    bgBitmap = pd->graphics->newBitmap(MAP_W, MAP_H, kColorBlack);
    GFX_PUSH(bgBitmap);
    GFX_CLEAR(kColorBlack);
    for (int ty = 0; ty < GRID_H; ty++) {
        for (int tx = 0; tx < GRID_W; tx++) {
            LCDBitmap* tile = images_get_tile(tileGrid[ty][tx]);
            if (tile) pd->graphics->drawBitmap(tile, tx * TILE_SIZE, ty * TILE_SIZE, kBitmapUnflipped);
        }
    }
    GFX_POP();
}

void rendering_init(void)
{
    build_tile_grid();
    render_background();
    DLOG("Rendering initialized: %dx%d background", MAP_W, MAP_H);
}

void rendering_update_background_shrink(void)
{
    int s = game.arenaShrink;
    int tileS = s / TILE_SIZE;
    for (int ty = 0; ty < GRID_H; ty++) {
        for (int tx = 0; tx < GRID_W; tx++) {
            if (tx <= tileS || tx >= GRID_W - 1 - tileS ||
                ty <= tileS || ty >= GRID_H - 1 - tileS) {
                if (tx == tileS || tx == GRID_W - 1 - tileS ||
                    ty == tileS || ty == GRID_H - 1 - tileS) {
                    tileGrid[ty][tx] = 4;
                } else if (tx < tileS || tx > GRID_W - 1 - tileS ||
                           ty < tileS || ty > GRID_H - 1 - tileS) {
                    tileGrid[ty][tx] = 6;
                }
            }
        }
    }
    render_background();
}

// ---------------------------------------------------------------------------
// Draw gameplay frame
// ---------------------------------------------------------------------------
void rendering_draw_playing(void)
{
    int cx = (int)game.cameraX - game.shakeX;
    int cy = (int)game.cameraY - game.shakeY;

    GFX_CLEAR(kColorBlack);

    // Background
    if (bgBitmap) pd->graphics->drawBitmap(bgBitmap, -cx, -cy, kBitmapUnflipped);

    // Anchors (ground layer)
    for (int i = 0; i < anchorCount; i++) {
        Anchor* a = &anchors[i];
        if (!a->alive) continue;
        int sx = (int)a->x - cx;
        int sy = (int)a->y - cy;
        LCDBitmap* aImg = images_get_anchor();
        int ax = sx - 6;
        int ay = sy - 6;
        
        // Drop animation: Slide down from above for the first 10 frames
        int elapsed = a->durationFrames - a->lifeFrames;
        if (elapsed < 10) {
            ay -= (10 - elapsed) * 4;
            // Dithered shadow on the ground
            LCDBitmap* sImg = images_get_drop_shadow();
            if (sImg) {
                pd->graphics->setDrawMode(kDrawModeFillBlack);
                pd->graphics->drawBitmap(sImg, sx - 4, sy + 2, kBitmapUnflipped);
                pd->graphics->setDrawMode(kDrawModeCopy);
            }
        }
        
        if (aImg) pd->graphics->drawBitmap(aImg, ax, ay, kBitmapUnflipped);
        
        // Pulsing indicator (using a simple XOR square pulse to save on circle math)
        if ((game.frameCount + i * 7) % 20 < 10) {
            pd->graphics->setDrawMode(kDrawModeXOR);
            GFX_RECT(sx - 15, sy - 15, 30, 30, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // Crates
    for (int i = 0; i < crateCount; i++) {
        Crate* c = &crates[i];
        if (!c->alive) continue;
        int sx = (int)c->x - cx;
        int sy = (int)c->y - cy;
        LCDBitmap* cImg = images_get_crate();
        if (cImg) pd->graphics->drawBitmap(cImg, sx - 9, sy - 9, kBitmapUnflipped);
    }

    // Riptide vortexes (ground layer)
    for (int i = 0; i < riptideCount; i++) {
        Riptide* r = &riptides[i];
        if (!r->alive) continue;
        int sx = (int)r->x - cx;
        int sy = (int)r->y - cy;
        if (sx < -80 || sx > SCREEN_W + 80 || sy < -80 || sy > SCREEN_H + 80) continue;
        
        // Rotating dithered spiral mask (XOR for high visibility)
        pd->graphics->setDrawMode(kDrawModeXOR);
        LCDBitmap* vImg = images_get_vortex_mask();
        if (vImg) {
            LCDBitmapFlip flip = (game.frameCount % 2 == 0) ? kBitmapFlippedX : kBitmapUnflipped;
            pd->graphics->drawBitmap(vImg, sx - 24, sy - 24, flip);
        }
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // Depth charges (ground layer)
    for (int i = 0; i < depthChargeCount; i++) {
        DepthCharge* dc = &depthCharges[i];
        if (!dc->alive) continue;
        int sx = (int)dc->x - cx;
        int sy = (int)dc->y - cy;
        if (sx < -60 || sx > SCREEN_W + 60 || sy < -60 || sy > SCREEN_H + 60) continue;

        if (!dc->detonated) {
            // Draw mine sprite (blink rapidly near end)
            int blink = (dc->fuseFrames < 10 && (dc->fuseFrames % 2 == 0));
            if (!blink) {
                LCDBitmap* dcImg = images_get_depth_charge();
                if (dcImg) pd->graphics->drawBitmap(dcImg, sx - 4, sy - 4, kBitmapUnflipped);
            }
            // Warning ring pulses outward
            int maxP = 10;
            int pFrame = (30 - dc->fuseFrames) % maxP;
            float rFrac = (float)pFrame / (float)maxP;
            int wr = (int)(dc->blastRadius * rFrac);
            if (wr > 2) {
                pd->graphics->setDrawMode(kDrawModeXOR);
                GFX_CIRCLE(sx - wr, sy - wr, wr * 2, wr * 2, 1, kColorWhite);
                pd->graphics->setDrawMode(kDrawModeCopy);
            }
        } else {
            // Slow field: dithered circle border
            int ri = (int)(dc->blastRadius * 0.8f);
            pd->graphics->setDrawMode(kDrawModeXOR);
            GFX_CIRCLE(sx - ri, sy - ri, ri * 2, ri * 2, 1, kColorWhite);
            
            // Inward flowing rings for the pull/slow effect
            int wave = ri - ((game.frameCount * 2) % ri);
            if (wave > 0) GFX_CIRCLE(sx - wave, sy - wave, wave * 2, wave * 2, 1, kColorWhite);
            
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // Pickups (enemy drops)
    for (int i = 0; i < pickupCount; i++) {
        Pickup* p = &pickups[i];
        if (!p->alive) continue;
        int sx = (int)p->x - cx;
        int sy = (int)p->y - cy;
        if (sx < -15 || sx > SCREEN_W + 15 || sy < -15 || sy > SCREEN_H + 15) continue;
        // Pulsing draw mode
        int pulse = (game.frameCount / 6) % 2;
        if (pulse) pd->graphics->setDrawMode(kDrawModeXOR);
        LCDBitmap* pImg = images_get_pickup(p->type);
        if (pImg) pd->graphics->drawBitmap(pImg, sx - 6, sy - 6, kBitmapUnflipped);
        if (pulse) pd->graphics->setDrawMode(kDrawModeCopy);
        // Attraction sparkle when close to player
        float dx = player.x - p->x;
        float dy = player.y - p->y;
        if (dx * dx + dy * dy < 6400.0f && game.frameCount % 4 == 0) {
            entities_spawn_particles(p->x, p->y, 1, 0);
        }
    }

    // XP gems
    for (int i = 0; i < xpGemCount; i++) {
        if (!xpGems[i].alive) continue;
        int sx = (int)xpGems[i].x - cx;
        int sy = (int)xpGems[i].y - cy;
        if (sx < -10 || sx > SCREEN_W + 10 || sy < -10 || sy > SCREEN_H + 10) continue;
        int small = (game.frameCount / 10) % 2;
        LCDBitmap* gemImg = images_get_xp_gem(small);
        int gw = small ? 5 : 6;
        pd->graphics->drawBitmap(gemImg, sx - gw, sy - gw, kBitmapUnflipped);
    }

    // Enemies
    for (int i = 0; i < enemyCount; i++) {
        Enemy* e = &enemies[i];
        if (!e->alive) continue;
        int sx = (int)e->x - cx;
        int sy = (int)e->y - cy;
        if (sx < -60 || sx > SCREEN_W + 60 || sy < -60 || sy > SCREEN_H + 60) continue;

        int hw = images_get_enemy_half_w(e->type);
        int hh = images_get_enemy_half_h(e->type);
        int bob = (e->bobTimer / 8) % 2 == 0 ? 0 : -1;

        {
            LCDBitmapFlip flip = (e->animFrame % 24 >= 12) ? kBitmapFlippedX : kBitmapUnflipped;
            int animF = (e->animFrame / 15) & 1;
            LCDBitmap* img = images_get_enemy(e->type, animF);
            if (e->flashTimer > 0) {
                // Draw sprite inverted for hit flash
                pd->graphics->setDrawMode(kDrawModeInverted);
                if (img) pd->graphics->drawBitmap(img, sx - hw, sy - hh + bob, flip);
                pd->graphics->setDrawMode(kDrawModeCopy);
            } else {
                if (img) pd->graphics->drawBitmap(img, sx - hw, sy - hh + bob, flip);
            }
        }

        // Seer charging telegraph
        if (e->charging) {
            GFX_CIRCLE(sx - hw - 3, sy - hh - 3, hw * 2 + 6, hh * 2 + 6, 1, kColorWhite);
        }
        // Harbinger aura ring
        if (e->type == ENEMY_HARBINGER && (e->bobTimer / 6) % 2 == 0) {
            GFX_CIRCLE(sx - 45, sy - 45, 90, 90, 1, kColorWhite);
        }
    }

    // Tide Pool orbs (drawn with XOR on top of enemies for visibility)
    {
        int tpIdx = -1;
        for (int i = 0; i < player.weaponCount; i++) {
            if (player.weapons[i].id == WEAPON_TIDE_POOL) { tpIdx = i; break; }
        }
        if (tpIdx >= 0 && !player.dead) {
            int lv = player.weapons[tpIdx].level;
            int orbCount[] = { 2, 3, 3 };
            float radius[] = { 40.0f, 52.0f, 60.0f };
            float speed[] = { 0.08f, 0.10f, 0.12f };
            int li = lv - 1;
            float twoPi = 2.0f * 3.14159265f;

            pd->graphics->setDrawMode(kDrawModeXOR);
            for (int o = 0; o < orbCount[li]; o++) {
                float a = game.tidePoolAngle + o * (twoPi / orbCount[li]);
                float ox = player.x + cosf(a) * radius[li];
                float oy = player.y + sinf(a) * radius[li];
                
                // Buoyant vertical bobbing (aesthetic trick)
                float bob = sinf((float)game.frameCount * 0.15f + o) * 3.0f;
                
                int osx = (int)ox - cx;
                int osy = (int)oy - cy + (int)bob;
                
                GFX_FILL(osx - 2, osy - 2, 4, 4, kColorWhite);
            }
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // Player
    if (!player.dead) {
        int px = (int)player.x - cx;
        int py = (int)player.y - cy;
        uint32_t now = pd->system->getCurrentTimeMilliseconds();
        int visible = 1;
        if (now < player.invulnUntil) {
            visible = ((now / 100) % 2 == 0);
        }
        if (visible) {
            LCDBitmap* pImg = images_get_player_frame(player.frameIdx);
            if (pImg) pd->graphics->drawBitmap(pImg, px - 15, py - 15, kBitmapUnflipped);
        }
        // Salt Ward shield pips above player
        if (player.saltWardShield > 0) {
            int totalW = player.saltWardShield * 6 - 2;
            int startX = px - totalW / 2;
            for (int s = 0; s < player.saltWardShield; s++) {
                GFX_FILL(startX + s * 6, py - 22, 4, 4, kColorWhite);
            }
        }
    }

    // Bullets
    for (int i = 0; i < bulletCount; i++) {
        Bullet* b = &bullets[i];
        if (!b->alive) continue;
        int sx = (int)b->x - cx;
        int sy = (int)b->y - cy;
        if (sx < -10 || sx > SCREEN_W + 10 || sy < -10 || sy > SCREEN_H + 10) continue;
        LCDBitmap* bImg;
        if (b->imageId == 1) {
            bImg = images_get_harpoon();
            // Harpoon "Rope" effect (jagged line toward player)
            pd->graphics->setDrawMode(kDrawModeXOR);
            int px = (int)player.x - cx;
            int py = (int)player.y - cy;
            // Draw a slightly jagged rope
            int mx = (sx + px) / 2 + rng_range(-2, 2);
            int my = (sy + py) / 2 + rng_range(-2, 2);
            pd->graphics->drawLine(sx, sy, mx, my, 1, kColorWhite);
            pd->graphics->drawLine(mx, my, px, py, 1, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
        else if (b->imageId == 2) {
            bImg = images_get_wisp_sprite();
            // Ghost Light special: XOR flicker offset
            pd->graphics->setDrawMode(kDrawModeXOR);
            if (game.frameCount % 2 == 0) sx += 1;
        }
        else {
            bImg = images_get_bullet();
            if (rng_range(0, 100) < 30) sx += rng_range(-1, 1);
        }
        
        if (bImg) {
            pd->graphics->drawBitmap(bImg, sx - 6, sy - 2, kBitmapUnflipped);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // Bullet trails (Optimized: 2 dots instead of 3, XOR mode for visibility)
    pd->graphics->setDrawMode(kDrawModeXOR);
    for (int i = 0; i < bulletCount; i++) {
        Bullet* b = &bullets[i];
        if (!b->alive) continue;
        float vlen_sq = b->vx * b->vx + b->vy * b->vy;
        if (vlen_sq > 1.0f) {
            float vlen = sqrtf(vlen_sq);
            float nx = -b->vx / vlen;
            float ny = -b->vy / vlen;
            GFX_FILL((int)(b->x + nx * 6.0f) - cx, (int)(b->y + ny * 6.0f) - cy, 2, 2, kColorWhite);
            GFX_FILL((int)(b->x + nx * 11.0f) - cx, (int)(b->y + ny * 11.0f) - cy, 1, 1, kColorWhite);
        }
    }
    pd->graphics->setDrawMode(kDrawModeCopy);

    // Chain lightning arcs (Optimized: 2 jagged segments + XOR spark)
    pd->graphics->setDrawMode(kDrawModeXOR);
    LCDBitmap* bSpr = images_get_bolt_sprite();
    for (int i = 0; i < MAX_CHAIN_VISUALS; i++) {
        ChainVisual* cv = &chainVisuals[i];
        if (cv->life <= 0) continue;
        int x1 = (int)cv->x1 - cx;
        int y1 = (int)cv->y1 - cy;
        int x2 = (int)cv->x2 - cx;
        int y2 = (int)cv->y2 - cy;
        
        // Single midpoint jitter instead of two
        int mx = (x1 + x2) / 2 + rng_range(-6, 6);
        int my = (y1 + y2) / 2 + rng_range(-6, 6);
        
        pd->graphics->drawLine(x1, y1, mx, my, 2, kColorWhite);
        pd->graphics->drawLine(mx, my, x2, y2, 2, kColorWhite);
        
        // Draw spark sprite at destination
        if (bSpr) pd->graphics->drawBitmap(bSpr, x2 - 8, y2 - 4, kBitmapUnflipped);
        
        cv->life--;
    }
    pd->graphics->setDrawMode(kDrawModeCopy);

    // Enemy bullets
    for (int i = 0; i < enemyBulletCount; i++) {
        EnemyBullet* b = &enemyBullets[i];
        if (!b->alive) continue;
        int sx = (int)b->x - cx;
        int sy = (int)b->y - cy;
        LCDBitmap* ebImg = images_get_enemy_bullet();
        if (ebImg) pd->graphics->drawBitmap(ebImg, sx - 5, sy - 5, kBitmapUnflipped);
    }

    // Brine Splash visual (Churning dithered pool)
    if (game.brineSplash.active) {
        game.brineSplash.frame++;
        int t = game.brineSplash.frame;
        int bsx = (int)game.brineSplash.x - cx;
        int bsy = (int)game.brineSplash.y - cy;
        
        LCDBitmap* poolImg = images_get_vortex_mask();
        if (poolImg) {
            pd->graphics->setDrawMode(kDrawModeXOR);
            LCDBitmapFlip flip = (game.frameCount % 2 == 0) ? kBitmapFlippedX : kBitmapUnflipped;
            pd->graphics->drawBitmap(poolImg, bsx - 24, bsy - 24, flip);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
        
        if (t >= 4) game.brineSplash.active = 0;
    }

    // Foghorn visual (expanding spectral ripple)
    if (game.foghornVisual.active) {
        game.foghornVisual.frame++;
        int t = game.foghornVisual.frame;
        int fsx = (int)game.foghornVisual.x - cx;
        int fsy = (int)game.foghornVisual.y - cy;
        
        LCDBitmap* ripImg = images_get_ripple_mask();
        if (ripImg) {
            pd->graphics->setDrawMode(kDrawModeXOR);
            // Construct a full ripple from 4 mirrored dithered quadrants
            pd->graphics->drawBitmap(ripImg, fsx - 16, fsy - 16, kBitmapUnflipped);
            pd->graphics->drawBitmap(ripImg, fsx, fsy - 16, kBitmapFlippedX);
            pd->graphics->drawBitmap(ripImg, fsx - 16, fsy, kBitmapFlippedY);
            pd->graphics->drawBitmap(ripImg, fsx, fsy, (LCDBitmapFlip)(kBitmapFlippedX | kBitmapFlippedY));
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
        
        if (t >= 6) game.foghornVisual.active = 0;
    }

    // Particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (p->life > 0) {
            p->x += p->vx;
            p->y += p->vy;
            p->life--;
            GFX_FILL((int)p->x - cx, (int)p->y - cy, p->sz, p->sz, kColorWhite);
        }
    }

    // FX animations
    for (int i = 0; i < MAX_FX; i++) {
        FXInstance* fx = &activeFX[i];
        if (!fx->active) continue;
        fx->timer++;
        if (fx->timer >= fx->speed) {
            fx->timer = 0;
            fx->frameIdx++;
            if (fx->frameIdx >= 4) { fx->active = 0; continue; }
        }
        int sx = (int)fx->x - cx;
        int sy = (int)fx->y - cy;
        
        if (fx->fxType == 1) {
            // Impact spark / muzzle flash (expanding star)
            int ext = 4 + fx->frameIdx * 3;
            int gap = fx->frameIdx * 4;
            // Diagonal lines
            pd->graphics->drawLine(sx - ext, sy - ext, sx - gap, sy - gap, 1, kColorWhite);
            pd->graphics->drawLine(sx + gap, sy + gap, sx + ext, sy + ext, 1, kColorWhite);
            pd->graphics->drawLine(sx + ext, sy - ext, sx + gap, sy - gap, 1, kColorWhite);
            pd->graphics->drawLine(sx - gap, sy + gap, sx - ext, sy + ext, 1, kColorWhite);
            // Orthogonal lines (only visible in early frames)
            if (fx->frameIdx < 2) {
                pd->graphics->drawLine(sx - ext, sy, sx - gap, sy, 1, kColorWhite);
                pd->graphics->drawLine(sx + gap, sy, sx + ext, sy, 1, kColorWhite);
                pd->graphics->drawLine(sx, sy - ext, sx, sy - gap, 1, kColorWhite);
                pd->graphics->drawLine(sx, sy + gap, sx, sy + ext, 1, kColorWhite);
            }
        } else {
            // Death burst ring or generic pulse (fxType 0)
            int rad = 6 + fx->frameIdx * 3;
            pd->graphics->setDrawMode(kDrawModeXOR);
            GFX_CIRCLE(sx - rad, sy - rad, rad * 2, rad * 2, 1, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // Damage numbers (floating combat text)
    {
        int cx2 = (int)game.cameraX - game.shakeX;
        int cy2 = (int)game.cameraY - game.shakeY;
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        for (int i = 0; i < MAX_DMG_NUMBERS; i++) {
            DamageNumber* dn = &game.dmgNumbers[i];
            if (dn->life <= 0) continue;
            int dx = (int)dn->x - cx2;
            int dy = (int)dn->y - cy2;
            if (dx < -20 || dx > SCREEN_W + 20 || dy < -20 || dy > SCREEN_H + 20) {
                dn->life = 0;
                continue;
            }
            char buf[16];
            if (dn->crit)
                snprintf(buf, sizeof(buf), "%d!", dn->value);
            else
                snprintf(buf, sizeof(buf), "%d", dn->value);
            // Thicker shadow for readability (draw black at all 8 directions)
            size_t slen = strlen(buf);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            for(int ox=-1; ox<=1; ox++) {
                for(int oy=-1; oy<=1; oy++) {
                    if(ox==0 && oy==0) continue;
                    pd->graphics->drawText(buf, slen, kASCIIEncoding, dx + ox, dy + oy);
                }
            }
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            pd->graphics->drawText(buf, slen, kASCIIEncoding, dx, dy);
            dn->y -= 2.25f;
            dn->life--;
        }
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // Cursed state XOR ring around player
    if (!player.dead && player.cursedTimer > 0) {
        int px = (int)player.x - cx;
        int py = (int)player.y - cy;
        float pulse = sinf((float)game.frameCount * 0.3f) * 3.0f;
        int r = (int)(30.0f + pulse);
        pd->graphics->setDrawMode(kDrawModeXOR);
        GFX_CIRCLE(px - r, py - r, r * 2, r * 2, 1, kColorWhite);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // Invulnerability concentric rings
    if (!player.dead) {
        uint32_t now = pd->system->getCurrentTimeMilliseconds();
        if (now < player.invulnUntil) {
            int px = (int)player.x - cx;
            int py = (int)player.y - cy;
            int pulse = (game.frameCount / 3) % 2;
            int r1 = 21 + pulse;
            int r2 = 27 + pulse;
            pd->graphics->setDrawMode(kDrawModeXOR);
            GFX_CIRCLE(px - r1, py - r1, r1 * 2, r1 * 2, 1, kColorWhite);
            GFX_CIRCLE(px - r2, py - r2, r2 * 2, r2 * 2, 1, kColorWhite);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // Low HP vignette (dithered black border at 1 HP)
    if (!player.dead && player.hp == 1) {
        for (int y = 0; y < SCREEN_H; y++) {
            for (int x = 0; x < 12; x++) {
                if ((x + y) % 2 == 0) GFX_FILL(x, y, 1, 1, kColorBlack);
            }
            for (int x = SCREEN_W - 12; x < SCREEN_W; x++) {
                if ((x + y) % 2 == 0) GFX_FILL(x, y, 1, 1, kColorBlack);
            }
        }
        for (int x = 12; x < SCREEN_W - 12; x++) {
            for (int y = 0; y < 12; y++) {
                if ((x + y) % 2 == 0) GFX_FILL(x, y, 1, 1, kColorBlack);
            }
            for (int y = SCREEN_H - 12; y < SCREEN_H; y++) {
                if ((x + y) % 2 == 0) GFX_FILL(x, y, 1, 1, kColorBlack);
            }
        }
    }

    // Streak flash overlay
    if (game.streakFlashTimer > 0) {
        game.streakFlashTimer--;
        // Dithered white overlay (checkerboard)
        for (int y = 0; y < SCREEN_H; y += 2) {
            for (int x = 0; x < SCREEN_W; x += 2) {
                GFX_FILL(x, y, 1, 1, kColorWhite);
            }
        }
    }

    // Surge overlay text
    if (game.surgeOverlayTimer > 0) {
        game.surgeOverlayTimer--;
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        ui_draw_centered_text("SURGE!", SCREEN_H / 2 - 20);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // HUD + announcements (screen space)
    ui_draw_hud();
    ui_draw_tier_announcement();
    ui_draw_crate_reward();

    // Vignette (Lantern light) overlay
    LCDBitmap* vImg = images_get_vignette();
    if (vImg) {
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawBitmap(vImg, 0, 0, kBitmapUnflipped);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }
}

void rendering_spawn_dmg_number(float x, float y, int value, int crit)
{
    DamageNumber* dn = &game.dmgNumbers[game.dmgNumberIdx];
    dn->x = x;
    dn->y = y;
    dn->value = value;
    dn->life = 20;
    dn->crit = (uint8_t)crit;
    game.dmgNumberIdx = (game.dmgNumberIdx + 1) % MAX_DMG_NUMBERS;
}

void rendering_draw_background(void)
{
    if (bgBitmap) pd->graphics->drawBitmap(bgBitmap, -(int)game.cameraX, -(int)game.cameraY, kBitmapUnflipped);
}
