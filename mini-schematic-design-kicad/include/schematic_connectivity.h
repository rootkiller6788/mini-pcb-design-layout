/**
 * @file schematic_connectivity.h
 * @brief Graph-theoretic connectivity analysis of schematic netlists
 *
 * Models the schematic as a bipartite graph (components ←→ nets)
 * and performs connectivity analysis using graph algorithms.
 * Core capability: verify that all nets are properly connected,
 * identify floating subnets, detect unintended shorts, and
 * trace signal paths through the design.
 *
 * Theoretical Foundations (L3-L4):
 *   - Graph Theory: adjacency, incidence, connectivity, spanning trees
 *   - Kirchhoff's Current Law encoded as graph cut constraints
 *   - Netlist topology = hypergraph (pins are hyperedges on nets)
 *   - Euler's formula for planar circuit graphs: V - E + F = 1 + C
 *
 * Course Mapping:
 *   - MIT 6.042 — graph theory, connected components
 *   - CMU 15-251 — graph algorithms
 *   - Berkeley EE219 — circuit topology analysis
 *
 * Reference:
 *   - Deo, N., "Graph Theory with Applications", 1974
 *   - Chua, L.O. & Lin, P.M., "Computer-Aided Analysis of Electronic
 *     Circuits: Algorithms and Computational Techniques", 1975
 */

#ifndef SCHEMATIC_CONNECTIVITY_H
#define SCHEMATIC_CONNECTIVITY_H

#include "schematic_core.h"
#include <stdbool.h>
#include <stdio.h>

/* ──────────── L1: Connectivity Analysis Types ──────────── */

/**
 * @brief Connected component in the netlist graph
 *
 * Components electrically isolated from each other form separate
 * connected components. E.g., analog frontend vs. digital logic
 * on the same schematic but powered independently.
 */
typedef struct {
    int      component_id;       /* Unique component identifier */
    int      num_vertices;       /* Number of pins in component */
    int      num_nets;           /* Number of nets in component */
    int     *vertex_indices;     /* Indices into graph vertices */
    bool     is_powered;         /* Has a power net connection */
    bool     is_grounded;        /* Has a ground net connection */
} connected_component_t;

/**
 * @brief Shortest path between two pins
 */
typedef struct {
    int      path_length;        /* Number of vertices in path */
    int     *vertices;           /* Vertex indices along path */
    int     *nets;               /* Net indices traversed */
    double   total_distance;     /* Estimated physical distance */
} connectivity_path_t;

/**
 * @brief Netlist cycle (loop) description
 *
 * A cycle in the netlist indicates a closed electrical loop,
 * which is normal for power distribution but may indicate
 * unintended antenna loops for signal paths.
 */
typedef struct {
    int      cycle_length;
    int     *vertices;
    bool     is_ground_loop;     /* Cycle involves multiple ground paths */
    bool     is_signal_loop;     /* Cycle involves signal nets */
    double   loop_area_estimate; /* Estimated loop area (EMI concern) */
} netlist_cycle_t;

/**
 * @brief Connectivity metrics for the entire design
 */
typedef struct {
    int      num_vertices;       /* Total vertex (pin) count */
    int      num_edges;          /* Total edge (net connection) count */
    int      num_connected_components; /* Number of isolated sub-circuits */
    double   avg_degree;         /* Average connections per pin */
    double   max_degree;         /* Maximum connections on any net */
    int      num_single_pin_nets; /* Nets with only one connection */
    int      num_floating_subnets; /* Subnets without power/ground */
    int      num_ground_loops;   /* Count of ground loops detected */
    bool     is_fully_connected; /* All components in one component */
} connectivity_metrics_t;

/* ──────────── L2: Netlist Hypergraph ──────────── */

/**
 * @brief Incidence matrix representation
 *
 * The circuit is modeled as a bipartite graph with two node sets:
 *   - Component-pins (size M)
 *   - Nets (size N)
 * Edges only exist between a pin and a net, encoding which pins
 * belong to which net. The incidence matrix B[m][n] = 1 if pin m
 * is on net n, 0 otherwise.
 */
typedef struct {
    int      num_pins;           /* M — all pins in design */
    int      num_nets;           /* N — all nets */
    int     *row_offsets;        /* CSR row pointer (M+1) */
    int     *col_indices;        /* CSR column indices */
    double  *values;             /* Non-zero values (0 = open, 1 = connected) */
    char   **pin_refs;           /* Reference designator per row */
    char   **pin_nums;           /* Pin number per row */
    char   **net_names;          /* Net name per column */
} incidence_matrix_t;

/**
 * @brief Reduced node-adjacency matrix (A) for nodal analysis
 *
 * A = B · B^T (pin-pin adjacency via shared nets)
 * A[i][j] != 0 iff pins i and j share a net.
 */
typedef struct {
    int      dimension;          /* n × n matrix size */
    double  *data;               /* Row-major dense storage */
    int     *degrees;            /* Diagonal entry = degree(pin) */
} adjacency_matrix_t;

/* ──────────── L5: Connectivity Graph Algorithms ──────────── */

/** Build incidence matrix from schematic netlist */
incidence_matrix_t* connectivity_build_incidence(const schematic_design_t *sch);
void incidence_matrix_free(incidence_matrix_t *im);

/** Build adjacency matrix (A = B × B^T, pin-pin connectivity) */
adjacency_matrix_t* connectivity_build_adjacency(const incidence_matrix_t *im);
void adjacency_matrix_free(adjacency_matrix_t *am);

/** Connected components via depth-first search */
int connectivity_find_components(const schematic_design_t *sch,
                                  connected_component_t **comps);

/** Compute connectivity metrics */
connectivity_metrics_t connectivity_compute_metrics(
    const schematic_design_t *sch);

/** Find the shortest path between two reference-pin pairs */
connectivity_path_t* connectivity_shortest_path(
    const schematic_design_t *sch,
    const char *ref_a, const char *pin_a,
    const char *ref_b, const char *pin_b);

/** Find all cycles in the netlist (detect ground loops) */
int connectivity_find_cycles(const schematic_design_t *sch,
                              netlist_cycle_t **cycles);

/** Check if two nets are electrically connected (same node) */
bool connectivity_nets_connected(const schematic_design_t *sch,
                                  const char *net_a, const char *net_b);

/** Count all reachable components from a given starting pin */
int connectivity_reachable_components(const schematic_design_t *sch,
                                       const char *ref, const char *pin);

/** Free connectivity path */
void connectivity_path_free(connectivity_path_t *path);

/* ──────────── L6: Canonical Problems ──────────── */

/**
 * @brief Detect unintended antenna loops (EMI concern)
 *
 * Identifies signal paths that form large-area current loops,
 * which act as unintentional antennas. Uses a modified DFS
 * that tracks geometric area from component/pin positions.
 */
int connectivity_detect_antenna_loops(const schematic_design_t *sch,
                                       netlist_cycle_t **loops,
                                       double area_threshold);

/**
 * @brief Verify single-point grounding integrity
 *
 * Checks that all ground connections converge to a single point
 * or are intentionally arranged in a star topology. Returns
 * the number of violations (multiple ground paths).
 */
int connectivity_verify_star_ground(const schematic_design_t *sch);

/**
 * @brief Find critical nets (nets whose removal disconnects the design)
 *
 * Uses articulation point / bridge-finding algorithm (Tarjan, 1974).
 * A net is critical if removing it would split the design into
 * two or more disconnected sub-circuits.
 */
int connectivity_find_critical_nets(const schematic_design_t *sch,
                                     int *critical_net_indices,
                                     int max_critical);

/**
 * @brief Check Manhattan routing feasibility (pre-PCB check)
 *
 * Verifies that all connections can be routed on a Manhattan grid
 * without crossings, using left-right planarity test.
 * Returns 0 if routable, >0 count of crossings needed.
 */
int connectivity_check_routability(const schematic_design_t *sch);

/** Print connectivity metrics report */
void connectivity_metrics_print(FILE *fp, const connectivity_metrics_t *m);

#endif /* SCHEMATIC_CONNECTIVITY_H */