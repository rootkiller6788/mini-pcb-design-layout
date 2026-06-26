/**
 * @file    dfm_cost.h
 * @brief   PCB Manufacturing Cost Models - L3 Structures, L5 Algorithms
 *
 * @details Cost estimation models covering materials, processing,
 *          testing, and yield-loss costs. Includes tolerance analysis
 *          for cost-vs-precision trade-offs.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - PCB cost drivers (layers, area, holes, finish)
 *     - NRE (Non-Recurring Engineering) costs
 *     - Per-unit cost structure
 *   L3 - Mathematical Structures:
 *     - Cost functions, linear and nonlinear
 *     - Tolerance-cost models (inverse power, exponential)
 *   L5 - Algorithms:
 *     - Cost optimization under manufacturing constraints
 *     - Tolerance allocation (worst-case, RSS, Monte Carlo)
 *
 * Reference: IPC-2221 cost guidelines
 *            R. L. Shook, "PCB Cost Modeling" (2004)
 */

#ifndef DFM_COST_H
#define DFM_COST_H

#include "dfm_core.h"
#include <stddef.h>
#include <stdbool.h>

/* ---- Cost Categories ---- */

typedef enum {
    COST_MATERIAL     = 0,
    COST_LABOR        = 1,
    COST_TOOLING      = 2,
    COST_TESTING      = 3,
    COST_YIELD_LOSS   = 4,
    COST_NRE          = 5,
    COST_SHIPPING     = 6,
    COST_OVERHEAD     = 7
} cost_category_t;

/* ---- Cost Breakdown ---- */

typedef struct {
    double material_cost;
    double labor_cost;
    double tooling_cost;
    double testing_cost;
    double yield_loss_cost;
    double nre_cost;
    double shipping_cost;
    double overhead_cost;
    double total_cost;
    double cost_per_board;
    double cost_per_unit_area;
    int    quantity;
    bool   is_prototype;  /* true = includes NRE allocation */
} cost_breakdown_t;

/* ---- Tolerance-Cost Model ---- */

/**
 * Tolerance-cost model: tighter tolerances exponentially increase cost.
 *
 * C(t) = C_base * (t_base / t)^p
 *
 * where C_base = cost at reference tolerance
 *       t_base = reference tolerance
 *       t      = target tolerance
 *       p      = cost sensitivity exponent (typical 1.0-3.0)
 */
typedef struct {
    double reference_tolerance_um;
    double reference_cost;
    double sensitivity_exponent;
    double min_achievable_um;
    double max_practical_um;
} tolerance_cost_model_t;

/* ---- Layer Count Cost ---- */

/**
 * Estimate PCB cost based on key parameters.
 *
 * Cost scales with:
 *   - Area (mm^2)
 *   - Layer count (nonlinear: each additional layer pair adds fixed cost)
 *   - IPC class (higher class adds ~30-50% per step)
 *   - Surface finish type
 *   - Copper weight
 *   - Minimum trace/spacing
 *
 * @param area_mm2           Board area
 * @param num_layers         Number of copper layers
 * @param ipc_class          IPC class
 * @param finish             Surface finish
 * @param copper             Copper weight
 * @param quantity           Production quantity
 * @param is_prototype       Prototype or production pricing
 * @return                   Cost breakdown
 */
cost_breakdown_t estimate_pcb_cost(double area_mm2, int num_layers,
                                    ipc_class_t ipc_class,
                                    surface_finish_t finish,
                                    copper_weight_t copper,
                                    int quantity, bool is_prototype);

/* ---- Tolerance Allocation ---- */

typedef enum {
    TOL_ALLOC_WORST_CASE = 0,
    TOL_ALLOC_RSS        = 1,
    TOL_ALLOC_STATISTICAL = 2
} tolerance_method_t;

typedef struct {
    tolerance_method_t method;
    double total_tolerance_um;
    double component_tolerances[20];
    int    num_components;
    double cpk_achieved;
    bool   acceptable;
} tolerance_allocation_t;

/**
 * Allocate tolerances across manufacturing steps.
 *
 * Worst-case: sum of individual tolerances
 * RSS: sqrt(sum of squares) - assumes normal distributions
 * Statistical: Monte Carlo based on process capability
 *
 * @param required_total_um  Required total tolerance (um)
 * @param processes          Array of process capabilities (Cpk per step)
 * @param num_processes      Number of processes in the chain
 * @param method             Allocation method
 * @return                   Tolerance allocation result
 */
tolerance_allocation_t allocate_tolerances(double required_total_um,
                                            const double *processes_cpk,
                                            int num_processes,
                                            tolerance_method_t method);

/* ---- NRE Cost ---- */

/**
 * Calculate NRE (Non-Recurring Engineering) cost.
 *
 * NRE includes:
 *   - CAM/engineering setup
 *   - Tooling fabrication (drill files, test fixtures)
 *   - First article inspection
 *   - Stencil fabrication (for SMT)
 *
 * @param num_layers     Number of layers
 * @param num_holes      Number of unique drill sizes
 * @param has_impedance  Requires impedance control
 * @param ipc_class      IPC class
 * @return               NRE cost in USD
 */
double calculate_nre_cost(int num_layers, int num_holes,
                          bool has_impedance, ipc_class_t ipc_class);

/* ---- Quantity Discount ---- */

/**
 * Compute quantity discount factor.
 *
 * Uses Wright's learning curve model:
 *   discount_factor = q^b
 *   where b = log(learning_rate) / log(2)
 *
 * Typical learning rates: 85-95% for PCB fabrication.
 *
 * @param quantity       Order quantity
 * @param learning_rate  Learning rate (0.85 = 85% learning)
 * @return               Discount factor (< 1.0 for quantities > 1)
 */
double compute_quantity_discount(int quantity, double learning_rate);

/* ---- Cost Optimization ---- */

/**
 * Find optimal layer count that minimizes per-board cost.
 *
 * More layers increase fabrication cost but may reduce board area
 * (enabling smaller panels). This finds the sweet spot.
 *
 * @param board_area_mm2    Board area for 2-layer baseline
 * @param max_layers        Maximum layers to consider
 * @param quantity          Production quantity
 * @param ipc_class         IPC class
 * @return                  Optimal number of layers
 */
int optimize_layer_count(double board_area_mm2, int max_layers,
                         int quantity, ipc_class_t ipc_class);

/* Board area estimation from component count */
double estimate_board_area_from_components(int num_components,
                                            double density_comps_per_cm2);

/* Manufacturing complexity index */
double compute_complexity_index(int num_layers,
                                 double min_trace_um,
                                 double min_space_um,
                                 bool has_microvia,
                                 double board_thickness_mm,
                                 bool has_impedance_control,
                                 substrate_material_t material);

#endif /* DFM_COST_H */
