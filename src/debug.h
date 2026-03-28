#ifndef DEBUG_H
#define DEBUG_H

#ifndef DEBUG_BUILD
#define DEBUG_BUILD 0
#endif

// Forward-declare pd for use in macros
extern PlaydateAPI* pd;

#if DEBUG_BUILD
  #define DLOG(fmt, ...) pd->system->logToConsole(fmt, ##__VA_ARGS__)
  #define ASSERT(cond) do { \
      if (!(cond)) { \
          pd->system->error("ASSERT FAIL [%s:%d]: %s", __FILE__, __LINE__, #cond); \
      } \
  } while(0)
#else
  #define DLOG(fmt, ...) ((void)0)
  #define ASSERT(cond) ((void)0)
#endif

// Performance timing helpers
typedef struct {
    float updateMs;
    float renderMs;
    float collisionMs;
    int enemyCount;
    int bulletCount;
    int particleCount;
    int peakEnemies;
} DebugStats;

#if DEBUG_BUILD
extern DebugStats debugStats;
#endif

#endif // DEBUG_H
