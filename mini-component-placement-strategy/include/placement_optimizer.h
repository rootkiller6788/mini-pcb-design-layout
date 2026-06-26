/**
 * @file placement_optimizer.h
 * @brief Multi-objective placement optimization framework
 *
 * Provides cost function evaluation, Pareto front tracking, and
 * multi-objective optimization for PCB component placement.
 *
 * Knowledge Mapping:
 *   L3 (Math Structures): Multi-objective optimization, Pareto optimality,
 *                         weighted sum method, ε-constraint method
 *   L4 (Fundamental Laws): Wire-length estimation theorems,
 *                         thermal resistance network laws
 *   L5 (Algorithms): Cost function computation, Pareto front construction
 *   L8 (Advanced Topics): Multi-objective evolutionary optimization
 *
 * Course Alignment:
 *   - Stanford EE364: Convex optimization principles
 *   - CMU 18-660: Numerical methods for engineering optimization
 *   - ETH 227-0427: Optimization in signal processing
 *
 * References:
 *   - Miettinen, "Nonlinear Multiobjective Optimization", 1999
 *   - Deb et al., "A Fast Elitist Non-dominated Sorting Genetic Algorithm",
 *     IEEE Trans. Evol. Comp., 2002 (NSGA-II)
 */

#ifndef PLACEMENT_OPTIMIZER_H
#define PLACEMENT_OPTIMIZER_H

#include "placement_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Cost Functions — Independent Implementations
 * ============================================================================ */

/**
 * Compute Half-Perimeter Wire Length (HPWL) cost.
 *
 * For each net n connecting a set of pins at positions {(x_i, y_i)}:
 *   HPWL(n) = (max x_i - min x_i) + (max y_i - min y_i)
 *
 * HPWL is the most widely used wire-length estimate in VLSI/PCB placement
 * due to its computational efficiency and reasonable accuracy for
 * Manhattan routing. Within 10-15% of actual routed length for typical designs.
 *
 * Reference: Shahookar & Mazumder, "VLSI Cell Placement Techniques",
 *            ACM Computing Surveys, Vol. 23(2), 1991.
 *
 * @param result  Placement result with placed components and defined nets
 * @return        HPWL cost in mm
 */
double placement_cost_hpwl(const PlacementResult* result);

/**
 * Compute Steiner tree wire length estimate (rectilinear).
 *
 * More accurate than HPWL but more expensive. Uses a fast heuristic
 * (FLUTE or Batched 1-Steiner approximation).
 *
 * A Rectilinear Steiner Tree (RST) connects a set of points using only
 * horizontal and vertical segments, minimizing total length.
 * NP-hard in general, but heuristic approximations exist.
 *
 * Reference: C. Chu, "FLUTE: Fast Lookup Table Based Wirelength
 *            Estimation Technique", ICCAD 2004.
 *
 * @param result  Placement result
 * @return        RST wire length estimate in mm
 */
double placement_cost_steiner(const PlacementResult* result);

/* ============================================================================
 * Cost Functions: Thermal
 * ============================================================================ */

/**
 * Compute thermal cost based on component temperature violations.
 *
 * Cost = Σ max(0, T_j_est(i) - T_j_max(i))²
 * where T_j_est accounts for self-heating and neighbor coupling.
 *
 * @param result     Placement result
 * @param ambient_C  Ambient temperature in Celsius
 * @return           Thermal cost (lower is better, 0 = all within limits)
 */
double placement_cost_thermal(const PlacementResult* result, double ambient_C);

/**
 * Compute thermal gradient cost — penalizes uneven heat distribution.
 *
 * Cost = variance of component junction temperatures.
 * Encourages spreading heat-generating components evenly.
 *
 * @param result     Placement result
 * @param ambient_C  Ambient temperature
 * @return           Thermal gradient cost
 */
double placement_cost_thermal_gradient(const PlacementResult* result,
                                        double ambient_C);

/* ============================================================================
 * Cost Functions: Signal Integrity
 * ============================================================================ */

/**
 * Compute signal integrity cost based on critical net lengths.
 *
 * Cost = Σ max(0, length(n) - max_length(n))² for all critical nets.
 * Penalizes placement that forces excessively long traces for
 * high-speed or impedance-controlled signals.
 *
 * @param result  Placement result
 * @return        Signal integrity cost
 */
double placement_cost_signal_integrity(const PlacementResult* result);

/* ============================================================================
 * Cost Functions: Density & Overlap
 * ============================================================================ */

/**
 * Compute overlap cost — penalizes overlapping component footprints.
 *
 * Cost = Σ max(0, overlap_area(i,j)) over all component pairs.
 * Overlap is never acceptable in final placement, but the optimizer
 * uses graded penalties to guide the search.
 *
 * @param result  Placement result
 * @return        Total overlap area penalty
 */
double placement_cost_overlap(const PlacementResult* result);

/**
 * Compute local density cost — penalizes areas with excessive
 * component density that would make routing impossible.
 *
 * Divides board into grid cells of size grid_size × grid_size mm.
 * For each cell with density > max_density:
 *   cost += (density - max_density)² * cell_area
 *
 * Reference: R. H. J. M. Otten, "Automatic Floorplan Design", DAC 1982.
 *
 * @param result       Placement result
 * @param grid_size    Grid cell size in mm
 * @param max_density  Maximum allowed area fraction (0-1)
 * @return             Density violation cost
 */
double placement_cost_density(const PlacementResult* result,
                               double grid_size, double max_density);

/* ============================================================================
 * Cost Functions: Manufacturability
 * ============================================================================ */

/**
 * Compute manufacturability (DFM) cost.
 *
 * Penalizes:
 *   - Components too close to board edge (< 3mm for THT, < 1.5mm for SMD)
 *   - Components placed outside wave-solderable orientation
 *   - Tall components up-wave from short ones (shadowing risk)
 *
 * @param result  Placement result
 * @return        DFM cost
 */
double placement_cost_manufacturability(const PlacementResult* result);

/* ============================================================================
 * Total Cost Computation
 * ============================================================================ */

/**
 * Weight structure for multi-objective cost aggregation.
 */
typedef struct {
    double w_hpwl;
    double w_thermal;
    double w_thermal_gradient;
    double w_signal_integrity;
    double w_overlap;
    double w_density;
    double w_manufacturability;
} CostWeights;

/**
 * Initialize default cost weights.
 *
 * Default: wire length dominant (0.5), with balanced secondary weights.
 *
 * @param weights  Weights structure to initialize
 */
void placement_cost_weights_init_default(CostWeights* weights);

/**
 * Compute total weighted placement cost.
 *
 * Cost = Σ w_i * c_i for all cost components.
 *
 * @param result      Placement result
 * @param weights     Cost weights
 * @param ambient_C   Ambient temperature for thermal costs
 * @param grid_size   Grid size for density cost
 * @param max_density Maximum density for density cost
 * @return            PlacementCost with all components populated
 */
PlacementCost placement_cost_compute_total(const PlacementResult* result,
                                            const CostWeights* weights,
                                            double ambient_C,
                                            double grid_size,
                                            double max_density);

/* ============================================================================
 * L8 Advanced: Multi-Objective Pareto Optimization
 * ============================================================================ */

/** A single point in objective space */
typedef struct {
    double hpwl;
    double thermal;
    double signal_integrity;
    uint32_t origin_index;  /* Index into the population that produced this point */
} ObjectivePoint;

/** Pareto front — set of non-dominated solutions */
typedef struct {
    uint32_t        count;
    ObjectivePoint* points;  /* Dynamically allocated */
    uint32_t        capacity;
} ParetoFront;

/**
 * Initialize a Pareto front with a given capacity.
 */
void placement_pareto_front_init(ParetoFront* front, uint32_t capacity);

/**
 * Free a Pareto front.
 */
void placement_pareto_front_free(ParetoFront* front);

/**
 * Check if point a dominates point b.
 *
 * Point a dominates b iff:
 *   a_i ≤ b_i for all objectives i, AND a_j < b_j for at least one j.
 *
 * @param a  First objective point
 * @param b  Second objective point
 * @return   True if a dominates b
 */
bool placement_pareto_dominates(const ObjectivePoint* a,
                                 const ObjectivePoint* b);

/**
 * Insert a point into the Pareto front, maintaining non-dominance.
 *
 * If the new point is dominated by any existing point, it is discarded.
 * If the new point dominates any existing points, those are removed.
 *
 * @param front  Pareto front to update
 * @param point  Point to insert
 * @return       True if point was added (not dominated)
 */
bool placement_pareto_front_insert(ParetoFront* front,
                                    const ObjectivePoint* point);

/**
 * Compute the hypervolume indicator (S-metric) of a Pareto front
 * relative to a reference point. Used to compare optimizer quality.
 *
 * @param front      Pareto front
 * @param ref_point  Reference point (must be dominated by all front points)
 * @return           Hypervolume (higher = better spread/convergence)
 */
double placement_pareto_hypervolume(const ParetoFront* front,
                                     const ObjectivePoint* ref_point);

#ifdef __cplusplus
}
#endif

#endif /* PLACEMENT_OPTIMIZER_H */
