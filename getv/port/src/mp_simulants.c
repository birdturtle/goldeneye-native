/* Adapted from birdturtle/goldeneye-pc-port, port/src/mp_simulants.c,
 * commit 7440ab16 (MIT, Copyright 2026 James Dansereau).
 * See getv/port/SIMULANT_SOURCE_LICENSE.txt. */
#include "mp_simulants.h"

/* A configuration contains human viewports followed by character actors.
 * It never claims extra controllers or player viewports for Simulants. */
static int s_count;
extern int gamemode;
extern int g_StageNum;
/* Stable enum values in src/bondconstants.h. Keep the policy testable without
 * pulling the game's conflicting platform headers into the port layer. */
#define GE_GAMEMODE_MULTI 1
#define GE_LEVEL_TITLE 90

int mpSimulantsGetCount(void) { return s_count; }

void mpSimulantsSetCount(int humans, int count)
{
    if (humans < 1 || humans > 4) {
        s_count = 0;
        return;
    }
    if (count < 0) count = 0;
    if (count > 4 - humans) count = 4 - humans;
    s_count = count;
}

int mpSimulantsIsMatch(void)
{
    return s_count > 0 && gamemode == GE_GAMEMODE_MULTI &&
           g_StageNum != GE_LEVEL_TITLE;
}

int mpSimulantsCanStart(int humans, int inputs)
{
    return humans >= 1 && humans <= 4 && humans <= inputs &&
           humans + s_count >= 2 && humans + s_count <= 4;
}
