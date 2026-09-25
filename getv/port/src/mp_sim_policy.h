#ifndef GE_MP_SIM_POLICY_H
#define GE_MP_SIM_POLICY_H

/* Simulant decisions are independent of map navigation and character actions.
 * Keep this declaration in sync with the game-side copy in patch 0042. */
typedef enum MpSimWeaponKind {
    MP_SIM_UNARMED,
    MP_SIM_PISTOL,
    MP_SIM_AUTOMATIC,
    MP_SIM_SHOTGUN,
    MP_SIM_RIFLE,
    MP_SIM_SNIPER,
    MP_SIM_MAGNUM,
    MP_SIM_LASER,
    MP_SIM_KNIFE,
    MP_SIM_GRENADE,
    MP_SIM_MINE,
    MP_SIM_LAUNCHER,
    MP_SIM_WEAPON_KINDS
} MpSimWeaponKind;

typedef struct MpSimWeaponPolicy {
    int pickup_value;
    float hold_range;
    unsigned shot_interval;
    int firearm_ready;
} MpSimWeaponPolicy;

const MpSimWeaponPolicy *mpSimWeaponPolicy(MpSimWeaponKind kind);
/* Higher values are better. Zero means the current combat executor cannot use
 * the item, even if it exists in the selected multiplayer weapon set. */
float mpSimPickupScore(MpSimWeaponKind kind, float distance);
/* The unarmed Simulant fights when cornered; an armed one may slap only at
 * extreme close range. Vertical separation must never produce a melee hit. */
int mpSimShouldSlap(float horizontal_distance, float vertical_distance,
                    int has_firearm);

#endif
