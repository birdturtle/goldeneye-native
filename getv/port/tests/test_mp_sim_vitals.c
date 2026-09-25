#include <stdio.h>
#include <stdlib.h>

#include "../src/mp_sim_vitals.c"

static void check(int ok, const char *what)
{
    if (!ok) { fprintf(stderr, "FAIL %s\n", what); exit(1); }
    printf("ok %s\n", what);
}

int main(void)
{
    float armour = mpSimArmourFromPickup(1.0f);
    float wounds = 3.0f;
    check(armour == MP_SIM_ACTOR_HEALTH && wounds == 3.0f,
          "armour pickup does not heal previous wounds");
    wounds += mpSimAbsorbDamage(&armour, 2.0f);
    check(wounds == 3.0f && armour == 6.0f,
          "armour absorbs a partial shot without touching health");
    wounds += mpSimAbsorbDamage(&armour, 7.0f);
    check(wounds == 4.0f && armour == 0.0f,
          "overflow damages wounded health after armour is depleted");
    check(mpSimAbsorbDamage(&armour, 4.0f) == 4.0f,
          "unarmoured damage passes through");
    armour = mpSimArmourFromPickup(0.5f);
    check(armour == 4.0f && wounds == 4.0f,
          "respawned half vest replaces armour without changing wounds");
    check(mpSimArmourPickupScore(4.0f, 4.0f, 10.0f) == 0.0f,
          "equal protection is not a pickup goal");
    check(mpSimArmourPickupScore(0.0f, 8.0f, 100.0f) >
          mpSimArmourPickupScore(0.0f, 4.0f, 100.0f),
          "full vest outranks half vest at equal distance");
    check(mpSimArmourPickupScore(0.0f, 8.0f, 100.0f) >
          mpSimArmourPickupScore(0.0f, 8.0f, 1000.0f),
          "nearer vest outranks farther vest");
    check(mpSimArmourPickupScore(6.0f, 8.0f, 100.0f) <
          mpSimArmourPickupScore(0.0f, 8.0f, 100.0f),
          "low armour deficit makes detour less valuable");
    armour = 8.0f;
    check(mpSimAbsorbDamage(&armour, 0.0f) == 0.0f && armour == 8.0f,
          "zero damage cannot consume armour");
    return 0;
}
