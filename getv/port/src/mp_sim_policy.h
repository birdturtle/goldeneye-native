#ifndef GE_MP_SIM_POLICY_H
#define GE_MP_SIM_POLICY_H

/* Simulant decisions are independent of map navigation and character actions.
 * Keep this declaration in sync with the game-side copy through patch 0049. */
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

/* The strategic choice is separate from the weapon used while carrying it
 * out. A bot can collect mine ammo while keeping its firearm equipped. */
typedef enum MpSimGoalKind {
    MP_SIM_GOAL_PURSUIT,
    MP_SIM_GOAL_WEAPON,
    MP_SIM_GOAL_ARMOUR,
    MP_SIM_GOAL_AMMO
} MpSimGoalKind;

typedef struct MpSimGoalChoice {
    MpSimGoalKind kind;
    float score;
} MpSimGoalChoice;

const MpSimWeaponPolicy *mpSimWeaponPolicy(MpSimWeaponKind kind);
/* Higher values are better. Item-specific gates in the game adapter keep
 * timed/proximity mines and unsupported explosives out of pickup selection. */
float mpSimPickupScore(MpSimWeaponKind kind, float distance);
float mpSimAmmoPickupScore(MpSimWeaponKind kind, int held, int capacity,
                           float distance);
/* A goal persists until a new candidate improves on it appreciably. */
MpSimGoalChoice mpSimChooseGoal(const MpSimGoalChoice *candidates, int count,
                                MpSimGoalKind current);
/* Select an available action without assuming the right hand holds every
 * weapon in the inventory. Returned item is supplied by the game adapter. */
int mpSimPreferKnife(int knives, int has_firearm, float distance);
int mpSimPreferRemoteMine(int mines, int mine_active, float distance);
/* A settled remote mine may retain its native projectile allocation. Only
 * AIRBORNE means it has not yet landed; preserve the blast safety spacing. */
int mpSimRemoteDetonationAllowed(int timer, int airborne,
                                 float bot_distance_sq, float player_distance_sq,
                                 float vertical_distance_sq);
/* The unarmed Simulant fights when cornered; an armed one may slap only at
 * extreme close range. Vertical separation must never produce a melee hit. */
int mpSimShouldSlap(float horizontal_distance, float vertical_distance,
                    int has_firearm);

/* Like PD's botcmd distance modes, but the game adapter chooses a reachable
 * GoldenEye waypoint for each move. ORBIT keeps a visible fight in motion. */
typedef enum MpSimCombatMode {
    MP_SIM_COMBAT_ADVANCE,
    MP_SIM_COMBAT_ORBIT,
    MP_SIM_COMBAT_RETREAT
} MpSimCombatMode;
MpSimCombatMode mpSimCombatDistanceMode(float range, float ideal_range,
                                        int visible, int melee,
                                        MpSimCombatMode previous);

#endif
