#include "game.h"

// ---------------------------------------------------------------------------
// Binary save format
// ---------------------------------------------------------------------------
#define SAVE_VERSION 2

typedef struct {
    uint32_t version;
    int32_t highScore;
    float bestTime;
    uint8_t unlockedWeapons[WEAPON_COUNT];
    uint8_t unlockedEnemies[ENEMY_TYPE_COUNT];
} SaveData;

void save_load(void)
{
    SDFile* f = pd->file->open("save.bin", kFileRead);
    if (!f) {
        // Defaults
        game.highScore = 0;
        game.bestTime = 0.0f;
        memset(game.unlockedWeapons, 0, sizeof(game.unlockedWeapons));
        memset(game.unlockedEnemies, 0, sizeof(game.unlockedEnemies));
        game.unlockedWeapons[WEAPON_SIGNAL_BEAM] = 1;
        game.unlockedEnemies[ENEMY_CREEPER] = 1;
        DLOG("No save file found, using defaults");
        return;
    }

    SaveData data;
    int bytesRead = pd->file->read(f, &data, sizeof(SaveData));
    pd->file->close(f);

    if (bytesRead == sizeof(SaveData) && data.version == SAVE_VERSION) {
        game.highScore = data.highScore;
        game.bestTime = data.bestTime;
        memcpy(game.unlockedWeapons, data.unlockedWeapons, sizeof(data.unlockedWeapons));
        memcpy(game.unlockedEnemies, data.unlockedEnemies, sizeof(data.unlockedEnemies));
        DLOG("Save loaded: highScore=%d bestTime=%.1f", game.highScore, (double)game.bestTime);
    } else {
        DLOG("Save version mismatch or corrupt, using defaults");
        game.highScore = 0;
        game.bestTime = 0.0f;
        game.unlockedWeapons[WEAPON_SIGNAL_BEAM] = 1;
        game.unlockedEnemies[ENEMY_CREEPER] = 1;
    }
}

void save_write(void)
{
    SaveData data;
    data.version = SAVE_VERSION;
    data.highScore = game.highScore;
    data.bestTime = game.bestTime;
    memcpy(data.unlockedWeapons, game.unlockedWeapons, sizeof(data.unlockedWeapons));
    memcpy(data.unlockedEnemies, game.unlockedEnemies, sizeof(data.unlockedEnemies));

    SDFile* f = pd->file->open("save.bin", kFileWrite);
    if (f) {
        pd->file->write(f, &data, sizeof(SaveData));
        pd->file->close(f);
        DLOG("Save written");
    } else {
        DLOG("ERROR: Could not open save.bin for writing");
    }
}

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
