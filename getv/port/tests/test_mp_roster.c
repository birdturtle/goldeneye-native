#include <stdio.h>

#include "mp_roster.c"

static int failures;

#define CHECK(expr) do { \
    if (expr) printf("  ok    %s\n", #expr); \
    else { printf("  FAIL  %s\n", #expr); failures++; } \
} while (0)

int main(void)
{
    MpRosterRef first, next, human;
    int humans, bots, i;

    for (humans = 1; humans <= MP_ROSTER_MAX; humans++) {
        for (bots = 0; bots <= MP_ROSTER_MAX - humans; bots++) {
            CHECK(mpRosterBegin(45, humans, bots));
            CHECK(mpRosterCount() == humans + bots);
            for (i = 0; i < humans + bots; i++) {
                const MpRosterEntry *entry = mpRosterEntry(i);
                CHECK(entry && entry->kind == (i < humans ? MP_ROSTER_HUMAN : MP_ROSTER_SIMULANT));
            }
            mpRosterEnd();
        }
    }

    CHECK(!mpRosterBegin(45, 4, 1));
    CHECK(!mpRosterBegin(45, 0, 1));
    CHECK(mpRosterCount() == 0);
    CHECK(mpRosterBegin(45, 1, 1));
    human = mpRosterRefForSlot(0);
    CHECK(mpRosterResolve(human) == 0);
    CHECK(mpRosterRefForSlot(1).slot == -1);
    CHECK(mpRosterBindSimulant(1, 42));
    CHECK(!mpRosterBindSimulant(1, 43));
    first = mpRosterRefForSlot(1);
    CHECK(mpRosterResolve(first) == 1);
    mpRosterUnbindSimulant(1, 43);
    CHECK(mpRosterResolve(first) == 1);
    mpRosterUnbindSimulant(1, 42);
    CHECK(mpRosterResolve(first) == -1);
    CHECK(mpRosterBindSimulant(1, 42));
    next = mpRosterRefForSlot(1);
    CHECK(mpRosterResolve(next) == 1);
    CHECK(mpRosterResolve(first) == -1);
    mpRosterEnd();
    CHECK(mpRosterResolve(next) == -1);
    CHECK(mpRosterResolve(human) == -1);
    CHECK(mpRosterBegin(45, 1, 1));
    CHECK(mpRosterResolve(next) == -1);
    return failures ? 1 : 0;
}
