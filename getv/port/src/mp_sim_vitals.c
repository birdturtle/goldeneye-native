#include "mp_sim_vitals.h"

float mpSimArmourFromPickup(float normalized_amount)
{
    return normalized_amount > 0.0f ? normalized_amount * MP_SIM_ACTOR_HEALTH : 0.0f;
}

float mpSimAbsorbDamage(float *armour, float incoming)
{
    if (!armour || incoming <= 0.0f) return incoming;
    if (*armour <= 0.0f) return incoming;
    if (incoming <= *armour) {
        *armour -= incoming;
        return 0.0f;
    }
    incoming -= *armour;
    *armour = 0.0f;
    return incoming;
}

float mpSimArmourPickupScore(float current, float pickup, float distance)
{
    if (current < 0.0f || pickup <= current || distance < 0.0f)
        return 0.0f;
    /* The value is the *new* protection, not total pickup size. Avoid a long
     * detour for a nearly depleted vest when already well protected. */
    return 210.0f * (pickup - current) / MP_SIM_ACTOR_HEALTH *
           500.0f / (500.0f + distance);
}
