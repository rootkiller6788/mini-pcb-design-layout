/**
 * @file schematic_core.c
 * @brief Core data structure operations for schematic design
 *
 * Implements creation, manipulation, and validation of the
 * schematic_design_t data structure and its sub-types.
 *
 * All memory management uses explicit malloc/free with
 * defensive null-pointer checks.
 */

#include "schematic_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ──────────── Schematic Design Lifecycle ──────────── */

schematic_design_t* schematic_design_create(const char *title)
{
    schematic_design_t *sch = calloc(1, sizeof(schematic_design_t));
    if (!sch) return NULL;

    if (title) {
        strncpy(sch->title, title, sizeof(sch->title) - 1);
    } else {
        strncpy(sch->title, "Untitled", sizeof(sch->title) - 1);
    }
    sch->title[sizeof(sch->title) - 1] = '\0';

    strncpy(sch->date, "2026-06-22", sizeof(sch->date) - 1);
    sch->date[sizeof(sch->date) - 1] = '\0';
    strncpy(sch->rev, "1.0", sizeof(sch->rev) - 1);
    sch->num_components = 0;
    sch->components = NULL;
    sch->num_nets = 0;
    sch->nets = NULL;
    sch->num_sheets = 0;
    sch->sheets = NULL;

    return sch;
}

void schematic_design_free(schematic_design_t *sch)
{
    if (!sch) return;

    /* Free components and their pins */
    for (int i = 0; i < sch->num_components; i++) {
        free(sch->components[i].pins);
    }
    free(sch->components);

    /* Free nets and their connections */
    for (int i = 0; i < sch->num_nets; i++) {
        free(sch->nets[i].connections);
    }
    free(sch->nets);

    /* Free sheets and their pins */
    for (int i = 0; i < sch->num_sheets; i++) {
        free(sch->sheets[i].pins);
    }
    free(sch->sheets);

    /* Zero the struct to catch use-after-free */
    memset(sch, 0, sizeof(schematic_design_t));
    free(sch);
}

/* ──────────── Component Management ──────────── */

int schematic_add_component(schematic_design_t *sch,
                            const char *ref, const char *value,
                            const char *footprint, const char *lib_id)
{
    if (!sch || !ref || !value) return -1;

    int idx = sch->num_components;
    schematic_component_t *new_comps = realloc(sch->components,
        (idx + 1) * sizeof(schematic_component_t));
    if (!new_comps) return -1;

    sch->components = new_comps;
    schematic_component_t *comp = &sch->components[idx];
    memset(comp, 0, sizeof(schematic_component_t));

    strncpy(comp->reference, ref, sizeof(comp->reference) - 1);
    comp->reference[sizeof(comp->reference) - 1] = '\0';
    strncpy(comp->value, value, sizeof(comp->value) - 1);
    comp->value[sizeof(comp->value) - 1] = '\0';
    if (footprint) {
        strncpy(comp->footprint, footprint, sizeof(comp->footprint) - 1);
        comp->footprint[sizeof(comp->footprint) - 1] = '\0';
    }
    if (lib_id) {
        strncpy(comp->library_id, lib_id, sizeof(comp->library_id) - 1);
        comp->library_id[sizeof(comp->library_id) - 1] = '\0';
    }

    comp->num_pins = 0;
    comp->pins = NULL;
    comp->pos_x = 0.0;
    comp->pos_y = 0.0;
    comp->rotation = 0.0;
    comp->unit = 1;
    sch->num_components++;

    return idx;
}

int schematic_component_add_pin(schematic_component_t *comp,
                                const char *name, const char *num,
                                pin_type_t etype)
{
    if (!comp || !name || !num) return -1;

    int idx = comp->num_pins;
    schematic_pin_t *new_pins = realloc(comp->pins,
        (idx + 1) * sizeof(schematic_pin_t));
    if (!new_pins) return -1;

    comp->pins = new_pins;
    schematic_pin_t *pin = &comp->pins[idx];
    memset(pin, 0, sizeof(schematic_pin_t));

    strncpy(pin->name, name, sizeof(pin->name) - 1);
    pin->name[sizeof(pin->name) - 1] = '\0';
    strncpy(pin->number, num, sizeof(pin->number) - 1);
    pin->number[sizeof(pin->number) - 1] = '\0';
    pin->electrical_type = etype;
    pin->shape = PIN_SHAPE_LINE;
    pin->orientation = PIN_ORIENT_RIGHT;
    pin->length = 100.0;      /* 100 mil default */
    pin->is_visible = true;
    pin->is_power = (etype == PIN_TYPE_POWER_IN ||
                     etype == PIN_TYPE_POWER_OUT);
    comp->num_pins++;

    return idx;
}

/* ──────────── Net Management ──────────── */

int schematic_add_net(schematic_design_t *sch, const char *name, int net_code)
{
    if (!sch || !name) return -1;

    int idx = sch->num_nets;
    schematic_net_t *new_nets = realloc(sch->nets,
        (idx + 1) * sizeof(schematic_net_t));
    if (!new_nets) return -1;

    sch->nets = new_nets;
    schematic_net_t *net = &sch->nets[idx];
    memset(net, 0, sizeof(schematic_net_t));

    strncpy(net->name, name, sizeof(net->name) - 1);
    net->name[sizeof(net->name) - 1] = '\0';
    net->net_code = (net_code >= 0) ? net_code : idx + 1;
    net->num_connections = 0;
    net->connections = NULL;
    net->is_power_net = (strstr(name, "VCC") || strstr(name, "VDD") ||
                         strstr(name, "GND") || strstr(name, "VSS") ||
                         strstr(name, "PWR") || strstr(name, "VBAT") ||
                         strstr(name, "VCC") != NULL);
    sch->num_nets++;

    return idx;
}

bool schematic_connect_pin(schematic_design_t *sch,
                           const char *ref, const char *pin_name,
                           const char *net_name)
{
    if (!sch || !ref || !pin_name || !net_name) return false;

    /* Find or create the net */
    int net_idx = schematic_find_net(sch, net_name);
    if (net_idx < 0) {
        net_idx = schematic_add_net(sch, net_name, -1);
        if (net_idx < 0) return false;
    }

    /* Find the component */
    int comp_idx = schematic_find_component(sch, ref);
    if (comp_idx < 0) return false;

    schematic_component_t *comp = &sch->components[comp_idx];

    /* Find the pin on the component */
    int pin_idx = -1;
    for (int i = 0; i < comp->num_pins; i++) {
        if (strcmp(comp->pins[i].name, pin_name) == 0 ||
            strcmp(comp->pins[i].number, pin_name) == 0) {
            pin_idx = i;
            break;
        }
    }
    if (pin_idx < 0) return false;

    /* Add connection to net */
    schematic_net_t *net = &sch->nets[net_idx];
    int conn_idx = net->num_connections;
    void *new_conns = realloc(net->connections,
        (conn_idx + 1) * sizeof(struct net_connection));
    if (!new_conns) return false;

    net->connections = new_conns;
    net->connections[conn_idx].pin_index = pin_idx;
    strncpy(net->connections[conn_idx].ref, comp->reference, 14);
    net->connections[conn_idx].ref[14] = '\0';
    net->connections[conn_idx].ref[15] = '\0';
    strncpy(net->connections[conn_idx].pin, comp->pins[pin_idx].number, 6);
    net->connections[conn_idx].pin[6] = '\0';
    net->connections[conn_idx].pin[7] = '\0';
    net->num_connections++;

    return true;
}

/* ──────────── Query Functions ──────────── */

int schematic_find_component(const schematic_design_t *sch, const char *ref)
{
    if (!sch || !ref) return -1;
    for (int i = 0; i < sch->num_components; i++) {
        if (strcmp(sch->components[i].reference, ref) == 0)
            return i;
    }
    return -1;
}

int schematic_find_net(const schematic_design_t *sch, const char *net_name)
{
    if (!sch || !net_name) return -1;
    for (int i = 0; i < sch->num_nets; i++) {
        if (strcmp(sch->nets[i].name, net_name) == 0)
            return i;
    }
    return -1;
}

int schematic_total_connections(const schematic_design_t *sch)
{
    if (!sch) return 0;
    int total = 0;
    for (int i = 0; i < sch->num_nets; i++) {
        total += sch->nets[i].num_connections;
    }
    return total;
}

int schematic_total_pins(const schematic_design_t *sch)
{
    if (!sch) return 0;
    int total = 0;
    for (int i = 0; i < sch->num_components; i++) {
        total += sch->components[i].num_pins;
    }
    return total;
}

/* ──────────── Validation ──────────── */

int schematic_validate_references(const schematic_design_t *sch,
                                   char *errors, size_t err_size)
{
    if (!sch || !errors || err_size == 0) return -1;
    errors[0] = '\0';
    int error_count = 0;
    size_t pos = 0;

    /* Check for duplicate references */
    for (int i = 0; i < sch->num_components; i++) {
        for (int j = i + 1; j < sch->num_components; j++) {
            if (strcmp(sch->components[i].reference,
                       sch->components[j].reference) == 0) {
                int written = snprintf(errors + pos, err_size - pos,
                    "ERR: Duplicate reference '%s'\n",
                    sch->components[i].reference);
                if (written > 0 && (size_t)written < err_size - pos)
                    pos += written;
                else
                    pos = err_size - 1;
                error_count++;
            }
        }
    }

    /* Check for empty references */
    for (int i = 0; i < sch->num_components; i++) {
        if (sch->components[i].reference[0] == '\0') {
            int written = snprintf(errors + pos, err_size - pos,
                "ERR: Empty reference at component index %d\n", i);
            if (written > 0 && (size_t)written < err_size - pos)
                pos += written;
            else
                pos = err_size - 1;
            error_count++;
        }
    }

    return error_count;
}

int schematic_check_power_connectivity(const schematic_design_t *sch)
{
    if (!sch) return -1;
    int unconnected = 0;

    for (int i = 0; i < sch->num_components; i++) {
        schematic_component_t *comp = &sch->components[i];
        for (int j = 0; j < comp->num_pins; j++) {
            if (comp->pins[j].is_power) {
                /* Check if this power pin appears in any net */
                bool found = false;
                for (int k = 0; k < sch->num_nets; k++) {
                    for (int c = 0; c < sch->nets[k].num_connections; c++) {
                        if (strcmp(sch->nets[k].connections[c].ref,
                                   comp->reference) == 0 &&
                            strcmp(sch->nets[k].connections[c].pin,
                                   comp->pins[j].number) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!found) unconnected++;
            }
        }
    }
    return unconnected;
}

/* ──────────── Graph Construction (L3) ──────────── */

netlist_graph_t* schematic_build_graph(const schematic_design_t *sch)
{
    if (!sch) return NULL;

    int total_pins = schematic_total_pins(sch);
    if (total_pins == 0) return NULL;

    netlist_graph_t *g = calloc(1, sizeof(netlist_graph_t));
    if (!g) return NULL;

    g->num_vertices = total_pins;
    g->num_edges = schematic_total_connections(sch);

    /* Allocate per-vertex metadata */
    g->vertex_refs = calloc(total_pins, sizeof(char*));
    g->vertex_pins = calloc(total_pins, sizeof(char*));
    if (!g->vertex_refs || !g->vertex_pins) {
        netlist_graph_free(g);
        return NULL;
    }

    /* First pass: assign vertex indices and count adjacency */
    int *degree = calloc(total_pins, sizeof(int));
    if (!degree) {
        netlist_graph_free(g);
        return NULL;
    }

    int vidx = 0;
    for (int i = 0; i < sch->num_components; i++) {
        schematic_component_t *comp = &sch->components[i];
        for (int j = 0; j < comp->num_pins; j++) {
            g->vertex_refs[vidx] = strdup(comp->reference);
            g->vertex_pins[vidx] = strdup(comp->pins[j].number);
            /* For each net containing this pin, increment degree for
             * all other pins on the same net (undirected edge) */
            for (int k = 0; k < sch->num_nets; k++) {
                for (int c = 0; c < sch->nets[k].num_connections; c++) {
                    if (strcmp(sch->nets[k].connections[c].ref,
                               comp->reference) == 0 &&
                        strcmp(sch->nets[k].connections[c].pin,
                               comp->pins[j].number) == 0) {
                        degree[vidx] += (sch->nets[k].num_connections - 1);
                        break;
                    }
                }
            }
            vidx++;
        }
    }

    /* Build CSR offsets */
    g->adjacency_offsets = calloc(total_pins + 1, sizeof(int));
    if (!g->adjacency_offsets) {
        free(degree);
        netlist_graph_free(g);
        return NULL;
    }

    g->adjacency_offsets[0] = 0;
    for (int i = 0; i < total_pins; i++) {
        g->adjacency_offsets[i + 1] = g->adjacency_offsets[i] + degree[i];
    }

    /* Allocate adjacency targets */
    int total_edges = g->adjacency_offsets[total_pins];
    g->adjacency_targets = calloc(total_edges, sizeof(int));
    if (!g->adjacency_targets) {
        free(degree);
        netlist_graph_free(g);
        return NULL;
    }

    /* Second pass: fill targets */
    int *fill_counter = calloc(total_pins, sizeof(int));
    if (!fill_counter) {
        free(degree);
        netlist_graph_free(g);
        return NULL;
    }

    vidx = 0;
    for (int i = 0; i < sch->num_components; i++) {
        schematic_component_t *comp = &sch->components[i];
        for (int j = 0; j < comp->num_pins; j++) {
            for (int k = 0; k < sch->num_nets; k++) {
                bool pin_on_net = false;
                for (int c = 0; c < sch->nets[k].num_connections; c++) {
                    if (strcmp(sch->nets[k].connections[c].ref,
                               comp->reference) == 0 &&
                        strcmp(sch->nets[k].connections[c].pin,
                               comp->pins[j].number) == 0) {
                        pin_on_net = true;
                        break;
                    }
                }
                if (pin_on_net) {
                    /* Add edges to all other pins on this net */
                    for (int c = 0; c < sch->nets[k].num_connections; c++) {
                        /* Find vertex index for this other pin */
                        if (strcmp(sch->nets[k].connections[c].ref,
                                   comp->reference) == 0 &&
                            strcmp(sch->nets[k].connections[c].pin,
                                   comp->pins[j].number) == 0) {
                            continue; /* Skip self */
                        }
                        /* Find target vertex */
                        int tgt = -1;
                        int tv = 0;
                        for (int ii = 0; ii < sch->num_components; ii++) {
                            schematic_component_t *c2 = &sch->components[ii];
                            for (int jj = 0; jj < c2->num_pins; jj++) {
                                if (strcmp(c2->reference,
                                           sch->nets[k].connections[c].ref) == 0 &&
                                    strcmp(c2->pins[jj].number,
                                           sch->nets[k].connections[c].pin) == 0) {
                                    tgt = tv;
                                    goto found_tgt;
                                }
                                tv++;
                            }
                        }
                        found_tgt:
                        if (tgt >= 0) {
                            int offset = g->adjacency_offsets[vidx];
                            g->adjacency_targets[offset + fill_counter[vidx]] = tgt;
                            fill_counter[vidx]++;
                        }
                    }
                }
            }
            vidx++;
        }
    }

    free(fill_counter);
    free(degree);
    return g;
}

void netlist_graph_free(netlist_graph_t *g)
{
    if (!g) return;
    if (g->vertex_refs) {
        for (int i = 0; i < g->num_vertices; i++)
            free(g->vertex_refs[i]);
        free(g->vertex_refs);
    }
    if (g->vertex_pins) {
        for (int i = 0; i < g->num_vertices; i++)
            free(g->vertex_pins[i]);
        free(g->vertex_pins);
    }
    free(g->adjacency_offsets);
    free(g->adjacency_targets);
    free(g);
}

/* ──────────── DFS: Connected Components (Tarjan, 1972) ──────────── */

int netlist_graph_connected_components(const netlist_graph_t *g,
                                        int *component_id)
{
    if (!g || !component_id) return -1;
    if (g->num_vertices == 0) return 0;

    /* Initialize: -1 = unvisited */
    for (int i = 0; i < g->num_vertices; i++)
        component_id[i] = -1;

    /* Iterative DFS stack */
    int *stack = calloc(g->num_vertices, sizeof(int));
    if (!stack) return -1;

    int comp_count = 0;

    for (int start = 0; start < g->num_vertices; start++) {
        if (component_id[start] >= 0) continue;

        /* Start new DFS from start vertex */
        int top = 0;
        stack[top++] = start;
        component_id[start] = comp_count;

        while (top > 0) {
            int v = stack[--top];
            int off_start = g->adjacency_offsets[v];
            int off_end   = g->adjacency_offsets[v + 1];

            for (int e = off_start; e < off_end; e++) {
                int w = g->adjacency_targets[e];
                if (component_id[w] < 0) {
                    component_id[w] = comp_count;
                    stack[top++] = w;
                }
            }
        }
        comp_count++;
    }

    free(stack);
    return comp_count;
}

/* ──────────── BFS: Shortest Path ──────────── */

int netlist_graph_shortest_path(const netlist_graph_t *g,
                                 int start_vertex, int end_vertex,
                                 int *path, int max_path_len)
{
    if (!g || !path || max_path_len < 1) return -1;
    if (start_vertex == end_vertex) {
        path[0] = start_vertex;
        return 1;
    }

    int *dist  = calloc(g->num_vertices, sizeof(int));
    int *prev  = calloc(g->num_vertices, sizeof(int));
    int *queue = calloc(g->num_vertices, sizeof(int));
    if (!dist || !prev || !queue) {
        free(dist); free(prev); free(queue);
        return -1;
    }

    for (int i = 0; i < g->num_vertices; i++) {
        dist[i] = -1;
        prev[i] = -1;
    }

    int q_head = 0, q_tail = 0;
    dist[start_vertex] = 0;
    queue[q_tail++] = start_vertex;

    bool found = false;
    while (q_head < q_tail) {
        int v = queue[q_head++];
        if (v == end_vertex) {
            found = true;
            break;
        }

        int off_start = g->adjacency_offsets[v];
        int off_end   = g->adjacency_offsets[v + 1];
        for (int e = off_start; e < off_end; e++) {
            int w = g->adjacency_targets[e];
            if (dist[w] < 0) {
                dist[w] = dist[v] + 1;
                prev[w] = v;
                queue[q_tail++] = w;
            }
        }
    }

    free(queue);
    free(dist);

    if (!found) {
        free(prev);
        return 0; /* No path exists */
    }

    /* Reconstruct path backwards */
    int path_len = 0;
    int curr = end_vertex;
    while (curr >= 0 && path_len < max_path_len) {
        path[path_len++] = curr;
        curr = prev[curr];
    }
    free(prev);

    /* Reverse to get start→end order */
    for (int i = 0; i < path_len / 2; i++) {
        int tmp = path[i];
        path[i] = path[path_len - 1 - i];
        path[path_len - 1 - i] = tmp;
    }

    return path_len;
}

bool schematic_has_floating_nets(const schematic_design_t *sch)
{
    if (!sch) return false;
    for (int i = 0; i < sch->num_nets; i++) {
        if (sch->nets[i].num_connections < 2)
            return true; /* Single-connection net = floating */
    }
    return false;
}