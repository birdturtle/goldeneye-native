#ifndef GE_MP_SIM_VITALS_H
#define GE_MP_SIM_VITALS_H

/* Multiplayer armour pickups are normalized to one player health unit. The
 * character damage path uses eight units for that same amount of health. */
#define MP_SIM_ACTOR_HEALTH 8.0f

float mpSimArmourPickupScore(float current, float pickup, float distance);
float mpSimArmourFromPickup(float normalized_amount);
/* Consume armour first and return only the damage that reaches health. */
float mpSimAbsorbDamage(float *armour, float incoming);

#endif
