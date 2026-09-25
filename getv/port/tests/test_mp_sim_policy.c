#include <stdio.h>
#include <stdlib.h>
#include "mp_sim_policy.c"

static void check(int condition, const char *name)
{
    if (!condition) { fprintf(stderr, "FAIL %s\n", name); exit(1); }
    printf("ok %s\n", name);
}

int main(void)
{
    check(mpSimPickupScore(MP_SIM_AUTOMATIC, 300) >
          mpSimPickupScore(MP_SIM_PISTOL, 300),
          "prefer stronger gun at equal travel distance");
    check(mpSimPickupScore(MP_SIM_PISTOL, 100) >
          mpSimPickupScore(MP_SIM_PISTOL, 800),
          "prefer near gun at equal strength");
    check(mpSimPickupScore(MP_SIM_KNIFE, 0) == 0 &&
          mpSimPickupScore(MP_SIM_MINE, 0) == 0 &&
          mpSimPickupScore(MP_SIM_LASER, 0) == 0,
          "unsupported attacks cannot be picked up yet");
    check(mpSimPickupScore((MpSimWeaponKind)-1, 0) == 0 &&
          mpSimPickupScore(MP_SIM_WEAPON_KINDS, 0) == 0,
          "invalid kind is safe");
    check(mpSimWeaponPolicy(MP_SIM_SHOTGUN)->hold_range <
          mpSimWeaponPolicy(MP_SIM_RIFLE)->hold_range,
          "shotgun closes closer than rifle");
    check(mpSimWeaponPolicy(MP_SIM_AUTOMATIC)->shot_interval <
          mpSimWeaponPolicy(MP_SIM_MAGNUM)->shot_interval,
          "weapon class controls cadence");
    check(mpSimShouldSlap(120, 0, 0) && !mpSimShouldSlap(120, 0, 1),
          "unarmed closes for a slap; armed keeps firing until nearer");
    check(mpSimShouldSlap(90, -30, 1) &&
          !mpSimShouldSlap(90, 100, 1) &&
          !mpSimShouldSlap(90, -100, 0),
          "melee requires the target to be on the same level");
    check(!mpSimShouldSlap(146, 0, 0) && !mpSimShouldSlap(-1, 0, 0),
          "melee respects range and invalid distance");
    return 0;
}
