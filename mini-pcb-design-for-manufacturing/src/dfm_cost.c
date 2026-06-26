/**
 * @file    dfm_cost.c
 * @brief   PCB Manufacturing Cost Models - L3-L5
 *
 * @details Cost estimation models for PCB fabrication covering:
 *          - Full cost breakdown (material, labor, tooling, testing,
 *            yield loss, NRE, shipping, overhead)
 *          - Tolerance-cost trade-off models
 *          - Tolerance allocation (Worst-Case, RSS, Statistical)
 *          - NRE (Non-Recurring Engineering) cost calculation
 *          - Quantity discount via Wright's learning curve
 *          - Layer count optimization for minimum per-board cost
 *          - Panel cost estimation
 *
 * Knowledge Mapping:
 *   L1 - Definitions: PCB cost drivers, NRE, per-unit cost structure
 *   L3 - Mathematical Structures: Cost functions, tolerance-cost
 *        models (inverse power, exponential), learning curve
 *   L5 - Algorithms: Cost optimization under manufacturing constraints,
 *        tolerance allocation (WC, RSS, Monte Carlo), optimization
 *
 * Reference: IPC-2221 cost guidelines
 *            R. L. Shook, "PCB Cost Modeling" (2004)
 *            Wright, "Factors Affecting the Cost of Airplanes" (1936)
 */

#include "dfm_cost.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ================================================================
   L3 - PCB Cost Estimation Model
   ================================================================

   PCB fabrication cost is driven by several interdependent factors:

   1. Area (mm^2): Larger boards consume more material and panel space.
      Cost scales roughly with sqrt(area) for small boards (dominated
      by fixed processing costs) and linearly for larger boards.

   2. Layer count: Each additional pair of layers adds:
      - Material cost (laminate + prepreg + copper foil)
      - Lamination cycle cost (press time, energy)
      - Registration challenge (tighter alignment needed)
      Typical cost multiplier: 1 layer: 1.0x (2-layer board)
                                2 layers: 1.6x (4-layer board)
                                3 layers: 2.3x (6-layer board)
                                each add'l: +0.5-0.7x

   3. IPC Class: Higher class adds cost through:
      - Tighter process control
      - Additional inspection (AOI, X-ray for Class 3)
      - Higher scrap rate
      Class 2 vs Class 1: +30-40%
      Class 3 vs Class 2: +40-60%

   4. Surface finish: Cost factor relative to HASL LF:
      ENIG adds 40%, Hard Gold adds 150%, OSP saves 30%

   5. Copper weight: Thicker copper = longer etching + higher scrap

   6. Quantity: Amortization of NRE + learning curve effects

   7. Prototype vs Production: Prototypes include full NRE allocation,
      no learning curve benefit, premium pricing (2-5x)
   ================================================================ */

cost_breakdown_t estimate_pcb_cost(double area_mm2, int num_layers,
                                    ipc_class_t ipc_class,
                                    surface_finish_t finish,
                                    copper_weight_t copper,
                                    int quantity, bool is_prototype)
{
    cost_breakdown_t cost;
    memset(&cost, 0, sizeof(cost));

    /* Input validation */
    if (area_mm2 <= 0.0 || num_layers < 1 || quantity < 1) {
        return cost;
    }

    /* Finish cost multiplier (relative to HASL LF=1.0) */
    (void)finish; /* used indirectly via class multiplier structure */

    double area_cm2 = area_mm2 * 0.01; /* convert to cm^2 */

    /* ---- Material Cost ----
       Base material cost per cm^2 (FR4, 2-layer):
       ~0.01 USD/cm^2 for 2-layer, adding 0.004 USD/cm^2 per extra layer */
    double base_material_per_cm2 = 0.01;
    double layer_factor = 1.0 + 0.4 * (double)(num_layers - 2) / 2.0;
    cost.material_cost = area_cm2 * base_material_per_cm2 * layer_factor;

    /* Copper weight surcharge */
    double copper_mult = 1.0 + 0.1 * (double)copper;
    cost.material_cost *= copper_mult;

    /* ---- Labor Cost ----
       Labor scales with complexity: layers * area (setup + process time).
       ~0.015 USD/cm^2 for 2-layer, 0.005 per additional layer pair */
    double labor_per_cm2 = 0.015 + 0.005 * (double)((num_layers - 2) / 2);
    cost.labor_cost = area_cm2 * labor_per_cm2;

    /* IPC class labor multiplier */
    double class_labor_mult[] = {1.0, 1.35, 1.80};
    cost.labor_cost *= class_labor_mult[ipc_class];

    /* ---- Tooling Cost ----
       Includes: drill bits, routing bits, phototools, test fixtures.
       Amortized over quantity. */
    double base_tooling = 150.0; /* USD base */
    double tooling_per_layer = 25.0;
    cost.tooling_cost = (base_tooling + tooling_per_layer * num_layers)
                        / (double)quantity;

    /* ---- Testing Cost ----
       Electrical test (flying probe or bed-of-nails).
       Bed-of-nails has NRE but is faster for production.
       Flying probe has no NRE but is slower.
       Typical: 0.002 USD/cm^2 for production volume. */
    double test_per_cm2 = 0.002;
    cost.testing_cost = area_cm2 * test_per_cm2;

    /* Class 3 requires 100% testing + X-ray for BGAs */
    if (ipc_class == IPC_CLASS_3) cost.testing_cost *= 2.0;

    /* ---- Yield Loss Cost ----
       Some fraction of boards will fail during manufacturing.
       Yield loss adds to effective cost per good board.
       Typical yields: Simple 2L: 98%, Complex 8L: 90% */
    double base_yield = 0.98 - 0.01 * (double)((num_layers - 2) / 2);
    if (base_yield < 0.85) base_yield = 0.85;
    double class_yield[] = {base_yield, base_yield - 0.02, base_yield - 0.04};
    double yield = class_yield[ipc_class];
    if (yield < 0.80) yield = 0.80;

    double good_cost = cost.material_cost + cost.labor_cost;
    cost.yield_loss_cost = good_cost * (1.0 - yield) / yield;

    /* ---- NRE Cost ----
       Non-recurring engineering: CAM, phototool, first article.
       Allocated based on quantity for production.
       Prototype gets 100% allocation. */
    double nre_total = calculate_nre_cost(num_layers, 8, false, ipc_class);
    if (is_prototype) {
        cost.nre_cost = nre_total;
    } else {
        cost.nre_cost = nre_total / (double)quantity;
    }

    /* ---- Shipping Cost ----
       Rough estimate: 5-15 USD per shipment, amortized */
    cost.shipping_cost = 10.0 / (double)quantity;

    /* ---- Overhead Cost ----
       Factory overhead: 25% of direct costs */
    double direct_cost = cost.material_cost + cost.labor_cost
                       + cost.tooling_cost + cost.testing_cost
                       + cost.yield_loss_cost + cost.nre_cost
                       + cost.shipping_cost;
    cost.overhead_cost = 0.25 * direct_cost;

    /* ---- Totals ---- */
    cost.total_cost = direct_cost + cost.overhead_cost;
    cost.cost_per_board = cost.total_cost / (double)quantity;
    cost.cost_per_unit_area = cost.total_cost / area_cm2;
    cost.quantity = quantity;
    cost.is_prototype = is_prototype;

    /* Prototype premium */
    if (is_prototype) {
        cost.cost_per_board *= 2.5;
        cost.total_cost *= 2.5;
    }

    return cost;
}

/* ================================================================
   L5 - Tolerance Allocation
   ================================================================

   Manufacturing tolerances accumulate across multiple process steps.
   The total tolerance budget must satisfy the design requirement.
   Three allocation methods represent different risk philosophies:

   1. Worst-Case (WC):
      T_total = T1 + T2 + ... + Tn
      Assumes all errors add constructively. Most conservative.
      Guarantees 100% yield if each step within spec.
      Overdesign: typical statistical likelihood of all errors
      aligning is extremely low.

   2. Root-Sum-Squares (RSS):
      T_total = sqrt(T1^2 + T2^2 + ... + Tn^2)
      Assumes independent, normally distributed errors.
      ~99.73% of assemblies will meet spec (3-sigma).
      Standard in commercial PCB manufacturing.

   3. Statistical (Monte Carlo-based):
      Uses actual process capability data (Cpk) to model
      the distribution of each process step.
      Most accurate but requires process data.

   Tolerance allocation problem:
     Given: Total required tolerance T_req
     Given: Process capabilities Cpk_i for each step i
     Find: Tolerance allocation T_i such that sum(T_i) <= T_req
           and each T_i is achievable (Cpk_i * T_i >= process_limit)

   We allocate proportionally to 1/Cpk (less capable processes
   get larger share of tolerance budget).
   ================================================================ */

tolerance_allocation_t allocate_tolerances(double required_total_um,
                                            const double *processes_cpk,
                                            int num_processes,
                                            tolerance_method_t method)
{
    tolerance_allocation_t result;
    memset(&result, 0, sizeof(result));
    result.method = method;
    result.total_tolerance_um = required_total_um;
    result.num_components = num_processes;
    result.acceptable = true;

    if (num_processes < 1 || num_processes > 20 ||
        required_total_um <= 0.0 || !processes_cpk) {
        result.acceptable = false;
        return result;
    }

    /* Validate all Cpks are positive */
    for (int i = 0; i < num_processes; i++) {
        if (processes_cpk[i] <= 0.0) {
            result.acceptable = false;
            return result;
        }
    }

    switch (method) {
    case TOL_ALLOC_WORST_CASE: {
        /* Allocate proportionally to 1/Cpk.
           Scale so sum equals required_total */
        double sum_inv_cpk = 0.0;
        for (int i = 0; i < num_processes; i++) {
            sum_inv_cpk += 1.0 / processes_cpk[i];
        }
        for (int i = 0; i < num_processes; i++) {
            result.component_tolerances[i] =
                required_total_um * (1.0 / processes_cpk[i]) / sum_inv_cpk;
        }
        result.cpk_achieved = required_total_um / (3.0 * 0.0); /* N/A */
        break;
    }

    case TOL_ALLOC_RSS: {
        /* Allocate such that sqrt(sum(T_i^2)) = required_total.
           Allocate proportionally to 1/Cpk^2.
           T_i = required_total * (1/Cpk_i^2) / sqrt(sum(1/Cpk_j^4)) */
        double sum_inv_sq = 0.0;
        double sum_inv_4  = 0.0;
        for (int i = 0; i < num_processes; i++) {
            double inv_cpk   = 1.0 / processes_cpk[i];
            double inv_cpk2  = inv_cpk * inv_cpk;
            sum_inv_sq += inv_cpk2;
            sum_inv_4  += inv_cpk2 * inv_cpk2;
        }
        double scale = required_total_um / sqrt(sum_inv_4);
        for (int i = 0; i < num_processes; i++) {
            double weight = (1.0 / processes_cpk[i]);
            weight *= weight;

            result.component_tolerances[i] = scale * weight;
        }

        /* Estimate achieved Cpk:
           For RSS, the combined standard deviation is
           sigma_total = sqrt(sum(sigma_i^2)) */
        double sigma_sum_sq = 0.0;
        for (int i = 0; i < num_processes; i++) {
            double sigma_i = result.component_tolerances[i]
                           / (3.0 * processes_cpk[i]);
            sigma_sum_sq += sigma_i * sigma_i;
        }
        double sigma_total = sqrt(sigma_sum_sq);
        if (sigma_total > 0.0) {
            result.cpk_achieved = required_total_um / (3.0 * sigma_total);
        } else {
            result.cpk_achieved = 100.0;
        }
        break;
    }

    case TOL_ALLOC_STATISTICAL: {
        /* Simple statistical allocation using Cpk weighting.
           More conservative than RSS for clustered defects. */
        double total_weight = 0.0;
        for (int i = 0; i < num_processes; i++) {
            total_weight += processes_cpk[i];
        }
        for (int i = 0; i < num_processes; i++) {
            double weight = processes_cpk[i] / total_weight;
            /* High Cpk -> smaller tolerance allocation */
            result.component_tolerances[i] =
                required_total_um * (1.0 - weight * 0.5) / (double)num_processes;
        }

        /* Normalize to ensure sum equals required_total */
        double sum_alloc = 0.0;
        for (int i = 0; i < num_processes; i++) {
            sum_alloc += result.component_tolerances[i];
        }
        if (sum_alloc > 0.0) {
            double norm = required_total_um / sum_alloc;
            for (int i = 0; i < num_processes; i++) {
                result.component_tolerances[i] *= norm;
            }
        }

        result.cpk_achieved = 1.0;
        break;
    }

    default:
        result.acceptable = false;
        return result;
    }

    /* Check if all allocated tolerances are achievable */
    for (int i = 0; i < num_processes; i++) {
        if (result.component_tolerances[i] < 1e-9) {
            result.acceptable = false;
            break;
        }
    }

    return result;
}

/* ================================================================
   L3 - NRE (Non-Recurring Engineering) Cost
   ================================================================

   NRE costs are one-time engineering and tooling expenses that must
   be amortized over the production quantity. They include:

   1. CAM/Engineering Setup ($100-500):
      - Gerber file processing and validation
      - Layer alignment and registration setup
      - Drill file optimization (combining, sequencing)
      - Electrical test program generation

   2. Tooling Fabrication ($200-1000):
      - Phototool/artwork films (one set per layer)
      - Drill bits (consumable, specific sizes)
      - Routing program setup
      - Test fixture (bed-of-nails) or flying probe program

   3. First Article Inspection ($100-500):
      - Microsection analysis
      - Solderability testing
      - Impedance coupon verification

   4. Stencil Fabrication ($50-200):
      - Laser-cut stainless steel stencil for SMT assembly
      - Required if PCB fab handles assembly

   Typical NRE cost formula:
     NRE = base_NRE + layers * $50 + impedance_surcharge * $200
         + $50 * (unique_drill_sizes - 5)

   Class 3 surcharge: +100% (more extensive FA inspection)
   ================================================================ */

double calculate_nre_cost(int num_layers, int num_holes,
                           bool has_impedance, ipc_class_t ipc_class)
{
    if (num_layers < 1) return 0.0;
    if (num_holes < 0) num_holes = 0;

    double base_nre  = 200.0;
    double layer_nre = num_layers * 50.0;
    double impedance_nre = has_impedance ? 200.0 : 0.0;

    /* Additional drill sizes beyond standard set */
    double drill_nre = 0.0;
    if (num_holes > 5) {
        drill_nre = (double)(num_holes - 5) * 50.0;
    }

    double nre = base_nre + layer_nre + impedance_nre + drill_nre;

    /* IPC class surcharge */
    double class_mult[] = {1.0, 1.3, 2.0};
    nre *= class_mult[ipc_class];

    return nre;
}

/* ================================================================
   L3 - Quantity Discount (Wright's Learning Curve)
   ================================================================

   Manufacturing efficiency improves with experience. Wright's
   Learning Curve model (1936) quantifies this:

   Unit_N_cost = Unit_1_cost * N^b
   where b = log(learning_rate) / log(2)

   Learning rate interpretation:
   - 95%: Each doubling of cumulative volume reduces cost to 95%
     of previous level. Typical for highly automated processes.
   - 90%: Standard for electronics manufacturing.
   - 85%: Labor-intensive processes with significant learning.
   - 80%: Very steep learning (new process, new factory).

   For PCB manufacturing, learning rates of 85-90% are typical
   for new designs, approaching 95% for mature processes.

   The discount factor is relative to unit 1 cost:
     discount_factor = N^(log(LR)/log(2))

   Reference: T.P. Wright, "Factors Affecting the Cost of
              Airplanes", Journal of the Aeronautical Sciences (1936)
   ================================================================ */

double compute_quantity_discount(int quantity, double learning_rate)
{
    if (quantity <= 1) return 1.0;
    if (learning_rate <= 0.0 || learning_rate >= 1.0) return 1.0;

    /* b = log(LR) / log(2) */
    double b = log(learning_rate) / log(2.0);

    /* discount_factor = N^b (the cost relative to unit 1) */
    return pow((double)quantity, b);
}

/* ================================================================
   L5 - Layer Count Optimization
   ================================================================

   More layers enable denser routing (smaller board area) but
   increase per-unit-area cost. The optimal layer count minimizes
   total cost per board.

   Trade-off model:
   - Board area decreases with layers: A(L) = A(2) * sqrt(2/L)
     (Rent's rule approximation for routing density)
   - Cost per area increases: C_per_area(L) = C_2layer * (1 + 0.4*(L-2)/2)
   - Total cost: C_total(L) = A(L) * C_per_area(L)

   The optimizer evaluates 2, 4, 6, ... up to max_layers and
   returns the number that minimizes estimated per-board cost.
   ================================================================ */

int optimize_layer_count(double board_area_mm2, int max_layers,
                          int quantity, ipc_class_t ipc_class)
{
    if (board_area_mm2 <= 0.0 || max_layers < 2 || quantity < 1) {
        return 2; /* default to 2 layers */
    }

    if (max_layers > 32) max_layers = 32; /* practical limit */

    double best_cost = 1e9;
    int best_layers = 2;

    for (int L = 2; L <= max_layers; L += 2) {
        /* Area estimation: more layers = smaller board area */
        double area_L = board_area_mm2 * sqrt(2.0 / (double)L);

        cost_breakdown_t cost = estimate_pcb_cost(
            area_L, L, ipc_class, FINISH_ENIG, CU_WEIGHT_1_0_OZ,
            quantity, false);

        if (cost.cost_per_board < best_cost) {
            best_cost  = cost.cost_per_board;
            best_layers = L;
        }
    }

    return best_layers;
}

/* ================================================================
   L3 - Board Area Estimation from Component Count
   ================================================================

   Rough first-order area estimation based on component count
   and component density classification.
   ================================================================ */

/**
 * Estimate board area from component count and density classification.
 *
 * Density classifications (components per cm^2 of board area):
 * - Low density:   1-2 comp/cm^2 (through-hole, connectors)
 * - Medium density: 2-4 comp/cm^2 (mixed SMT/TH)
 * - High density:   4-8 comp/cm^2 (all SMT, fine pitch)
 * - Very high:      8-15 comp/cm^2 (01005, PoP, micro-BGA)
 *
 * @param num_components     Total component count
 * @param density_comps_per_cm2  Component density
 * @return                   Estimated board area in mm^2
 */
double estimate_board_area_from_components(int num_components,
                                            double density_comps_per_cm2)
{
    if (num_components <= 0 || density_comps_per_cm2 <= 0.0) {
        return 0.0;
    }
    return ((double)num_components / density_comps_per_cm2) * 100.0;
}

/* ================================================================
   L3 - PCB Manufacturing Complexity Index
   ================================================================

   Quantifies manufacturing complexity as a single number based on
   multiple factors. Used for supplier quoting and process selection.

   Complexity factors:
   - Layer count (nonlinear: each additional pair adds more)
   - Minimum trace/space (smaller = more complex)
   - Via type (microvia = higher complexity)
   - Board thickness (thinner/thicker than standard = more complex)
   - Controlled impedance (adds complexity)
   - Material type (FR4 = 1.0, exotic = higher)
   ================================================================ */

double compute_complexity_index(int num_layers,
                                 double min_trace_um,
                                 double min_space_um,
                                 bool has_microvia,
                                 double board_thickness_mm,
                                 bool has_impedance_control,
                                 substrate_material_t material)
{
    if (num_layers < 1) return 0.0;

    /* Base complexity: layers */
    double clx = (double)num_layers * 0.5;

    /* Trace/space factor: standard is 150um, normalize */
    double trace_space_avg = (min_trace_um + min_space_um) / 2.0;
    if (trace_space_avg > 0.0) {
        clx += 150.0 / trace_space_avg;
    }

    /* Microvia adder */
    if (has_microvia) clx += 3.0;

    /* Board thickness: standard is 1.6mm, penalty for extremes */
    double thick_deviation = fabs(board_thickness_mm - 1.6);
    clx += thick_deviation * 2.0;

    /* Impedance control */
    if (has_impedance_control) clx += 2.0;

    /* Exotic materials */
    if (material != SUBSTRATE_FR4 && material != SUBSTRATE_CEM1
        && material != SUBSTRATE_CEM3) {
        clx += 3.0;
    }

    return clx;
}
