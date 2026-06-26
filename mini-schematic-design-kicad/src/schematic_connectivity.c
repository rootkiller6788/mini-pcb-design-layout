/**
 * @file schematic_connectivity.c
 * @brief Graph-theoretic connectivity analysis for schematic netlists
 *
 * Implements connectivity verification, component detection,
 * shortest path, cycle detection, antenna loop identification,
 * critical net analysis, and Manhattan routing checks.
 *
 * Knowledge: L1-L6 Complete, L7 Partial (EMI), L8 Partial (Planarity)
 * Course: MIT 6.042, CMU 15-251, Berkeley EE219, ETH 227-0455
 * Reference: Deo (1974), Tarjan (1974), Chua & Lin (1975)
 */

#include "schematic_connectivity.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

/* ─── Incidence Matrix (L3: CSR sparse format) ─── */

incidence_matrix_t* connectivity_build_incidence(const schematic_design_t *sch)
{
    if (!sch) return NULL;
    int total_pins = schematic_total_pins(sch);
    if (total_pins == 0 || sch->num_nets == 0) return NULL;
    incidence_matrix_t *im = calloc(1, sizeof(incidence_matrix_t));
    if (!im) return NULL;
    im->num_pins = total_pins; im->num_nets = sch->num_nets;
    im->pin_refs = calloc(total_pins, sizeof(char*));
    im->pin_nums = calloc(total_pins, sizeof(char*));
    if (!im->pin_refs || !im->pin_nums) { incidence_matrix_free(im); return NULL; }
    int *nnz_per_row = calloc(total_pins, sizeof(int));
    if (!nnz_per_row) { incidence_matrix_free(im); return NULL; }
    int row = 0;
    for (int c = 0; c < sch->num_components; c++) {
        schematic_component_t *comp = &sch->components[c];
        for (int p = 0; p < comp->num_pins; p++) {
            im->pin_refs[row] = strdup(comp->reference);
            im->pin_nums[row] = strdup(comp->pins[p].number);
            for (int n = 0; n < sch->num_nets; n++) {
                for (int conn = 0; conn < sch->nets[n].num_connections; conn++) {
                    if (strcmp(sch->nets[n].connections[conn].ref, comp->reference) == 0 &&
                        strcmp(sch->nets[n].connections[conn].pin, comp->pins[p].number) == 0) {
                        nnz_per_row[row] = 1; break;
                    }
                }
                if (nnz_per_row[row] > 0) break;
            }
            row++;
        }
    }
    im->row_offsets = calloc(total_pins + 1, sizeof(int));
    if (!im->row_offsets) { free(nnz_per_row); incidence_matrix_free(im); return NULL; }
    im->row_offsets[0] = 0;
    for (int i = 0; i < total_pins; i++) im->row_offsets[i + 1] = im->row_offsets[i] + nnz_per_row[i];
    int total_nnz = im->row_offsets[total_pins];
    im->col_indices = calloc(total_nnz, sizeof(int));
    im->values = calloc(total_nnz, sizeof(double));
    im->net_names = calloc(sch->num_nets, sizeof(char*));
    if (!im->col_indices || !im->values || !im->net_names) {
        free(nnz_per_row); incidence_matrix_free(im); return NULL;
    }
    int *fill = calloc(total_pins, sizeof(int));
    if (!fill) { free(nnz_per_row); incidence_matrix_free(im); return NULL; }
    row = 0;
    for (int c = 0; c < sch->num_components; c++) {
        schematic_component_t *comp = &sch->components[c];
        for (int p = 0; p < comp->num_pins; p++) {
            for (int n = 0; n < sch->num_nets; n++) {
                bool found = false;
                for (int conn = 0; conn < sch->nets[n].num_connections; conn++) {
                    if (strcmp(sch->nets[n].connections[conn].ref, comp->reference) == 0 &&
                        strcmp(sch->nets[n].connections[conn].pin, comp->pins[p].number) == 0)
                        { found = true; break; }
                }
                if (found) {
                    int offset = im->row_offsets[row];
                    im->col_indices[offset + fill[row]] = n;
                    im->values[offset + fill[row]] = 1.0;
                    fill[row]++; break;
                }
            }
            row++;
        }
    }
    for (int n = 0; n < sch->num_nets; n++) im->net_names[n] = strdup(sch->nets[n].name);
    free(fill); free(nnz_per_row);
    return im;
}

void incidence_matrix_free(incidence_matrix_t *im)
{
    if (!im) return;
    if (im->pin_refs) {
        for (int i = 0; i < im->num_pins; i++) free(im->pin_refs[i]);
        free(im->pin_refs);
    }
    if (im->pin_nums) {
        for (int i = 0; i < im->num_pins; i++) free(im->pin_nums[i]);
        free(im->pin_nums);
    }
    if (im->net_names) {
        for (int i = 0; i < im->num_nets; i++) free(im->net_names[i]);
        free(im->net_names);
    }
    free(im->row_offsets); free(im->col_indices); free(im->values);
    free(im);
}

/* ─── Adjacency Matrix: A = B * B^T (L3) ─── */

adjacency_matrix_t* connectivity_build_adjacency(const incidence_matrix_t *im)
{
    if (!im) return NULL;
    int n = im->num_pins;
    adjacency_matrix_t *am = calloc(1, sizeof(adjacency_matrix_t));
    if (!am) return NULL;
    am->dimension = n;
    am->data = calloc(n * n, sizeof(double));
    am->degrees = calloc(n, sizeof(int));
    if (!am->data || !am->degrees) { adjacency_matrix_free(am); return NULL; }
    for (int i = 0; i < im->num_pins; i++) {
        int rs = im->row_offsets[i], re = im->row_offsets[i + 1];
        for (int nz_i = rs; nz_i < re; nz_i++) {
            int net = im->col_indices[nz_i];
            for (int j = 0; j < im->num_pins; j++) {
                int rs2 = im->row_offsets[j], re2 = im->row_offsets[j + 1];
                for (int nz_j = rs2; nz_j < re2; nz_j++) {
                    if (im->col_indices[nz_j] == net) {
                        am->data[i * n + j] += 1.0; break;
                    }
                }
            }
        }
        for (int j = 0; j < n; j++)
            if (am->data[i * n + j] > 0.0) am->degrees[i]++;
    }
    return am;
}

void adjacency_matrix_free(adjacency_matrix_t *am)
{
    if (!am) return;
    free(am->data); free(am->degrees); free(am);
}

/* ─── Connected Components (L5: DFS on CSR graph) ─── */

int connectivity_find_components(const schematic_design_t *sch,
                                  connected_component_t **comps)
{
    if (!sch || !comps) return -1;
    *comps = NULL;
    netlist_graph_t *g = schematic_build_graph(sch);
    if (!g) return -1;
    int *comp_id = calloc(g->num_vertices, sizeof(int));
    if (!comp_id) { netlist_graph_free(g); return -1; }
    int num_comps = netlist_graph_connected_components(g, comp_id);
    if (num_comps <= 0) { free(comp_id); netlist_graph_free(g); return 0; }
    connected_component_t *result = calloc(num_comps, sizeof(connected_component_t));
    if (!result) { free(comp_id); netlist_graph_free(g); return -1; }
    for (int i = 0; i < g->num_vertices; i++) {
        int cid = comp_id[i];
        if (cid >= 0 && cid < num_comps) result[cid].num_vertices++;
    }
    for (int c = 0; c < num_comps; c++) {
        result[c].component_id = c;
        result[c].vertex_indices = calloc(result[c].num_vertices, sizeof(int));
        if (!result[c].vertex_indices) {
            for (int d = 0; d < c; d++) free(result[d].vertex_indices);
            free(result); free(comp_id); netlist_graph_free(g); return -1;
        }
        result[c].num_vertices = 0;
    }
    for (int i = 0; i < g->num_vertices; i++) {
        int cid = comp_id[i];
        if (cid >= 0 && cid < num_comps)
            result[cid].vertex_indices[result[cid].num_vertices++] = i;
    }
    for (int c = 0; c < num_comps; c++) {
        result[c].is_powered = false;
        result[c].is_grounded = false;
        result[c].num_nets = result[c].num_vertices;
    }
    free(comp_id); netlist_graph_free(g);
    *comps = result;
    return num_comps;
}

/* ─── Connectivity Metrics (L2) ─── */

connectivity_metrics_t connectivity_compute_metrics(const schematic_design_t *sch)
{
    connectivity_metrics_t m; memset(&m, 0, sizeof(m));
    if (!sch) return m;
    m.num_vertices = schematic_total_pins(sch);
    m.num_edges = schematic_total_connections(sch);
    connected_component_t *comps = NULL;
    m.num_connected_components = connectivity_find_components(sch, &comps);
    if (comps) {
        for (int i = 0; i < m.num_connected_components; i++) free(comps[i].vertex_indices);
        free(comps);
    }
    if (m.num_vertices > 0) {
        m.avg_degree = (double)m.num_edges / (double)m.num_vertices;
        int max_conn = 0;
        for (int i = 0; i < sch->num_nets; i++)
            if (sch->nets[i].num_connections > max_conn) max_conn = sch->nets[i].num_connections;
        m.max_degree = (double)max_conn;
    }
    m.num_single_pin_nets = 0; m.num_floating_subnets = 0;
    for (int i = 0; i < sch->num_nets; i++) {
        if (sch->nets[i].num_connections < 2) {
            m.num_single_pin_nets++;
            if (!sch->nets[i].is_power_net) m.num_floating_subnets++;
        }
    }
    m.is_fully_connected = (m.num_connected_components == 1);
    return m;
}

/* ─── Shortest Path (L5: BFS) ─── */

connectivity_path_t* connectivity_shortest_path(
    const schematic_design_t *sch, const char *ref_a, const char *pin_a,
    const char *ref_b, const char *pin_b)
{
    if (!sch || !ref_a || !pin_a || !ref_b || !pin_b) return NULL;
    netlist_graph_t *g = schematic_build_graph(sch);
    if (!g) return NULL;
    int start_v = -1, end_v = -1;
    for (int i = 0; i < g->num_vertices; i++) {
        if (start_v < 0 && g->vertex_refs[i] && g->vertex_pins[i] &&
            strcmp(g->vertex_refs[i], ref_a) == 0 && strcmp(g->vertex_pins[i], pin_a) == 0)
            start_v = i;
        if (end_v < 0 && g->vertex_refs[i] && g->vertex_pins[i] &&
            strcmp(g->vertex_refs[i], ref_b) == 0 && strcmp(g->vertex_pins[i], pin_b) == 0)
            end_v = i;
    }
    if (start_v < 0 || end_v < 0) { netlist_graph_free(g); return NULL; }
    int *path = calloc(g->num_vertices, sizeof(int));
    if (!path) { netlist_graph_free(g); return NULL; }
    int path_len = netlist_graph_shortest_path(g, start_v, end_v, path, g->num_vertices);
    if (path_len <= 0) { free(path); netlist_graph_free(g); return NULL; }
    connectivity_path_t *cp = calloc(1, sizeof(connectivity_path_t));
    if (!cp) { free(path); netlist_graph_free(g); return NULL; }
    cp->path_length = path_len;
    cp->vertices = calloc(path_len, sizeof(int));
    cp->nets = calloc(path_len > 1 ? path_len - 1 : 1, sizeof(int));
    if (!cp->vertices || !cp->nets) {
        connectivity_path_free(cp); free(path); netlist_graph_free(g); return NULL;
    }
    memcpy(cp->vertices, path, path_len * sizeof(int));
    for (int i = 0; i < path_len - 1; i++) {
        int v1 = path[i], v2 = path[i + 1]; cp->nets[i] = -1;
        for (int n = 0; n < sch->num_nets; n++) {
            bool has_v1 = false, has_v2 = false;
            for (int c = 0; c < sch->nets[n].num_connections; c++) {
                if (strcmp(sch->nets[n].connections[c].ref, g->vertex_refs[v1]) == 0 &&
                    strcmp(sch->nets[n].connections[c].pin, g->vertex_pins[v1]) == 0) has_v1 = true;
                if (strcmp(sch->nets[n].connections[c].ref, g->vertex_refs[v2]) == 0 &&
                    strcmp(sch->nets[n].connections[c].pin, g->vertex_pins[v2]) == 0) has_v2 = true;
            }
            if (has_v1 && has_v2) { cp->nets[i] = n; break; }
        }
    }
    cp->total_distance = (double)(path_len - 1);
    free(path); netlist_graph_free(g);
    return cp;
}

void connectivity_path_free(connectivity_path_t *path)
{
    if (!path) return;
    free(path->vertices); free(path->nets); free(path);
}

/* ─── Net Connectivity & Reachability ─── */

bool connectivity_nets_connected(const schematic_design_t *sch,
                                  const char *net_a, const char *net_b)
{
    if (!sch || !net_a || !net_b) return false;
    if (strcmp(net_a, net_b) == 0) return true;
    int na = schematic_find_net(sch, net_a);
    int nb = schematic_find_net(sch, net_b);
    if (na < 0 || nb < 0) return false;
    for (int ca = 0; ca < sch->nets[na].num_connections; ca++)
        for (int cb = 0; cb < sch->nets[nb].num_connections; cb++)
            if (strcmp(sch->nets[na].connections[ca].ref,
                       sch->nets[nb].connections[cb].ref) == 0 &&
                strcmp(sch->nets[na].connections[ca].pin,
                       sch->nets[nb].connections[cb].pin) == 0)
                return true;
    return false;
}

int connectivity_reachable_components(const schematic_design_t *sch,
                                       const char *ref, const char *pin)
{
    if (!sch || !ref || !pin) return -1;
    netlist_graph_t *g = schematic_build_graph(sch);
    if (!g) return -1;
    int start_v = -1;
    for (int i = 0; i < g->num_vertices; i++)
        if (g->vertex_refs[i] && strcmp(g->vertex_refs[i], ref) == 0 &&
            g->vertex_pins[i] && strcmp(g->vertex_pins[i], pin) == 0)
            { start_v = i; break; }
    if (start_v < 0) { netlist_graph_free(g); return -1; }
    bool *visited = calloc(g->num_vertices, sizeof(bool));
    int *queue = calloc(g->num_vertices, sizeof(int));
    if (!visited || !queue) { free(visited); free(queue); netlist_graph_free(g); return -1; }
    int head = 0, tail = 0;
    visited[start_v] = true; queue[tail++] = start_v;
    while (head < tail) {
        int v = queue[head++];
        int os = g->adjacency_offsets[v], oe = g->adjacency_offsets[v + 1];
        for (int e = os; e < oe; e++) {
            int w = g->adjacency_targets[e];
            if (!visited[w]) { visited[w] = true; queue[tail++] = w; }
        }
    }
    int count = 0;
    char **seen = calloc(g->num_vertices, sizeof(char*));
    int seen_cnt = 0;
    for (int i = 0; i < g->num_vertices; i++) {
        if (visited[i] && g->vertex_refs[i]) {
            bool dup = false;
            for (int j = 0; j < seen_cnt; j++)
                if (strcmp(seen[j], g->vertex_refs[i]) == 0) { dup = true; break; }
            if (!dup) { seen[seen_cnt++] = strdup(g->vertex_refs[i]); count++; }
        }
    }
    for (int j = 0; j < seen_cnt; j++) free(seen[j]);
    free(seen); free(queue); free(visited);
    netlist_graph_free(g); return count;
}

/* ─── Cycle Detection (L5: DFS back-edges) ─── */

int connectivity_find_cycles(const schematic_design_t *sch,
                              netlist_cycle_t **cycles)
{
    if (!sch || !cycles) return -1;
    *cycles = NULL;
    netlist_graph_t *g = schematic_build_graph(sch);
    if (!g || g->num_vertices == 0) { if (g) netlist_graph_free(g); return 0; }
    int *parent = calloc(g->num_vertices, sizeof(int));
    int *depth = calloc(g->num_vertices, sizeof(int));
    bool *in_stack = calloc(g->num_vertices, sizeof(bool));
    int *stack = calloc(g->num_vertices, sizeof(int));
    if (!parent || !depth || !in_stack || !stack) {
        free(parent); free(depth); free(in_stack); free(stack);
        netlist_graph_free(g); return -1;
    }
    for (int i = 0; i < g->num_vertices; i++) { parent[i] = -1; depth[i] = -1; }
    int dtime = 0, top = 0, capacity = 16, found_count = 0;
    netlist_cycle_t *found = calloc(capacity, sizeof(netlist_cycle_t));
    if (!found) {
        free(parent); free(depth); free(in_stack); free(stack);
        netlist_graph_free(g); return -1;
    }
    for (int start = 0; start < g->num_vertices; start++) {
        if (depth[start] >= 0) continue;
        top = 0; stack[top++] = start;
        parent[start] = -2; depth[start] = dtime++; in_stack[start] = true;
        while (top > 0) {
            int v = stack[top - 1]; bool done = true;
            int os = g->adjacency_offsets[v], oe = g->adjacency_offsets[v + 1];
            for (int e = os; e < oe; e++) {
                int w = g->adjacency_targets[e];
                if (depth[w] < 0) {
                    parent[w] = v; depth[w] = dtime++;
                    in_stack[w] = true; stack[top++] = w;
                    done = false; break;
                } else if (in_stack[w] && w != parent[v] && found_count < 50) {
                    if (found_count >= capacity) {
                        capacity *= 2;
                        void *tmp = realloc(found, capacity * sizeof(netlist_cycle_t));
                        if (!tmp) break;
                        found = tmp;
                    }
                    netlist_cycle_t *cyc = &found[found_count];
                    int clen = 0, curr = v;
                    while (curr >= 0 && curr != w && clen < 100) { clen++; curr = parent[curr]; }
                    clen += 2;
                    cyc->cycle_length = clen;
                    cyc->vertices = calloc(clen, sizeof(int));
                    if (cyc->vertices) {
                        int pos = 0; curr = v;
                        while (curr >= 0 && pos < clen - 1) {
                            cyc->vertices[pos++] = curr;
                            if (curr == w) break;
                            curr = parent[curr];
                        }
                        cyc->vertices[pos] = w;
                        cyc->is_ground_loop = false;
                        cyc->is_signal_loop = true;
                        cyc->loop_area_estimate = (double)clen * 1.0;
                        found_count++;
                    }
                }
            }
            if (done) in_stack[stack[--top]] = false;
        }
    }
    free(parent); free(depth); free(in_stack); free(stack);
    netlist_graph_free(g);
    *cycles = found;
    return found_count;
}

/* ─── Antenna Loop Detection (L6: EMI) ─── */

int connectivity_detect_antenna_loops(const schematic_design_t *sch,
                                       netlist_cycle_t **loops,
                                       double area_threshold)
{
    if (!sch || !loops) return -1;
    *loops = NULL;
    netlist_cycle_t *all_cycles = NULL;
    int num_cycles = connectivity_find_cycles(sch, &all_cycles);
    if (num_cycles <= 0) return 0;
    int capacity = 8, loop_count = 0;
    netlist_cycle_t *al = calloc(capacity, sizeof(netlist_cycle_t));
    if (!al) {
        for (int i = 0; i < num_cycles; i++) free(all_cycles[i].vertices);
        free(all_cycles); return -1;
    }
    for (int i = 0; i < num_cycles; i++) {
        netlist_cycle_t *cyc = &all_cycles[i];
        if (cyc->is_signal_loop && cyc->loop_area_estimate > area_threshold) {
            if (loop_count >= capacity) {
                capacity *= 2;
                void *tmp = realloc(al, capacity * sizeof(netlist_cycle_t));
                if (!tmp) {
                    for (int j = 0; j < loop_count; j++) free(al[j].vertices);
                    free(al);
                    for (int j = 0; j < num_cycles; j++) free(all_cycles[j].vertices);
                    free(all_cycles); return -1;
                }
                al = tmp;
            }
            al[loop_count] = *cyc;
            al[loop_count].vertices = calloc(cyc->cycle_length, sizeof(int));
            if (al[loop_count].vertices)
                memcpy(al[loop_count].vertices, cyc->vertices,
                       cyc->cycle_length * sizeof(int));
            loop_count++;
        }
    }
    for (int i = 0; i < num_cycles; i++) free(all_cycles[i].vertices);
    free(all_cycles);
    *loops = al;
    return loop_count;
}

/* ─── Star Ground Verification (L6) ─── */

int connectivity_verify_star_ground(const schematic_design_t *sch)
{
    if (!sch) return -1;
    int ground_net_count = 0;
    int ground_nets[32];
    for (int i = 0; i < sch->num_nets && ground_net_count < 32; i++) {
        if (sch->nets[i].is_power_net &&
            (strstr(sch->nets[i].name, "GND") ||
             strstr(sch->nets[i].name, "VSS") ||
             strstr(sch->nets[i].name, "GROUND")))
            ground_nets[ground_net_count++] = i;
    }
    if (ground_net_count <= 1) return 0;
    int violations = 0;
    for (int i = 1; i < ground_net_count; i++) {
        if (!connectivity_nets_connected(sch, sch->nets[ground_nets[0]].name,
                                          sch->nets[ground_nets[i]].name))
            violations++;
    }
    return violations;
}

/* ─── Critical Net Detection (L5: Tarjan 1974 bridge finding) ─── */

static void bridge_dfs(int u, int parent_u, int *disc, int *low,
                        bool *visited, int *time, const netlist_graph_t *g,
                        int *bridge_edges, int *bridge_count, int max_bridges)
{
    visited[u] = true;
    disc[u] = low[u] = ++(*time);
    int os = g->adjacency_offsets[u], oe = g->adjacency_offsets[u + 1];
    for (int e = os; e < oe; e++) {
        int v = g->adjacency_targets[e];
        if (!visited[v]) {
            bridge_dfs(v, u, disc, low, visited, time, g,
                       bridge_edges, bridge_count, max_bridges);
            low[u] = (low[u] < low[v]) ? low[u] : low[v];
            if (low[v] > disc[u] && *bridge_count < max_bridges)
                bridge_edges[(*bridge_count)++] = e;
        } else if (v != parent_u) {
            low[u] = (low[u] < disc[v]) ? low[u] : disc[v];
        }
    }
}

int connectivity_find_critical_nets(const schematic_design_t *sch,
                                     int *critical_net_indices,
                                     int max_critical)
{
    if (!sch || !critical_net_indices || max_critical <= 0) return -1;
    netlist_graph_t *g = schematic_build_graph(sch);
    if (!g || g->num_vertices == 0) { if (g) netlist_graph_free(g); return 0; }
    int *disc = calloc(g->num_vertices, sizeof(int));
    int *low = calloc(g->num_vertices, sizeof(int));
    bool *visited = calloc(g->num_vertices, sizeof(bool));
    if (!disc || !low || !visited) {
        free(disc); free(low); free(visited); netlist_graph_free(g); return -1;
    }
    int time = 0, bridge_count = 0;
    int *be = calloc(max_critical, sizeof(int));
    if (!be) {
        free(disc); free(low); free(visited); netlist_graph_free(g); return -1;
    }
    for (int i = 0; i < g->num_vertices; i++)
        if (!visited[i])
            bridge_dfs(i, -1, disc, low, visited, &time, g, be, &bridge_count, max_critical);
    int net_count = 0;
    for (int i = 0; i < bridge_count && net_count < max_critical; i++) {
        int candidate = be[i] % (sch->num_nets > 0 ? sch->num_nets : 1);
        bool dup = false;
        for (int j = 0; j < net_count; j++)
            if (critical_net_indices[j] == candidate) { dup = true; break; }
        if (!dup) critical_net_indices[net_count++] = candidate;
    }
    free(be); free(disc); free(low); free(visited);
    netlist_graph_free(g);
    return net_count;
}

/* ─── Manhattan Routability (L8: Planarity heuristic) ─── */

int connectivity_check_routability(const schematic_design_t *sch)
{
    if (!sch) return -1;
    int crossing_estimate = 0;
    for (int n1 = 0; n1 < sch->num_nets; n1++) {
        for (int n2 = n1 + 1; n2 < sch->num_nets; n2++) {
            double n1_min_x = DBL_MAX, n1_max_x = -DBL_MAX;
            double n1_min_y = DBL_MAX, n1_max_y = -DBL_MAX;
            double n2_min_x = DBL_MAX, n2_max_x = -DBL_MAX;
            double n2_min_y = DBL_MAX, n2_max_y = -DBL_MAX;
            for (int c1 = 0; c1 < sch->nets[n1].num_connections; c1++) {
                int cidx = schematic_find_component(sch, sch->nets[n1].connections[c1].ref);
                if (cidx >= 0) {
                    double cx = sch->components[cidx].pos_x;
                    double cy = sch->components[cidx].pos_y;
                    if (cx < n1_min_x) n1_min_x = cx;
                    if (cx > n1_max_x) n1_max_x = cx;
                    if (cy < n1_min_y) n1_min_y = cy;
                    if (cy > n1_max_y) n1_max_y = cy;
                }
            }
            for (int c2 = 0; c2 < sch->nets[n2].num_connections; c2++) {
                int cidx = schematic_find_component(sch, sch->nets[n2].connections[c2].ref);
                if (cidx >= 0) {
                    double cx = sch->components[cidx].pos_x;
                    double cy = sch->components[cidx].pos_y;
                    if (cx < n2_min_x) n2_min_x = cx;
                    if (cx > n2_max_x) n2_max_x = cx;
                    if (cy < n2_min_y) n2_min_y = cy;
                    if (cy > n2_max_y) n2_max_y = cy;
                }
            }
            bool x_int = (n1_min_x < n2_min_x && n1_max_x > n2_min_x) ||
                          (n2_min_x < n1_min_x && n2_max_x > n1_min_x);
            bool y_int = (n1_min_y < n2_min_y && n1_max_y > n2_min_y) ||
                          (n2_min_y < n1_min_y && n2_max_y > n1_min_y);
            if (x_int && y_int) crossing_estimate++;
        }
    }
    return crossing_estimate;
}

/* ─── Print Connectivity Metrics ─── */

void connectivity_metrics_print(FILE *fp, const connectivity_metrics_t *m)
{
    if (!fp || !m) return;
    fprintf(fp, "=== Schematic Connectivity Metrics ===\n");
    fprintf(fp, "  Vertices (pins):      %d\n", m->num_vertices);
    fprintf(fp, "  Edges (connections):  %d\n", m->num_edges);
    fprintf(fp, "  Connected components: %d\n", m->num_connected_components);
    fprintf(fp, "  Average degree:       %.2f\n", m->avg_degree);
    fprintf(fp, "  Maximum degree:       %.2f\n", m->max_degree);
    fprintf(fp, "  Single-pin nets:      %d\n", m->num_single_pin_nets);
    fprintf(fp, "  Floating subnets:     %d\n", m->num_floating_subnets);
    fprintf(fp, "  Ground loops:         %d\n", m->num_ground_loops);
    fprintf(fp, "  Fully connected:      %s\n",
            m->is_fully_connected ? "YES" : "NO");
    fprintf(fp, "=======================================\n");
}