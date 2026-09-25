#ifndef GE_MP_NAV_GRAPH_H
#define GE_MP_NAV_GRAPH_H

/* Map-independent runtime graph. Zero-initialize before first build. A stage
 * adapter supplies pads with STAN associations and an engine-backed direct-walk
 * check. No stage pointers are retained; rebuild after each stage load.
 * Keep this declaration in sync with the game-side copy in patch 0037. */
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
void mpNavGraphClear(MpNavGraph *graph);

#endif
