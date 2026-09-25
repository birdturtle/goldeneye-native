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
    MpSimGoalChoice options[] = {
        { MP_SIM_GOAL_WEAPON, 75.0f },
        { MP_SIM_GOAL_ARMOUR, 160.0f },
        { MP_SIM_GOAL_AMMO, 180.0f }
    };
    check(mpSimChooseGoal(options, 3, MP_SIM_GOAL_PURSUIT).kind == MP_SIM_GOAL_AMMO,
          "ammo can beat weapon and armour on a shared scale");
    check(mpSimChooseGoal(options, 3, MP_SIM_GOAL_ARMOUR).kind == MP_SIM_GOAL_ARMOUR,
          "minor improvement does not reverse an existing goal");
    check(mpSimAmmoPickupScore(MP_SIM_KNIFE, 0, 10, 200) >
          mpSimAmmoPickupScore(MP_SIM_KNIFE, 2, 10, 200),
          "first knife is more urgent than stocking up");
    check(mpSimAmmoPickupScore(MP_SIM_MINE, 5, 5, 0) == 0 &&
          mpSimAmmoPickupScore(MP_SIM_GRENADE, 0, 5, 0) == 0,
          "full or unsupported ammo has no goal");
    check(mpSimPreferKnife(1, 0, 400) && !mpSimPreferKnife(1, 1, 400) &&
          !mpSimPreferKnife(2, 0, 900), "knife selection uses inventory and range");
    check(mpSimPreferRemoteMine(1, 0, 500) &&
          !mpSimPreferRemoteMine(1, 1, 500),
          "one active remote mine blocks another placement");
    check(mpSimRemoteDetonationAllowed(1, 0, 400 * 400, 50 * 50, 0),
          "an armed settled mine can detonate while its projectile still exists");
    check(!mpSimRemoteDetonationAllowed(1, 1, 400 * 400, 0, 0) &&
          !mpSimRemoteDetonationAllowed(2, 0, 400 * 400, 0, 0) &&
          !mpSimRemoteDetonationAllowed(1, 0, 200 * 200, 0, 0),
          "an airborne, unarmed, or unsafe mine cannot detonate");
    check(mpSimPickupScore(MP_SIM_AUTOMATIC, 300) >
          mpSimPickupScore(MP_SIM_PISTOL, 300),
          "prefer stronger gun at equal travel distance");
    check(mpSimPickupScore(MP_SIM_PISTOL, 100) >
          mpSimPickupScore(MP_SIM_PISTOL, 800),
          "prefer near gun at equal strength");
    check(mpSimPickupScore(MP_SIM_KNIFE, 0) > 0 &&
          mpSimPickupScore(MP_SIM_MINE, 0) > 0,
          "knife and remote mine pickup priorities are enabled");
    check(mpSimPickupScore(MP_SIM_GRENADE, 0) == 0 &&
          mpSimPickupScore(MP_SIM_LASER, 0) == 0 &&
          mpSimPickupScore(MP_SIM_LAUNCHER, 0) == 0,
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
    check(mpSimCombatDistanceMode(100, 310, 1, 0, MP_SIM_COMBAT_ORBIT) ==
          MP_SIM_COMBAT_RETREAT &&
          mpSimCombatDistanceMode(310, 310, 1, 0, MP_SIM_COMBAT_RETREAT) ==
          MP_SIM_COMBAT_ORBIT &&
          mpSimCombatDistanceMode(700, 310, 1, 0, MP_SIM_COMBAT_ORBIT) ==
          MP_SIM_COMBAT_ADVANCE,
          "visible gunfight retreats, circles, and advances by distance");
    check(mpSimCombatDistanceMode(100, 310, 0, 0, MP_SIM_COMBAT_RETREAT) ==
          MP_SIM_COMBAT_ADVANCE &&
          mpSimCombatDistanceMode(50, 145, 1, 1, MP_SIM_COMBAT_ORBIT) ==
          MP_SIM_COMBAT_ORBIT,
          "lost sight resumes pursuit and melee does not back away");
    check(mpSimCombatDistanceMode(220, 310, 1, 0, MP_SIM_COMBAT_RETREAT) ==
          MP_SIM_COMBAT_RETREAT &&
          mpSimCombatDistanceMode(390, 310, 1, 0, MP_SIM_COMBAT_ADVANCE) ==
          MP_SIM_COMBAT_ADVANCE,
          "distance mode hysteresis prevents rapid reversals");
    return 0;
}
