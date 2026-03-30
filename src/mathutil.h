#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <math.h>
#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Fast PRNG (xorshift32) — much faster than rand() on ARM
// ---------------------------------------------------------------------------
extern uint32_t _rng_state;

static inline void rng_seed(uint32_t seed) {
    _rng_state = seed ? seed : 1;
}

static inline uint32_t rng_next(void) {
    _rng_state ^= _rng_state << 13;
    _rng_state ^= _rng_state >> 17;
    _rng_state ^= _rng_state << 5;
    return _rng_state;
}

// Random int in [min, max] inclusive
static inline int rng_range(int mn, int mx) {
    if (mn >= mx) return mn;
    return mn + (int)(rng_next() % (uint32_t)(mx - mn + 1));
}

// Random float in [0, 1)
static inline float rng_float(void) {
    return (float)(rng_next() & 0xFFFF) / 65536.0f;
}

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------

static inline float dist_sq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

static inline float clampf(float val, float mn, float mx) {
    if (val < mn) return mn;
    if (val > mx) return mx;
    return val;
}

static inline int clampi(int val, int mn, int mx) {
    if (val < mn) return mn;
    if (val > mx) return mx;
    return val;
}

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

// Fast inverse square root (Quake III style, ~1% error after Newton step)
static inline float fast_inv_sqrt(float x) {
    float xhalf = 0.5f * x;
    int i;
    memcpy(&i, &x, sizeof(i));
    i = 0x5f3759df - (i >> 1);
    memcpy(&x, &i, sizeof(x));
    x = x * (1.5f - xhalf * x * x);
    return x;
}

// Float-precision PI to avoid double-promotion warnings
#define PI_F 3.14159265f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#endif // MATHUTIL_H
