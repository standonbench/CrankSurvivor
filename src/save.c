#include "game.h"

// ---------------------------------------------------------------------------
// Binary save format — v3 adds meta-progression
// ---------------------------------------------------------------------------
#define SAVE_VERSION 3

typedef struct {
    uint32_t version;
    // Legacy fields
    int32_t  highScore;
    float    bestTime;
    uint8_t  unlockedWeapons[WEAPON_COUNT];
    uint8_t  unlockedEnemies[ENEMY_TYPE_COUNT];
    // Meta-progression (new in v3)
    uint32_t totalSalvage;
    uint32_t spentSalvage;
    uint32_t totalRuns;
    uint32_t totalKills;
    uint32_t bestTimeMs;
    uint32_t killsPerEnemy[ENEMY_TYPE_COUNT];
    uint32_t killsPerWeapon[WEAPON_COUNT];
    uint8_t  keeperUnlocked[KEEPER_COUNT];
    uint8_t  weaponStartUnlocked[WEAPON_COUNT];
    uint8_t  selectedKeeper;
    uint8_t  selectedStarterWeapon;
} SaveDataV3;

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------
static void save_set_defaults(void)
{
    game.highScore = 0;
    game.bestTime = 0.0f;
    memset(game.unlockedWeapons, 0, sizeof(game.unlockedWeapons));
    memset(game.unlockedEnemies, 0, sizeof(game.unlockedEnemies));
    game.unlockedWeapons[WEAPON_SIGNAL_BEAM] = 1;
    game.unlockedEnemies[ENEMY_CREEPER] = 1;

    game.totalSalvage = 0;
    game.spentSalvage = 0;
    game.totalRuns = 0;
    game.totalKills = 0;
    game.bestTimeMs = 0;
    memset(game.killsPerEnemy, 0, sizeof(game.killsPerEnemy));
    memset(game.killsPerWeapon, 0, sizeof(game.killsPerWeapon));
    memset(game.keeperUnlocked, 0, sizeof(game.keeperUnlocked));
    memset(game.weaponStartUnlocked, 0, sizeof(game.weaponStartUnlocked));
    game.keeperUnlocked[KEEPER_DEFAULT] = 1;
    game.weaponStartUnlocked[WEAPON_SIGNAL_BEAM] = 1;
    game.selectedKeeper = KEEPER_DEFAULT;
    game.selectedStarterWeapon = WEAPON_SIGNAL_BEAM;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------
void save_load(void)
{
    SDFile* f = pd->file->open("save.bin", kFileRead);
    if (!f) {
        save_set_defaults();
        DLOG("No save file found, using defaults");
        return;
    }

    SaveDataV3 data;
    int bytesRead = pd->file->read(f, &data, sizeof(SaveDataV3));
    pd->file->close(f);

    if (bytesRead == sizeof(SaveDataV3) && data.version == SAVE_VERSION) {
        game.highScore = data.highScore;
        game.bestTime = data.bestTime;
        memcpy(game.unlockedWeapons, data.unlockedWeapons, sizeof(data.unlockedWeapons));
        memcpy(game.unlockedEnemies, data.unlockedEnemies, sizeof(data.unlockedEnemies));
        game.totalSalvage = data.totalSalvage;
        game.spentSalvage = data.spentSalvage;
        game.totalRuns = data.totalRuns;
        game.totalKills = data.totalKills;
        game.bestTimeMs = data.bestTimeMs;
        memcpy(game.killsPerEnemy, data.killsPerEnemy, sizeof(data.killsPerEnemy));
        memcpy(game.killsPerWeapon, data.killsPerWeapon, sizeof(data.killsPerWeapon));
        memcpy(game.keeperUnlocked, data.keeperUnlocked, sizeof(data.keeperUnlocked));
        memcpy(game.weaponStartUnlocked, data.weaponStartUnlocked, sizeof(data.weaponStartUnlocked));
        game.selectedKeeper = data.selectedKeeper;
        game.selectedStarterWeapon = data.selectedStarterWeapon;
        DLOG("Save v3 loaded: salvage=%u runs=%u kills=%u",
             game.totalSalvage, game.totalRuns, game.totalKills);
    } else {
        DLOG("Save version mismatch or corrupt (%d bytes, ver=%u), resetting",
             bytesRead, bytesRead >= 4 ? data.version : 0);
        save_set_defaults();
    }
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------
void save_write(void)
{
    SaveDataV3 data;
    memset(&data, 0, sizeof(data));
    data.version = SAVE_VERSION;
    data.highScore = game.highScore;
    data.bestTime = game.bestTime;
    memcpy(data.unlockedWeapons, game.unlockedWeapons, sizeof(data.unlockedWeapons));
    memcpy(data.unlockedEnemies, game.unlockedEnemies, sizeof(data.unlockedEnemies));
    data.totalSalvage = game.totalSalvage;
    data.spentSalvage = game.spentSalvage;
    data.totalRuns = game.totalRuns;
    data.totalKills = game.totalKills;
    data.bestTimeMs = game.bestTimeMs;
    memcpy(data.killsPerEnemy, game.killsPerEnemy, sizeof(data.killsPerEnemy));
    memcpy(data.killsPerWeapon, game.killsPerWeapon, sizeof(data.killsPerWeapon));
    memcpy(data.keeperUnlocked, game.keeperUnlocked, sizeof(data.keeperUnlocked));
    memcpy(data.weaponStartUnlocked, game.weaponStartUnlocked, sizeof(data.weaponStartUnlocked));
    data.selectedKeeper = game.selectedKeeper;
    data.selectedStarterWeapon = game.selectedStarterWeapon;

    SDFile* f = pd->file->open("save.bin", kFileWrite);
    if (f) {
        pd->file->write(f, &data, sizeof(SaveDataV3));
        pd->file->close(f);
        DLOG("Save written (v3)");
    } else {
        DLOG("ERROR: Could not open save.bin for writing");
    }
}

// ---------------------------------------------------------------------------
// Update high score (called at end of run)
// ---------------------------------------------------------------------------
void save_update_high_score(void)
{
    int changed = 0;
    if (game.score > game.highScore) {
        game.highScore = game.score;
        changed = 1;
    }
    if (game.gameTime > game.bestTime) {
        game.bestTime = game.gameTime;
        changed = 1;
    }
    if (changed) save_write();
}

// ---------------------------------------------------------------------------
// End-of-run: calculate Salvage, update lifetime stats, persist
// ---------------------------------------------------------------------------
void save_end_run(void)
{
    // Calculate Salvage earned this run
    int kills = game.runKills;
    int timeSec = (int)game.gameTime;
    int scoreBonus = game.score / 50;

    // Base: 1 Salvage per 5 kills + 1 per 30 seconds survived + score/50
    int earned = kills / 5 + timeSec / 30 + scoreBonus;

    // Victory bonus: +50 Salvage
    if (game.gameTime >= VICTORY_TIME) earned += 50;

    // Minimum 1 Salvage per run (so players always feel progress)
    if (earned < 1) earned = 1;

    game.runSalvage = earned;
    game.totalSalvage += (uint32_t)earned;
    game.totalRuns++;
    game.totalKills += (uint32_t)kills;

    // Update best time in ms
    uint32_t timeMs = (uint32_t)(game.gameTime * 1000.0f);
    if (timeMs > game.bestTimeMs) game.bestTimeMs = timeMs;

    // Check for weapon unlock milestones (50 kills with weapon = unlock as starter)
    for (int i = 0; i < WEAPON_COUNT; i++) {
        if (!game.weaponStartUnlocked[i] && game.killsPerWeapon[i] >= 50) {
            game.weaponStartUnlocked[i] = 1;
            DLOG("Unlocked starter weapon: %d", i);
        }
    }

    // Check for keeper unlock affordability is handled in the shop UI,
    // but we auto-unlock based on achievement milestones too:
    // Dredger: 10 runs completed
    if (!game.keeperUnlocked[KEEPER_DREDGER] && game.totalRuns >= 10)
        game.keeperUnlocked[KEEPER_DREDGER] = 1;
    // Wrecker: 500 total kills
    if (!game.keeperUnlocked[KEEPER_WRECKER] && game.totalKills >= 500)
        game.keeperUnlocked[KEEPER_WRECKER] = 1;
    // Lamplighter: 1 victory
    if (!game.keeperUnlocked[KEEPER_LAMPLIGHTER] && game.gameTime >= VICTORY_TIME)
        game.keeperUnlocked[KEEPER_LAMPLIGHTER] = 1;

    save_update_high_score();
    save_write();

    DLOG("Run ended: kills=%d salvage=%d total_salvage=%u",
         kills, earned, game.totalSalvage);
}

// ---------------------------------------------------------------------------
// Salvage helpers
// ---------------------------------------------------------------------------
int save_get_salvage(void)
{
    return (int)(game.totalSalvage - game.spentSalvage);
}

int save_try_purchase(int cost)
{
    int available = save_get_salvage();
    if (available >= cost) {
        game.spentSalvage += (uint32_t)cost;
        save_write();
        return 1;
    }
    return 0;
}
