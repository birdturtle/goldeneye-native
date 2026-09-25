#include <stdio.h>

int gamemode;
int g_StageNum;
#include "mp_simulants.c"

static int failures;
#define CHECK(expr) do { \
    if (expr) printf("  ok    %s\n", #expr); \
    else { printf("  FAIL  %s\n", #expr); failures++; } \
} while (0)

int main(void)
{
    int humans, bots;
    for (humans = 1; humans <= 4; humans++) {
        for (bots = 0; bots <= 4; bots++) {
            int allowed = bots < 4 - humans ? bots : 4 - humans;
            mpSimulantsSetCount(humans, bots);
            CHECK(mpSimulantsGetCount() == allowed);
            CHECK(mpSimulantsCanStart(humans, humans) == (humans + allowed >= 2));
            CHECK(!mpSimulantsCanStart(humans, humans - 1));
        }
    }
    mpSimulantsSetCount(1, -10);
    CHECK(mpSimulantsGetCount() == 0);
    mpSimulantsSetCount(0, 3);
    CHECK(mpSimulantsGetCount() == 0);
    mpSimulantsSetCount(1, 1);
    gamemode = GE_GAMEMODE_MULTI;
    g_StageNum = 45;
    CHECK(mpSimulantsIsMatch());
    g_StageNum = GE_LEVEL_TITLE;
    CHECK(!mpSimulantsIsMatch());
    gamemode = 0;
    g_StageNum = 45;
    CHECK(!mpSimulantsIsMatch());
    gamemode = GE_GAMEMODE_MULTI;
    mpSimulantsSetCount(1, 0);
    CHECK(!mpSimulantsIsMatch());
    return failures ? 1 : 0;
}
