/**
 * @file example_rigid_flex_design.c
 * @brief End-to-end example: Complete 6-layer rigid-flex stackup design
 *        for an automotive engine control unit (ECU) interconnect.
 *
 * Demonstrates: stackup construction, bend zone analysis, DRC validation,
 * transition zone stress analysis, and thermal derating.
 *
 * L6 Canonical Problem: Rigid-flex design for automotive under-hood.
 * L7 Application: Automotive ECU flex interconnect (-40°C to 125°C).
 */

#include "flex_stackup.h"
#include "flex_bend.h"
#include "flex_design_rule.h"
#include "flex_rigid_transition.h"
#include "flex_thermal.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Rigid-Flex Design: Automotive ECU Interconnect ===\n\n");

    /* Step 1: Build the stackup */
    flex_stackup_t stackup = flex_stackup_init("ECU_RigidFlex_6L");
    strncpy(stackup.ipc_class, "3", sizeof(stackup.ipc_class) - 1);

    /* 6-layer rigid-flex: 4 rigid layers + 2 flex-through layers */
    flex_stackup_add_signal_layer(&stackup, "L1_RIGID_TOP", 35.0,
        FLEX_COPPER_ED, FLEX_DIELEC_POLYIMIDE, 100.0);
    flex_stackup_add_plane_layer(&stackup, "L2_GND", 35.0, FLEX_COPPER_ED);
    flex_stackup_add_signal_layer(&stackup, "L3_FLEX_SIG1", 18.0,
        FLEX_COPPER_RA, FLEX_DIELEC_POLYIMIDE, 50.0);
    flex_stackup_add_signal_layer(&stackup, "L4_FLEX_SIG2", 18.0,
        FLEX_COPPER_RA, FLEX_DIELEC_POLYIMIDE, 50.0);
    flex_stackup_add_plane_layer(&stackup, "L5_PWR", 35.0, FLEX_COPPER_ED);
    flex_stackup_add_signal_layer(&stackup, "L6_RIGID_BOT", 35.0,
        FLEX_COPPER_ED, FLEX_DIELEC_POLYIMIDE, 100.0);

    /* L1, L2, L5, L6 = rigid only; L3, L4 = flex-through */
    flex_stackup_set_rigid_only(&stackup, 1);
    flex_stackup_set_rigid_only(&stackup, 2);
    flex_stackup_set_flex_through(&stackup, 3);
    flex_stackup_set_flex_through(&stackup, 4);
    flex_stackup_set_rigid_only(&stackup, 5);
    flex_stackup_set_rigid_only(&stackup, 6);

    /* Add bend zone (flex section between two rigid PCBs) */
    flex_stackup_add_bend_zone(&stackup, 0, 0, 0, 30.0, 5.0, 90.0, 0);

    /* Add FR-4 stiffener for connector area */
    flex_stackup_add_stiffener(&stackup, FLEX_STIFFENER_FR4, 1.0, 1);

    /* Step 2: Display stackup summary */
    printf("  Stackup Summary:\n");
    printf("  ----------------\n");
    printf("  Design: %s\n", stackup.design_name);
    printf("  IPC Class: %s\n", stackup.ipc_class);
    printf("  Total layers: %d\n", stackup.total_layer_count);
    printf("  Flex layers: %d\n",
           flex_stackup_flex_layer_count(&stackup));
    printf("  Flex thickness: %.3f mm\n",
           flex_stackup_flex_thickness(&stackup));
    printf("  Flex copper: %.1f μm\n",
           flex_stackup_flex_copper_total(&stackup));

    /* Step 3: Neutral axis and flexural rigidity */
    double neutral;
    flex_stackup_neutral_axis(&stackup, &neutral);
    double d = flex_stackup_flexural_rigidity(&stackup);
    printf("  Neutral axis: %.3f mm from bottom\n", neutral);
    printf("  Flexural rigidity: %.1f N·mm²/mm\n", d);
    printf("  Symmetric: %s\n",
           flex_stackup_verify_symmetry(&stackup) ? "YES" : "NO");

    /* Step 4: Bend analysis */
    printf("\n  Bend Analysis:\n");
    printf("  --------------\n");
    flex_bend_params_t bp;
    memset(&bp, 0, sizeof(bp));
    bp.bend_radius_mm = 5.0;
    bp.bend_angle_deg = 90.0;
    bp.total_thickness_mm = flex_stackup_flex_thickness(&stackup);
    bp.copper_thickness_total_um = flex_stackup_flex_copper_total(&stackup);
    bp.copper_elongation_limit_percent = 16.0;
    bp.num_layers = flex_stackup_flex_layer_count(&stackup);
    bp.is_dynamic = 0;  /* Static bend — one-time form */
    bp.youngs_modulus_copper_mpa = 117000.0;

    flex_bend_result_t bend_result;
    flex_bend_analyze(&bp, &bend_result);
    printf("  Min bend radius: %.2f mm\n", bend_result.min_bend_radius_mm);
    printf("  Safety factor: %.2f×\n", bend_result.safety_factor);
    printf("  Max Cu strain: %.2f%%\n",
           bend_result.max_copper_strain_percent);

    /* Step 5: DRC check */
    printf("\n  DRC Summary:\n");
    printf("  -----------\n");
    flex_drc_report_t drc = flex_drc_run_full(&stackup);
    printf("  Violations: %d (errors: %d, warnings: %d)\n",
           drc.total_violations, drc.error_count, drc.warning_count);

    /* Step 6: Transition zone analysis */
    printf("\n  Transition Zone Analysis:\n");
    printf("  ------------------------\n");
    flex_transition_params_t tp;
    memset(&tp, 0, sizeof(tp));
    tp.trans_type             = FLEX_TRANS_SELECTIVE_FLEX;
    tp.transition_length_mm   = 2.5;
    tp.rigid_thickness_mm     = 1.6;
    tp.flex_thickness_mm      = flex_stackup_flex_thickness(&stackup);
    tp.rigid_cte_xy_ppm       = 14.0;     /* FR-4 */
    tp.flex_cte_xy_ppm        = 20.0;     /* PI */
    tp.rigid_modulus_mpa      = 24000.0;
    tp.flex_modulus_mpa       = 2500.0;
    tp.has_anchor_tab         = 1;
    tp.anchor_tab_length_mm   = 2.0;
    tp.anchor_tab_width_mm    = 3.0;
    tp.anchor_tab_count       = 4;
    tp.has_tear_stop          = 1;
    tp.tear_stop_diameter_mm  = 0.8;
    tp.tear_stop_spacing_mm   = 2.0;
    tp.temp_range_min_c       = -40.0;
    tp.temp_range_max_c       = 125.0;
    tp.expected_thermal_cycles = 3000;
    tp.rigid_z0_ohm           = 50.0;
    tp.flex_z0_ohm            = 50.0;

    flex_transition_result_t trans_result;
    flex_transition_analyze(&tp, &trans_result);
    printf("  Max shear stress: %.1f MPa\n",
           trans_result.max_shear_stress_mpa);
    printf("  Max peel stress:  %.1f MPa\n",
           trans_result.max_peel_stress_mpa);
    printf("  Stress concentration: %.2f\n",
           trans_result.stress_concentration_factor);
    printf("  Anchor tab strength: %.1f N\n",
           trans_result.anchor_tab_strength_n);
    printf("  Thermal cycles to failure: %.0f\n",
           trans_result.estimated_thermal_life);
    printf("  Transition rating: %d/5\n",
           trans_result.transition_rating);

    /* Step 7: Thermal analysis */
    printf("\n  Thermal Analysis (worst-case trace):\n");
    printf("  -----------------------------------\n");
    flex_thermal_config_t tc;
    memset(&tc, 0, sizeof(tc));
    tc.ambient_temp_c          = 125.0;  /* Under-hood max */
    tc.max_operating_temp_c    = 150.0;
    tc.copper_trace_width_um   = 200.0;
    tc.copper_trace_thickness_um = 18.0;
    tc.trace_length_mm         = 50.0;
    tc.layer_position          = 3;
    tc.is_outer_layer          = 0;
    tc.airflow_m_per_s         = 0.5;    /* Some forced airflow */
    tc.board_thickness_mm      = flex_stackup_flex_thickness(&stackup);
    tc.board_width_mm          = 20.0;
    tc.board_length_mm         = 50.0;

    flex_thermal_result_t therm_result;
    flex_thermal_analyze(&tc, &therm_result);
    printf("  Max safe current: %.2f A\n",
           therm_result.max_trace_current_a);
    printf("  Trace resistance: %.3f Ω\n",
           therm_result.trace_resistance_ohm);
    printf("  CTE mismatch stress: %.1f MPa\n",
           therm_result.cte_mismatch_stress_mpa);
    printf("  Thermal time constant: %.1f s\n",
           therm_result.thermal_time_constant_s);

    /* Step 8: Cost estimate */
    printf("\n  Cost Estimate:\n");
    printf("  -------------\n");
    double cost = flex_stackup_cost_index(&stackup);
    printf("  Relative cost index: %.1f× (baseline = 2L simple flex)\n",
           cost);

    /* Final verdict */
    printf("\n=== Final Verdict ===\n");
    int issues = 0;
    if (!bend_result.is_compliant_ipc2223) {
        printf("  FAIL: Bend radius non-compliant.\n"); issues++;
    }
    if (drc.error_count > 0) {
        printf("  FAIL: %d DRC errors.\n", drc.error_count); issues++;
    }
    if (trans_result.transition_rating < 2) {
        printf("  FAIL: Transition zone rating %d/5.\n",
               trans_result.transition_rating); issues++;
    }
    if (issues == 0) {
        printf("  PASS — Design ready for prototyping.\n");
    }

    return 0;
}
