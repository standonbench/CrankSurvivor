#include <stdio.h>
#include <stdlib.h>
#include "game.h"

PlaydateAPI* pd = NULL;

#if DEBUG_BUILD
DebugStats debugStats = {0};
#endif

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg)
{
    (void)arg;

    if (event == kEventInit)
    {
        pd = playdate;
        pd->display->setRefreshRate(30);
        pd->system->setUpdateCallback(update, pd);

        // Seed RNG with current time
        unsigned int ms;
        pd->system->getSecondsSinceEpoch(&ms);
        rng_seed((uint32_t)ms ^ 0xDEADBEEF);

        game_init();
    }

    return 0;
}
