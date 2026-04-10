#ifndef KEEPER_H
#define KEEPER_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Keeper (character) definitions
// ---------------------------------------------------------------------------

typedef enum {
    KEEPER_DEFAULT = 0,    // The Keeper — balanced
    KEEPER_DREDGER,        // The Dredger — AoE focus
    KEEPER_WRECKER,        // The Wrecker — damage focus
    KEEPER_LAMPLIGHTER,    // The Lamplighter — homing focus
    KEEPER_COUNT
} KeeperId;

typedef struct {
    const char* name;
    const char* desc;
    float speedMult;       // multiplied into base move speed
    float dmgMult;         // multiplied into all damage
    float aoeMult;         // multiplied into AoE radii
    int hpBonus;           // added to base max HP
    int starterWeapon;     // WeaponId for starting weapon
    int unlockCost;        // Salvage cost to unlock (0 = free)
} KeeperDef;

extern const KeeperDef KEEPER_DEFS[KEEPER_COUNT];

#endif // KEEPER_H
