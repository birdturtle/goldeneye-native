#include "mp_nav_graph.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct MpNavCandidate {
    int index;
    double distance;
    int pad_id;
} MpNavCandidate;

static int mpNavSortAnchors(const void *left, const void *right)
{
    const MpNavAnchor *a = left, *b = right;
    return (a->pad_id > b->pad_id) - (a->pad_id < b->pad_id);
}

static int mpNavSortCandidates(const void *left, const void *right)
{
    const MpNavCandidate *a = left, *b = right;
    if (a->distance < b->distance) return -1;
    if (a->distance > b->distance) return 1;
    return (a->pad_id > b->pad_id) - (a->pad_id < b->pad_id);
}

void mpNavGraphClear(MpNavGraph *graph)
{
    if (!graph) return;
    free(graph->nodes);
    free(graph->edges);
    free(graph->component);
    memset(graph, 0, sizeof(*graph));
}

static int mpNavIndex(const MpNavGraph *graph, int pad_id)
{
    int lo = 0, hi = graph->node_count;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (graph->nodes[mid].pad_id < pad_id) lo = mid + 1;
        else hi = mid;
    }
    return lo < graph->node_count && graph->nodes[lo].pad_id == pad_id ? lo : -1;
}

int mpNavGraphReachable(const MpNavGraph *graph, int from_pad, int to_pad)
{
    int from, to;
    if (!graph || !graph->component) return 0;
    from = mpNavIndex(graph, from_pad);
    to = mpNavIndex(graph, to_pad);
    return from >= 0 && to >= 0 &&
           graph->component[from] == graph->component[to];
}

int mpNavGraphRoute(const MpNavGraph *graph, int from_pad, int to_pad,
                    int *pad_ids, int capacity)
{
    int from, to, *previous, *queue, head = 0, tail = 0, count = 0, written;
    int i, current;
    if (!graph || !graph->nodes || !graph->edges || !graph->component ||
        !pad_ids || capacity <= 0) return 0;
    from = mpNavIndex(graph, from_pad);
    to = mpNavIndex(graph, to_pad);
    if (from < 0 || to < 0 || graph->component[from] != graph->component[to]) return 0;
    previous = malloc((size_t)graph->node_count * sizeof(*previous));
    queue = malloc((size_t)graph->node_count * sizeof(*queue));
    if (!previous || !queue) { free(previous); free(queue); return 0; }
    for (i = 0; i < graph->node_count; ++i) previous[i] = -1;
    previous[from] = from;
    queue[tail++] = from;
    while (head < tail && previous[to] < 0) {
        current = queue[head++];
        for (i = 0; i < graph->node_count; ++i) {
            if (graph->edges[(size_t)current * graph->node_count + i] &&
                previous[i] < 0) {
                previous[i] = current;
                queue[tail++] = i;
            }
        }
    }
    if (previous[to] >= 0) {
        /* The BFS queue is no longer needed. Reuse it to reverse the route. */
        for (current = to;; current = previous[current]) {
            queue[count++] = current;
            if (current == from) break;
        }
        written = count < capacity ? count : capacity;
        for (i = 0; i < written; ++i)
            pad_ids[i] = graph->nodes[queue[count - 1 - i]].pad_id;
    } else {
        written = 0;
    }
    free(previous);
    free(queue);
    return written;
}

int mpNavGraphBuild(MpNavGraph *graph, int stage, const MpNavAnchor *pads,
                    int pad_count, int candidate_limit,
                    MpNavDirectWalk direct_walk, void *context)
{
    MpNavGraph next = {0};
    MpNavCandidate *candidates = NULL;
    int *queue = NULL, *dist = NULL;
    int i, j, n = 0;

    if (!graph || stage <= 0 || !pads || pad_count <= 0 ||
        candidate_limit <= 0 || !direct_walk) return 0;
    for (i = 0; i < pad_count; i++) n += pads[i].has_stan != 0;
    if (n == 0 || (size_t)n > SIZE_MAX / (size_t)n ||
        (size_t)n > SIZE_MAX / sizeof(*next.nodes) ||
        (size_t)n > SIZE_MAX / sizeof(*candidates) ||
        (size_t)n > SIZE_MAX / sizeof(*next.component) || n > INT_MAX / 2)
        return 0;
    next.nodes = malloc((size_t)n * sizeof(*next.nodes));
    next.edges = calloc((size_t)n * (size_t)n, 1);
    next.component = malloc((size_t)n * sizeof(*next.component));
    candidates = malloc((size_t)n * sizeof(*candidates));
    queue = malloc((size_t)n * sizeof(*queue));
    dist = malloc((size_t)n * sizeof(*dist));
    if (!next.nodes || !next.edges || !next.component || !candidates || !queue || !dist)
        goto failed;
    next.stage = stage;
    next.node_count = n;
    for (i = 0, j = 0; i < pad_count; i++)
        if (pads[i].has_stan) next.nodes[j++] = pads[i];
    qsort(next.nodes, (size_t)n, sizeof(*next.nodes), mpNavSortAnchors);
    for (i = 1; i < n; i++)
        if (next.nodes[i].pad_id == next.nodes[i - 1].pad_id) goto failed;

    for (i = 0; i < n; i++) {
        int count = 0, limit;
        for (j = 0; j < n; j++) {
            double dx, dy, dz;
            if (i == j) continue;
            dx = (double)next.nodes[j].x - next.nodes[i].x;
            dy = (double)next.nodes[j].y - next.nodes[i].y;
            dz = (double)next.nodes[j].z - next.nodes[i].z;
            candidates[count++] = (MpNavCandidate){j, dx*dx + dy*dy + dz*dz,
                                                   next.nodes[j].pad_id};
        }
        qsort(candidates, (size_t)count, sizeof(*candidates), mpNavSortCandidates);
        limit = count < candidate_limit ? count : candidate_limit;
        for (j = 0; j < limit; j++) {
            int k = candidates[j].index;
            if (next.edges[(size_t)i * n + k]) continue;
            if (direct_walk(&next.nodes[i], &next.nodes[k], context) &&
                direct_walk(&next.nodes[k], &next.nodes[i], context)) {
                next.edges[(size_t)i * n + k] = 1;
                next.edges[(size_t)k * n + i] = 1;
                next.edge_count++;
            }
        }
    }

    for (i = 0; i < n; i++) {
        int degree = 0;
        next.component[i] = -1;
        for (j = 0; j < n; j++) degree += next.edges[(size_t)i * n + j] != 0;
        if (!degree) next.isolated_nodes++;
        if (degree > next.max_neighbours) next.max_neighbours = degree;
    }
    for (i = 0; i < n; i++) {
        int head = 0, tail = 0;
        if (next.component[i] != -1) continue;
        next.component[i] = next.component_count;
        queue[tail++] = i;
        while (head < tail) {
            int current = queue[head++];
            for (j = 0; j < n; j++) {
                if (next.edges[(size_t)current * n + j] && next.component[j] == -1) {
                    next.component[j] = next.component_count;
                    queue[tail++] = j;
                }
            }
        }
        if (tail > next.largest_component) next.largest_component = tail;
        next.component_count++;
    }
    /* Graph diameter is diagnostic. The actor's native six-waypoint route
     * buffer must never be confused with this total path length. */
    next.max_route_hops = n > 256 ? -1 : 0;
    for (i = 0; i < n && n <= 256; i++) {
        int head = 0, tail = 0;
        for (j = 0; j < n; j++) dist[j] = -1;
        dist[i] = 0;
        queue[tail++] = i;
        while (head < tail) {
            int current = queue[head++];
            for (j = 0; j < n; j++) {
                if (next.edges[(size_t)current * n + j] && dist[j] == -1) {
                    dist[j] = dist[current] + 1;
                    if (dist[j] > next.max_route_hops) next.max_route_hops = dist[j];
                    queue[tail++] = j;
                }
            }
        }
    }
    free(candidates);
    free(queue);
    free(dist);
    mpNavGraphClear(graph);
    *graph = next;
    return 1;

failed:
    free(candidates);
    free(queue);
    free(dist);
    mpNavGraphClear(&next);
    return 0;
}
