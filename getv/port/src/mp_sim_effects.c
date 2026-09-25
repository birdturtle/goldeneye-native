#include "mp_sim_effects.h"

#define MP_SIM_EFFECT_CAPACITY 512

typedef struct MpSimEffect {
    const void *key;
    MpRosterRef owner;
    MpSimEffectKind kind;
} MpSimEffect;

static MpSimEffect s_effects[MP_SIM_EFFECT_CAPACITY];
static MpRosterRef s_pending = {0, 0, -1};

static MpRosterRef none(void)
{
    return (MpRosterRef){0, 0, -1};
}

void mpSimEffectsReset(void)
{
    for (int i = 0; i < MP_SIM_EFFECT_CAPACITY; ++i) s_effects[i].key = 0;
    s_pending = none();
}

int mpSimEffectTrack(const void *key, MpSimEffectKind kind, MpRosterRef owner)
{
    int free_index = -1;
    /* Keep an old life's tag until the native object is freed. Its ref will
     * no longer resolve for damage, but must still suppress player-bit credit. */
    if (!key || kind < MP_SIM_EFFECT_KNIFE || kind > MP_SIM_EFFECT_EXPLOSION ||
        owner.slot < 0 || owner.slot >= MP_ROSTER_MAX || !owner.generation) return 0;
    for (int i = 0; i < MP_SIM_EFFECT_CAPACITY; ++i) {
        if (s_effects[i].key == key) { free_index = i; break; }
        if (!s_effects[i].key && free_index < 0) free_index = i;
    }
    if (free_index < 0) return 0;
    s_effects[free_index] = (MpSimEffect){key, owner, kind};
    return 1;
}

int mpSimEffectOwned(const void *key, MpSimEffectKind kind)
{
    if (!key) return 0;
    for (int i = 0; i < MP_SIM_EFFECT_CAPACITY; ++i)
        if (s_effects[i].key == key && s_effects[i].kind == kind) return 1;
    return 0;
}

MpRosterRef mpSimEffectOwner(const void *key, MpSimEffectKind kind)
{
    if (key)
        for (int i = 0; i < MP_SIM_EFFECT_CAPACITY; ++i)
            if (s_effects[i].key == key && s_effects[i].kind == kind)
                return s_effects[i].owner;
    return none();
}

void mpSimEffectForget(const void *key)
{
    if (!key) return;
    for (int i = 0; i < MP_SIM_EFFECT_CAPACITY; ++i)
        if (s_effects[i].key == key) s_effects[i].key = 0;
}

void mpSimEffectSetPending(MpRosterRef owner)
{
    s_pending = owner.slot >= 0 && owner.slot < MP_ROSTER_MAX &&
                owner.generation ? owner : none();
}

MpRosterRef mpSimEffectPending(void)
{
    return s_pending;
}
