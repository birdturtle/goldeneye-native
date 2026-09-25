#include "mp_nav_graph.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct MpNavCandidate {
    int index;
    double distance;
    int pad_id;
} MpNavCandidate;

typedef struct MpNavBridge {
    int from, to;
    double distance;
} MpNavBridge;

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

static int mpNavSortBridges(const void *left, const void *right)
{
    const MpNavBridge *a = left, *b = right;
    if (a->distance < b->distance) return -1;
    if (a->distance > b->distance) return 1;
    if (a->from != b->from) return (a->from > b->from) - (a->from < b->from);
    return (a->to > b->to) - (a->to < b->to);
}

static void mpNavMeasureComponents(MpNavGraph *graph, int *queue)
{
    int n = graph->node_count;
    graph->component_count = graph->largest_component = 0;
    graph->isolated_nodes = graph->max_neighbours = 0;
    for (int i = 0; i < n; ++i) {
        int degree = 0;
        graph->component[i] = -1;
        for (int j = 0; j < n; ++j)
            degree += graph->edges[(size_t)i * n + j] != 0;
        if (!degree) graph->isolated_nodes++;
        if (degree > graph->max_neighbours) graph->max_neighbours = degree;
    }
    for (int i = 0; i < n; ++i) {
        int head = 0, tail = 0;
        if (graph->component[i] != -1) continue;
        graph->component[i] = graph->component_count;
        queue[tail++] = i;
        while (head < tail) {
            int current = queue[head++];
            for (int j = 0; j < n; ++j) {
                if (graph->edges[(size_t)current * n + j] &&
                    graph->component[j] == -1) {
                    graph->component[j] = graph->component_count;
                    queue[tail++] = j;
                }
            }
        }
        if (tail > graph->largest_component) graph->largest_component = tail;
        graph->component_count++;
    }
}

/* PD's authored waypoint graph has links between room clusters. A nearest-pad
 * budget has no such guarantee: on a dense map all 24 candidates can lie on
 * the same side of a doorway. After the local pass, search for a bounded set
 * of inter-component links and admit only segments the stage walker accepts
 * in both directions. This never invents a link through a wall. */
static void mpNavBridgeComponents(MpNavGraph *graph, MpNavCandidate *candidates,
                                  MpNavDirectWalk direct_walk, void *context)
{
    int n = graph->node_count, count = 0, attempts = 0;
    int per_node = n - 1 < 32 ? n - 1 : 32;
    MpNavBridge *bridges;
    if (graph->component_count <= 1 || per_node <= 0 ||
        (size_t)n > SIZE_MAX / (size_t)per_node / sizeof(*bridges)) return;
    bridges = malloc((size_t)n * per_node * sizeof(*bridges));
    if (!bridges) return;
    for (int i = 0; i < n; ++i) {
        int choices = 0, written = 0;
        for (int j = i + 1; j < n; ++j) {
            double dx, dy, dz;
            if (graph->component[i] == graph->component[j]) continue;
            dx = (double)graph->nodes[j].x - graph->nodes[i].x;
            dy = (double)graph->nodes[j].y - graph->nodes[i].y;
            dz = (double)graph->nodes[j].z - graph->nodes[i].z;
            candidates[choices++] = (MpNavCandidate){j, dx*dx + dy*dy + dz*dz,
                                                       graph->nodes[j].pad_id};
        }
        qsort(candidates, (size_t)choices, sizeof(*candidates), mpNavSortCandidates);
        while (written < choices && written < per_node) {
            bridges[count++] = (MpNavBridge){i, candidates[written].index,
                                              candidates[written].distance};
            written++;
        }
    }
    qsort(bridges, (size_t)count, sizeof(*bridges), mpNavSortBridges);
    for (int k = 0; k < count && attempts < 256 && graph->component_count > 1; ++k) {
        int i = bridges[k].from, j = bridges[k].to;
        int old, replacement;
        if (graph->component[i] == graph->component[j]) continue;
        attempts++;
        if (!direct_walk(&graph->nodes[i], &graph->nodes[j], context) ||
            !direct_walk(&graph->nodes[j], &graph->nodes[i], context)) continue;
        graph->edges[(size_t)i * n + j] = graph->edges[(size_t)j * n + i] = 1;
        graph->edge_count++;
        old = graph->component[j];
        replacement = graph->component[i];
        for (int m = 0; m < n; ++m)
            if (graph->component[m] == old) graph->component[m] = replacement;
        graph->component_count--;
    }
    free(bridges);
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

int mpNavGraphCosts(const MpNavGraph *graph, int from_pad,
                    float *costs, int capacity)
{
    unsigned char *settled;
    int start, i, step, n;
    if (!graph || !graph->nodes || !graph->edges || !costs ||
        graph->node_count <= 0 || capacity < graph->node_count) return 0;
    n = graph->node_count;
    start = mpNavIndex(graph, from_pad);
    if (start < 0) return 0;
    settled = calloc((size_t)n, 1);
    if (!settled) return 0;
    for (i = 0; i < n; ++i) costs[i] = -1.0f;
    costs[start] = 0.0f;
    for (step = 0; step < n; ++step) {
        int current = -1, j;
        for (i = 0; i < n; ++i)
            if (!settled[i] && costs[i] >= 0.0f &&
                (current < 0 || costs[i] < costs[current])) current = i;
        if (current < 0) break;
        settled[current] = 1;
        for (j = 0; j < n; ++j) {
            float dx, dy, dz, next;
            if (!graph->edges[(size_t)current * n + j] || settled[j]) continue;
            dx = graph->nodes[current].x - graph->nodes[j].x;
            dy = graph->nodes[current].y - graph->nodes[j].y;
            dz = graph->nodes[current].z - graph->nodes[j].z;
            next = costs[current] + sqrtf(dx * dx + dz * dz + 16.0f * dy * dy);
            if (costs[j] < 0.0f || next < costs[j]) costs[j] = next;
        }
    }
    free(settled);
    return 1;
}

float mpNavGraphCostToPad(const MpNavGraph *graph, const float *costs,
                          int to_pad)
{
    int index;
    if (!graph || !graph->nodes || !costs) return -1.0f;
    index = mpNavIndex(graph, to_pad);
    return index >= 0 ? costs[index] : -1.0f;
}

typedef struct MpNavCutSearch {
    const MpNavGraph *graph;
    int *discovered, *low, *parent;
    unsigned char *cut;
    int clock, bridges;
} MpNavCutSearch;

static void mpNavFindCuts(MpNavCutSearch *search, int u)
{
    int children = 0, n = search->graph->node_count;
    search->discovered[u] = search->low[u] = ++search->clock;
    for (int v = 0; v < n; ++v) {
        if (!search->graph->edges[(size_t)u * n + v]) continue;
        if (!search->discovered[v]) {
            search->parent[v] = u;
            ++children;
            mpNavFindCuts(search, v);
            if (search->low[v] < search->low[u]) search->low[u] = search->low[v];
            if (search->parent[u] >= 0 &&
                search->low[v] >= search->discovered[u]) search->cut[u] = 1;
            if (search->low[v] > search->discovered[u]) ++search->bridges;
        } else if (v != search->parent[u] &&
                   search->discovered[v] < search->low[u]) {
            search->low[u] = search->discovered[v];
        }
    }
    if (search->parent[u] < 0 && children > 1) search->cut[u] = 1;
}

int mpNavGraphBottlenecks(const MpNavGraph *graph, int *pad_ids, int capacity,
                          int *bridge_count)
{
    MpNavCutSearch search = {0};
    int count = 0, n;
    if (!graph || !graph->nodes || !graph->edges || !bridge_count ||
        capacity < 0 || (capacity && !pad_ids)) return -1;
    n = graph->node_count;
    search.graph = graph;
    search.discovered = calloc((size_t)n, sizeof(int));
    search.low = calloc((size_t)n, sizeof(int));
    search.parent = malloc((size_t)n * sizeof(int));
    search.cut = calloc((size_t)n, 1);
    if (!search.discovered || !search.low || !search.parent || !search.cut) {
        free(search.discovered); free(search.low);
        free(search.parent); free(search.cut);
        return -1;
    }
    for (int i = 0; i < n; ++i) search.parent[i] = -1;
    for (int i = 0; i < n; ++i)
        if (!search.discovered[i]) mpNavFindCuts(&search, i);
    for (int i = 0; i < n; ++i)
        if (search.cut[i]) {
            if (count < capacity) pad_ids[count] = graph->nodes[i].pad_id;
            ++count;
        }
    *bridge_count = search.bridges;
    free(search.discovered); free(search.low);
    free(search.parent); free(search.cut);
    return count;
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

    mpNavMeasureComponents(&next, queue);
    mpNavBridgeComponents(&next, candidates, direct_walk, context);
    mpNavMeasureComponents(&next, queue);
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
