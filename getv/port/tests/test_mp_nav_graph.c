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

int main(void)
{
    MpNavGraph graph = {0}, reordered = {0};
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
    CHECK(!mpNavGraphReachable(&graph, 10, 40));
    CHECK(!mpNavGraphReachable(&graph, 10, 50));
    CHECK(mpNavGraphBuild(&reordered, 45, order, 5, 3, walk, NULL));
    CHECK(memcmp(graph.edges, reordered.edges, 16) == 0);
    CHECK(!mpNavGraphBuild(&graph, 45, pads, 5, 3, NULL, NULL));
    CHECK(mpNavGraphReachable(&graph, 10, 30)); /* Failed rebuild preserves stage. */
    CHECK(mpNavGraphBuild(&graph, 46, pads, 5, 3, one_way, NULL));
    CHECK(graph.edge_count == 0); /* Both directions must be walkable. */
    CHECK(graph.stage == 46);
    mpNavGraphClear(&graph);
    CHECK(!mpNavGraphReachable(&graph, 10, 30));
    CHECK(graph.node_count == 0);
    mpNavGraphClear(&reordered);
    return failures ? 1 : 0;
}
