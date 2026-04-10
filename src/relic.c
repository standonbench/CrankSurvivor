#include "game.h"
#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Relic definitions
// ---------------------------------------------------------------------------
const RelicDef RELIC_DEFS[RELIC_COUNT] = {
    [RELIC_CURSED_LANTERN] = {
        .name     = "Cursed Lantern",
        .desc     = "1.5x weapon damage; 1.5x damage taken.",
        .logEntry = "The lantern burns bright — and so do you."
    },
    [RELIC_DROWNED_BELL] = {
        .name     = "The Drowned Bell",
        .desc     = "No cooldowns for first 30s; all cooldowns +30% after.",
        .logEntry = "The bell tolled once. Now it costs."
    },
    [RELIC_WAILING_CURRENT] = {
        .name     = "Wailing Current",
        .desc     = "XP gems spawn at your position on kill.",
        .logEntry = "Stay close. The tide brings what the dead leave."
    },
    [RELIC_FATHOM_DEBT] = {
        .name     = "Fathom Debt",
        .desc     = "Taking damage charges next shot for 5x damage.",
        .logEntry = "Pain is currency. Spend it wisely."
    },
    [RELIC_BONE_COMPASS] = {
        .name     = "Bone Compass",
        .desc     = "Harpoon pierces 3 targets; max 3 weapons this run.",
        .logEntry = "It points toward something. Best not to ask."
    },
    [RELIC_TIDEBREAKER] = {
        .name     = "The Tidebreaker",
        .desc     = "Aura weapons 1.5x radius; projectile damage -20%.",
        .logEntry = "The sea yields to it. Bullets do not."
    },
    [RELIC_DROWNED_COMPASS] = {
        .name     = "Drowned Compass",
        .desc     = "Enemies beeline straight at you; enemy HP -30%.",
        .logEntry = "They know where you are. They always did."
    },
    [RELIC_BLACK_PEARL] = {
        .name     = "Black Pearl",
        .desc     = "Start with Lighthouse Lens; no passives this run.",
        .logEntry = "Everything gained. Everything else, forfeit."
    },
    [RELIC_PALE_TIDE] = {
        .name     = "Pale Tide",
        .desc     = "All weapons fire 2x faster; damage -40%.",
        .logEntry = "Quantity over quality. The pale tide never stops."
    },
    [RELIC_KEEPERS_DOOM] = {
        .name     = "The Keeper's Doom",
        .desc     = "Max HP capped at 3; XP x(1 + missing HP).",
        .logEntry = "The flame is brightest just before it goes out."
    },
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
const char* relic_get_name(RelicId id)
{
    if (id < 0 || id >= RELIC_COUNT) return "No Relic";
    return RELIC_DEFS[id].name;
}

const char* relic_get_desc(RelicId id)
{
    if (id < 0 || id >= RELIC_COUNT) return "";
    return RELIC_DEFS[id].desc;
}

// ---------------------------------------------------------------------------
// Draw relic selection screen
// ---------------------------------------------------------------------------
void ui_draw_relic_select(void)
{
    GFX_CLEAR(kColorBlack);

    // Wave-like scanline pattern (reuse upgrade screen aesthetic)
    for (int y = 0; y < SCREEN_H; y += 8) {
        int wx = (int)(sinf((float)y * 0.08f + game.frameCount * 0.05f) * 3.0f);
        GFX_FILL(wx, y, SCREEN_W - wx * 2, 1, kColorWhite);
    }

    // Title
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    GFX_CLEAR(kColorBlack);

    // Black top bar for title readability
    GFX_FILL(0, 0, SCREEN_W, 36, kColorBlack);

    const char* title = "RELIC OF THE TIDE";
    int tw = 0;
    if (game.fontBold) tw = pd->graphics->getTextWidth(game.fontBold, title, strlen(title), kASCIIEncoding, 0);
    pd->graphics->drawText(title, strlen(title), kASCIIEncoding, (SCREEN_W - tw) / 2, 6);

    const char* sub = "Crank to choose  \x81 to accept";
    int sw = 0;
    if (game.fontBold) sw = pd->graphics->getTextWidth(game.fontBold, sub, strlen(sub), kASCIIEncoding, 0);
    pd->graphics->drawText(sub, strlen(sub), kASCIIEncoding, (SCREEN_W - sw) / 2, 22);

    GFX_LINE(0, 36, SCREEN_W, 36, 1, kColorWhite);

    // Relic card
    RelicId rid = game.relicOptions[game.relicSelectIndex];
    int cx = 20, cy = 45, cw = SCREEN_W - 40, ch = 140;

    GFX_FILL(cx, cy, cw, ch, kColorBlack);
    GFX_RECT(cx, cy, cw, ch, kColorWhite);
    GFX_RECT(cx + 2, cy + 2, cw - 4, ch - 4, kColorWhite);

    if (rid >= 0 && rid < RELIC_COUNT) {
        const char* rname = RELIC_DEFS[rid].name;
        int rnw = 0;
        if (game.fontBold) rnw = pd->graphics->getTextWidth(game.fontBold, rname, strlen(rname), kASCIIEncoding, 0);
        pd->graphics->drawText(rname, strlen(rname), kASCIIEncoding, cx + (cw - rnw) / 2, cy + 12);

        GFX_LINE(cx + 8, cy + 32, cx + cw - 8, cy + 32, 1, kColorWhite);

        const char* rdesc = RELIC_DEFS[rid].desc;
        // Word-wrap description across 3 lines at ~32 chars
        int dlen = (int)strlen(rdesc);
        if (dlen <= 36) {
            int ddw = 0;
            if (game.fontBold) ddw = pd->graphics->getTextWidth(game.fontBold, rdesc, dlen, kASCIIEncoding, 0);
            pd->graphics->drawText(rdesc, dlen, kASCIIEncoding, cx + (cw - ddw) / 2, cy + 42);
        } else {
            // Split at a space near the midpoint
            int mid = dlen / 2;
            while (mid > 0 && rdesc[mid] != ' ') mid--;
            pd->graphics->drawText(rdesc, mid, kASCIIEncoding, cx + 10, cy + 42);
            pd->graphics->drawText(rdesc + mid + 1, dlen - mid - 1, kASCIIEncoding, cx + 10, cy + 58);
        }

        // Log entry italic-style (use bold font, smaller indent)
        const char* log = RELIC_DEFS[rid].logEntry;
        int llen = (int)strlen(log);
        pd->graphics->drawText(log, llen, kASCIIEncoding, cx + 16, cy + 80);
    }

    // Dot indicators (3 options)
    int dotY = cy + ch + 12;
    int dotSpacing = 14;
    int dotStartX = SCREEN_W / 2 - dotSpacing;
    for (int d = 0; d < 3; d++) {
        int dx2 = dotStartX + d * dotSpacing;
        if (d == game.relicSelectIndex) {
            GFX_FILL(dx2 - 3, dotY - 3, 7, 7, kColorWhite);
        } else {
            GFX_RECT(dx2 - 3, dotY - 3, 7, 7, kColorWhite);
        }
    }
}

// ---------------------------------------------------------------------------
// Draw 3 random non-duplicate relics into relicOptions[]
// ---------------------------------------------------------------------------
void relic_init(void)
{
    game.relicOptions[0] = RELIC_NONE;
    game.relicOptions[1] = RELIC_NONE;
    game.relicOptions[2] = RELIC_NONE;

    int picked = 0;
    int attempts = 0;
    while (picked < 3 && attempts < 100) {
        attempts++;
        RelicId candidate = (RelicId)rng_range(0, RELIC_COUNT - 1);

        // Ensure no duplicates
        int dup = 0;
        for (int i = 0; i < picked; i++) {
            if (game.relicOptions[i] == candidate) { dup = 1; break; }
        }
        if (!dup) {
            game.relicOptions[picked++] = candidate;
        }
    }
    // Fallback: fill sequentially if RNG stalled
    for (int i = picked; i < 3; i++) {
        game.relicOptions[i] = (RelicId)i;
    }
}

// ---------------------------------------------------------------------------
// Apply relic effects at game start (after keeper modifiers)
// ---------------------------------------------------------------------------
void relic_apply(RelicId id)
{
    game.relicMaxWeapons = MAX_WEAPONS;

    switch (id) {
    case RELIC_BLACK_PEARL:
        player.lighthouseLens = 1;
        break;
    case RELIC_KEEPERS_DOOM:
        if (player.maxHp > 3) player.maxHp = 3;
        if (player.hp > 3)    player.hp = 3;
        break;
    case RELIC_BONE_COMPASS:
        game.relicMaxWeapons = 3;
        break;
    case RELIC_DROWNED_BELL:
        game.relicBellTimer = 30.0f;
        break;
    case RELIC_PALE_TIDE:
        for (int i = 0; i < player.weaponCount; i++) {
            if (player.weapons[i].cooldownMs > 0)
                player.weapons[i].cooldownMs /= 2;
        }
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Per-frame relic effects (called in STATE_PLAYING update)
// ---------------------------------------------------------------------------
void relic_update(void)
{
    switch (game.activeRelic) {
    case RELIC_DROWNED_BELL:
        if (game.relicBellTimer > 0.0f) {
            game.relicBellTimer -= FRAME_MS / 1000.0f;
            if (game.relicBellTimer < 0.0f) game.relicBellTimer = 0.0f;
        }
        break;
    case RELIC_KEEPERS_DOOM:
        if (player.maxHp > 3) player.maxHp = 3;
        if (player.hp > player.maxHp) player.hp = player.maxHp;
        break;
    default:
        break;
    }
}
