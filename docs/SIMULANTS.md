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

Temple has no authored multiplayer waypoint table. With generated navigation
enabled, it now adds centres from live walkable STAN tiles between sparse setup
pads. Original pad IDs remain valid for spawn and pickup objects; the synthetic
points exist only for the duration of that stage. The log reports `stan=` for
the extra nodes, or reports when sampling falls back to setup pads. The bot
retains its current reachable pursuit target between route polls and switches
only when another player offers a substantially cheaper route. These paths
still require a Temple match playtest for door handling and actor clearance.

For a short route diagnostic run, set `GETV_SIM_NAV_TRACE=1` alongside
`GETV_SIM_NAV=generated`. The `sim nav trace:` lines name route pad prefixes,
the actor's current pad and movement mode, door activations, and graph cut
points. A route reported as running has only been accepted by the actor's
movement code; repeated `nextdist` with little position change identifies the
specific segment that needs repair. The trace is off during normal play.

Route decisions now measure path length on the active waypoint graph, with
unreachable destinations removed from pickup, chase, maneuver and mine retreat
choices. The graph is built once per stage and the Simulant's distance map is
refreshed when its start waypoint changes or every 30 polls. Pursuit passes the
human's current horizontal position to the native character route action.
Facility's authored links and the generated graph both provide bounded route
prefixes for the six-slot actor buffer. Native collision and doors still decide
whether a planned move actually succeeds. This is a first route-selection pass;
map coverage and dynamic obstacles still need in-game testing.

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

When a player is visible, the Simulant uses its weapon's preferred range to
advance, retreat, or move sideways between reachable waypoints. Its right arm
tracks the player and it can shoot during native route movement. If the graph
offers no useful lateral route, GoldenEye's standing turn faces the opponent
until the next replan. Weapon and armour pickup goals still take precedence
when their shared score warrants it; loss of sight resumes normal pursuit.
This is a first combat movement pass, not a cover or advanced aiming system.

## Weapon policy foundation

The Simulant classifies the selected match weapons and ranks usable world
firearms by preference and travel distance. It retains its current pickup goal
unless another choice is substantially better. Pistol, automatic, shotgun,
rifle, sniper and magnum classes have separate shot intervals and preferred
fight ranges. The policy lives in `getv/port/src/mp_sim_policy.c`; patch `0042`
introduced its game adapter and patch `0049` adds combat movement.
Its design follows the separation of weapon choice and distance choice in the
MIT Perfect Dark decomp (`n64decomp/perfect_dark`, commit `169ed48bdcbf`,
`src/game/botinv.c` and `src/game/botcmd.c`); these GoldenEye values are new.

Knife and remote mine pickups now have attack paths. Grenades, timed and
proximity mines, launchers and lasers remain unavailable to the bot until
their actions are implemented. The item-specific pickup gate prevents the
shared mine policy from admitting the unsupported mine types.

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

Throwing knives and remote mines use native object motion and impact or blast
effects. `mp_sim_effects` holds their ownership in a sidecar keyed by the
live object and roster incarnation. Knife impact and blast damage reach the
human multiplayer health and scoring bridge without pretending the bot is a
controller player. Old-life projectiles remain recognizable but cannot deal
damage after respawn. Mine chain reactions inherit that ownership. The bot
throws at a visible, aligned target, then detonates its one active remote mine
when it is armed, the opponent is close, and the bot is farther away. The
throw arc, aim and detonation spacing are initial values for playtesting.
Knife collision also tests a human's native collision polygon when their
first-person character model is not on screen. A remote mine can remain a
projectile after settling; its AIRBORNE flag decides whether it has landed.
When the armed mine is too close to the bot, it routes to a safe waypoint
before trying to detonate.

## Armour and health

Each Simulant life holds armour separately from the character's accumulated
health damage. A body-armour pickup replaces the remaining protection as it
does for a human, without healing previous wounds. Bullet and explosion hits
consume armour first; the surviving damage follows the native character
health and death path. Spawn and respawn reset the pool. The bot seeks live,
respawning vests when its current protection is lower, after securing a gun.
It fights a visible close opponent before detouring to a pickup. The shared
math and pickup preference live in `mp_sim_vitals`; patch `0044` connects
pickup, damage, and life state to the game.
Set `GETV_SIM_ARMOUR_TRACE=1` to print absorbed damage and remaining armour
while checking a match; pickup events are logged without that setting.

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
