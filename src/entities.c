#include "game.h"

// ---------------------------------------------------------------------------
// Entity pools
// ---------------------------------------------------------------------------
Enemy enemies[MAX_ENEMIES];
int enemyCount = 0;

Bullet bullets[MAX_BULLETS];
int bulletCount = 0;

EnemyBullet enemyBullets[MAX_ENEMY_BULLETS];
int enemyBulletCount = 0;

XPGem xpGems[MAX_XP_GEMS];
int xpGemCount = 0;

Anchor anchors[MAX_ANCHORS];
int anchorCount = 0;

Crate crates[MAX_CRATES];
int crateCount = 0;

Pickup pickups[MAX_PICKUPS];
int pickupCount = 0;

Riptide riptides[MAX_RIPTIDES];
int riptideCount = 0;

DepthCharge depthCharges[MAX_DEPTH_CHARGES];
int depthChargeCount = 0;

ChainVisual chainVisuals[MAX_CHAIN_VISUALS];
int chainVisualIdx = 0;

Particle particles[MAX_PARTICLES];
int particleIdx = 0;

FXInstance activeFX[MAX_FX];
int fxIdx = 0;

// ---------------------------------------------------------------------------
// Init: zero all pools
// ---------------------------------------------------------------------------
void entities_init(void)
{
    memset(enemies, 0, sizeof(enemies));
    enemyCount = 0;
    memset(bullets, 0, sizeof(bullets));
    bulletCount = 0;
    memset(enemyBullets, 0, sizeof(enemyBullets));
    enemyBulletCount = 0;
    memset(xpGems, 0, sizeof(xpGems));
    xpGemCount = 0;
    memset(anchors, 0, sizeof(anchors));
    anchorCount = 0;
    memset(crates, 0, sizeof(crates));
    crateCount = 0;
    memset(pickups, 0, sizeof(pickups));
    pickupCount = 0;
    memset(riptides, 0, sizeof(riptides));
    riptideCount = 0;
    memset(depthCharges, 0, sizeof(depthCharges));
    depthChargeCount = 0;
    memset(chainVisuals, 0, sizeof(chainVisuals));
    chainVisualIdx = 0;
    memset(particles, 0, sizeof(particles));
    particleIdx = 0;
    memset(activeFX, 0, sizeof(activeFX));
    fxIdx = 0;
}

// ---------------------------------------------------------------------------
// Spawn enemy — returns index or -1 if full
// ---------------------------------------------------------------------------
int entities_spawn_enemy(float x, float y, float speed, EnemyType type)
{
    if (enemyCount >= MAX_ENEMIES) return -1;

    Enemy* e = &enemies[enemyCount];
    memset(e, 0, sizeof(Enemy));
    e->x = x;
    e->y = y;
    e->speed = speed;
    e->hp = 2.0f; // caller should override
    e->type = type;
    e->alive = 1;
    e->slowFactor = 1.0f;
    e->phase = rng_float() * PI_F * 2.0f;
    e->bobTimer = rng_range(0, 15);
    e->animFrame = rng_range(0, 23);
    e->lungeTimer = rng_range(0, 90);
    if (type == ENEMY_SEER) {
        e->shotCooldown = rng_range(0, 30);
    }

    enemyCount++;
    return enemyCount - 1;
}

// ---------------------------------------------------------------------------
// Spawn bullet — returns index or -1 if full
// ---------------------------------------------------------------------------
int entities_spawn_bullet(float x, float y, float dx, float dy,
                          float dmg, float speed, int lifeFrames,
                          uint8_t piercing, uint8_t homing, float turnRate,
                          uint8_t retargets, uint8_t imageId)
{
    if (bulletCount >= MAX_BULLETS) return -1;

    Bullet* b = &bullets[bulletCount];
    memset(b, 0, sizeof(Bullet));
    b->x = x;
    b->y = y;
    b->vx = dx * speed;
    b->vy = dy * speed;
    b->speed = speed;
    b->dmg = dmg;
    b->lifeFrames = (int16_t)lifeFrames;
    b->alive = 1;
    b->piercing = piercing;
    b->homing = homing;
    b->turnRate = turnRate;
    b->retargets = retargets;
    b->homingTarget = -1;
    b->imageId = imageId;

    bulletCount++;
    return bulletCount - 1;
}

// ---------------------------------------------------------------------------
// Spawn enemy bullet
// ---------------------------------------------------------------------------
int entities_spawn_enemy_bullet(float x, float y, float dx, float dy)
{
    if (enemyBulletCount >= MAX_ENEMY_BULLETS) return -1;

    EnemyBullet* b = &enemyBullets[enemyBulletCount];
    memset(b, 0, sizeof(EnemyBullet));
    b->x = x;
    b->y = y;
    float speed = 3.0f;
    b->vx = dx * speed;
    b->vy = dy * speed;
    b->lifeFrames = 60; // 2 seconds
    b->alive = 1;

    enemyBulletCount++;
    return enemyBulletCount - 1;
}

// ---------------------------------------------------------------------------
// Spawn XP gem
// ---------------------------------------------------------------------------
int entities_spawn_xp_gem(float x, float y, int value)
{
    if (xpGemCount >= MAX_XP_GEMS) return -1;

    XPGem* g = &xpGems[xpGemCount];
    memset(g, 0, sizeof(XPGem));
    g->x = x;
    g->y = y;
    g->value = value;
    g->lifeFrames = 300; // ~10 seconds
    g->alive = 1;

    xpGemCount++;
    return xpGemCount - 1;
}

// ---------------------------------------------------------------------------
// Spawn pickup (enemy drop)
// ---------------------------------------------------------------------------
int entities_spawn_pickup(float x, float y)
{
    if (pickupCount >= MAX_PICKUPS) return -1;

    // Roll pickup type
    int roll = rng_range(1, 100);
    PickupType type;
    if (roll <= 15) {
        type = PICKUP_WEAPON_UPGRADE;
    } else if (roll <= 30) {
        type = PICKUP_NEW_WEAPON;
    } else if (roll <= 70) {
        type = PICKUP_HEALTH;
    } else {
        type = PICKUP_XP_BURST;
    }

    Pickup* p = &pickups[pickupCount];
    memset(p, 0, sizeof(Pickup));
    p->x = x;
    p->y = y;
    p->baseY = y;
    p->lifeFrames = 300; // 10 seconds
    p->bobFrame = 0;
    p->alive = 1;
    p->type = type;

    pickupCount++;
    return pickupCount - 1;
}

// ---------------------------------------------------------------------------
// Spawn particles (ring buffer)
// ---------------------------------------------------------------------------
void entities_spawn_particles(float x, float y, int count, int big)
{
    for (int i = 0; i < count; i++) {
        Particle* p = &particles[particleIdx];
        float angle = rng_float() * PI_F * 2.0f;
        float spd = 0.5f + rng_float() * (big ? 2.5f : 1.5f);
        p->x = x;
        p->y = y;
        p->vx = cosf(angle) * spd;
        p->vy = sinf(angle) * spd;
        p->life = 6;
        p->sz = big ? (uint8_t)rng_range(3, 5) : 3;
        particleIdx = (particleIdx + 1) % MAX_PARTICLES;
    }
}

// ---------------------------------------------------------------------------
// Spawn FX (ring buffer)
// ---------------------------------------------------------------------------
void entities_spawn_fx(float x, float y, int fxType, int speed)
{
    FXInstance* fx = &activeFX[fxIdx];
    fx->x = x;
    fx->y = y;
    fx->fxType = (uint8_t)fxType;
    fx->frameIdx = 0;
    fx->timer = 0;
    fx->speed = (uint8_t)speed;
    fx->active = 1;
    fxIdx = (fxIdx + 1) % MAX_FX;
}

// ---------------------------------------------------------------------------
// Cleanup dead entities (swap-and-pop)
// ---------------------------------------------------------------------------
void entities_cleanup_dead(void)
{
    // Enemies
    int i = 0;
    while (i < enemyCount) {
        if (!enemies[i].alive) {
            enemies[i] = enemies[enemyCount - 1];
            enemyCount--;
        } else {
            i++;
        }
    }

    // Bullets
    i = 0;
    while (i < bulletCount) {
        if (!bullets[i].alive) {
            bullets[i] = bullets[bulletCount - 1];
            bulletCount--;
        } else {
            i++;
        }
    }

    // Enemy bullets
    i = 0;
    while (i < enemyBulletCount) {
        if (!enemyBullets[i].alive) {
            enemyBullets[i] = enemyBullets[enemyBulletCount - 1];
            enemyBulletCount--;
        } else {
            i++;
        }
    }

    // XP gems
    i = 0;
    while (i < xpGemCount) {
        if (!xpGems[i].alive) {
            xpGems[i] = xpGems[xpGemCount - 1];
            xpGemCount--;
        } else {
            i++;
        }
    }

    // Pickups
    i = 0;
    while (i < pickupCount) {
        if (!pickups[i].alive) {
            pickups[i] = pickups[pickupCount - 1];
            pickupCount--;
        } else {
            i++;
        }
    }

    // Riptides
    i = 0;
    while (i < riptideCount) {
        if (!riptides[i].alive) {
            riptides[i] = riptides[riptideCount - 1];
            riptideCount--;
        } else {
            i++;
        }
    }

    // Depth charges
    i = 0;
    while (i < depthChargeCount) {
        if (!depthCharges[i].alive) {
            depthCharges[i] = depthCharges[depthChargeCount - 1];
            depthChargeCount--;
        } else {
            i++;
        }
    }
}
