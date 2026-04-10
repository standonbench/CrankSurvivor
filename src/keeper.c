#include "keeper.h"

// WeaponId values (matching game.h enum order)
#define WID_SIGNAL_BEAM    0
#define WID_BRINE_SPLASH   3
#define WID_HARPOON        2
#define WID_GHOST_LIGHT    4

const KeeperDef KEEPER_DEFS[KEEPER_COUNT] = {
    [KEEPER_DEFAULT] = {
        .name         = "The Keeper",
        .desc         = "Balanced. No bonuses or penalties.",
        .speedMult    = 1.0f,
        .dmgMult      = 1.0f,
        .aoeMult      = 1.0f,
        .hpBonus      = 0,
        .starterWeapon = WID_SIGNAL_BEAM,
        .unlockCost   = 0,
    },
    [KEEPER_DREDGER] = {
        .name         = "The Dredger",
        .desc         = "+20% AoE size, -10% speed.",
        .speedMult    = 0.9f,
        .dmgMult      = 1.0f,
        .aoeMult      = 1.2f,
        .hpBonus      = 0,
        .starterWeapon = WID_BRINE_SPLASH,
        .unlockCost   = 1000,
    },
    [KEEPER_WRECKER] = {
        .name         = "The Wrecker",
        .desc         = "+15% damage, -1 max HP.",
        .speedMult    = 1.0f,
        .dmgMult      = 1.15f,
        .aoeMult      = 1.0f,
        .hpBonus      = -1,
        .starterWeapon = WID_HARPOON,
        .unlockCost   = 1500,
    },
    [KEEPER_LAMPLIGHTER] = {
        .name         = "The Lamplighter",
        .desc         = "+10% speed, start with Ghost Light.",
        .speedMult    = 1.1f,
        .dmgMult      = 1.0f,
        .aoeMult      = 1.0f,
        .hpBonus      = 0,
        .starterWeapon = WID_GHOST_LIGHT,
        .unlockCost   = 2000,
    },
};
