#include <stdio.h>
#include "../src/mp_roster.c"
#include "../src/mp_sim_effects.c"

static int fails;
#define CHECK(x) do { if (x) printf("ok %s\n", #x); else { printf("FAIL %s\n", #x); ++fails; } } while (0)

int main(void)
{
    int knife, mine, blast, reused;
    MpRosterRef first, next;
    CHECK(mpRosterBegin(45, 1, 1));
    CHECK(mpRosterBindSimulant(1, 0x6fff));
    first = mpRosterRefForSlot(1);
    CHECK(mpSimEffectTrack(&knife, MP_SIM_EFFECT_KNIFE, first));
    CHECK(mpSimEffectTrack(&mine, MP_SIM_EFFECT_REMOTE_MINE, first));
    CHECK(mpSimEffectOwned(&knife, MP_SIM_EFFECT_KNIFE));
    CHECK(!mpSimEffectOwned(&knife, MP_SIM_EFFECT_REMOTE_MINE));
    CHECK(mpRosterResolve(mpSimEffectOwner(&mine, MP_SIM_EFFECT_REMOTE_MINE)) == 1);
    mpSimEffectSetPending(mpSimEffectOwner(&mine, MP_SIM_EFFECT_REMOTE_MINE));
    CHECK(mpSimEffectTrack(&blast, MP_SIM_EFFECT_EXPLOSION, mpSimEffectPending()));
    mpSimEffectSetPending((MpRosterRef){0, 0, -1});
    CHECK(mpSimEffectPending().slot == -1);
    mpRosterUnbindSimulant(1, 0x6fff);
    CHECK(mpRosterResolve(mpSimEffectOwner(&blast, MP_SIM_EFFECT_EXPLOSION)) < 0);
    CHECK(mpSimEffectOwned(&blast, MP_SIM_EFFECT_EXPLOSION));
    CHECK(mpSimEffectTrack(&reused, MP_SIM_EFFECT_KNIFE, first));
    CHECK(mpRosterResolve(mpSimEffectOwner(&reused, MP_SIM_EFFECT_KNIFE)) < 0);
    CHECK(mpRosterBindSimulant(1, 0x6fff));
    next = mpRosterRefForSlot(1);
    CHECK(mpSimEffectTrack(&knife, MP_SIM_EFFECT_KNIFE, next));
    CHECK(mpRosterResolve(mpSimEffectOwner(&knife, MP_SIM_EFFECT_KNIFE)) == 1);
    mpSimEffectForget(&mine);
    CHECK(!mpSimEffectOwned(&mine, MP_SIM_EFFECT_REMOTE_MINE));
    mpSimEffectsReset();
    CHECK(!mpSimEffectOwned(&knife, MP_SIM_EFFECT_KNIFE));
    mpRosterEnd();
    return fails != 0;
}
