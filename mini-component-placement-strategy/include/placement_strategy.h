/**
 * @file placement_strategy.h
 * @brief PCB component placement strategy algorithms
 *
 * Implements multiple placement optimization strategies ranging from
 * simple greedy heuristics to metaheuristic global optimization.
 *
 * Knowledge Mapping:
 *   L5 (Algorithms): Greedy placement, Simulated Annealing,
 *                    Force-Directed Placement, Partition-based placement,
 *                    Genetic Algorithm, Clustering-based placement
 *   L3 (Math Structures): Cost functions, Metropolis criterion,
 *                          spring-electrical force model, min-cut formulation
 *   L6 (Canonical Problems): Mixed-signal partition, BGA fanout,
 *                            high-speed differential pair placement
 *
 * Course Alignment:
 *   - MIT 6.450: optimization in communication system layout
 *   - Stanford EE359: RF component placement considerations
 *   - Georgia Tech ECE 6601: combinatorial optimization
 *   - CMU 18-600: VLSI CAD algorithms adapted to PCB
 *
 * References:
 *   - Kirkpatrick, Gelatt, Vecchi, "Optimization by Simulated Annealing", Science, 1983
 *   - Fruchterman & Reingold, "Graph Drawing by Force-Directed Placement", SPE, 1991
 *   - Fiduccia & Mattheyses, "A Linear-Time Heuristic for Network Partitioning", DAC, 1982
 */

#ifndef PLACEMENT_STRATEGY_H
#define PLACEMENT_STRATEGY_H

#include "placement_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Strategy Context and Configuration
 * ============================================================================ */

/** Placement strategy identifiers */
typedef enum {
    STRATEGY_GREEDY              = 0,
    STRATEGY_SIMULATED_ANNEALING = 1,
    STRATEGY_FORCE_DIRECTED      = 2,
    STRATEGY_PARTITION_BISECTION = 3,
    STRATEGY_GENETIC             = 4,
    STRATEGY_CLUSTERING          = 5,
    STRATEGY_HYBRID              = 6,
    STRATEGY_COUNT               = 7
} StrategyType;

/** Simulated annealing cooling schedule types */
typedef enum {
    COOLING_LINEAR       = 0,  /* T_k = T_0 - k * (T_0 - T_f) / N */
    COOLING_EXPONENTIAL  = 1,  /* T_k = T_0 * alpha^k */
    COOLING_LOGARITHMIC  = 2,  /* T_k = T_0 / log(1 + k)  — Geman & Geman */
    COOLING_COUNT        = 3
} CoolingSchedule;

/** Configuration for simulated annealing placement */
typedef struct {
    double      initial_temperature;     /* T_0: starting temperature */
    double      final_temperature;       /* T_f: stopping temperature */
    CoolingSchedule schedule;            /* Cooling schedule type */
    double      alpha;                   /* Exponential cooling factor (0.8-0.99) */
    uint32_t    moves_per_temperature;   /* Inner loop iterations per temperature step */
    uint32_t    max_iterations;          /* Maximum total iterations */
    uint32_t    random_seed;             /* Random seed for reproducibility */
    double      swap_probability;        /* Probability of swap vs. move perturbation */
    double      max_move_distance;       /* Maximum single component move distance (mm) */
} SAConfig;

/** Configuration for force-directed placement */
typedef struct {
    double   spring_stiffness;       /* Spring constant for connected components */
    double   electrical_repulsion;   /* Repulsion constant for all component pairs */
    double   ideal_edge_length_mm;   /* Target distance between connected components */
    double   damping_factor;         /* Velocity damping (0-1, prevents oscillation) */
    double   convergence_threshold;  /* Stop when max displacement < threshold */
    uint32_t max_iterations;
} FDConfig;

/** Configuration for genetic algorithm placement */
typedef struct {
    uint32_t population_size;
    uint32_t generations;
    double   crossover_rate;         /* Probability of crossover per pair */
    double   mutation_rate;          /* Probability of mutation per gene */
    uint32_t tournament_size;        /* Tournament selection group size */
    double   elitism_fraction;       /* Fraction of population kept as-is */
    uint32_t random_seed;
} GAConfig;

/** Configuration for partition-based placement (min-cut bisection) */
typedef struct {
    uint32_t max_partitions;        /* Maximum partition depth */
    double   balance_tolerance;     /* Max area imbalance between partitions (0=exact) */
    bool     use_terminal_propagation; /* Account for external connections */
} PartitionConfig;

/** Configuration for clustering-based placement */
typedef struct {
    uint32_t n_clusters;            /* Number of clusters for K-means */
    bool     use_spectral;          /* Use spectral clustering instead of k-means */
    double   connectivity_weight;   /* Weight of connectivity in distance metric */
    uint32_t max_kmeans_iters;
} ClusterConfig;

/** Master strategy configuration */
typedef struct {
    StrategyType    type;
    union {
        SAConfig        sa;
        FDConfig        fd;
        GAConfig        ga;
        PartitionConfig partition;
        ClusterConfig   cluster;
    } config;
    /* Cost weights */
    double weight_wire_length;
    double weight_thermal;
    double weight_signal_integrity;
    double weight_overlap;
    double weight_density;
} StrategyConfig;

/* ============================================================================
 * Strategy API
 * ============================================================================ */

/**
 * Initialize strategy configuration with sensible defaults for the given type.
 *
 * @param config  Configuration to initialize
 * @param type    Strategy type
 */
void placement_strategy_config_init(StrategyConfig* config, StrategyType type);

/* ============================================================================
 * L5 Algorithm: Greedy Placement
 * ============================================================================ */

/**
 * Greedy sequential placement algorithm.
 *
 * Places components one at a time in order of decreasing connectivity.
 * For each component, selects the position that minimizes incremental
 * wire length among a set of candidate positions near already-placed
 * connected components.
 *
 * Algorithm (O(C^2 * P) where C=components, P=candidates):
 *   1. Sort components by connectivity degree (descending)
 *   2. Place first component at board center
 *   3. For each subsequent component:
 *      a. Generate candidate positions near connected placed components
 *      b. Evaluate HPWL cost for each candidate
 *      c. Select candidate with minimum cost that satisfies constraints
 *
 * Reference: M. Hanan, "On Steiner's Problem with Rectilinear Distance",
 *            SIAM J. Appl. Math, 1966.
 *
 * @param result  Placement result (board and netlist must be populated)
 * @return        Number of components successfully placed
 */
uint32_t placement_strategy_greedy(PlacementResult* result);

/* ============================================================================
 * L5 Algorithm: Simulated Annealing Placement
 * ============================================================================ */

/**
 * Simulated annealing optimization for component placement.
 *
 * Uses the Metropolis criterion to probabilistically accept moves that
 * increase cost, enabling escape from local minima. The acceptance
 * probability for a cost-increasing move is P = exp(-ΔC / T).
 *
 * Perturbation operators:
 *   - Single component move (random displacement within max_move_distance)
 *   - Pairwise component swap
 *   - Component rotation by 90° increments
 *
 * Cooling schedules supported:
 *   - Linear:    T_k = T_0 - k * (T_0 - T_f) / N
 *   - Exponential: T_k = T_0 * alpha^k   (most common)
 *   - Logarithmic: T_k = T_0 / ln(1 + k)  (guarantees global min, Geman 1984)
 *
 * Complexity: O(M * C^2 * P) where M = moves_per_temp * temp_steps,
 *             C = number of components, P = candidate positions
 *
 * Reference:
 *   S. Kirkpatrick, C. D. Gelatt Jr., M. P. Vecchi,
 *   "Optimization by Simulated Annealing", Science, Vol. 220, 1983.
 *
 * @param result  Placement result (components must be initialized)
 * @param config  SA configuration
 * @return        Number of iterations completed
 */
uint32_t placement_strategy_simulated_annealing(PlacementResult* result,
                                                 const SAConfig* config);

/* ============================================================================
 * L5 Algorithm: Force-Directed Placement
 * ============================================================================ */

/**
 * Force-directed placement using spring-electrical force model.
 *
 * Models the placement as a physical system:
 *   - Connected components attract via springs (Hooke's law):
 *     F_spring(i,j) = K_s * (d_ij - L_ideal) * u_ij
 *   - All components repel via Coulomb-like force:
 *     F_repel(i,j) = K_r / d_ij^2 * u_ij
 *
 * where d_ij = distance between components i and j,
 *       u_ij = unit vector from i to j.
 *
 * Iteratively updates positions until convergence or max iterations.
 *
 * Complexity: O(I * C^2) where I = iterations, C = components.
 * Optimized with Barnes-Hut quadtree to O(I * C log C).
 *
 * Reference:
 *   T. Fruchterman, E. Reingold,
 *   "Graph Drawing by Force-Directed Placement",
 *   Software: Practice and Experience, Vol. 21(11), 1991.
 *
 * @param result  Placement result
 * @param config  Force-directed configuration
 * @return        Number of iterations until convergence
 */
uint32_t placement_strategy_force_directed(PlacementResult* result,
                                            const FDConfig* config);

/* ============================================================================
 * L5 Algorithm: Partition-Based Placement (Min-Cut)
 * ============================================================================ */

/**
 * Recursive min-cut bisection placement (Fiduccia-Mattheyses algorithm).
 *
 * Recursively partitions the circuit into two halves, minimizing the
 * number of nets crossing the cut line. Alternates cut direction
 * (horizontal/vertical) at each recursion level.
 *
 * The FM algorithm is a linear-time heuristic:
 *   1. Start with an initial balanced partition
 *   2. Compute gain (net cut reduction) for moving each cell
 *   3. Repeatedly move the cell with highest gain, locking it
 *   4. After all cells moved, find the point in the sequence with best cut
 *   5. Repeat passes until no improvement
 *
 * Complexity: O(P) per pass where P = total pins (linear in circuit size).
 *
 * Reference:
 *   C. M. Fiduccia, R. M. Mattheyses,
 *   "A Linear-Time Heuristic for Improving Network Partitions",
 *   Proc. 19th Design Automation Conference, 1982.
 *
 * @param result  Placement result
 * @param config  Partition configuration
 * @return        Number of partition levels executed
 */
uint32_t placement_strategy_partition_bisection(PlacementResult* result,
                                                 const PartitionConfig* config);

/* ============================================================================
 * L5 Algorithm: Genetic Algorithm Placement
 * ============================================================================ */

/**
 * Genetic algorithm for component placement optimization.
 *
 * Representation: Each chromosome is an ordered list of (x, y, rotation)
 * tuples for each component.
 *
 * Operators:
 *   - Selection: Tournament selection (selects best of k randomly chosen)
 *   - Crossover: Order-based crossover (preserves relative positions)
 *   - Mutation: Random perturbation of component positions
 *   - Elitism: Preserve best N individuals unchanged
 *
 * Fitness function: Inverse of total placement cost.
 *
 * Complexity: O(G * P * C^2) where G = generations, P = population, C = components.
 *
 * Reference:
 *   J. H. Holland, "Adaptation in Natural and Artificial Systems", 1975.
 *   D. E. Goldberg, "Genetic Algorithms in Search, Optimization, and
 *   Machine Learning", 1989.
 *
 * @param result  Placement result
 * @param config  GA configuration
 * @return        Number of generations completed
 */
uint32_t placement_strategy_genetic_algorithm(PlacementResult* result,
                                               const GAConfig* config);

/* ============================================================================
 * L5 Algorithm: Clustering-Based Placement
 * ============================================================================ */

/**
 * Clustering-based placement using k-means or spectral clustering.
 *
 * Groups components by combined spatial-connectivity distance, then
 * places clusters sequentially. Within each cluster, applies force-directed
 * placement for refinement.
 *
 * Distance metric: d(i,j) = α * |pos_i - pos_j| + (1-α) * (1 - connectivity(i,j))
 * where connectivity(i,j) is normalized number of shared nets.
 *
 * Reference:
 *   J. MacQueen, "Some Methods for Classification and Analysis of
 *   Multivariate Observations", Proc. 5th Berkeley Symp. Math. Stat. Prob., 1967.
 *
 * @param result  Placement result
 * @param config  Clustering configuration
 * @return        Number of clusters formed
 */
uint32_t placement_strategy_clustering(PlacementResult* result,
                                        const ClusterConfig* config);

/* ============================================================================
 * Strategy Execution
 * ============================================================================ */

/**
 * Execute the configured placement strategy on a placement result.
 *
 * @param result  Placement result (board, components, nets must be populated)
 * @param config  Strategy configuration
 * @return        Number of components placed
 */
uint32_t placement_strategy_execute(PlacementResult* result,
                                     const StrategyConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* PLACEMENT_STRATEGY_H */
