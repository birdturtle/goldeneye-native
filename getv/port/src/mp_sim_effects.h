#ifndef GE_MP_SIM_EFFECTS_H
#define GE_MP_SIM_EFFECTS_H

#include "mp_roster.h"

/* Game objects and explosions own no controller slot. Keep their roster life
 * beside the native object rather than packing it into the two player bits. */
typedef enum MpSimEffectKind {
    MP_SIM_EFFECT_KNIFE = 1,
    MP_SIM_EFFECT_REMOTE_MINE,
    MP_SIM_EFFECT_CHAIN,
    MP_SIM_EFFECT_EXPLOSION
} MpSimEffectKind;

void mpSimEffectsReset(void);
int mpSimEffectTrack(const void *key, MpSimEffectKind kind, MpRosterRef owner);
int mpSimEffectOwned(const void *key, MpSimEffectKind kind);
MpRosterRef mpSimEffectOwner(const void *key, MpSimEffectKind kind);
void mpSimEffectForget(const void *key);
/* propExplode synchronously calls explosionCreate. The pending owner is only
 * visible during that call; it is cleared even if allocation fails. */
void mpSimEffectSetPending(MpRosterRef owner);
MpRosterRef mpSimEffectPending(void);

#endif
