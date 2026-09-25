#ifndef GE_MP_NAV_GRAPH_H
#define GE_MP_NAV_GRAPH_H

/* Map-independent runtime graph. Zero-initialize before first build. A stage
 * adapter supplies pads with STAN associations and an engine-backed direct-walk
 * check. No stage pointers are retained; rebuild after each stage load.
 * Keep this declaration in sync with the game-side copy through patch 0050. */
typedef struct MpNavAnchor {
    int pad_id;
    float x, y, z;
    int has_stan;
} MpNavAnchor;

typedef int (*MpNavDirectWalk)(const MpNavAnchor *from,
                               const MpNavAnchor *to, void *context);

typedef struct MpNavGraph {
    int stage;
    int node_count;
    MpNavAnchor *nodes;
    unsigned char *edges;
    int *component;
    int edge_count;
    int component_count;
    int largest_component;
    int isolated_nodes;
    int max_neighbours;
    int max_route_hops;
} MpNavGraph;

/* Expects stable, unique pad IDs. Each node examines at most candidate_limit
 * nearest other pads (squared 3D distance, then pad ID). Edges are symmetric
 * only when the walk check accepts travel in both directions. */
int mpNavGraphBuild(MpNavGraph *graph, int stage, const MpNavAnchor *pads,
                    int pad_count, int candidate_limit,
                    MpNavDirectWalk direct_walk, void *context);
int mpNavGraphReachable(const MpNavGraph *graph, int from_pad, int to_pad);
/* Returns a bounded prefix of the shortest route, including the start pad.
 * Zero means no route; callers can ask again after reaching the last returned
 * pad. The complete path is checked before any prefix is returned. */
int mpNavGraphRoute(const MpNavGraph *graph, int from_pad, int to_pad,
                    int *pad_ids, int capacity);
/* Path length along actual links, with height penalized like the goal policy.
 * Costs are indexed by graph->nodes (sorted pad IDs); unreachable is negative.
 * Reuse the result for all candidates in a decision tick. */
int mpNavGraphCosts(const MpNavGraph *graph, int from_pad,
                    float *costs, int capacity);
float mpNavGraphCostToPad(const MpNavGraph *graph, const float *costs,
                          int to_pad);
/* Finds single nodes whose removal disconnects a component. Returns the total
 * count, writes up to capacity pad IDs, and reports single-edge bottlenecks. */
int mpNavGraphBottlenecks(const MpNavGraph *graph, int *pad_ids, int capacity,
                          int *bridge_count);
void mpNavGraphClear(MpNavGraph *graph);

#endif
