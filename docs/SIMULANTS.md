# Simulant multiplayer migration

This branch is migrating the character-based Simulant match from
`birdturtle/goldeneye-pc-port` to the native port. The target is up to four
combatants, with human players assigned first and character Simulants filling
unused match slots. A one-human match must keep one full-size viewport and all
ordinary multiplayer presentation and rules.

## Implemented foundation

- `mp_roster` holds four match identities without treating Simulants as
  controllers or viewports. A character number is bound per life; generation
  and incarnation reject references from previous stages or respawns.
- `mp_simulants` stores the requested count, clamps it to the remaining slots,
  and permits a one-controller match only when a Simulant fills the second
  combatant slot.
- `mp_nav_graph` builds a deterministic graph from pads with valid
  STAN associations. The caller must provide a real direct-walk check. It
  measures nodes, edges, components, isolated nodes, degree and maximum route
  hops. It owns copies of pad coordinates and indices, not pointers into stage
  memory. Clear it during stage teardown.

The Simulant match, scoring, HUD, radar, weapon pickup and combat are wired into
gameplay. Facility still uses its proven solo navigation tables by default.
For a generated graph experiment, set `GETV_SIM_NAV=generated` before launch.
The map menu then permits the normally unlocked multiplayer maps with a
Simulant. The graph is rebuilt from that stage's linked pads and STAN tiles,
with a bounded route bridge for the actor's six waypoint slots. No graph is
stored per map or shipped as map data. The log reports node, edge, component,
largest component and isolated pad counts for each generated stage.

On Windows, after applying the patch, launch with:

```powershell
$env:GETV_SIM_NAV='generated'
.\getv\dev_windows.ps1
Remove-Item Env:\GETV_SIM_NAV
```

Test Facility first, then another unlocked map. A synthetic graph test cannot
establish that any GoldenEye map is connected or walkable. Direct STAN segment
checks do not prove clearance for the actor or dynamic door traversal; failed
paths are reported and normal movement collision still applies. Without the
flag, the established Facility behavior remains the gameplay baseline.

When armed and a player is visible, the actor keeps pursuing at range and
fires while aligned with the player. Inside close range it yields travel to
GoldenEye's own standing turn, then resumes pursuit when the player moves out
of close range or sight is lost. While unarmed it still prioritizes a world
firearm. This is the first engagement decision, not a finished aiming, weapon
balance or cover system.

## Weapon policy foundation

The Simulant classifies the selected match weapons and ranks usable world
firearms by preference and travel distance. It retains its current pickup goal
unless another choice is substantially better. Pistol, automatic, shotgun,
rifle, sniper and magnum classes have separate shot intervals and distances at
which a visible bot stops its route to face the opponent. The policy lives in
`getv/port/src/mp_sim_policy.c` and the game-side adapter is in patch `0042`.
Its design follows the separation of weapon choice and distance choice in the
MIT Perfect Dark decomp (`n64decomp/perfect_dark`, commit `169ed48bdcbf`,
`src/game/botinv.c` and `src/game/botcmd.c`); these GoldenEye values are new.

Knife, grenade, mine, launcher and laser classes currently have no pickup
value. The combat executor only applies firearm hits, so picking up those
items before their attack paths exist would leave a bot unable to fight.
Their next phase needs melee, projectile and explosion paths with multiplayer
ownership and scoring. Armour also needs a separate health and armour model:
character negative `damage` represents armour, whereas the human multiplayer
player stores health and armour separately. Simply setting negative character
damage would heal a wounded Simulant when it picked up armour.

## Close combat

When an unarmed Simulant encounters a visible opponent at close range, it
stops to slap instead of continuing toward a weapon pickup. At longer range
it still prioritizes a world firearm. An armed one chooses a slap at extreme
close range; otherwise it uses its firearm. A strike requires
an unobstructed STAN segment, a narrow facing cone and a target on the same
level. The damage and kill credit still pass through `mpCombatDamageHuman`,
and the third-person character briefly extends a hand for the hit. The
slapper's cadence and range decision are in `mp_sim_policy`, with the game-side
execution in patch `0043-simulant-melee-combat.patch`.

Throwing knives and explosives need a separate projectile owner: GoldenEye's
human throw routines encode an owner as a human player index and use it later
for impact and explosion credit. A Simulant roster slot cannot be inserted
into those routines as though it were a controller player.

## Generated route constraints

- `stanFillSearch` reads each tile's `point[1].link`: a nonzero high portion
  denotes a linked tile, and the link is an offset from `standTileStart`. It
  aborts once its discovery stack reaches 351 tiles. Its BFS proves local
  STAN connectivity but is not a global map reachability oracle.
- `walkTilesBetweenPoints_NoCallback` traverses tiles along a straight X/Z
  segment. Its result alone does not establish actor radius, height clearance,
  a traversable door, or a usable path through multiple turns. The stage
  adapter must establish the correct direct-walk predicate from source and
  measurements before passing it to `mp_nav_graph`.
- `plot_course_for_actor` obtains nearby authored waypoints and calls
  `waypointFindRoute(..., MAX_CHRWAYPOINTS)` with a six-element local array.
  The group solver's `waypointFindRouteInGroup` adds 9999 to its output bound.
  Generated routes bypass the original group solver and return at most five
  waypoints plus a terminator. The actor replans when the prefix is consumed.

## Next playtest gates

1. Compare Facility's generated component coverage and movement with the
   existing 157-waypoint, 22-waygroup reference.
2. Play one full spawn, seek, fight, death and respawn loop on Temple, Complex,
   Stack and an enclosed map. Record each stage's graph counts and any stalls.
3. Check every multiplayer start and weapon pad for membership in a useful
   connected component; adjust the general STAN adapter if coverage is poor.
4. Classify dynamic doors, narrow gaps, disconnected regions and movement
   separately. Promote generated navigation to the default only after the
   generated route performs acceptably across the multiplayer maps.

The port's existing `ge_bot.c` posts controller input into a player slot.
`ge_bot_ai.c` can spawn characters but does not attach them to the multiplayer
roster. Neither is the completed match implementation described here.
