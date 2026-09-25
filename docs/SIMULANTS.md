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
- `mp_nav_graph` builds a deterministic diagnostic graph from pads with valid
  STAN associations. The caller must provide a real direct-walk check. It
  measures nodes, edges, components, isolated nodes, degree and maximum route
  hops. It owns copies of pad coordinates and indices, not pointers into stage
  memory. Clear it during stage teardown.

These modules do not yet enter gameplay. In particular, a synthetic graph
test does not establish that any GoldenEye map is connected or walkable.

## Contracts to verify before installing generated routes

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
  Generated waygroups require a route-buffer audit before installation;
  splitting groups by an arbitrary node count does not prove route safety.

## Next integration gates

1. Adapt the old match's menu, actor, combat, score, radar, death, respawn,
   entrance, HUD and pause hooks into a new numbered game-source patch. A
   one-human match must load the multiplayer setup even though the native port
   currently uses `getPlayerCount() >= 2` in that decision.
2. Verify Facility one-human/one-Simulant behavior with the currently proven
   Facility route graph. Keep this as the gameplay baseline while changing
   navigation independently.
3. Supply a stage adapter that enumerates pads and tests direct walkability;
   log graph statistics, and check every multiplayer start and weapon pad for
   membership in a useful connected component. Compare Facility with the
   existing 157-waypoint, 22-waygroup reference.
4. Generate and validate native waypoint/waygroup arrays under stage ownership.
   Retire the embedded Facility table only after a complete spawn, seek,
   fight, death and respawn loop works with the generated graph.
5. Measure Temple, Complex, Stack and an enclosed map, then all multiplayer
   maps. Classify disconnected pads, unsafe edges, grouping, doors and movement
   separately before adding any map-specific rule.

The port's existing `ge_bot.c` posts controller input into a player slot.
`ge_bot_ai.c` can spawn characters but does not attach them to the multiplayer
roster. Neither is the completed match implementation described here.
