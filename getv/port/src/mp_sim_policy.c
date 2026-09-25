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
    { 95, 120.0f, 48, 0 }, /* knife or throwing knife */
    {  0, 520.0f,  0, 0 }, /* grenade */
    { 80, 520.0f, 75, 0 }, /* remote mine; other mine types remain gated */
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
    if (!policy || policy->pickup_value <= 0 ||
        distance < 0.0f) return 0.0f;
    return (float)policy->pickup_value * 500.0f / (500.0f + distance);
}

float mpSimAmmoPickupScore(MpSimWeaponKind kind, int held, int capacity,
                           float distance)
{
    int value = kind == MP_SIM_KNIFE ? 190 : kind == MP_SIM_MINE ? 155 : 0;
    int goal = kind == MP_SIM_KNIFE ? 3 : kind == MP_SIM_MINE ? 2 : 0;
    if (!value || held < 0 || capacity <= 0 || held >= capacity ||
        held >= goal ||
        distance < 0.0f) return 0.0f;
    /* The first projectile opens an otherwise unavailable action. Later
     * crates matter less, as in PD's target versus critical ammo goals. */
    return (float)value * (held == 0 ? 1.0f :
             (held < 2 ? 0.65f : 0.30f)) *
           500.0f / (500.0f + distance);
}

MpSimGoalChoice mpSimChooseGoal(const MpSimGoalChoice *candidates, int count,
                                MpSimGoalKind current)
{
    MpSimGoalChoice best = { MP_SIM_GOAL_PURSUIT, 0.0f };
    MpSimGoalChoice previous = best;
    if (!candidates || count <= 0) return best;
    for (int i = 0; i < count; ++i) {
        if (candidates[i].score > best.score) best = candidates[i];
        if (candidates[i].kind == current) previous = candidates[i];
    }
    return previous.score > 0.0f && previous.score >= best.score * 0.85f
        ? previous : best;
}

int mpSimPreferKnife(int knives, int has_firearm, float distance)
{
    return knives >= (has_firearm ? 2 : 1) && distance >= 170.0f &&
           distance <= 800.0f;
}

int mpSimPreferRemoteMine(int mines, int mine_active, float distance)
{
    return mines > 0 && !mine_active && distance >= 350.0f &&
           distance <= 750.0f;
}

int mpSimRemoteDetonationAllowed(int timer, int airborne,
                                 float bot_distance_sq, float player_distance_sq,
                                 float vertical_distance_sq)
{
    return timer == 1 && !airborne &&
           bot_distance_sq >= 330.0f * 330.0f &&
           player_distance_sq < 240.0f * 240.0f &&
           vertical_distance_sq < 120.0f * 120.0f;
}

int mpSimShouldSlap(float horizontal_distance, float vertical_distance,
                    int has_firearm)
{
    float range = has_firearm ? 95.0f : 145.0f;
    return horizontal_distance >= 0.0f && horizontal_distance <= range &&
           vertical_distance >= -75.0f && vertical_distance <= 75.0f;
}

/* PD botcmd.c uses advance/backup/OK bands and 25-unit persistence. Here OK
 * becomes orbit so GoldenEye's actor keeps maneuvering during a firefight.
 * n64decomp/perfect_dark @ 169ed48bdcbfb3b568b028bd5bebb27680073514,
 * src/game/botcmd.c (MIT); thresholds are specific to this port. */
MpSimCombatMode mpSimCombatDistanceMode(float range, float ideal_range,
                                        int visible, int melee,
                                        MpSimCombatMode previous)
{
    float minimum, maximum;
    if (range < 0.0f || ideal_range <= 0.0f || !visible)
        return MP_SIM_COMBAT_ADVANCE;
    minimum = melee ? 0.0f : ideal_range * 0.70f;
    maximum = melee ? ideal_range : ideal_range * 1.25f;
    if (previous == MP_SIM_COMBAT_RETREAT) minimum += 35.0f;
    if (previous == MP_SIM_COMBAT_ADVANCE) maximum -= 35.0f;
    if (range < minimum) return MP_SIM_COMBAT_RETREAT;
    if (range > maximum) return MP_SIM_COMBAT_ADVANCE;
    return MP_SIM_COMBAT_ORBIT;
}
