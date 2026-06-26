/**
 * @file    example_4layer_design.c
 * @brief   Example: Complete DFM Analysis of a 4-layer PCB Design
 *
 * Demonstrates a realistic DFM workflow for a 100x80mm 4-layer board:
 * 1. Material selection (FR4 standard)
 * 2. Surface finish selection (ENIG for fine-pitch)
 * 3. Design rule checking (trace width, spacing, annular ring)
 * 4. Thermal analysis (junction temperature, copper balance)
 * 5. Panelization (optimize panel utilization)
 * 6. Cost estimation
 *
 * Reference scenario: IoT gateway board with BGA processor,
 * DDR memory, and power management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dfm_core.h"
#include "dfm_rules.h"
#include "dfm_cost.h"
#include "dfm_panel.h"
#include "dfm_thermal.h"
#include "dfm_yield.h"

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  IoT Gateway PCB - DFM Analysis\n");
    printf("========================================\n");
    printf("\nDesign Spec: 100x80mm, 4-layer, Class 2\n\n");

    /* ---- Step 1: Material and Finish Selection ---- */
    printf("--- 1. Material & Finish Selection ---\n");
    const substrate_properties_t *sub = substrate_lookup(SUBSTRATE_FR4);
    printf("Substrate: %s\n", sub->trade_name);
    printf("  Dk=%.2f, Df=%.3f, Tg=%.0f C\n",
           sub->dielectric_constant, sub->loss_tangent,
           sub->tg_celsius);

    const finish_properties_t *fin = finish_lookup(FINISH_ENIG);
    printf("Surface Finish: %s\n", fin->name);
    printf("  Thickness=%.0f um, Shelf Life=%.0f months\n",
           fin->thickness_um, fin->shelf_life_months);
    printf("  RoHS: %s, Wire Bondable: %s\n",
           fin->rohs_compliant ? "Yes" : "No",
           fin->wire_bondable ? "Yes" : "No");

    /* ---- Step 2: Design Rule Checks ---- */
    printf("\n--- 2. Design Rule Checks ---\n");

    /* Trace width check */
    double min_trace = get_min_trace_width(CU_WEIGHT_1_0_OZ, IPC_CLASS_2);
    printf("Min trace width (1oz, Class 2): %.0f um\n", min_trace);

    /* Spacing check for 3.3V and 12V nets */
    double spacing_33v = compute_required_spacing(3.3, true, false, false);
    double spacing_12v = compute_required_spacing(12.0, true, false, false);
    printf("Min spacing for 3.3V: %.0f um\n", spacing_33v);
    printf("Min spacing for 12V:  %.0f um\n", spacing_12v);

    /* Annular ring check for a 0.3mm via */
    bool ring_ok = check_annular_ring(600.0, 300.0, true, IPC_CLASS_2, 0.0);
    printf("Annular ring check (0.3mm via, 0.6mm pad): %s\n",
           ring_ok ? "PASS" : "FAIL");

    /* Solder mask web check between two 0603 pads */
    const solder_mask_rule_t *mask = get_solder_mask_rule(IPC_CLASS_2);
    printf("Solder mask expansion: %.0f um\n",
           mask->min_mask_expansion_um);
    printf("Min mask web: %.0f um\n", mask->min_mask_web_um);

    /* Edge clearance */
    const edge_clearance_rule_t *ec =
        get_edge_clearance_rule(IPC_CLASS_2);
    printf("Copper-to-edge clearance: %.0f um\n",
           ec->copper_to_edge_um);

    /* ---- Step 3: Thermal Analysis ---- */
    printf("\n--- 3. Thermal Analysis ---\n");

    /* WiFi/BLE module: 0.5W, on 4-layer board with ground plane */
    thermal_resistance_t tr = compute_junction_temp(
        0.5,     /* 0.5W power */
        15.0,    /* Junction-to-board: 15 K/W */
        25.0,    /* Board-to-ambient: 25 K/W (4-layer with plane) */
        45.0,    /* Ambient: 45 C (enclosure internal) */
        105.0);  /* Max junction: 105 C */
    printf("WiFi Module Thermal:\n");
    printf("  T_junction = %.1f C (max %.0f C)\n",
           tr.junction_temp_c, tr.max_junction_temp_c);
    printf("  Status: %s\n",
           tr.within_limits ? "OK" : "EXCEEDS LIMIT");
    printf("  R_ja = %.1f K/W\n", tr.r_junction_ambient);

    /* Copper balance for 4-layer board */
    double copper_areas[] = {35.0, 65.0, 65.0, 35.0};
    double board_area_mm2 = 100.0 * 80.0; /* 8000 mm^2 */
    copper_balance_t bal = analyze_copper_balance(
        copper_areas, 4, board_area_mm2);
    printf("\nCopper Balance:\n");
    printf("  Top fill: %.1f%%, Inner1: %.1f%%\n",
           bal.fill_top_pct, bal.fill_inner1_pct);
    printf("  Inner2: %.1f%%, Bottom: %.1f%%\n",
           bal.fill_inner2_pct, bal.fill_bot_pct);
    printf("  Asymmetry: %.1f%% (%s)\n",
           bal.asymmetry_index,
           bal.is_balanced ? "OK" : "UNBALANCED");

    /* Trace current capacity for power traces */
    double trace_I = compute_trace_current_capacity(
        500.0, 35.0, 10.0, true);  /* 0.5mm width, 1oz, external */
    printf("\nTrace Current Capacity (0.5mm, 1oz, ext, 10C rise):\n");
    printf("  I_max = %.2f A\n", trace_I);

    /* ---- Step 4: Panelization ---- */
    printf("\n--- 4. Panelization ---\n");
    panel_optimization_result_t panel = optimize_panel_utilization(
        100.0, 80.0,     /* board size */
        350.0, 280.0,    /* panel size (standard 18x24" = 457x610mm, here smaller example) */
        2.0, 10.0,       /* spacing, edge clearance */
        12.0,            /* rail width */
        BREAKAWAY_TAB_ROUTE);
    printf("Optimal layout: %d x %d = %d boards\n",
           panel.config.boards_x, panel.config.boards_y,
           panel.config.total_boards);
    printf("Panel utilization: %.1f%%\n", panel.utilization_pct);
    printf("Wasted area: %.0f mm^2\n", panel.wasted_area_mm2);

    /* Tooling holes */
    tooling_hole_t holes[4];
    int num_holes = 0;
    generate_tooling_holes(panel.config.panel_width_mm,
                            panel.config.panel_height_mm,
                            panel.config.rail_width_mm,
                            holes, &num_holes);
    printf("Tooling holes: %d (diameter=%.3f mm)\n",
           num_holes, holes[0].diameter_mm);

    /* ---- Step 5: Cost Estimation ---- */
    printf("\n--- 5. Cost Estimation ---\n");
    cost_breakdown_t cost = estimate_pcb_cost(
        board_area_mm2, 4, IPC_CLASS_2,
        FINISH_ENIG, CU_WEIGHT_1_0_OZ, 1000, false);
    printf("Production cost (qty=1000):\n");
    printf("  Material:  $%.2f\n", cost.material_cost);
    printf("  Labor:     $%.2f\n", cost.labor_cost);
    printf("  Tooling:   $%.2f\n", cost.tooling_cost);
    printf("  NRE:       $%.2f\n", cost.nre_cost);
    printf("  Overhead:  $%.2f\n", cost.overhead_cost);
    printf("  --------------------------\n");
    printf("  Per board: $%.2f\n", cost.cost_per_board);

    /* ---- Step 6: Yield Prediction ---- */
    printf("\n--- 6. Yield Prediction ---\n");
    double crit_area = compute_critical_area(
        500.0,     /* 500 cm total trace length */
        150.0,     /* 150 um min spacing */
        100.0,     /* 100 um min trace width */
        board_area_mm2 * 0.01); /* board area in cm^2 */

    /* Compare yield models for realistic defect density */
    double defect_density = 0.05; /* 0.05 defects/cm^2 (reasonable) */
    double Y_poisson = yield_poisson(crit_area, defect_density);
    double Y_murphy  = yield_murphy(crit_area, defect_density);
    double Y_seeds   = yield_seeds(crit_area, defect_density);
    double Y_nb      = yield_neg_binomial(crit_area, defect_density, 2.0);

    printf("Critical area: %.2f cm^2\n", crit_area);
    printf("Defect density: %.3f/cm^2\n", defect_density);
    printf("Yield predictions:\n");
    printf("  Poisson:      %.1f%%\n", Y_poisson * 100.0);
    printf("  Murphy:       %.1f%%\n", Y_murphy * 100.0);
    printf("  Seeds:        %.1f%%\n", Y_seeds * 100.0);
    printf("  Neg Binomial: %.1f%%\n", Y_nb * 100.0);

    /* Panel yield */
    double panel_Y = compute_panel_yield(Y_murphy,
                                          panel.config.total_boards);
    printf("\nPanel yield (Murphy, %d boards): %.1f%%\n",
           panel.config.total_boards, panel_Y * 100.0);

    /* ---- Step 7: Process Capability ---- */
    printf("\n--- 7. Process Capability (Trace Width) ---\n");
    double trace_samples[] = {98.0, 102.0, 99.0, 101.0, 100.5,
                               99.5, 100.0, 101.5, 98.5, 100.0};
    process_capability_t cpk = compute_process_capability(
        trace_samples, 10, 110.0, 90.0);
    printf("Trace width Cpk: %.3f\n", cpk.cpk);
    printf("  Cp: %.3f (potential)\n", cpk.cp);
    printf("  Mean: %.1f um, StdDev: %.2f um\n",
           cpk.mean_um, cpk.stddev_um);
    printf("  Process: %s\n",
           cpk.capable ? "CAPABLE" : "NOT CAPABLE");
    printf("  Est. DPMO: %.0f\n", compute_dpmo_from_cpk(cpk.cpk));

    /* ---- Step 8: OEE for this production line ---- */
    printf("\n--- 8. OEE (Overall Equipment Effectiveness) ---\n");
    double oee = compute_pcb_oee(
        8.0,    /* 8 hours planned production */
        0.5,    /* 0.5 hours downtime */
        30.0,   /* 30 seconds ideal cycle per board */
        850.0,  /* 850 total processed */
        820.0); /* 820 good boards */
    printf("OEE: %.1f%%\n", oee * 100.0);
    printf("  (World-class: >=85%%)\n");

    printf("\n========================================\n");
    printf("  DFM Analysis Complete\n");
    printf("  Recommendation: Design is manufacturable\n");
    printf("========================================\n\n");

    return 0;
}
