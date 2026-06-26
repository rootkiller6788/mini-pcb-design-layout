/**
 * @file    test_dfm.c
 * @brief   Comprehensive test suite for DFM library
 *
 * Tests all public APIs across the 6 modules:
 *   - dfm_core: IPC classes, substrate/finish lookup, copper conversion,
 *               process capability, DRC management, OEE
 *   - dfm_rules: Trace width, spacing, annular ring, mask, silkscreen,
 *                drill, edge, thermal relief, IPC-2221 clearance
 *   - dfm_cost: PCB cost estimation, tolerance allocation, NRE,
 *               quantity discount, layer optimization
 *   - dfm_panel: Panel utilization, tooling holes, fiducial marks,
 *                tab placement, copper thieving
 *   - dfm_thermal: Junction temperature, copper balance, trace current,
 *                  thermal vias, warpage, spreading resistance
 *   - dfm_yield: All yield models, Monte Carlo, critical area
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "dfm_core.h"
#include "dfm_rules.h"
#include "dfm_cost.h"
#include "dfm_panel.h"
#include "dfm_thermal.h"
#include "dfm_yield.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { tests_passed++; } \
    else { \
        tests_failed++; \
        printf("FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
    } \
} while(0)

#define TEST_NEAR(name, val, expected, tol) do { \
    if (fabs((val) - (expected)) <= (tol)) { tests_passed++; } \
    else { \
        tests_failed++; \
        printf("FAIL: %s: got %.6f, expected %.6f (tol=%.6f)\n", \
               name, (val), (expected), (tol)); \
    } \
} while(0)

/* ================================================================
   dfm_core Tests
   ================================================================ */
static void test_ipc_classes(void)
{
    TEST("IPC_CLASS_1 name", strstr(ipc_class_name(IPC_CLASS_1), "Class 1") != NULL);
    TEST("IPC_CLASS_2 name", strstr(ipc_class_name(IPC_CLASS_2), "Class 2") != NULL);
    TEST("IPC_CLASS_3 name", strstr(ipc_class_name(IPC_CLASS_3), "Class 3") != NULL);
    TEST("IPC invalid name", strstr(ipc_class_name((ipc_class_t)999), "Unknown") != NULL);
}

static void test_substrate_lookup(void)
{
    const substrate_properties_t *fr4 = substrate_lookup(SUBSTRATE_FR4);
    TEST("FR4 lookup not null", fr4 != NULL);
    TEST_NEAR("FR4 Dk", fr4->dielectric_constant, 4.50, 0.01);
    TEST_NEAR("FR4 Df", fr4->loss_tangent, 0.020, 0.001);
    TEST_NEAR("FR4 Tg", fr4->tg_celsius, 140.0, 1.0);

    const substrate_properties_t *rogers = substrate_lookup(SUBSTRATE_ROGERS_4350);
    TEST("Rogers lookup", rogers != NULL);
    TEST_NEAR("Rogers Dk", rogers->dielectric_constant, 3.48, 0.01);

    const substrate_properties_t *invalid = substrate_lookup((substrate_material_t)999);
    TEST("Invalid substrate returns NULL", invalid == NULL);
}

static void test_finish_lookup(void)
{
    const finish_properties_t *enig = finish_lookup(FINISH_ENIG);
    TEST("ENIG lookup", enig != NULL);
    TEST_NEAR("ENIG cost factor", enig->cost_factor, 1.40, 0.01);
    TEST("ENIG RoHS", enig->rohs_compliant == true);
    TEST("ENIG wire bondable", enig->wire_bondable == true);

    const finish_properties_t *osp = finish_lookup(FINISH_OSP);
    TEST("OSP lookup", osp != NULL);
    TEST("OSP not wire bondable", osp->wire_bondable == false);

    const finish_properties_t *invalid = finish_lookup((surface_finish_t)999);
    TEST("Invalid finish NULL", invalid == NULL);
}

static void test_copper_weight(void)
{
    double cu_1oz = copper_weight_to_um(CU_WEIGHT_1_0_OZ, false);
    TEST_NEAR("1oz unplated", cu_1oz, 34.79, 0.1);

    double cu_1oz_plated = copper_weight_to_um(CU_WEIGHT_1_0_OZ, true);
    TEST("1oz plated > unplated", cu_1oz_plated > cu_1oz);

    double cu_2oz = copper_weight_to_um(CU_WEIGHT_2_0_OZ, false);
    TEST_NEAR("2oz unplated", cu_2oz, 69.58, 0.2);
}

static void test_process_capability(void)
{
    /* Capable process: centered, tight distribution */
    double good_data[] = {100.0, 101.0, 99.0, 100.5, 99.5, 100.0};
    process_capability_t cpk_good = compute_process_capability(
        good_data, 6, 110.0, 90.0);
    TEST("Good process capable", cpk_good.capable == true);
    TEST("Good Cpk >= 1.0", cpk_good.cpk >= 1.0);

    /* Not capable: mean shifted UP (closer to USL than LSL) */
    double bad_data[] = {105.0, 106.0, 104.0, 107.0, 105.5, 104.5};
    process_capability_t cpk_bad = compute_process_capability(
        bad_data, 6, 110.0, 90.0);
    /* Mean ~105.3, USL=110 gives small margin, LSL=90 gives large margin.
       cpk_lower = (105.3-90)/3s > cpk_upper = (110-105.3)/3s */
    TEST("Shifted process asymmetric Cpk", cpk_bad.cpk_lower > cpk_bad.cpk_upper);

    /* Zero-variance edge case */
    double same_data[] = {50.0, 50.0, 50.0};
    process_capability_t cpk_zero = compute_process_capability(
        same_data, 3, 60.0, 40.0);
    TEST("Zero variance capable", cpk_zero.capable == true);
    TEST_NEAR("Zero variance Cpk", cpk_zero.cpk, 100.0, 0.1);
}

static void test_drc_result(void)
{
    drc_result_t result;
    drc_result_init(&result, IPC_CLASS_2, 4);
    TEST("DRC init passed", result.passed == true);
    TEST("DRC init empty", result.num_violations == 0);

    drc_violation_t v;
    memset(&v, 0, sizeof(v));
    v.severity = DRC_ERROR;
    v.rule_name = "Min Trace Width";
    v.message = "Trace width too narrow";
    v.measured_value_um = 75.0;
    v.required_value_um = 100.0;
    v.margin_um = -25.0;
    v.location.x_mm = 10.0;
    v.location.y_mm = 20.0;
    v.location.layer = 1;
    v.location.net_name = "VCC";

    drc_result_add(&result, &v);
    TEST("DRC add 1 violation", result.num_violations == 1);
    TEST("DRC failed after error", result.passed == false);
    TEST("DRC 1 error", result.num_errors == 1);

    /* Add warning */
    v.severity = DRC_WARN;
    v.rule_name = "Silkscreen Near Pad";
    drc_result_add(&result, &v);
    TEST("DRC 2 violations", result.num_violations == 2);
    TEST("DRC 1 warning", result.num_warnings == 1);

    /* Report (no crash test) */
    drc_result_report(&result, false);

    drc_result_free(&result);
    TEST("DRC free", result.num_violations == 0);
}

static void test_oee(void)
{
    double oee = compute_pcb_oee(8.0, 1.0, 30.0, 800.0, 780.0);
    TEST("OEE reasonable", oee > 0.0 && oee <= 1.0);

    double oee_zero = compute_pcb_oee(0.0, 0.0, 0.0, 0.0, 0.0);
    TEST_NEAR("OEE zero inputs", oee_zero, 0.0, 0.001);

    double oee_perfect = compute_pcb_oee(8.0, 0.0, 30.0, 960.0, 960.0);
    TEST("OEE near perfect", oee_perfect > 0.80);
}

static void test_via_checks(void)
{
    via_geometry_t via;
    memset(&via, 0, sizeof(via));
    via.type = VIA_THROUGH_HOLE;
    via.drill_diameter_mm = 0.3;
    via.pad_diameter_mm = 0.6;
    via.start_layer = 1;
    via.end_layer = 4;

    TEST("Via annular ring OK", via_annular_ring_ok(&via, IPC_CLASS_2));
    TEST("Via aspect ratio OK", via_aspect_ratio_ok(&via));

    /* Too small annular ring */
    via.pad_diameter_mm = 0.32;
    TEST("Via ring too small", !via_annular_ring_ok(&via, IPC_CLASS_3));
}

/* ================================================================
   dfm_rules Tests
   ================================================================ */
static void test_trace_width(void)
{
    double w = get_min_trace_width(CU_WEIGHT_1_0_OZ, IPC_CLASS_2);
    TEST_NEAR("1oz trace width", w, 100.0, 1.0);

    double w_heavy = get_min_trace_width(CU_WEIGHT_3_0_OZ, IPC_CLASS_3);
    TEST("Heavy copper wider", w_heavy > w);
}

static void test_spacing(void)
{
    double s_low = compute_required_spacing(12.0, true, false, false);
    TEST_NEAR("12V external spacing", s_low, 50.0, 10.0);

    double s_high = compute_required_spacing(300.0, true, false, false);
    TEST("300V > 12V spacing", s_high > s_low);

    /* At V=12: internal base=0.05mm*1.5=0.075mm=75um, external=0.05mm=50um.
       For low voltages with equal base values, the 1.5x multiplier makes
       internal greater than external. */
    double s_internal = compute_required_spacing(12.0, false, false, false);
    double s_external_12v = compute_required_spacing(12.0, true, false, false);
    TEST("Internal > external at 12V", s_internal > s_external_12v);

    double s_coated = compute_required_spacing(300.0, true, true, false);
    TEST("Coated < uncoated", s_coated < s_high);
}

static void test_annular_ring_check(void)
{
    TEST("Class 2 with 150um ring OK",
         check_annular_ring(600.0, 300.0, true, IPC_CLASS_2, 0.0));
    TEST("Class 2 with zero ring fail",
         !check_annular_ring(300.0, 300.0, true, IPC_CLASS_2, 0.0));
    TEST("Class 3 with breakout fail",
         !check_annular_ring(600.0, 300.0, true, IPC_CLASS_3, 45.0));
}

static void test_solder_mask(void)
{
    const solder_mask_rule_t *rule = get_solder_mask_rule(IPC_CLASS_2);
    TEST("Mask rule found", rule != NULL);

    double opening = compute_mask_opening(400.0, 75.0);
    TEST_NEAR("Mask opening", opening, 550.0, 0.1);

    TEST("Mask web OK",
         check_mask_web(550.0, 550.0, 1200.0, 100.0));
    /* Two openings of 600um each, centers 690um apart: web = 690-300-300 = 90 < 100 */
    TEST("Mask web fail",
         !check_mask_web(600.0, 600.0, 690.0, 100.0));
}

static void test_ipc2221_clearance(void)
{
    /* 60V falls in 50-100V range: external=0.40mm*1000=400um */
    double cl = compute_ipc2221_clearance(60.0, true, false, 0.0);
    TEST_NEAR("IPC2221 60V external", cl, 400.0, 50.0);

    /* At 30V, internal base=0.05mm*1.5=0.075mm*1000=75um, external=0.10mm*1000=100um.
       For these low voltages internal can be less. But internal uses 1.5x multiplier.
       At 15V: internal base=0.05*1.5=0.075mm=75um, external=0.05mm=50um => internal > external */
    double cl_internal = compute_ipc2221_clearance(15.0, false, false, 0.0);
    double cl_external_15 = compute_ipc2221_clearance(15.0, true, false, 0.0);
    TEST("Internal > external at 15V", cl_internal > cl_external_15);

    double cl_highalt = compute_ipc2221_clearance(60.0, true, false, 5000.0);
    TEST("High altitude > sea level", cl_highalt > cl);
}

static void test_etch_compensation(void)
{
    double w = compute_etch_compensation(100.0, 35.0, 1.0);
    TEST_NEAR("Etch comp 1oz", w, 170.0, 1.0);
}

static void test_drill_rules(void)
{
    const drill_rule_t *dr = get_drill_rule(false);
    TEST("Standard drill rule", dr != NULL);
    TEST_NEAR("Min drill 0.2mm", dr->min_drill_diameter_mm, 0.20, 0.01);

    const drill_rule_t *da = get_drill_rule(true);
    TEST("Advanced drill rule", da != NULL);
    TEST("Adv min drill < std", da->min_drill_diameter_mm < dr->min_drill_diameter_mm);
}

static void test_thermal_relief(void)
{
    double area = compute_thermal_relief_area(600.0, 1000.0, 250.0, 4, 35.0);
    TEST("Thermal relief area > 0", area > 0.0);
}

static void test_edge_clearance(void)
{
    const edge_clearance_rule_t *ec = get_edge_clearance_rule(IPC_CLASS_3);
    TEST("Edge clearance found", ec != NULL);
    TEST("Class 3 > 0", ec->copper_to_edge_um > 0.0);
}

/* ================================================================
   dfm_cost Tests
   ================================================================ */
static void test_cost_estimation(void)
{
    /* 100x100mm, 4-layer, IPC Class 2, ENIG, 1oz, qty 100 */
    cost_breakdown_t cost = estimate_pcb_cost(
        10000.0, 4, IPC_CLASS_2, FINISH_ENIG, CU_WEIGHT_1_0_OZ, 100, false);
    TEST("Cost total > 0", cost.total_cost > 0.0);
    TEST("Cost per board > 0", cost.cost_per_board > 0.0);
    TEST("Cost material > 0", cost.material_cost > 0.0);

    /* Prototype should be more expensive */
    cost_breakdown_t proto = estimate_pcb_cost(
        10000.0, 4, IPC_CLASS_2, FINISH_ENIG, CU_WEIGHT_1_0_OZ, 10, true);
    TEST("Proto cost > prod cost",
         proto.cost_per_board > cost.cost_per_board);
}

static void test_tolerance_allocation(void)
{
    double cpks[] = {1.0, 1.33, 1.5, 0.8};
    tolerance_allocation_t wc = allocate_tolerances(
        100.0, cpks, 4, TOL_ALLOC_WORST_CASE);
    TEST("WC allocation", wc.acceptable == true);
    double wc_sum = 0.0;
    for (int i = 0; i < 4; i++) wc_sum += wc.component_tolerances[i];
    TEST_NEAR("WC sum equals total", wc_sum, 100.0, 0.1);

    tolerance_allocation_t rss = allocate_tolerances(
        100.0, cpks, 4, TOL_ALLOC_RSS);
    TEST("RSS allocation", rss.acceptable == true);
    double rss_sq = 0.0;
    for (int i = 0; i < 4; i++) {
        rss_sq += rss.component_tolerances[i] * rss.component_tolerances[i];
    }
    TEST_NEAR("RSS sqrt sum", sqrt(rss_sq), 100.0, 0.2);
}

static void test_nre_cost(void)
{
    double nre = calculate_nre_cost(4, 10, true, IPC_CLASS_2);
    TEST("NRE > 0", nre > 100.0);

    double nre_class3 = calculate_nre_cost(4, 10, true, IPC_CLASS_3);
    TEST("Class3 NRE > Class2", nre_class3 > nre);

    double nre_invalid = calculate_nre_cost(0, 0, false, IPC_CLASS_1);
    TEST_NEAR("Invalid NRE = 0", nre_invalid, 0.0, 0.01);
}

static void test_quantity_discount(void)
{
    double d1 = compute_quantity_discount(1, 0.90);
    TEST_NEAR("Qty 1 = 1.0", d1, 1.0, 0.001);

    double d100 = compute_quantity_discount(100, 0.90);
    TEST("Qty 100 < 1.0", d100 < 1.0);

    double d1000 = compute_quantity_discount(1000, 0.90);
    TEST("Qty 1000 < qty 100", d1000 < d100);
}

static void test_layer_optimization(void)
{
    int opt = optimize_layer_count(10000.0, 8, 100, IPC_CLASS_2);
    TEST("Optimal layers >= 2", opt >= 2);
    TEST("Optimal layers <= 8", opt <= 8);
}

static void test_complexity(void)
{
    double clx = compute_complexity_index(4, 100.0, 100.0, false, 1.6, true, SUBSTRATE_FR4);
    TEST("Complexity > 0", clx > 0.0);

    double clx_hdi = compute_complexity_index(6, 75.0, 75.0, true, 1.0, true, SUBSTRATE_ROGERS_4350);
    TEST("HDI > Standard", clx_hdi > clx);
}

/* ================================================================
   dfm_panel Tests
   ================================================================ */
static void test_panel_optimization(void)
{
    panel_optimization_result_t result = optimize_panel_utilization(
        50.0, 50.0, 250.0, 250.0, 2.0, 5.0, 10.0, BREAKAWAY_TAB_ROUTE);
    TEST("Panel boards > 0", result.config.total_boards > 0);
    TEST("Utilization > 0", result.utilization_pct > 0.0);
    TEST("Utilization <= 100", result.utilization_pct <= 100.0);

    double util = calculate_panel_utilization(&result.config);
    TEST_NEAR("Utilization matches", util, result.utilization_pct, 0.01);
}

static void test_tooling_holes(void)
{
    tooling_hole_t holes[4];
    int n = 0;
    generate_tooling_holes(300.0, 200.0, 10.0, holes, &n);
    TEST("4 tooling holes", n == 4);
    TEST("Hole diameter > 0", holes[0].diameter_mm > 0.0);
    TEST("Not plated", holes[0].is_plated == false);
}

static void test_fiducial_marks(void)
{
    fiducial_mark_t marks[3];
    int n = 0;
    generate_global_fiducials(100.0, 80.0, 5.0, marks, &n);
    TEST("3 fiducials", n == 3);
    TEST("Fiducial type", marks[0].type == FIDUCIAL_GLOBAL);
    TEST("Fiducial diameter > 0", marks[0].copper_diameter_mm > 0.0);
}

static void test_tab_placements(void)
{
    double positions[10];
    int n = compute_tab_placements(100.0, 5.0, 3, 10.0, positions, 10);
    TEST("Tab count >= 3", n >= 3);
    TEST("First tab > corner offset", positions[0] > 10.0);
}

static void test_copper_thieving(void)
{
    copper_thieving_t th = design_copper_thieving(40.0, 70.0, 10000.0, 1.0);
    TEST("Thieving fill > current", th.fill_percentage > 40.0);
    TEST("Thieving crosshatch", th.is_crosshatch == true);

    double fill = compute_copper_fill(3000.0, 10000.0);
    TEST_NEAR("Copper fill 30%", fill, 30.0, 0.1);
}

static void test_panel_cost(void)
{
    double pc = estimate_panel_cost(300.0, 250.0, 4, IPC_CLASS_2);
    TEST("Panel cost > 0", pc > 0.0);
}

/* ================================================================
   dfm_thermal Tests
   ================================================================ */
static void test_thermal_materials(void)
{
    thermal_material_t fr4 = get_fr4_thermal_properties();
    TEST_NEAR("FR4 K", fr4.thermal_conductivity_k, 0.30, 0.01);

    thermal_material_t cu = get_copper_thermal_properties();
    TEST_NEAR("Cu K", cu.thermal_conductivity_k, 385.0, 1.0);
    TEST("Cu >> FR4", cu.thermal_conductivity_k > fr4.thermal_conductivity_k * 100.0);
}

static void test_junction_temp(void)
{
    thermal_resistance_t tr = compute_junction_temp(
        1.0, 10.0, 30.0, 25.0, 85.0);
    TEST_NEAR("Tj = 25 + 1*40 = 65", tr.junction_temp_c, 65.0, 0.1);
    TEST("Within limits", tr.within_limits == true);

    thermal_resistance_t tr2 = compute_junction_temp(
        3.0, 10.0, 30.0, 25.0, 85.0);
    TEST("Exceeds limit", tr2.within_limits == false);
}

static void test_copper_balance(void)
{
    /* 33/38/38/33 -> fills 33%/38%/38%/33%, max diff = 5% < 10%, balanced */
    double areas[] = {33.0, 38.0, 38.0, 33.0};
    copper_balance_t bal = analyze_copper_balance(areas, 4, 100.0);
    TEST("Balanced", bal.is_balanced == true);
    TEST_NEAR("Asymmetry ~5", bal.asymmetry_index, 5.0, 2.0);

    double areas2[] = {10.0, 80.0, 20.0, 30.0}; /* unbalanced */
    copper_balance_t bal2 = analyze_copper_balance(areas2, 4, 100.0);
    TEST("Unbalanced", bal2.is_balanced == false);
    TEST("High asymmetry", bal2.asymmetry_index > 30.0);
}

static void test_trace_current(void)
{
    /* 1mm wide, 1oz, 10C rise, external */
    double I = compute_trace_current_capacity(1000.0, 35.0, 10.0, true);
    TEST("Current > 0", I > 0.0);
    TEST("Current < 10A", I < 10.0);

    double I_int = compute_trace_current_capacity(1000.0, 35.0, 10.0, false);
    TEST("Internal < external", I_int < I);
}

static void test_trace_width_calc(void)
{
    /* 2A, 1oz, 10C rise, external */
    double w = compute_required_trace_width(2.0, 35.0, 10.0, true);
    TEST("Required width > 0", w > 0.0);

    double w_int = compute_required_trace_width(2.0, 35.0, 10.0, false);
    TEST("Internal needs wider", w_int > w);
}

static void test_thermal_vias(void)
{
    thermal_via_array_t tva = design_thermal_vias(
        2.0, 30.0, 1.6, 0.3, 100);
    TEST("Thermal vias > 0", tva.num_vias > 0);
    TEST("Thermal vias <= max", tva.num_vias <= 100);
    TEST("Resistance > 0", tva.total_thermal_resistance_kw > 0.0);
}

static void test_warpage(void)
{
    double areas[] = {30.0, 40.0, 30.0, 30.0};
    copper_balance_t bal = analyze_copper_balance(areas, 4, 100.0);
    double warp = estimate_warpage(&bal);
    TEST("Warpage >= 0", warp >= 0.0);
}

static void test_spreading_resistance(void)
{
    double R = compute_spreading_resistance(2.0, 10.0, 35.0);
    TEST("Spreading R > 0", R > 0.0);

    double R_invalid = compute_spreading_resistance(0.0, 10.0, 35.0);
    TEST_NEAR("Invalid R = 0", R_invalid, 0.0, 0.001);
}

static void test_effective_conductivity(void)
{
    double k_inplane = compute_effective_thermal_conductivity(1.5, 0.14, true);
    TEST("In-plane K > FR4 K", k_inplane > 0.3);

    double k_through = compute_effective_thermal_conductivity(1.5, 0.14, false);
    TEST("Through K < in-plane", k_through < k_inplane);
    TEST("Through K near FR4", k_through < 1.0); /* FR4 dominates */
}

/* ================================================================
   dfm_yield Tests
   ================================================================ */
static void test_yield_poisson(void)
{
    double Y = yield_poisson(1.0, 0.0);
    TEST_NEAR("Poisson D=0 => Y=1", Y, 1.0, 0.0001);

    double Y2 = yield_poisson(1.0, 1.0);
    TEST_NEAR("Poisson D=1 A=1 => exp(-1)", Y2, 0.367879, 0.001);
}

static void test_yield_murphy(void)
{
    double Y = yield_murphy(1.0, 1.0);
    TEST("Murphy Y in (0,1)", Y > 0.0 && Y < 1.0);
    TEST("Murphy > Poisson (D=1)", Y > yield_poisson(1.0, 1.0));
}

static void test_yield_seeds(void)
{
    /* At A*D=4, Seeds=exp(-2)=0.135, Murphy=((1-exp(-4))/4)^2=0.061.
       Seeds > Murphy for larger A*D values. */
    double Y = yield_seeds(4.0, 1.0);
    TEST("Seeds Y in (0,1)", Y > 0.0 && Y < 1.0);
    /* Seeds is more optimistic than Murphy for large critical area */
    TEST("Seeds > Murphy", Y > yield_murphy(4.0, 1.0));
}

static void test_yield_negbinomial(void)
{
    double Y_nb = yield_neg_binomial(1.0, 1.0, 1.0);
    TEST_NEAR("NegBin alpha=1 => 1/(1+1)", Y_nb, 0.5, 0.01);

    double Y_nb2 = yield_neg_binomial(1.0, 1.0, 10.0);
    TEST("Large alpha => near Poisson",
         fabs(Y_nb2 - yield_poisson(1.0, 1.0)) < 0.1);
}

static void test_yield_montecarlo(void)
{
    yield_result_t mc = yield_monte_carlo(
        2.0, 0.1, YIELD_POISSON, 1.0, 1000, 42);
    TEST("MC yield in (0,1)", mc.predicted_yield > 0.0
         && mc.predicted_yield < 1.0);
    TEST("MC CI positive", mc.confidence_interval_95 > 0.0);
}

static void test_yield_full(void)
{
    yield_result_t yr = yield_compute_full(1.0, 0.5, 2.0);
    TEST("Full yield in (0,1)", yr.predicted_yield > 0.0
         && yr.predicted_yield < 1.0);
}

static void test_critical_area(void)
{
    double A = compute_critical_area(1000.0, 100.0, 100.0, 500.0);
    TEST("Critical area > 0", A > 0.0);
    TEST("Critical area <= board area", A <= 500.0);
}

static void test_panel_yield(void)
{
    double Yp = compute_panel_yield(0.95, 4);
    TEST_NEAR("Panel yield 4x @ 95%", Yp, 0.8145, 0.01);
}

static void test_required_defect_density(void)
{
    double D = estimate_required_defect_density(0.90, 2.0);
    TEST_NEAR("D for 90% yield, A=2", D, 0.0527, 0.001);
}

/* ================================================================
   Main
   ================================================================ */
int main(void)
{
    printf("=== DFM Library Test Suite ===\n\n");

    printf("--- dfm_core ---\n");
    test_ipc_classes();
    test_substrate_lookup();
    test_finish_lookup();
    test_copper_weight();
    test_process_capability();
    test_drc_result();
    test_oee();
    test_via_checks();

    printf("\n--- dfm_rules ---\n");
    test_trace_width();
    test_spacing();
    test_annular_ring_check();
    test_solder_mask();
    test_ipc2221_clearance();
    test_etch_compensation();
    test_drill_rules();
    test_thermal_relief();
    test_edge_clearance();

    printf("\n--- dfm_cost ---\n");
    test_cost_estimation();
    test_tolerance_allocation();
    test_nre_cost();
    test_quantity_discount();
    test_layer_optimization();
    test_complexity();

    printf("\n--- dfm_panel ---\n");
    test_panel_optimization();
    test_tooling_holes();
    test_fiducial_marks();
    test_tab_placements();
    test_copper_thieving();
    test_panel_cost();

    printf("\n--- dfm_thermal ---\n");
    test_thermal_materials();
    test_junction_temp();
    test_copper_balance();
    test_trace_current();
    test_trace_width_calc();
    test_thermal_vias();
    test_warpage();
    test_spreading_resistance();
    test_effective_conductivity();

    printf("\n--- dfm_yield ---\n");
    test_yield_poisson();
    test_yield_murphy();
    test_yield_seeds();
    test_yield_negbinomial();
    test_yield_montecarlo();
    test_yield_full();
    test_critical_area();
    test_panel_yield();
    test_required_defect_density();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n",
           tests_passed, tests_failed);
    printf("========================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
