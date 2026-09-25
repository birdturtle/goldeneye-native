#include "mp_sim_policy.h"

/* Inspired by the separation of weapon preferences and distance settings in
 * Perfect Dark's MIT decompilation, n64decomp/perfect_dark at 169ed48bdcbf,
 * src/game/botinv.c and src/game/botcmd.c. These values and rules are original
 * GoldenEye tuning; no Perfect Dark weapon table is copied here. */
static const MpSimWeaponPolicy s_weapon_policies[MP_SIM_WEAPON_KINDS] = {
    /* pickup, hold range, fire interval, supported by current executor */
    {  0, 145.0f, 40, 0 }, /* unarmed melee */
    {100, 310.0f, 24, 1 }, /* pistol */
    {170, 480.0f, 13, 1 }, /* automatic */
    {145, 250.0f, 35, 1 }, /* shotgun */
    {165, 620.0f, 15, 1 }, /* rifle */
    {115, 760.0f, 44, 1 }, /* sniper */
    {125, 370.0f, 32, 1 }, /* magnum */
    {  0, 550.0f, 24, 0 }, /* laser: no native bot beam path yet */
    {  0, 120.0f,  0, 0 }, /* knife */
    {  0, 520.0f,  0, 0 }, /* grenade */
    {  0, 520.0f,  0, 0 }, /* mine */
    {  0, 750.0f,  0, 0 }, /* launcher */
};

const MpSimWeaponPolicy *mpSimWeaponPolicy(MpSimWeaponKind kind)
{
    if ((unsigned)kind >= MP_SIM_WEAPON_KINDS) return 0;
    return &s_weapon_policies[kind];
}

float mpSimPickupScore(MpSimWeaponKind kind, float distance)
{
    const MpSimWeaponPolicy *policy = mpSimWeaponPolicy(kind);
    if (!policy || !policy->firearm_ready || policy->pickup_value <= 0 ||
        distance < 0.0f) return 0.0f;
    return (float)policy->pickup_value * 500.0f / (500.0f + distance);
}

int mpSimShouldSlap(float horizontal_distance, float vertical_distance,
                    int has_firearm)
{
    float range = has_firearm ? 95.0f : 145.0f;
    return horizontal_distance >= 0.0f && horizontal_distance <= range &&
           vertical_distance >= -75.0f && vertical_distance <= 75.0f;
}
