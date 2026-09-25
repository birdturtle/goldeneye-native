#include <stdio.h>
#include <string.h>

#include "mp_nav_graph.c"

static int failures;
#define CHECK(expr) do { \
    if (expr) printf("  ok    %s\n", #expr); \
    else { printf("  FAIL  %s\n", #expr); failures++; } \
} while (0)

/* A mocked direct-walk check represents two rooms connected only by a
 * doorway. Geometric proximity alone must never produce a wall crossing. */
static int walk(const MpNavAnchor *a, const MpNavAnchor *b, void *unused)
{
    (void)unused;
    return (a->pad_id == 10 && b->pad_id == 20) ||
           (a->pad_id == 20 && b->pad_id == 10) ||
           (a->pad_id == 20 && b->pad_id == 30) ||
           (a->pad_id == 30 && b->pad_id == 20);
}

static int one_way(const MpNavAnchor *a, const MpNavAnchor *b, void *unused)
{
    (void)unused;
    return a->pad_id < b->pad_id;
}

static int chain(const MpNavAnchor *a, const MpNavAnchor *b, void *unused)
{
    (void)unused;
    return a->pad_id - b->pad_id == 1 || b->pad_id - a->pad_id == 1;
}

static int ring(const MpNavAnchor *a, const MpNavAnchor *b, void *unused)
{
    (void)unused;
    return a->pad_id - b->pad_id == 1 || b->pad_id - a->pad_id == 1 ||
           (a->pad_id == 0 && b->pad_id == 3) ||
           (a->pad_id == 3 && b->pad_id == 0);
}

static int detour_walk(const MpNavAnchor *a, const MpNavAnchor *b, void *unused)
{
    int x = a->pad_id, y = b->pad_id;
    (void)unused;
    return (x == 1 && (y == 3 || y == 6)) ||
           (y == 1 && (x == 3 || x == 6)) ||
           (x == 3 && y == 4) || (x == 4 && y == 3) ||
           (x == 4 && y == 5) || (x == 5 && y == 4) ||
           (x == 5 && y == 2) || (x == 2 && y == 5);
}

static int doorway_walk(const MpNavAnchor *a, const MpNavAnchor *b, void *unused)
{
    int x = a->pad_id, y = b->pad_id;
    (void)unused;
    return (x == 0 && y == 1) || (x == 1 && y == 0) ||
           (x == 2 && y == 3) || (x == 3 && y == 2) ||
           (x == 1 && y == 2) || (x == 2 && y == 1);
}

/* A stage can put spawn and weapon pads at opposite ends of a bent hall.
 * Floor-tile centres supply the missing turns while the old pad IDs stay put. */
static int corner_walk(const MpNavAnchor *a, const MpNavAnchor *b, void *unused)
{
    int x = a->pad_id, y = b->pad_id;
    (void)unused;
    return (x == 0 && y == 3) || (x == 3 && y == 0) ||
           (x == 3 && y == 4) || (x == 4 && y == 3) ||
           (x == 4 && y == 5) || (x == 5 && y == 4) ||
           (x == 5 && y == 1) || (x == 1 && y == 5);
}

int main(void)
{
    MpNavGraph graph = {0}, reordered = {0};
    int route[6] = {-1, -1, -1, -1, -1, -1};
    MpNavAnchor pads[] = {
        {30, 2, 0, 0, 1}, {10, 0, 0, 0, 1},
        {40, 0, 0, 1, 1}, {20, 1, 0, 0, 1},
        {50, 100, 0, 0, 0}
    };
    MpNavAnchor order[] = {pads[4], pads[3], pads[1], pads[2], pads[0]};
    CHECK(mpNavGraphBuild(&graph, 45, pads, 5, 3, walk, NULL));
    CHECK(graph.node_count == 4);
    CHECK(graph.edge_count == 2);
    CHECK(graph.component_count == 2);
    CHECK(graph.largest_component == 3);
    CHECK(graph.isolated_nodes == 1);
    CHECK(graph.max_neighbours == 2);
    CHECK(graph.max_route_hops == 2);
    CHECK(mpNavGraphReachable(&graph, 10, 30));
    {
        float costs[4];
        CHECK(mpNavGraphCosts(&graph, 10, costs, 4));
        CHECK(mpNavGraphCostToPad(&graph, costs, 10) == 0.0f);
        CHECK(mpNavGraphCostToPad(&graph, costs, 30) == 2.0f);
        CHECK(mpNavGraphCostToPad(&graph, costs, 40) < 0.0f);
        CHECK(mpNavGraphCostToPad(&graph, costs, 50) < 0.0f);
        CHECK(!mpNavGraphCosts(&graph, 10, costs, 3));
        CHECK(!mpNavGraphCosts(&graph, 50, costs, 4));
    }
    CHECK(mpNavGraphRoute(&graph, 10, 30, route, 2) == 2);
    CHECK(route[0] == 10 && route[1] == 20 && route[2] == -1);
    CHECK(mpNavGraphRoute(&graph, 10, 30, route, 6) == 3);
    CHECK(route[0] == 10 && route[1] == 20 && route[2] == 30);
    CHECK(mpNavGraphRoute(&graph, 10, 10, route, 6) == 1 && route[0] == 10);
    CHECK(mpNavGraphRoute(&graph, 10, 40, route, 6) == 0);
    CHECK(!mpNavGraphReachable(&graph, 10, 40));
    CHECK(!mpNavGraphReachable(&graph, 10, 50));
    CHECK(mpNavGraphBuild(&reordered, 45, order, 5, 3, walk, NULL));
    CHECK(memcmp(graph.edges, reordered.edges, 16) == 0);
    CHECK(mpNavGraphRoute(&reordered, 10, 30, route, 6) == 3);
    CHECK(route[0] == 10 && route[1] == 20 && route[2] == 30);
    CHECK(!mpNavGraphBuild(&graph, 45, pads, 5, 3, NULL, NULL));
    CHECK(mpNavGraphReachable(&graph, 10, 30)); /* Failed rebuild preserves stage. */
    CHECK(mpNavGraphBuild(&graph, 46, pads, 5, 3, one_way, NULL));
    CHECK(graph.edge_count == 0); /* Both directions must be walkable. */
    CHECK(graph.stage == 46);
    mpNavGraphClear(&graph);
    CHECK(!mpNavGraphReachable(&graph, 10, 30));
    CHECK(graph.node_count == 0);
    mpNavGraphClear(&reordered);
    {
        /* The nearest-pad pass sees only same-room neighbours. PD's
         * authored route has the doorway link; the generated adapter must
         * discover a validated cross-component link as well. */
        MpNavAnchor rooms[] = {{0, 0, 0, 0, 1}, {1, 1, 0, 0, 1},
                               {2, 8, 0, 0, 1}, {3, 9, 0, 0, 1}};
        CHECK(mpNavGraphBuild(&graph, 49, rooms, 4, 1, doorway_walk, NULL));
        CHECK(graph.component_count == 1 && graph.edge_count == 3);
        CHECK(mpNavGraphRoute(&graph, 0, 3, route, 6) == 4);
        CHECK(route[0] == 0 && route[1] == 1 && route[2] == 2 && route[3] == 3);
        mpNavGraphClear(&graph);
    }
    {
        /* The nearest-looking pad is behind a wall; the graph route runs
         * around its far side, so the genuinely cheap pickup should win. */
        MpNavAnchor detour[] = {{1, 0, 0, 0, 1}, {2, 0, 0, 1, 1},
                                {3, 0, 0, 100, 1}, {4, 1, 0, 100, 1},
                                {5, 1, 0, 50, 1}, {6, -30, 0, 0, 1}};
        float costs[6];
        CHECK(mpNavGraphBuild(&graph, 48, detour, 6, 5, detour_walk, NULL));
        CHECK(mpNavGraphCosts(&graph, 1, costs, 6));
        CHECK(mpNavGraphCostToPad(&graph, costs, 2) > 199.0f &&
              mpNavGraphCostToPad(&graph, costs, 2) < 202.0f);
        CHECK(mpNavGraphCostToPad(&graph, costs, 6) == 30.0f);
        CHECK(mpNavGraphCostToPad(&graph, costs, 2) >
              mpNavGraphCostToPad(&graph, costs, 6));
        mpNavGraphClear(&graph);
    }
    {
        MpNavAnchor bottleneck[] = {{0, 0, 0, 0, 1}, {1, 1, 0, 0, 1},
                                    {2, 2, 0, 0, 1}, {3, 3, 0, 0, 1}};
        int cutpads[4] = {-1, -1, -1, -1}, bridges = -1;
        CHECK(mpNavGraphBuild(&graph, 50, bottleneck, 4, 3, chain, NULL));
        CHECK(mpNavGraphBottlenecks(&graph, cutpads, 4, &bridges) == 2);
        CHECK(cutpads[0] == 1 && cutpads[1] == 2 && bridges == 3);
        mpNavGraphClear(&graph);
        {
            MpNavAnchor disconnected[] = {{0, 0, 0, 0, 1}, {1, 1, 0, 0, 1},
                                          {2, 2, 0, 0, 1}, {3, 3, 0, 0, 1},
                                          {10, 10, 0, 0, 1}, {11, 11, 0, 0, 1}};
            CHECK(mpNavGraphBuild(&graph, 50, disconnected, 6, 5, chain, NULL));
            CHECK(graph.component_count == 2);
            CHECK(mpNavGraphBottlenecks(&graph, cutpads, 4, &bridges) == 2 &&
                  cutpads[0] == 1 && cutpads[1] == 2 && bridges == 4);
            mpNavGraphClear(&graph);
        }
        CHECK(mpNavGraphBuild(&graph, 50, bottleneck, 4, 3, ring, NULL));
        CHECK(mpNavGraphBottlenecks(&graph, cutpads, 4, &bridges) == 0 &&
              bridges == 0);
        mpNavGraphClear(&graph);
    }
    {
        MpNavAnchor sparse[] = {{0, 0, 0, 0, 1}, {1, 10, 0, 10, 1}};
        MpNavAnchor floor[] = {{0, 0, 0, 0, 1}, {1, 10, 0, 10, 1},
                               {3, 0, 0, 5, 1}, {4, 5, 0, 5, 1},
                               {5, 10, 0, 5, 1}};
        CHECK(mpNavGraphBuild(&graph, 50, sparse, 2, 1, corner_walk, NULL));
        CHECK(!mpNavGraphReachable(&graph, 0, 1));
        CHECK(mpNavGraphBuild(&graph, 50, floor, 5, 4, corner_walk, NULL));
        CHECK(graph.component_count == 1);
        CHECK(mpNavGraphRoute(&graph, 0, 1, route, 6) == 5);
        CHECK(route[0] == 0 && route[1] == 3 && route[2] == 4 &&
              route[3] == 5 && route[4] == 1);
        mpNavGraphClear(&graph);
    }
    {
        MpNavAnchor corridor[9];
        for (int i = 0; i < 9; ++i)
            corridor[i] = (MpNavAnchor){i, (float)i, 0, 0, 1};
        CHECK(mpNavGraphBuild(&graph, 47, corridor, 9, 2, chain, NULL));
        CHECK(mpNavGraphRoute(&graph, 0, 8, route, 5) == 5);
        CHECK(route[0] == 0 && route[4] == 4);
        CHECK(mpNavGraphRoute(&graph, route[3], 8, route, 5) == 5);
        CHECK(route[0] == 3 && route[4] == 7);
        CHECK(mpNavGraphRoute(&graph, route[3], 8, route, 5) == 3);
        CHECK(route[0] == 6 && route[2] == 8);
        mpNavGraphClear(&graph);
    }
    return failures ? 1 : 0;
}
